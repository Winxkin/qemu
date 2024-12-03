#include "qemu/osdep.h"
#include "hw/vst/sha3/sha3.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/vst/reg_interface.h"
#include "hw/vst/nettle/sha3.h"

// define base address for SHA3 registers
#define SHA3_CONTROL_REG    0x00
#define SHA3_STATUS_REG     0x04
#define SHA3_INPUTLEN_REG   0x08
#define SHA3_INPUT_REG      0x0C
#define SHA3_OUTPUT_REG(i)  (0x10 + i*0x04) // 16 register i from 0 to 15 address base 0x10 to 0x40

/*
---Define bit for SHA3_CONTROL_REG---
    bit 0: reset : default 0x00
        - 0x00 : not reset
        - 0x01 : reset
    bit 1-2: mode : default 0x00
        - 0x00 : sha3_128
        - 0x01 : sha3_224
        - 0x02 : sha3_256
        - 0x03 : sha3_384
        - 0x04 : sha3_512
    bit 3-31: reserved
*/
#define CONTROL_RESET_BIT(val) ((val >> 0) & 0x1)
#define CONTROL_MODE_BIT(val)  ((val >> 1) & 0x03) 

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
    bit 3-31: reserved
*/
#define STATUS_READY_BIT(val)       ((val >> 0) & 0x1)
#define STATUS_DONE_BIT(val)        ((val >> 1) & 0x1)
#define STATUS_ERROR_BIT(val)       ((val >> 2) & 0x1)

/*
---Define bit for SHA3_INPUTLEN_REG---
    bit 0-31: input length : default 0x00 (byte)
---Define bit for SHA3_INPUT_REG---
    bit 0-31: input data : default 0x00
---Define bit for SHA3_OUTPUT_REG(i), i = [0,15]---
    bit 0-31: output data : default 0x00
*/


#define MAX_REG 20
Register32 *sha3_reg_list[MAX_REG];

// Define additional functions in here
void set_bits(uint32_t *reg_val, uint32_t value, uint32_t bit_start, uint32_t bit_end);

// Define callback functions in here
void cb_input_reg(Register32 *reg, uint32_t value);

// Define other functions
void assign_digest_output(void);

enum REGISTER_NAME 
{
    eCONTROL_REG = 0,
    eSTATUS_REG,
    eINPUTLEN_REG,
    eINPUT_REG,
    eOUTPUT_REG0,
    eOUTPUT_REG1,
    eOUTPUT_REG2,
    eOUTPUT_REG3,
    eOUTPUT_REG4,
    eOUTPUT_REG5,
    eOUTPUT_REG6,
    eOUTPUT_REG7,
    eOUTPUT_REG8,
    eOUTPUT_REG9,
    eOUTPUT_REG10,
    eOUTPUT_REG11,
    eOUTPUT_REG12,
    eOUTPUT_REG13,
    eOUTPUT_REG14,
    eOUTPUT_REG15
};

struct sha3_128_ctx _sha3_128_ctx;
struct sha3_224_ctx _sha3_224_ctx;
struct sha3_256_ctx _sha3_256_ctx;
struct sha3_384_ctx _sha3_384_ctx;
struct sha3_512_ctx _sha3_512_ctx;

// define sha3 ouput buffers
uint8_t sha3_output[16*4];


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

