#include "eye_animation_engine.h"
#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "EYE_ENGINE";

// =====================================================
// CONSTRUCTOR
// =====================================================
EyeAnimationEngine::EyeAnimationEngine(lv_obj_t *parent_container)
    : container(parent_container)
    , container_width(100)
    , container_height(100)
    , current_mood(EyeMood::DEFAULT)
    , mood_tired(false)
    , mood_angry(false)
    , mood_happy(false)
    , mood_curious(false)
    , frame_interval_ms(33)  // ~30 FPS default
    , last_frame_time_ms(0)
    , eye_left_obj(nullptr)
    , eye_right_obj(nullptr)
{
    // Initialize animation flags
    anim_flags.autoblink_enabled = false;
    anim_flags.idle_enabled = false;
    anim_flags.h_flicker = false;
    anim_flags.v_flicker = false;
    anim_flags.flicker_alternate = false;
    anim_flags.laugh_active = false;
    anim_flags.confused_active = false;
    anim_flags.sweat_active = false;
    anim_flags.cyclops = false;
    anim_flags.flicker_amplitude = 2;
    
    // Initialize timers
    anim_timers.blink_interval_ms = 3000;
    anim_timers.blink_variation_ms = 1000;
    anim_timers.blink_timer = 0;
    anim_timers.idle_interval_ms = 2000;
    anim_timers.idle_variation_ms = 1000;
    anim_timers.idle_timer = 0;
    anim_timers.laugh_duration_ms = 500;
    anim_timers.laugh_timer = 0;
    anim_timers.confused_duration_ms = 500;
    anim_timers.confused_timer = 0;
}

EyeAnimationEngine::~EyeAnimationEngine() {
    if (eye_left_obj) {
        lv_obj_del(eye_left_obj);
    }
    if (eye_right_obj) {
        lv_obj_del(eye_right_obj);
    }
}

// =====================================================
// INITIALIZATION
// =====================================================
void EyeAnimationEngine::initialize(int screen_width, int screen_height, uint8_t fps) {
    container_width = screen_width;
    container_height = screen_height;
    frame_interval_ms = 1000 / fps;
    last_frame_time_ms = get_time_ms();
    
    // Default eye dimensions
    int eye_width = 26; //was 36
    int eye_height = 36;
    int space_between = 10;
    
    // Initialize eye geometries
    eye_left.width_default = eye_width;
    eye_left.height_default = eye_height;
    eye_left.width_current = eye_width;
    eye_left.height_current = eye_height;
    eye_left.width_next = eye_width;
    eye_left.height_next = eye_height;
    
    eye_left.border_radius_default = 8;
    eye_left.border_radius_current = 8;
    eye_left.border_radius_next = 8;
    
    eye_left.is_open = true;
    eye_left.eyelid_height = 0;
    eye_left.eyelid_height_next = 0;
    
    // Position left eye
    eye_left.x_default = (screen_width - (eye_width + space_between + eye_width)) / 2;
    eye_left.y_default = clamp((screen_height - eye_height) / 2, 0, screen_height - eye_height);
    eye_left.x_current = eye_left.x_default;
    eye_left.y_current = eye_left.y_default;
    eye_left.x_next = eye_left.x_default;
    eye_left.y_next = eye_left.y_default;
    
    // Right eye (mirror of left)
    eye_right.width_default = eye_width;
    eye_right.height_default = eye_height;
    eye_right.width_current = eye_width;
    eye_right.height_current = eye_height;
    eye_right.width_next = eye_width;
    eye_right.height_next = eye_height;
    
    eye_right.border_radius_default = 8;
    eye_right.border_radius_current = 8;
    eye_right.border_radius_next = 8;
    
    eye_right.is_open = true;
    eye_right.eyelid_height = 0;
    eye_right.eyelid_height_next = 0;
    
    eye_right.x_default = eye_left.x_default + eye_width + space_between;
    eye_right.y_default = clamp(eye_left.y_default, 0, screen_height - eye_height);
    eye_right.x_current = eye_right.x_default;
    eye_right.y_current = eye_right.y_default;
    eye_right.x_next = eye_right.x_default;
    eye_right.y_next = eye_right.y_default;
    
    spacing_default = space_between;
    spacing_current = space_between;
    spacing_next = space_between;
    
    // Create LVGL objects for eyes
    eye_left_obj = lv_obj_create(container);
    lv_obj_set_size(eye_left_obj, eye_width, eye_height);
    lv_obj_set_pos(eye_left_obj, eye_left.x_current, eye_left.y_current);
    lv_obj_set_style_bg_color(eye_left_obj, lv_color_make(0,150,255), 0);
    lv_obj_set_style_radius(eye_left_obj, 8, 0);
    lv_obj_set_style_border_width(eye_left_obj, 0, 0);
    lv_obj_clear_flag(eye_left_obj, LV_OBJ_FLAG_SCROLLABLE);
    
    eye_right_obj = lv_obj_create(container);
    lv_obj_set_size(eye_right_obj, eye_width, eye_height);
    lv_obj_set_pos(eye_right_obj, eye_right.x_current, eye_right.y_current);
    lv_obj_set_style_bg_color(eye_right_obj, lv_color_make(0,150,255), 0);
    lv_obj_set_style_radius(eye_right_obj, 8, 0);
    lv_obj_set_style_border_width(eye_right_obj, 0, 0);
    lv_obj_clear_flag(eye_right_obj, LV_OBJ_FLAG_SCROLLABLE);
    
    ESP_LOGI(TAG, "Eye animation engine initialized: %dx%d @ %d FPS", 
             screen_width, screen_height, fps);
}

