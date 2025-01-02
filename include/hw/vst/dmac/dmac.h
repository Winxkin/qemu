#ifndef HW_DMAC_H
#define HW_DMAC_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"

#define TYPE_DMAC "dmac"
OBJECT_DECLARE_SIMPLE_TYPE(DMACdevice, DMAC)

typedef struct DMACdevice {
    SysBusDevice parent;
    MemoryRegion io;
} DMACdevice;

DMACdevice *dmac_init(MemoryRegion *address_space, hwaddr base);

#endif