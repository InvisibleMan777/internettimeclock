#include <esp_event.h>
#include <esp_sntp.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <driver/gptimer.h>

#include "time_networkstatus_display.h"
#include "time_keeping.h"

beat_time_t beat_time = 0; // Time in amounth of centibeats
static gptimer_handle_t centibeat_timer; // GPTimer handle for centibeat updates
bool first_synch_completed = false; // Flag to indicate if the first SNTP synchronization has completed
QueueHandle_t *queue; // Queue to send time updates to the main controller

// Callback function to be called when SNTP time synchronization occurs
void synch_callback() {
    // Get the current time in seconds since the epoch
    time_t now;
    time(&now);

    // Convert to time of day (hours, minutes, seconds)
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    // Get seconds since midnight
    uint32_t seconds = timeinfo.tm_sec + timeinfo.tm_min * 60 + timeinfo.tm_hour * 3600;

    // Convert to centibeats (1 centibeat = 0.00864 seconds)
    beat_time_t buffer_beat_time = seconds / 0.864f; 

    // Sometimes we get a bullshit response when sychronizing, so we just ignore that
    if (buffer_beat_time >= 100000) {
        return;
    }

    beat_time = buffer_beat_time;

    // Start the clock on the first synchronization
    if (first_synch_completed == false) {
        gptimer_start(centibeat_timer);
        first_synch_completed = true;
    }
}

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
        *queue,
        &beat_time, 
        &high_task_wakeup 
    );

    // If sending to the queue woke up a higher priority task, yield to it
    return (high_task_wakeup == pdTRUE);
}

// Function to initialize the GPTimer for centibeat updates and set up SNTP synchronization
static void init_time_counter() {
    //initalize gptimer that ticks every 0.1 ms
    gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = 10000, // 10 kHz resolution (1 tick = 0.1 ms)
    };

    gptimer_new_timer(&timer_config, &centibeat_timer);

    // Configure the timer to trigger an alarm every 8640 ticks (864 ms / 1 centibeat)
    gptimer_alarm_config_t alarm_config = {
        .alarm_count = 8640,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };

    gptimer_set_alarm_action(centibeat_timer, &alarm_config);

    // increase time every centibeat
    gptimer_event_callbacks_t cbs = {
        .on_alarm = isr_increase_time, 
    };

    gptimer_register_event_callbacks(centibeat_timer, &cbs, NULL);
    gptimer_enable(centibeat_timer);
}

// Function to set up time keeping, sets up the SNTP synchronization and the GPTimer for centibeat updates
void set_up_time_keeping(QueueHandle_t *time_update_queue) {
    // Initalize a queue to send calculated times to
    *time_update_queue = xQueueCreate(10, sizeof(beat_time_t));
    queue = time_update_queue;

    // Initialize the timer to start updating the time (beats) every centibeat
    init_time_counter();

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