# Estrutura da Engine de Animação de Olhos - LVGL Adaptation

## 📋 Análise Comparativa

### Sistema Atual (ui_display + LVGL)
- **Renderização**: Imagens estáticas (sprite sheets: eva_idle, eva_blink, eva_smile)
- **Controle**: Queue baseado em mensagens (DISP_CMD_STATE_*)
- **Animação**: Troca de imagens pré-renderizadas
- **Framework Gráfico**: LVGL (biblioteca de UI abstrata)

### Sistema Robot Eyes (oled_gfx)
- **Renderização**: Desenho em tempo real (primitivas: retângulos, triângulos)
- **Controle**: Estados e flags diretos (tired, angry, happy, etc)
- **Animação**: Interpolação suave de parâmetros geométricos
- **Framework Gráfico**: Adafruit GFX (baixo nível, pixel direto)

---

## 🎨 Conceitos-Chave da Engine Robot Eyes

### 1. **Sistema de Estados (Moods)**
```
TIRED  → Pálpebras fechadas (triângulos acima dos olhos)
ANGRY  → Pálpebras inclinadas (expressão agressiva)
HAPPY  → Pálpebras inferiores levantadas (sorriso)
CURIOUS → Offset de altura nas pálpebras
```

### 2. **Geometria Dinâmica dos Olhos**
- **Dimensões Suaves**: 
  - `Current = (Current + Next) / 2` (interpolação linear)
  - Transição suave entre estados

- **Parâmetros por Olho**:
  - Posição (x, y)
  - Largura e altura (redimensionamento)
  - Raio da borda (border-radius)
  - Offset de pálpebra

### 3. **Animações Macro (Automáticas)**
```
AUTOBLINKER    → Pisca aleatória a cada N segundos
IDLE_MODE      → Movimento aleatório dos olhos
CONFUSED       → Oscilação horizontal (hFlicker)
LAUGH          → Oscilação vertical (vFlicker)
SWEAT_DROPS    → Gotas caindo com física
```

### 4. **Sistema de Timing**
- FPS baseado em intervalo: `frameInterval = 1000 / fps`
- Verificação por `millis()` antes de desenhar
- Timers separados para cada animação

---

## 🔄 Fluxo de Renderização (drawEyes)

```
1. CÁLCULOS PRÉ-ANIMAÇÃO
   ├─ Aplicar mood (tired/angry/happy)
   ├─ Aplicar curious mode
   └─ Calcular offsets de pálpebra

2. INTERPOLAÇÃO SUAVE
   ├─ Heights: eyeLheightCurrent = (Current + Next) / 2
   ├─ Widths: eyeLwidthCurrent = (Current + Next) / 2
   ├─ Posições: eyeLx = (Lx + LxNext) / 2
   └─ Raios: eyeLborderRadiusCurrent = (Current + Next) / 2

3. LÓGICA DE ABRIR/FECHAR
   ├─ Se eyeL_open && eyeLheightCurrent ≤ 1
   │  └─ eyeLheightNext = eyeLheightDefault
   └─ Sincronizar ambos os olhos

4. ANIMAÇÕES ATIVAS
   ├─ Autoblink (timer com variação aleatória)
   ├─ Idle (movimento aleatório)
   ├─ Flickering (oscilação)
   ├─ Laugh (vFlicker)
   ├─ Confused (hFlicker)
   └─ Sweat (gotas com trajetória)

5. RENDERIZAÇÃO
   ├─ Limpar display
   ├─ Desenhar corpos dos olhos (retângulos com raio)
   ├─ Desenhar pálpebras (triângulos de cobertura)
   ├─ Desenhar gotas de suor (se ativo)
   └─ Atualizar display
```

---

## 🏗️ Estrutura Proposta para LVGL

### **Arquivo: `eye_animation_engine.h`**

