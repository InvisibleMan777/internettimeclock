#include <driver/gpio.h>
#include <esp_event.h>
#include "stepper_motor.h"

#define STEPPER_PIN_1 GPIO_NUM_34 // GPIO pin for stepper motor coil 1
#define STEPPER_PIN_2 GPIO_NUM_33 // GPIO pin for stepper motor coil 2
#define STEPPER_PIN_3 GPIO_NUM_18 // GPIO pin for stepper motor coil 3
#define STEPPER_PIN_4 GPIO_NUM_17 // GPIO pin for stepper motor coil 4

void task_turn_stepper_motor(void *args) {
    // Configure the GPIO pins for the stepper motor coils
    QueueHandle_t *queue = (QueueHandle_t *) args; // Cast the arguments to the correct type so we can access the queue

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL<<STEPPER_PIN_1) | (1ULL<<STEPPER_PIN_2) | (1ULL<<STEPPER_PIN_3) | (1ULL<<STEPPER_PIN_4)); // bit mask for the stepper motor pins
    io_conf.mode = GPIO_MODE_OUTPUT; // Set as output
    io_conf.pull_up_en = 0; // No pull-up
    io_conf.pull_down_en = 0; // No pull-down
    io_conf.intr_type = GPIO_INTR_DISABLE; // No interrupts
    gpio_config(&io_conf);
    
    uint16_t command; // variable to hold the received command from the queue (not actually used, we just want to receive something to trigger the motor)
    uint8_t step_position = 0; // variable to keep track of the current step position of the motor (0-3 for a 4-step sequence)

    while(1) {
        xQueueReceive(
            *queue, // Queue
            &command, // Buffer to hold the received command (not actually used)
            portMAX_DELAY // Wait indefinitely for a command to be available
        );

        for (int i = 0; i < 41; i++) { // Turn the motor for a certain number of steps (adjust as needed)
            switch (step_position) {
                case 0:
                    gpio_set_level(STEPPER_PIN_1, 1);
                    gpio_set_level(STEPPER_PIN_2, 0);
                    gpio_set_level(STEPPER_PIN_3, 0);
                    gpio_set_level(STEPPER_PIN_4, 0);
                    step_position = 1;
                    break;
                case 1:
                    gpio_set_level(STEPPER_PIN_1, 1);
                    gpio_set_level(STEPPER_PIN_2, 1);
                    gpio_set_level(STEPPER_PIN_3, 0);
                    gpio_set_level(STEPPER_PIN_4, 0);
                    step_position = 2;
                    break;
                case 2:
                    gpio_set_level(STEPPER_PIN_1, 0);
                    gpio_set_level(STEPPER_PIN_2, 1);
                    gpio_set_level(STEPPER_PIN_3, 0);
                    gpio_set_level(STEPPER_PIN_4, 0);
                    step_position = 3;
                    break;
                case 3:
    
                    gpio_set_level(STEPPER_PIN_1, 0);
                    gpio_set_level(STEPPER_PIN_2, 1);
                    gpio_set_level(STEPPER_PIN_3, 1);
                    gpio_set_level(STEPPER_PIN_4, 0);
                    step_position = 4;
                    break;
                case 4:
                    gpio_set_level(STEPPER_PIN_1, 0);
                    gpio_set_level(STEPPER_PIN_2, 0);
                    gpio_set_level(STEPPER_PIN_3, 1);
                    gpio_set_level(STEPPER_PIN_4, 0);
                    step_position = 5;
                    break;
                case 5:
                    gpio_set_level(STEPPER_PIN_1, 0);
                    gpio_set_level(STEPPER_PIN_2, 0);
                    gpio_set_level(STEPPER_PIN_3, 1);
                    gpio_set_level(STEPPER_PIN_4, 1);
                    step_position = 6;
                    break;
                case 6:
                    gpio_set_level(STEPPER_PIN_1, 0);
                    gpio_set_level(STEPPER_PIN_2, 0);
                    gpio_set_level(STEPPER_PIN_3, 0);
                    gpio_set_level(STEPPER_PIN_4, 1);
                    step_position = 7;
                    break;
                case 7:
                    gpio_set_level(STEPPER_PIN_1, 1);
                    gpio_set_level(STEPPER_PIN_2, 0);
                    gpio_set_level(STEPPER_PIN_3, 0);
                    gpio_set_level(STEPPER_PIN_4, 1);
                    step_position = 0;
                    break;
            }
            vTaskDelay(pdMS_TO_TICKS(10)); // Delay between steps (adjust as needed)
        }
    }
}

void set_up_stepper_motor(QueueHandle_t *queue) {
    // Initalize a queue to send stepper motor control commands to
    *queue = xQueueCreate(10, sizeof(uint16_t));

    // Create a task to control the stepper motor
    xTaskCreate(
        task_turn_stepper_motor, 
        "stepper_motor_task", 
        2048, 
        queue, 
        1, 
        NULL);
}