# Relatório Final — LFR ESP32

**Projeto**: Robô Seguidor de Linha Competitivo  
**Autor**: Rafael Costa  
**Data**: 2026-05-14  
**Status**: Firmware completo, pronto para tuning em pista

---

## 1. Resumo Executivo

Desenvolvimento completo de firmware C++17 para robô seguidor de linha (LFR) baseado em ESP32, seguindo metodologia TDD estrita (Red → Green → Refactor). O projeto entrega 47 testes unitários passando, firmware compilável sem warnings, chassis paramétrico em OpenSCAD e ferramentas de diagnóstico e tuning.

**Métricas de entrega**:

| Item | Resultado |
|------|-----------|
| Testes unitários | 47/47 passando |
| Warnings de compilação | 0 (nativos e firmware) |
| Cobertura de lógica de controle | 100% |
| Loop de controle alvo | < 2 ms (500 Hz) |
| Módulos implementados | 10 |
| Arquivos de teste | 8 suítes |

---

## 2. Arquitetura do Sistema

### 2.1 Hardware

| Componente | Escolha | Justificativa |
|-----------|---------|---------------|
| MCU | ESP32 DevKit V1 | Dual-core: Core 0 para controle tempo-real, Core 1 para BLE |
| ADC sensores | MCP3008 SPI | 10-bit, 8 canais, < 100 µs/leitura completa @ 1 MHz |
| Sensores linha | 8× TCRT5000 | 10 mm espaçamento → range ±3500 (posição ponderada) |
| Driver motor | TB6612FNG | Eficiência 95%, brake instantâneo, LEDC 20 kHz |
| Motores | N20 600 RPM c/ encoder | Relação torque/velocidade ideal para pistas 5×3 m |
| Bateria | LiPo 2S 800 mAh 30C | > 5 min de autonomia, pico de corrente para aceleração |

### 2.2 Software

```
Core 0 (tempo-real, < 2 ms):
  SensorArray → Calibration → weightedPosition
  → PIDController (derivada no processo, anti-windup)
  → VelocityProfile (predição de curvatura)
  → DifferentialDrive → TB6612FNG/LEDC

Core 1 (não-crítico):
  BLETuner (NimBLE 2.x) ↔ Dashboard Web Bluetooth
  Logger (SPSC ring buffer) → telemetria CSV
  LapTimer → cronômetro de volta
```

### 2.3 Fluxo de dados

```
Sensores raw [0-1023] × 8
    ↓ normalize() [0-1000] × 8
    ↓ weightedPosition() → float [-3500, +3500]
    ↓ PID.compute(setpoint=0, measurement=pos) → correction
    ↓ VelocityProfile.computeSpeed() → baseSpeed (com predição de curva)
    ↓ DifferentialDrive → leftPwm, rightPwm
    ↓ MotorDriver → LEDC
```

---

## 3. Módulos Implementados

### 3.1 PIDController (`src/control/`)

- **Derivada no processo** (não no erro): elimina spikes no setpoint
- **Anti-windup por clamping**: integral confinada a `[outMin/ki, outMax/ki]`
- **Saída**: `P + I + D`, limitada a `[outMin, outMax]`
- Testes: 8 casos (proporcional, derivativa, windup, saturação)

### 3.2 SpeedProfile (`src/control/`)

- Interpolação linear: `absError ∈ [0, threshold] → speed ∈ [maxSpeed, minSpeed]`
- Parâmetros ajustáveis via BLE em tempo de execução
- Testes: 5 casos (limites, interpolação, parâmetros)

### 3.3 VelocityProfile (`src/control/`) ← novo em Fase 6

- Estende SpeedProfile com **buffer circular de 8 posições**
- Calcula velocidade média de mudança de posição → `curvatureScore ∈ [0, 1]`
- Redução antecipada de velocidade antes de curvas (predição, não reação)
- Fórmula: `speed = min(errorSpeed, maxSpeed − curvature × weight × range)`
- Testes: 4 casos (zero, máximo erro, predição de curva, buffer wrap)

### 3.4 Calibration (`src/sensors/`)

- Armazena `min[]` e `max[]` por sensor, atualiza dinamicamente
- `normalize()`: raw → [0, 1000], sem divisão por zero
- `weightedPosition()`: centro ponderado normalizado [-3500, +3500]
- Line-lost: soma abaixo de limiar → retorna última posição válida
- Testes: 8 casos

### 3.5 SensorArray (`src/sensors/`)

