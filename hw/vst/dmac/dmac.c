#include "qemu/osdep.h"
#include "hw/vst/dmac/dmac.h"
#include "hw/irq.h"
#include "exec/cpu-common.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "qemu/log.h"
#include "hw/qdev-core.h"
#include "hw/irq.h"
#include "exec/memory.h"
#include "exec/address-spaces.h"

/*define base address for DMAC registers*/
// DMAC_CTRLx_REG   0x00 -> 0x1C
#define DMA_CTRL0_REG    0x00
#define DMA_CTRL1_REG    0x04
#define DMA_CTRL2_REG    0x08
#define DMA_CTRL3_REG    0x0C
#define DMA_CTRL4_REG    0x10
#define DMA_CTRL5_REG    0x14
#define DMA_CTRL6_REG    0x18
#define DMA_CTRL7_REG    0x1C

// DMAC_CFGy_REG    0x20 -> 0x30
#define DMA_CFG0_REG      0x20
#define DMA_CFG1_REG      0x24
#define DMA_CFG2_REG      0x28
#define DMA_CFG3_REG      0x2C

// DMAC_SRCx_REG    0x30 -> 0x4C
#define DMA_SRC0_REG      0x30
#define DMA_SRC1_REG      0x34
#define DMA_SRC2_REG      0x38
#define DMA_SRC3_REG      0x3C
#define DMA_SRC4_REG      0x40
#define DMA_SRC5_REG      0x44
#define DMA_SRC6_REG      0x48
#define DMA_SRC7_REG      0x4C

// DMAC_DSTx_REG     0x50 -> 0x6C
#define DMA_DST0_REG      0x50
#define DMA_DST1_REG      0x54
#define DMA_DST2_REG      0x58
#define DMA_DST3_REG      0x5C
#define DMA_DST4_REG      0x60
#define DMA_DST5_REG      0x64
#define DMA_DST6_REG      0x68
#define DMA_DST7_REG      0x6C

// DMAC_SIZEx_REG   0x70 -> 0x8C
#define DMA_SIZE0_REG     0x70
#define DMA_SIZE1_REG     0x74
#define DMA_SIZE2_REG     0x78
#define DMA_SIZE3_REG     0x7C
#define DMA_SIZE4_REG     0x80  
#define DMA_SIZE5_REG     0x84
#define DMA_SIZE6_REG     0x88
#define DMA_SIZE7_REG     0x8C

// DMAC_STATUSy_REG  0x90 
#define DMA_STATUS0_REG    0x90
#define DMA_STATUS1_REG    0x94
#define DMA_STATUS2_REG    0x98
#define DMA_STATUS3_REG    0x9C

// DMAC_TRIGGERx_REG  0xA0 (using for testing mode)
#define DMA_TRIGGER0_REG   0xA0
#define DMA_TRIGGER1_REG   0xA4
#define DMA_TRIGGER2_REG   0xA8
#define DMA_TRIGGER3_REG   0xAC
#define DMA_TRIGGER4_REG   0xB0
#define DMA_TRIGGER5_REG   0xB4
#define DMA_TRIGGER6_REG   0xB8
#define DMA_TRIGGER7_REG   0xBC

typedef enum REGISTER_NAME
{
    eDMA_CTRL0_REG = 0,
    eDMA_CTRL1_REG,
    eDMA_CTRL2_REG,
    eDMA_CTRL3_REG,
    eDMA_CTRL4_REG,
    eDMA_CTRL5_REG,
    eDMA_CTRL6_REG,
    eDMA_CTRL7_REG,
    eDMA_CFG0_REG,
    eDMA_CFG1_REG,
    eDMA_CFG2_REG,
    eDMA_CFG3_REG,
    eDMA_SRC0_REG,
    eDMA_SRC1_REG,
    eDMA_SRC2_REG,
    eDMA_SRC3_REG,
    eDMA_SRC4_REG,
    eDMA_SRC5_REG,
    eDMA_SRC6_REG,
    eDMA_SRC7_REG,
    eDMA_DST0_REG,
    eDMA_DST1_REG,
    eDMA_DST2_REG,
    eDMA_DST3_REG,
    eDMA_DST4_REG,
    eDMA_DST5_REG,
    eDMA_DST6_REG,
    eDMA_DST7_REG,
    eDMA_SIZE0_REG,
    eDMA_SIZE1_REG,
    eDMA_SIZE2_REG,
    eDMA_SIZE3_REG,
    eDMA_SIZE4_REG,
    eDMA_SIZE5_REG,
    eDMA_SIZE6_REG,
    eDMA_SIZE7_REG,
    eDMA_STATUS0_REG,
    eDMA_STATUS1_REG,
    eDMA_STATUS2_REG,
    eDMA_STATUS3_REG,
    eDMA_TRIGGER0_REG,
    eDMA_TRIGGER1_REG,
    eDMA_TRIGGER2_REG,
    eDMA_TRIGGER3_REG,
    eDMA_TRIGGER4_REG,
    eDMA_TRIGGER5_REG,
    eDMA_TRIGGER6_REG,
    eDMA_TRIGGER7_REG,
} REGISTER_NAME;

/*
---Define bits for DMAC_CTRLx_REG--- (the x index corresponds to the number of channels from 0 to 7)
        bit 0: DMAEN : default 0x00     (when the DMAEN is clear, DMAC stop operation)
            - 0x00 : disable
            - 0x01 : enable
        bit 1: DMALEVEL : default 0x00  (DMALEVEL is used to select the level sensitive or edge sensitive)
            - 0x00 : edge sensitive
            - 0x01 : level sensitive
        bit 2-3: reserved
        bit 4-6: MODE : default 0x00
            - 0x00 : Single transfer (Each transfer require a trigger. DMAEN is automatically cleared when DMA_SIZE_REG is decremented to 0)
            - 0x01 : Block transfer (A complete blok is transfered with one trigger. DMAEN is automatically cleared at the end of burst-block transfer)
            - 0x02, 0x03: Brust-block transfer (CPU activity is interleaved with a block transfer. DMAEN is automatically cleared at the end of the brust-transfer)
            - 0x04: Repeated single transfer (Each transfer require a trigger. DMAEN remain enabled until it is cleared by software)
            - 0x05: Repeated block transfer (A complete block is transfered with one trigger. DMAEN remain enabled until it is cleared by software)
            - 0x06, 0x07: Repeated brust-block transfer (CPU activity is interleaved with a block transfer. DMAEN remains enabled until it is cleared by software)
        bit 7: reserved
        bit 8-9: DSTINC : default 0x00  (Destination address increment. When DSTBYTE = 1, DSTINC is increased by 1 byte. When DSTBYTE = 0, DSTINC is increased by 4 bytes)
            - 0x00 : Destination no increment
            - 0x01 : Destination no increment
            - 0x02 : Destination is decrement
            - 0x03 : Destination is increment
        bit 10-11: SRCINC : default 0x00 (Source address increment. When SRCBYTE = 1, SRCINC is increased by 1 byte. When SRCBYTE = 0, SRCINC is increased by 4 bytes)
            - 0x00 : Source no increment
            - 0x01 : Source no increment
            - 0x02 : Source is decrement
            - 0x03 : Source is increment
        bit 12: DSTBYTE : default 0x00     (Destination byte size)
            - 0x00 : 4 bytes
            - 0x01 : 1 byte
        bit 13: SRCBYTE : default 0x00     (Source byte size)
            - 0x00 : 4 bytes 
            - 0x01 : 1 byte
        bit 14-30: reserved
        bit 31: TEST : default 0x00
            - 0x00 : normal operation
            - 0x01 : test mode
*/

