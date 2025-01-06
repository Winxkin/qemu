#include "hw/vst/vst_gpio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIN_MASK(num_pins) ((1UL << (num_pins)) - 1)


typedef struct vst_port_binding_node {
    vst_port_binding_t binding;           // Binding data
    struct vst_port_binding_node *next;   // Pointer to the next node
} vst_port_binding_node_t;

typedef struct vst_gpio_binding_node {
    vst_gpio_binding_t binding;           // Binding data
    struct vst_gpio_binding_node *next;   // Pointer to the next node
} vst_gpio_binding_node_t;

vst_port_binding_node_t *vst_port_bindings_head = NULL;
vst_gpio_binding_node_t *vst_gpio_bindings_head = NULL;



void vst_port_init(vst_gpio_port *port, const char *name, uint32_t num_pins, vst_gpio_mode mode, vst_port_callback_t callback, void *context, void *parent)
{
    port->name = name;

    if(num_pins > 32)
    {
        port->num_pins = 32;
        qemu_log("Warning: PORT '%s' has more than 32 pins, only the first 32 pins will be used\n", port->name);
    }
    else
    {
        port->num_pins = num_pins;
    }
    port->value = 0x00;
    port->mode = mode;
    port->mask = PIN_MASK(num_pins);

    if(mode == GPIO_MODE_INPUT)
    {
        port->callback = callback;
        port->callback_context = context;
        port->parent = parent;
    }
    else
    {
        port->callback = NULL;
        port->callback_context = NULL;
        port->parent = NULL;
    }
}

void vst_port_write(vst_gpio_port *port, uint32_t value)
{
    if(port->mode == GPIO_MODE_OUTPUT)
    {
        if(port->value == (value & port->mask))
        {
            return;
        }

        port->value = value & port->mask;
        
        vst_port_binding_node_t *current = vst_port_bindings_head;
        while (current->next != NULL) 
        {
            if (current->binding.output_port == port && strcmp(current->binding.output_port->name, port->name) == 0) 
            {
                current->binding.input_port->value = port->value;
                if(current->binding.input_port->callback)
                {
                    current->binding.input_port->callback(port->value, current->binding.input_port->callback_context, current->binding.input_port->parent);
                }
            }
            current = current->next; // Traverse to the end of the list
        }

        // the lastest node
        if (current->binding.output_port == port && strcmp(current->binding.output_port->name, port->name) == 0) 
        {
            current->binding.input_port->value = port->value;
            if(current->binding.input_port->callback)
            {
                current->binding.input_port->callback(port->value, current->binding.input_port->callback_context, current->binding.input_port->parent);
            }
        }
    }
    else
    {
        qemu_log("Port '%s' is not an output mode\n", port->name);
    }
}

uint32_t vst_port_read(vst_gpio_port *port)
{
    return port->value;
}

int vst_port_bind(vst_gpio_port *output_port, vst_gpio_port *input_port)
{

    if(output_port->num_pins != input_port->num_pins)
    {
        qemu_log("Error: PORTs '%s' and '%s' have different number of pins\n", output_port->name, input_port->name);
        return -1;
    }

    if(output_port->mode == input_port->mode)
    {
        qemu_log("Error: PORTs '%s' and '%s' have the same mode\n", output_port->name, input_port->name);
        return -1;
    }
    // Check input port is bound to another output port
    vst_port_binding_node_t *current = vst_port_bindings_head;
    while (current != NULL) 
    {
        if (current->binding.input_port == input_port) 
        {
            qemu_log("Error: PORT '%s' is already bound to '%s'\n", input_port->name, current->binding.output_port->name);
            return -1;
        }
        current = current->next;
    }

    vst_port_binding_node_t *new_node = (vst_port_binding_node_t *)malloc(sizeof(vst_port_binding_node_t));

    if (new_node == NULL) {
        qemu_log("Failed to allocate memory for a new PORT binding\n");
        return -1; // Memory allocation failed
    }

    // Populate the binding data
    new_node->binding.output_port = output_port;
    new_node->binding.input_port = input_port;
    new_node->next = NULL;

    // Add the new node to the end of the list
    if (vst_port_bindings_head == NULL) 
    {
        vst_port_bindings_head = new_node; // First node in the list
    } 
    else 
    {
        current = vst_port_bindings_head;
        while (current->next != NULL) 
        {
            current = current->next; // Traverse to the end of the list
        }
        current->next = new_node; // Append the new node
    }
    return 0; // Success
}


// Initialize a GPIO group with a set of pins
void vst_gpio_init(vst_gpio_pin *pin, const char *pin_name, vst_gpio_mode pin_mode, vst_gpio_callback_t callback, void *context, void *parent) 
{
    pin->name = pin_name;
    pin->mode = pin_mode;
    pin->state = GPIO_LOW;
    pin->trigger = LEVEL_SENSITIVE;
    if (pin_mode == GPIO_MODE_INPUT) 
    {
        pin->callback = callback;
        pin->callback_context = context;
        pin->parent = parent;
    }
    else    
    {
        pin->callback = NULL;
        pin->callback_context = NULL;
        pin->parent = NULL;
    }
}

