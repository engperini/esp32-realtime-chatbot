#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "seeed-studio-round-display.hpp"
#include "lvgl.h"

#include "esp_timer.h"
#include "eye_animation.h"

LV_FONT_DECLARE(font_ptbr_20);

// =====================================================
// LOG
// =====================================================
static const char *TAG = "DISPLAY";

// =====================================================
// LVGL OBJECTS (DEPRECATED)
// =====================================================
// static lv_obj_t *root   = nullptr;
// static lv_obj_t *top    = nullptr;
// static lv_obj_t *bottom = nullptr;
// static lv_obj_t *label  = nullptr;



// ===== ICONS =====
// LV_IMG_DECLARE(eva_idle);
// LV_IMG_DECLARE(eva_blink);
// LV_IMG_DECLARE(eva_smile);

// static lv_obj_t *icon_img = nullptr;

// ===== STATUS (DEPRECATED) =====
// static lv_obj_t *status = nullptr;
// static lv_obj_t *status_label = nullptr;



// =====================================================
// TASK / QUEUE (DEPRECATED)
// =====================================================
// static TaskHandle_t display_task_handle = nullptr;
// static QueueHandle_t disp_queue = nullptr;

// =====================================================
// DISPLAY COMMANDS
// =====================================================
typedef enum {
    DISP_CMD_NONE = 0,
    DISP_CMD_BOOT,
    DISP_CMD_SET_TEXT,
    DISP_CMD_APPEND_TEXT,
    DISP_CMD_STATE_IDLE,
    DISP_CMD_STATE_LISTENING,
    DISP_CMD_STATE_SPEAKING
} disp_cmd_t;

typedef struct {
    disp_cmd_t type;
    char text[64];
} disp_msg_t;





// =====================================================
// INTERNAL (DEPRECATED)
// =====================================================
// static void display_task(void *);

// =====================================================
// PUBLIC API
// =====================================================

void display_init(void)
{
    // Display.cpp agora é SECUNDÁRIO
    // eye_animation_init() é o PRINCIPAL que cuida do display
    // Esta função apenas garante compatibilidade com código existente
    ESP_LOGI(TAG, "Display init (SECUNDÁRIO - eye_animation é principal)");
}

void display_show_boot(void)
{
    // Redireciona para eye_animation (PRINCIPAL)
    eye_set_text("Iniciando...");
}

void display_set_text(const char *text)
{
    if (!text) return;
    // Redireciona para eye_animation
    eye_set_text(text);
}

void display_set_state_idle(void)
{
    // Redireciona para eye_animation
    eye_set_state(EYE_STATE_IDLE);
    eye_set_status("IDLE");
}

void display_set_state_listening(void)
{
    // Redireciona para eye_animation
    eye_set_state(EYE_STATE_LISTENING);
    eye_set_status("LISTENING");
}

void display_set_state_speaking(void)
{
    // Redireciona para eye_animation
    eye_set_state(EYE_STATE_SPEAKING);
    eye_set_status("SPEAKING");
}

void display_append_text(const char *text)
{
    if (!text) return;
    // Redireciona para eye_animation
    eye_append_text(text);
}

// static void display_set_status(const char *text)
// {
//     if (!status_label) return;
//     lv_label_set_text(status_label, text);
// }



// =====================================================
// DISPLAY TASK (DEPRECATED - eye_animation cuida disso)
// =====================================================
// A task foi removida. eye_animation_init() é o PRINCIPAL agora.
// Manter display_init() vazio para compatibilidade com código existente.
