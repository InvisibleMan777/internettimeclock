#ifndef POWER_MODE_CONTROL_H
#define POWER_MODE_CONTROL_H

#include <driver/gpio.h>
#include <esp_event.h>

// Structure to hold the arguments for the power mode control module
struct power_mode_control_args {
    QueueHandle_t *power_mode_message_box; // Queue to send the current power mode to the main controller
    gpio_num_t power_mode_button_pin; // GPIO pin for the button to change power modes
};

enum power_mode {
    LOW_POWER_MODE,
    NORMAL_MODE,
    UNDEFINED_POWER_MODE,
};

void set_up_power_mode_control(struct power_mode_control_args *power_mode_control_args); // Function to set up the power mode control module, also initializes the queue for sending the current power mode to the main controller and sets up the GPIO pin and interrupt for the power mode button

#endif