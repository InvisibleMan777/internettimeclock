#ifndef STEPPER_MOTOR_H
#define STEPPER_MOTOR_H

#include <esp_event.h>

void set_up_stepper_motor(QueueHandle_t *queue); // Function to set up the stepper motor control task, also initializes the queue for stepper motor control commands

#endif