#define CTRL_DMAEN_BIT(val)     ((val >> 0) & 0x1)
#define CTRL_DMALEVEL_BIT(val)  ((val >> 1) & 0x1)
#define CTRL_MODE_BIT(val)      ((val >> 4) & 0x7)
#define CTRL_DSTINC_BIT(val)    ((val >> 8) & 0x3)
#define CTRL_SRCINC_BIT(val)    ((val >> 10) & 0x3)
#define CTRL_DSTBYTE_BIT(val)   ((val >> 12) & 0x1)
#define CTRL_SRCBYTE_BIT(val)   ((val >> 13) & 0x1)
#define CTRL_TEST_BIT(val)      ((val >> 31) & 0x1)

/*
---Define bits for DMAC_CFG0_REG--- (y index from 0 to 4)
        bit 0-4: CH0SEL : default 0x00 (DMA channel 0 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 5-15: reserved
        bit 16-20: CH1SEL : default 0x00 (DMA channel 1 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 21-31: reserved
*/

#define CFG_CH0SEL_BIT(val)    ((val >> 0) & 0x1F)
#define CFG_CH1SEL_BIT(val)    ((val >> 16) & 0x1F)

/*
---Define bits for DMAC_CFG1_REG--- (y index from 0 to 4)
        bit 0-4: CH2SEL : default 0x00 (DMA channel 2 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 5-15: reserved
        bit 16-20: CH3SEL : default 0x00 (DMA channel 3 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 21-31: reserved
*/

#define CFG_CH2SEL_BIT(val)    ((val >> 0) & 0x1F)
#define CFG_CH3SEL_BIT(val)    ((val >> 16) & 0x1F)

/*
---Define bits for DMAC_CFG2_REG--- (y index from 0 to 4)
        bit 0-4: CH4SEL : default 0x00 (DMA channel 4 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 5-15: reserved
        bit 16-20: CH5SEL : default 0x00 (DMA channel 5 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 21-31: reserved
*/

#define CFG_CH4SEL_BIT(val)    ((val >> 0) & 0x1F)
#define CFG_CH5SEL_BIT(val)    ((val >> 16) & 0x1F)

/*
---Define bits for DMAC_CFG_REG--- (y index from 0 to 4)
        bit 0-4: CH6SEL : default 0x00 (DMA channel 6 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 5-15: reserved
        bit 16-20: CH7SEL : default 0x00 (DMA channel 7 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 21-31: reserved
*/

#define CFG_CH6SEL_BIT(val)    ((val >> 0) & 0x1F)
#define CFG_CH7SEL_BIT(val)    ((val >> 16) & 0x1F)

/*
---Define bits for DMA_SRCx_REG--- (the x index corresponds to the number of channels from 0 to 7)
        bit 0-31: SRCADDR : default 0x00 (Source address)
*/

/*
---Define bits for DMA_DSTx_REG--- (the x index corresponds to the number of channels from 0 to 7)
        bit 0-31: DSTADDR : default 0x00 (Destination address)
*/

/*
---Define bits for DMA_SIZEx_REG--- (the x index corresponds to the number of channels from 0 to 7)
        bit 0-31: DMASZ : default 0x00 (DMA size. The DMA size register defines the number of byte/word to be transferred, 
                                        it is decremented after each transfer. When the DMA size register reaches 0, the DMAEN bit is cleared)
*/

/*
---Define bits for DMA_STATUS0_REG---
        bit 0: CH0DONE : default 0x00 (Indicating that the DMA channel 0 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 1: CH0RUN : default 0x00  (Indicating that the DMA channel 0 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 2: CH0REQ : default 0x00  
        (Indicating that the DMA channel 0 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 3: CH0ERR : default 0x00  (Indicating that the DMA channel 0 transfer is error)
            - 0x00 : no error
            - 0x01 : error
        bit 4-15: reserved
        bit 16: CH1DONE : default 0x00 (Indicating that the DMA channel 1 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 17: CH1RUN : default 0x00  (Indicating that the DMA channel 1 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 18: CH1REQ : default 0x00  
        (Indicating that the DMA channel 1 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 19: CH1ERR : default 0x00  (Indicating that the DMA channel 1 transfer is error)
            - 0x00 : no error
            - 0x01 : error
*/

#define STATUS_CH0DONE_BIT(val)     ((val >> 0) & 0x1)
#define STATUS_CH0RUN_BIT(val)      ((val >> 1) & 0x1)
#define STATUS_CH0REQ_BIT(val)      ((val >> 2) & 0x1)
#define STATUS_CH0ERR_BIT(val)      ((val >> 3) & 0x1)
#define STATUS_CH1DONE_BIT(val)     ((val >> 16) & 0x1) 
#define STATUS_CH1RUN_BIT(val)      ((val >> 17) & 0x1)
#define STATUS_CH1REQ_BIT(val)      ((val >> 18) & 0x1)
#define STATUS_CH1ERR_BIT(val)      ((val >> 19) & 0x1)

/*---Define bits for DMA_STATUS1_REG---
        bit 0: CH2DONE : default 0x00 (Indicating that the DMA channel 2 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 1: CH2RUN : default 0x00  (Indicating that the DMA channel 2 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 2: CH2REQ : default 0x00  
        (Indicating that the DMA channel 2 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 3: CH2ERR : default 0x00  (Indicating that the DMA channel 2 transfer is error)
            - 0x00 : no error
            - 0x01 : error
        bit 4-15: reserved
        bit 16: CH3DONE : default 0x00 (Indicating that the DMA channel 3 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 17: CH3RUN : default 0x00  (Indicating that the DMA channel 3 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 18: CH3REQ : default 0x00  
        (Indicating that the DMA channel 3 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 19: CH3ERR : default 0x00  (Indicating that the DMA channel 3 transfer is error)
            - 0x00 : no error
            - 0x01 : error
*/

#define STATUS_CH2DONE_BIT(val)     ((val >> 0) & 0x1)
#define STATUS_CH2RUN_BIT(val)      ((val >> 1) & 0x1)
#define STATUS_CH2REQ_BIT(val)      ((val >> 2) & 0x1)
#define STATUS_CH2ERR_BIT(val)      ((val >> 3) & 0x1)
#define STATUS_CH3DONE_BIT(val)     ((val >> 16) & 0x1)
#define STATUS_CH3RUN_BIT(val)      ((val >> 17) & 0x1)
#define STATUS_CH3REQ_BIT(val)      ((val >> 18) & 0x1)
#define STATUS_CH3ERR_BIT(val)      ((val >> 19) & 0x1)

