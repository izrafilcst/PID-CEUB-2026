# CLAUDE.md — Projeto LFR (Line Follower Racer)

## Visão Geral

Robô seguidor de linha competitivo, otimizado para **velocidade máxima com precisão**, projetado para vencer competições brasileiras (RoboCore, IronCup, Salão de Robótica).

**Filosofia**: TDD em cada camada — nenhum módulo é integrado sem testes passando. Cada componente é validado isoladamente antes da integração.

---

## Stack Técnica

| Camada | Tecnologia |
|--------|-----------|
| MCU | ESP32 DevKit V1 (dual-core 240MHz, WiFi/BT) |
| Framework | Arduino (PlatformIO) |
| Linguagem | C++ (C++17) |
| Testes | Unity Test Framework (PlatformIO native) |
| Build | PlatformIO CLI |
| Sensores | 8× TCRT5000/QRE1113 analógico via MCP3008 (SPI) |
| Motores | 2× N20 600RPM 6V com encoder Hall |
| Driver | TB6612FNG dual H-bridge |
| Bateria | LiPo 2S 7.4V 800mAh 30C |
| Chassis | PETG impresso 3D, formato F1 |
| Regulador | MP1584 buck → 5V |

---

## Arquitetura do Software

```
src/
├── main.cpp                  # Entry point, setup/loop, orquestração
├── config.h                  # Constantes, pinout, tuning params
│
├── sensors/
│   ├── SensorArray.h/.cpp        # Leitura MCP3008, calibração, posição
│   ├── Calibration.h/.cpp        # Armazenamento min/max, normalização
│   └── Encoder.h/.cpp            # Quadratura 4× (Fase A — ISR ESP32)
│
├── control/
│   ├── PIDController.h/.cpp  # PID genérico (derivada no processo)
│   └── SpeedProfile.h/.cpp   # Velocidade adaptativa baseada em erro
│
├── motors/
│   ├── MotorDriver.h/.cpp    # Abstração TB6612FNG + PWM LEDC
│   └── DifferentialDrive.h/.cpp # Conversão correção → velocidades L/R
│
├── strategy/
│   ├── LineFollower.h/.cpp   # Orquestração: sensor→PID→motor
│   └── LapTimer.h/.cpp       # Cronômetro de volta, melhor tempo
│
└── comm/
    ├── BLETuner.h/.cpp       # Ajuste Kp/Ki/Kd/vel via BLE
    └── Logger.h/.cpp         # Telemetria via BLE (não Serial)

test/
├── test_pid/
│   └── test_pid.cpp          # Testes unitários do PID
├── test_sensors/
│   └── test_sensors.cpp      # Testes da leitura e calibração
├── test_motors/
│   └── test_motors.cpp       # Testes do driver e differential
├── test_speed_profile/
│   └── test_speed_profile.cpp # Testes da velocidade adaptativa
├── test_line_follower/
│   └── test_line_follower.cpp # Testes de integração
└── test_calibration/
    └── test_calibration.cpp  # Testes da calibração

hardware/
├── chassis/
│   ├── body_f1.stl           # Chassis principal formato F1
│   ├── sensor_bar.stl        # Barra de sensores (separada, ajustável)
│   ├── wheel_hub.stl         # Hub de roda para O-ring silicone
│   └── motor_bracket.stl     # Suporte dos motores N20
├── pcb/
│   └── shield_esp32.kicad    # PCB shield (opcional, pode ser perfboard)
└── bom.csv                   # Bill of Materials com links e preços

docs/
├── REGRAS.md                 # Regras resumidas das competições-alvo
├── TUNING.md                 # Log de ajustes PID (Kp, Kd, vel, resultado)
├── CALIBRATION.md            # Procedimento de calibração de sensores
└── WIRING.md                 # Diagrama de fiação completo
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

### Testes
- **Framework**: Unity (via PlatformIO `test/`)
- **Ambiente**: `native` para lógica pura (PID, calibração, perfil de velocidade)
- **Ambiente**: `esp32dev` para testes que precisam de hardware
- **Cobertura mínima**: 100% das funções de lógica de controle
- **Naming**: `test_<módulo>_<comportamento_esperado>`

### Hardware Constraints
- **Loop PID**: < 2 ms (>500 Hz)
- **Leitura sensores**: < 100 µs (MCP3008 SPI @ 1 MHz)
- **PWM motores**: 20 kHz, 10 bits (LEDC)
- **Core 0**: Sensor + PID + Motor (tempo real)
- **Core 1**: BLE + Logger (não-crítico)

---

## Parâmetros de Tuning (Defaults Iniciais)

```cpp
// config.h — valores iniciais, ajustáveis via BLE
#define KP_DEFAULT          3.0f
#define KI_DEFAULT          0.0f
#define KD_DEFAULT          12.0f
#define BASE_SPEED_DEFAULT  160     // 0-255
#define MAX_SPEED           230     // 0-255
#define MIN_SPEED           60      // velocidade em curva fechada
#define ERROR_THRESHOLD     0.6f    // acima disso, reduz velocidade
#define SENSOR_COUNT        8
#define SENSOR_SPACING_MM   10      // espaçamento entre sensores
#define SENSOR_HEIGHT_MM    4       // altura do solo
#define LOOKAHEAD_MM        70      // distância sensores → eixo motriz
```

---

## Metodologia TDD

### Red → Green → Refactor em cada módulo

1. **RED**: Escrever teste que falha (define comportamento esperado)
2. **GREEN**: Implementar o mínimo para o teste passar
3. **REFACTOR**: Limpar, otimizar, documentar

### Ordem de desenvolvimento (bottom-up)

```
Fase 1: Fundações (testes nativos, sem hardware)
  └─ PIDController → SpeedProfile → Calibration → SensorArray (mock)

Fase 2: Hardware Drivers (testes no ESP32)
  └─ MotorDriver → DifferentialDrive → SensorArray (real)

Fase 3: Integração (testes no ESP32 + hardware)
  └─ LineFollower → LapTimer → BLETuner

Fase 4: Tuning (na pista)
  └─ Calibração → PID tuning → Speed profiling → Competição
```

### Critério de "Done" por módulo
- [ ] Todos os testes unitários passando
- [ ] Sem warnings de compilação (-Wall -Wextra)
- [ ] Header documentado com Doxygen-style comments
- [ ] Revisão de performance (timing no ESP32)
- [ ] Integrado com módulo anterior sem regressão

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