void assign_digest_output(void)
{
    sha3_reg_list[eOUTPUT_REG0]->value  = (sha3_output[3]  << 24) | (sha3_output[2]  << 16) | (sha3_output[1]  << 8) | sha3_output[0];
    sha3_reg_list[eOUTPUT_REG1]->value  = (sha3_output[7]  << 24) | (sha3_output[6]  << 16) | (sha3_output[5]  << 8) | sha3_output[4];
    sha3_reg_list[eOUTPUT_REG2]->value  = (sha3_output[11] << 24) | (sha3_output[10] << 16) | (sha3_output[9]  << 8) | sha3_output[8];
    sha3_reg_list[eOUTPUT_REG3]->value  = (sha3_output[15] << 24) | (sha3_output[14] << 16) | (sha3_output[13] << 8) | sha3_output[12];
    sha3_reg_list[eOUTPUT_REG4]->value  = (sha3_output[19] << 24) | (sha3_output[18] << 16) | (sha3_output[17] << 8) | sha3_output[16];
    sha3_reg_list[eOUTPUT_REG5]->value  = (sha3_output[23] << 24) | (sha3_output[22] << 16) | (sha3_output[21] << 8) | sha3_output[20];
    sha3_reg_list[eOUTPUT_REG6]->value  = (sha3_output[27] << 24) | (sha3_output[26] << 16) | (sha3_output[25] << 8) | sha3_output[24];
    sha3_reg_list[eOUTPUT_REG7]->value  = (sha3_output[31] << 24) | (sha3_output[30] << 16) | (sha3_output[29] << 8) | sha3_output[28];
    sha3_reg_list[eOUTPUT_REG8]->value  = (sha3_output[35] << 24) | (sha3_output[34] << 16) | (sha3_output[33] << 8) | sha3_output[32];
    sha3_reg_list[eOUTPUT_REG9]->value  = (sha3_output[39] << 24) | (sha3_output[38] << 16) | (sha3_output[37] << 8) | sha3_output[36];
    sha3_reg_list[eOUTPUT_REG10]->value = (sha3_output[43] << 24) | (sha3_output[42] << 16) | (sha3_output[41] << 8) | sha3_output[40];
    sha3_reg_list[eOUTPUT_REG11]->value = (sha3_output[47] << 24) | (sha3_output[46] << 16) | (sha3_output[45] << 8) | sha3_output[44];
    sha3_reg_list[eOUTPUT_REG12]->value = (sha3_output[51] << 24) | (sha3_output[50] << 16) | (sha3_output[49] << 8) | sha3_output[48];
    sha3_reg_list[eOUTPUT_REG13]->value = (sha3_output[55] << 24) | (sha3_output[54] << 16) | (sha3_output[53] << 8) | sha3_output[52];
    sha3_reg_list[eOUTPUT_REG14]->value = (sha3_output[59] << 24) | (sha3_output[58] << 16) | (sha3_output[57] << 8) | sha3_output[56];
    sha3_reg_list[eOUTPUT_REG15]->value = (sha3_output[63] << 24) | (sha3_output[62] << 16) | (sha3_output[61] << 8) | sha3_output[60];
}