/*---Define bits for DMA_STATUS2_REG---
        bit 0: CH4DONE : default 0x00 (Indicating that the DMA channel 4 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 1: CH4RUN : default 0x00  (Indicating that the DMA channel 4 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 2: CH4REQ : default 0x00  
        (Indicating that the DMA channel 4 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 3: CH4ERR : default 0x00  (Indicating that the DMA channel 4 transfer is error)
            - 0x00 : no error
            - 0x01 : error
        bit 4-15: reserved
        bit 16: CH5DONE : default 0x00 (Indicating that the DMA channel 5 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 17: CH5RUN : default 0x00  (Indicating that the DMA channel 5 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 18: CH5REQ : default 0x00  
        (Indicating that the DMA channel 5 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 19: CH5ERR : default 0x00  (Indicating that the DMA channel 5 transfer is error)
            - 0x00 : no error
            - 0x01 : error
*/

#define STATUS_CH4DONE_BIT(val)     ((val >> 0) & 0x1)
#define STATUS_CH4RUN_BIT(val)      ((val >> 1) & 0x1)
#define STATUS_CH4REQ_BIT(val)      ((val >> 2) & 0x1)
#define STATUS_CH4ERR_BIT(val)      ((val >> 3) & 0x1)      
#define STATUS_CH5DONE_BIT(val)     ((val >> 16) & 0x1)
#define STATUS_CH5RUN_BIT(val)      ((val >> 17) & 0x1)
#define STATUS_CH5REQ_BIT(val)      ((val >> 18) & 0x1)
#define STATUS_CH5ERR_BIT(val)      ((val >> 19) & 0x1)

/*---Define bits for DMA_STATUS3_REG---
        bit 0: CH6DONE : default 0x00 (Indicating that the DMA channel 6 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 1: CH6RUN : default 0x00  (Indicating that the DMA channel 6 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 2: CH6REQ : default 0x00  
        (Indicating that the DMA channel 6 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 3: CH6ERR : default 0x00  (Indicating that the DMA channel 6 transfer is error)
            - 0x00 : no error
            - 0x01 : error
        bit 4-15: reserved
        bit 16: CH7DONE : default 0x00 (Indicating that the DMA channel 7 transfer is done)
            - 0x00 : not done
            - 0x01 : done
        bit 17: CH7RUN : default 0x00  (Indicating that the DMA channel 7 transfer is running)
            - 0x00 : not running
            - 0x01 : running
        bit 18: CH7REQ : default 0x00  
        (Indicating that the DMA channel 7 transfer requests for the next triggered from peripheral, edge triggered or level sensitive that depends on the DMALEVEL bit)
            - 0x00 : not requested
            - 0x01 : requested
        bit 19: CH7ERR : default 0x00  (Indicating that the DMA channel 7 transfer is error)
            - 0x00 : no error
            - 0x01 : error
*/

#define STATUS_CH6DONE_BIT(val)     ((val >> 0) & 0x1)
#define STATUS_CH6RUN_BIT(val)      ((val >> 1) & 0x1)
#define STATUS_CH6REQ_BIT(val)      ((val >> 2) & 0x1)
#define STATUS_CH6ERR_BIT(val)      ((val >> 3) & 0x1)
#define STATUS_CH7DONE_BIT(val)     ((val >> 16) & 0x1)
#define STATUS_CH7RUN_BIT(val)      ((val >> 17) & 0x1)
#define STATUS_CH7REQ_BIT(val)      ((val >> 18) & 0x1)
#define STATUS_CH7ERR_BIT(val)      ((val >> 19) & 0x1)

/*
---Define bits for DMA_TRIGGERx_REG--- (the x index corresponds to the number of channels from 0 to 7)
        bit 0-31: TRIGGER : default 0x00 (DMA trigger source from 0 to 31 at channel x)
*/

#define TRIGGER_BIT(bit,val)     ((val >> bit) & 0x1)


#define MAX_REG 48
Register32 *dmac_reg_list[MAX_REG];

QemuSpin spinlock; // Declare a spinlock
DMACdevice *gdmac; // Declare a global DMAC device
/* Internal functions*/

/* The DMA operations are depicted in here*/
void dma_set_error(uint32_t id);
void dma_set_done(uint32_t id);
void dma_set_run(uint32_t id);
void dma_set_req(uint32_t id);
void dma_transfer(DMA_Channel *channel);
void dma_single_transfer(DMA_Channel *channel);
void dma_block_transfer(DMA_Channel *channel);
void dma_repeated_single_transfer(DMA_Channel *channel);
void dma_repeated_block_transfer(DMA_Channel *channel);

void dma_transfer(DMA_Channel *channel)
{
    DMA_Channel *dma_ch = (DMA_Channel *)channel;

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
    {
        // DMAC is disabled
        qemu_log("[dmac] (Error) Channel %d: DMAC is disabled\n", dma_ch->id);
        return;
    }

    if(CTRL_SRCBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) != CTRL_DSTBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
    {
        qemu_log("[dmac] (Error) Channel %d: Source and destination byte size are not the same\n", dma_ch->id);
        dma_set_error(dma_ch->id);
        return;
    }

    // DMA transfering in here
    if(CTRL_MODE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x00) // Single transfer mode
    {
        dma_single_transfer(dma_ch);
    }
    else if(CTRL_MODE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x01) // block transfer mode
    {
        dma_block_transfer(dma_ch);
    }
    else if(CTRL_MODE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x04) // repeated single transfer mode
    {
        dma_repeated_single_transfer(dma_ch);
    }
    else if(CTRL_MODE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x05) // repeated block transfer mode
    {
        dma_repeated_block_transfer(dma_ch);
    }
    else
    {
        qemu_log("[dmac] (Error) Channel %d: Unsupported mode\n", dma_ch->id);
        dma_set_error(dma_ch->id);
    }
}

void dma_single_transfer(DMA_Channel *channel)
{
    DMA_Channel *dma_ch = (DMA_Channel *)channel;
    qemu_log("[dmac] Channel %d: Single transfer mode\n", dma_ch->id);
    uint32_t dma_byte;

    if(CTRL_SRCBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
    {
        dma_byte = 1;
        hwaddr src_addr = (hwaddr)dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value ;     // src memory address
        hwaddr dst_addr = (hwaddr)dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value ;     // src memory address
        MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
        char buffer;                                              // buffer 1 bytes
        MemTxResult result;

        // DMA read from source address
        result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
        }
        else
        {
            qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
        }

        // DMA write to destination address
        result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
        }
        else
        {
            qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
        }
    }
    else
    {
        dma_byte = 4;
        hwaddr src_addr = (hwaddr)dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value ;     // src memory address
        hwaddr dst_addr = (hwaddr)dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value ;     // src memory address
        MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
        uint32_t buffer;                                              // buffer 4 bytes
        MemTxResult result;

        // DMA read from source address
        result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
        }
        else
        {
            qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
        }
        
        // DMA write to destination address
        result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
        }
        else
        {
            qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
        }
    }

    
    // modify DMA_DST
    if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
    {
        dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value += dma_byte;
    }
    else if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
    {
        dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value -= dma_byte;
    }

    // modify DMA_SRC
    if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
    {
        dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value += dma_byte;
    }
    else if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
    {
        dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value -= dma_byte;
    }

    // Decrease size
    dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value -= dma_byte;
    if(dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value == 0)
    {
        // Set DMA done status and clear DMAEN bit
        dma_set_done(dma_ch->id);
        set_bits(&dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value, 0, 0, 0); // clear DMAEN bit
        qemu_log("[dmac] Channel %d: DMA transfer is done\n", dma_ch->id);

        dma_ch->size = dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value;
        dma_ch->src_addr = dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value;
        dma_ch->dst_addr = dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value;
    }
    else
    {
        // request for next transfer
        dma_set_req(dma_ch->id);
    }
}

