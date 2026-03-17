#include "esp_err.h"

#define SERVO_0_GPIO 41
#define SERVO_1_GPIO 40

esp_err_t init_servo(void);
esp_err_t servo_set_angle(int servo_id, int angle);
