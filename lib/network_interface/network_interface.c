#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <nvs_flash.h>
#include <esp_netif.h>
#include <esp_log.h>
#include <lwip/ip4_addr.h>
#include "network_interface.h"

#define WIFI_CONNECTED_BIT BIT0 // Bit in eventgroup to indicate WiFi connection status

EventGroupHandle_t wifi_event_group; // Event group to signal when WiFi is connected
esp_netif_t *wifi_netif; // Network interface handle for WiFi
QueueHandle_t *message_box; // Queue to hold network status for communication with the time/network status display task

// WiFi event handler
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    // When WiFi starts, attempt to connect
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    // If disconnected, attempt to reconnect, and clear the connected bit
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();

        // Set the network status based on the disconnection reason, so that we can display it on the OLED
        switch (((wifi_event_sta_disconnected_t*) data)->reason) {
            case WIFI_REASON_AUTH_FAIL:
                xQueueOverwrite(*message_box, &(enum network_status){NOT_CONNECTED});
                break;
            case WIFI_REASON_NO_AP_FOUND:
                xQueueOverwrite(*message_box, &(enum network_status){NOT_AVAILABLE});
                break;
            default:
                xQueueOverwrite(*message_box, &(enum network_status){ERROR});
                break;
        }
    // When we get an IP, set the DNS server (hotspots have DNS assign issues, so we set it manualy to Google's public DNS) and set the connected bit
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        esp_netif_dns_info_t dns_set = {0};
        ip4_addr_t ip4 = {0};
        ip4addr_aton("8.8.8.8", &ip4);
        dns_set.ip.u_addr.ip4.addr = ip4.addr;
        esp_netif_set_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &dns_set);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        xQueueOverwrite(*message_box, &(enum network_status){CONNECTED});
    }
}

// Function to start WiFi connection
void wifi_start(char* ssid, char* password, QueueHandle_t* networkstatus_message_box) {
    // Initalize a queue to hold the network status, which the time/network status display task can read to display the current network status on the OLED
    *networkstatus_message_box = xQueueCreate(1, sizeof(enum network_status));
    message_box = networkstatus_message_box;

    char ssid_buf[32];
    char password_buf[64];

    // Copy the SSID and password into the buffers
    strncpy((char*) ssid_buf, ssid, sizeof(ssid_buf) - 1);
    ssid_buf[sizeof(ssid_buf) - 1] = '\0';
    strncpy((char*) password_buf, password, sizeof(password_buf) - 1);
    password_buf[sizeof(password_buf) - 1] = '\0';

    // Initialize NVS, network interface, and event loop
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_netif = esp_netif_create_default_wifi_sta();

    // Create the event group to handle WiFi connection events
    wifi_event_group = xEventGroupCreate();
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Register event handlers for WiFi and IP events
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL);

    // Configure and start WiFi in station mode
    wifi_config_t wifi_config = {0};
    memcpy(wifi_config.sta.ssid, ssid_buf, sizeof(ssid_buf));
    memcpy(wifi_config.sta.password, password_buf, sizeof(password_buf));
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// Wait for the WiFi connection to be established, with a timeout of 15 seconds
bool wait_on_connection() {
    if (!(xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000)) & WIFI_CONNECTED_BIT)) {
        return false;
    }

    return true;
}