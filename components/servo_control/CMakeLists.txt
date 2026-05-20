idf_component_register(
    SRCS "servo_control.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc esp_timer freertos
)
