#include "display_status.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ==== CONFIG OLED ====
#define I2C_PORT I2C_NUM_0
#define OLED_ADDR 0x3C
#define OLED_WIDTH 128
#define OLED_HEIGHT 64

// Fonte 6x8 simplificada (exemplo)
static const uint8_t font6x8[][6] = {
    // só exemplo, você pode expandir
};

static void i2c_init() {
    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = GPIO_NUM_4;
    conf.scl_io_num = GPIO_NUM_5;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;
    i2c_param_config(I2C_PORT, &conf);
    i2c_driver_install(I2C_PORT, conf.mode, 0, 0, 0);
}

void DisplayStatus::begin() {
    i2c_init();
    // init SSD1306 (sequência padrão)
    // clear screen
    setStatus(BotStatus::BOOTING);
}

void DisplayStatus::setStatus(BotStatus status) {
    _status = status;
}

void DisplayStatus::setScrollingText(const std::string& text) {
    _scroll_text = text;
    _scroll_x = OLED_WIDTH;
}

void DisplayStatus::update() {
    drawStatus();
    drawScroll();
}

void DisplayStatus::drawStatus() {
    // limpa faixa superior
    // escreve texto conforme status
    // ex:
    // BOOTING → "Inicializando..."
    // CONNECTING → "Conectando WS..."
    // READY → "BOT PRONTO"
}

void DisplayStatus::drawScroll() {
    if (_scroll_text.empty()) return;

    // desenha texto na linha inferior usando _scroll_x
    _scroll_x -= 1;
    if (_scroll_x < -(int)(_scroll_text.length() * 6)) {
        _scroll_x = OLED_WIDTH;
    }
}