```cpp
#pragma once

#include "lvgl.h"
#include <cstdint>
#include <cmath>

// ==================== ENUMERATIONS ====================
enum EyeMood {
    MOOD_DEFAULT = 0,
    MOOD_TIRED,
    MOOD_ANGRY,
    MOOD_HAPPY,
    MOOD_CURIOUS
};

enum EyeAnimation {
    ANIM_NONE = 0,
    ANIM_BLINK,
    ANIM_LAUGH,
    ANIM_CONFUSED,
    ANIM_IDLE
};

// ==================== EYE ENGINE CLASS ====================
class EyeAnimationEngine {
private:
    // Container LVGL
    lv_obj_t *container;
    
    // ====== GEOMETRY (Estado Atual) ======
    struct EyeState {
        // Posição
        int x_current, y_current;
        int x_next, y_next;
        
        // Dimensões
        int width_current, height_current;
        int width_next, height_next;
        int width_default, height_default;
        
        // Border radius
        uint8_t border_radius_current;
        uint8_t border_radius_next;
        uint8_t border_radius_default;
        
        // Pálpebra
        int eyelid_offset;
        int eyelid_offset_next;
        
        // Estado
        bool is_open;
        
    } eye_left, eye_right;
    
    // Espaço entre olhos
    int spacing_current;
    int spacing_next;
    int spacing_default;
    
    // ====== MOODS ======
    bool mood_tired;
    bool mood_angry;
    bool mood_happy;
    bool mood_curious;
    bool cyclops;  // Um olho só
    
    // ====== ANIMAÇÕES MACRO ======
    struct AnimationState {
        bool autoblink_enabled;
        uint32_t blink_interval_ms;
        uint32_t blink_interval_variation_ms;
        uint32_t blink_timer;
        
        bool idle_enabled;
        uint32_t idle_interval_ms;
        uint32_t idle_variation_ms;
        uint32_t idle_timer;
        
        bool h_flicker;  // Horizontal
        bool v_flicker;  // Vertical
        uint8_t flicker_amplitude;
        bool flicker_alternate;
        
        bool laugh_active;
        uint32_t laugh_timer;
        uint32_t laugh_duration_ms;
        
        bool confused_active;
        uint32_t confused_timer;
        uint32_t confused_duration_ms;
        
        bool sweat_active;
    } animation;
    
    // ====== TIMING ======
    uint32_t frame_interval_ms;
    uint32_t last_frame_time_ms;
    
    // ====== LVGL OBJECTS ======
    lv_obj_t *eye_left_obj;
    lv_obj_t *eye_right_obj;
    lv_obj_t *eyelid_left_obj;
    lv_obj_t *eyelid_right_obj;
    lv_obj_t *sweat_drops[3];
    
public:
    // Constructor
    EyeAnimationEngine(lv_obj_t *parent_container);
    
    // ====== INICIALIZAÇÃO ======
    void initialize(int screen_width, int screen_height, uint8_t fps);
    void update();
    
    // ====== MOOD SETTERS ======
    void set_mood(EyeMood mood);
    void set_position_look(int x, int y);
    
    // ====== ANIMAÇÃO BÁSICA ======
    void close(bool left = true, bool right = true);
    void open(bool left = true, bool right = true);
    void blink(bool left = true, bool right = true);
    
    // ====== ANIMAÇÕES MACRO ======
    void enable_autoblink(bool enable, uint32_t interval_ms = 3000, uint32_t variation_ms = 1000);
    void enable_idle_mode(bool enable, uint32_t interval_ms = 2000, uint32_t variation_ms = 1000);
    void set_flicker(bool horizontal, bool enable, uint8_t amplitude = 5);
    void trigger_laugh();
    void trigger_confused();
    void trigger_sweat(bool enable);
    
    // ====== CONFIGURAÇÃO VISUAL ======
    void set_eye_size(uint8_t left_width, uint8_t left_height, 
                      uint8_t right_width, uint8_t right_height);
    void set_border_radius(uint8_t left, uint8_t right);
    void set_spacing(int spacing);
    void set_cyclops(bool enable);
    
private:
    // ====== INTERNAL METHODS ======
    void interpolate_parameters();
    void apply_mood_effects();
    void update_animations();
    void render_eyes();
    void render_eyelids();
    void render_sweat();
    
    uint32_t get_time_ms();
};
```

### **Arquivo: `eye_animation_engine.cpp`** (Pseudocódigo)

