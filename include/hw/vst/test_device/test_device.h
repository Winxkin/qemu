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
    qemu_irq signal_in[1];  // Signal input line 
    qemu_irq gpio_in[32];  // GPIO input lines
    MemoryRegion io;
};


void test_device_register_init(void);

Testdevice *test_device_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq);


#endif