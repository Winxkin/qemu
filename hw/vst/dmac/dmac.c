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
#define DMAC_CTRL_REG    0x00
#define DMAC_START_REG   0x04
#define DMA_CFG0_REG     0x08
#define DMA_CFG1_REG     0x0C
#define DMA_SRC_REG      0x10
#define DMA_DST_REG      0x14
#define DMA_SIZE_REG      0x18
#define DMA_STATUS_REG   0x1C

/*
---Define bits for DMAC_CTRL_REG---
        bit 0: DMAEN : default 0x00     (when the DMAEN is clear, DMAC stop operation)
            - 0x00 : disable
            - 0x01 : enable
        bit 1: DMALEVEL : default 0x00  (DMALEVEL is used to select the level sensitive or edge sensitive)
            - 0x00 : edge sensitive
            - 0x01 : level sensitive
        bit 2-3: reserved
        bit 4-6: MODE
            - 0x00 : Single transfer (Each transfer require a trigger. DMAEN is automatically cleared when DMA_SIZE_REG is decremented to 0)
            - 0x01 : Block transfer (A complete blok is transfered with one trigger. DMAEN is automatically cleared at the end of burst-block transfer)
            - 0x02, 0x03: Brust-block transfer (CPU activity is interleaved with a block transfer. DMAEN is automatically cleared at the end of the brust-transfer)
            - 0x04: Repeated single transfer (Each transfer require a trigger. DMAEN remain enabled until it is cleared by software)
            - 0x05: Repeated block transfer (A complete block is transfered with one trigger. DMAEN remain enabled until it is cleared by software)
            - 0x06, 0x07: Repeated brust-block transfer (CPU activity is interleaved with a block transfer. DMAEN remains enabled until it is cleared by software)
*/




#define MAX_REG 7
Register32 *dmac_reg_list[MAX_REG];

void dmac_register_init(void);


void dmac_register_init(void)
{

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