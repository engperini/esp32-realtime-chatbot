#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Inicializa hardware + LVGL + task interna
void display_init(void);

// Mostra tela inicial (boot)
void display_show_boot(void);

// Atualiza texto inferior (transcrição, logs etc)
void display_set_text(const char *text);

// Atualiza estado visual do robô
void display_set_state_idle(void);
void display_set_state_speaking(void);
void display_set_state_listening(void);
void display_append_text(const char *text);


#ifdef __cplusplus
}
#endif
