#include "qemu/osdep.h"
#include "hw/vst/sha3/sha3.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/vst/reg_interface.h"
#include "hw/vst/nettle/sha3.h"

/*define base address for SHA3 registers*/
#define SHA3_CONTROL_REG    0x00
#define SHA3_STATUS_REG     0x04
#define SHA3_INPUTLEN_REG   0x08
#define SHA3_INPUT_REG      0x0C
#define SHA3_OUTPUT_REG     0x10
#define SHA3_OUTPUTLEN_REG  0x14
#define SHA3_OUTPUTCTRL_REG 0x18

/*
---Define bit for SHA3_CONTROL_REG---
    bit 0: reset : default 0x00
        - 0x00 : not reset
        - 0x01 : reset
    bit 1-2: mode : default 0x00
        - 0x00 : sha3_224
        - 0x01 : sha3_256
        - 0x02 : sha3_384
        - 0x03 : sha3_512
        - 0x04 : shake_128
        - 0x05 : shake_256
    bit 3: Endian : default 0x00
        - 0x00 : little endian
        - 0x01 : big endian
    bit 4-5: suspend : default 0x00
        - 0x00 : not suspend
        - 0x01 : suspend    (When suspend is set, the SHA3 engine will stop processing and wait for the resume signal)
        - 0x02 : resume     (When resume is set, the SHA3 engine will resume processing)
        - 0x03 : reserved
    bit 6-7: reserved
    bit 8: shake_output : default 0x00
        - 0x00 : reset internal state when shake output is done (maximum support to get 64 bytes output)
        - 0x01 : do not reset internal state when shake output is done
    bit 9-30: reserved
    bit 31: DMA : default 0x00
        - 0x00 : disable
        - 0x01 : enable
*/
#define CONTROL_RESET_BIT(val) ((val >> 0) & 0x1)
#define CONTROL_MODE_BIT(val)  ((val >> 1) & 0x03)
#define CONTROL_ENDIAN_BIT(val) ((val >> 3) & 0x1)
#define CONTROL_DMA_BIT(val)   ((val >> 31) & 0x1)
#define CONTROL_SUSPEND_BIT(val) ((val >> 4) & 0x3)
#define CONTROL_SHAKE_OUTPUT_BIT(val) ((val >> 8) & 0x1)

/*
---Define bit for SHA3_STATUS_REG---
    bit 0: ready : default 0x00
        - 0x00 : not ready
        - 0x01 : ready
    bit 1: done : default 0x00
        - 0x00 : not done
        - 0x01 : done
    bit 2: error : default 0x00
        - 0x00 : no error
        - 0x01 : error
    bit 3: suspend : default 0x00
        - 0x00 : not suspend
        - 0x01 : suspend
    bit 4-31: reserved
*/
#define STATUS_READY_BIT(val)       ((val >> 0) & 0x1)
#define STATUS_DONE_BIT(val)        ((val >> 1) & 0x1)
#define STATUS_ERROR_BIT(val)       ((val >> 2) & 0x1)
#define STATUS_SUSPEND_BIT(val)     ((val >> 3) & 0x1) 

/*
---Define bit for SHA3_INPUTLEN_REG---
    bit 0-31: input length : default 0x00 (byte)
---Define bit for SHA3_INPUT_REG---
    bit 0-31: input data : default 0x00
---Define bit for SHA3_OUTPUT_REG---
    bit 0-31: output data : default 0x00
---Define bit for SHA3_OUTPUTLEN_REG---
    bit 0-31: output length : default 0x00 (byte)
*/

/*
---Define bit for SHA3_OUTPUTCTRL_REG---
    bit 0-7 : read_ptr : default 0x00   
        (8 bits to select 16 output slots, range: 0-15)
        In case of suspend and resume operation, 
        this pointer supports the read operation to read the output state index range from 0 to 50.
        Maximum to 2^8 = 256 output slots
    bit 8-31: reserved
*/
#define OUTPUTCTRL_READ_PTR_BIT(val) ((val >> 0) & 0xFF)

#define MAX_REG 7
Register32 *sha3_reg_list[MAX_REG];

