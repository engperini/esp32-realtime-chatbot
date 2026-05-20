#pragma once

#include "driver/ledc.h"
#include "driver/adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class ServoControl {
public:
    void begin(gpio_num_t servo_gpio, adc1_channel_t adc_channel);
    void goToZero();
    void update();                 // chama periodicamente (task)
    int  getLastPulse() const;

private:
    // ===== CONFIG =====
    static constexpr int SERVO_US_MIN = 600;
    static constexpr int SERVO_US_MAX = 2400;
    static constexpr int SERVO_DEAD_US = 10;
    static constexpr int FILTER_N = 8;

    static constexpr ledc_channel_t LEDC_CH = LEDC_CHANNEL_0;
    static constexpr ledc_timer_t   LEDC_TM = LEDC_TIMER_0;
    static constexpr ledc_mode_t    LEDC_MD = LEDC_LOW_SPEED_MODE;

    gpio_num_t _servo_gpio;
    adc1_channel_t _adc_channel;

    int _adc_buf[FILTER_N];
    int _buf_idx = 0;
    int _last_us = -1;

    int filterADC(int raw);
    uint32_t usToDuty(int us);
};
