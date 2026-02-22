#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <esp_event.h>
#include <driver/gpio.h>

struct stepper_motor_args {
    QueueHandle_t* queue; // Queue to send stepper motor control commands to
    gpio_num_t pin1; // GPIO pin for stepper motor coil 1
    gpio_num_t pin2; // GPIO pin for stepper motor coil 2
    gpio_num_t pin3; // GPIO pin for stepper motor coil 3
    gpio_num_t pin4; // GPIO pin for stepper motor coil 4
};

void set_up_stepper_motor(struct stepper_motor_args *args); // Function to set up the stepper motor control task, also initializes the queue for stepper motor control commands

// type to represent stepper motor control commands, represents the deca-degrees to rotate the stepper motor (0 - 3600)
typedef uint16_t deca_degrees_command_stepper_t; 

#endif