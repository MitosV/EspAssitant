#include "driver/ledc.h"
#include "esp_log.h"

#include "servo.h"

#define SERVO_FREQ_HZ      50
#define SERVO_RESOLUTION    LEDC_TIMER_14_BIT
#define SERVO_DUTY_MIN      410  // ~500us at 14-bit/50Hz (0 degrees)
#define SERVO_DUTY_MAX      2048 // ~2500us at 14-bit/50Hz (180 degrees)
#define SERVO_COUNT          2

static const char *TAG = "WILLOW/SERVO";

static const int servo_gpios[SERVO_COUNT] = {SERVO_0_GPIO, SERVO_1_GPIO};
static const int servo_channels[SERVO_COUNT] = {LEDC_CHANNEL_2, LEDC_CHANNEL_3};

static int angle_to_duty(int angle)
{
    if (angle < 0) {
        angle = 0;
    }
    if (angle > 180) {
        angle = 180;
    }
    return SERVO_DUTY_MIN + (angle * (SERVO_DUTY_MAX - SERVO_DUTY_MIN)) / 180;
}

esp_err_t init_servo(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "initializing servos");

    const ledc_timer_config_t cfg_timer = {
        .clk_cfg = LEDC_AUTO_CLK,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz = SERVO_FREQ_HZ,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = 2,
    };

    ret = ledc_timer_config(&cfg_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to config LEDC timer for servo: %s", esp_err_to_name(ret));
        return ret;
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        const ledc_channel_config_t cfg_channel = {
            .channel = servo_channels[i],
            .duty = angle_to_duty(90),
            .gpio_num = servo_gpios[i],
            .hpoint = 0,
            .intr_type = LEDC_INTR_DISABLE,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_sel = 2,
        };

        ret = ledc_channel_config(&cfg_channel);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to config LEDC channel for servo %d: %s", i, esp_err_to_name(ret));
            return ret;
        }

        ESP_LOGI(TAG, "servo %d initialized on GPIO %d", i, servo_gpios[i]);
    }

    return ESP_OK;
}

esp_err_t servo_set_angle(int servo_id, int angle)
{
    if (servo_id < 0 || servo_id >= SERVO_COUNT) {
        ESP_LOGE(TAG, "invalid servo ID: %d", servo_id);
        return ESP_ERR_INVALID_ARG;
    }

    int duty = angle_to_duty(angle);
    ESP_LOGI(TAG, "servo %d: angle=%d duty=%d", servo_id, angle, duty);
    return ledc_set_duty_and_update(LEDC_LOW_SPEED_MODE, servo_channels[servo_id], duty, 0);
}
