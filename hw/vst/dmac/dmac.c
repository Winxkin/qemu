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
#define DMAC_CTRL0_REG    0x00
#define DMAC_CTRL1_REG    0x04
#define DMAC_CTRL2_REG    0x08
#define DMAC_CTRL3_REG    0x0C
#define DMAC_CTRL4_REG    0x10
#define DMAC_CTRL5_REG    0x14
#define DMAC_CTRL6_REG    0x18
#define DMAC_CTRL7_REG    0x1C

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
    eDMAC_CTRL0_REG = 0,
    eDMAC_CTRL1_REG,
    eDMAC_CTRL2_REG,
    eDMAC_CTRL3_REG,
    eDMAC_CTRL4_REG,
    eDMAC_CTRL5_REG,
    eDMAC_CTRL6_REG,
    eDMAC_CTRL7_REG,
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
        bit 1: DMAREQ : default 0x00    (when the DMAREQ is set, DMAC is ready to start operation)
            - 0x00 : disable
            - 0x01 : enable
        bit 2: DMALEVEL : default 0x00  (DMALEVEL is used to select the level sensitive or edge sensitive)
            - 0x00 : edge sensitive
            - 0x01 : level sensitive
        bit 3: reserved
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
#define CTRL_DMAREQ_BIT(val)    ((val >> 1) & 0x1)
#define CTRL_DMALEVEL_BIT(val)  ((val >> 2) & 0x1)
#define CTRL_MODE_BIT(val)      ((val >> 4) & 0x7)
#define CTRL_DSTINC_BIT(val)    ((val >> 8) & 0x3)
#define CTRL_SRCINC_BIT(val)    ((val >> 10) & 0x3)
#define CTRL_DSTBYTE_BIT(val)   ((val >> 12) & 0x1)
#define CTRL_SRCBYTE_BIT(val)   ((val >> 13) & 0x1)
#define CTRL_TEST_BIT(val)      ((val >> 31) & 0x1)

/*
---Define bits for DMAC_CTRL0_REG--- (y index from 0 to 4)
        bit 0-15: CH0SEL : default 0x00 (DMA channel 0 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 16-31: CH1SEL : default 0x00 (DMA channel 1 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
*/

#define CTRL_CH0SEL_BIT(val)    ((val >> 0) & 0xFFFF)
#define CTRL_CH1SEL_BIT(val)    ((val >> 16) & 0xFFFF)

/*
---Define bits for DMAC_CTRL1_REG--- (y index from 0 to 4)
        bit 0-15: CH2SEL : default 0x00 (DMA channel 2 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 16-31: CH3SEL : default 0x00 (DMA channel 3 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
*/

#define CTRL_CH2SEL_BIT(val)    ((val >> 0) & 0xFFFF)
#define CTRL_CH3SEL_BIT(val)    ((val >> 16) & 0xFFFF)

/*
---Define bits for DMAC_CTRL2_REG--- (y index from 0 to 4)
        bit 0-15: CH4SEL : default 0x00 (DMA channel 4 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 16-31: CH5SEL : default 0x00 (DMA channel 5 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
*/

#define CTRL_CH4SEL_BIT(val)    ((val >> 0) & 0xFFFF)
#define CTRL_CH5SEL_BIT(val)    ((val >> 16) & 0xFFFF)

/*
---Define bits for DMAC_CTRL3_REG--- (y index from 0 to 4)
        bit 0-15: CH6SEL : default 0x00 (DMA channel 6 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
        bit 16-31: CH7SEL : default 0x00 (DMA channel 7 is selected, including 32 trigger sources)
            - 0x00 : DMA trigger source 0
            - 0x01 : DMA trigger source 1
            - 0x02 : DMA trigger source 2
            ...
            - 0x1F : DMA trigger source 31
*/

#define CTRL_CH6SEL_BIT(val)    ((val >> 0) & 0xFFFF)
#define CTRL_CH7SEL_BIT(val)    ((val >> 16) & 0xFFFF)

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


#define MAX_REG 48
Register32 *dmac_reg_list[MAX_REG];


/*Callback register*/
void cb_dmac_ctrl0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl3_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl4_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl5_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl6_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dmac_ctrl7_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_cfg0_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_cfg1_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_cfg2_reg(void *opaque, Register32 *reg, uint32_t value);
void cb_dma_cfg3_reg(void *opaque, Register32 *reg, uint32_t value);
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

}

