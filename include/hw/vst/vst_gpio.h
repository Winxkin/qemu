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

typedef enum
{
    LEVEL_SENSITIVE = 0,
    NEGEDGE_SENSITIVE, // 1 -> 0
    POSEDGE_SENSITIVE // 0 -> 1
} vst_gpio_trigger;

// GPIO callback function
typedef void (*vst_gpio_callback_t)(vst_gpio_state state, void *context);
typedef void (*vst_port_callback_t)(uint32_t value, void *context);

// GPIO Pin structure
typedef struct vst_gpio_pin
{
    vst_gpio_mode mode;            // Pin mode (input/output)
    vst_gpio_trigger trigger;      // Trigger type
    vst_gpio_state state;          // Current state (LOW/HIGH)
    const char *name;            // GPIO name (e.g., "GPIO1")
    vst_gpio_callback_t callback;    // Callback for input pin when triggered
    void *callback_context;      // Callback context (user data)
} vst_gpio_pin;

typedef struct vst_gpio_port 
{
    vst_gpio_mode mode;
    const char *name;
    uint32_t num_pins;
    uint32_t value;
    uint32_t mask;
    vst_port_callback_t callback;
    void *callback_context;
} vst_gpio_port;

typedef struct vst_port_binding 
{
    vst_gpio_port *output_port;   // Output group
    vst_gpio_port *input_port;    // Input group
} vst_port_binding_t;

// GPIO Binding structure
typedef struct vst_gpio_binding 
{
    vst_gpio_pin *output_pin;   // Output pin
    vst_gpio_pin *input_pin;    // Input pin
} vst_gpio_binding_t;


// PORT functions
void vst_port_init(vst_gpio_port *port, const char *name, uint32_t num_pins, vst_gpio_mode mode, vst_port_callback_t callback, void *context);
void vst_port_write(vst_gpio_port *port, uint32_t value);
uint32_t vst_port_read(vst_gpio_port *port);
int vst_port_bind(vst_gpio_port *output_port, vst_gpio_port *input_port);

// GPIO group functions
void vst_gpio_init(vst_gpio_pin *pin, const char *pin_name, vst_gpio_mode pin_mode, vst_gpio_callback_t callback, void *context);
void vst_gpio_write(vst_gpio_pin *pin, vst_gpio_state state);
vst_gpio_state vst_gpio_read(vst_gpio_pin *pin) ;
int vst_gpio_bind(vst_gpio_pin *output_pin, vst_gpio_pin *input_pin);


// Free memory allocated for gpio bindings 
void vst_free_bindings(void);

#endif // VST_GPIO_H
