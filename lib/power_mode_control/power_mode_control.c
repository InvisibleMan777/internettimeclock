#include <driver/gpio.h>
#include <esp_event.h>
#include <time.h>
#include "power_mode_control.h"

enum power_mode current_power_mode; // variable to hold the current power mode, initialized to normal mode
clock_t last_power_mode_button_press = 0; // Variable to keep track of the last time the button was pressed, used for debouncing

void isr_power_mode_button(void* args) {
    // Anti-debounce
    if (last_power_mode_button_press != 0 && (clock() - last_power_mode_button_press) < CLOCKS_PER_SEC / 2) {
        return;
    }
    last_power_mode_button_press = clock();

    struct power_mode_control_args* power_mode_control_args = (struct power_mode_control_args*) args; // Cast the arguments to the correct type so we can access the queue

    // Read the current power mode from the queue, if there is no current power mode, we assume we are in normal mode
    current_power_mode = (current_power_mode == NORMAL_MODE) ? LOW_POWER_MODE : NORMAL_MODE;

    // Send the new power mode to the main controller through the queue
    xQueueOverwrite(*power_mode_control_args->power_mode_message_box, &current_power_mode);
}

void set_up_power_mode_control(struct power_mode_control_args *power_mode_control_args) {
    *power_mode_control_args->power_mode_message_box = xQueueCreate(1, sizeof(enum power_mode)); // Initialize the queue for sending the current power mode to the main controller

    // Configure the GPIO pin for the power mode button
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << power_mode_control_args->power_mode_button_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    // Set up an interrupt for the power mode button to change power modes when the button is pressed
    gpio_isr_handler_add(power_mode_control_args->power_mode_button_pin, isr_power_mode_button, power_mode_control_args);

    isr_power_mode_button(power_mode_control_args); // Call the ISR handler once to set the initial power mode in the main controller
}