// =====================================================
// UPDATE LOOP
// =====================================================
void EyeAnimationEngine::update() {
    uint32_t now = get_time_ms();
    
    if ((now - last_frame_time_ms) >= frame_interval_ms) {
        // 1. Interpolate parameters smoothly
        interpolate_parameters();
        
        // 2. Apply mood effects
        apply_mood_effects();
        
        // 3. Update animations
        update_animations();
        
        // 4. Render
        render_eyes();
        render_eyelids();
        
        last_frame_time_ms = now;
    }
}

// =====================================================
// INTERPOLATION
// =====================================================
void EyeAnimationEngine::interpolate_parameters() {
    // Heights
    eye_left.height_current = (eye_left.height_current + eye_left.height_next) / 2;
    eye_right.height_current = (eye_right.height_current + eye_right.height_next) / 2;
    
    // Widths
    eye_left.width_current = (eye_left.width_current + eye_left.width_next) / 2;
    eye_right.width_current = (eye_right.width_current + eye_right.width_next) / 2;
    
    // Positions
    eye_left.x_current = (eye_left.x_current + eye_left.x_next) / 2;
    eye_left.y_current = (eye_left.y_current + eye_left.y_next) / 2;
    eye_right.x_current = (eye_right.x_current + eye_right.x_next) / 2;
    eye_right.y_current = (eye_right.y_current + eye_right.y_next) / 2;
    
    // Border radius
    eye_left.border_radius_current = (eye_left.border_radius_current + eye_left.border_radius_next) / 2;
    eye_right.border_radius_current = (eye_right.border_radius_current + eye_right.border_radius_next) / 2;
    
    // Spacing
    spacing_current = (spacing_current + spacing_next) / 2;
    
    // Eyelids
    eye_left.eyelid_height = (eye_left.eyelid_height + eye_left.eyelid_height_next) / 2;
    eye_right.eyelid_height = (eye_right.eyelid_height + eye_right.eyelid_height_next) / 2;
    eye_left.eyelid_angry_height = (eye_left.eyelid_angry_height + eye_left.eyelid_angry_height_next) / 2;
    eye_right.eyelid_angry_height = (eye_right.eyelid_angry_height + eye_right.eyelid_angry_height_next) / 2;
    eye_left.eyelid_happy_offset = (eye_left.eyelid_happy_offset + eye_left.eyelid_happy_offset_next) / 2;
    eye_right.eyelid_happy_offset = (eye_right.eyelid_happy_offset + eye_right.eyelid_happy_offset_next) / 2;
}

// =====================================================
// MOOD EFFECTS
// =====================================================
void EyeAnimationEngine::apply_mood_effects() {
    // Apply mood-specific eyelid movements
    if (mood_tired) {
        eye_left.eyelid_height_next = eye_left.height_current / 2;
        eye_right.eyelid_height_next = eye_right.height_current / 2;
    } else {
        eye_left.eyelid_height_next = 0;
        eye_right.eyelid_height_next = 0;
    }
    
    if (mood_angry) {
        eye_left.eyelid_angry_height_next = eye_left.height_current / 2;
        eye_right.eyelid_angry_height_next = eye_right.height_current / 2;
    } else {
        eye_left.eyelid_angry_height_next = 0;
        eye_right.eyelid_angry_height_next = 0;
    }
    
    if (mood_happy) {
        eye_left.eyelid_happy_offset_next = eye_left.height_current / 2;
        eye_right.eyelid_happy_offset_next = eye_right.height_current / 2;
    } else {
        eye_left.eyelid_happy_offset_next = 0;
        eye_right.eyelid_happy_offset_next = 0;
    }
    
    // Curious mode adds height offset
    if (mood_curious) {
        eye_left.height_next = eye_left.height_default + 8;
        eye_right.height_next = eye_right.height_default + 8;
    }
}

