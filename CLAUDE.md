# CLAUDE.md — Projeto LFR (Line Follower Racer)

## Visão Geral

Robô seguidor de linha competitivo, otimizado para **velocidade máxima com precisão**, projetado para vencer competições brasileiras (RoboCore, IronCup, Salão de Robótica).

**Filosofia**: TDD em cada camada — nenhum módulo é integrado sem testes passando. Cada componente é validado isoladamente antes da integração.

**Estado atual**: firmware do `main` completo até a **Fase D** — controle em cascata (PID de posição externo + PID de RPM interno por motor), encoders em quadratura 4×, estimativa de velocidade com auto-calibração de PPR persistida em NVS, e tuner BLE estendido; timing de controle com **dt real (µs)** + tratamento de cruzamento. **91 testes nativos passando; firmware `src/` compila sem warnings (`-Wall -Wextra`).**

---

## Estado do Projeto por Branch

| Branch | Foco | Estado |
|--------|------|--------|
| `main` | Firmware ESP32 DevKit V1, cascade PID completo | **Estável — Fase A→D + timing/cruzamento, 91 testes** |
| `feat/esp32s3-port` | Port para ESP32-S3 N16R8 + simulador RL sim-to-real | **Pausada** (ver [decisão 2026-07-04](docs/DECISOES.md)) |

> **Decisão de 2026-07-04 ([docs/DECISOES.md](docs/DECISOES.md)):** o roadmap S3/RL/EDF está
> **congelado** até haver baseline medido na pista. Prioridade atual: montar o hardware do `main`
> e coletar tempos de volta reais. A branch S3 fica pausada (não abandonada) — o simulador é
> preservado como ativo, mas sai do caminho crítico.

O trabalho **canônico e testado** vive em `main`. A branch `feat/esp32s3-port` explora a próxima geração:
- **Port ESP32-S3 N16R8** (16 MB Flash, 8 MB PSRAM): lap mapping em PSRAM (Modo Corsa), inferência TFLite (Modo Offroad), downforce por EDF.
- **Simulador RL** (`simulator/`): PyBullet + Gymnasium + Stable-Baselines3 (PPO/SAC), com HAL portável Python→C++ e harness de vetores-golden que garante paridade numérica entre o PID/Cascade do firmware e o do simulador. Fases: **S1 concluída** (HAL + URDF + env), **S2a em andamento** (`reference_pid` + golden vectors de paridade).

Ao trabalhar em `main`, **não** assuma recursos exclusivos da branch S3 (PSRAM, TFLite, EDF, WiFi). Ao trabalhar na branch S3, preserve a portabilidade sim-to-real descrita em `simulator/CLAUDE.md`.

---

## Stack Técnica (main)

| Camada | Tecnologia |
|--------|-----------|
| MCU | ESP32 DevKit V1 (dual-core 240MHz, WiFi/BT) |
| Framework | Arduino (PlatformIO) |
| Linguagem | C++ (C++17) |
| Testes | Unity Test Framework (PlatformIO native) |
| Build | PlatformIO CLI |
| Sensores | 8× TCRT5000/QRE1113 analógico via MCP3008 (SPI) |
| Encoders | 2× Hall em quadratura 4× (N20) |
| Motores | 2× N20 600RPM 6V com encoder Hall |
| Driver | TB6612FNG dual H-bridge |
| Persistência | NVS (ESP32 Preferences) — PPR calibrado sobrevive a reboot |
| BLE | NimBLE-Arduino 2.x |
| Bateria | LiPo 2S 7.4V 800mAh 30C |
| Chassis | PETG impresso 3D, formato F1 |
| Regulador | MP1584 buck → 5V |

---

## Arquitetura do Software

