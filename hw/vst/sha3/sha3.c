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

// define bit for SHA3_CONTROL_REG
// bit 0: reset
// bit 1-2: mode
#define CONTROL_RESET_BIT(val) ((val >> 0) & 0x1)
#define CONTROL_MODE_BIT(val)  ((val >> 1) & 0x03) 

// define bit for SHA3_STATUS_REG
// bit 0: ready
// bit 1: done
// bit 2: error
#define STATUS_READY_BIT(val) ((val >> 0) & 0x1)
#define STATUS_DONE_BIT(val)  ((val >> 1) & 0x1)
#define STATUS_ERROR_BIT(val) ((val >> 2) & 0x1)


#define MAX_REG 20
Register32 *sha3_reg_list[MAX_REG];

// Define additional functions in here
void set_bits(uint32_t *reg_val, uint32_t value, uint32_t bit_start, uint32_t bit_end);

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


// callback for register
void cb_input_reg(Register32 *reg, uint32_t value) 
{
    qemu_log("[sha3] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    // check reset bit
    if(CONTROL_RESET_BIT(sha3_reg_list[eCONTROL_REG]->value) == 0x01)
    {
        // reset sha3 cxt
    }
    else
    {
        // sha3 process

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