void dma_block_transfer(DMA_Channel *channel)
{
    DMA_Channel *dma_ch = (DMA_Channel *)channel;
    qemu_log("[dmac] Channel %d: Block transfer mode\n", dma_ch->id);
    uint32_t dma_byte;

    while(dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value != 0)
    {
        if(CTRL_SRCBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
        {
            dma_byte = 1;
            hwaddr src_addr = (hwaddr)dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value ;     // src memory address
            hwaddr dst_addr = (hwaddr)dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value ;     // src memory address
            MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
            char buffer;                                              // buffer 1 bytes
            MemTxResult result;

            // DMA read from source address
            result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
            }
            else
            {
                qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
            }

            // DMA write to destination address
            result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
            }
            else
            {
                qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
            }
        }
        else
        {
            dma_byte = 4;
            hwaddr src_addr = (hwaddr)dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value ;     // src memory address
            hwaddr dst_addr = (hwaddr)dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value ;     // src memory address
            MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
            uint32_t buffer;                                              // buffer 4 bytes
            MemTxResult result;

            // DMA read from source address
            result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
            }
            else
            {
                qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
            }

            // DMA write to destination address
            result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
            }
            else
            {
                qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
            }
        }

        // modify DMA_DST
        if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
        {
            dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value += dma_byte;
        }
        else if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
        {
            dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value -= dma_byte;
        }

        // modify DMA_SRC
        if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
        {
            dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value += dma_byte;
        }
        else if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
        {
            dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value -= dma_byte;
        }

        // Decrease size
        dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value -= dma_byte;
        if(dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value == 0)
        {
            // Set DMA done status and clear DMAEN bit
            dma_set_done(dma_ch->id);
            set_bits(&dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value, 0, 0, 0); // clear DMAEN bit
            qemu_log("[dmac] Channel %d: DMA transfer is done\n", dma_ch->id);
            dma_ch->size = dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value;
            dma_ch->src_addr = dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value;
            dma_ch->dst_addr = dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value;
            break;
        }
    }

}

void dma_repeated_single_transfer(DMA_Channel *channel)
{
    DMA_Channel *dma_ch = (DMA_Channel *)channel;
    qemu_log("[dmac] Channel %d: Repeated single transfer mode\n", dma_ch->id);
    uint32_t dma_byte;

    if(CTRL_SRCBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
    {
        dma_byte = 1;
        hwaddr src_addr = (hwaddr)dma_ch->src_addr;     // src memory address
        hwaddr dst_addr = (hwaddr)dma_ch->dst_addr;     // dst memory address
        MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
        char buffer;                                              // buffer 1 bytes
        MemTxResult result;

        // DMA read from source address
        result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
        }
        else
        {
            qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
        }

        // DMA write to destination address
        result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
        }
        else
        {
            qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
        }
    }
    else
    {
        dma_byte = 4;
        hwaddr src_addr = (hwaddr)dma_ch->src_addr;     // src memory address
        hwaddr dst_addr = (hwaddr)dma_ch->dst_addr;     // src memory address
        MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
        uint32_t buffer;                                              // buffer 4 bytes
        MemTxResult result;

        // DMA read from source address
        result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
        }
        else
        {
            qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
        }
        
        // DMA write to destination address
        result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

        if (result != MEMTX_OK) 
        {
            qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
        }
        else
        {
            qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
        }
    }

    
    // modify DMA_DST
    if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
    {
        dma_ch->dst_addr += dma_byte;
    }
    else if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
    {
        dma_ch->dst_addr -= dma_byte;
    }

    // modify DMA_SRC
    if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
    {
        dma_ch->src_addr += dma_byte;
    }
    else if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
    {
        dma_ch->src_addr -= dma_byte;
    }

    // Decrease size
    dma_ch->size -= dma_byte;
    if(dma_ch->size == 0)
    {
        // Set DMA done status and clear DMAEN bit
        dma_set_done(dma_ch->id);
        qemu_log("[dmac] Channel %d: DMA transfer is done\n", dma_ch->id);
        // Reload size, src_addr, dst_addr
        dma_ch->size = dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value;
        dma_ch->src_addr = dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value;
        dma_ch->dst_addr = dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value;
        // request for next repeated transfer
        dma_set_req(dma_ch->id);
    }
    else
    {
        // request for next transfer
        dma_set_req(dma_ch->id);
    }
}

void dma_repeated_block_transfer(DMA_Channel *channel)
{
    DMA_Channel *dma_ch = (DMA_Channel *)channel;
    qemu_log("[dmac] Channel %d: Repeated block transfer mode\n", dma_ch->id);
    uint32_t dma_byte;

    while(dma_ch->size != 0)
    {
        if(CTRL_SRCBYTE_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value))
        {
            dma_byte = 1;
            hwaddr src_addr = (hwaddr)dma_ch->src_addr;     // src memory address
            hwaddr dst_addr = (hwaddr)dma_ch->dst_addr;     // src memory address
            MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
            char buffer;                                              // buffer 1 bytes
            MemTxResult result;

            // DMA read from source address
            result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
            }
            else
            {
                qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
            }

            // DMA write to destination address
            result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
            }
            else
            {
                qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
            }
        }
        else
        {
            dma_byte = 4;
            hwaddr src_addr = (hwaddr)dma_ch->src_addr;     // src memory address
            hwaddr dst_addr = (hwaddr)dma_ch->dst_addr;     // src memory address
            MemTxAttrs attrs = MEMTXATTRS_UNSPECIFIED;                  // Default attributes
            uint32_t buffer;                                              // buffer 4 bytes
            MemTxResult result;

            // DMA read from source address
            result = address_space_rw(&address_space_memory, src_addr, attrs, &buffer, dma_byte, false);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Read data from 0x%lX failed with result: %d\n",src_addr, result);
            }
            else
            {
                qemu_log("[dmac] Read data from 0x%lX: 0x%X\n",src_addr, buffer);
            }

            // DMA write to destination address
            result = address_space_write(&address_space_memory, dst_addr, attrs, &buffer, dma_byte);

            if (result != MEMTX_OK) 
            {
                qemu_log("[dmac] Write data to 0x%lX failed with result: %d\n",dst_addr, result);
            }
            else
            {
                qemu_log("[dmac] Write data to 0x%lX: 0x%X\n",dst_addr, buffer);
            }
        }

        // modify DMA_DST
        if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
        {
            dma_ch->dst_addr += dma_byte;
        }
        else if(CTRL_DSTINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
        {
            dma_ch->dst_addr -= dma_byte;
        }

        // modify DMA_SRC
        if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x03)
        {
            dma_ch->src_addr += dma_byte;
        }
        else if(CTRL_SRCINC_BIT(dmac_reg_list[eDMA_CTRL0_REG + dma_ch->id]->value) == 0x02)
        {
            dma_ch->src_addr -= dma_byte;
        }

        // Decrease size
        dma_ch->size -= dma_byte;
        if(dma_ch->size == 0)
        {
            // Set DMA done status and clear DMAEN bit
            dma_set_done(dma_ch->id);
            qemu_log("[dmac] Channel %d: DMA transfer is done\n", dma_ch->id);
            // Reload size, src_addr, dst_addr
            dma_ch->size = dmac_reg_list[eDMA_SIZE0_REG + dma_ch->id]->value;
            dma_ch->src_addr = dmac_reg_list[eDMA_SRC0_REG + dma_ch->id]->value;
            dma_ch->dst_addr = dmac_reg_list[eDMA_DST0_REG + dma_ch->id]->value;
            // request for next repeated transfer
            dma_set_req(dma_ch->id);
            break;
        }
    }
}


