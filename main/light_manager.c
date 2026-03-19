#include "light_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>
#include "esp_err.h"

#define LIGHT_GPIO_PIN 41

static const char *TAG = "WILLOW/LIGHT";

static volatile bool current_state = false;

esp_err_t init_light_manager() {

    ESP_LOGI(TAG, "Light manager initialized");

    esp_err_t ret = ESP_OK;
    gpio_config_t io_conf = {
        .pin_bit_mask = 1ULL << LIGHT_GPIO_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ret = gpio_config(&io_conf);
    if (ret == ESP_OK) {
        gpio_set_level(LIGHT_GPIO_PIN, 0);
        current_state = false;
    }
    return ret;
}


void light_set_active(bool active) {
    ESP_LOGI(TAG, "Entering light_set_active");
    current_state = active;
    esp_err_t err = gpio_set_level(LIGHT_GPIO_PIN, active ? 1 : 0);
    ESP_LOGI(TAG, "gpio_set_level returned: %d", err);
    ESP_LOGI(TAG, "Light set to %s", active ? "active" : "inactive");
}


bool is_light_active() {
    int level = gpio_get_level(LIGHT_GPIO_PIN);
    return level == 1;
}
