#include <driver/gpio.h>
#include <esp_event.h>
#include "stepper_motor.h"
#include "stepper_motor_position_set_control.h"
#include "time.h"

volatile clock_t last_rotery_encoder_pin_a_interrupt = 0; // Variable to keep track of the last time the rotary encoder pin A interrupt was triggered, used for debouncing
volatile clock_t last_press = 0; // Variable to keep track of the last time the button was pressed, used for debouncing

enum stepper_motor_mode current_mode; // current mode of the stepper motor (TIME_SETTING_MODE or NORMAL_OPERATION)

// Interrupt service routine for the rotary encoder pin A, used to change the position of the stepper motor in time setting mode
void isr_rotery_encoder_pin_a(void* args) {
    // Anti debounce
    if (last_rotery_encoder_pin_a_interrupt != 0 && (clock() - last_rotery_encoder_pin_a_interrupt) < CLOCKS_PER_SEC / 15) {
        return;
    }
    last_rotery_encoder_pin_a_interrupt = clock();

    // Get the arguments passed to the ISR handler
    struct stepper_motor_position_set_control_args* stepper_motor_position_set_control_args = (struct stepper_motor_position_set_control_args*) args;

    // Check if the rotery encoder is being turned clockwise or anti-clockwise by comparing the levels of the two pins (A and B), 
    if (gpio_get_level(stepper_motor_position_set_control_args->rotery_encoder_pin_b) == gpio_get_level(stepper_motor_position_set_control_args->rotery_encoder_pin_a)) {
        // If the levels are the same, the encoder is turning anti-clockwise and we move the stepper motor a centibeat anti-clockwise
       xQueueSendFromISR(
            *stepper_motor_position_set_control_args->stepper_motor_command_queue,
            &((struct stepper_motor_command){.command = 36, .reverse = true}),
            NULL
        );
    } else {
        // If not, the encoder is turning clockwise and we move the stepper motor a centibeat clockwise
        xQueueSendFromISR(
            *stepper_motor_position_set_control_args->stepper_motor_command_queue,
            &((struct stepper_motor_command){.command = 36, .reverse = false}),
            NULL
        );
    }
}

// Interrupt service routine for the button to change modes, used to switch between normal operation mode and time setting mode
void isr_button(void* args) {
    // Anti debounce
    if (last_press != 0 && (clock() - last_press) < CLOCKS_PER_SEC / 2) {
        return;
    }
    last_press = clock();

    // Get the arguments passed to the ISR handler
    struct stepper_motor_position_set_control_args* stepper_motor_position_set_control_args = (struct stepper_motor_position_set_control_args*) args;

    // Change mode
    if (current_mode == TIME_SETTING_MODE) {
        // Disable interrupts for the rotary encoder in normal operation mode
        current_mode = NORMAL_OPERATION;
        gpio_intr_disable(stepper_motor_position_set_control_args->rotery_encoder_pin_a);
    } else {
        // Enable interrupts for the rotary encoder in time setting mode to allow changing the position on the stepper motor
        current_mode = TIME_SETTING_MODE;
        gpio_intr_enable(stepper_motor_position_set_control_args->rotery_encoder_pin_a);
    }

    // Set the LED to indicate the current mode (on for time setting mode, off for normal operation)
    gpio_set_level(stepper_motor_position_set_control_args->mode_led, (current_mode == NORMAL_OPERATION) ? 0 : 1);

    // Update the message box to the main controller with the current mode
    xQueueOverwriteFromISR(
        *stepper_motor_position_set_control_args->stepper_motor_mode_message_box, 
        &current_mode,
        NULL
    );
}

// Function to set up the stepper motor position set control task, also initializes the queue for receiving the current mode of the stepper motor from the main controller
void set_up_stepper_motor_position_set_control(struct stepper_motor_position_set_control_args *stepper_motor_position_set_control_args) {
    // Initialize a messagebox/queue for receiving the current mode of the stepper motor (normal operation or time setting mode) from the main controller
    *stepper_motor_position_set_control_args->stepper_motor_mode_message_box = xQueueCreate(1, sizeof(enum stepper_motor_mode));

    // Configure input pins for the button and rotary encoder pins
    gpio_config_t input_io_conf = {
        .pin_bit_mask = ((1ULL<<stepper_motor_position_set_control_args->button_pin)| (1ULL<<stepper_motor_position_set_control_args->rotery_encoder_pin_a) | (1ULL<<stepper_motor_position_set_control_args->rotery_encoder_pin_b)),
        .mode = GPIO_MODE_INPUT, 
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&input_io_conf);

    gpio_install_isr_service(0);

    // Set up interrupts for the rotary encoder to change the position of the stepper motor in time setting mode
    // We only need a interrupt for pin A, we determine the direction by comparing pin a to pin b in the interupt service routine
    gpio_isr_handler_add(stepper_motor_position_set_control_args->rotery_encoder_pin_a, isr_rotery_encoder_pin_a, stepper_motor_position_set_control_args); // Add an ISR handler for the rotary encoder pin A to detect changes in the rotary encoder position

    // Set up interrupts for the press of the button to change modes
    gpio_isr_handler_add(stepper_motor_position_set_control_args->button_pin, isr_button, stepper_motor_position_set_control_args); // Add an ISR handler for the button pin to detect button presses

    // Configure output pin for the LED to indicate the current mode (on for time setting mode, off for normal operation)
    gpio_config_t output_io_conf = {
        .pin_bit_mask = (1ULL<<stepper_motor_position_set_control_args->mode_led), 
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&output_io_conf);

    // We call the button ISR manually here to set the initial mode (TIME_SETTING_MODE)
    isr_button(stepper_motor_position_set_control_args);
}