# LFR ESP32 — Robô Seguidor de Linha Competitivo

Firmware completo em C++17 para robô seguidor de linha baseado em ESP32, desenvolvido com TDD estrito (Red → Green → Refactor). Projetado para competições brasileiras (RoboCore, IronCup, Salão de Robótica).

**47 testes unitários • 0 warnings • loop de controle < 2 ms • BLE tuning em tempo real**

---

## Hardware

| Componente | Especificação |
|-----------|--------------|
| MCU | ESP32 DevKit V1 — dual-core 240 MHz |
| Sensores | 8× TCRT5000 via MCP3008 SPI (10-bit) |
| Motores | 2× N20 600 RPM com encoder Hall |
| Driver | TB6612FNG dual H-bridge |
| Bateria | LiPo 2S 7.4V 800 mAh 30C |
| Chassis | PETG impresso 3D, formato F1 |
| Regulador | MP1584 buck → 5V |

Custo estimado: **~R$ 423**. BOM completa em [`docs/RELATORIO_FINAL.md`](docs/RELATORIO_FINAL.md).

---

## Início Rápido

### Pré-requisitos

```bash
pip install platformio
pio platform install espressif32
```

### Compilar e subir firmware

```bash
pio run -e esp32dev --target upload
```

### Rodar testes (sem hardware)

```bash
pio test -e native
```

Saída esperada: `47 test cases: 47 succeeded`

---

## Estrutura do Projeto

```
├── src/
│   ├── config.h                  # Pinout, constantes, parâmetros PID
│   ├── main.cpp                  # Orquestração, máquina de estados, dual-core
│   ├── control/
│   │   ├── PIDController         # PID com derivada no processo e anti-windup
│   │   ├── SpeedProfile          # Velocidade adaptativa por erro
│   │   └── VelocityProfile       # SpeedProfile + predição de curvatura (buffer 8)
│   ├── sensors/
│   │   ├── SensorArray           # Leitura MCP3008 via SPI
│   │   └── Calibration           # Min/max por canal, posição ponderada [-3500,+3500]
│   ├── motors/
│   │   ├── MotorDriver           # TB6612FNG via LEDC (20 kHz, 10 bits)
│   │   └── DifferentialDrive     # Conversão correção → PWM esq/dir
│   ├── strategy/
│   │   ├── LineFollower          # Pipeline sensor→PID→motor
│   │   └── LapTimer             # Cronômetro de volta, melhor tempo
│   └── comm/
│       ├── BLETuner              # Ajuste PID/vel via BLE (NimBLE 2.x)
│       └── Logger                # Telemetria CSV via BLE (SPSC ring buffer)
│
├── test/                         # 8 suítes Unity, ambiente native
│   ├── test_pid/
│   ├── test_speed_profile/
│   ├── test_velocity_profile/
│   ├── test_calibration/
│   ├── test_motors/
│   ├── test_line_follower/
│   ├── test_lap_timer/
│   └── test_smoke/
│
├── hardware/chassis/             # OpenSCAD paramétrico
│   ├── body_f1.scad              # Chassis 180×120mm formato F1
│   ├── sensor_bar.scad           # Barra 8 sensores + suporte ajustável
│   └── wheel_hub.scad            # Hub O-ring silicone 34mm, eixo D 3mm
│
├── docs/
│   ├── dashboard/
│   │   ├── lfr-cockpit-mock.html   # Dashboard completo (BLE real + simulação WS)
│   │   ├── lfr_sim.py              # Simulador Python — replica protocolo ESP32
│   │   └── motion.js               # Motion.dev v11 bundle (offline)
│   ├── WIRING.md                   # Diagrama completo de fiação
│   ├── TUNING.md                   # Procedimento Ziegler-Nichols + checklist
│   ├── tuning_log.csv              # Log de sessões de tuning
│   └── RELATORIO_FINAL.md          # Relatório completo do projeto
│
└── tools/
    └── analyze_telemetry.py      # Análise pós-corrida: IAE/ISE/ITAE, plots
```

---

## Arquitetura de Software

### Máquina de estados

```
IDLE ──BTN──► CALIBRATING ──BTN──► READY ──BTN──► RUNNING
                                                      │
                                              erro crítico
                                                      ▼
                                                   ERROR
```

### Pipeline de controle (Core 0, < 2 ms)

```
SensorArray.read()        → raw[8]  (0–1023)
Calibration.normalize()   → norm[8] (0–1000)
Calibration.weightedPosition() → pos  (-3500 … +3500)
PIDController.compute(0, pos)  → correction
VelocityProfile.computeSpeed() → baseSpeed  (+ predição de curva)
DifferentialDrive.update()     → leftPwm, rightPwm
MotorDriver.setSpeed()         → LEDC PWM
```

