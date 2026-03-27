#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"

#include "door_manager.h"
#include "servo.h"

#define TRIG_GPIO            40
#define ECHO_GPIO            39
#define SERVO_ANGLE_OPEN     105
#define SERVO_ANGLE_CLOSED   180
#define DETECT_THRESHOLD_CM  20  // solo detecta a 12cm o menos
#define CLOSE_DELAY_MS       1000  // Tiempo sin detección antes de cerrar (ms)
#define SENSOR_POLL_MS       200   // Intervalo de muestreo del sensor (ms)
#define ECHO_TIMEOUT_US      30000 // Timeout del echo (30ms ≈ ~5m)

#define DOOR_REST_HOLD_MS    8000  // Tiempo abierta cuando se abre por REST (ms)

static const char *TAG = "WILLOW/DOOR";

static door_state_t door_state = DOOR_CLOSED;
static int64_t rest_open_until_us = 0;

/* Devuelve distancia en cm, o -1.0 si hay timeout */
static float ultrasonic_measure_cm(void)
{
    /* Pulso de disparo de 10µs */
    gpio_set_level(TRIG_GPIO, 0);
    ets_delay_us(2);
    gpio_set_level(TRIG_GPIO, 1);
    ets_delay_us(10);
    gpio_set_level(TRIG_GPIO, 0);

    /* Esperar flanco de subida del echo */
    int64_t t = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 0) {
        if (esp_timer_get_time() - t > ECHO_TIMEOUT_US) {
            return -1.0f;
        }
    }

    /* Medir duración del pulso alto */
    int64_t echo_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO) == 1) {
        if (esp_timer_get_time() - echo_start > ECHO_TIMEOUT_US) {
            return -1.0f;
        }
    }
    int64_t echo_end = esp_timer_get_time();

    /* distance (cm) = duration_us * 0.01715  (velocidad sonido / 2) */
    return (float)(echo_end - echo_start) * 0.01715f;
}

esp_err_t door_open(void)
{
    ESP_LOGI(TAG, "opening door");
    esp_err_t ret = servo_set_angle(SERVO_ANGLE_OPEN);
    if (ret == ESP_OK) {
        door_state = DOOR_OPEN;
    }
    return ret;
}

esp_err_t door_open_rest(void)
{
    ESP_LOGI(TAG, "opening door via REST (hold %d ms)", DOOR_REST_HOLD_MS);
    rest_open_until_us = esp_timer_get_time() + ((int64_t)DOOR_REST_HOLD_MS * 1000LL);
    return door_open();
}

esp_err_t door_close(void)
{
    ESP_LOGI(TAG, "closing door");
    esp_err_t ret = servo_set_angle(SERVO_ANGLE_CLOSED);
    if (ret == ESP_OK) {
        door_state = DOOR_CLOSED;
    }
    return ret;
}

door_state_t door_get_state(void)
{
    return door_state;
}

static void door_sensor_task(void *arg)
{
    TickType_t last_detected_tick = 0;

    while (true) {
        float dist = ultrasonic_measure_cm();
        bool detected = (dist > 0.0f && dist < DETECT_THRESHOLD_CM);
        const char *state_str = (door_state == DOOR_OPEN) ? "ABIERTA" : "CERRADA";

        if (dist < 0.0f) {
            ESP_LOGW(TAG, "[puerta=%s] sensor: TIMEOUT (sin eco)", state_str);
        } else {
            ESP_LOGI(TAG, "[puerta=%s] distancia=%.1f cm  deteccion=%s",
                     state_str, dist, detected ? "SI" : "no");
        }

        if (detected) {
            last_detected_tick = xTaskGetTickCount();
            if (door_state == DOOR_CLOSED) {
                ESP_LOGI(TAG, "objeto detectado a %.1f cm — abriendo puerta", dist);
                door_open();
            }
        } else if (door_state == DOOR_OPEN) {
            int64_t now_us = esp_timer_get_time();
            if (now_us < rest_open_until_us) {
                int64_t remaining_ms = (rest_open_until_us - now_us) / 1000;
                ESP_LOGI(TAG, "[puerta=ABIERTA] REST hold activo — cierra en %lld ms", remaining_ms);
            } else {
                uint32_t elapsed_ms = (xTaskGetTickCount() - last_detected_tick) * portTICK_PERIOD_MS;
                uint32_t remaining_ms = (elapsed_ms < CLOSE_DELAY_MS) ? (CLOSE_DELAY_MS - elapsed_ms) : 0;
                ESP_LOGI(TAG, "[puerta=ABIERTA] sin deteccion hace %lu ms — cierra en %lu ms", elapsed_ms, remaining_ms);
                if (elapsed_ms >= CLOSE_DELAY_MS) {
                    ESP_LOGI(TAG, "tiempo agotado — cerrando puerta");
                    door_close();
                    last_detected_tick = 0;
                }
            }
        }

        vTaskDelay(SENSOR_POLL_MS / portTICK_PERIOD_MS);
    }
}

esp_err_t door_manager_init(void)
{
    esp_err_t ret;

    gpio_config_t trig_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << TRIG_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&trig_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to config TRIG GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    gpio_config_t echo_cfg = {
        .intr_type    = GPIO_INTR_DISABLE,
        .mode         = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << ECHO_GPIO),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
    };
    ret = gpio_config(&echo_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to config ECHO GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Asegurar estado inicial cerrado */
    ret = door_close();
    if (ret != ESP_OK) {
        return ret;
    }

    xTaskCreate(door_sensor_task, "door_sensor", 2 * 1024, NULL, 5, NULL);

    ESP_LOGI(TAG, "door manager initialized (TRIG=%d, ECHO=%d, threshold=%dcm)", TRIG_GPIO, ECHO_GPIO, DETECT_THRESHOLD_CM);
    return ESP_OK;
}