void dma_set_error(uint32_t id)
{
    switch (id)
    {
        case 0:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 3, 3); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 1, 1); // clear run bit 
                break;
            }
        case 1:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 19, 19); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 17, 17); // clear run bit
                break;
            }
        case 2:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 3, 3); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 1, 1); // clear run bit
                break;
            }
        case 3:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 19, 19); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 17, 17); // clear run bit
                break;
            }
        case 4:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 3, 3); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 1, 1); // clear run bit
                break;
            }
        case 5:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 19, 19); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 17, 17); // clear run bit
                break;
            }
        case 6:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 3, 3); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 1, 1); // clear run bit
                break;
            }
        case 7:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 19, 19); // Set error bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 17, 17); // clear run bit
                break;
            }
    }
    
}

void dma_set_done(uint32_t id)
{
    switch (id)
    {
        case 0:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 0, 0); // set done bit
                break;
            }
        case 1:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 16, 16); // set done bit
                break;
            }
        case 2:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 0, 0); // set done bit
                break;
            }
        case 3:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 16, 16); // set done bit
                break;
            }
        case 4:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 0, 0); // set done bit
                break;
            }
        case 5:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 16, 16); // set done bit
                break;
            }
        case 6:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 0, 0); // set done bit
                break;
            }
        case 7:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 16, 16); // set done bit
                break;
            }
    }

    // edge triggered output signal O_done
    vst_gpio_write(&gdmac->ch[id].O_done, GPIO_LOW);
    usleep(1000);
    vst_gpio_write(&gdmac->ch[id].O_done, GPIO_HIGH);
    usleep(1000);
    vst_gpio_write(&gdmac->ch[id].O_done, GPIO_LOW);
}

void dma_set_run(uint32_t id)
{
    switch (id)
    {
        case 0:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 3, 3); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 0, 0); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 1, 1); // set run bit 
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 2, 2); // clear req bit 
                break;
            }
        case 1:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 19, 19); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 16, 16); // clear done bit 
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 17, 17); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 0, 18, 18); // clear req bit
                break;
            }
        case 2:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 3, 3); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 0, 0); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 1, 1); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 2, 2); // clear req bit
                break;
            }
        case 3:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 19, 19); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 16, 16); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 17, 17); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 0, 18, 18); // clear req bit
                break;
            }
        case 4:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 3, 3); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 0, 0); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 1, 1); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 2, 2); // clear req bit
                break;
            }
        case 5:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 19, 19); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 16, 16); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 17, 17); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 0, 18, 18); // clear req bit
                break;
            }
        case 6:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 3, 3); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 0, 0); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 1, 1); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 2, 2); // clear req bit
                break;
            }
        case 7:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 19, 19); // clear error bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 16, 16); // clear done bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 17, 17); // set run bit
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 0, 18, 18); // clear req bit
                break;
            }
    }
}

void dma_set_req(uint32_t id)
{
    switch (id)
    {
        case 0:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 2, 2); // set req bit 
                break;
            }
        case 1:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS0_REG]->value, 1, 18, 18); // set req bit
                break;
            }
        case 2:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 2, 2); // set req bit
                break;
            }
        case 3:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS1_REG]->value, 1, 18, 18); // set req bit
                break;
            }
        case 4:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 2, 2); // set req bit
                break;
            }
        case 5:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS2_REG]->value, 1, 18, 18); // set req bit
                break;
            }
        case 6:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 2, 2); // set req bit
                break;
            }
        case 7:
            {
                set_bits(&dmac_reg_list[eDMA_STATUS3_REG]->value, 1, 18, 18); // set req bit
                break;
            }
    }

    // edge triggered output signal O_req
    vst_gpio_write(&gdmac->ch[id].O_req, GPIO_LOW);
    usleep(1000);
    vst_gpio_write(&gdmac->ch[id].O_req, GPIO_HIGH);
    usleep(1000);
    vst_gpio_write(&gdmac->ch[id].O_req, GPIO_LOW);
}

/* DMA Triggered operation */
void dmac_trigger_channel(DMACdevice *dmac, int channel_id);
void dmac_trigger_channel(DMACdevice *dmac, int channel_id) 
{
    if (channel_id < 0 || channel_id >= 8) 
    {
        qemu_log("[dmac] Invalid channel ID: %d\n", channel_id);
        return;
    }
    usleep(5000);   // Sleep 5ms to wait for the next trigger signal
    DMA_Channel *channel = &dmac->ch_op[channel_id];

    dma_set_run(channel->id);   // Set DMA run status
    dma_transfer(channel);   // DMA transfering operation

}

/*Callback register*/
void cb_dmac_ctrl0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl7_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_src7_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_dst7_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_size7_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_trigger7_reg(void *opaque, Register32 *reg, uint32_t value);

void cb_dmac_ctrl0_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(0);
        qemu_log("[dmac] DMA channel 0 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 0 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[0].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 0 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[0].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 0: Invalid mode\n");
        dma_set_error(0);
    }
    
}

void cb_dmac_ctrl1_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(1);
        qemu_log("[dmac] DMA channel 1 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 1 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[1].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 1 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[1].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 1: Invalid mode\n");
        dma_set_error(1);
    }
    
}

void cb_dmac_ctrl2_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(2);
        qemu_log("[dmac] DMA channel 2 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 2 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[2].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 2 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[2].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 2: Invalid mode\n");
        dma_set_error(2);
    }
}

void cb_dmac_ctrl3_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(3);
        qemu_log("[dmac] DMA channel 3 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 3 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[3].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 3 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[3].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 3: Invalid mode\n");
        dma_set_error(3);
    }
}

void cb_dmac_ctrl4_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(4);
        qemu_log("[dmac] DMA channel 4 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 4 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[4].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 4 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[4].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 4: Invalid mode\n");
        dma_set_error(4);
    }
}

void cb_dmac_ctrl5_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(5);
        qemu_log("[dmac] DMA channel 5 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 5 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[5].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 5 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[5].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 5: Invalid mode\n");
        dma_set_error(5);
    }
}

void cb_dmac_ctrl6_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(6);
        qemu_log("[dmac] DMA channel 6 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 6 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[6].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 6 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[6].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 6: Invalid mode\n");
        dma_set_error(6);
    }
}