```
src/
├── main.cpp                  # Orquestração, máquina de estados, dual-core FreeRTOS
├── config.h                  # Pinout, constantes, parâmetros de tuning
│
├── sensors/
│   ├── SensorArray.h/.cpp        # Leitura MCP3008 via SPI
│   ├── Calibration.h/.cpp        # min/max por canal, posição ponderada [-3500,+3500]
│   ├── Encoder.h/.cpp            # Quadratura 4× (Fase A — ISR ESP32)
│   └── VelocityEstimator.h/.cpp  # Encoder → RPM filtrado (IIR) + auto-cal PPR (Fase B)
│
├── control/
│   ├── PIDController.h/.cpp      # PID genérico (derivada no processo, anti-windup)
│   ├── SpeedProfile.h/.cpp       # Velocidade adaptativa por erro
│   ├── VelocityProfile.h/.cpp    # SpeedProfile + predição de curvatura (buffer 8)
│   └── CascadeController.h/.cpp  # PID posição (externo) → 2× PID RPM (interno) (Fase C)
│
├── motors/
│   ├── MotorDriver.h/.cpp        # Abstração TB6612FNG + PWM LEDC (20 kHz, 10 bits)
│   └── DifferentialDrive.h/.cpp  # Conversão correção → velocidades L/R
│
├── strategy/
│   ├── LineFollower.h/.cpp       # Pipeline sensor→PID→motor
│   └── LapTimer.h/.cpp           # Cronômetro de volta, melhor tempo
│
├── storage/
│   ├── IPersistentStore.h        # Interface chave-valor (DI para testabilidade)
│   ├── NvsStore.h/.cpp           # Implementação ESP32 (Preferences/NVS)
│   └── InMemoryStore.h           # Implementação native para testes
│
└── comm/
    ├── BLETuner.h/.cpp           # Ajuste PID/PIDv/RPM/cal via BLE + telemetria (Fase D)
    └── Logger.h/.cpp             # Telemetria via BLE (SPSC ring buffer, não Serial)

test/                             # Suítes Unity (ambiente native)
├── test_pid/                     # PIDController
├── test_speed_profile/           # SpeedProfile
├── test_velocity_profile/        # VelocityProfile
├── test_calibration/             # Calibration
├── test_encoder/                 # Encoder (quadratura)
├── test_velocity_estimator/      # VelocityEstimator + auto-cal PPR
├── test_cascade/                 # CascadeController
├── test_motors/                  # DifferentialDrive
├── test_line_follower/           # LineFollower (integração)
├── test_lap_timer/               # LapTimer
├── test_ble_tuner/               # Parsing de comandos BLE (Fase D)
├── test_smoke/                   # Sanidade do framework
└── test_sensors/                 # Hardware-only (test_ignore no ambiente native)

examples/                         # Sketches de bring-up de hardware (não é firmware)
├── sensor_raw_test.cpp           # Leitura crua MCP3008
├── sensor_calibration_test.cpp   # Verificação de calibração
├── motor_smoke_test.cpp          # Direção/sentido dos motores
├── differential_hw_test.cpp      # Differential drive na bancada
├── encoder_smoke_test.cpp        # Contagem de encoder
└── velocity_calibration_test.cpp # Auto-calibração de PPR

hardware/
├── chassis/                      # OpenSCAD paramétrico (body_f1, sensor_bar, wheel_hub)
└── bom.csv                       # Bill of Materials com links e preços

docs/
├── WIRING.md                     # Diagrama de fiação completo
├── TUNING.md                     # Procedimento Ziegler-Nichols + checklist
├── tuning_log.csv                # Log de sessões de tuning
├── RELATORIO_FINAL.md            # Relatório completo do projeto
├── dashboard/                    # Cockpit BLE (HTML) + simulador Python (WebSocket)
└── superpowers/                  # Planos e specs de cada fase (A–D, S1–S2a)
```

---

## Regras de Competição (Referência)

- **Dimensões máx**: 250 × 250 × 200 mm
- **Linha**: branca 19±1 mm sobre manta preta
- **Pista**: retas + curvas + cruzamentos, ~5 × 2.8 m
- **Tentativas**: 3 de 3-5 min, menor tempo vence
- **Autonomia**: 100% autônomo

---

## Convenções de Código