// Define additional functions in here
void set_bits(uint32_t *reg_val, uint32_t value, uint32_t bit_start, uint32_t bit_end);

// Define callback functions in here
void cb_input_reg(Register32 *reg, uint32_t value);
void cb_outputctrl_reg(Register32 *reg, uint32_t value);
void cb_control_reg(Register32 *reg, uint32_t value);
void cb_outputlen_reg(Register32 *reg, uint32_t value);

// Define other functions
void get_internal_state(void);
void assign_internal_state(uint32_t value);

enum REGISTER_NAME 
{
    eCONTROL_REG = 0,
    eSTATUS_REG,
    eINPUTLEN_REG,
    eINPUT_REG,
    eOUTPUT_REG,
    eOUTPUTLEN_REG,
    eOUTPUTCTRL_REG,
    eSTATE_REG
};

/*For SHA3*/
struct sha3_224_ctx _sha3_224_ctx;
struct sha3_256_ctx _sha3_256_ctx;
struct sha3_384_ctx _sha3_384_ctx;
struct sha3_512_ctx _sha3_512_ctx;

/*For SHAKE*/
struct sha3_128_ctx _shake_128_ctx;
struct sha3_256_ctx _shake_256_ctx;


/*
    The sha3 state is a 5x5 matrix of 64-bit words. In the notation of
   Keccak description, S[x,y] is element x + 5*y, so if x is
   interpreted as the row index and y the column index, it is stored
   in column-major order. 
    #define SHA3_STATE_LENGTH 25

    The "width" is 1600 bits or 200 octets 
    struct sha3_state
    {
      uint64_t a[SHA3_STATE_LENGTH];
    };
*/
uint32_t sha3_internal_state[50];
uint32_t state_index;

// define sha3 ouput buffers
#define MAXIMUM_OUTPUT_SIZE 64
uint8_t sha3_output[MAXIMUM_OUTPUT_SIZE];


// set bit for register
void set_bits(uint32_t *reg_val, uint32_t value, uint32_t bit_start, uint32_t bit_end) 
{
    // Create a mask to clear the bits between bit_start and bit_end
    uint32_t mask = ((1 << (bit_end - bit_start + 1)) - 1) << bit_start;
    
    // Clear the bits in the register (set the target bits to 0)
    *reg_val &= ~mask;
    
    // Insert the new value into the cleared bit positions
    *reg_val |= (value << bit_start) & mask;
}



