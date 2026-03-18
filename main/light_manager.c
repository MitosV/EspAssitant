#include "light_manager.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>
#include "esp_err.h"

#define LIGHT_GPIO_PIN 47 //GPIO_NUM_47

esp_err_t init_light_manager() {
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
    }
    return ret;
}


void light_set_active(bool active) {
    gpio_set_level(LIGHT_GPIO_PIN, active ? 1 : 0);
}


bool is_light_active() {
    return gpio_get_level(LIGHT_GPIO_PIN) == 1;
}
