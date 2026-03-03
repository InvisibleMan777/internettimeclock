#include <esp_event.h>
#include <esp_log.h>

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
    .power_mode_button_pin = POWER_MODE_BUTTON_GPIO,
    .power_mode_message_box = &power_mode_message_box
};

void app_main(void) {
    // Initialize the OLED display
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
    struct clock_intervals clock_intervals; // Buffer variable to hold the calculated intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)

    enum network_status current_network_status = UNDEFINED; // variable to hold the current network status received from the network interface module
    enum network_status last_network_status = UNDEFINED; // Last known network status, used to detect changes in network status to reset SNTP synchronization when connection is (re-)established

    enum stepper_motor_mode current_stepper_motor_mode = NORMAL_OPERATION; // variable to hold the current mode of the stepper motor (normal operation or time setting mode) received from the stepper motor position set control module
    enum power_mode current_power_mode = NORMAL_MODE; // variable to hold the current power mode received from the power mode control module

    while (1) {
        // Wait for time updates from the time keeping module
        // we will get an update every centibeat
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

        // Send time and network status to the time and networkstatus display task, wich will display it on the OLED LCD and update the network status LED
        switch (current_power_mode) {
            case NORMAL_MODE:
                xQueueSendToBack(
                    time_networkstatus_display_queue,
                    &((struct time_networkstatus_display_data){.beat_time = current_time, .status = current_network_status, .disable_oled_lcd = false}),
                    pdMS_TO_TICKS(800) // max wait is around a centibeat
                );
                break;
            
            // In low power mode, we disable the OLED LCD display to save power
            case LOW_POWER_MODE:
                xQueueSendToBack(
                    time_networkstatus_display_queue,
                    &((struct time_networkstatus_display_data){.beat_time = current_time, .status = current_network_status, .disable_oled_lcd = true}),
                    pdMS_TO_TICKS(800) // max wait is around a centibeat
                );
        }

        // Send a command to the buzzer task to make a short sound of 25ms
        xQueueSendToBack(
            buzzer_command_queue,
            &(uint32_t){100},
            pdMS_TO_TICKS(800)
        );

        // Update the stepper motor
        switch (current_stepper_motor_mode) {
            // If the mode is normal operation, we want to update the position of the stepper motor to show the current time,
            case NORMAL_OPERATION:
                // Calculate the intervals between the current time and the time on the stepper motor in both directions (clockwise and anti-clockwise)
                clock_intervals = calculate_clock_intervals(time_on_stepper_motor, current_time);

                // Move the stepper motor in the direction of the shortest interval to the current time
                if (clock_intervals.clockwise_interval < clock_intervals.anti_clockwise_interval) {
                    // Instead of correcting big intervals in one big step, We move in steps of 2 per update (which looks continuous because it takes roughly half a centibeat to move a step)
                    // The reason why we do this is so that the stepper motor module isnt stuck executing one big step for a long time
                    // Also when when going in one big step anti-clockwise, because time moves forward, it will overshoot and have to correct itself clockwise again, wich looks very wonky
                    if (clock_intervals.clockwise_interval > 1) {
                        xQueueSendToBack(
                            stepper_motor_command_queue,
                            &((struct stepper_motor_command){.command = 36 * 2, .reverse = false}),
                            pdMS_TO_TICKS(800)
                        );

                        // Instead of corecting the time in one big step, we move the motor 2 steps in the closest direction every update
                        time_on_stepper_motor = (time_on_stepper_motor + 2 < 100000) ? time_on_stepper_motor + 2 : time_on_stepper_motor + 2 - 100000;

                    // When the interval is only 1 centibeat, we can just move one step, and then we will be at the correct time on the stepper motor
                    } else {
                        xQueueSendToBack(
                            stepper_motor_command_queue,
                            &((struct stepper_motor_command){.command = 36, .reverse = false}),
                            pdMS_TO_TICKS(800)
                        );

                        time_on_stepper_motor = current_time;
                    }

                // Same anti-clockwise
                } else if (clock_intervals.anti_clockwise_interval < clock_intervals.clockwise_interval) {
                    if (clock_intervals.anti_clockwise_interval > 1) {
                        xQueueSendToBack(
                            stepper_motor_command_queue,
                            &((struct stepper_motor_command){.command = 36 * 2, .reverse = true}),
                            pdMS_TO_TICKS(800)
                        );

                        time_on_stepper_motor = (time_on_stepper_motor >= 2) ? time_on_stepper_motor - 2 : 100000 - (2 - time_on_stepper_motor);

                    } else {
                        xQueueSendToBack(
                            stepper_motor_command_queue,
                            &((struct stepper_motor_command){.command = 36, .reverse = true}),
                            pdMS_TO_TICKS(800)
                        );

                        time_on_stepper_motor = current_time;
                    }
                }
                // if the intervals are the same, the time is already correct, so we do not move the stepper motor,

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
        if ((last_network_status != CONNECTED && last_network_status != UNDEFINED) && current_network_status == CONNECTED) {
            restart_sntp_synch();
        }

        last_network_status = current_network_status;
    }
}