#ifndef BUZZER_H
#define BUZZER_H
#include <esp_event.h>
#include <driver/gpio.h>

struct buzzer_args {
    QueueHandle_t* buzzer_command_queue; // Queue to receive buzzer control commands, the command represents the duration in milliseconds for which the buzzer should be on
    gpio_num_t buzzer_pin; // GPIO pin for the buzzer
};

void set_up_buzzer(struct buzzer_args *buzzer_args); // Function to set up the buzzer task, also initializes the queue for receiving buzzer control commands from the main controller

#endif