### Dual-core FreeRTOS

| Core | Tarefa | Prioridade |
|------|--------|-----------|
| Core 0 | Sensor + PID + Motor | Tempo-real |
| Core 1 | BLE + Logger | Não-crítico |

---

## Módulos

### PIDController

Derivada calculada sobre a **medição** (não sobre o erro), eliminando spikes de derivativo quando o setpoint muda abruptamente. Anti-windup por clamping da integral.

```cpp
PIDController pid(Kp, Ki, Kd, outMin, outMax);
float correction = pid.compute(setpoint, measurement);
```

### VelocityProfile

Mantém um buffer circular de 8 leituras de posição. Calcula a taxa de variação média (curvature score) para antecipar curvas e reduzir velocidade antes de entrar nelas — não apenas reagir ao erro atual.

```cpp
VelocityProfile vp(minSpeed, baseSpeed, maxSpeed, threshold);
vp.update(position, timestampMs);
int speed = vp.computeSpeed();  // considera erro E curvatura
```

### LapTimer

Timestamps injetados externamente (testável sem `millis()`). Registra cada cruzamento da linha de chegada e mantém o melhor tempo.

```cpp
LapTimer lt;
lt.start(millis());
lt.notifyLineCrossing(millis());  // a cada cruzamento detectado
uint32_t best = lt.getBestLapMs();
```

### BLETuner

Ajuste de parâmetros e telemetria em tempo real via BLE (NimBLE-Arduino 2.x). Protocolo JSON de 2 características:

| Característica BLE | UUID | Direção | Formato |
|-------------------|------|---------|---------|
| Telemetry | `0xABCD` | ESP32 → App (Notify) | JSON `{"t":"info"/"tel", ...}` |
| Command   | `0xABCE` | App → ESP32 (Write)  | JSON `{"t":"pid"/"spd"/"start"/"stop"/"reset", ...}` |

**Telemetria** (30 Hz):
```json
{"t":"tel","pos":-350.0,"corr":12.4,"vL":168,"vR":152,
 "dt":1750,"s":[0,5,80,100,60,10,0,0],"bat":73.2,"lap":8.51,"laps":[8.51,9.03]}
```

**Comandos** (App → ESP32):
```json
{"t":"pid","kp":3.0,"ki":0.0,"kd":12.0}
{"t":"spd","base":160,"min":60,"max":230,"thrs":0.6}
{"t":"start"}  {"t":"stop"}  {"t":"reset"}
```

Conectar com o dashboard `docs/dashboard/lfr-cockpit-mock.html` ou testar offline com o simulador Python.

---

## Dashboard de Telemetria

Abrir `docs/dashboard/lfr-cockpit-mock.html` em Chrome/Edge (Web Bluetooth requer HTTPS ou localhost).

**Funcionalidades:**
- Conecta ao ESP32 por BLE (botão CONNECT) ou ao simulador Python via WebSocket
- Gráfico rolling 5 s: posição, correção, velocidades L/R
- Sliders PID (Kp, Ki, Kd) e velocidade (base, min, max, threshold) com envio ao vivo
- Barra de 8 sensores com brilho proporcional ao valor lido
- Cards: loop µs (alerta > 2 ms), erro, PWM médio, melhor volta
- LapTimer: histórico de últimas 5 voltas, destaque da melhor
- Bateria com indicador visual e alertas críticos
- Modo claro/escuro; layout responsivo (mobile swipe / desktop grid)
- Export de log CSV
- Animações de entrada via Motion.dev v11

**Simulador Python** (sem hardware):
```bash
cd docs/dashboard
python3 lfr_sim.py &          # WebSocket em ws://localhost:8766
python3 -m http.server 8765   # HTTP em http://localhost:8765
# Abrir: http://localhost:8765/lfr-cockpit-mock.html
```

---

## Chassi 3D

Exportar STLs com OpenSCAD instalado:

```bash
openscad -o hardware/chassis/body_f1.stl    hardware/chassis/body_f1.scad
openscad -o hardware/chassis/sensor_bar.stl hardware/chassis/sensor_bar.scad
openscad -o hardware/chassis/wheel_hub.stl  hardware/chassis/wheel_hub.scad
```

Imprimir em PETG. Configurações recomendadas em cada arquivo `.scad`.

---

## Calibração e Tuning

### Calibração de sensores

1. Posicionar o robô com sensores sobre a pista
2. Pressionar BTN (GPIO 0) → modo CALIBRATING
3. Varrer a linha devagar durante ~5 s
4. Pressionar BTN novamente → READY

### Tuning PID — Método Ziegler-Nichols

