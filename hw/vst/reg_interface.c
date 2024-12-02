#include "hw/vst/reg_interface.h"

// Function to create a new register
Register* create_register(const char *name, uint32_t base_addr, RegPermission permissions, uint32_t init,  uint32_t mask,
                                    void (*callback)(Register *reg,uint32_t value))
{
    Register *reg = (Register*)malloc(sizeof(Register));
    reg->name = strdup(name);   // Duplicate the name to avoid issues with the pointer later
    *(uint32_t *)&reg->base_addr = base_addr; // Use a cast to bypass the `const` restriction
    *(uint32_t *)&reg->mask = mask;           // Use a cast to bypass the `const` restriction
    reg->value = init;             // Initialize the register value to 0
    reg->permissions = permissions;
    reg->callback = callback;
    return reg;
}

// Function to read from a register
uint32_t read_register(Register *reg) {
    if (!(reg->permissions & REG_READ_ONLY)) {
        qemu_log("Error: Register %s is write-only.\n", reg->name);
        return 0;
    }
    qemu_log("Reading from register %s at base address 0x%X : value = 0x%X\n", reg->name, reg->base_addr, reg->value);
    return reg->value;
}

// Function to write to a register
void write_register(Register *reg, uint32_t value) {
    if (!(reg->permissions & REG_WRITE_ONLY) && !(reg->permissions & REG_READ_WRITE)) {
        qemu_log("Error: Register %s is read-only.\n", reg->name);
        return;
    }
    qemu_log("Writing to register %s at base address 0x%X : value = 0x%X\n", reg->name, reg->base_addr, value);
    reg->pre_value = reg->value; // Save the previous value
    reg->value = value & reg->mask; // Mask the value to the register's mask
    if (reg->callback) {
        reg->callback(reg, value);  // Invoke callback on write if defined
    }
}