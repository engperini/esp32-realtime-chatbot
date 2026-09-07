#pragma once

inline bool device_config_fields_valid(const char *ssid, const char *password, const char *api_key) {
    return ssid != nullptr && ssid[0] != '\0' &&
           password != nullptr &&
           api_key != nullptr && api_key[0] != '\0';
}
