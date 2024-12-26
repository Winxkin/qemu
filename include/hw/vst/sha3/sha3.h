#ifndef HW_SHA3_H
#define HW_SHA3_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"

#define TYPE_SHA3 "sha3"
OBJECT_DECLARE_SIMPLE_TYPE(SHA3device, SHA3)

typedef struct SHA3device {
    SysBusDevice parent;
    qemu_irq irq_err;  // IRQ line
    qemu_irq irq_done; // IRQ line
    MemoryRegion io;
} SHA3device;


void sha3_register_init(void);

SHA3device *sha3_init(MemoryRegion *address_space,
                         hwaddr base, qemu_irq irq_err, qemu_irq irq_done);


#endif