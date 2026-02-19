#include "oled.h"
#include <string.h>
#include "esp_log.h"
#include "esp_event.h"
#include "time_networkstatus_display.h"

void task_time_networkstatus_display(void *args) {
    struct time_networkstatus_display display_data;

    while(1) {
        // Wait for time and network status updates from the queue
        xQueueReceive(
            (QueueHandle_t) args, // Queue to receive time and network status updates
            &display_data, // Buffer to hold the received data
            portMAX_DELAY // Wait indefinitely for data to be available
        );

        // Format the beats value into a string
        char beats_str[16]; // buffer to hold the formatted beats string
        snprintf(beats_str, sizeof(beats_str), "beats: %.2f", display_data.beats); // Format the beats value into a string
        ESP_LOGI("DISPLAY", "Received beats: %.2f", display_data.beats); // Log the received beats value for debugging

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

        // Write the beats and network status to the OLED display
        write_to_oled(beats_str, status_str);
    }
}