// =====================================================
// ANIMATION UPDATE
// =====================================================
void EyeAnimationEngine::update_animations() {
    uint32_t now = get_time_ms();
    
    // Auto-blink
    if (anim_flags.autoblink_enabled && now >= anim_timers.blink_timer) {
        blink();
        anim_timers.blink_timer = now + anim_timers.blink_interval_ms + 
                                  (rand() % anim_timers.blink_variation_ms);
    }
    
    // Idle mode (random movement)
    if (anim_flags.idle_enabled && now >= anim_timers.idle_timer) {
        // Espaço total necessário para os dois olhos com espaçamento
        int total_eye_width = eye_left.width_current + spacing_current + eye_right.width_current;
        
        // Garante que só gera posições onde AMBOS os olhos cabem
        eye_left.x_next = clamp(rand() % (container_width - total_eye_width + 1), 0, container_width - total_eye_width);
        eye_left.y_next = clamp(rand() % (container_height - eye_left.height_current), 0, container_height - eye_left.height_current);
        
        // Olho direito posicionado automaticamente após o esquerdo
        eye_right.x_next = eye_left.x_next + eye_left.width_current + spacing_current;
        eye_right.y_next = eye_left.y_next;
        
        anim_timers.idle_timer = now + anim_timers.idle_interval_ms + 
                                 (rand() % anim_timers.idle_variation_ms);
    }
    
    // Horizontal flicker
    if (anim_flags.h_flicker) {
        if (anim_flags.flicker_alternate) {
            eye_left.x_current += anim_flags.flicker_amplitude;
            eye_right.x_current += anim_flags.flicker_amplitude;
        } else {
            eye_left.x_current -= anim_flags.flicker_amplitude;
            eye_right.x_current -= anim_flags.flicker_amplitude;
        }
        anim_flags.flicker_alternate = !anim_flags.flicker_alternate;
    }
    
    // Vertical flicker
    if (anim_flags.v_flicker) {
        if (anim_flags.flicker_alternate) {
            eye_left.y_current += anim_flags.flicker_amplitude;
            eye_right.y_current += anim_flags.flicker_amplitude;
        } else {
            eye_left.y_current -= anim_flags.flicker_amplitude;
            eye_right.y_current -= anim_flags.flicker_amplitude;
        }
        anim_flags.flicker_alternate = !anim_flags.flicker_alternate;
    }
    
    // Laugh (vertical flicker)
    if (anim_flags.laugh_active) {
        if (anim_timers.laugh_timer == 0) {
            anim_timers.laugh_timer = now;
            anim_flags.v_flicker = true;
        } else if (now >= anim_timers.laugh_timer + anim_timers.laugh_duration_ms) {
            anim_flags.laugh_active = false;
            anim_flags.v_flicker = false;
            anim_timers.laugh_timer = 0;
        }
    }
    
    // Confused (horizontal flicker)
    if (anim_flags.confused_active) {
        if (anim_timers.confused_timer == 0) {
            anim_timers.confused_timer = now;
            anim_flags.h_flicker = true;
        } else if (now >= anim_timers.confused_timer + anim_timers.confused_duration_ms) {
            anim_flags.confused_active = false;
            anim_flags.h_flicker = false;
            anim_timers.confused_timer = 0;
        }
    }
    
    // Open eyes if they were closed
    if (eye_left.is_open && eye_left.height_current <= 1) {
        eye_left.height_next = eye_left.height_default;
    }
    if (eye_right.is_open && eye_right.height_current <= 1) {
        eye_right.height_next = eye_right.height_default;
    }
}

