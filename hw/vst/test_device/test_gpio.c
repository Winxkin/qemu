#include "qemu/osdep.h"
#include "hw/vst/test_device/test_gpio.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/vst/reg_interface.h"
#include "hw/qdev-core.h"
#include "hw/irq.h"

#define REG_SIGNAL  0x00
#define REG_GPIO    0x04
#define MAX_REG 2
Register32 *tsg_reg_list[MAX_REG];

void test_gpio_register_init(void);
void test_gpio_gpio_init(Testgpio *tsd);

void cb_reg_signal(void *opaque, Register32 *reg, uint32_t value);
void cb_reg_gpio(void *opaque, Register32 *reg, uint32_t value);

void cb_reg_signal(void *opaque, Register32 *reg, uint32_t value) {
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    // Testgpio *tsg = TEST_GPIO(opaque);

}

void cb_reg_gpio(void *opaque, Register32 *reg, uint32_t value) {
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    // Testgpio *tsg = TEST_GPIO(opaque);

    
}


void test_gpio_register_init(void)
{
    tsg_reg_list[0] = create_register32("REG_SIGNAL", REG_SIGNAL, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_signal);
    tsg_reg_list[1] = create_register32("REG_GPIO", REG_GPIO, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_gpio);
}

void test_gpio_gpio_init(Testgpio *tsd)
{
    
}

/*This is template code for registration new device in qemu*/

static uint64_t test_gpio_read(void *opaque, hwaddr addr, unsigned size)
{
    // qemu_log("[test-gpio] read at offset address 0x%X\n", (uint32_t)addr);
    uint32_t value = 0;
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == tsg_reg_list[i]->base_addr) 
        {
            value = read_register32(opaque, tsg_reg_list[i]);
            break;
        }
    }
    return value;
}

static void test_gpio_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    // qemu_log("[test-gpio] write 0x%lX at offset address 0x%X\n", value, (uint32_t)addr);
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == tsg_reg_list[i]->base_addr) 
        {
            write_register32(opaque, tsg_reg_list[i], value);
        }
    }
}

static const VMStateDescription vmstate_test_gpio = {
    .name = "test-gpio",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static const MemoryRegionOps test_gpio_ops = {
    .read = test_gpio_read,
    .write = test_gpio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.max_access_size = 8,
    .impl.max_access_size = 8,
};

static void test_gpio_realize(DeviceState *dev, Error **errp)
{
    Testgpio *tsg = TEST_GPIO(dev);

    memory_region_init_io(&tsg->io, OBJECT(dev), 
                &test_gpio_ops, tsg,"test-gpio", 0x100);

    sysbus_init_mmio(SYS_BUS_DEVICE(tsg), &tsg->io);
    sysbus_init_irq(SYS_BUS_DEVICE(tsg), &tsg->irq);

}

Testgpio *test_gpio_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq)
{
    Testgpio *tsg = TEST_GPIO(qdev_new(TYPE_TEST_GPIO));
    MemoryRegion *mr;

   // Realize the device and connect IRQ
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tsg), &error_fatal);
    sysbus_connect_irq(SYS_BUS_DEVICE(tsg), 0, irq);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(tsg), 0);
    memory_region_add_subregion(address_space, base, mr);

    /*initialize gpio*/



    /*initialize register*/
    test_gpio_register_init();

    qemu_log("test-gpio initialized\n");
    return tsg;
}


static void test_gpio_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = test_gpio_realize;
    dc->vmsd = &vmstate_test_gpio;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_TEST_GPIO,
        .parent = TYPE_SYS_BUS_DEVICE,
        .class_init = test_gpio_class_init,
        .instance_size = sizeof(Testgpio),
    },
};

DEFINE_TYPES(types)