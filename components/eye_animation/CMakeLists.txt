idf_component_register(
    SRC_DIRS "."
    INCLUDE_DIRS "."
    REQUIRES lvgl esp_timer freertos seeed-studio-round-display driver
    #PRIV_REQUIRES esp_log
)
