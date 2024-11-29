#include "qemu/osdep.h"
#include "hw/vst/test_device/test_device.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"

static uint64_t test_device_read(void *opaque, hwaddr addr, unsigned size)
{
    // SerialMM *s = SERIAL_MM(opaque);
    return 0;
}

static void test_device_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    // SerialMM *s = SERIAL_MM(opaque);
    
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

    memory_region_init_io(tsd->parent.mmio[0].memory, OBJECT(dev), 
                &test_device_ops, tsd,"test-device", 4 * sizeof(uint32_t));

    sysbus_init_mmio(SYS_BUS_DEVICE(tsd), tsd->parent.mmio[0].memory);
    sysbus_init_irq(SYS_BUS_DEVICE(tsd), &tsd->irq);

    qemu_log("test device initialized\n");
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

