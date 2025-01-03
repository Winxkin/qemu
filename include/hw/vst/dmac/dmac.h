#ifndef HW_DMAC_H
#define HW_DMAC_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"
#include "hw/vst/vst_gpio.h"
#include "qemu/thread.h"

#define TYPE_DMAC "dmac"
OBJECT_DECLARE_SIMPLE_TYPE(DMACdevice, DMAC)

typedef struct DMACH
{
    vst_gpio_pin I_trigger[32];
    vst_gpio_pin O_done;
    vst_gpio_pin O_req;
} DMACH;

typedef struct DMACdevice {
    SysBusDevice parent;
    MemoryRegion io;
    DMACH ch[8];
    QemuThread thread;
    QemuEvent event;
} DMACdevice;

DMACdevice *dmac_init(MemoryRegion *address_space, hwaddr base);

#endif