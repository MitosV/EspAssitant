#include "esp_err.h"

#define SERVO_GPIO 38

esp_err_t init_servo(void);
esp_err_t servo_set_angle(int angle);
