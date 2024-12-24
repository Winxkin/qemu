#ifndef VST_GPIO_H
#define VST_GPIO_H
#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qapi/error.h"

// GPIO pin modes
typedef enum 
{
    GPIO_MODE_INPUT,
    GPIO_MODE_OUTPUT
} vst_gpio_mode;

// GPIO pin states
typedef enum 
{
    GPIO_LOW = 0,
    GPIO_HIGH = 1
} vst_gpio_state;

// GPIO callback function
typedef void (*vst_gpio_callback_t)(vst_gpio_state state, void *context);
typedef void (*vst_port_callback_t)(uint32_t value, void *context);

// GPIO Pin structure
typedef struct vst_gpio_pin 
{
    uint32_t pin;                // Pin number
    vst_gpio_mode mode;            // Pin mode (input/output)
    vst_gpio_state state;          // Current state (LOW/HIGH)
    const char *name;            // GPIO name (e.g., "GPIO1")
    vst_gpio_callback_t callback;    // Callback for input pin when triggered
    void *callback_context;      // Callback context (user data)
} vst_gpio_pin;

typedef struct vst_gpio_port 
{
    vst_gpio_mode mode;
    const char *name;
    uint32_t num_pins : 6;
    uint32_t value;
    uint32_t mask;
    vst_port_callback_t callback;
    void *callback_context;
} vst_gpio_port;

// Device structure containing GPIOs
typedef struct vst_gpio_group 
{
    const char *grp_name;     // Device name
    vst_gpio_pin *pins;            // GPIO pins array
    uint32_t num_pins;           // Number of GPIO pins
} vst_gpio_grp;

typedef struct vst_port_binding 
{
    vst_gpio_port *output_port;   // Output group
    const char *output_name;        // Output pin name
    vst_gpio_port *input_port;    // Input group
    const char *input_name;         // Input pin name
} vst_port_binding_t;

// GPIO Binding structure
typedef struct vst_gpio_binding 
{
    vst_gpio_grp *output_group;   // Output group
    const char *output_name;        // Output pin name
    vst_gpio_grp *input_group;    // Input group
    const char *input_name;         // Input pin name
} vst_gpio_binding_t;


// PORT functions
void vst_port_init(vst_gpio_port *port, const char *name, uint32_t num_pins, vst_gpio_mode mode);
void vst_port_write(vst_gpio_port *port, const char *name, uint32_t value);
uint32_t vst_port_read(vst_gpio_port *port, const char *name);
// PORT binding functions
int vst_port_bind(vst_gpio_port *output_port, const char *output_name, vst_gpio_port *input_port, const char *input_name);
void vst_port_update_input(vst_gpio_port *port, const char *name, uint32_t value);

// GPIO group functions
void vst_gpio_grp_init(vst_gpio_grp *group, const char *grp_name, uint32_t num_pins);
void vst_gpio_set_mode(vst_gpio_grp *group, const char *name, vst_gpio_mode mode);
void vst_gpio_write(vst_gpio_grp *group, const char *name, vst_gpio_state state);
vst_gpio_state vst_gpio_read(vst_gpio_grp *group, const char *name);
int vst_gpio_get_pin_index(vst_gpio_grp *group, const char *name);

// GPIO binding functions
int vst_gpio_bind(vst_gpio_grp *output_group, const char *output_name, vst_gpio_grp *input_group, const char *input_name);
void vst_gpio_update_input(vst_gpio_grp *group, const char *name, vst_gpio_state state);

// Input GPIO callback handler
void vst_gpio_set_callback(vst_gpio_grp *group, const char *name, vst_gpio_callback_t callback, void *context);

// Input PORT callback handler
void vst_port_set_callback(vst_gpio_port *port, const char *name, vst_port_callback_t callback, void *context);

// Free memory allocated for gpio bindings 
void vst_free_bindings(void);

#endif // VST_GPIO_H