### Estilo
- **Nomes**: `PascalCase` para classes, `camelCase` para métodos/vars, `SCREAMING_SNAKE` para constantes
- **Headers**: include guards `#pragma once`
- **Comentários**: em português para domínio, inglês para API pública
- **Indentação**: 4 espaços, sem tabs
- **Linha máx**: 100 caracteres

### Padrões
- **Dependency Injection**: módulos recebem dependências no construtor (testabilidade)
- **Interface-first**: cada módulo tem header com interface clara antes da implementação
- **Sem globals mutáveis**: estado encapsulado em classes
- **Sem `delay()`**: usar millis() ou timers de hardware
- **Sem `Serial.print()` em produção**: toda saída via Logger/BLE, compilação condicional
- **Guards de plataforma**: `#ifndef NATIVE_BUILD` protege código Arduino/NimBLE nos testes nativos

### Testes
- **Framework**: Unity (via PlatformIO `test/`)
- **Ambiente**: `native` para lógica pura (PID, cascade, calibração, velocidade, encoders)
- **Ambiente**: `esp32dev` para testes que precisam de hardware (`test_sensors`)
- **Cobertura mínima**: 100% das funções de lógica de controle
- **Naming**: `test_<módulo>_<comportamento_esperado>`

### Hardware Constraints
- **Loop PID**: < 2 ms (>500 Hz)
- **Leitura sensores**: < 100 µs (MCP3008 SPI @ 1 MHz)
- **PWM motores**: 20 kHz, 10 bits (LEDC)
- **Core 0**: Sensor + PID + Cascade + Motor (tempo real)
- **Core 1**: BLE + Logger (não-crítico)

---

## Pipeline de Controle (Core 0, < 2 ms)

```
SensorArray.read()               → raw[8]  (0–1023)
Calibration.normalize()          → norm[8] (0–1000)
Calibration.weightedPosition()   → pos  (-3500 … +3500)
PIDController.compute(0, pos)     → correction               (PID externo — posição)
VelocityEstimator.getRPM() ×2    → rpmL, rpmR                (encoders → RPM filtrado)
CascadeController.compute(...)    → pwmL, pwmR                (2× PID interno — RPM)
MotorDriver.setSpeed()           → LEDC PWM
```

O `CascadeController` traduz `correction` + `baseRpm` em setpoints de RPM por roda
(`setpointL = base + correction`, `setpointR = base − correction`), e cada PID interno
fecha a malha contra o RPM medido pelo `VelocityEstimator`.

---

## Parâmetros de Tuning (config.h)

```cpp
// ── PID externo (posição da linha) — ajustável via BLE {"t":"pid"} ──
#define KP_DEFAULT          3.0f
#define KI_DEFAULT          0.0f
#define KD_DEFAULT          12.0f

// ── Velocidade (perfil em PWM, legado) — {"t":"spd"} ──
#define BASE_SPEED_DEFAULT  160     // 0-255
#define MAX_SPEED           230     // 0-255
#define MIN_SPEED           60      // velocidade em curva fechada
#define ERROR_THRESHOLD     0.6f    // acima disso, reduz velocidade

// ── Sensores ──
#define SENSOR_COUNT        8
#define SENSOR_SPACING_MM   10      // espaçamento entre sensores
#define SENSOR_HEIGHT_MM    4       // altura do solo
#define LOOKAHEAD_MM        70      // distância sensores → eixo motriz

// ── Encoder / Velocity (Fase B) ──
#define ENCODER_DEFAULT_PPR_X4   28.0f  // 4× PPR; auto-cal via BLE sobrescreve e salva em NVS
#define VELOCITY_FILTER_ALPHA    0.3f   // IIR low-pass (menor = mais suave, mais delay)

// ── Cascade PID interno (velocidade dos motores, Fase C) — {"t":"pidv"} / {"t":"rpm"} ──
#define KP_VEL_DEFAULT   0.8f
#define KI_VEL_DEFAULT   0.0f
#define KD_VEL_DEFAULT   0.05f
#define MAX_RPM_DEFAULT  1200.0f    // teto de clamp do setpoint
#define BASE_RPM_DEFAULT  600.0f    // alvo nominal em reta
#define RPM_HARD_CEILING 2000.0f    // teto físico absoluto p/ comandos BLE (anti-runaway)

// ── PWM LEDC / SPI / Timing ──
#define PWM_FREQ         20000
#define PWM_RESOLUTION      10
#define PWM_MAX           1023
#define SPI_FREQ        1000000
#define LOOP_PERIOD_US    2000
```

