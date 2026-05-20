#include "eye_animation.h"
#include "eye_animation_engine.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "seeed-studio-round-display.hpp"
#include "lvgl.h"

// =====================================================
// LOG
// =====================================================
static const char *TAG = "EYE_ANIM";

// =====================================================
// STATE
// =====================================================
static EyeAnimationEngine *g_eye_engine = nullptr;
static TaskHandle_t eye_task_handle = nullptr;
static QueueHandle_t eye_queue = nullptr;
static SemaphoreHandle_t eye_initialized_sem = nullptr;

static bool eye_initialized = false;

// LVGL Objects for text overlay
static lv_obj_t *text_overlay = nullptr;
static lv_obj_t *text_label = nullptr;
static lv_obj_t *status_label = nullptr;

LV_FONT_DECLARE(font_ptbr_20);

// =====================================================
// COMMAND TYPES
// =====================================================
typedef enum {
    EYE_CMD_NONE = 0,
    EYE_CMD_INIT,
    EYE_CMD_SET_STATE,
    EYE_CMD_SET_AUTOBLINK,
    EYE_CMD_SET_IDLE_MODE,
    EYE_CMD_SET_FLICKER,
    EYE_CMD_TRIGGER_LAUGH,
    EYE_CMD_TRIGGER_CONFUSED,
    EYE_CMD_TRIGGER_SWEAT,
    EYE_CMD_LOOK_DIRECTION,
    EYE_CMD_SET_SIZE,
    EYE_CMD_SET_SPACING,
    EYE_CMD_SET_CYCLOPS,
    EYE_CMD_SET_TEXT,           // Texto overlay
    EYE_CMD_APPEND_TEXT,        // Append texto
    EYE_CMD_SET_STATUS,         // Status
    EYE_CMD_CLEAR_TEXT          // Limpar texto
} eye_cmd_t;

typedef struct {
    eye_cmd_t type;
    
    // Union of parameters
    union {
        eye_state_t state;
        bool bool_param;
        uint8_t uint8_param;
        struct {
            int x;
            int y;
        } position;
        struct {
            uint8_t width;
            uint8_t height;
        } size;
        struct {
            bool horizontal;
            bool enable;
            uint8_t amplitude;
        } flicker;
        char text[128];  // Texto overlay
    } params;
} eye_msg_t;

// =====================================================
// TASK
// =====================================================
static void eye_animation_task(void *arg);

// =====================================================
// PUBLIC API
// =====================================================

void eye_animation_init(void)
{
    if (eye_initialized) {
        return;
    }

    if (!eye_queue) {
        eye_queue = xQueueCreate(16, sizeof(eye_msg_t));
    }

    if (!eye_initialized_sem) {
        eye_initialized_sem = xSemaphoreCreateBinary();
    }

    xTaskCreatePinnedToCore(
        eye_animation_task,
        "eye_animation_task",
        12288,
        nullptr,
        2,
        &eye_task_handle,
        1   // Core 1
    );

    // Aguarda inicialização da task completar
    xSemaphoreTake(eye_initialized_sem, pdMS_TO_TICKS(5000));
    eye_initialized = true;
}

void eye_set_autoblink(bool enable)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_AUTOBLINK;
    msg.params.bool_param = enable;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_idle_mode(bool enable)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_IDLE_MODE;
    msg.params.bool_param = enable;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_flicker(bool horizontal, bool enable, uint8_t amplitude)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_FLICKER;
    msg.params.flicker.horizontal = horizontal;
    msg.params.flicker.enable = enable;
    msg.params.flicker.amplitude = amplitude;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_trigger_laugh(void)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_TRIGGER_LAUGH;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_trigger_confused(void)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_TRIGGER_CONFUSED;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_trigger_sweat(bool enable)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_TRIGGER_SWEAT;
    msg.params.bool_param = enable;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_state(eye_state_t state)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_STATE;
    msg.params.state = state;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_look_direction(int x, int y)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_LOOK_DIRECTION;
    msg.params.position.x = x;
    msg.params.position.y = y;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_size(uint8_t width, uint8_t height)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_SIZE;
    msg.params.size.width = width;
    msg.params.size.height = height;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_spacing(uint8_t spacing)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_SPACING;
    msg.params.uint8_param = spacing;
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_cyclops(bool enable)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_CYCLOPS;
    msg.params.bool_param = enable;
    xQueueSend(eye_queue, &msg, 0);
}

// =====================================================
// TEXT OVERLAY API
// =====================================================

