#include <esp_event.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <esp_netif.h>
#include <esp_timer.h>
#include <esp_timer.h>
#include <esp_log.h>
#include <driver/gptimer.h>
#include <math.h>

#include "time_networkstatus_display.h"
#include "time_keeping.h"

#define FRACTIONAL_TO_MILLISECONDS(fractional) ((fractional) * 1000 / (1ULL << 32)) // Convert the fractional part of an NTP timestamp to milliseconds
#define NTP_EPOCH_OFFSET_MS 2208988800000ULL // Number of milliseconds between NTP epoch (1900) and Unix epoch (1970)

#define SYNCH_ENABLED_BIT BIT0 // Bit in event group to indicate SNTP synchronization is enabled
#define START_TIME_KEEPING_ON_SYNCH_BIT BIT1 // Bit in event group to indicate to start the time keeping when the first SNTP synchronization occurs
#define SYNCH_STARTED_BIT BIT2 // Bit in event group to indicate that the first SNTP synchronization has occurred

beat_time_t beat_time = 0; // Time in amounth of centibeats
gptimer_handle_t beat_timer_handle; // GPTimer handle for centibeat updates

EventGroupHandle_t sntp_synch_event_group; // Event group for sntp synchronization

struct sockaddr_in socket_addres; // Socket address info for SNTP synchronization

// Structure of ntp header, used for sending commands to and parsing the response from the SNTP server to get the current time
struct ntp_packet {
    uint8_t li_vn_mode;      // Leap Indicator (2 bits), Version Number (3 bits), Mode (3 bits)
    uint8_t stratum;         // Stratum level of the local clock
    uint8_t poll;            // Polling interval
    uint8_t precision;       // Precision of the local clock
    uint32_t root_delay;     // Total round-trip delay to the primary reference source
    uint32_t root_dispersion;// Maximum error relative to the primary reference source
    uint32_t ref_id;         // Reference ID (identifier of the primary reference source)
    uint32_t ref_timestamp_sec;  // Reference timestamp seconds
    uint32_t ref_timestamp_frac; // Reference timestamp fraction
    uint32_t orig_timestamp_sec;  // Originate timestamp seconds
    uint32_t orig_timestamp_frac; // Originate timestamp fraction
    uint32_t recv_timestamp_sec;  // Receive timestamp seconds
    uint32_t recv_timestamp_frac; // Receive timestamp fraction
    uint32_t trans_timestamp_sec;  // Transmit timestamp seconds
    uint32_t trans_timestamp_frac; // Transmit timestamp fraction  
};

// ISR to be called on each GPTimer alarm event, which occurs every centibeat (864 ms)
bool isr_increase_time(struct gptimer_t *timer, const gptimer_alarm_event_data_t *edata, void *user_data) {
    // Increase by 1 centibeat every callback (864 ms)
    beat_time++;

    // Reset time after reaching 100000 beats
    if (beat_time >= 100000) {
        beat_time = 0;
    }

    // This variable is used to indicate whether a higher priority task was woken up by sending data to the queue, which would require a context switch at the end of the ISR
    BaseType_t high_task_wakeup = pdFALSE;

    // Send the updated time to main controller
    xQueueSendToBackFromISR(
        *(QueueHandle_t *) user_data, // Time update queue
        &beat_time, 
        &high_task_wakeup 
    );

    // If sending to the queue woke up a higher priority task, yield to it
    return (high_task_wakeup == pdTRUE);
}