---

## Metodologia TDD

### Red → Green → Refactor em cada módulo

1. **RED**: Escrever teste que falha (define comportamento esperado)
2. **GREEN**: Implementar o mínimo para o teste passar
3. **REFACTOR**: Limpar, otimizar, documentar

### Fases de desenvolvimento (concluídas em `main`)

```
Fase 0/1: Fundações (testes nativos, sem hardware)
  └─ PIDController → SpeedProfile → VelocityProfile → Calibration → SensorArray

Fase 2: Drivers de hardware
  └─ MotorDriver → DifferentialDrive → SensorArray (real)

Fase A: Encoders em quadratura 4× (ISR ESP32)
Fase B: VelocityEstimator (RPM filtrado + auto-calibração de PPR em NVS)
Fase C: CascadeController (PID posição externo + PID RPM interno por motor)
Fase D: BLETuner estendido (pidv/rpm/cal + telemetria RPM) + dashboard cockpit

Fase 4: Tuning na pista
  └─ Calibração → PID tuning (Ziegler-Nichols) → Speed profiling → Competição
```

### Roadmap (branch `feat/esp32s3-port`)

```
Port ESP32-S3 N16R8: lap mapping em PSRAM (Corsa) + TFLite (Offroad) + EDF downforce
Simulador RL sim-to-real:
  └─ Fase S1 (HAL + URDF + env)  .............. concluída
  └─ Fase S2a (reference_pid + golden vectors)  em andamento
```

### Critério de "Done" por módulo
- [ ] Todos os testes unitários passando
- [ ] Sem warnings de compilação (-Wall -Wextra)
- [ ] Header documentado com Doxygen-style comments
- [ ] Revisão de performance (timing no ESP32)
- [ ] Integrado com módulo anterior sem regressão

---

## Comandos Úteis

```bash
pio test -e native                 # roda as 12 suítes nativas (esperado: 91 test cases succeeded)
pio test -e native -f test_cascade # uma suíte específica
pio run -e esp32dev --target upload  # compila e sobe firmware
pio test -e esp32dev               # testes que exigem hardware (test_sensors)
```

---

## BOM Resumida (R$423 estimado)

| Componente | Preço |
|-----------|-------|
| ESP32 DevKit V1 | R$45 |
| MCP3008 ADC | R$25 |
| 8ch TCRT5000 array | R$40 |
| 2× N20 600RPM encoder | R$80 |
| TB6612FNG módulo | R$18 |
| LiPo 2S 800mAh | R$55 |
| Carregador B3 | R$35 |
| MP1584 buck | R$8 |
| PETG filamento | R$25 |
| Rodas silicone 34mm | R$20 |
| Ball caster metálico | R$8 |
| Parafusos M2/M3 kit | R$15 |
| Fios/conectores | R$20 |
| Capacitores | R$5 |
| Chave SPDT | R$3 |
| Perfboard 5×7 | R$6 |
| Diversos | R$15 |
| **TOTAL** | **~R$423** |

---

## Links de Referência

- [Semreh V2 — Campeão brasileiro (UFABC/Tamandutech)](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
- [ESP32 Line Follower — RoboChallenge 2024](https://github.com/dandominicstaicu/esp32-line-follower)
- [PID Line Follower com ESP32](https://github.com/felipemmattia/PID_line_follower_robot)
- [Regras RoboCore — Seguidor de Linha](https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf)
- [PID Tuning Guide — Zbotic](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [Pololu Micro Metal Gearmotors](https://www.pololu.com/category/60/micro-metal-gearmotors)