// callback for register
void cb_input_reg(Register32 *reg, uint32_t value) 
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    // check reset bit
    if(CONTROL_RESET_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x01)
    {
        switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
        {
            case 0x00:
                sha3_128_init(&_sha3_128_ctx);
                break;
            case 0x01:
                sha3_224_init(&_sha3_224_ctx);
                break;
            case 0x02:
                sha3_256_init(&_sha3_256_ctx);
                break;
            case 0x03:
                sha3_384_init(&_sha3_384_ctx);
                break;
            case 0x04:
                sha3_512_init(&_sha3_512_ctx);
                break;
            default:
                break;
        }

        // Reset output buffer
        for(int i = 0; i < 16*4; i++)
        {
            sha3_output[i] = 0;
        }

        // clear status register
        sha3_reg_list[eSTATUS_REG]->value = 0x00;

        // clear reset bit
        set_bits(&sha3_reg_list[eCONTROL_REG]->value, 0x00, 0, 0);
    }

    uint32_t input_data = value; // get input data from register 32 bits (4 bytes)
    set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 0, 0); // set status ready

    if(sha3_reg_list[eINPUTLEN_REG]->value <= 0x04)
    {
        // last block, get digest
        switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
        {
            case 0x00:
            {
                sha3_128_update(&_sha3_128_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                sha3_128_shake(&_sha3_128_ctx, SHA3_128_DIGEST_SIZE, (uint8_t *)&sha3_output); 
                break;
            }
            case 0x01:
            {
                sha3_224_update(&_sha3_224_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                sha3_224_digest(&_sha3_224_ctx, SHA3_224_DIGEST_SIZE, (uint8_t *)&sha3_output);
                break;
            }
            case 0x02:
            {
                sha3_256_update(&_sha3_256_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                sha3_256_digest(&_sha3_256_ctx, SHA3_256_DIGEST_SIZE, (uint8_t *)&sha3_output);
                break;
            }
            case 0x03:
            {
                sha3_384_update(&_sha3_384_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                sha3_384_digest(&_sha3_384_ctx, SHA3_384_DIGEST_SIZE, (uint8_t *)&sha3_output);
                break;
            }
            case 0x04:
            {
                sha3_512_update(&_sha3_512_ctx, sha3_reg_list[eINPUTLEN_REG]->value, (uint8_t *)&input_data);
                sha3_512_digest(&_sha3_512_ctx, SHA3_512_DIGEST_SIZE, (uint8_t *)&sha3_output);
                break;
            }
            default:
                break;
        }

        // assign output to register
        assign_digest_output();

        // set status done
        set_bits(&sha3_reg_list[eSTATUS_REG]->value, 0x01, 1, 1);
        
    }
    else
    {
        // in the case the input length is more than 4 byte
        switch(CONTROL_MODE_BIT(sha3_reg_list[eCONTROL_REG]->value))
        {
            case 0x00:
                sha3_128_update(&_sha3_128_ctx, 0x04, (uint8_t *)&input_data);
                break;
            case 0x01:
                sha3_224_update(&_sha3_224_ctx, 0x04, (uint8_t *)&input_data);
                break;
            case 0x02:
                sha3_256_update(&_sha3_256_ctx, 0x04, (uint8_t *)&input_data);
                break;
            case 0x03:
                sha3_384_update(&_sha3_384_ctx, 0x04, (uint8_t *)&input_data);
                break;
            case 0x04:
                sha3_512_update(&_sha3_512_ctx, 0x04, (uint8_t *)&input_data);
                break;
            default:
                break;
        }
        // update input length register
        sha3_reg_list[eINPUTLEN_REG]->value = sha3_reg_list[eINPUTLEN_REG]->value - 0x04; // decrease input length by 4 byte
    }
    
}


void sha3_register_init(void)
{
    sha3_reg_list[eCONTROL_REG]  = create_register32("SHA3_CONTROL_REG" , SHA3_CONTROL_REG   , REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eSTATUS_REG]   = create_register32("SHA3_STATUS_REG"  , SHA3_STATUS_REG    , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);   
    sha3_reg_list[eINPUTLEN_REG] = create_register32("SHA3_INPUTLEN_REG", SHA3_INPUTLEN_REG  , REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eINPUT_REG]    = create_register32("SHA3_INPUT_REG"   , SHA3_INPUT_REG     , REG_READ_WRITE, 0, 0xFFFFFFFF, cb_input_reg);
    sha3_reg_list[eOUTPUT_REG0]  = create_register32("SHA3_OUTPUT_REG0" , SHA3_OUTPUT_REG(0) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG1]  = create_register32("SHA3_OUTPUT_REG1" , SHA3_OUTPUT_REG(1) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG2]  = create_register32("SHA3_OUTPUT_REG2" , SHA3_OUTPUT_REG(2) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG3]  = create_register32("SHA3_OUTPUT_REG3" , SHA3_OUTPUT_REG(3) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG4]  = create_register32("SHA3_OUTPUT_REG4" , SHA3_OUTPUT_REG(4) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG5]  = create_register32("SHA3_OUTPUT_REG5" , SHA3_OUTPUT_REG(5) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG6]  = create_register32("SHA3_OUTPUT_REG6" , SHA3_OUTPUT_REG(6) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG7]  = create_register32("SHA3_OUTPUT_REG7" , SHA3_OUTPUT_REG(7) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG8]  = create_register32("SHA3_OUTPUT_REG8" , SHA3_OUTPUT_REG(8) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG9]  = create_register32("SHA3_OUTPUT_REG9" , SHA3_OUTPUT_REG(9) , REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG10] = create_register32("SHA3_OUTPUT_REG10", SHA3_OUTPUT_REG(10), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG11] = create_register32("SHA3_OUTPUT_REG11", SHA3_OUTPUT_REG(11), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG12] = create_register32("SHA3_OUTPUT_REG12", SHA3_OUTPUT_REG(12), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG13] = create_register32("SHA3_OUTPUT_REG13", SHA3_OUTPUT_REG(13), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG14] = create_register32("SHA3_OUTPUT_REG14", SHA3_OUTPUT_REG(14), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL);
    sha3_reg_list[eOUTPUT_REG15] = create_register32("SHA3_OUTPUT_REG15", SHA3_OUTPUT_REG(15), REG_READ_ONLY , 0, 0xFFFFFFFF, NULL); 
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