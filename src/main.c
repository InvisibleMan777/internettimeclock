#include <esp_event.h>
#include <esp_log.h>

#include "oled.h"
#include "time_networkstatus_display.h"
#include "time_keeping.h"
#include "network_interface.h"
#include "stepper_motor.h"

// env variables for wifi credentials, set in the .env file
#define WIFI_SSID CONFIG_WIFI_SSID // WiFi SSID to connect to
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD // WiFi password

#define I2C_SDA_GPIO 1 // GPIO number for I2C SDA line
#define I2C_SCL_GPIO 2 // GPIO number for I2C SCL line
#define OLED_ADDR 0x3C // I2C address of the OLED display

#define CLOCK_CYCLE_LED_GPIO 35 // GPIO number for the LED that indicates clock cycles
#define NETWORK_STATUS_LED_GPIO 36 // GPIO number for the LED that indicates network status

QueueHandle_t time_update_queue; // Queue to hold calculated time updates for display on the OLED
QueueHandle_t stepper_motor_queue; // Queue to hold stepper motor control commands
QueueHandle_t time_networkstatus_display_queue; // Queue to hold time and network status updates
struct time_networkstatus_display_args time_networkstatus_display_args; // Structure to hold arguments for the time and network status display task

void app_main(void) {
    // Initialize the OLED display
    initialize_oled(OLED_ADDR, I2C_SDA_GPIO, I2C_SCL_GPIO);
    write_to_oled("Starting...", ""); // Display "Connecting to WiFi..." on the OLED

    // Start WiFi connection
    wifi_start(WIFI_SSID, WIFI_PASSWORD);
    write_to_oled("Connecting...", WIFI_SSID); // Display "Connecting to WiFi..." on the OLED

    // Wait for the WiFi connection to be established, with a timeout of 15 seconds
    if (!(wait_on_connection())) {
        write_to_oled("WiFi Failed", "");
        return;
    }

    write_to_oled("Connected", WIFI_SSID);

    // Set up the time and network status display task
    time_networkstatus_display_args = (struct time_networkstatus_display_args) {
        .queue = &time_networkstatus_display_queue,
        .clock_cycle_led_gpio = CLOCK_CYCLE_LED_GPIO,
        .networkstatus_led_gpio = NETWORK_STATUS_LED_GPIO
    };
    set_up_time_networkstatus_display(&time_networkstatus_display_args);

    set_up_stepper_motor(&stepper_motor_queue);

    set_up_time_keeping(&time_update_queue);
    write_to_oled("Fetching...", "");

    beat_time_t beat_time;
    struct time_networkstatus_display_data time_networkstatus_display_data;

    while (1) {
        // Wait for time updates from the time keeping module
        xQueueReceive(
            time_update_queue,
            &beat_time,
            pdMS_TO_TICKS(5000)
        );

        time_networkstatus_display_data.beat_time = beat_time;
        time_networkstatus_display_data.status = CONNECTED;

        // Send update to the time and networkstatus display task
        xQueueSendToBack(
            time_networkstatus_display_queue,
            &time_networkstatus_display_data,
            pdMS_TO_TICKS(5000)
        );

        // Send a command to the stepper motor task to turn the motor (the actual value sent is not used, we just want to send something to trigger the motor)
        xQueueSendToBack(
            stepper_motor_queue,
            &(uint16_t){0},
            pdMS_TO_TICKS(5000)
        );
    }
}

