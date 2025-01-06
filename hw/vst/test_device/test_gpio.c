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
#include "exec/memory.h"
#include "exec/address-spaces.h"

#define REG_PIN1    0x00
#define REG_PORT1   0x04
#define REG_PORT2   0x08
#define REG_ADDR    0x0C
#define REG_TRANS   0x10
#define REG_READ    0x14
#define MAX_REG 6
Register32 *tsg_reg_list[MAX_REG];

typedef enum REG_NAME
{
    e_REG_PIN1 =0,
    e_REG_PORT1,
    e_REG_PORT2,
    e_REG_ADDR,
    e_REG_TRANS,
    e_REG_READ
} REG_NAME;

void test_gpio_register_init(void);
void test_gpio_gpio_init(Testgpio *tsd);

void cb_reg_pin1(void *opaque, Register32 *reg, uint32_t value);
void cb_reg_port1(void *opaque, Register32 *reg, uint32_t value);
void cb_reg_port2(void *opaque, Register32 *reg, uint32_t value);
void cb_reg_trans(void *opaque, Register32 *reg, uint32_t value);
void cb_reg_read(void *opaque, Register32 *reg, uint32_t value);

void cb_reg_read(void *opaque, Register32 *reg, uint32_t value)
{
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);

    hwaddr addr = (hwaddr)tsg_reg_list[e_REG_ADDR]->value ;     // Target memory address
    MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
    uint32_t data;                                              // Data to read
    hwaddr len = 0x04;

    MemTxResult result = address_space_rw(&address_space_memory, addr, attrs, &data, len, false);
    
    qemu_log("[test-gpio] Read data: 0x%X\n", data);

    tsg_reg_list[e_REG_READ]->value = (uint32_t)data;

    if (result == MEMTX_OK) {
        qemu_log("[test-gpio] Read succeeded!\n");
    } else {
        qemu_log("[test-gpio] Read failed with result: %d\n", result);
    }
}

void cb_reg_trans(void *opaque, Register32 *reg, uint32_t value)
{
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);

    hwaddr addr = (hwaddr)tsg_reg_list[e_REG_ADDR]->value ;     // Target memory address
    MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
    uint32_t data = value;                                      // Data to write
    hwaddr len = 0x04;

    MemTxResult result = address_space_write(&address_space_memory, addr, attrs, &data, len);

    if (result == MEMTX_OK) {
        qemu_log("[test-gpio] Write succeeded!\n");
    } else {
        qemu_log("[test-gpio] Write failed with result: %d\n", result);
    }
}

void cb_reg_pin1(void *opaque, Register32 *reg, uint32_t value) {
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    Testgpio *tsg = TEST_GPIO(opaque);
    if(value == 0)
    {
        vst_gpio_write(&tsg->O_pin1, GPIO_LOW);
    }
    else
    {
        vst_gpio_write(&tsg->O_pin1, GPIO_HIGH);
    }
}

void cb_reg_port1(void *opaque, Register32 *reg, uint32_t value) {
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    Testgpio *tsg = TEST_GPIO(opaque);
    vst_port_write(&tsg->O_port1, value);
}

void cb_reg_port2(void *opaque, Register32 *reg, uint32_t value) {
    qemu_log("[test-gpio] Callback for register %s invoked with value 0x%X\n", reg->name, value);
    Testgpio *tsg = TEST_GPIO(opaque);
    vst_port_write(&tsg->O_port2, value);
}

void test_gpio_register_init(void)
{
    tsg_reg_list[e_REG_PIN1] = create_register32("REG_PIN1", REG_PIN1, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_pin1);
    tsg_reg_list[e_REG_PORT1] = create_register32("REG_PORT1", REG_PORT1, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_port1);
    tsg_reg_list[e_REG_PORT2] = create_register32("REG_PORT2", REG_PORT2, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_reg_port2);
    tsg_reg_list[e_REG_ADDR] = create_register32("REG_ADDR", REG_ADDR, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    tsg_reg_list[e_REG_TRANS] = create_register32("REG_TRANS", REG_TRANS, REG_READ_WRITE , 0, 0xFFFFFFFF, cb_reg_trans);
    tsg_reg_list[e_REG_READ] = create_register32("REG_READ", REG_READ, REG_READ_WRITE , 0, 0xFFFFFFFF, cb_reg_read);
}

void test_gpio_gpio_init(Testgpio *tsd)
{
    vst_port_init(&tsd->O_port1, "O_PORT1", 32, GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_port_init(&tsd->O_port2, "O_PORT2", 16, GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&tsd->O_pin1, "O_PIN1", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
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
    test_gpio_gpio_init(tsg);

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