```cpp
#include "eye_animation_engine.h"

EyeAnimationEngine::EyeAnimationEngine(lv_obj_t *parent) 
    : container(parent) 
{
    // Inicializar objetos LVGL (retângulos com raio + triângulos)
    // Criar layers para olhos, pálpebras, etc.
}

void EyeAnimationEngine::initialize(int width, int height, uint8_t fps) {
    // Calcular posições padrão dos olhos
    // Config: dimensões, espaçamento, border-radius
    // Config: frame interval = 1000 / fps
}

void EyeAnimationEngine::update() {
    uint32_t now = get_time_ms();
    
    if ((now - last_frame_time_ms) >= frame_interval_ms) {
        
        // 1. Interpolação suave
        interpolate_parameters();
        
        // 2. Aplicar efeitos de mood
        apply_mood_effects();
        
        // 3. Processar animações
        update_animations();
        
        // 4. Renderizar
        render_eyes();
        render_eyelids();
        if (animation.sweat_active) {
            render_sweat();
        }
        
        last_frame_time_ms = now;
    }
}

void EyeAnimationEngine::interpolate_parameters() {
    // HEIGHTS
    eye_left.height_current = (eye_left.height_current + eye_left.height_next) / 2;
    eye_right.height_current = (eye_right.height_current + eye_right.height_next) / 2;
    
    // WIDTHS
    eye_left.width_current = (eye_left.width_current + eye_left.width_next) / 2;
    eye_right.width_current = (eye_right.width_current + eye_right.width_next) / 2;
    
    // POSITIONS
    eye_left.x_current = (eye_left.x_current + eye_left.x_next) / 2;
    eye_left.y_current = (eye_left.y_current + eye_left.y_next) / 2;
    eye_right.x_current = (eye_right.x_current + eye_right.x_next) / 2;
    eye_right.y_current = (eye_right.y_current + eye_right.y_next) / 2;
    
    // RADIUS
    eye_left.border_radius_current = 
        (eye_left.border_radius_current + eye_left.border_radius_next) / 2;
    eye_right.border_radius_current = 
        (eye_right.border_radius_current + eye_right.border_radius_next) / 2;
    
    // SPACING
    spacing_current = (spacing_current + spacing_next) / 2;
    
    // EYELIDS
    eye_left.eyelid_offset = (eye_left.eyelid_offset + eye_left.eyelid_offset_next) / 2;
    eye_right.eyelid_offset = (eye_right.eyelid_offset + eye_right.eyelid_offset_next) / 2;
}

void EyeAnimationEngine::apply_mood_effects() {
    // TIRED: desenhar triângulos sobre os olhos
    // ANGRY: desenhar triângulos inclinados
    // HAPPY: desenhar pálpebra inferior
    // CURIOUS: aumentar offset de altura
}

void EyeAnimationEngine::update_animations() {
    uint32_t now = get_time_ms();
    
    // AUTOBLINK
    if (animation.autoblink_enabled && now >= animation.blink_timer) {
        blink();
        animation.blink_timer = now + animation.blink_interval_ms + 
                               rand() % animation.blink_interval_variation_ms;
    }
    
    // IDLE
    if (animation.idle_enabled && now >= animation.idle_timer) {
        eye_left.x_next = rand() % container_width;
        eye_left.y_next = rand() % container_height;
        animation.idle_timer = now + animation.idle_interval_ms + 
                              rand() % animation.idle_variation_ms;
    }
    
    // FLICKER
    if (animation.h_flicker) {
        if (animation.flicker_alternate) {
            eye_left.x_current += animation.flicker_amplitude;
            eye_right.x_current += animation.flicker_amplitude;
        } else {
            eye_left.x_current -= animation.flicker_amplitude;
            eye_right.x_current -= animation.flicker_amplitude;
        }
        animation.flicker_alternate = !animation.flicker_alternate;
    }
    
    // Similar para V_FLICKER, LAUGH, CONFUSED, SWEAT...
}

void EyeAnimationEngine::render_eyes() {
    // Atualizar posição e tamanho dos objetos LVGL
    lv_obj_set_pos(eye_left_obj, eye_left.x_current, eye_left.y_current);
    lv_obj_set_size(eye_left_obj, eye_left.width_current, eye_left.height_current);
    
    lv_obj_set_pos(eye_right_obj, eye_right.x_current, eye_right.y_current);
    lv_obj_set_size(eye_right_obj, eye_right.width_current, eye_right.height_current);
    
    // Aplicar border-radius
    lv_obj_set_style_radius(eye_left_obj, eye_left.border_radius_current, 0);
    lv_obj_set_style_radius(eye_right_obj, eye_right.border_radius_current, 0);
}

void EyeAnimationEngine::render_eyelids() {
    // Usar shapes LVGL (triângulos) ou canvas para pálpebras
    // Posicionar baseado em eyelid_offset
}

void EyeAnimationEngine::render_sweat() {
    // Atualizar posição das gotas com interpolação
    // Aplicar velocidade vertical
}

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
    if (left) eye_left.is_open = true;
    if (right) eye_right.is_open = true;
}

void EyeAnimationEngine::blink(bool left, bool right) {
    close(left, right);
    open(left, right);
}

// ... implementar setters ...
```

