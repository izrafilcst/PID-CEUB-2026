#pragma once

// ═══ PINAGEM ESP32 DevKit V1 ═══
#define PIN_SPI_MOSI        13
#define PIN_SPI_MISO        12
#define PIN_SPI_CLK         14
#define PIN_SPI_CS          15

#define PIN_MOTOR_A_PWM     25
#define PIN_MOTOR_A_IN1     26
#define PIN_MOTOR_A_IN2     27
#define PIN_MOTOR_B_PWM     33
#define PIN_MOTOR_B_IN1     32
#define PIN_MOTOR_B_IN2      4
#define PIN_MOTOR_STBY      23

// ═══ ENCODERS Hall (Fase A — leitura quadratura 4×) ════════════════════════
// GPIO 36 e 39 são input-only no ESP32 e não têm pull-up interno: use 10kΩ externo.
// Resolução: 4 transições por período × PPR efetivo (medido na Fase B).
#define PIN_ENC_LEFT_A      18
#define PIN_ENC_LEFT_B      19
#define PIN_ENC_RIGHT_A     36
#define PIN_ENC_RIGHT_B     39

#define PIN_BTN_START        0
#define PIN_BUZZER          22
#define PIN_LED_STATUS       2
#define PIN_VBAT_ADC        34

// ═══ PID DEFAULTS ═══
#define KP_DEFAULT          3.0f
#define KI_DEFAULT          0.0f
#define KD_DEFAULT          12.0f

// ═══ VELOCIDADE ═══
#define BASE_SPEED_DEFAULT  160
#define MAX_SPEED           230
#define MIN_SPEED            60
#define ERROR_THRESHOLD     0.6f

// ═══ SENSORES ═══
#define SENSOR_COUNT          8
#define SENSOR_SPACING_MM    10
#define SENSOR_HEIGHT_MM      4
#define LOOKAHEAD_MM         70

// ═══ ENCODER / VELOCITY (Fase B) ═══════════════════════════════════════════
// PPR_X4 default = 4 × PPR motor. N20 com encoder Hall típico = 7 PPR → 28 X4.
// Auto-calibração via BLE sobrescreve esse valor e salva em NVS.
#define ENCODER_DEFAULT_PPR_X4   28.0f
#define VELOCITY_FILTER_ALPHA    0.3f  // 0 ≤ α ≤ 1 — menor = mais suave, mais delay (α=0 congela)

// ═══ PWM LEDC ═══
#define PWM_FREQ         20000
#define PWM_RESOLUTION      10
#define PWM_MAX           1023

// ═══ SPI ═══
#define SPI_FREQ        1000000

// ═══ TIMING ═══
#define LOOP_PERIOD_US    2000
