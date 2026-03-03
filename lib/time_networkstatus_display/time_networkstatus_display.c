#include "oled.h"
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "time_networkstatus_display.h"
#include "network_interface.h"

void task_time_networkstatus_display(void *args) {
    struct time_networkstatus_display_args time_networkstatus_display_args = *(struct time_networkstatus_display_args *) args; // cast the arguments to the correct type so we can access the queue and GPIO numbers

    // Configure the GPIO pins for the clock cycle LED and network status LED
    gpio_config_t io_conf = {
        .pin_bit_mask = ((1ULL<<time_networkstatus_display_args.clock_cycle_led) | (1ULL<<time_networkstatus_display_args.networkstatus_led)), //bit mask
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    struct time_networkstatus_display_data display_data; // variable to hold the time and network status data received from the queue

    while(1) {
        // Wait for time and network status updates from the queue
        xQueueReceive(
            *(time_networkstatus_display_args.time_networkstatus_display_queue),
            &display_data,
            portMAX_DELAY
        );

        // Update the OLED display with received time and network status data, if the OLED LCD display is not disabled
        if (!display_data.disable_oled_lcd) {
            // Format the time value into a string
            char beats_str[40]; 
            snprintf(beats_str, sizeof(beats_str), "beats: @%.2f", display_data.beat_time / 100.0f);

            // format the network status into a string
            char status_str[23];
            switch (display_data.status) {
                case CONNECTED:
                    snprintf(status_str, sizeof(status_str), "status: CONNECTED");
                    break;
                case NOT_CONNECTED:
                    snprintf(status_str, sizeof(status_str), "status: NOT_CONNECTED");
                    break;
                case NOT_AVAILABLE:
                    snprintf(status_str, sizeof(status_str), "status: NOT_AVAILABLE");
                    break;
                case ERROR:
                    snprintf(status_str, sizeof(status_str), "status: ERROR");
                    break;
                default:
                    snprintf(status_str, sizeof(status_str), "status: ERROR");
                    break;
            }

            // Write the time and network status to the OLED display
            write_to_oled(beats_str, status_str);

        } else {
            // If the OLED LCD display is disabled, instead of the time and networkstatus, we write empty strings to it, 
            // Because it is oled, this will turn off all the pixels
            write_to_oled("", "");
        }

        // Blink the clock cycle LED to indicate that we have updated the display
        gpio_set_level(time_networkstatus_display_args.clock_cycle_led, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(time_networkstatus_display_args.clock_cycle_led, 0);
    }
}

// Function to set up the time and network status display task, also initializes the queue for time and network status updates
void set_up_time_networkstatus_display(struct time_networkstatus_display_args* time_networkstatus_display_args) {
    // Initalize queue to hold time and network status updates
    *time_networkstatus_display_args->time_networkstatus_display_queue = xQueueCreate(99, sizeof(struct time_networkstatus_display_data));

    // Create a task to handle displaying time and network status
    xTaskCreate(
        task_time_networkstatus_display, // Task function
        "time_networkstatus_display", // Name
        4096, // Stack size
        time_networkstatus_display_args, // arguments
        2, // Priority
        NULL // Task handle (not used in this case)
    );
}