// =====================================================
// RENDERING
// =====================================================
void EyeAnimationEngine::render_eyes() {
    // Hide right eye if cyclops mode
    if (anim_flags.cyclops) {
        lv_obj_add_flag(eye_right_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(eye_right_obj, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Compensar Y quando a altura muda (para piscar parecer natural de cima para baixo)
    int y_offset_left = (eye_left.height_default - eye_left.height_current) / 2;
    int y_offset_right = (eye_right.height_default - eye_right.height_current) / 2;
    
    // Update left eye
    lv_obj_set_pos(eye_left_obj, eye_left.x_current, eye_left.y_current + y_offset_left);
    lv_obj_set_size(eye_left_obj, eye_left.width_current, eye_left.height_current);
    lv_obj_set_style_radius(eye_left_obj, eye_left.border_radius_current, 0);
    
    // Update right eye
    lv_obj_set_pos(eye_right_obj, eye_right.x_current, eye_right.y_current + y_offset_right);
    lv_obj_set_size(eye_right_obj, eye_right.width_current, eye_right.height_current);
    lv_obj_set_style_radius(eye_right_obj, eye_right.border_radius_current, 0);
}

void EyeAnimationEngine::render_eyelids() {
    // Eyelids rendering would be implemented with canvas or additional shapes
    // For now, the height reduction serves as visual closing
    // In a more advanced version, add triangle shapes for tired/angry eyelids
}

// =====================================================
// MOOD SETTERS
// =====================================================
void EyeAnimationEngine::set_mood(EyeMood mood) {
    current_mood = mood;
    mood_tired = false;
    mood_angry = false;
    mood_happy = false;
    mood_curious = false;
    
    switch (mood) {
        case EyeMood::TIRED:
            mood_tired = true;
            break;
        case EyeMood::ANGRY:
            mood_angry = true;
            break;
        case EyeMood::HAPPY:
            mood_happy = true;
            break;
        case EyeMood::CURIOUS:
            mood_curious = true;
            break;
        case EyeMood::DEFAULT:
        default:
            break;
    }
}

void EyeAnimationEngine::set_position_look(int x, int y) {
    eye_left.x_next = clamp(x, 0, container_width - eye_left.width_current);
    eye_left.y_next = clamp(y, 0, container_height - eye_left.height_current);
    
    eye_right.x_next = eye_left.x_next + eye_left.width_current + spacing_current;
    eye_right.y_next = eye_left.y_next;
}

// =====================================================
// BASIC ANIMATIONS
// =====================================================
void EyeAnimationEngine::close(bool left, bool right) {
    if (left) {
        eye_left.height_next = 1;
        eye_left.is_open = false;
    }
    if (right) {
        eye_right.height_next = 1;
        eye_right.is_open = false;
    }
}

void EyeAnimationEngine::open(bool left, bool right) {
    if (left) {
        eye_left.is_open = true;
    }
    if (right) {
        eye_right.is_open = true;
    }
}

void EyeAnimationEngine::blink(bool left, bool right) {
    close(left, right);
    open(left, right);
}

// =====================================================
// MACRO ANIMATIONS
// =====================================================
void EyeAnimationEngine::enable_autoblink(bool enable, uint32_t interval_ms, uint32_t variation_ms) {
    anim_flags.autoblink_enabled = enable;
    anim_timers.blink_interval_ms = interval_ms;
    anim_timers.blink_variation_ms = variation_ms;
    if (enable) {
        anim_timers.blink_timer = get_time_ms();
    }
}

void EyeAnimationEngine::enable_idle_mode(bool enable, uint32_t interval_ms, uint32_t variation_ms) {
    anim_flags.idle_enabled = enable;
    anim_timers.idle_interval_ms = interval_ms;
    anim_timers.idle_variation_ms = variation_ms;
    if (enable) {
        anim_timers.idle_timer = get_time_ms();
    }
}

void EyeAnimationEngine::set_flicker(bool horizontal, bool enable, uint8_t amplitude) {
    if (horizontal) {
        anim_flags.h_flicker = enable;
    } else {
        anim_flags.v_flicker = enable;
    }
    anim_flags.flicker_amplitude = amplitude;
}

void EyeAnimationEngine::trigger_laugh() {
    anim_flags.laugh_active = true;
    anim_timers.laugh_timer = 0;
}

void EyeAnimationEngine::trigger_confused() {
    anim_flags.confused_active = true;
    anim_timers.confused_timer = 0;
}

void EyeAnimationEngine::trigger_sweat(bool enable) {
    anim_flags.sweat_active = enable;
}

// =====================================================
// VISUAL CONFIGURATION
// =====================================================
void EyeAnimationEngine::set_eye_size(uint8_t width_left, uint8_t height_left,
                                       uint8_t width_right, uint8_t height_right) {
    eye_left.width_next = width_left;
    eye_left.height_next = height_left;
    eye_left.width_default = width_left;
    eye_left.height_default = height_left;
    
    eye_right.width_next = width_right;
    eye_right.height_next = height_right;
    eye_right.width_default = width_right;
    eye_right.height_default = height_right;
}

void EyeAnimationEngine::set_eye_size_default(uint8_t width, uint8_t height) {
    set_eye_size(width, height, width, height);
}

void EyeAnimationEngine::set_border_radius(uint8_t left, uint8_t right) {
    eye_left.border_radius_next = left;
    eye_left.border_radius_default = left;
    eye_right.border_radius_next = right;
    eye_right.border_radius_default = right;
}

void EyeAnimationEngine::set_spacing(int spacing) {
    spacing_next = spacing;
    spacing_default = spacing;
}

void EyeAnimationEngine::set_cyclops(bool enable) {
    anim_flags.cyclops = enable;
}

// =====================================================
// GETTERS
// =====================================================
bool EyeAnimationEngine::is_rendering() const {
    return eye_left_obj != nullptr && eye_right_obj != nullptr;
}

// =====================================================
// UTILITY
// =====================================================
uint32_t EyeAnimationEngine::get_time_ms() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

int EyeAnimationEngine::clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}
