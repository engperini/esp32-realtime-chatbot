#pragma once

#include "lvgl.h"
#include <cstdint>
#include <cstdlib>

// =====================================================
// MOOD ENUM
// =====================================================
enum class EyeMood : uint8_t {
    DEFAULT = 0,
    TIRED = 1,
    ANGRY = 2,
    HAPPY = 3,
    CURIOUS = 4
};

// =====================================================
// EYE GEOMETRY STATE
// =====================================================
struct EyeGeometry {
    // Position
    int x_current;
    int y_current;
    int x_next;
    int y_next;
    int x_default;
    int y_default;
    
    // Dimensions
    int width_current;
    int height_current;
    int width_next;
    int height_next;
    int width_default;
    int height_default;
    
    // Border radius
    uint8_t border_radius_current;
    uint8_t border_radius_next;
    uint8_t border_radius_default;
    
    // Eyelid
    int eyelid_height;
    int eyelid_height_next;
    int eyelid_angry_height;
    int eyelid_angry_height_next;
    int eyelid_happy_offset;
    int eyelid_happy_offset_next;
    
    // State
    bool is_open;
};

// =====================================================
// ANIMATION STATE
// =====================================================
struct AnimationFlags {
    bool autoblink_enabled;
    bool idle_enabled;
    bool h_flicker;
    bool v_flicker;
    bool flicker_alternate;
    bool laugh_active;
    bool confused_active;
    bool sweat_active;
    bool cyclops;
    
    uint8_t flicker_amplitude;
};

struct AnimationTimers {
    uint32_t blink_interval_ms;
    uint32_t blink_variation_ms;
    uint32_t blink_timer;
    
    uint32_t idle_interval_ms;
    uint32_t idle_variation_ms;
    uint32_t idle_timer;
    
    uint32_t laugh_duration_ms;
    uint32_t laugh_timer;
    
    uint32_t confused_duration_ms;
    uint32_t confused_timer;
};

// =====================================================
// MAIN ENGINE CLASS
// =====================================================
class EyeAnimationEngine {
private:
    // Container
    lv_obj_t *container;
    int container_width;
    int container_height;
    
    // Eye geometries
    EyeGeometry eye_left;
    EyeGeometry eye_right;
    
    // Spacing
    int spacing_current;
    int spacing_next;
    int spacing_default;
    
    // Moods
    EyeMood current_mood;
    bool mood_tired;
    bool mood_angry;
    bool mood_happy;
    bool mood_curious;
    
    // Animation state
    AnimationFlags anim_flags;
    AnimationTimers anim_timers;
    
    // Timing
    uint32_t frame_interval_ms;
    uint32_t last_frame_time_ms;
    
    // LVGL objects
    lv_obj_t *eye_left_obj;
    lv_obj_t *eye_right_obj;
    
public:
    EyeAnimationEngine(lv_obj_t *parent_container);
    ~EyeAnimationEngine();
    
    // Initialization
    void initialize(int screen_width, int screen_height, uint8_t fps);
    void update();
    
    // Mood setters
    void set_mood(EyeMood mood);
    void set_position_look(int x, int y);
    
    // Basic animation
    void close(bool left = true, bool right = true);
    void open(bool left = true, bool right = true);
    void blink(bool left = true, bool right = true);
    
    // Macro animations
    void enable_autoblink(bool enable, uint32_t interval_ms = 3000, uint32_t variation_ms = 1000);
    void enable_idle_mode(bool enable, uint32_t interval_ms = 2000, uint32_t variation_ms = 1000);
    void set_flicker(bool horizontal, bool enable, uint8_t amplitude = 5);
    void trigger_laugh();
    void trigger_confused();
    void trigger_sweat(bool enable);
    
    // Visual configuration
    void set_eye_size(uint8_t width_left, uint8_t height_left,
                      uint8_t width_right, uint8_t height_right);
    void set_eye_size_default(uint8_t width, uint8_t height);
    void set_border_radius(uint8_t left, uint8_t right);
    void set_spacing(int spacing);
    void set_cyclops(bool enable);
    
    // Getters
    bool is_rendering() const;

private:
    // Internal rendering
    void interpolate_parameters();
    void apply_mood_effects();
    void update_animations();
    void render_eyes();
    void render_eyelids();
    
    // Utility
    uint32_t get_time_ms();
    int clamp(int value, int min, int max);
};
