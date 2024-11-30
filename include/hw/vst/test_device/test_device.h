#ifndef HW_TEST_DEVICE_H
#define HW_TEST_DEVICE_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"

#define TYPE_TEST_DEVICE "test-device"
OBJECT_DECLARE_SIMPLE_TYPE(Testdevice, TEST_DEVICE)

struct Testdevice {
    SysBusDevice parent;
    uint32_t reg_value;
    qemu_irq irq;  // IRQ line
    MemoryRegion io;
};

void cb_reg_01(Register *reg, uint32_t value);
void cb_reg_02(Register *reg, uint32_t value);
void cb_reg_03(Register *reg, uint32_t value);
void cb_reg_04(Register *reg, uint32_t value);
void test_device_register_init(void);

Testdevice *test_device_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq);


#endif