// Task function to handle SNTP synchronization
void task_sntp_synch(void* args) {
    int socket_file_descripter; // Socket file descriptor for SNTP synchronization
    int err; // Variable to hold error codes from socket operations
    uint16_t synch_interval_ms = ((struct sntp_synch_args*) args)->synch_interval_ms; // Synchronization interval in milliseconds

    // Timeout for receiving data on the socket, so that if synchronization fails we don't wait forever
    struct timeval timeout;
    timeout.tv_sec = 200;
    timeout.tv_usec = 0;

    // ntp packet to send to the SNTP server to request the current time, we only need to set the li_vn_mode field to indicate that this is a client request, the rest of the fields can be set to 0
    struct ntp_packet request_ntp_packet = {
        .li_vn_mode = (0 << 6) | (4 << 3) | 3, // LI = 0 (no warning), VN = 4 (version number), Mode = 3 (client)
        .stratum = 0,
        .poll = 0,
        .precision = 0,
        .root_delay = 0,
        .root_dispersion = 0,
        .ref_id = 0,
        .ref_timestamp_sec = 0,
        .ref_timestamp_frac = 0,
        .orig_timestamp_sec = 0,
        .orig_timestamp_frac = 0,
        .recv_timestamp_sec = 0,
        .recv_timestamp_frac = 0,
        .trans_timestamp_sec = 0,
        .trans_timestamp_frac = 0
    };

    // Serialize the NTP packet structure
    uint8_t request_buffer[48] = {0};
    memcpy(request_buffer, &request_ntp_packet, sizeof(request_ntp_packet));

    // Initialize a buffer to hold the response from the SNTP server, and a structure to deserialize the response into
    uint8_t response_buffer[48] = {0};
    struct ntp_packet response_ntp_packet;

    uint64_t client_transmit_time; // Variable to hold the time when the NTP request is sent, in milliseconds
    uint64_t server_receive_time; // Variable to hold the time when the NTP request is received by the server, in milliseconds
    uint64_t server_transmit_time; // Variable to hold the time when the NTP response is sent by the server, in milliseconds
    uint64_t client_receive_time; // Variable to hold the time when the NTP response is received, in milliseconds

    int64_t time_to_synchronize_to; // Variable to hold the calculated current time based on the SNTP response and the round-trip time, this is the time that we will synchronize our clock to

    while (1) {
        // Create the socket
        socket_file_descripter = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

        // End task and clear flag if socket creation fails
        if (socket_file_descripter == -1) {
            xEventGroupClearBits(sntp_synch_event_group, SYNCH_ENABLED_BIT);
            vTaskDelete(NULL);
        }

        // Set timeout for waiting on response
        setsockopt(socket_file_descripter, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

        // Connect with server
        err = connect(socket_file_descripter, (struct sockaddr*)&socket_addres, sizeof(socket_addres));

        // If connection fails, close the socket and try again
        if (err < 0) {
            close(socket_file_descripter);
            continue;
        }
        
        // Record the time when the request is sent in microseconds
        client_transmit_time = esp_timer_get_time() / 1000.0f;

        // Send the NTP request packet to the server
        err = send(socket_file_descripter, request_buffer, sizeof(request_buffer), 0);

        // If sending fails, close the socket and try again
        if (err < 0) {
            close(socket_file_descripter);
            continue;
        }

        // Wait for the response from the server
        err = recv(socket_file_descripter, response_buffer, sizeof(response_buffer), 0);

        // Record the time when the response is received in microseconds
        client_receive_time = esp_timer_get_time() / 1000.0f;

        // If receiving fails, close the socket and try again
        if (err < 0) {
            close(socket_file_descripter);
            continue;
        }

        // Close the socket after receiving the response
        close(socket_file_descripter); 

        // Convert the buffer from network byte order to host byte order
        for (size_t offset = 4; offset < sizeof(response_buffer); offset += sizeof(uint32_t)) {
            uint32_t value;
            memcpy(&value, &response_buffer[offset], sizeof(value));
            value = ntohl(value);
            memcpy(&response_buffer[offset], &value, sizeof(value));
        }

        // Deserialize the response into a ntp header structure
        memcpy(&response_ntp_packet, response_buffer, sizeof(response_ntp_packet));

        // Calculate the server receive and transmit times in milliseconds by combining the seconds and fractional seconds from the NTP response
        // We also have to account for the ntp epoch, which starts in 1900, while the Unix epoch starts in 1970, by subtracting 70 years worth of seconds from the server times (2 208 988 800 seconds / 2208988800000 milliseconds) 
        server_receive_time = ((uint64_t) response_ntp_packet.recv_timestamp_sec) * 1000 + FRACTIONAL_TO_MILLISECONDS(response_ntp_packet.recv_timestamp_frac) - NTP_EPOCH_OFFSET_MS;
        server_transmit_time = ((uint64_t) response_ntp_packet.trans_timestamp_sec) * 1000 + FRACTIONAL_TO_MILLISECONDS(response_ntp_packet.trans_timestamp_frac) - NTP_EPOCH_OFFSET_MS;

        // Calculate the current time (t4 + ((t2 - t1) + (t3 - t4)) / 2)
        // Round to the nearest second so we can use localtime_r to convert to time of day, which is needed to convert to beat time
        // Round has to be done with double precision to avoid overflow issues with the large millisecond timestamps, which would cause incorrect rounding when using single precision floating point numbers
        time_to_synchronize_to = (int64_t) llround((client_receive_time + ((server_receive_time - client_transmit_time) + (server_transmit_time - client_receive_time)) / 2.0) / 1000.0);

        // Convert to time of day (hours, minutes, seconds)
        struct tm timeinfo;
        localtime_r(&time_to_synchronize_to, &timeinfo);

        // Get seconds since midnight
        uint32_t seconds = timeinfo.tm_sec + timeinfo.tm_min * 60 + timeinfo.tm_hour * 3600;

        // Convert to centibeats (1 centibeat = 0.00864 seconds) and set the global beat_time variable
        beat_time = seconds / 0.864f; 

        // Start the time keeping if the flag to start on synchronization is set and synchronization has not yet started
        if (xEventGroupGetBits(sntp_synch_event_group) & START_TIME_KEEPING_ON_SYNCH_BIT && !(xEventGroupGetBits(sntp_synch_event_group) & SYNCH_STARTED_BIT)) {
            start_time_keeping();
            xEventGroupSetBits(sntp_synch_event_group, SYNCH_STARTED_BIT);
        }

        // Wait for the specified interval before the next synchronization, as recommended by SNTP to avoid excessive network traffic
        vTaskDelay(pdMS_TO_TICKS(synch_interval_ms));
    }
}

// Function to set up time keeping, sets up the SNTP synchronization and the GPTimer for centibeat updates
void set_up_time_keeping(QueueHandle_t *time_update_queue) {
    // Initalize a queue to send calculated times to
    *time_update_queue = xQueueCreate(99, sizeof(beat_time_t));

    //initalize gptimer that ticks every 0.1 ms
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 10000, // 10 kHz resolution (1 tick = 0.1 ms)
    };
    gptimer_new_timer(&timer_config, &beat_timer_handle);

    // Configure the timer to trigger an alarm every 8640 ticks (864 ms / 1 centibeat)
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 8640,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    gptimer_set_alarm_action(beat_timer_handle, &alarm_config);

    // increase time every centibeat
    gptimer_event_callbacks_t cbs = {
        .on_alarm = isr_increase_time, 
    };
    gptimer_register_event_callbacks(beat_timer_handle, &cbs, time_update_queue);

    // Enable the timer, but it will only start counting when start_time_keeping() is called or after the first SNTP synchronization if start_time_keeping_on_sync() is called
    gptimer_enable(beat_timer_handle);
}

