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



void vst_port_init(vst_gpio_port *port, const char *name, uint32_t num_pins, vst_gpio_mode mode)
{
    port->name = name;

    if(num_pins > 32)
    {
        port->num_pins = 32;
    }
    else
    {
        port->num_pins = num_pins;
    }
    port->value = 0x00;
    port->mode = mode;
    port->mask = PIN_MASK(num_pins);
}

void vst_port_write(vst_gpio_port *port,const char *name, uint32_t value)
{
    if(port->mode == GPIO_MODE_OUTPUT)
    {
        port->value = value & port->mask;
        
        vst_port_binding_node_t *current = vst_port_bindings_head;
        while (current->next != NULL) 
        {
            if (current->binding.output_port == port && strcmp(current->binding.output_name, name) == 0) 
            {
                vst_port_update_input(current->binding.input_port, current->binding.input_name, value);
            }
            current = current->next; // Traverse to the end of the list
        }

        // the lastest node
        if(current->binding.output_port == port && strcmp(current->binding.output_name, name) == 0) 
        {
            vst_port_update_input(current->binding.input_port, current->binding.input_name, value);
        }
    }
    else
    {
        qemu_log("Port '%s' is not an output mode\n", port->name);
    }
}

uint32_t vst_port_read(vst_gpio_port *port, const char *name)
{
    return port->value;
}

int vst_port_bind(vst_gpio_port *output_port, const char *output_name, vst_gpio_port *input_port, const char *input_name)
{

    vst_port_binding_node_t *new_node = (vst_port_binding_node_t *)malloc(sizeof(vst_port_binding_node_t));

    if (new_node == NULL) {
        qemu_log("Failed to allocate memory for a new PORT binding\n");
        return -1; // Memory allocation failed
    }
    // Populate the binding data
    new_node->binding.output_port = output_port;
    new_node->binding.output_name = output_name; // Duplicate string
    new_node->binding.input_port = input_port;
    new_node->binding.input_name = input_name; // Duplicate string
    new_node->next = NULL;

    // Add the new node to the end of the list
    if (vst_port_bindings_head == NULL) 
    {
        vst_port_bindings_head = new_node; // First node in the list
    } 
    else 
    {
        vst_port_binding_node_t *current = vst_port_bindings_head;
        while (current->next != NULL) 
        {
            current = current->next; // Traverse to the end of the list
        }
        current->next = new_node; // Append the new node
    }
    return 0; // Success
}

void vst_port_update_input(vst_gpio_port *port, const char *name, uint32_t value)
{
    if(port->callback)
    {
        port->value = value & port->mask;
        port->callback(port->value, port->callback_context);
    }
}



// Initialize a GPIO group with a set of pins
void vst_gpio_grp_init(vst_gpio_grp *group, const char *grp_name, uint32_t num_pins) 
{
    group->grp_name = grp_name;
    group->num_pins = num_pins;
    group->pins = (vst_gpio_pin *)malloc(sizeof(vst_gpio_pin) * num_pins);

    for (uint32_t i = 0; i < num_pins; i++) {
        group->pins[i].pin = i;
        group->pins[i].mode = GPIO_MODE_INPUT;
        group->pins[i].state = GPIO_LOW;
        group->pins[i].name = NULL;
        group->pins[i].callback = NULL;
        group->pins[i].callback_context = NULL;
    }
}

// Get GPIO pin index by its name
int vst_gpio_get_pin_index(vst_gpio_grp *group, const char *name) 
{
    for (uint32_t i = 0; i < group->num_pins; i++) {
        if (strcmp(group->pins[i].name, name) == 0) 
        {
            return i;
        }
    }
    return -1; // Pin not found
}

// Set GPIO pin mode (input/output)
void vst_gpio_set_mode(vst_gpio_grp *group, const char *name, vst_gpio_mode mode) 
{
    int pin_index = vst_gpio_get_pin_index(group, name);
    if (pin_index >= 0) 
    {
        group->pins[pin_index].mode = mode;
    } 
    else 
    {
        qemu_log("GPIO pin '%s' not found\n", name);
    }
}