void cb_dmac_ctrl7_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);

    if(CTRL_DMAEN_BIT(value))
    {
        dma_set_req(7);
        qemu_log("[dmac] DMA channel 7 is enabled\n");
    }

    if(CTRL_DMALEVEL_BIT(value))
    {
        qemu_log("[dmac] DMA channel 7 is level triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[7].I_trigger[i].trigger = LEVEL_SENSITIVE;
        }
    }
    else
    {
        qemu_log("[dmac] DMA channel 7 is edge triggered\n");
        for(uint32_t i = 0; i < 32; i ++)
        {
            dmac->ch[7].I_trigger[i].trigger = POSEDGE_SENSITIVE;
        }
    }

    if(CTRL_MODE_BIT(value) > 0x07)
    {
        qemu_log("[dmac] (Error) DMA channel 7: Invalid mode\n");
        dma_set_error(7);
    }
}

void cb_dma_src0_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[0].src_addr = value;
}

void cb_dma_src1_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[1].src_addr = value;
}

void cb_dma_src2_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[2].src_addr = value;
}

void cb_dma_src3_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[3].src_addr = value;
}

void cb_dma_src4_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[4].src_addr = value;
}

void cb_dma_src5_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[5].src_addr = value;
}

void cb_dma_src6_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[6].src_addr = value;
}

void cb_dma_src7_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[7].src_addr = value;
}

void cb_dma_dst0_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[0].dst_addr = value;
}

void cb_dma_dst1_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[1].dst_addr = value;
}

void cb_dma_dst2_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[2].dst_addr = value;
}

void cb_dma_dst3_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[3].dst_addr = value;
}

void cb_dma_dst4_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[4].dst_addr = value;
}

void cb_dma_dst5_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[5].dst_addr = value;
}

void cb_dma_dst6_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[6].dst_addr = value;
}

void cb_dma_dst7_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[7].dst_addr = value;
}

void cb_dma_size0_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[0].size = value;
}

void cb_dma_size1_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[1].size = value;
}

void cb_dma_size2_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[2].size = value;
}

void cb_dma_size3_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[3].size = value;
}

void cb_dma_size4_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[4].size = value;
}

void cb_dma_size5_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[5].size = value;
}

void cb_dma_size6_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[6].size = value;
}

void cb_dma_size7_reg(void *opaque, Register32 *reg, uint32_t value)
{
    DMACdevice *dmac = DMAC(opaque);
    dmac->ch_op[7].size = value;
}

void cb_dma_trigger0_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH0SEL_BIT(dmac_reg_list[eDMA_CFG0_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL0_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER0_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER0_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL0_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER0_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL0_REG]->value))
    {
        qemu_log("[dmac] Channel 0 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 0);
    }
}

void cb_dma_trigger1_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH1SEL_BIT(dmac_reg_list[eDMA_CFG0_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL1_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER1_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER1_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL1_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER1_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL1_REG]->value))
    {
        qemu_log("[dmac] Channel 1 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 1);
    }
}

void cb_dma_trigger2_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH2SEL_BIT(dmac_reg_list[eDMA_CFG1_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL2_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER2_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER2_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL2_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER2_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL2_REG]->value))
    {
        qemu_log("[dmac] Channel 2 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 2);
    }
}

void cb_dma_trigger3_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH3SEL_BIT(dmac_reg_list[eDMA_CFG1_REG]->value);
    
    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL3_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER3_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER3_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL3_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER3_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL3_REG]->value))
    {
        qemu_log("[dmac] Channel 3 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 3);
    }
}

void cb_dma_trigger4_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH4SEL_BIT(dmac_reg_list[eDMA_CFG2_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL4_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER4_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER4_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL4_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER4_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL4_REG]->value))
    {
        qemu_log("[dmac] Channel 4 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 4);
    }
}

void cb_dma_trigger5_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH5SEL_BIT(dmac_reg_list[eDMA_CFG2_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL5_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER5_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER5_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL5_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER5_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL5_REG]->value))
    {
        qemu_log("[dmac] Channel 5 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 5);
    }
}

void cb_dma_trigger6_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH6SEL_BIT(dmac_reg_list[eDMA_CFG3_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL6_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER6_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER6_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL6_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER6_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL6_REG]->value))
    {
        qemu_log("[dmac] Channel 6 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 6);
    }
}

void cb_dma_trigger7_reg(void *opaque, Register32 *reg, uint32_t value)
{
    uint32_t index = CFG_CH7SEL_BIT(dmac_reg_list[eDMA_CFG3_REG]->value);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL7_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER7_REG]->value) == TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER7_REG]->pre_value))
    {   
        // In case of the trigger bit is not clear before re-triggering
        return;
    }

    if(CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL7_REG]->value) && TRIGGER_BIT(index, dmac_reg_list[eDMA_TRIGGER7_REG]->value) && CTRL_DMALEVEL_BIT(dmac_reg_list[eDMA_CTRL7_REG]->value))
    {
        qemu_log("[dmac] Channel 7 is triggered\n");
        DMACdevice *_dmac = (DMACdevice *)opaque;
        dmac_trigger_channel(_dmac, 7);
    }
}


/*Callback port or pin*/
void cb_ch0_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch1_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch2_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch3_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch4_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch5_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch6_trigger_input(vst_gpio_state state, void *context, void *parent);
void cb_ch7_trigger_input(vst_gpio_state state, void *context, void *parent);


void cb_ch0_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL0_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL0_REG]->value))
    {
        uint32_t index = CFG_CH0SEL_BIT(dmac_reg_list[eDMA_CFG0_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch0_trigger%d", index);

        qemu_log("[dmac] Channel 0 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH) 
        {
            qemu_log("[dmac] Channel 0 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 0);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 0 is in test mode\n");
    }
}

void cb_ch1_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL1_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL1_REG]->value))
    {
        uint32_t index = CFG_CH1SEL_BIT(dmac_reg_list[eDMA_CFG0_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch1_trigger%d", index);

        qemu_log("[dmac] Channel 1 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 1 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 1);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 1 is in test mode\n");
    }
}

void cb_ch2_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL2_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL2_REG]->value))
    {
        uint32_t index = CFG_CH2SEL_BIT(dmac_reg_list[eDMA_CFG1_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch2_trigger%d", index);

        qemu_log("[dmac] Channel 2 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 2 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 2);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 2 is in test mode\n");
    }
}

void cb_ch3_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL3_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL3_REG]->value))
    {
        uint32_t index = CFG_CH3SEL_BIT(dmac_reg_list[eDMA_CFG1_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch3_trigger%d", index);

        qemu_log("[dmac] Channel 3 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 3 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 3);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 3 is in test mode\n");
    }
}

void cb_ch4_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL4_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL4_REG]->value))
    {
        uint32_t index = CFG_CH4SEL_BIT(dmac_reg_list[eDMA_CFG2_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch4_trigger%d", index);

        qemu_log("[dmac] Channel 4 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 4 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 4);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 4 is in test mode\n");
    }
}

void cb_ch5_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL5_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL5_REG]->value))
    {
        uint32_t index = CFG_CH5SEL_BIT(dmac_reg_list[eDMA_CFG2_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch5_trigger%d", index);
        
        qemu_log("[dmac] Channel 5 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 5 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 5);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 5 is in test mode\n");
    }
}

void cb_ch6_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL6_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL6_REG]->value))
    {
        uint32_t index = CFG_CH6SEL_BIT(dmac_reg_list[eDMA_CFG3_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch6_trigger%d", index);

        qemu_log("[dmac] Channel 6 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 6 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 6);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 6 is in test mode\n");
    }
}

void cb_ch7_trigger_input(vst_gpio_state state, void *context, void *parent)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);

    if(!CTRL_DMAEN_BIT(dmac_reg_list[eDMA_CTRL7_REG]->value))
    {
        // DMAC is disabled
        return;
    }

    if(!CTRL_TEST_BIT(dmac_reg_list[eDMA_CTRL7_REG]->value))
    {
        uint32_t index = CFG_CH7SEL_BIT(dmac_reg_list[eDMA_CFG3_REG]->value);
        char name[20];
        snprintf(name, sizeof(name), "I_ch7_trigger%d", index);

        qemu_log("[dmac] Channel 7 is mapped to trigger input name: %s\n", name);

        if(pin->name == name && state == GPIO_HIGH)
        {
            qemu_log("[dmac] Channel 7 is triggered\n");
            DMACdevice *_dmac = (DMACdevice *)parent;
            dmac_trigger_channel(_dmac, 7);
        }
    }
    else
    {
        qemu_log("[dmac] Channel 7 is in test mode\n");
    }
}

