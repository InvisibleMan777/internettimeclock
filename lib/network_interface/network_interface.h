#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

void wifi_start(char* ssid, char* password, QueueHandle_t* networkstatus_message_box); // Function to start WiFi connection with given SSID and password

bool wait_on_connection(); // Wait for the WiFi connection to be established, with a timeout of 15 seconds

// enum to represent the network status, used for communication between the WiFi connection task and the time/network status display task
enum network_status {
    CONNECTED,
    NOT_CONNECTED,
    NOT_AVAILABLE,
    ERROR,
    UNDEFINED,
};

#endif