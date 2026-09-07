#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

constexpr size_t DEVICE_CONFIG_SSID_MAX_LEN = 32;
constexpr size_t DEVICE_CONFIG_PASSWORD_MAX_LEN = 64;
constexpr size_t DEVICE_CONFIG_API_KEY_MAX_LEN = 255;

struct device_config_t {
    char wifi_ssid[DEVICE_CONFIG_SSID_MAX_LEN + 1];
    char wifi_password[DEVICE_CONFIG_PASSWORD_MAX_LEN + 1];
    char openai_api_key[DEVICE_CONFIG_API_KEY_MAX_LEN + 1];
};

using device_config_status_callback_t = void (*)(const char *message);

bool device_config_load(device_config_t *config);
esp_err_t device_config_save(const device_config_t *config);
bool device_config_is_valid(const device_config_t *config);
esp_err_t device_config_erase(void);

esp_err_t device_config_start_portal(device_config_status_callback_t status_callback);
void device_config_stop_portal(void);
const char *device_config_portal_ssid(void);
const char *device_config_portal_password(void);
constexpr const char *DEVICE_CONFIG_PORTAL_IP = "192.168.4.1";