/*Initialization functions are defined in here*/
void dmac_register_init(void);
void dmac_gpio_init(DMACdevice *dmac);

void dmac_register_init(void)
{
    dmac_reg_list[eDMA_CTRL0_REG] = create_register32("DMA_CTRL0_REG", DMA_CTRL0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl0_reg);
    dmac_reg_list[eDMA_CTRL1_REG] = create_register32("DMA_CTRL1_REG", DMA_CTRL1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl1_reg);
    dmac_reg_list[eDMA_CTRL2_REG] = create_register32("DMA_CTRL2_REG", DMA_CTRL2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl2_reg);
    dmac_reg_list[eDMA_CTRL3_REG] = create_register32("DMA_CTRL3_REG", DMA_CTRL3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl3_reg);
    dmac_reg_list[eDMA_CTRL4_REG] = create_register32("DMA_CTRL4_REG", DMA_CTRL4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl4_reg);
    dmac_reg_list[eDMA_CTRL5_REG] = create_register32("DMA_CTRL5_REG", DMA_CTRL5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl5_reg);
    dmac_reg_list[eDMA_CTRL6_REG] = create_register32("DMA_CTRL6_REG", DMA_CTRL6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl6_reg);
    dmac_reg_list[eDMA_CTRL7_REG] = create_register32("DMA_CTRL7_REG", DMA_CTRL7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl7_reg);
    dmac_reg_list[eDMA_CFG0_REG] = create_register32("DMA_CFG0_REG", DMA_CFG0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_CFG1_REG] = create_register32("DMA_CFG1_REG", DMA_CFG1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_CFG2_REG] = create_register32("DMA_CFG2_REG", DMA_CFG2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_CFG3_REG] = create_register32("DMA_CFG3_REG", DMA_CFG3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC0_REG] = create_register32("DMA_SRC0_REG", DMA_SRC0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src0_reg);
    dmac_reg_list[eDMA_SRC1_REG] = create_register32("DMA_SRC1_REG", DMA_SRC1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src1_reg);
    dmac_reg_list[eDMA_SRC2_REG] = create_register32("DMA_SRC2_REG", DMA_SRC2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src2_reg);
    dmac_reg_list[eDMA_SRC3_REG] = create_register32("DMA_SRC3_REG", DMA_SRC3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src3_reg);
    dmac_reg_list[eDMA_SRC4_REG] = create_register32("DMA_SRC4_REG", DMA_SRC4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src4_reg);
    dmac_reg_list[eDMA_SRC5_REG] = create_register32("DMA_SRC5_REG", DMA_SRC5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src5_reg);
    dmac_reg_list[eDMA_SRC6_REG] = create_register32("DMA_SRC6_REG", DMA_SRC6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src6_reg);
    dmac_reg_list[eDMA_SRC7_REG] = create_register32("DMA_SRC7_REG", DMA_SRC7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_src7_reg);
    dmac_reg_list[eDMA_DST0_REG] = create_register32("DMA_DST0_REG", DMA_DST0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst0_reg);
    dmac_reg_list[eDMA_DST1_REG] = create_register32("DMA_DST1_REG", DMA_DST1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst1_reg);
    dmac_reg_list[eDMA_DST2_REG] = create_register32("DMA_DST2_REG", DMA_DST2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst2_reg);
    dmac_reg_list[eDMA_DST3_REG] = create_register32("DMA_DST3_REG", DMA_DST3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst3_reg);
    dmac_reg_list[eDMA_DST4_REG] = create_register32("DMA_DST4_REG", DMA_DST4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst4_reg);
    dmac_reg_list[eDMA_DST5_REG] = create_register32("DMA_DST5_REG", DMA_DST5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst5_reg);
    dmac_reg_list[eDMA_DST6_REG] = create_register32("DMA_DST6_REG", DMA_DST6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst6_reg);
    dmac_reg_list[eDMA_DST7_REG] = create_register32("DMA_DST7_REG", DMA_DST7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_dst7_reg);
    dmac_reg_list[eDMA_SIZE0_REG] = create_register32("DMA_SIZE0_REG", DMA_SIZE0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size0_reg);
    dmac_reg_list[eDMA_SIZE1_REG] = create_register32("DMA_SIZE1_REG", DMA_SIZE1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size1_reg);
    dmac_reg_list[eDMA_SIZE2_REG] = create_register32("DMA_SIZE2_REG", DMA_SIZE2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size2_reg);
    dmac_reg_list[eDMA_SIZE3_REG] = create_register32("DMA_SIZE3_REG", DMA_SIZE3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size3_reg);
    dmac_reg_list[eDMA_SIZE4_REG] = create_register32("DMA_SIZE4_REG", DMA_SIZE4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size4_reg);
    dmac_reg_list[eDMA_SIZE5_REG] = create_register32("DMA_SIZE5_REG", DMA_SIZE5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size5_reg);
    dmac_reg_list[eDMA_SIZE6_REG] = create_register32("DMA_SIZE6_REG", DMA_SIZE6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size6_reg);
    dmac_reg_list[eDMA_SIZE7_REG] = create_register32("DMA_SIZE7_REG", DMA_SIZE7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_size7_reg);
    dmac_reg_list[eDMA_STATUS0_REG] = create_register32("DMA_STATUS0_REG", DMA_STATUS0_REG, REG_READ_ONLY, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_STATUS1_REG] = create_register32("DMA_STATUS1_REG", DMA_STATUS1_REG, REG_READ_ONLY, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_STATUS2_REG] = create_register32("DMA_STATUS2_REG", DMA_STATUS2_REG, REG_READ_ONLY, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_STATUS3_REG] = create_register32("DMA_STATUS3_REG", DMA_STATUS3_REG, REG_READ_ONLY, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_TRIGGER0_REG] = create_register32("DMA_TRIGGER0_REG", DMA_TRIGGER0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger0_reg);
    dmac_reg_list[eDMA_TRIGGER1_REG] = create_register32("DMA_TRIGGER1_REG", DMA_TRIGGER1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger1_reg);
    dmac_reg_list[eDMA_TRIGGER2_REG] = create_register32("DMA_TRIGGER2_REG", DMA_TRIGGER2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger2_reg);
    dmac_reg_list[eDMA_TRIGGER3_REG] = create_register32("DMA_TRIGGER3_REG", DMA_TRIGGER3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger3_reg);
    dmac_reg_list[eDMA_TRIGGER4_REG] = create_register32("DMA_TRIGGER4_REG", DMA_TRIGGER4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger4_reg);
    dmac_reg_list[eDMA_TRIGGER5_REG] = create_register32("DMA_TRIGGER5_REG", DMA_TRIGGER5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger5_reg);
    dmac_reg_list[eDMA_TRIGGER6_REG] = create_register32("DMA_TRIGGER6_REG", DMA_TRIGGER6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger6_reg);
    dmac_reg_list[eDMA_TRIGGER7_REG] = create_register32("DMA_TRIGGER7_REG", DMA_TRIGGER7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_trigger7_reg);    
}