void cb_dmac_ctrl1_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl2_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl3_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl4_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl5_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl6_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dmac_ctrl7_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_cfg0_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_cfg1_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_cfg2_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_cfg3_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger0_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger1_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger2_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger3_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger4_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger5_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger6_reg(void *opaque, Register32 *reg, uint32_t value)
{

}

void cb_dma_trigger7_reg(void *opaque, Register32 *reg, uint32_t value)
{

}


/*Callback port or pin*/
void cb_ch0_trigger_input(vst_gpio_state state, void *context);
void cb_ch1_trigger_input(vst_gpio_state state, void *context);
void cb_ch2_trigger_input(vst_gpio_state state, void *context);
void cb_ch3_trigger_input(vst_gpio_state state, void *context);
void cb_ch4_trigger_input(vst_gpio_state state, void *context);
void cb_ch5_trigger_input(vst_gpio_state state, void *context);
void cb_ch6_trigger_input(vst_gpio_state state, void *context);
void cb_ch7_trigger_input(vst_gpio_state state, void *context);


void cb_ch0_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch1_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch2_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch3_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch4_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch5_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch6_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

void cb_ch7_trigger_input(vst_gpio_state state, void *context)
{
    vst_gpio_pin *pin = (vst_gpio_pin *)context;
    qemu_log("[dmac] Callback for %s invoked with state %d\n", pin->name, state);
}

/*Initialization functions are defined in here*/
void dmac_register_init(void);
void dmac_gpio_init(DMACdevice *dmac);

