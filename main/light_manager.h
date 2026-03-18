#pragma once

#include <stdbool.h>
#include "esp_err.h"

esp_err_t init_light_manager();
void light_set_active(bool active);
bool is_light_active();
