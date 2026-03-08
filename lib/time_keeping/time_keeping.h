#ifndef TIME_KEEPING_H
#define TIME_KEEPING_H

#include <esp_event.h>
#include <esp_netif.h>

typedef uint32_t beat_time_t; // time in centibeats (00000 - 99999)

struct clock_intervals {
    uint32_t anti_clockwise_interval; // Interval in centibeats to the left (anti-clockwise) of the current beat time
    uint32_t clockwise_interval; // Interval in centibeats to the right (clockwise) of the current beat time
};

struct sntp_synch_args {
    uint16_t synch_interval_ms; // Interval in milliseconds for how often to synchronize with the SNTP server, default is 15 minutes (900000 ms)
    char ntp_server_url[256]; // URL of the SNTP server to synchronize with
};

struct clock_intervals calculate_clock_intervals(beat_time_t time_1, beat_time_t time_2); // Util function to calculate the intervals on a clock between two beat times in both directions (clockwise and anti-clockwise)


void set_up_time_keeping(QueueHandle_t *queue); // Function to initialize time keeping, sets up the SNTP synchronization and the GPTimer for centibeat updates
void set_up_sntp_sync(struct sntp_synch_args* args); // Function to enable SNTP synchronization and set the callback function to be called when synchronization occurs
void restart_sntp_synch(struct sntp_synch_args* args); // Function to restart SNTP synchronization

void start_time_keeping(); // Function to start the time keeping by starting the GPTimer
void start_time_keeping_on_sync(); // Function to enable the flag that indicates to start the time keeping when the first SNTP synchronization occurs

#endif