void get_internal_state(void)
{
    switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
    {
        case 0x00:
        {    for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _sha3_224_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_sha3_224_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        case 0x01:
        {
            for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _sha3_256_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_sha3_256_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        case 0x02:
        {
            for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _sha3_384_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_sha3_384_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        case 0x03:
        {
            for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _sha3_512_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_sha3_512_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        case 0x04:
        {
            for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _shake_128_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_shake_128_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        case 0x05:
        {
            for(int i = 0; i < 25; i++)
            {
                sha3_internal_state[i*2]   = _shake_256_ctx.state.a[i] & 0xFFFFFFFF;
                sha3_internal_state[i*2+1] = (_shake_256_ctx.state.a[i] >> 32) & 0xFFFFFFFF;
            }
            break;
        }
        default:
            break;
    }   
}

void assign_internal_state(uint32_t value)
{
    // assign cxt to the correct state
    switch(CONTROL_MODE_BIT(value))
    {
        case 0x00:
        {    
            for(int i = 0; i < 25; i++)
            {
                _sha3_224_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        case 0x01:
        {    
            for(int i = 0; i < 25; i++)
            {
                _sha3_256_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        case 0x02:
        {    
            for(int i = 0; i < 25; i++)
            {
                _sha3_384_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        case 0x03:
        {    
            for(int i = 0; i < 25; i++)
            {
                _sha3_512_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        case 0x04:
        {    
            for(int i = 0; i < 25; i++)
            {
                _shake_128_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        case 0x05:
        {    
            for(int i = 0; i < 25; i++)
            {
                _shake_256_ctx.state.a[i] = (uint64_t)sha3_internal_state[i*2] | ((uint64_t)sha3_internal_state[i*2+1] << 32);
            }
            break;
        }
        default:
            break;
    }
}


// callback for register
void cb_input_reg(Register32 *reg, uint32_t value) 
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    // check reset bit
    if(CONTROL_RESET_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x01)
    {
        // clear reset bit
        set_bits(&sha3_reg_list[eCONTROL_REG]->value, 0x00, 0, 0);
    }

    // Check suspend bit, if suspend bit is set, the sha3 engine will stop processing and wait for the resume signal
    if(CONTROL_SUSPEND_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x00)
    {
        // get input data from register
        uint32_t input_data = value; // get input data from register 32 bits (4 bytes)
        set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 0, 0); // set status ready

        if(sha3_reg_list[eINPUTLEN_REG]->value <= 0x04)
        {
            // last block, get digest
            switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
            {
                case 0x00:
                {
                    sha3_224_update(&_sha3_224_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                    sha3_224_digest(&_sha3_224_ctx, SHA3_224_DIGEST_SIZE, (uint8_t *)&sha3_output); // reset internal state
                    break;
                }
                case 0x01:
                {
                    sha3_256_update(&_sha3_256_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                    sha3_256_digest(&_sha3_256_ctx, SHA3_256_DIGEST_SIZE, (uint8_t *)&sha3_output); // reset internal state
                    break;
                }
                case 0x02:
                {
                    sha3_384_update(&_sha3_384_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                    sha3_384_digest(&_sha3_384_ctx, SHA3_384_DIGEST_SIZE, (uint8_t *)&sha3_output); // reset internal state
                    break;
                }
                case 0x03:
                {
                    sha3_512_update(&_sha3_512_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                    sha3_512_digest(&_sha3_512_ctx, SHA3_512_DIGEST_SIZE, (uint8_t *)&sha3_output); // reset internal state
                    break;
                }
                case 0x04:
                {
                    sha3_128_update(&_shake_128_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&sha3_output);
                    if(CONTROL_SHAKE_OUTPUT_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x00)
                    {
                        if(sha3_reg_list[eOUTPUTLEN_REG]->value >= MAXIMUM_OUTPUT_SIZE)
                        {
                            sha3_128_shake(&_shake_128_ctx, MAXIMUM_OUTPUT_SIZE, (uint8_t *)&sha3_output); // reset internal state
                        }
                        else
                        {
                            sha3_128_shake(&_shake_128_ctx, sha3_reg_list[eOUTPUTLEN_REG]->value, (uint8_t *)&sha3_output); // reset internal state
                        }   
                    }
                    else
                    {
                        if(sha3_reg_list[eOUTPUTLEN_REG]->value >= MAXIMUM_OUTPUT_SIZE)
                        {
                            sha3_128_shake_output(&_shake_128_ctx, MAXIMUM_OUTPUT_SIZE, (uint8_t *)&sha3_output); // do not reset internal state
                        }
                        else
                        {
                            sha3_128_shake_output(&_shake_128_ctx, sha3_reg_list[eOUTPUTLEN_REG]->value, (uint8_t *)&sha3_output); // do not reset internal state
                        }  
                    }
                    break;
                }
                case 0x05:
                {
                    sha3_256_update(&_shake_256_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&sha3_output);
                    if(CONTROL_SHAKE_OUTPUT_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x00)
                    {
                        if(sha3_reg_list[eOUTPUTLEN_REG]->value >= MAXIMUM_OUTPUT_SIZE)
                        {
                            sha3_256_shake(&_shake_256_ctx, MAXIMUM_OUTPUT_SIZE, (uint8_t *)&sha3_output); // reset internal state
                        }
                        else
                        {
                            sha3_256_shake(&_shake_256_ctx, sha3_reg_list[eOUTPUTLEN_REG]->value, (uint8_t *)&sha3_output); // reset internal state
                        }   
                    }
                    else
                    {
                        if(sha3_reg_list[eOUTPUTLEN_REG]->value >= MAXIMUM_OUTPUT_SIZE)
                        {
                            sha3_256_shake_output(&_shake_256_ctx, MAXIMUM_OUTPUT_SIZE, (uint8_t *)&sha3_output); // do not reset internal state
                        }
                        else
                        {
                            sha3_256_shake_output(&_shake_256_ctx, sha3_reg_list[eOUTPUTLEN_REG]->value, (uint8_t *)&sha3_output); // do not reset internal state
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            // set status done
            set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 1, 1);
            // clear status ready
            set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x00, 0, 0); 
        }
        else
        {
            // in the case the input length is more than 4 byte
            switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
            {
                case 0x00:
                    sha3_224_update(&_sha3_224_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                case 0x01:
                    sha3_256_update(&_sha3_256_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                case 0x02:
                    sha3_384_update(&_sha3_384_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                case 0x03:
                    sha3_512_update(&_sha3_512_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                case 0x04:
                    sha3_128_update(&_shake_128_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                case 0x05:
                    sha3_256_update(&_shake_256_ctx, 0x04, (uint8_t *)&input_data);
                    break;
                default:
                    break;
            }
            // update input length register
            sha3_reg_list[eINPUTLEN_REG]->value = sha3_reg_list[eINPUTLEN_REG]->value - 0x04; // decrease input length by 4 byte

            // clear status done
            set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x00, 1, 1);
            // set status ready
            set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 0, 0); 
        }
    }
    else
    {
        // if suspend bit is set, the sha3 input register is used to input cxt state for the correct mode
        if(state_index < 50)
        {
            // assign input data to internal state 50 times
            sha3_internal_state[state_index] = value;
            state_index++;
        }
    }
    
}

void cb_outputctrl_reg(Register32 *reg, uint32_t value)
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    uint8_t index = OUTPUTCTRL_READ_PTR_BIT(value);
    // check bit status done or suspend
    if(STATUS_DONE_BIT(sha3_reg_list[eSTATUS_REG]->value) == 0x01)
    {
        if(index < 16)
        {
            sha3_reg_list[eOUTPUT_REG]->value = *((uint32_t *)&sha3_output[index*4]);   // getting 4 bytes from sha3_output buffer
        }
    }
    else if(STATUS_SUSPEND_BIT(sha3_reg_list[eSTATUS_REG]->value) == 0x01)
    {
        if(index < 50)
        {
            sha3_reg_list[eOUTPUT_REG]->value = sha3_internal_state[index];  // assign internal state to output register
        }
    }
}

void cb_outputlen_reg(Register32 *reg, uint32_t value)
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    if(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x04 || CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x05)
    {
        if(CONTROL_SHAKE_OUTPUT_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x01 && STATUS_READY_BIT(sha3_reg_list[eSTATUS_REG]->value) == 0x00)
        {
            if(STATUS_DONE_BIT(sha3_reg_list[eSTATUS_REG]->value) == 0x01)
            {
                // clear output buffer
                for(int i = 0; i < MAXIMUM_OUTPUT_SIZE; i++)
                {
                    sha3_output[i] = 0;
                }
                sha3_256_shake_output(&_shake_256_ctx, value, (uint8_t *)&sha3_output); // do not reset internal state
            }
        }
        
    }
    
}

void cb_control_reg(Register32 *reg, uint32_t value)
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    if(CONTROL_RESET_BIT(value) == 0x01)
    {
        // reset all sha3 cxt
        sha3_224_init(&_sha3_224_ctx);
        sha3_256_init(&_sha3_256_ctx);
        sha3_384_init(&_sha3_384_ctx);
        sha3_512_init(&_sha3_512_ctx);
        sha3_128_init(&_shake_128_ctx);
        sha3_256_init(&_shake_256_ctx);
        // reset internal state
        for(int i = 0; i < 50; i++)
        {
            sha3_internal_state[i] = 0;
        }
        // Reset output buffer
        for(int i = 0; i < MAXIMUM_OUTPUT_SIZE; i++)
        {
            sha3_output[i] = 0;
        }
        // clear status register
        sha3_reg_list[eSTATUS_REG]->value = 0x00;
        qemu_log("[sha3] Reset sha3\n");
    }

    if(CONTROL_SUSPEND_BIT(value) == 0x01)  // suspend
    {
        // get internal state
        get_internal_state();
        // set status suspend
        set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 3, 3);
        // clear status ready
        set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x00, 0, 0);
        // reset state index
        state_index = 0;
        qemu_log("[sha3] suspend sha3\n");
    }
    else if(CONTROL_SUSPEND_BIT(value) == 0x02) //resume
    {
        // clear status suspend
        set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x00, 3, 3);
        assign_internal_state(value);
        qemu_log("[sha3] resume sha3\n");
    }
    
}


void sha3_register_init(void)
{
    sha3_reg_list[eCONTROL_REG]  = create_register32("SHA3_CONTROL_REG" , SHA3_CONTROL_REG   , REG_READ_WRITE, 0, 0xFFFFFFFF, cb_control_reg);
    sha3_reg_list[eSTATUS_REG]   = create_register32("SHA3_STATUS_REG"  , SHA3_STATUS_REG    , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);   
    sha3_reg_list[eINPUTLEN_REG] = create_register32("SHA3_INPUTLEN_REG", SHA3_INPUTLEN_REG  , REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eINPUT_REG]    = create_register32("SHA3_INPUT_REG"   , SHA3_INPUT_REG     , REG_READ_WRITE, 0, 0xFFFFFFFF, cb_input_reg);
    sha3_reg_list[eOUTPUT_REG]   = create_register32("SHA3_OUTPUT_REG"  , SHA3_OUTPUT_REG    , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUTLEN_REG]= create_register32("SHA3_OUTPUTLEN_REG", SHA3_OUTPUTLEN_REG, REG_READ_WRITE , 0, 0xFFFFFFFF, cb_outputlen_reg);
    sha3_reg_list[eOUTPUTCTRL_REG]= create_register32("SHA3_OUTPUTCTRL_REG", SHA3_OUTPUTCTRL_REG, REG_READ_WRITE , 0, 0xFFFFFFFF, cb_outputctrl_reg);
    
}


/*This is template code for registration new device in qemu*/

static uint64_t sha3_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log("[sha3] read at offset address 0x%X\n", (uint32_t)addr);
    uint32_t value = 0;
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == sha3_reg_list[i]->base_addr) 
        {
            value = read_register32(sha3_reg_list[i]);
            break;
        }
    }
    return value;
}

static void sha3_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    qemu_log("[sha3] write 0x%lX at offset address 0x%X\n", value, (uint32_t)addr);
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == sha3_reg_list[i]->base_addr) 
        {
            write_register32(sha3_reg_list[i], value);
        }
    }
}

static const VMStateDescription vmstate_sha3 = {
    .name = "sha3",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static const MemoryRegionOps sha3_ops = {
    .read = sha3_read,
    .write = sha3_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.max_access_size = 8,
    .impl.max_access_size = 8,
};

static void sha3_realize(DeviceState *dev, Error **errp)
{
    SHA3device *sha3 = SHA3(dev);

    memory_region_init_io(&sha3->io, OBJECT(dev), 
                &sha3_ops, sha3,"sha3", 0x1000);

    sysbus_init_mmio(SYS_BUS_DEVICE(sha3), &sha3->io);
    sysbus_init_irq(SYS_BUS_DEVICE(sha3), &sha3->irq);

}

SHA3device *sha3_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq)
{
    SHA3device *sha3 = SHA3(qdev_new(TYPE_SHA3));
    MemoryRegion *mr;

   // Realize the device and connect IRQ
    sysbus_realize_and_unref(SYS_BUS_DEVICE(sha3), &error_fatal);
    sysbus_connect_irq(SYS_BUS_DEVICE(sha3), 0, irq);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(sha3), 0);
    memory_region_add_subregion(address_space, base, mr);

    /*initialize register*/
    sha3_register_init();

    qemu_log("sha3 initialized\n");
    return sha3;
}


static void sha3_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = sha3_realize;
    dc->vmsd = &vmstate_sha3;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_SHA3,
        .parent = TYPE_SYS_BUS_DEVICE,
        .class_init = sha3_class_init,
        .instance_size = sizeof(SHA3device),
    },
};

DEFINE_TYPES(types)