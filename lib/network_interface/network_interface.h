#ifndef NETWORK_INTERFACE_H
#define NETWORK_INTERFACE_H

void wifi_start(char* ssid, char* password); // Function to start WiFi connection with given SSID and password

bool wait_on_connection(); // Wait for the WiFi connection to be established, with a timeout of 15 seconds

#endif