// Function to set up and enable SNTP synchronization
void set_up_sntp_sync(struct sntp_synch_args* args) {
    // Set up the socket address for the SNTP server (pool.ntp.org)
    struct hostent *ntp_server = gethostbyname(args->ntp_server_url); //get domain info including ip address
    char ntp_server_ip_str[16];
    memccpy(ntp_server_ip_str, inet_ntoa(*(struct in_addr*)ntp_server->h_addr_list[0]), 0, sizeof(ntp_server_ip_str)); // Put the IP address in a string

    // Setup socket address structure for SNTP server
    inet_pton(AF_INET, ntp_server_ip_str, &socket_addres.sin_addr);
    socket_addres.sin_family = AF_INET;
    socket_addres.sin_port = htons(123);

    // Set the timezone settings to UTC-1 (used for converting the synchronized time to beat time, which is based on UTC-1)
    // localtime_r will use this timezone setting to convert the fetched seconds since the Unix epoch to the correct time of day in UTC-1, which is needed to calculate the correct beat time
    setenv("TZ", "UTC-1", 1); // Set the timezone to UTC-1
    tzset(); // Apply the timezone setting

    // Create event group for SNTP synchronization and set the flag to indicate that synchronization is enabled
    sntp_synch_event_group = xEventGroupCreate();

    xEventGroupSetBits(sntp_synch_event_group, SYNCH_ENABLED_BIT);

    // Start the SNTP synchronization task
    xTaskCreate(
        task_sntp_synch, 
        "sntp_synch", 
        4096, 
        args, 
        1, // Lowest priority
        NULL
    ); 
}

// Function to restart SNTP synchronization
void restart_sntp_synch(struct sntp_synch_args* args) {
    if (!(xEventGroupGetBits(sntp_synch_event_group) & SYNCH_ENABLED_BIT)) {
        // Start the SNTP synchronization task
        xTaskCreate(
            task_sntp_synch, 
            "sntp_synch", 
            4096, 
            args, 
            1, // Lowest priority
            NULL
        ); 
        xEventGroupSetBits(sntp_synch_event_group, SYNCH_ENABLED_BIT);
    }
}

// Function to start the time keeping by starting the GPTimer
void start_time_keeping() {
    gptimer_start(beat_timer_handle);
}

//enables flag that indicates to start the time keeping when the first SNTP synchronization occurs
void start_time_keeping_on_sync() {
    xEventGroupSetBits(sntp_synch_event_group, START_TIME_KEEPING_ON_SYNCH_BIT);
}

//util function to calculate the intervals between two beat times on a clock in both directions (clockwise and anti-clockwise)
struct clock_intervals calculate_clock_intervals(beat_time_t time_1, beat_time_t time_2) {
    // we modulo the results with 100 so that we get the intervals looking only at the centibeats
    uint32_t interval_anti_clockwise = (time_1 > time_2 ? time_1 - time_2 : 100000 - time_2 + time_1) % 100;
    uint32_t interval_clockwise = (time_1 > time_2 ? 100000 - time_1 + time_2: time_2 - time_1) % 100;

    return (struct clock_intervals){.anti_clockwise_interval = interval_anti_clockwise, .clockwise_interval = interval_clockwise};
}