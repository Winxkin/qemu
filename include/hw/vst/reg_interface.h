#ifndef REG_INTERFACE_H
#define REG_INTERFACE_H
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"

// Forward declaration of Register for the callback function
struct Register32;
struct Register64;

// Callback function type
typedef void (*RegisterCallback32)(void *opaque, struct Register32 *reg, uint32_t value);
typedef void (*RegisterCallback64)(void *opaque, struct Register64 *reg, uint64_t value);

// Enum to define register permissions
typedef enum {
    REG_READ_ONLY = 1,
    REG_WRITE_ONLY = 2,
    REG_READ_WRITE = 3
} RegPermission;

// Structure to represent a register
typedef struct Register32{
    char *name;                // Name of the register
    const uint32_t base_addr;       // Base address of the register
    const uint32_t mask;            // mask of the register
    uint32_t value;            // Value of the register
    uint32_t pre_value;        // Previous value of the register
    RegPermission permissions; // Permissions (read, write, or both)
    RegisterCallback32 callback;
} Register32;

typedef struct Register64{
    char *name;                // Name of the register
    const uint32_t base_addr;       // Base address of the register
    const uint64_t mask;            // mask of the register
    uint64_t value;            // Value of the register
    uint64_t pre_value;        // Previous value of the register
    RegPermission permissions; // Permissions (read, write, or both)
    RegisterCallback64 callback;
} Register64;

// Function to create a new register
Register32* create_register32(const char *name, uint32_t base_addr, RegPermission permissions, uint32_t init,  uint32_t mask,
                                    void (*callback)(void *opaque, Register32 *reg,uint32_t value));

Register64* create_register64(const char *name, uint32_t base_addr, RegPermission permissions, uint64_t init,  uint64_t mask,
                                    void (*callback)(void *opaque, Register64 *reg,uint64_t value));

// Function to read from a register
uint32_t read_register32(void *opaque, Register32 *reg);

uint64_t read_register64(void *opaque, Register64 *reg);

// Function to write to a register
void write_register32(void *opaque, Register32 *reg, uint32_t value);

void write_register64(void *opaque, Register64 *reg, uint64_t value);

void set_bits(uint32_t *reg_val, uint32_t value, uint32_t bit_start, uint32_t bit_end);

#endif