#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "lwip/ip4_addr.h"
#include "oled.h"
#include "esp_netif_sntp.h"
#include "esp_sntp.h"
#include "esp_http_client.h"
#include "time_networkstatus_display.h"

// env variables for wifi credentials, set in the .env file
#define WIFI_SSID CONFIG_WIFI_SSID // WiFi SSID to connect to
#define WIFI_PASSWORD CONFIG_WIFI_PASSWORD // WiFi password

#define I2C_SDA_GPIO 1 // GPIO number for I2C SDA line
#define I2C_SCL_GPIO 2 // GPIO number for I2C SCL line
#define OLED_ADDR 0x3C // I2C address of the OLED display

#define WIFI_CONNECTED_BIT BIT0 // Bit in eventgroup to indicate WiFi connection status

static EventGroupHandle_t wifi_event_group; // Event group to signal when WiFi is connected
static QueueHandle_t time_networkstatus_queue; // Queue to hold time and network status updates

static esp_netif_t *wifi_netif; // Network interface handle for WiFi

// WiFi event handler
static void wifi_handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    // When WiFi starts, attempt to connect
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();

    // If disconnected, attempt to reconnect, and clear the connected bit
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();

    // When we get an IP, set the DNS server (hotspots have DNS assign issues, so we set it manualy to Google's public DNS) and set the connected bit
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        // esp_netif_dns_info_t dns_set = {0};
        // ip4_addr_t ip4 = {0};
        // ip4addr_aton("8.8.8.8", &ip4);
        // dns_set.ip.u_addr.ip4.addr = ip4.addr;
        // esp_netif_set_dns_info(wifi_netif, ESP_NETIF_DNS_MAIN, &dns_set);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Function to start WiFi connection
static void wifi_start(void) {
    // Initialize NVS, network interface, and event loop
    nvs_flash_init(); //nvs is used by wifi to store credentials and other data, nvs is used because it is non-volatile storage, so it can store data even when the device is powered off, witch is helpful for wifi credentials and other data that we want to persist
    esp_netif_init();
    esp_event_loop_create_default();
    wifi_netif = esp_netif_create_default_wifi_sta();

    // Create event group for WiFi connection status
    wifi_event_group = xEventGroupCreate();
    
    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Register event handlers for WiFi and IP events
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_handler, NULL);

    // Configure and start WiFi in station mode
    wifi_config_t wifi_config = {.sta = {.ssid = WIFI_SSID, .password = WIFI_PASSWORD}};
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_start();
}

// Callback function to be called when SNTP time synchronization occurs
void synch_callback() {
    time_t now;
    struct tm timeinfo;
    struct time_networkstatus_display display_data;

    // Set the network status to CONNECTED (since we are able to synchronize time, we can assume we are connected to the internet)
    display_data.status = CONNECTED;
    time(&now); // Get the current time in seconds since the epoch
    localtime_r(&now, &timeinfo); // Convert the time in seconds to broken-down time (year, month, day, etc.) in the local timezone

    uint32_t seconds = timeinfo.tm_sec + timeinfo.tm_min * 60 + timeinfo.tm_hour * 3600; // daytime in seconds since midnight
    display_data.beats = seconds / 86400.0f * 1000.0f; // Convert seconds to beats (1 day = 1000 beats = 86400 seconds)
    
    ESP_LOGI("SNTP", "The current beats is: %.2f", display_data.beats);

    // Send the updated time and network status to the queue for display
    xQueueSendToBack(
        time_networkstatus_queue, // Queue to send time and network status updates
        &display_data, // Data to send (current beats and network status)
        pdMS_TO_TICKS(100000) // Wait up to 100 second for space in the queue
    );
}

// Function to set up the time and network status display task
void set_up_time_networkstatus_display() {
    // Initalize queue to hold time and network status updates
    time_networkstatus_queue = xQueueCreate(10, sizeof(struct time_networkstatus_display));

    // Create a task to handle displaying time and network status
    xTaskCreate(
        task_time_networkstatus_display, // Task function
        "time_networkstatus_display", // Name
        4096, // Stack size
        (void *) time_networkstatus_queue, // arguments, we pass the queue handle to the task so it can receive updates
        1, // Priority
        NULL // Task handle (not used in this case)
    );
}

void app_main(void) {
    initialize_oled(OLED_ADDR, I2C_SDA_GPIO, I2C_SCL_GPIO);
    set_up_time_networkstatus_display();

    wifi_start();

    ESP_LOGI("WIFI", "Connecting to %s...", WIFI_SSID);

    // Wait for the WiFi connection to be established, with a timeout of 15 seconds
    if (!(xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, pdMS_TO_TICKS(15000)) & WIFI_CONNECTED_BIT)) {
        ESP_LOGI("WIFI", "Failed to connect to WiFi within the timeout period");
        return;
    }
    
    // Set up SNTP to synchronize time with an NTP server
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_init();

    setenv("TZ", "UTC-1", 1); // Set the timezone to UTC-1
    tzset(); // Apply the timezone setting

    sntp_set_time_sync_notification_cb(synch_callback); // Register the callback function to be called when time is synchronized
    sntp_set_sync_interval(10000); // Set the synchronization interval to 10 seconds
}

