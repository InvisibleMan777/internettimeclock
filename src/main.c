#include <esp_event.h>
#include <esp_log.h>
#include <esp_netif.h>

#include "oled.h"
#include "time_networkstatus_display.h"
#include "time_keeping.h"
#include "network_interface.h"
#include "stepper_motor.h"
#include "stepper_motor_position_set_control.h"
#include "buzzer.h"
#include "power_mode_control.h"

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
#define MODE_LED_GPIO GPIO_NUM_37 // GPIO number for the LED that indicates the current mode of the stepper motor (on for time setting mode, off for normal operation)

// Stepper motor IO
#define STEPPER_PIN_1 GPIO_NUM_34 // GPIO pin for coil 1
#define STEPPER_PIN_2 GPIO_NUM_33 // GPIO pin for coil 2
#define STEPPER_PIN_3 GPIO_NUM_18 // GPIO pin for coil 3
#define STEPPER_PIN_4 GPIO_NUM_17 // GPIO pin for coil 4

// Rotery encoder IO for stepper motor position set control
#define ROTARY_ENCODER_PIN_A GPIO_NUM_4 // GPIO pin for rotary encoder pin A
#define ROTARY_ENCODER_PIN_B GPIO_NUM_5 // GPIO pin for rotary encoder pin B
#define MODE_BUTTON_PIN GPIO_NUM_7 // GPIO pin for the button to change modes

// IO buzzer
#define BUZZER_GPIO GPIO_NUM_6 // GPIO pin for the buzzer

// IO power mode control
#define POWER_MODE_BUTTON_GPIO GPIO_NUM_0 // this is connected to the boot button

// Synchonization settings
#define SNTP_SYNCH_INTERVAL_MS 20 * 1000 // synchronize every 20 seconds
#define SNTP_SERVER_URL "time.google.com" // URL of the SNTP server to synchronize with

// Task communication queues
QueueHandle_t time_update_queue; // Time keeping module sends time updates to the main controller through this queue
QueueHandle_t networkstatus_message_box; // The network interface task updates the current network status, wich the the main controller can read through this queue/messagebox
QueueHandle_t stepper_motor_mode_message_box; // The stepper motor position set control task sends the current mode of the stepper motor (normal operation or time setting mode) to the main controller through this queue/messagebox
QueueHandle_t power_mode_message_box; // The power mode control module sends the current power mode to the main controller through this queue/messagebox
QueueHandle_t stepper_motor_command_queue; // The main controller and stepper motor position set controller send commands to the stepper motor task through this queue, the command represents the deca-degrees to rotate the stepper motor (0 - 3600)
QueueHandle_t time_networkstatus_display_queue; // The main controller sends time and network status updates to the time and network status display task through this queue, which will display it on the OLED LCD and update the network status LED
QueueHandle_t buzzer_command_queue; // The main controller sends commands to the buzzer task through this queue, the command represents the duration in milliseconds for which the buzzer should be on

// Arguments for the stepper motor module
struct stepper_motor_args stepper_motor_args = {
    .stepper_motor_command_queue = &stepper_motor_command_queue,
    .pin1 = STEPPER_PIN_1,
    .pin2 = STEPPER_PIN_2,
    .pin3 = STEPPER_PIN_3,
    .pin4 = STEPPER_PIN_4
};

// Arguments for the stepper motor position set control module
struct stepper_motor_position_set_control_args stepper_motor_position_set_control_args = {
    .stepper_motor_mode_message_box = &stepper_motor_mode_message_box, 
    .stepper_motor_command_queue = &stepper_motor_command_queue, 
    .button_pin = MODE_BUTTON_PIN,
    .mode_led = MODE_LED_GPIO,
    .rotery_encoder_pin_a = ROTARY_ENCODER_PIN_A,
    .rotery_encoder_pin_b = ROTARY_ENCODER_PIN_B 
};

// Arguments for the time and network status display module
struct time_networkstatus_display_args time_networkstatus_display_args = {
    .time_networkstatus_display_queue = &time_networkstatus_display_queue,
    .clock_cycle_led = CLOCK_CYCLE_LED_GPIO,
    .networkstatus_led = NETWORK_STATUS_LED_GPIO 
};

// Arguments for the buzzer module
struct buzzer_args buzzer_args = {
    .buzzer_command_queue = &buzzer_command_queue,
    .buzzer_pin = BUZZER_GPIO
};