void dmac_gpio_init(DMACdevice *dmac)
{
    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch0_trigger%d", i);
        vst_gpio_init(&dmac->ch[0].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch0_trigger_input, &dmac->ch[0].I_trigger[i], dmac);
    }
    
    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch1_trigger%d", i);
        vst_gpio_init(&dmac->ch[1].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch1_trigger_input, &dmac->ch[1].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch2_trigger%d", i);
        vst_gpio_init(&dmac->ch[2].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch2_trigger_input, &dmac->ch[2].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch3_trigger%d", i);
        vst_gpio_init(&dmac->ch[3].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch3_trigger_input, &dmac->ch[3].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch4_trigger%d", i);
        vst_gpio_init(&dmac->ch[4].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch4_trigger_input, &dmac->ch[4].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch5_trigger%d", i);
        vst_gpio_init(&dmac->ch[5].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch5_trigger_input, &dmac->ch[5].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch6_trigger%d", i);
        vst_gpio_init(&dmac->ch[6].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch6_trigger_input, &dmac->ch[6].I_trigger[i], dmac);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch7_trigger%d", i);
        vst_gpio_init(&dmac->ch[7].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch7_trigger_input, &dmac->ch[7].I_trigger[i], dmac);
    }

    vst_gpio_init(&dmac->ch[0].O_done, "O_ch0_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[1].O_done, "O_ch1_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[2].O_done, "O_ch2_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[3].O_done, "O_ch3_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[4].O_done, "O_ch4_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[5].O_done, "O_ch5_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[6].O_done, "O_ch6_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[7].O_done, "O_ch7_done", GPIO_MODE_OUTPUT, NULL, NULL, NULL);

    vst_gpio_init(&dmac->ch[0].O_req, "O_ch0_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[1].O_req, "O_ch1_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[2].O_req, "O_ch2_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[3].O_req, "O_ch3_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[4].O_req, "O_ch4_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[5].O_req, "O_ch5_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[6].O_req, "O_ch6_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
    vst_gpio_init(&dmac->ch[7].O_req, "O_ch7_req", GPIO_MODE_OUTPUT, NULL, NULL, NULL);
}

/*This is template code for registration new device in qemu*/

static uint64_t dmac_read(void *opaque, hwaddr addr, unsigned size)
{
    uint32_t value = 0;
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == dmac_reg_list[i]->base_addr) 
        {
            value = read_register32(opaque, dmac_reg_list[i]);
            break;
        }
    }
    return value;
}

static void dmac_write(void *opaque, hwaddr addr,
                            uint64_t value, unsigned size)
{
    for (int i = 0; i < MAX_REG; i++) 
    {
        if ((uint32_t)addr == dmac_reg_list[i]->base_addr) 
        {
            write_register32(opaque, dmac_reg_list[i], value);
        }
    }
}

static const VMStateDescription vmstate_dmac = {
    .name = "dmac",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (const VMStateField[]) {
        VMSTATE_END_OF_LIST()
    }
};

static const MemoryRegionOps dmac_ops = {
    .read = dmac_read,
    .write = dmac_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.max_access_size = 8,
    .impl.max_access_size = 8,
};

static void dmac_realize(DeviceState *dev, Error **errp)
{
    DMACdevice *dmac = DMAC(dev);

    memory_region_init_io(&dmac->io, OBJECT(dev), 
                &dmac_ops, dmac,"dmac", 0x1000);

    sysbus_init_mmio(SYS_BUS_DEVICE(dmac), &dmac->io);
}

static void dmac_unrealize(DeviceState *dev)
{
    // DMACdevice *dmac = DMAC(dev);

    // for(uint i =0 ; i < 8; i++)
    // {
    //     qemu_thread_join(&dmac->ch_op[i].thread);
    // }

    // for(uint i =0 ; i < 8; i++)
    // {
    //     qemu_mutex_destroy(&dmac->ch_op[i].mutex);
    //     qemu_cond_destroy(&dmac->ch_op[i].cond);

    // }
    // qemu_spin_destroy(&spinlock);
}

DMACdevice *dmac_init(MemoryRegion *address_space, hwaddr base)
{
    DMACdevice *dmac = DMAC(qdev_new(TYPE_DMAC));
    MemoryRegion *mr;

   // Realize the device and connect IRQ
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dmac), &error_fatal);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dmac), 0);
    memory_region_add_subregion(address_space, base, mr);

    /*register initialize*/
    dmac_register_init();

    /*gpio initialize*/
    dmac_gpio_init(dmac);

    /*DMA channels initialize*/
    qemu_spin_init(&spinlock);
    for(uint i =0 ; i < 8; i++)
    {
    //     dmac->dma_state[i] = eDMA_STATE_REQ;
    //     dmac->ch_op[i].active = 0x01;
        dmac->ch_op[i].id = i;

    //     qemu_mutex_init(&dmac->ch_op[i].mutex);
    //     qemu_cond_init(&dmac->ch_op[i].cond);

    //     qemu_thread_create(&dmac->ch_op[i].thread, "dma_channel_thread",
    //                        dma_channel_thread, &dmac->ch_op[i], QEMU_THREAD_JOINABLE);

    }

    gdmac = dmac;
    qemu_log("dmac initialized\n");
    return dmac;
}


static void dmac_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = dmac_realize;
    dc->unrealize = dmac_unrealize;
    dc->vmsd = &vmstate_dmac;
}

static const TypeInfo types[] = {
    {
        .name = TYPE_DMAC,
        .parent = TYPE_SYS_BUS_DEVICE,
        .class_init = dmac_class_init,
        .instance_size = sizeof(DMACdevice),
    },
};

DEFINE_TYPES(types)