void eye_set_text(const char *text)
{
    if (!eye_queue || !text) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_TEXT;
    strncpy(msg.params.text, text, sizeof(msg.params.text) - 1);
    xQueueSend(eye_queue, &msg, 0);
}

void eye_append_text(const char *text)
{
    if (!eye_queue || !text) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_APPEND_TEXT;
    strncpy(msg.params.text, text, sizeof(msg.params.text) - 1);
    xQueueSend(eye_queue, &msg, 0);
}

void eye_set_status(const char *text)
{
    if (!eye_queue || !text) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_SET_STATUS;
    strncpy(msg.params.text, text, sizeof(msg.params.text) - 1);
    xQueueSend(eye_queue, &msg, 0);
}

void eye_clear_text(void)
{
    if (!eye_queue) return;

    eye_msg_t msg = {};
    msg.type = EYE_CMD_CLEAR_TEXT;
    xQueueSend(eye_queue, &msg, 0);
}

// =====================================================
// TASK IMPLEMENTATION
// =====================================================

static void eye_animation_task(void *arg)
{
    ESP_LOGI(TAG, "Inicializando eye animation (PRINCIPAL)");

    // PIN CONFIG
    espp::SsRoundDisplay::PinConfig cfg = {};
    cfg.sda = GPIO_NUM_5;
    cfg.scl = GPIO_NUM_6;
    cfg.usd_cs = GPIO_NUM_3;
    cfg.lcd_cs = GPIO_NUM_2;
    cfg.lcd_dc = GPIO_NUM_4;
    cfg.lcd_backlight = GPIO_NUM_NC;
    cfg.miso = GPIO_NUM_NC;
    cfg.mosi = GPIO_NUM_9;
    cfg.sck  = GPIO_NUM_7;
    cfg.touch_interrupt = GPIO_NUM_44;

    espp::SsRoundDisplay::set_pin_config(cfg);
    auto &disp = espp::SsRoundDisplay::get();

    disp.initialize_lcd();
    disp.initialize_display(disp.lcd_width() * 2);

    lv_display_set_rotation(
        lv_display_get_default(),
        LV_DISPLAY_ROTATION_270
    );

    // CREATE ROOT CONTAINER (100% tela cheia)
    lv_obj_t *root = lv_scr_act();
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(root, lv_color_black(), 0);

    
    // Parametrização da área dos olhos (multiplicador do display)
    float eye_width_scale = 0.48f;   // 40% da largura do display
    float eye_height_scale = 0.30f;  // 30% da altura do display
    float eye_top_margin = 0.10f;    // 10% de espaço da borda superior

    
    // CREATE EYE CONTAINER com tamanho baseado no display
    lv_obj_t *eye_container = lv_obj_create(root);
    lv_obj_set_size(eye_container, 
                    (int)(disp.lcd_width() * eye_width_scale),
                    (int)(disp.lcd_height() * eye_height_scale));
    
    // Posiciona com margem superior
    int y_offset = (int)(disp.lcd_height() * eye_top_margin);
    lv_obj_align(eye_container, LV_ALIGN_TOP_MID, 0, y_offset);
    
    lv_obj_set_style_bg_color(eye_container, lv_color_black(), 0);
    lv_obj_set_style_pad_all(eye_container, 0, 0); 
    lv_obj_clear_flag(eye_container, LV_OBJ_FLAG_SCROLLABLE);

    // Calcular tamanho do container diretamente
    int container_w = (int)(disp.lcd_width() * eye_width_scale);
    int container_h = (int)(disp.lcd_height() * eye_height_scale);

    // CREATE EYE ENGINE com tamanho calculado
    g_eye_engine = new EyeAnimationEngine(eye_container);
    g_eye_engine->initialize(container_w, container_h, 30);
        
    // Default animations
    g_eye_engine->enable_autoblink(true, 3000, 1000);
    g_eye_engine->enable_idle_mode(true, 2000, 1000);
    //g_eye_engine->set_mood(EyeMood::CURIOUS);

    // CREATE TEXT OVERLAY (opcional, para textos)
    text_overlay = lv_obj_create(root);
    lv_obj_set_size(text_overlay, LV_PCT(75), LV_PCT(60));
    lv_obj_align(text_overlay, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(text_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(text_overlay, LV_OPA_100, 0);  // no Semi-transparente
    lv_obj_set_style_border_width(text_overlay, 0, 0);
    lv_obj_set_style_pad_all(text_overlay, 10, 0);
    lv_obj_clear_flag(text_overlay, LV_OBJ_FLAG_SCROLLABLE);

    text_label = lv_label_create(text_overlay);
    lv_label_set_long_mode(text_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text(text_label, "");
    lv_obj_set_width(text_label, LV_PCT(100));
    lv_obj_set_style_text_align(text_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(text_label, &font_ptbr_20, 0);
    lv_obj_set_style_text_color(text_label, lv_color_white(), 0);

    status_label = lv_label_create(text_overlay);
    lv_label_set_text(status_label, "READY");
    lv_obj_align_to(status_label, text_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_set_style_text_font(status_label, &font_ptbr_20, 0);
    lv_obj_set_style_text_color(status_label, lv_color_white(), 0);

    ESP_LOGI(TAG, "Eye animation ready");

    // Signal initialization complete
    xSemaphoreGive(eye_initialized_sem);

    // MAIN LOOP
    while (true) {
        eye_msg_t msg;

        // Process queue
        while (xQueueReceive(eye_queue, &msg, 0)) {
            switch (msg.type) {

            case EYE_CMD_SET_STATE:
                switch (msg.params.state) {
                    case EYE_STATE_IDLE:
                        g_eye_engine->set_mood(EyeMood::DEFAULT);
                        break;
                    case EYE_STATE_LISTENING:
                        g_eye_engine->set_mood(EyeMood::CURIOUS);
                        break;
                    case EYE_STATE_SPEAKING:
                        g_eye_engine->set_mood(EyeMood::HAPPY);
                        break;
                    case EYE_STATE_CONFUSED:
                        g_eye_engine->trigger_confused();
                        break;
                    case EYE_STATE_HAPPY:
                        g_eye_engine->set_mood(EyeMood::HAPPY);
                        break;
                    case EYE_STATE_TIRED:
                        g_eye_engine->set_mood(EyeMood::TIRED);
                        break;
                    case EYE_STATE_ANGRY:
                        g_eye_engine->set_mood(EyeMood::ANGRY);
                        break;
                    default:
                        break;
                }
                break;

            case EYE_CMD_SET_AUTOBLINK:
                g_eye_engine->enable_autoblink(msg.params.bool_param);
                break;

            case EYE_CMD_SET_IDLE_MODE:
                g_eye_engine->enable_idle_mode(msg.params.bool_param);
                break;

            case EYE_CMD_SET_FLICKER:
                g_eye_engine->set_flicker(msg.params.flicker.horizontal,
                                         msg.params.flicker.enable,
                                         msg.params.flicker.amplitude);
                break;

            case EYE_CMD_TRIGGER_LAUGH:
                g_eye_engine->trigger_laugh();
                break;

            case EYE_CMD_TRIGGER_CONFUSED:
                g_eye_engine->trigger_confused();
                break;

            case EYE_CMD_TRIGGER_SWEAT:
                g_eye_engine->trigger_sweat(msg.params.bool_param);
                break;

            case EYE_CMD_LOOK_DIRECTION:
                g_eye_engine->set_position_look(msg.params.position.x, 
                                               msg.params.position.y);
                break;

            case EYE_CMD_SET_SIZE:
                g_eye_engine->set_eye_size_default(msg.params.size.width,
                                                   msg.params.size.height);
                break;

            case EYE_CMD_SET_SPACING:
                g_eye_engine->set_spacing(msg.params.uint8_param);
                break;

            case EYE_CMD_SET_CYCLOPS:
                g_eye_engine->set_cyclops(msg.params.bool_param);
                break;

            case EYE_CMD_SET_TEXT:
                if (text_label) {
                    lv_label_set_text(text_label, msg.params.text);
                }
                break;

            case EYE_CMD_APPEND_TEXT:
                if (text_label) {
                    const char *current = lv_label_get_text(text_label);
                    char buffer[256] = {0};
                    snprintf(buffer, sizeof(buffer), "%s%s", current, msg.params.text);
                    lv_label_set_text(text_label, buffer);
                }
                break;

            case EYE_CMD_SET_STATUS:
                if (status_label) {
                    lv_label_set_text(status_label, msg.params.text);
                }
                break;

            case EYE_CMD_CLEAR_TEXT:
                if (text_label) {
                    lv_label_set_text(text_label, "");
                }
                break;

            default:
                break;
            }
        }

        // Update animation
        if (g_eye_engine) {
            g_eye_engine->update();
        }

        // LVGL handler
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }



}