// Arguments for the power mode control module
struct power_mode_control_args power_mode_control_args = {
    .power_mode_message_box = &power_mode_message_box,
    .power_mode_button_pin = POWER_MODE_BUTTON_GPIO
};

// Arguments for the SNTP synchronization
struct sntp_synch_args sntp_synch_args = {
    .synch_interval_ms = SNTP_SYNCH_INTERVAL_MS,
    .ntp_server_url = SNTP_SERVER_URL
};

// Main task
void app_main(void) {
    vTaskPrioritySet(NULL, 3); // Set the priority of the main task to 3 (highest)
    esp_event_loop_create_default(); // Create the default event loop, needed for communication between tasks

    initialize_oled(OLED_ADDR, I2C_SDA_GPIO, I2C_SCL_GPIO);
    write_to_oled("Starting...", ""); // Display "Connecting to WiFi..." on the OLED

    // Initialize the GPIO ISR service, needed for pin interrupts
    gpio_install_isr_service(0);

    // Initialize tasks, queues, and modules
    set_up_time_networkstatus_display(&time_networkstatus_display_args);
    set_up_stepper_motor(&stepper_motor_args);
    set_up_stepper_motor_position_set_control(&stepper_motor_position_set_control_args);
    set_up_power_mode_control(&power_mode_control_args);
    set_up_buzzer(&buzzer_args);
    set_up_time_keeping(&time_update_queue);

    // Start WiFi connection
    wifi_start(WIFI_SSID, WIFI_PASSWORD, &networkstatus_message_box);
    write_to_oled("Connecting...", WIFI_SSID); // Display "Connecting to WiFi..." on the OLED

    bool connected_on_start = wait_on_connection(); // Wait for the WiFi connection to be established, after 15 seconds it times out, and we continue without a connection

    // Wait for the WiFi connection to be established, after 15 seconds it times out, and we continue without a connection
    if (!connected_on_start) {
        // Wifi fails
        write_to_oled("WiFi Failed", "");
        start_time_keeping(); // Start the time keeping even if WiFi connection failed, so that we at least have the clock running and can display the time and network status on the OLED)

    } else {
        // Wifi succeeds
        write_to_oled("Connected", WIFI_SSID);
        set_up_sntp_sync(&sntp_synch_args); // Enable SNTP synchronization to get the correct time from the network every x minutes
        start_time_keeping_on_sync(); // Start the time keeping when the first SNTP synchronization occurs, so that we have the correct time from the start
    }

    beat_time_t time_on_stepper_motor = 0; // variable to keep track of the time that is currently shown on the stepper motort
    beat_time_t current_time = 0; // Buffer to hold the current time received from the time keeping module

    struct clock_intervals clock_intervals; // Variable to hold the calculated intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)
    uint32_t steps_to_take; // Variable to hold the calculated number of steps the stepper motor needs to take to move

    enum network_status current_network_status = UNDEFINED_NETWORKSTATUS; // buffer to hold the current network status received from the network interface module
    enum network_status last_network_status = connected_on_start ? CONNECTED : UNDEFINED_NETWORKSTATUS; // Last known network status, used to detect changes in network status to reset SNTP synchronization when connection is (re-)established

    enum stepper_motor_mode current_stepper_motor_mode = NORMAL_OPERATION; // buffer to hold the current mode of the stepper motor (normal operation or time setting mode) received from the stepper motor position set control module
    enum power_mode current_power_mode = NORMAL_MODE; // buffer to hold the current power mode received from the power mode control module

    while (1) {
        // Wait for time updates from the time keeping module
        // we will get an update every centibeat
        if (xQueueReceive(time_update_queue, &current_time, pdMS_TO_TICKS(20000)) != pdTRUE) {
            // When connected, timekeeping wil only start after the first synch, When this takes to long, or if we lose connection while waiting. Start the time keeping to at least have the clock running
            start_time_keeping();
            continue;
        }

        // Get the current network status from the network interface
        xQueuePeek(
            networkstatus_message_box,
            &current_network_status,
            0
        );

        // Get the current mode of the stepper motor (normal operation or time setting mode) from the stepper motor position set control module
        xQueuePeek(
            stepper_motor_mode_message_box,
            &current_stepper_motor_mode,
            0
        ); 

        // Get the current power mode from the power mode control module
        xQueuePeek(
            power_mode_message_box,
            &current_power_mode,
            0
        );

        // Send a command to the buzzer task to make a short beep sound
        xQueueSendToBack(
            buzzer_command_queue,
            &(uint32_t){100},
            pdMS_TO_TICKS(800) // max wait is around a centibeat
        );

        // Send time and network status to the time and networkstatus display task, wich will display it on the OLED LCD and update the network status LED
        switch (current_power_mode) {
            case NORMAL_MODE:
                xQueueSendToBack(
                    time_networkstatus_display_queue,
                    &((struct time_networkstatus_display_data){.beat_time = current_time, .status = current_network_status, .disable_oled_lcd = false}),
                    pdMS_TO_TICKS(800)
                );
                break;
            
            // In low power mode, we disable the OLED LCD display to save power
            case LOW_POWER_MODE:
                xQueueSendToBack(
                    time_networkstatus_display_queue,
                    &((struct time_networkstatus_display_data){.beat_time = current_time, .status = current_network_status, .disable_oled_lcd = true}),
                    pdMS_TO_TICKS(800)
                );
                break;

            default:
                break;
        }

        // Update the stepper motor
        switch (current_stepper_motor_mode) {
            // If the mode is normal operation, we want to update the position of the stepper motor to show the current time,
            case NORMAL_OPERATION:
                // If the time is the same, we do not need to move the stepper motor
                if (time_on_stepper_motor == current_time) {
                    break;
                }

                // Calculate the intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)
                clock_intervals = calculate_clock_intervals(time_on_stepper_motor, current_time);

                // Move the stepper motor in the direction of the shortest interval to the current time
                // NOTE: the stepper motor takes about half a centibeat to move one step, so after 2 steps the time wil have gone a centibeat forward (clockwise)
                // For this reason, going clockwise will take 1.5 times the interval, and going anti-clockwise 1/1.5 times the interval
                if (clock_intervals.anti_clockwise_interval / 1.5 < clock_intervals.clockwise_interval * 1.5) {
                    // Instead of correcting big intervals in one big step, We move in steps of max 2 per update (which looks continuous because it takes roughly half a centibeat to move a step)
                    // The reason why we do this is so that the stepper motor module isnt stuck executing one big step for a long time
                    // Also when when going in one big step anti-clockwise, because time moves forward, it will overshoot and have to correct itself clockwise again, wich looks very wonky
                    steps_to_take = clock_intervals.anti_clockwise_interval > 2 ? 2 : clock_intervals.anti_clockwise_interval;

                    xQueueSendToBack(
                        stepper_motor_command_queue,
                        &((struct stepper_motor_command){.command = 36 * steps_to_take, .reverse = true}), // Every centibeat corresponds to 1 100th of a full rotation, so a 36 command (360/100 = 3,6 degrees) corresponds to 1 centibeat
                        pdMS_TO_TICKS(800)
                    );

                    // Update the tracked time on the stepper motor, we have to make sure to wrap around at a full rotation (100 centibeats)
                    time_on_stepper_motor = (time_on_stepper_motor >= steps_to_take) ? time_on_stepper_motor - steps_to_take : 100000 - (steps_to_take - time_on_stepper_motor);
                  
                // same going clockwise
                } else {
                    steps_to_take = clock_intervals.clockwise_interval > 2 ? 2 : clock_intervals.clockwise_interval;

                    xQueueSendToBack(
                        stepper_motor_command_queue,
                        &((struct stepper_motor_command){.command = 36 * steps_to_take, .reverse = false}),
                        pdMS_TO_TICKS(800)
                    );

                    time_on_stepper_motor = (time_on_stepper_motor + steps_to_take < 100000) ? time_on_stepper_motor + steps_to_take : time_on_stepper_motor + steps_to_take - 100000;
                }
                break;

            // if the mode is time setting mode, we do not update the position of the stepper motor, so that the user can set 0 point of the clock
            case TIME_SETTING_MODE:
                // The only thing we have to do is set the tracked position of the stepper motor to 0, the code will then take care of the rest once the mode gets set back to normal operation mode
                time_on_stepper_motor = 0;
                break;

            default:
                break;
        }

        // Reset SNTP synchronization when connection is re-established
        // When losing connection, synchonization will stop until its reset again
        if ((last_network_status != CONNECTED && current_network_status == CONNECTED)) {
            // Start or restart SNTP synchronization to get the correct time from the network, depending on whether synchronization was already enabled before losing connection or not
            if (connected_on_start) {
                restart_sntp_synch(&sntp_synch_args);
            } else {
                set_up_sntp_sync(&sntp_synch_args);
            }
        }

        last_network_status = current_network_status;
    }
}