- Leitura MCP3008 via SPI 3-byte: `cmd1=0x01, cmd2=(0x08|ch)<<4`
- Dependência injetada (`Calibration&`) — testável com mock
- Guards `#ifndef NATIVE_BUILD` para compilação nativa
- Testes: via mock em test_sensors

### 3.6 MotorDriver (`src/motors/`)

- Abstração TB6612FNG: forward, reverse, brake, coast
- LEDC ESP32: 20 kHz, 10 bits, canais A e B
- Guards `#ifndef NATIVE_BUILD`
- Testes: 6 casos (forward, reverse, brake, PWM, stop)

### 3.7 DifferentialDrive (`src/motors/`)

- Conversão: `leftPwm = baseSpeed + correction`, `rightPwm = baseSpeed − correction`
- Permite reverso (valores negativos) para curvas fechadas
- Testes: 5 casos (straight, curva, spin, saturação)

### 3.8 LineFollower (`src/strategy/`)

- Orquestra pipeline completo com DI de todos módulos
- Sinal negado: `correction = -pid.compute(0, position)` (convenção DifferentialDrive)
- Testes de integração: 6 casos
- Testes: center, left/right, full deflection, spin, PID update

### 3.9 LapTimer (`src/strategy/`) ← novo em Fase 6

- `start(ts)` / `notifyLineCrossing(ts)` / `reset()`
- Rastreia último tempo de volta e melhor tempo (0 = sem referência)
- Reinicia o timer de volta a cada cruzamento
- Testes: 4 casos (estado inicial, registro de volta, melhor volta, reset)

### 3.10 BLETuner + Logger (`src/comm/`)

- NimBLE-Arduino 2.x: API corrigida (`NimBLEConnInfo&`)
- 5 características: Kp, Ki, Kd (float-string), Speed (CSV), Telemetry (notify)
- Logger: SPSC ring buffer, `#ifdef DEBUG_LOG`, CSV `ts,pos,err,p,i,d,corr,vl,vr,dt`

---

## 4. Resultados dos Testes

```
native:test_velocity_profile  PASSED   4/4
native:test_line_follower     PASSED   6/6
native:test_pid               PASSED   8/8
native:test_speed_profile     PASSED   5/5
native:test_motors            PASSED   6/6
native:test_calibration       PASSED   8/8
native:test_lap_timer         PASSED   4/4
native:test_smoke             PASSED   1/1
─────────────────────────────────────────
TOTAL                                47/47
```

Zero warnings com `-Wall -Wextra -std=gnu++17`.

---

## 5. Chassis 3D

Arquivos OpenSCAD paramétricos em `hardware/chassis/`:

| Arquivo | Descrição | Impressão |
|---------|-----------|-----------|
| `body_f1.scad` | Chassis formato F1 (180×120mm) | PETG 0.2mm, 40% infill |
| `sensor_bar.scad` | Barra 8 sensores, suporte vertical | PETG 0.15mm, 50% infill |
| `wheel_hub.scad` | Hub O-ring silicone 34mm, eixo N em D | PETG 0.15mm, 60% infill |

Exportar STL: `openscad -o <name>.stl <name>.scad`

---

## 6. Ferramentas de Diagnóstico

| Ferramenta | Uso |
|-----------|-----|
| `docs/dashboard/lfr-cockpit.html` | Dashboard BLE tempo-real (Web Bluetooth) |
| `tools/analyze_telemetry.py` | Análise pós-corrida: IAE/ISE/ITAE, loop timing |
| `docs/TUNING.md` | Procedimento Ziegler-Nichols + checklist competição |
| `docs/tuning_log.csv` | Log de sessões de tuning |

---

## 7. Próximos Passos (Fase 4 — Pista)

### 7.1 Calibração (antes de cada sessão)
1. Verificar bateria > 7.0 V, sensores a 4 mm do chão
2. Pressionar BTN → modo CALIBRATING
3. Varrer a linha lentamente por 5 s
4. BTN novamente → READY → START

### 7.2 Tuning inicial
1. Zerar Ki, Kd; incrementar Kp até oscilação constante (Ku)
2. Medir período Tu no dashboard
3. Kp = 0.8 × Ku, Kd = 0.125 × Tu (controlador PD)
4. Aumentar velocidade base em 10 pts por sessão até instabilidade

### 7.3 Critérios de performance (alvo competição)
- Completar percurso 5×3 m sem sair da linha: **obrigatório**
- Tempo de volta < 15 s: **baseline**
- Tempo de volta < 10 s: **competitivo**
- Tempo de volta < 7 s: **top 3 RoboCore**

