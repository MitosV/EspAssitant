#include "cJSON.h"
#include "audio_hal.h"
#include "audio_thread.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include "audio.h"
#include "config.h"
#include "http.h"
#include "shared.h"
#include "slvgl.h"
#include "timer.h"
#include "mood_manager.h"
#include "light_manager.h"

#define DEFAULT_AUTH_HEADER        ""
#define DEFAULT_AUTH_PASS          ""
#define DEFAULT_AUTH_TYPE          "None"
#define DEFAULT_AUTH_USER          ""
#define DEFAULT_URL                "http://your_rest_url"
#define DEFAULT_WEBHOOK_INTERVAL_S 10

static const char *TAG = "WILLOW/REST";

void rest_send(const char *data)
{
    bool ok = false;
    char *auth_type = NULL, *body = NULL, *pass = NULL, *url = NULL, *user = NULL;
    esp_err_t ret;
    int http_status;

    esp_http_client_handle_t hdl_hc = init_http_client();

    auth_type = config_get_char("rest_auth_type", DEFAULT_AUTH_TYPE);
    if (strcmp(auth_type, "Header") == 0) {
        char *auth_header = config_get_char("rest_auth_header", DEFAULT_AUTH_HEADER);
        ret = esp_http_client_set_header(hdl_hc, "Authorization", auth_header);
        free(auth_header);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to set authorization header: %s", esp_err_to_name(ret));
        }
    } else if (strcmp(auth_type, "Basic") == 0) {
        pass = config_get_char("rest_auth_pass", DEFAULT_AUTH_PASS);
        user = config_get_char("rest_auth_user", DEFAULT_AUTH_USER);
        ret = http_set_basic_auth(hdl_hc, user, pass);
        free(pass);
        free(user);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "failed to enable HTTP Basic Authentication: %s", esp_err_to_name(ret));
        }
    }
    free(auth_type);

    url = config_get_char("rest_url", DEFAULT_URL);
    ret = http_post(hdl_hc, url, "application/json", data, &body, &http_status);
    ESP_LOGI(TAG, "SEND REST TO %s", url);
    free(url);

    if (ret == ESP_OK) {
        if (http_status >= 200 && http_status <= 299) {
            ok = true;
        }
    } else {
        ESP_LOGE(TAG, "failed to read HTTP POST response");
    }


    if (ok) {

        if (body != NULL) {
            ESP_LOGI(TAG, "RAW BODY: %s", body);
            cJSON *json = cJSON_Parse(body);

            cJSON *text = cJSON_GetObjectItemCaseSensitive(json, "text");
            cJSON *light = cJSON_GetObjectItemCaseSensitive(json, "light");
            if (cJSON_IsString(text) && text->valuestring != NULL && strlen(text->valuestring) > 1) {
                ESP_LOGI(TAG, "REST response: %s", text->valuestring);
                war.fn_ok(text->valuestring);
            }
            if (cJSON_IsBool(light)) {
                bool state = cJSON_IsTrue(light);
                ESP_LOGI(TAG, "REST light state (bool): %s", state ? "true" : "false");
                light_set_active(state);
            } else if (cJSON_IsString(light) && light->valuestring != NULL && strlen(text->valuestring) > 1) {
                bool state = (strcmp(light->valuestring, "true") == 0);
                ESP_LOGI(TAG, "REST light state (string): %s", state ? "true" : "false");
                light_set_active(state);
            }
            cJSON_Delete(json);
        } else {
            ESP_LOGI(TAG, "REST successful");
            war.fn_ok("Success");
        }
    } else {
        ESP_LOGI(TAG, "REST failed");
        war.fn_err("Error");
    }

    if (lvgl_port_lock(lvgl_lock_timeout)) {
        lv_obj_clear_flag(lbl_ln4, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(lbl_ln5, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_align(lbl_ln4, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_set_style_text_align(lbl_ln5, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_remove_event_cb(lbl_ln4, cb_btn_cancel);
        if (body != NULL) {
            cJSON *json = cJSON_Parse(body);

            cJSON *text = cJSON_GetObjectItemCaseSensitive(json, "text");
            if (cJSON_IsString(text) && text->valuestring != NULL && strlen(text->valuestring) > 1) {
                lv_label_set_text_static(lbl_ln4, "Respuesta:");
                lv_label_set_text(lbl_ln5, text->valuestring);
            }
            cJSON_Delete(json);
        } else {
            lv_label_set_text_static(lbl_ln4, "Command status:");
            lv_label_set_text(lbl_ln5, ok ? "Success" : "Error");
        }
        lvgl_port_unlock();
    }

    reset_timer(hdl_display_timer, config_get_int("display_timeout", DEFAULT_DISPLAY_TIMEOUT), false);

    free(body);
}

static void rest_webhook_task(void *data)
{
    char *body = NULL;
    char *url = NULL;
    esp_err_t ret;
    int http_status;
    int interval_s = config_get_int("rest_webhook_interval", DEFAULT_WEBHOOK_INTERVAL_S);

    while (true) {
        vTaskDelay((interval_s * 1000) / portTICK_PERIOD_MS);

        esp_http_client_handle_t hdl_hc = init_http_client();

        char *auth_type = config_get_char("rest_auth_type", DEFAULT_AUTH_TYPE);
        if (strcmp(auth_type, "Header") == 0) {
            char *auth_header = config_get_char("rest_auth_header", DEFAULT_AUTH_HEADER);
            esp_http_client_set_header(hdl_hc, "Authorization", auth_header);
            free(auth_header);
        } else if (strcmp(auth_type, "Basic") == 0) {
            char *pass = config_get_char("rest_auth_pass", DEFAULT_AUTH_PASS);
            char *user = config_get_char("rest_auth_user", DEFAULT_AUTH_USER);
            http_set_basic_auth(hdl_hc, user, pass);
            free(pass);
            free(user);
        }
        free(auth_type);

        url = config_get_char("rest_webhook_url", NULL);
        if (url == NULL) {
            url = config_get_char("rest_url", DEFAULT_URL);
        }

        ESP_LOGI(TAG, "polling webhook: %s", url);
        ret = http_get(hdl_hc, url, &body, &http_status);
        free(url);

        if (ret != ESP_OK || http_status < 200 || http_status > 299) {
            ESP_LOGW(TAG, "webhook poll failed or non-2xx status: %d", http_status);
            free(body);
            body = NULL;
            continue;
        }

        if (body != NULL) {
            cJSON *json = cJSON_Parse(body);
            free(body);
            body = NULL;

            if (json != NULL) {
                cJSON *text = cJSON_GetObjectItemCaseSensitive(json, "text");
                if (cJSON_IsString(text) && text->valuestring != NULL && strlen(text->valuestring) > 0) {
                    ESP_LOGI(TAG, "webhook text: %s", text->valuestring);
                    war.fn_ok(text->valuestring);
                }
                cJSON_Delete(json);
            }
        }
    }

    vTaskDelete(NULL);
}

void rest_webhook_start(void)
{
    xTaskCreate(rest_webhook_task, "rest_webhook", 4 * 1024, NULL, 5, NULL);
}