// Write a state (HIGH or LOW) to a GPIO pin
void vst_gpio_write(vst_gpio_grp *group, const char *name, vst_gpio_state state) 
{
    int pin_index = vst_gpio_get_pin_index(group, name);

    if (pin_index >= 0 && group->pins[pin_index].mode == GPIO_MODE_OUTPUT) 
    {
        group->pins[pin_index].state = state;
        qemu_log("GPIO '%s' set to %s\n", name, state == GPIO_HIGH ? "HIGH" : "LOW");

        // Update all bound input GPIOs if an output changes state
        vst_gpio_binding_node_t *current = vst_gpio_bindings_head;
        while (current->next != NULL) 
        {
            if (current->binding.output_group == group && strcmp(current->binding.output_name, name) == 0) 
            {
                vst_gpio_update_input(current->binding.input_group, current->binding.input_name, state);
            }
            current = current->next; // Traverse to the end of the list
        }
        // the lastest node
        if (current->binding.output_group == group && strcmp(current->binding.output_name, name) == 0) 
        {
            vst_gpio_update_input(current->binding.input_group, current->binding.input_name, state);
        }
    } 
    else 
    {
        qemu_log("GPIO pin '%s' is not an output pin\n", name);
    }
}

// Read the state of a GPIO pin (HIGH/LOW)
vst_gpio_state vst_gpio_read(vst_gpio_grp *group, const char *name) 
{
    int pin_index = vst_gpio_get_pin_index(group, name);

    if (pin_index >= 0 && group->pins[pin_index].mode == GPIO_MODE_INPUT) 
    {
        return group->pins[pin_index].state;
    }
    qemu_log("GPIO pin '%s' is not an input pin\n", name);
    return GPIO_LOW;
}

// Set the callback function for an input GPIO pin
void vst_gpio_set_callback(vst_gpio_grp *group, const char *name, vst_gpio_callback_t callback, void *context) 
{
    int pin_index = vst_gpio_get_pin_index(group, name);

    if (pin_index >= 0 && group->pins[pin_index].mode == GPIO_MODE_INPUT) 
    {
        group->pins[pin_index].callback = callback;
        group->pins[pin_index].callback_context = context;
    } else {
        qemu_log("GPIO pin '%s' is not an input pin\n", name);
    }
}

void vst_port_set_callback(vst_gpio_port *port, const char *name, vst_port_callback_t callback, void *context)
{
    if(port->mode == GPIO_MODE_INPUT)
    {
        port->callback = callback;
        port->callback_context = context;
    }
    else 
    {
        qemu_log("Port '%s' is not an input \n", name);
    }
}

// Update the state of an input GPIO when its bound output GPIO is triggered
void vst_gpio_update_input(vst_gpio_grp *group, const char *name, vst_gpio_state state) 
{
    int pin_index = vst_gpio_get_pin_index(group, name);

    if (pin_index >= 0 && group->pins[pin_index].callback) 
    {
        group->pins[pin_index].state = state;
        group->pins[pin_index].callback(state, group->pins[pin_index].callback_context);
    }
}

// Bind an output GPIO from one group to an input GPIO of another group
int vst_gpio_bind(vst_gpio_grp *output_group, const char *output_name, vst_gpio_grp *input_group, const char *input_name) 
{
    
    vst_gpio_binding_node_t *new_node = (vst_gpio_binding_node_t *)malloc(sizeof(vst_gpio_binding_node_t));

    if (new_node == NULL) 
    {
        qemu_log("Failed to allocate memory for a new PORT binding\n");
        return -1; // Memory allocation failed
    }
    // Populate the binding data
    new_node->binding.output_group = output_group;
    new_node->binding.output_name = output_name; // Duplicate string
    new_node->binding.input_group = input_group;
    new_node->binding.input_name = input_name; // Duplicate string
    new_node->next = NULL;

    // Add the new node to the end of the list
    if (vst_gpio_bindings_head == NULL) 
    {
        vst_gpio_bindings_head = new_node; // First node in the list
    } 
    else 
    {
        vst_gpio_binding_node_t *current = vst_gpio_bindings_head;
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
        
        // Free dynamically allocated strings
        free((char *)ptemp->binding.output_name);
        free((char *)ptemp->binding.input_name);

        // Free the node itself
        free(ptemp);
    }

    vst_gpio_binding_node_t *gcurrent = vst_gpio_bindings_head;
    while (gcurrent != NULL) 
    {
        vst_gpio_binding_node_t *gtemp = gcurrent;
        gcurrent = gcurrent->next;
        
        // Free dynamically allocated strings
        free((char *)gtemp->binding.output_name);
        free((char *)gtemp->binding.input_name);

        // Free the node itself
        free(gtemp);
    }

    vst_port_bindings_head = NULL;
    vst_gpio_bindings_head = NULL;
    qemu_log("All GPIO bindings are freed.\n");
}