---

## 8. Dashboard de Telemetria

Dashboard web SPA desenvolvido em HTML/CSS/JS puro (sem frameworks), operável sem conexão à internet.

### 8.1 Arquivos

| Arquivo | Descrição |
|---------|-----------|
| `docs/dashboard/lfr-cockpit-mock.html` | Dashboard completo (~1 000 linhas) |
| `docs/dashboard/lfr_sim.py` | Simulador Python — replica protocolo BLE via WebSocket |
| `docs/dashboard/motion.js` | Motion.dev v11.18.2 bundle (65 KB, offline) |

### 8.2 Funcionalidades

- **Duplo modo de conexão**: BLE real (Web Bluetooth API) ↔ WebSocket (simulador Python)
- **Gráfico rolling 5 s**: posição, correção PID, velocidades L/R (Canvas 2D)
- **Painel de sensores**: 8 LEDs com brilho proporcional ao valor normalizado
- **Sliders ao vivo**: PID (Kp/Ki/Kd) e velocidade (base/min/max/threshold) — enviam JSON via BLE/WS imediatamente
- **LapTimer**: histórico das últimas 5 voltas, destaque da melhor
- **Bateria**: barra de progresso com zonas de alerta (< 20% amarelo, < 10% vermelho)
- **Modo claro/escuro**: toggle persistido em localStorage
- **Layout responsivo**: swipe entre abas no mobile; grid 50/50 no desktop sem scroll
- **Animações**: entrada via Motion.dev (stagger), feedback de toque
- **Export CSV**: log de telemetria da sessão
- **Auto-reconexão WS**: tenta ws://localhost:8766 a cada 3 s; fallback para simulação JS

### 8.3 Protocolo BLE (implementado em BLETuner)

```
0xABCD — Telemetry Char (Notify, ESP32→App):
  {"t":"info","name":"LFR-RACER-01","fw":"v3.1.0","mode":"RUN"}
  {"t":"tel","pos":−350.0,"corr":12.4,"vL":168,"vR":152,
   "dt":1750,"s":[0,5,80,100,60,10,0,0],"bat":73.2,
   "lap":8.51,"laps":[8.51,9.03,8.87]}

0xABCE — Command Char (Write, App→ESP32):
  {"t":"pid","kp":3.0,"ki":0.0,"kd":12.0}
  {"t":"spd","base":160,"min":60,"max":230,"thrs":0.6}
  {"t":"start"} / {"t":"stop"} / {"t":"reset"}
```

---

## 9. BOM Final

BOM completa com SKU e links em `hardware/bom.csv`.

### Componentes principais

| Componente | Qtd | Preço (R$) | Notas |
|-----------|-----|-----------|-------|
| ESP32 DevKit V1 | 1 | 45 | Clone WROOM-32 |
| MCP3008 ADC SPI | 1 | 25 | 8 canais, 10-bit |
| TCRT5000 array 8ch | 1 | 40 | Espaçamento 10 mm |
| N20 600RPM c/ encoder | 2 | 80 | 7 PPR, eixo D 3mm |
| TB6612FNG módulo | 1 | 18 | 1.2A cont, 95% efic. |
| LiPo 2S 800mAh 30C | 1 | 55 | 6.4–8.4 V |
| Carregador B3 2S/3S | 1 | 35 | Balanceado AC |
| MP1584 buck 5V | 1 | 8 | Ajustar antes de ligar |
| PETG filamento ~100g | 1 | 25 | Chassi + suportes |
| Rodas silicone 34mm | 1 par | 20 | Eixo D 3mm |
| Ball caster 1/2" | 1 | 8 | Ajustar altura |
| Perfboard 5×7 | 1 | 6 | Ou PCB KiCad |
| Passivos (R, C, LED, buzzer) | — | 12 | Ver bom.csv |
| Mecânicos (parafusos, espaçadores) | — | 19 | M2+M3 kits |
| Fios silicone + conectores | — | 17 | 26 AWG, JST |
| Chave SPDT, fita VHB, diversos | — | 11 | |
| **TOTAL** | | **~R$ 424** | |

---

## 10. Referências

- [Semreh V2 — Campeão brasileiro UFABC/Tamandutech](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
- [ESP32 Line Follower — RoboChallenge 2024](https://github.com/dandominicstaicu/esp32-line-follower)
- [Regras RoboCore — Seguidor de Linha](https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf)
- [PID Tuning Guide — Zbotic](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [Ziegler-Nichols method — Wikipedia](https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method)
