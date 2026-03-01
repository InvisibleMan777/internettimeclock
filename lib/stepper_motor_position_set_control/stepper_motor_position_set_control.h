#ifndef POSITION_SET_CONTROL_H
#define POSITION_SET_CONTROL_H

#include <esp_event.h>
#include <driver/gpio.h>

enum stepper_motor_mode {
    NORMAL_OPERATION,
    TIME_SETTING_MODE
};

struct stepper_motor_position_set_control_args {
    QueueHandle_t* stepper_motor_mode_message_box; // Queue to receive the current mode of the stepper motor (normal operation or time setting mode) from the main controller
    QueueHandle_t* stepper_motor_command_queue; // Queue to send stepper motor control commands to the stepper motor control task, the command represents the deca-degrees to rotate the stepper motor (0 - 3600)
    gpio_num_t button_pin; // GPIO pin for the button to change modes
    gpio_num_t led_pin; // GPIO pin for the LED to indicate the current mode (on for time setting mode, off for normal operation)
    gpio_num_t rotery_encoder_pin_a; // GPIO pin for the rotary encoder pin A
    gpio_num_t rotery_encoder_pin_b; // GPIO pin for the rotary encoder pin B
};

void set_up_stepper_motor_position_set_control(struct stepper_motor_position_set_control_args *stepper_motor_position_set_control_args); // Function to set up the stepper motor position set control task, also initializes the queue for receiving the current mode of the stepper motor from the main controller and the queue for sending commands to the stepper motor control task

#endif