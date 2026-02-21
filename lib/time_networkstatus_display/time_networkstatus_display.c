#include "oled.h"
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "time_networkstatus_display.h"

void task_time_networkstatus_display(void *args) {
    struct time_networkstatus_display_args *time_networkstatus_display_args = (struct time_networkstatus_display_args *) args; // cast the arguments to the correct type so we can access the queue and GPIO numbers
    struct time_networkstatus_display_data display_data; // variable to hold the time and network status data received from the queue
    enum network_status current_network_status = NOT_CONNECTED; // variable to hold the current network status, initialized to NOT_AVAILABLE

    // Configure the GPIO pins for the clock cycle LED and network status LED
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = ((1ULL<<time_networkstatus_display_args->clock_cycle_led_gpio) | (1ULL<<time_networkstatus_display_args->networkstatus_led_gpio)); //bit mask
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = 0;
    io_conf.pull_down_en = 0;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    while(1) {
        // Wait for time and network status updates from the queue
        xQueueReceive(
            *time_networkstatus_display_args->queue, // Queue
            &display_data, // Buffer to hold the received data
            portMAX_DELAY // Wait indefinitely for data to be available
        );

        // Format the time value into a string
        char beats_str[40]; // buffer to hold the formatted time string
        snprintf(beats_str, sizeof(beats_str), "beats: @%.2f", display_data.beat_time / 100.0f);

        // format the network status into a string
        char status_str[23]; // buffer to hold the formatted network status string
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
            case CONNECTING:
                snprintf(status_str, sizeof(status_str), "status: CONNECTING");
                break;
            default:
                snprintf(status_str, sizeof(status_str), "status: UNKNOWN");
                break;
        }

        // Write the time and network status to the OLED display
        write_to_oled(beats_str, status_str);

        // Update the network status LED based on the current network status if it has changed
        if (display_data.status != current_network_status) {
            // Update the network status LED based on the current network status
            if (display_data.status == CONNECTED) {
                gpio_set_level(time_networkstatus_display_args->networkstatus_led_gpio, 1); // Turn on the network status LED if connected
            } else {
                gpio_set_level(time_networkstatus_display_args->networkstatus_led_gpio, 0); // Turn off the network status LED if not connected
            }
    
            current_network_status = display_data.status;
        }

        // Blink the clock cycle LED to indicate that we have updated the display
        gpio_set_level(time_networkstatus_display_args->clock_cycle_led_gpio, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(time_networkstatus_display_args->clock_cycle_led_gpio, 0);
    }
}

// Function to set up the time and network status display task, also initializes the queue for time and network status updates
void set_up_time_networkstatus_display(struct time_networkstatus_display_args* time_networkstatus_display_args) {
    // Initalize queue to hold time and network status updates
    *time_networkstatus_display_args->queue = xQueueCreate(10, sizeof(struct time_networkstatus_display_data));

    // Create a task to handle displaying time and network status
    xTaskCreate(
        task_time_networkstatus_display, // Task function
        "time_networkstatus_display", // Name
        4096, // Stack size
        (void *) time_networkstatus_display_args, // arguments
        1, // Priority
        NULL // Task handle (not used in this case)
    );
}