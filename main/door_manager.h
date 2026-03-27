#pragma once

#include "esp_err.h"

typedef enum {
    DOOR_CLOSED,
    DOOR_OPEN,
} door_state_t;

esp_err_t door_manager_init(void);
esp_err_t door_open(void);
esp_err_t door_open_rest(void);
esp_err_t door_close(void);
door_state_t door_get_state(void);