void dmac_register_init(void)
{
    dmac_reg_list[eDMAC_CTRL0_REG] = create_register32("DMAC_CTRL0_REG", DMAC_CTRL0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl0_reg);
    dmac_reg_list[eDMAC_CTRL1_REG] = create_register32("DMAC_CTRL1_REG", DMAC_CTRL1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl1_reg);
    dmac_reg_list[eDMAC_CTRL2_REG] = create_register32("DMAC_CTRL2_REG", DMAC_CTRL2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl2_reg);
    dmac_reg_list[eDMAC_CTRL3_REG] = create_register32("DMAC_CTRL3_REG", DMAC_CTRL3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl3_reg);
    dmac_reg_list[eDMAC_CTRL4_REG] = create_register32("DMAC_CTRL4_REG", DMAC_CTRL4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl4_reg);
    dmac_reg_list[eDMAC_CTRL5_REG] = create_register32("DMAC_CTRL5_REG", DMAC_CTRL5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl5_reg);
    dmac_reg_list[eDMAC_CTRL6_REG] = create_register32("DMAC_CTRL6_REG", DMAC_CTRL6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl6_reg);
    dmac_reg_list[eDMAC_CTRL7_REG] = create_register32("DMAC_CTRL7_REG", DMAC_CTRL7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dmac_ctrl7_reg);
    dmac_reg_list[eDMA_CFG0_REG] = create_register32("DMA_CFG0_REG", DMA_CFG0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_cfg0_reg);
    dmac_reg_list[eDMA_CFG1_REG] = create_register32("DMA_CFG1_REG", DMA_CFG1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_cfg1_reg);
    dmac_reg_list[eDMA_CFG2_REG] = create_register32("DMA_CFG2_REG", DMA_CFG2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_cfg2_reg);
    dmac_reg_list[eDMA_CFG3_REG] = create_register32("DMA_CFG3_REG", DMA_CFG3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, cb_dma_cfg3_reg);
    dmac_reg_list[eDMA_SRC0_REG] = create_register32("DMA_SRC0_REG", DMA_SRC0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC1_REG] = create_register32("DMA_SRC1_REG", DMA_SRC1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC2_REG] = create_register32("DMA_SRC2_REG", DMA_SRC2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC3_REG] = create_register32("DMA_SRC3_REG", DMA_SRC3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC4_REG] = create_register32("DMA_SRC4_REG", DMA_SRC4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC5_REG] = create_register32("DMA_SRC5_REG", DMA_SRC5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC6_REG] = create_register32("DMA_SRC6_REG", DMA_SRC6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SRC7_REG] = create_register32("DMA_SRC7_REG", DMA_SRC7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST0_REG] = create_register32("DMA_DST0_REG", DMA_DST0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST1_REG] = create_register32("DMA_DST1_REG", DMA_DST1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST2_REG] = create_register32("DMA_DST2_REG", DMA_DST2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST3_REG] = create_register32("DMA_DST3_REG", DMA_DST3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST4_REG] = create_register32("DMA_DST4_REG", DMA_DST4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST5_REG] = create_register32("DMA_DST5_REG", DMA_DST5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST6_REG] = create_register32("DMA_DST6_REG", DMA_DST6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_DST7_REG] = create_register32("DMA_DST7_REG", DMA_DST7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE0_REG] = create_register32("DMA_SIZE0_REG", DMA_SIZE0_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE1_REG] = create_register32("DMA_SIZE1_REG", DMA_SIZE1_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE2_REG] = create_register32("DMA_SIZE2_REG", DMA_SIZE2_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE3_REG] = create_register32("DMA_SIZE3_REG", DMA_SIZE3_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE4_REG] = create_register32("DMA_SIZE4_REG", DMA_SIZE4_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE5_REG] = create_register32("DMA_SIZE5_REG", DMA_SIZE5_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE6_REG] = create_register32("DMA_SIZE6_REG", DMA_SIZE6_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
    dmac_reg_list[eDMA_SIZE7_REG] = create_register32("DMA_SIZE7_REG", DMA_SIZE7_REG, REG_READ_WRITE, 0, 0xFFFFFFFF, NULL);
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
        vst_gpio_init(&dmac->ch[0].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch0_trigger_input, &dmac->ch[0].I_trigger[i]);
    }
    
    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch1_trigger%d", i);
        vst_gpio_init(&dmac->ch[1].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch1_trigger_input, &dmac->ch[1].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch2_trigger%d", i);
        vst_gpio_init(&dmac->ch[2].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch2_trigger_input, &dmac->ch[2].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch3_trigger%d", i);
        vst_gpio_init(&dmac->ch[3].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch3_trigger_input, &dmac->ch[3].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch4_trigger%d", i);
        vst_gpio_init(&dmac->ch[4].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch4_trigger_input, &dmac->ch[4].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch5_trigger%d", i);
        vst_gpio_init(&dmac->ch[5].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch5_trigger_input, &dmac->ch[5].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch6_trigger%d", i);
        vst_gpio_init(&dmac->ch[6].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch6_trigger_input, &dmac->ch[6].I_trigger[i]);
    }

    for (uint32_t i = 0; i < 32; i++) 
    {
        char name[20];
        snprintf(name, sizeof(name), "I_ch7_trigger%d", i);
        vst_gpio_init(&dmac->ch[7].I_trigger[i], name, GPIO_MODE_INPUT, cb_ch7_trigger_input, &dmac->ch[7].I_trigger[i]);
    }

    vst_gpio_init(&dmac->ch[0].O_done, "O_ch0_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[1].O_done, "O_ch1_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[2].O_done, "O_ch2_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[3].O_done, "O_ch3_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[4].O_done, "O_ch4_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[5].O_done, "O_ch5_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[6].O_done, "O_ch6_done", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[7].O_done, "O_ch7_done", GPIO_MODE_OUTPUT, NULL, NULL);

    vst_gpio_init(&dmac->ch[0].O_req, "O_ch0_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[1].O_req, "O_ch1_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[2].O_req, "O_ch2_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[3].O_req, "O_ch3_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[4].O_req, "O_ch4_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[5].O_req, "O_ch5_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[6].O_req, "O_ch6_req", GPIO_MODE_OUTPUT, NULL, NULL);
    vst_gpio_init(&dmac->ch[7].O_req, "O_ch7_req", GPIO_MODE_OUTPUT, NULL, NULL);
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

DMACdevice *dmac_init(MemoryRegion *address_space, hwaddr base)
{
    DMACdevice *dmac = DMAC(qdev_new(TYPE_DMAC));
    MemoryRegion *mr;

   // Realize the device and connect IRQ
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dmac), &error_fatal);

    mr = sysbus_mmio_get_region(SYS_BUS_DEVICE(dmac), 0);
    memory_region_add_subregion(address_space, base, mr);

    /*initialize register*/
    dmac_register_init();

    /*initialize gpio*/
    dmac_gpio_init(dmac);

    qemu_log("dmac initialized\n");
    return dmac;
}


static void dmac_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = dmac_realize;
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