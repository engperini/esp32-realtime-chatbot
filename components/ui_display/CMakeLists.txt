idf_component_register(
    SRCS
        "display.cpp"
        "font_ptbr_20.c"
        
        "icons/eva_idle.c"
        "icons/eva_blink.c"
        "icons/eva_smile.c"
        
    INCLUDE_DIRS
        "."
    REQUIRES
        lvgl
        esp_timer
        seeed-studio-round-display
        eye_animation
        
)
