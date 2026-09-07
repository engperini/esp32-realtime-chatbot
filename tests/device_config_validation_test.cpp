#include "device_config_validation.hpp"

#include <cassert>

int main() {
    assert(device_config_fields_valid("Casa", "senha-segura", "sk-test"));
    assert(device_config_fields_valid("RedeAberta", "", "sk-test"));
    assert(!device_config_fields_valid("", "senha-segura", "sk-test"));
    assert(!device_config_fields_valid("Casa", "senha-segura", ""));
    return 0;
}
