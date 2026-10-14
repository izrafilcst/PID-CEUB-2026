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

// Ganhos, velocidades e demais parâmetros de sintonia: valores de fábrica em
// src/config/RobotConfig.h, ajustáveis por BLE e persistidos na NVS (spec §4.3).

// ═══ SENSORES ═══
#define SENSOR_COUNT          8
#define SENSOR_SPACING_MM    10
#define SENSOR_HEIGHT_MM      3
#define LOOKAHEAD_MM         85   // sensores → eixo motriz, modelo v2 (D12)

// ═══ ENCODER / VELOCITY ═══════════════════════════════════════════════════
// Contagens ×4 por volta do EIXO DE SAÍDA. 600 = encoder de 12 CPR no eixo do motor ×
// redução 50:1 (N20 de 600 RPM). PROVISÓRIO: a auto-calibração por BLE ({"t":"cal"}) é
// obrigatória antes de usar o modo cascata (F-17).
#define ENCODER_DEFAULT_PPR_X4   600.0f
// Faixa única do PPR: RobotConfig (pprL/pprR), ConfigStore e VelocityEstimator (A-WP1-04)
#define ENCODER_PPR_X4_MIN       1.0f
#define ENCODER_PPR_X4_MAX       10000.0f
#define VELOCITY_FILTER_ALPHA    0.3f  // 0 ≤ α ≤ 1 — menor = mais suave, mais atraso

// ═══ FALHA DE ENCODER NA CASCATA (E-4) ═════════════════════════════════════
// mode = 1 e uma roda com duty ≥ ENC_FAULT_DUTY × PWM_MAX e |RPM| < ENC_FAULT_RPM_FRAC × rpmMax
// por ENC_FAULT_MS contínuos → ERROR(ENC)
#define ENC_FAULT_DUTY       0.6f
#define ENC_FAULT_RPM_FRAC   0.05f
#define ENC_FAULT_MS         200

// ═══ BATERIA ═══════════════════════════════════════════════════════════════
// Divisor 100 kΩ / 47 kΩ → Vadc = Vbat × 47/147. 8,4 V → 2,69 V (dentro do ADC_11db).
#define VBAT_DIVIDER_RATIO  (147.0f / 47.0f)

// ═══ BUZZER ════════════════════════════════════════════════════════════════
#define BUZZER_LEDC_CHANNEL   2      // canais 0 e 1 são dos motores
#define BUZZER_FREQ_HZ     2500

// ═══ PWM LEDC ═══
#define PWM_FREQ         20000
#define PWM_RESOLUTION      10
#define PWM_MAX           1023

// ═══ SPI ═══
#define SPI_FREQ        1000000

// ═══ TIMING ═══
#define LOOP_PERIOD_US    2000
