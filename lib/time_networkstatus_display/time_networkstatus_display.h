#ifndef TIME_NETWORKSTATUS_DISPLAY_H
#define TIME_NETWORKSTATUS_DISPLAY_H

#include <stdint.h>
#include <driver/gpio.h>
#include "time_keeping.h"
#include "network_interface.h"
#include "power_mode_control.h"

struct time_networkstatus_display_args {
    QueueHandle_t* time_networkstatus_display_queue; 
    gpio_num_t clock_cycle_led; 
    gpio_num_t networkstatus_led;
};

// Structure to hold time and network status information
struct time_networkstatus_display_data {
    beat_time_t beat_time; // variable to hold beats * 100
    enum network_status status; // variable to hold the network status
    bool disable_oled_lcd; // variable to indicate whether to disable the OLED LCD display
};

void task_time_networkstatus_display(void *args); // Task function to handle displaying time and network status on the OLED display

void set_up_time_networkstatus_display(struct time_networkstatus_display_args* time_networkstatus_display_args); // Function to set up the time and network status display task, also initializes the queue for time and network status updates

#endif