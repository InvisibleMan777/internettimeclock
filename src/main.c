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

// OLED Display
#define I2C_SDA_GPIO GPIO_NUM_1 // GPIO number for I2C SDA line
#define I2C_SCL_GPIO GPIO_NUM_2 // GPIO number for I2C SCL line
#define OLED_ADDR 0x3C // I2C address of the OLED display

// LEDS
#define CLOCK_CYCLE_LED_GPIO GPIO_NUM_35 // GPIO number for the LED that indicates clock cycles
#define NETWORK_STATUS_LED_GPIO GPIO_NUM_36 // GPIO number for the LED that indicates network status

// Stepper motor
#define STEPPER_PIN_1 GPIO_NUM_34 // GPIO pin for coil 1
#define STEPPER_PIN_2 GPIO_NUM_33 // GPIO pin for coil 2
#define STEPPER_PIN_3 GPIO_NUM_18 // GPIO pin for coil 3
#define STEPPER_PIN_4 GPIO_NUM_17 // GPIO pin for coil 4

QueueHandle_t time_update_queue; // Queue to hold calculated time updates for display on the OLED
QueueHandle_t stepper_motor_queue; // Queue to hold stepper motor control commands
QueueHandle_t time_networkstatus_display_queue; // Queue to hold time and network status updates

// Structure to hold arguments for the stepper motor control task
struct stepper_motor_args stepper_motor_args = {
    .queue = &stepper_motor_queue,
    .pin1 = STEPPER_PIN_1,
    .pin2 = STEPPER_PIN_2,
    .pin3 = STEPPER_PIN_3,
    .pin4 = STEPPER_PIN_4
};

// Structure to hold arguments for the time and network status display task
struct time_networkstatus_display_args time_networkstatus_display_args = {
    .queue = &time_networkstatus_display_queue,
    .clock_cycle_led_gpio = CLOCK_CYCLE_LED_GPIO,
    .networkstatus_led_gpio = NETWORK_STATUS_LED_GPIO
};

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

    set_up_time_networkstatus_display(&time_networkstatus_display_args);
    set_up_stepper_motor(&stepper_motor_args);
    set_up_time_keeping(&time_update_queue);

    beat_time_t beat_time = 0;
    beat_time_t last_beat_time = 0;
    struct time_networkstatus_display_data time_networkstatus_display_data;

    while (1) {
        // Wait for time updates from the time keeping module
        xQueueReceive(
            time_update_queue,
            &beat_time,
            pdMS_TO_TICKS(1000)
        );

        time_networkstatus_display_data.beat_time = beat_time;
        time_networkstatus_display_data.status = CONNECTED;

        // Send update to the time and networkstatus display task
        xQueueSendToBack(
            time_networkstatus_display_queue,
            &time_networkstatus_display_data,
            pdMS_TO_TICKS(1000)
        );

        // Initialize last_beat_time on the first run by setting it to the current beat_time so that the diffrence is 0
        // If the time is earlier then before (wich can happen after a synch), we set also set the diffrence to 0 so that the motor stays still till the time has cached up with the position of the motor
        // The exception to this is if the time goes from 999.99 to 0, in wich case we DO want the motor to move forward one step

        if (last_beat_time == 0 || (beat_time < last_beat_time && beat_time != 0)) {
            last_beat_time = beat_time;
        }

        // Send a command to the stepper motor task to turn the motor
        xQueueSendToBack(
            stepper_motor_queue,
            &(deca_degrees_command_stepper_t){36 * (beat_time - last_beat_time)}, // Move a step (3,6 degrees) for every centibeat
            pdMS_TO_TICKS(1000)
        );

        // Update last_beat_time to the current beat_time for the next iteration
        last_beat_time = beat_time;
    }
}