// Write a state (HIGH or LOW) to a GPIO pin
void vst_gpio_write(vst_gpio_pin *pin, vst_gpio_state state) 
{

    if (pin->mode == GPIO_MODE_OUTPUT) 
    {
        if(pin->state == state)
        {
            return;
        }

        pin->state = state;
        // Update all bound input GPIOs if an output changes state
        vst_gpio_binding_node_t *current = vst_gpio_bindings_head;

        while (current->next != NULL) 
        {
            if (current->binding.output_pin == pin && strcmp(current->binding.output_pin->name, pin->name) == 0) 
            {
                current->binding.input_pin->state = pin->state;
                if(current->binding.input_pin->callback)
                {
                    if(current->binding.input_pin->trigger == POSEDGE_SENSITIVE && pin->state == GPIO_HIGH)
                    {
                        current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                    }
                    else if(current->binding.input_pin->trigger == NEGEDGE_SENSITIVE && pin->state == GPIO_LOW)
                    {
                        current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                    }
                    else if(current->binding.input_pin->trigger == LEVEL_SENSITIVE)
                    {
                        current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                    }
                }
            }
            current = current->next; // Traverse to the end of the list
        }
        // the lastest node
        if (current->binding.output_pin == pin && strcmp(current->binding.output_pin->name, pin->name) == 0) 
        {
            current->binding.input_pin->state = pin->state;
            if(current->binding.input_pin->callback)
            {
                if(current->binding.input_pin->trigger == POSEDGE_SENSITIVE && pin->state == GPIO_HIGH)
                {
                    current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                }
                else if(current->binding.input_pin->trigger == NEGEDGE_SENSITIVE && pin->state == GPIO_LOW)
                {
                    current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                }
                else if(current->binding.input_pin->trigger == LEVEL_SENSITIVE)
                {
                    current->binding.input_pin->callback(state, current->binding.input_pin->callback_context, current->binding.input_pin->parent);
                }
            }
        }
    } 
    else 
    {
        qemu_log("GPIO pin '%s' is not an output pin\n", pin->name);
    }
}

// Read the state of a GPIO pin (HIGH/LOW)
vst_gpio_state vst_gpio_read(vst_gpio_pin *pin) 
{
    return pin->state;
}


// Bind an output GPIO from one group to an input GPIO of another group
int vst_gpio_bind(vst_gpio_pin *output_pin, vst_gpio_pin *input_pin) 
{
    
    if(output_pin->mode == input_pin->mode)
    {
        qemu_log("Error: pin '%s' and '%s' have the same mode\n", output_pin->name, input_pin->name);
        return -1;
    }

    // Check input pin is bound to another output pin
    vst_gpio_binding_node_t *current = vst_gpio_bindings_head;
    while (current != NULL) 
    {
        if (current->binding.input_pin == input_pin) 
        {
            qemu_log("Error: pin '%s' is already bound to '%s'\n", input_pin->name, current->binding.output_pin->name);
            return -1;
        }
        current = current->next;
    }

    vst_gpio_binding_node_t *new_node = (vst_gpio_binding_node_t *)malloc(sizeof(vst_gpio_binding_node_t));

    if (new_node == NULL) 
    {
        qemu_log("Failed to allocate memory for a new pin binding\n");
        return -1; // Memory allocation failed
    }

    // Populate the binding data
    new_node->binding.output_pin = output_pin;
    new_node->binding.input_pin = input_pin;
    new_node->next = NULL;

    // Add the new node to the end of the list
    if (vst_gpio_bindings_head == NULL) 
    {
        vst_gpio_bindings_head = new_node; // First node in the list
    } 
    else 
    {
        current = vst_gpio_bindings_head;
        while (current->next != NULL) 
        {
            current = current->next; // Traverse to the end of the list
        }
        current->next = new_node; // Append the new node
    }

    return 0; // Success
}

// Free all ports | pins bindings
void vst_free_bindings(void) 
{
    vst_port_binding_node_t *pcurrent = vst_port_bindings_head;
    while (pcurrent != NULL) 
    {
        vst_port_binding_node_t *ptemp = pcurrent;
        pcurrent = pcurrent->next;
        // Free the node itself
        free(ptemp);
    }

    vst_gpio_binding_node_t *gcurrent = vst_gpio_bindings_head;
    while (gcurrent != NULL) 
    {
        vst_gpio_binding_node_t *gtemp = gcurrent;
        gcurrent = gcurrent->next;
        // Free the node itself
        free(gtemp);
    }

    vst_port_bindings_head = NULL;
    vst_gpio_bindings_head = NULL;
}