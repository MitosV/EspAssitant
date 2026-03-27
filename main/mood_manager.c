#include <string.h>
#include "mood_manager.h"
#include "servo.h"
#include "esp_log.h"

static const char *TAG = "WILLOW/MOOD";


void update_mood(char *mood){
    ESP_LOGI(TAG, "UPDATE MOOD");
    if (strcmp(mood, "HAPPY") == 0) {
        servo_set_angle(0);
        ESP_LOGI(TAG, "Mood updated to HAPPY");
    } else if (strcmp(mood, "ANGRY") == 0) {
        servo_set_angle(0);
        ESP_LOGI(TAG, "Mood updated to ANGRY");
    } else if (strcmp(mood, "NORMAL") == 0) {
        ESP_LOGI(TAG, "Mood updated to NORMAL");
        servo_set_angle(0);
    }
}