1. Zerar Ki e Kd via slider no dashboard
2. Incrementar Kp até o robô oscilar com amplitude constante → **Ku**
3. Medir período de oscilação no gráfico → **Tu** (ms)
4. Aplicar: `Kp = 0.8 × Ku`, `Kd = 0.125 × Tu`, `Ki = 0`
5. Aumentar velocidade base em 10 pts por sessão

Documentação completa: [`docs/TUNING.md`](docs/TUNING.md)  
Registrar cada sessão em: [`docs/tuning_log.csv`](docs/tuning_log.csv)

---

## Análise de Telemetria

```bash
# Análise básica (requer pandas)
python3 tools/analyze_telemetry.py corrida.csv

# Com gráficos (requer matplotlib)
python3 tools/analyze_telemetry.py corrida.csv --plot

# Exportar relatório Markdown
python3 tools/analyze_telemetry.py corrida.csv --report relatorio.md
```

Métricas calculadas: IAE, ISE, ITAE, loop timing p95, zero crossings, velocidade média.

---

## Testes

```bash
# Todos os testes nativos (sem hardware)
pio test -e native

# Suíte específica
pio test -e native -f test_pid
pio test -e native -f test_velocity_profile

# Testes no ESP32 (hardware conectado)
pio test -e esp32dev
```

| Suíte | Testes | Módulo |
|-------|--------|--------|
| test_pid | 9 | PIDController |
| test_speed_profile | 7 | SpeedProfile |
| test_velocity_profile | 4 | VelocityProfile |
| test_calibration | 10 | Calibration |
| test_motors | 6 | DifferentialDrive |
| test_line_follower | 6 | LineFollower |
| test_lap_timer | 4 | LapTimer |
| test_smoke | 1 | Framework |
| **Total** | **47** | |

---

## Fiação

Diagrama completo: [`docs/WIRING.md`](docs/WIRING.md)

**Resumo de conexões críticas:**

| ESP32 | Periférico |
|-------|-----------|
| GPIO 13/12/14/15 | MCP3008 (MOSI/MISO/CLK/CS) |
| GPIO 25/26/27 | TB6612FNG motor A (PWM/IN1/IN2) |
| GPIO 33/32/4 | TB6612FNG motor B (PWM/IN1/IN2) |
| GPIO 23 | TB6612FNG STBY |
| GPIO 18/19 | Encoder esquerdo A/B |
| GPIO 36/39 | Encoder direito A/B (input-only) |
| GPIO 0 | Botão START/CAL |
| GPIO 22 | Buzzer |
| GPIO 34 | Divisor VBAT |

---

## Regras de Competição

| Regra | Valor |
|-------|-------|
| Dimensões máx | 250 × 250 × 200 mm |
| Linha | Branca 19±1 mm sobre manta preta |
| Pista | Retas + curvas + cruzamentos, ~5 × 2.8 m |
| Tentativas | 3 de 3–5 min, menor tempo vence |
| Autonomia | 100% autônomo |

---

## Convenções de Código

- **Nomes**: `PascalCase` classes, `camelCase` métodos, `SCREAMING_SNAKE` constantes
- **Indentação**: 4 espaços, sem tabs
- **Sem `delay()`**: usar `millis()` ou timers de hardware
- **Sem `Serial.print()` em produção**: toda saída via `Logger`/BLE
- **Dependency Injection**: todos os módulos recebem dependências no construtor
- **Guards de plataforma**: `#ifndef NATIVE_BUILD` protege código Arduino em testes nativos

---

## Desenvolvimento

### Metodologia TDD

```
RED   → escrever teste que falha (define o comportamento esperado)
GREEN → implementar o mínimo para o teste passar
REFACTOR → limpar, sem quebrar testes
```

Ordem de desenvolvimento (bottom-up):
1. PIDController → SpeedProfile → VelocityProfile → Calibration
2. MotorDriver → DifferentialDrive → SensorArray
3. LineFollower → LapTimer → BLETuner → Logger
4. main.cpp — orquestra tudo com máquina de estados

### Build flags

```ini
[env:native]
build_flags = -Wall -Wextra -std=gnu++17 -DUNITY_INCLUDE_FLOAT -DNATIVE_BUILD

[env:esp32dev]
build_flags = -Wall -Wextra -std=gnu++17 -DUNITY_INCLUDE_FLOAT
```

---

## Referências

- [Semreh V2 — Campeão brasileiro (UFABC/Tamandutech)](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
- [Regras RoboCore — Seguidor de Linha](https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf)
- [PID Tuning Guide — Zbotic](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
- [PlatformIO ESP32](https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html)

---

**Autor**: Rafael Costa — 2026
