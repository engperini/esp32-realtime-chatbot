#include "servo_control.h"
#include "esp_log.h"

static const char *TAG = "ServoControl";

static constexpr int ADC_MAX = 4095;

void ServoControl::begin(gpio_num_t servo_gpio,
                         adc1_channel_t adc_channel)
{
    _servo_gpio = servo_gpio;
    _adc_channel = adc_channel;

    // ===== ADC =====
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(_adc_channel, ADC_ATTEN_DB_11);

    // inicializa buffer do filtro
    for (int i = 0; i < FILTER_N; i++) {
        _adc_buf[i] = adc1_get_raw(_adc_channel);
    }

    // ===== LEDC (PWM SERVO) =====
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_MD;
    timer.timer_num = LEDC_TM;
    timer.duty_resolution = LEDC_TIMER_14_BIT;
    timer.freq_hz = 50;               // servo = 50 Hz
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {};
    ch.gpio_num = _servo_gpio;
    ch.speed_mode = LEDC_MD;
    ch.channel = LEDC_CH;
    ch.timer_sel = LEDC_TM;
    ch.duty = usToDuty(SERVO_US_MIN);
    ch.hpoint = 0;
    ledc_channel_config(&ch);

    // ===== INICIALIZAÇÃO DECENTE =====
    goToZero();
    vTaskDelay(pdMS_TO_TICKS(700));

    ESP_LOGI(TAG, "Servo inicializado no GPIO %d", _servo_gpio);
}

void ServoControl::goToZero()
{
    uint32_t duty = usToDuty(SERVO_US_MIN);
    ledc_set_duty(LEDC_MD, LEDC_CH, duty);
    ledc_update_duty(LEDC_MD, LEDC_CH);
    _last_us = SERVO_US_MIN;
}

int ServoControl::filterADC(int raw)
{
    _adc_buf[_buf_idx++] = raw;
    if (_buf_idx >= FILTER_N) _buf_idx = 0;

    int sum = 0;
    for (int i = 0; i < FILTER_N; i++) {
        sum += _adc_buf[i];
    }
    return sum / FILTER_N;
}

uint32_t ServoControl::usToDuty(int us)
{
    // período = 20 ms → 50 Hz
    return (uint32_t)((us * 16384UL) / 20000UL);

}

void ServoControl::update()
{
    int raw = adc1_get_raw(_adc_channel);
    int filtered = filterADC(raw);

    int servo_us = SERVO_US_MIN +
        (filtered * (SERVO_US_MAX - SERVO_US_MIN)) / ADC_MAX;

    if (_last_us < 0 || abs(servo_us - _last_us) > SERVO_DEAD_US) {
        uint32_t duty = usToDuty(servo_us);
        ledc_set_duty(LEDC_MD, LEDC_CH, duty);
        ledc_update_duty(LEDC_MD, LEDC_CH);
        _last_us = servo_us;
    }
}

int ServoControl::getLastPulse() const
{
    return _last_us;
}
