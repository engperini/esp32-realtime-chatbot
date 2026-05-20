#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================
// PUBLIC API
// =====================================================

// Inicializa engine de animação dos olhos
void eye_animation_init(void);

// Estados para os olhos
typedef enum {
    EYE_STATE_IDLE = 0,
    EYE_STATE_LISTENING,
    EYE_STATE_SPEAKING,
    EYE_STATE_CONFUSED,
    EYE_STATE_HAPPY,
    EYE_STATE_TIRED,
    EYE_STATE_ANGRY
} eye_state_t;

// Ativa/desativa animações automáticas
void eye_set_autoblink(bool enable);
void eye_set_idle_mode(bool enable);
void eye_set_flicker(bool horizontal, bool enable, uint8_t amplitude);

// Triggers animações únicas
void eye_trigger_laugh(void);
void eye_trigger_confused(void);
void eye_trigger_sweat(bool enable);

// Muda estado/mood dos olhos
void eye_set_state(eye_state_t state);

// Controle de posição (look direction)
void eye_look_direction(int x, int y);

// Configuração visual
void eye_set_size(uint8_t width, uint8_t height);
void eye_set_spacing(uint8_t spacing);
void eye_set_cyclops(bool enable);

// =====================================================
// TEXT OVERLAY API
// =====================================================

// Define texto no overlay
void eye_set_text(const char *text);

// Anexa texto ao overlay existente
void eye_append_text(const char *text);

// Define status (na segunda linha do overlay)
void eye_set_status(const char *text);

// Limpa texto do overlay
void eye_clear_text(void);

#ifdef __cplusplus
}
#endif
