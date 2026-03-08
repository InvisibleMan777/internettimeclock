#include <esp_event.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include "buzzer.h"

void task_buzzer(void *args) {
    struct buzzer_args buzzer_args = *(struct buzzer_args *) args; // cast the arguments to the correct type so we can access the queue and GPIO number

    // Configure the GPIO pin for the buzzer
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << buzzer_args.buzzer_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);

    // Configure the LEDC timer and channel for controlling the buzzer with PWM
    ledc_timer_config_t ledc_timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = 880,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer_conf);

    // Configure the LEDC channel for the buzzer, we will use this channel to set the duty cycle to turn the buzzer on and off
    ledc_channel_config_t ledc_channel_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = buzzer_args.buzzer_pin,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ledc_channel_conf);

    uint32_t duration_ms; // variable to hold the duration for which the buzzer should be on, received from the queue

    while(1) {
        // Wait for a command from the queue to turn on the buzzer for a certain duration
        xQueueReceive(
            *(buzzer_args.buzzer_command_queue),
            &duration_ms,
            portMAX_DELAY
        );

        // Turn on the buzzer by setting the duty cycle to a non-zero value, in this case 8 out of 1023 (10-bit resolution), wich gives a low volume sound
        ledc_set_duty(ledc_channel_conf.speed_mode, ledc_channel_conf.channel, 8);
        ledc_update_duty(ledc_channel_conf.speed_mode, ledc_channel_conf.channel); // Update the duty to apply the change

        // Keep the buzzer on for the specified duration
        vTaskDelay(pdMS_TO_TICKS(duration_ms));

        // Turn off the buzzer by setting the duty cycle back to 0
        ledc_set_duty(ledc_channel_conf.speed_mode, ledc_channel_conf.channel, 0);
        ledc_update_duty(ledc_channel_conf.speed_mode, ledc_channel_conf.channel);
    }
}

// Function to set up the buzzer task, also initializes the queue for receiving buzzer control commands from the main controller
void set_up_buzzer(struct buzzer_args *buzzer_args) {
    // Initialize a queue for receiving buzzer commands (duration to turn on the buzzer) from the main controller
    *buzzer_args->buzzer_command_queue = xQueueCreate(99, sizeof(uint32_t));

    // Create the buzzer task
    xTaskCreate(
        task_buzzer, 
        "buzzer_task", 
        2048, 
        buzzer_args, 
        2, 
        NULL);
}