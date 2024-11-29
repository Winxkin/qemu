#include "qemu/osdep.h"
#include "hw/vst/test_device/test_device.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/vst/reg_interface.h"

#define REG_01 0x00
#define REG_02 0x04
#define REG_03 0x08
#define REG_04 0x0C
#define MAX_REG 4
Register *tsd_reg_list[MAX_REG];

void cb_reg_01(Register *reg, uint32_t value) {
    qemu_log("[test-device] Callback for register %s invoked with value 0x%X\n", reg->name, value);
}

void cb_reg_02(Register *reg, uint32_t value) {
    qemu_log("[test-device] Callback for register %s invoked with value 0x%X\n", reg->name, value);
}

void cb_reg_03(Register *reg, uint32_t value) {
    qemu_log("[test-device] Callback for register %s invoked with value 0x%X\n", reg->name, value);
}

void cb_reg_04(Register *reg, uint32_t value) {
    qemu_log("[test-device] Callback for register %s invoked with value 0x%X\n", reg->name, value);
}

void test_device_register_init(void)
{
    tsd_reg_list[0] = create_register("REG_01", REG_01, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_01);
    tsd_reg_list[1] = create_register("REG_02", REG_02, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_02);
    tsd_reg_list[2] = create_register("REG_03", REG_03, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_03);
    tsd_reg_list[3] = create_register("REG_04", REG_04, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_04);
}

/*This is template code for registration new device in qemu*/

static uint64_t test_device_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log("[test-device] read at offset address 0x%X\n", (uint32_t)addr);
    uint32_t value = 0;
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == tsd_reg_list[i]->base_addr) 
        {
            value = read_register(tsd_reg_list[i]);
            break;
        }
    }
    return value;
}

static void test_device_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    qemu_log("[test-device] write 0x%lX at offset address 0x%X\n", value, (uint32_t)addr);
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == tsd_reg_list[i]->base_addr) 
        {
            write_register(tsd_reg_list[i], value);
        }
    }
}

static const VMStateDescription vmstate_test_device = {
    .name = "test-device",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static const MemoryRegionOps test_device_ops = {
    .read = test_device_read,
    .write = test_device_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.max_access_size = 8,
    .impl.max_access_size = 8,
};

static void test_device_realize(DeviceState *dev, Error **errp)
{
    Testdevice *tsd = TEST_DEVICE(dev);

    memory_region_init_io(&tsd->io, OBJECT(dev), 
                &test_device_ops, tsd,"test-device", 0x1000);

    sysbus_init_mmio(SYS_BUS_DEVICE(tsd), &tsd->io);
    sysbus_init_irq(SYS_BUS_DEVICE(tsd), &tsd->irq);

}

Testdevice *test_device_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq)
{
    Testdevice *tsd = TEST_DEVICE(qdev_new(TYPE_TEST_DEVICE));
    MemoryRegion *mr;

   // Realize the device and connect IRQ
    sysbus_realize_and_unref(SYS_BUS_DEVICE(tsd), &error_fatal);
    sysbus_connect_irq(SYS_BUS_DEVICE(tsd), 0, irq);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(tsd), 0);
    memory_region_add_subregion(address_space, base, mr);

    /*initialize register*/
    test_device_register_init();

    qemu_log("test-device initialized\n");
    return tsd;
}


static void test_device_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = test_device_realize;
    dc->vmsd = &vmstate_test_device;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_TEST_DEVICE,
        .parent = TYPE_SYS_BUS_DEVICE,
        .class_init = test_device_class_init,
        .instance_size = sizeof(Testdevice),
    },
};

DEFINE_TYPES(types)

