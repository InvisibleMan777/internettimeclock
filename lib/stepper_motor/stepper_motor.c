#include <driver/gpio.h>
#include <esp_event.h>
#include <math.h>
#include "stepper_motor.h"

void task_turn_stepper_motor(void *args) {
    struct stepper_motor_args stepper_motor_args = *(struct stepper_motor_args *) args; // Cast the arguments to the correct type so we can access the queue

    // Configure the GPIO pins for the stepper motor coils
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL<<stepper_motor_args.pin1) | (1ULL<<stepper_motor_args.pin2) | (1ULL<<stepper_motor_args.pin3) | (1ULL<<stepper_motor_args.pin4)); // bit mask for the stepper motor pins
    io_conf.mode = GPIO_MODE_OUTPUT; // Set as output
    io_conf.pull_up_en = 0; // No pull-up
    io_conf.pull_down_en = 0; // No pull-down
    io_conf.intr_type = GPIO_INTR_DISABLE; // No interrupts
    gpio_config(&io_conf);
    
    struct stepper_motor_command command; // variable to hold the received stepper motor control command, represents the deca-degrees to rotate the stepper motor (0 - 3600)
    uint8_t step_position = 0; // variable to keep track of the current step position of the motor
    uint16_t steps_to_move; // variable to hold the calculated number of steps to move based on the received command

    while(1) {
        xQueueReceive(
            *(stepper_motor_args.stepper_motor_command_queue),
            &command,
            portMAX_DELAY
        );

        steps_to_move = roundf(command.command / 0.88f);

        // We are moving the motor in half-steps, so we need to go through 8 step positions for a full cycle. The step_position variable keeps track of the current step position, and we update it in a circular manner as we move the motor.
        for (int i = 0; i < steps_to_move; i++) {
            // We inverse direction by adding 6 and taking modulo 8 of the tracked step position to manipulate the step sequence 
            // this is the same as subtracting 2 (so instead of going from 4 to 5, we go to 5-2=3), but it works better for handling the rollovers
            // We are doing it this way because its cleaner then manualy checking wheter to go up or down the sequence and wheter or not to rollover
            if (command.reverse) {
                step_position = (step_position + 6) % 8;
            }

            switch (step_position) {
                case 0:
                    gpio_set_level(stepper_motor_args.pin1, 1);
                    gpio_set_level(stepper_motor_args.pin2, 0);
                    gpio_set_level(stepper_motor_args.pin3, 0);
                    gpio_set_level(stepper_motor_args.pin4, 0);
                    step_position = 1;
                    break;
                case 1:
                    gpio_set_level(stepper_motor_args.pin1, 1);
                    gpio_set_level(stepper_motor_args.pin2, 0);
                    gpio_set_level(stepper_motor_args.pin3, 0);
                    gpio_set_level(stepper_motor_args.pin4, 1);
                    step_position = 2;
                    break;
                case 2:
                    gpio_set_level(stepper_motor_args.pin1, 0);
                    gpio_set_level(stepper_motor_args.pin2, 0);
                    gpio_set_level(stepper_motor_args.pin3, 0);
                    gpio_set_level(stepper_motor_args.pin4, 1);
                    step_position = 3;
                    break;
                case 3:
    
                    gpio_set_level(stepper_motor_args.pin1, 0);
                    gpio_set_level(stepper_motor_args.pin2, 0);
                    gpio_set_level(stepper_motor_args.pin3, 1);
                    gpio_set_level(stepper_motor_args.pin4, 1);
                    step_position = 4;
                    break;
                case 4:
                    gpio_set_level(stepper_motor_args.pin1, 0);
                    gpio_set_level(stepper_motor_args.pin2, 0);
                    gpio_set_level(stepper_motor_args.pin3, 1);
                    gpio_set_level(stepper_motor_args.pin4, 0);
                    step_position = 5;
                    break;
                case 5:
                    gpio_set_level(stepper_motor_args.pin1, 0);
                    gpio_set_level(stepper_motor_args.pin2, 1);
                    gpio_set_level(stepper_motor_args.pin3, 1);
                    gpio_set_level(stepper_motor_args.pin4, 0);
                    step_position = 6;
                    break;
                case 6:
                    gpio_set_level(stepper_motor_args.pin1, 0);
                    gpio_set_level(stepper_motor_args.pin2, 1);
                    gpio_set_level(stepper_motor_args.pin3, 0);
                    gpio_set_level(stepper_motor_args.pin4, 0);
                    step_position = 7;
                    break;
                case 7:
                    gpio_set_level(stepper_motor_args.pin1, 1);
                    gpio_set_level(stepper_motor_args.pin2, 1);
                    gpio_set_level(stepper_motor_args.pin3, 0);
                    gpio_set_level(stepper_motor_args.pin4, 0);
                    step_position = 0;
                    break;
            }

            vTaskDelay(pdMS_TO_TICKS(10)); 
        }
    }
}

void set_up_stepper_motor(struct stepper_motor_args *stepper_motor_args) {
    // Initalize a queue to send stepper motor control commands to
    *stepper_motor_args->stepper_motor_command_queue = xQueueCreate(99, sizeof(struct stepper_motor_command));

    // Create a task to control the stepper motor
    xTaskCreate(
        task_turn_stepper_motor, // Task function
        "stepper_motor_task", // Name
        2048, // Stack size
        stepper_motor_args, // arguments
        2, // Priority
        NULL // Task handle (not used in this case)
    ); 
}