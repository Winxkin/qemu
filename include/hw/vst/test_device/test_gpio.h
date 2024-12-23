#ifndef HW_TEST_GPIO_H
#define HW_TEST_GPIO_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"

#define TYPE_TEST_GPIO "test-gpio"
OBJECT_DECLARE_SIMPLE_TYPE(Testgpio, TEST_GPIO)

struct Testgpio {
    SysBusDevice parent;
    uint32_t reg_value;
    qemu_irq irq;  // IRQ line
    qemu_irq signal_out[1];  // Signal output line
    qemu_irq gpio_out[32];  // GPIO input lines
    MemoryRegion io;
};


void test_gpio_register_init(void);

Testgpio *test_gpio_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq);


#endif