#ifndef TIME_NETWORKSTATUS_DISPLAY_H
#define TIME_NETWORKSTATUS_DISPLAY_H

enum network_status {
    CONNECTED,
    NOT_CONNECTED,
    NOT_AVAILABLE,
    CONNECTING,
};

// Structure to hold time and network status information
struct time_networkstatus_display {
    float beats; // variable to hold the calculated beats
    enum network_status status; // variable to hold the network status
};

void task_time_networkstatus_display(void *args); // Task function to handle displaying time and network status on the OLED display

#endif