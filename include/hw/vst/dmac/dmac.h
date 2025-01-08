#ifndef HW_DMAC_H
#define HW_DMAC_H
#include "exec/memory.h"
#include "chardev/char.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/vst/reg_interface.h"
#include "hw/vst/vst_gpio.h"
#include "qemu/thread.h"
#include "qemu/main-loop.h"


#define TYPE_DMAC "dmac"
OBJECT_DECLARE_SIMPLE_TYPE(DMACdevice, DMAC)

typedef enum DMA_STATE
{
    eDMA_STATE_REQ = 0,
    eDMA_STATE_RUN,
    eDMA_STATE_DONE,
    eDMA_STATE_ERROR,
} DMA_STATE;

typedef struct DMAIOCH
{
    vst_gpio_pin I_trigger[32];
    vst_gpio_pin O_done[32];
    vst_gpio_pin O_req[32];
} DMAIOCH;

// DMA Channel Structure
typedef struct {
    int id;                 // Channel ID
    uint32_t src_addr;      // Source Address
    uint32_t dst_addr;      // Destination Address
    uint32_t size;          // Transfer Size
} DMA_Channel;

typedef struct DMACdevice {
    SysBusDevice parent;
    MemoryRegion io;
    DMAIOCH ch[8]; // DMA IO definition
    DMA_Channel ch_op[8]; // DMA channel operation definition
    DMA_STATE dma_state[8]; // DMA channel state
} DMACdevice;

DMACdevice *dmac_init(MemoryRegion *address_space, hwaddr base);

#endif