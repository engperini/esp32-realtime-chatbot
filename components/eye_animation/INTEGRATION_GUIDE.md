# Eye Animation Component - Integration Guide

## 📦 Componente Criado: `components/eye_animation/`

```
components/eye_animation/
├── CMakeLists.txt
├── eye_animation.h              # API C pública
├── eye_animation.cpp            # Task + Queue + API
├── eye_animation_engine.h       # Engine C++
└── eye_animation_engine.cpp     # Implementação completa
```

### Arquitetura

```
┌─────────────────────────────────────────┐
│  Application (main.cpp)                 │
│  ├─ eye_set_state(EYE_STATE_IDLE)      │
│  ├─ eye_set_autoblink(true)            │
│  └─ eye_trigger_laugh()                │
└─────────────┬───────────────────────────┘
              │
        Queue (async)
              │
┌─────────────▼───────────────────────────┐
│  eye_animation_task (Core 1)            │
│  ├─ Processa fila de comandos          │
│  ├─ Chama g_eye_engine->update()       │
│  └─ Roda LVGL timer handler            │
└─────────────┬───────────────────────────┘
              │
┌─────────────▼───────────────────────────┐
│  EyeAnimationEngine (C++)               │
│  ├─ interpolate_parameters()            │
│  ├─ apply_mood_effects()                │
│  ├─ update_animations()                 │
│  └─ render_eyes()                       │
└─────────────┬───────────────────────────┘
              │
      LVGL Objects (Display)
```

---

## 🔌 Como Usar

### 1. **Inicializar o Componente**

```c
// Em main.cpp ou seu código principal
#include "eye_animation.h"

void app_main() {
    // ... outras inicializações ...
    
    // Inicializa o componente de animação dos olhos
    eye_animation_init();
    
    // ... resto do código ...
}
```

### 2. **Controlar Estados**

```c
// Mudar estado dos olhos
eye_set_state(EYE_STATE_IDLE);        // Padrão
eye_set_state(EYE_STATE_LISTENING);   // Curioso (escutando)
eye_set_state(EYE_STATE_SPEAKING);    // Feliz (falando)
eye_set_state(EYE_STATE_CONFUSED);    // Confuso
eye_set_state(EYE_STATE_HAPPY);       // Feliz
eye_set_state(EYE_STATE_TIRED);       // Cansado
eye_set_state(EYE_STATE_ANGRY);       // Bravo
```

### 3. **Ativar Animações**

```c
// Autoblink
eye_set_autoblink(true);              // Ativa pisca automática
eye_set_autoblink(false);             // Desativa

// Idle mode (movimento aleatório)
eye_set_idle_mode(true);              // Ativa
eye_set_idle_mode(false);             // Desativa

// Flicker (oscilação)
eye_set_flicker(true, true, 5);       // Horizontal, ativar, amplitude 5
eye_set_flicker(false, true, 3);      // Vertical, ativar, amplitude 3
eye_set_flicker(true, false, 0);      // Parar horizontal

// Animações únicas
eye_trigger_laugh();                  // Risada (vibração vertical)
eye_trigger_confused();               // Confusão (vibração horizontal)
```

### 4. **Configuração Visual**

```c
// Tamanho dos olhos
eye_set_size(40, 40);                 // 40x40 pixels

// Espaço entre olhos
eye_set_spacing(15);                  // 15 pixels entre eles

// Cyclops (um olho só)
eye_set_cyclops(true);                // Ativa
eye_set_cyclops(false);               // Desativa (dois olhos)

// Direção do olhar
eye_look_direction(100, 50);          // Olha para (100, 50)
```

---

## 🎬 Exemplo de Uso Completo

