#ifndef REG_INTERFACE_H
#define REG_INTERFACE_H
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"

// Forward declaration of Register for the callback function
struct Register;

// Callback function type
typedef void (*RegisterCallback)(struct Register *reg, uint32_t value);

// Enum to define register permissions
typedef enum {
    REG_READ_ONLY = 1,
    REG_WRITE_ONLY = 2,
    REG_READ_WRITE = REG_READ_ONLY | REG_WRITE_ONLY
} RegPermission;

// Structure to represent a register
typedef struct Register{
    char *name;                // Name of the register
    const uint32_t base_addr;       // Base address of the register
    const uint32_t mask;            // mask of the register
    uint32_t value;            // Value of the register
    uint32_t pre_value;        // Previous value of the register
    RegPermission permissions; // Permissions (read, write, or both)
    RegisterCallback callback;
} Register;

// Function to create a new register
Register* create_register(const char *name, uint32_t base_addr, RegPermission permissions, uint32_t init,  uint32_t mask,
                                    void (*callback)(Register *reg,uint32_t value));

// Function to read from a register
uint32_t read_register(Register *reg);

// Function to write to a register
void write_register(Register *reg, uint32_t value);

#endif