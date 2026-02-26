#include <esp_event.h>
#include <esp_log.h>

#include "oled.h"
#include "time_networkstatus_display.h"
#include "time_keeping.h"
#include "network_interface.h"
#include "stepper_motor.h"
#include <math.h>

// env variables for wifi credentials, set in the .env file
#define WIFI_SSID CONFIG_WIFI_SSID // WiFi SSID to connect to
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD // WiFi password

// OLED Display IO
#define I2C_SDA_GPIO GPIO_NUM_1 // GPIO number for I2C SDA line
#define I2C_SCL_GPIO GPIO_NUM_2 // GPIO number for I2C SCL line
#define OLED_ADDR 0x3C // I2C address of the OLED display

// LEDS IO
#define CLOCK_CYCLE_LED_GPIO GPIO_NUM_35 // GPIO number for the LED that indicates clock cycles
#define NETWORK_STATUS_LED_GPIO GPIO_NUM_36 // GPIO number for the LED that indicates network status

// Stepper motor IO
#define STEPPER_PIN_1 GPIO_NUM_34 // GPIO pin for coil 1
#define STEPPER_PIN_2 GPIO_NUM_33 // GPIO pin for coil 2
#define STEPPER_PIN_3 GPIO_NUM_18 // GPIO pin for coil 3
#define STEPPER_PIN_4 GPIO_NUM_17 // GPIO pin for coil 4

// Task communication queues
QueueHandle_t time_update_queue; // Time keeping module sends time updates to the main controller through this queue
QueueHandle_t stepper_motor_queue; // The main controller sends stepper motor control commands to the stepper motor control task through this queue, the command represents the deca-degrees to rotate the stepper motor (0 - 3600)
QueueHandle_t time_networkstatus_display_queue; // The main controller sends time and network status updates to the time and network status display task through this queue, which will display it on the OLED LCD and update the network status LED
QueueHandle_t networkstatus_message_box; // The network interface task updates the current network status, wich the the main controller can read through this queue/messagebox,it uses it to decide what to display on the OLED and whether to reset SNTP synchronization when connection is (re-)established

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

    // Initialize tasks
    set_up_time_networkstatus_display(&time_networkstatus_display_args);
    set_up_stepper_motor(&stepper_motor_args);
    set_up_time_keeping(&time_update_queue);

    // Start WiFi connection
    wifi_start(WIFI_SSID, WIFI_PASSWORD, &networkstatus_message_box);
    write_to_oled("Connecting...", WIFI_SSID); // Display "Connecting to WiFi..." on the OLED

    // Wait for the WiFi connection to be established, after 15 seconds it times out, and we continue without a connection
    if (!(wait_on_connection())) {
        // Wifi fails
        write_to_oled("WiFi Failed", "");
        start_time_keeping(); // Start the time keeping even if WiFi connection failed, so that we at least have the clock running and can display the time and network status on the OLED)

    } else {
        // Wifi succeeds
        write_to_oled("Connected", WIFI_SSID);
        enable_sntp_sync(); // Enable SNTP synchronization to get the correct time from the network every x minutes
        start_time_keeping_on_sync(); // Start the time keeping when the first SNTP synchronization occurs, so that we have the correct time from the start
    }

    beat_time_t time_on_stepper_motor = 0; // variable to keep track of the time that is currently shown on the stepper motor, initialized to 0 (12:00:00) at the start
    beat_time_t current_time = 0; // Buffer variable to hold the current time received from the time keeping module

    enum network_status current_network_status = UNDEFINED; // variable to hold the current network status received from the network interface module
    enum network_status last_network_status = UNDEFINED; // Last known network status, used to detect changes in network status to reset SNTP synchronization when connection is (re-)established

    struct time_intervals clock_intervals; // Buffer variable to hold the calculated intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)

    while (1) {
        // Wait for time updates from the time keeping module
        xQueueReceive(
            time_update_queue,
            &current_time,
            portMAX_DELAY
        );

        // Get the current netwWork status from the network interface
        xQueuePeek(
            networkstatus_message_box,
            &current_network_status,
            0
        );

        // Send time and network status to the time and networkstatus display task, wich will display it on the OLED LCD and update the network status LED
        xQueueSendToBack(
            time_networkstatus_display_queue,
            &((struct time_networkstatus_display_data){.beat_time = current_time, .status = current_network_status}),
            pdMS_TO_TICKS(1000)
        );

        // Calculate the intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)
        clock_intervals = calculate_clock_intervals(time_on_stepper_motor, current_time);

        // If the clockwise interval from time on the stepper motor to the current time is smaller, move the stepper motor clockwise to the right time
        // If the anti-clockwise interval is smaller, wait until the current time is the same or past the time on the stepper motor,
        // we specificly do not move the stepper motor anti-clockwise, because that would just look bad and be confusing on a clock
        if (clock_intervals.clockwise_interval < clock_intervals.anti_clockwise_interval) {
            xQueueSendToBack(
                stepper_motor_queue,
                &(deca_degrees_command_stepper_t){36 * (clock_intervals.clockwise_interval)}, // Move a step (3,6 degrees) for every centibeat
                pdMS_TO_TICKS(1000)
            );

            time_on_stepper_motor = current_time;
        }

        // Reset SNTP synchronization when connection is re-established
        // When losing connection, synchonization will stop until its reset again
        if ((last_network_status != CONNECTED && last_network_status != UNDEFINED) && current_network_status == CONNECTED) {
            restart_sntp_synch();
        }

        last_network_status = current_network_status;
    }
}

