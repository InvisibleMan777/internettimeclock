#ifndef TIME_KEEPING_H
#define TIME_KEEPING_H

#include "esp_event.h"

void set_up_time_keeping(QueueHandle_t *queue); // Function to initialize time keeping, sets up the SNTP synchronization and the GPTimer for centibeat updates
void enable_sntp_sync(); // Function to enable SNTP synchronization and set the callback function to be called when synchronization occurs
void restart_sntp_synch(); // Function to restart SNTP synchronization

void start_time_keeping(); // Function to start the time keeping by starting the GPTimer
void stop_time_keeping(); // Function to stop the time keeping by stopping the GPTimer
void start_time_keeping_on_sync(); // Function to enable the flag that indicates to start the time keeping when the first SNTP synchronization occurs

typedef uint32_t beat_time_t; // time in centibeats (00000 - 99999)

struct time_intervals {
    uint32_t anti_clockwise_interval; // Interval in centibeats to the left (anti-clockwise) of the current beat time
    uint32_t clockwise_interval; // Interval in centibeats to the right (clockwise) of the current beat time
};

struct time_intervals calculate_clock_intervals(beat_time_t time_1, beat_time_t time_2); // Util function to calculate the intervals on a clock between two beat times in both directions (clockwise and anti-clockwise)

#endif