---

## 🔗 Integração com `display.cpp`

### Opção 1: Substituir Imagens por Engine
```cpp
// Remover:
static lv_obj_t *icon_img = nullptr;
LV_IMG_DECLARE(eva_idle);
LV_IMG_DECLARE(eva_blink);
LV_IMG_DECLARE(eva_smile);

// Adicionar:
EyeAnimationEngine *eye_engine = nullptr;

// Em display_task():
eye_engine = new EyeAnimationEngine(top);
eye_engine->initialize(LV_PCT(100), LV_PCT(40), 30);  // 30 FPS
eye_engine->enable_autoblink(true, 3000, 1000);
eye_engine->enable_idle_mode(true, 2000, 1000);

// No loop:
case DISP_CMD_STATE_IDLE:
    eye_engine->set_mood(MOOD_DEFAULT);
    eye_engine->trigger_laugh();
    display_set_status("IDLE");
    break;

case DISP_CMD_STATE_LISTENING:
    eye_engine->set_mood(MOOD_CURIOUS);
    eye_engine->set_flicker(true, true, 3);
    display_set_status("LISTENING");
    break;

case DISP_CMD_STATE_SPEAKING:
    eye_engine->set_mood(MOOD_HAPPY);
    eye_engine->trigger_laugh();
    display_set_status("SPEAKING");
    break;

// Chamar a cada frame
eye_engine->update();
```

### Opção 2: Manter Imagens + Engine Complementar
```cpp
// Usar imagens como base
lv_image_set_src(icon_img, &eva_smile);

// Sobrepor animações da engine (partículas, pálpebras, etc)
eye_engine->render_eyelids();  // Render apenas eyelids
eye_engine->render_sweat();     // Render apenas sweat drops
```

---

## 📊 Vantagens da Abordagem

| Aspecto | Imagens Estáticas | Engine Dinâmica |
|--------|------------------|-----------------|
| **Suavidade** | Troca abrupta | Interpolação contínua |
| **Customização** | Fixa (pré-renderizada) | 100% paramétrica |
| **Tamanho Flash** | Múltiplas imagens | Uma classe C++ |
| **Expressões** | Limitadas a sprites | Infinitas combinações |
| **Performance** | Baixa (simples) | Média (mais cálculos) |
| **Escalabilidade** | Difícil | Fácil (resize parameters) |

---

## 🎯 Próximos Passos

1. **Criar `eye_animation_engine.h`** com estrutura de dados
2. **Implementar `eye_animation_engine.cpp`** com interpolação
3. **Adicionar suporte a LVGL shapes** (retângulos com raio, triângulos)
4. **Integrar com `display.cpp`** via commands de mood
5. **Testar com diferentes FPS** (20, 30, 60 Hz)
6. **Otimizar rendering** (dirty flags, batching)

---

## 💡 Notas de Implementação

- Use `lv_obj_t` para cada componente (olho, pálpebra)
- Interpolação: `current = (current + next) / 2`
- Timing baseado em `esp_timer_get_time()` convertido para ms
- Estados persistem entre frames
- Animações são flags que ativam lógicas no `update()`
- Considerar usar `lv_canvas` para desenho customizado se primitivas LVGL forem limitadas
