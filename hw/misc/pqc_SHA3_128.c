/*
 * QEMU simulated SHA3-128 device.
 *
 *
 * Authors:
 *     Duy-Huan Nguyen <huannd25@viettel.com.vn>
 *
 * This work is licensed under the terms of the GNU GPL, version 2 or later.
 * See the COPYING file in the top-level directory.
 *
 */
#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "hw/register.h"
#include "hw/qdev-properties.h"
#include "hw/irq.h"
#include "qemu/log.h"


#define TYPE_PQC_SHA3_128 "pqc-sha3-128"

typedef struct PQCSHA3_128_State PQCSHA3_128_State;

DECLARE_INSTANCE_CHECKER(PQCSHA3_128_State, PQCSHA3_128, TYPE_PQC_SHA3_128)

#define SHA3_128_REG_CTRL           0x00  // Control register
#define SHA3_128_REG_STAT           0x04  // Status register
#define SHA3_128_REG_DATA_IN        0x08  // Input data register
#define SHA3_128_REG_DATA_OUT       0x0C  // Output data register
#define SHA3_128_REG_COUNT          4     // Total number of registers

typedef struct PQCSHA3_128_State 
{
    SysBusDevice parent_obj;
    uint32_t regs[SHA3_128_REG_COUNT]; // Registers
    uint8_t input_buffer[200];     // Keccak input buffer (max size for SHA-3)
    uint8_t output_buffer[64];     // Hash output buffer (max SHA3-512 hash size)
    size_t input_len;              // Current input length
    size_t hash_size;              // Current hash size (e.g., 256, 512 bits)
    qemu_irq irq;                  // IRQ line
    // SHA3_CTX sha3_ctx;             // SHA-3 context
} PQCSHA3_128_State;

static void sha3_128_device_write(void *opaque, hwaddr offset, uint64_t value, unsigned size) 
{
    PQCSHA3_128_State *s = (PQCSHA3_128_State*)opaque;

    switch (offset) {
    case SHA3_128_REG_CTRL:
        if (value == 0x1) {  // Start hashing
            qemu_log("SHA-3: Starting hash computation\n");
            // sha3_init(&s->sha3_ctx, s->hash_size);
            // sha3_update(&s->sha3_ctx, s->input_buffer, s->input_len);
            // sha3_final(&s->sha3_ctx, s->output_buffer);
            s->regs[SHA3_128_REG_STAT / 4] = 0x1;  // Mark as ready
            qemu_irq_pulse(s->irq);  // Trigger interrupt
        }
        break;

    case SHA3_128_REG_DATA_IN:
        qemu_log("SHA-3: Writing input data: 0x%08" PRIx64 "\n", value);
        if (s->input_len < sizeof(s->input_buffer)) {
            memcpy(s->input_buffer + s->input_len, &value, size);
            s->input_len += size;
        }
        break;

    case SHA3_128_REG_STAT:
    case SHA3_128_REG_DATA_OUT:
        qemu_log_mask(LOG_GUEST_ERROR, "SHA-3: Invalid write to read-only register\n");
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "SHA-3: Invalid write offset: 0x%" HWADDR_PRIx "\n", offset);
        break;
    }
}


static uint64_t sha3_128_device_read(void *opaque, hwaddr offset, unsigned size) 
{
    PQCSHA3_128_State *s = (PQCSHA3_128_State *)opaque;
    uint64_t value = 0;

    switch (offset) {
    case SHA3_128_REG_STAT:
        value = s->regs[SHA3_128_REG_STAT / 4];
        qemu_log("SHA-3: Reading status: 0x%08x\n", (uint32_t)value);
        break;

    case SHA3_128_REG_DATA_OUT:
        if (s->regs[SHA3_128_REG_STAT / 4] == 0x1) {  // Ready
            memcpy(&value, s->output_buffer, size);
            qemu_log("SHA-3: Reading output data: 0x%08" PRIx64 "\n", value);
        }
        break;

    default:
        qemu_log_mask(LOG_GUEST_ERROR, "SHA-3: Invalid read offset: 0x%" HWADDR_PRIx "\n", offset);
        break;
    }

    return value;
}

static const MemoryRegionOps sha3_128_device_ops = 
{
    .read = sha3_128_device_read,
    .write = sha3_128_device_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 4,
    },
};

static void sha3_128_device_init(Object *obj) 
{
    PQCSHA3_128_State *s = PQCSHA3_128(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    // Initialize memory region
    memory_region_init_io(s->parent_obj.mmio[0].memory, obj, &sha3_128_device_ops, s,"sha3_128_device_mmio", SHA3_128_REG_COUNT * sizeof(uint32_t));

    sysbus_init_mmio(sbd, s->parent_obj.mmio[0].memory);

    // Initialize IRQ
    sysbus_init_irq(sbd, &s->irq);

    // // Set default values
    // s->input_len = 0;
    // s->hash_size = 256;  // Default SHA-3 variant
    // memset(s->regs, 0, sizeof(s->regs));
    // // sha3_init(&s->sha3_ctx, s->hash_size);

    qemu_log("SHA-3 device initialized\n");
}


static void sha3_128_device_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "SHA3 128-bit device";

}


static const TypeInfo sha3_128_device_info = 
{
    .name          = TYPE_PQC_SHA3_128,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(PQCSHA3_128_State),
    .instance_init = sha3_128_device_init,
    .class_init = sha3_128_device_class_init,
};

static void sha3_128_device_register_types(void)
{
    type_register_static(&sha3_128_device_info);
}

type_init(sha3_128_device_register_types);