```c
#include "eye_animation.h"

void demo_eyes() {
    // Inicializar
    eye_animation_init();
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // Sequência de demonstração
    
    // 1. Estado idle com autoblink
    eye_set_state(EYE_STATE_IDLE);
    eye_set_autoblink(true);
    eye_set_idle_mode(true);
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 2. Ouvindo
    eye_set_state(EYE_STATE_LISTENING);
    eye_set_flicker(true, true, 3);  // Flicker horizontal
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 3. Confuso
    eye_trigger_confused();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 4. Falando com risada
    eye_set_state(EYE_STATE_SPEAKING);
    eye_trigger_laugh();
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // 5. Cansado
    eye_set_state(EYE_STATE_TIRED);
    eye_set_autoblink(false);
    vTaskDelay(pdMS_TO_TICKS(1500));
    
    // 6. Bravo
    eye_set_state(EYE_STATE_ANGRY);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Volta ao idle
    eye_set_state(EYE_STATE_IDLE);
}
```

---

## 🔄 Integração com `ui_display`

Se você quiser manter o `ui_display` para textos enquanto usa `eye_animation` para os olhos:

### Opção 1: Componentes Totalmente Separados

```
┌─────────────────────────────────────────┐
│         Main App                        │
├─────────────────────────────────────────┤
│  eye_animation_init()  ──┐              │
│  display_init()        ──┼─┐            │
│                          │ │            │
│  ┌──────────────────────┘ │            │
│  ▼                        │            │
│  [Eye Component]          │            │
│  [Display Component]      │            │
│  (Top 40% - Eyes)      ◄──┘            │
│  (Bottom 60% - Text)                   │
└─────────────────────────────────────────┘
```

### Opção 2: Substituir Icons no Display

Remova do `display.cpp`:
```cpp
// REMOVER
icon_img = lv_image_create(top);
lv_image_set_src(icon_img, &eva_idle);
```

Adicione chamada a `eye_animation_init()` após `display_init()`.

---

## 📊 Comparação: Antes vs Depois

### ANTES (Imagens Estáticas)
```
display_set_state_idle()
  ↓
lv_image_set_src(icon_img, &eva_idle)
  ↓
Imagem troca de forma abrupta
```

### DEPOIS (Engine Dinâmica)
```
eye_set_state(EYE_STATE_IDLE)
  ↓
Queue envia mensagem
  ↓
Engine interpola parâmetros suavemente
  ↓
Cada frame: nova geometria
  ↓
Transição suave + animações contínuas
```

---

## 🎨 Moods Disponíveis

| Mood | Efeito |
|------|--------|
| **DEFAULT** | Olhos normais |
| **TIRED** | Pálpebra superior cobre 50% |
| **ANGRY** | Pálpebra superior inclinada |
| **HAPPY** | Pálpebra inferior levantada |
| **CURIOUS** | Altura aumentada 8px |

---

## ⚙️ Configurações Padrão

```cpp
// Dimensões
eye_width_default = 36px
eye_height_default = 36px
spacing_between = 10px
border_radius = 8px

// Animações
autoblink_interval = 3000ms ± 1000ms
idle_interval = 2000ms ± 1000ms
laugh_duration = 500ms
confused_duration = 500ms

// Frame Rate
fps_default = 30 FPS
frame_interval = 33ms
```

---

## 🔧 Customização

### Alterar FPS na inicialização

```cpp
// No eye_animation_engine.cpp, altere:
g_eye_engine->initialize(disp.lcd_width(), disp.lcd_height(), 60);  // 60 FPS
```

### Alterar tamanho padrão dos olhos

```cpp
g_eye_engine->set_eye_size_default(45, 45);  // 45x45 pixels
```

### Cores dos olhos

Modifique em `eye_animation_engine.cpp`:
```cpp
lv_obj_set_style_bg_color(eye_left_obj, lv_color_white(), 0);  // Mude a cor
```

---

## 📝 Próximas Melhorias

- [ ] Suporte a pupils (pupilas) dinâmicas
- [ ] Canvas para desenho de pálpebras customizadas
- [ ] Gotas de suor animadas
- [ ] Múltiplos estilos de olhos
- [ ] Calibração de posição por toque
- [ ] Gravação de sequências de animação

---

## ✅ Checklist de Uso

```
□ #include "eye_animation.h" adicionado
□ eye_animation_init() chamado em app_main()
□ Componente adicionado ao CMakeLists.txt raiz
□ Rebuild do projeto
□ Testar eye_set_state(EYE_STATE_IDLE)
□ Verificar animações automáticas
□ Integração com display.cpp (opcional)
```

