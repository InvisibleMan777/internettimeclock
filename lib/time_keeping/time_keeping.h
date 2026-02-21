#ifndef TIME_KEEPING_H
#define TIME_KEEPING_H

#include "esp_event.h"

void set_up_time_keeping(QueueHandle_t *queue); // Function to initialize time keeping, sets up the SNTP synchronization and the GPTimer for centibeat updates

typedef uint32_t beat_time_t; // time in centibeats (00000 - 99999)

#endif