# PROMPTS.md — Sequência TDD com Skills Superpowers + Karpathy

## Filosofia

### Skills Superpowers do Claude usadas neste projeto

| Skill | Onde usamos | Por quê |
|-------|------------|---------|
| **Code Execution** (bash/python) | Fases 0-3, 6 | Rodar testes Unity nativos, compilar, simular PID em Python |
| **File Creation** (.cpp, .h, .scad) | Todas | Gerar código, configs, hardware CAD |
| **Frontend Design** (.jsx artifact) | Fase 5 | Dashboard de telemetria React com Web Bluetooth |
| **Chart Display** | Fases 0, 1, 5, 6 | Visualizar resposta PID, step response, tuning, telemetria |
| **Data Analysis** (python) | Fases 0, 5, 6 | Simular PID antes de portar pra C++, análise de logs |
| **Web Search** | Fase 0, pontual | Consultar datasheets, projetos de referência |
| **xlsx** | Fase 5 | Planilha de tuning log estruturada |
| **Markdown/docs** | Todas | Documentação técnica, wiring, procedimentos |

### Receita Karpathy aplicada

Andrej Karpathy tem uma "receita" para treinar redes neurais que se aplica
perfeitamente a engenharia de controle. O mapeamento:

| Passo Karpathy | Fase LFR | Significado aqui |
|---------------|----------|-----------------|
| **1. Become one with the data** | Fase 0 | Entender profundamente o que cada sensor retorna, como o PID funciona matematicamente, ANTES de escrever código |
| **2. Set up end-to-end skeleton + dumb baseline** | Fase 1 | Pipeline completo sensor→PID→motor funcionando com valores hardcoded |
| **3. Overfit** | Fase 2 | Fazer funcionar PERFEITAMENTE em UMA reta, UMA curva |
| **4. Regularize** | Fase 3 | Tornar robusto: cruzamentos, perda de linha, variação de luz |
| **5. Tune** | Fase 5 | Ziegler-Nichols, ajuste fino na pista real |
| **6. Squeeze out the juice** | Fase 6 | Velocity profiling, predição de curva, shaving milissegundos |

---

## FASE 0 — "Become one with the data"

> **Karpathy Step 1**: Não escreva código de produção. Primeiro ENTENDA profundamente
> o domínio. Visualize. Simule. Só depois codifique.

### Prompt 0.1 — Simulação PID em Python

**Skills: Code Execution (Python) + Chart Display**

```
Antes de escrever qualquer C++, preciso ENTENDER o PID visualmente.

Use Code Execution (Python com matplotlib) para:

1. Implementar um PID simples em Python (~30 linhas)
2. Simular um robô seguindo uma linha com modelo simplificado:
   - Pista: função senoidal (reta → curva → reta → curva oposta)
   - Robô: posição atual, responde à correção com inércia (lowpass)
   - Sensores: retornam erro = posição_linha - posição_robô

3. Plotar 4 gráficos (subplots):
   a) Trajetória da linha vs trajetória do robô
   b) Erro ao longo do tempo
   c) Componentes P, I, D separadas ao longo do tempo
   d) Velocidades motor esquerdo/direito ao longo do tempo

4. Rodar 3 cenários e comparar:
   a) Apenas P (Kp=2, Ki=0, Kd=0) → mostra oscilação
   b) PD (Kp=2, Ki=0, Kd=8) → mostra amortecimento
   c) PID (Kp=2, Ki=0.01, Kd=8) → mostra correção de bias

5. Após gerar os gráficos, usar Chart Display para mostrar
   o comparativo dos 3 cenários inline no chat.

O objetivo é VISUALIZAR o efeito de cada termo antes de codificar.
Não pular direto pro C++ — esse passo constrói intuição.
```

### Prompt 0.2 — Simulação do sensor array em Python

**Skills: Code Execution (Python) + Chart Display**

```
Use Code Execution (Python) para simular a leitura dos sensores:

1. Modelar 8 sensores espaçados 10mm sobre uma linha de 19mm:
   - Cada sensor retorna um valor analógico (0-1023)
   - Valor máximo quando centrado na linha, cai com distância
   - Modelo gaussiano: valor = 1023 * exp(-(d/sigma)²)
     onde d = distância do centro do sensor ao centro da linha

2. Simular a linha em diferentes posições (-40mm a +40mm)
   e plotar:
   a) Heatmap: 8 sensores × posição da linha
   b) Posição calculada (média ponderada) vs posição real
   c) Erro da posição calculada vs real

3. Usar Chart Display para mostrar a curva de transferência
   (posição real → posição medida).

4. Testar com diferentes espaçamentos (8mm, 10mm, 12mm) e
   mostrar qual dá a melhor linearidade na zona de operação.

Insight esperado: "Com 10mm de espaçamento, a zona linear cobre
±25mm, suficiente para a linha de 19mm."
```

### Prompt 0.3 — Scaffolding PlatformIO

**Skills: File Creation + Code Execution (bash)**

```
Leia o CLAUDE.md deste projeto.

Use File Creation para criar toda a estrutura do projeto PlatformIO:

1. platformio.ini com:
   - [env:native] platform=native, para testes sem hardware
   - [env:esp32dev] board=esp32dev, framework=arduino
   - build_flags: -Wall -Wextra -std=gnu++17 -DUNITY_INCLUDE_FLOAT
   - lib_deps: Unity
   - test_build_src = true

2. src/config.h — todas constantes do CLAUDE.md
3. src/main.cpp — setup/loop vazios
4. Estrutura de pastas completa (sensors/, control/, motors/, etc.)
5. .gitignore

Use Code Execution (bash) para:
   $ pio run -e native
   → Verificar que compila sem erros.

   $ pio test -e native
   → Verificar framework funcional (criar test_smoke.cpp com TEST_ASSERT_TRUE(1))
```

---

## FASE 1 — "Dumb Baseline" + TDD da Lógica Pura

> **Karpathy Step 2**: "Set up end-to-end skeleton and get dumb baselines."
> Construir o pipeline inteiro com lógica testável. Sem hardware.
> Cada módulo: RED → GREEN → REFACTOR → VISUALIZAR.

### Prompt 1.1 — PIDController TDD

**Skills: File Creation + Code Execution (bash: pio test) + Chart Display**

```
Leia o CLAUDE.md.

═══ RED ═══
Use File Creation → test/test_pid/test_pid.cpp (Unity):
1. test_pid_zero_error_zero_output
2. test_pid_proportional_scales_linearly
3. test_pid_derivative_on_measurement_not_error
   → Setpoint muda: output NÃO deve ter spike
4. test_pid_derivative_reduces_overshoot
5. test_pid_integral_accumulates_over_time
6. test_pid_integral_windup_clamp
7. test_pid_output_clamped
8. test_pid_reset_clears_state
9. test_pid_symmetry → compute(0, +X) == -compute(0, -X)

Use Code Execution: `pio test -e native -f test_pid` → TODOS FALHAM (RED).

═══ GREEN ═══
Use File Creation → src/control/PIDController.h + PIDController.cpp
  - Derivada no PROCESSO (measurement), não no erro
  - Anti-windup por clamping
  - Output clamping

Use Code Execution: `pio test -e native -f test_pid` → TODOS PASSAM (GREEN).

═══ VISUALIZAR ═══
Use Code Execution (Python) para simular step response do PID.
Use Chart Display para mostrar inline:
  - Curva com Kp=3,Kd=12 (amortecido) vs Kp=3,Kd=0 (oscilatório)
Prova visual de que o D funciona.

═══ REFACTOR ═══
Revisar, renomear, documentar. Rodar testes de novo.
```

### Prompt 1.2 — SpeedProfile TDD

**Skills: File Creation + Code Execution + Chart Display**

```
Leia o CLAUDE.md. Rodar `pio test -e native` → confirmar anteriores passam.

═══ RED ═══
File Creation → test/test_speed_profile/test_speed_profile.cpp
1. test_zero_error_max_speed
2. test_small_error_high_speed
3. test_large_error_min_speed
4. test_linear_interpolation
5. test_symmetry_positive_negative
6. test_clamps_to_bounds
7. test_params_updateable_runtime

Code Execution: `pio test -e native -f test_speed` → FALHAM.

═══ GREEN ═══
File Creation → src/control/SpeedProfile.h/.cpp
Code Execution → PASSAM.

═══ VISUALIZAR ═══
Use Chart Display para plotar curva: erro (-4000..+4000) vs velocidade.
Mostra visualmente: rápido na reta, lento na curva.
```

### Prompt 1.3 — Calibration + PositionCalculator TDD

**Skills: File Creation + Code Execution + Chart Display**

```
Leia o CLAUDE.md. Confirmar testes anteriores passam.

═══ RED ═══
File Creation → test/test_calibration/test_calibration.cpp
1. test_initial_min_max_inverted
2. test_update_captures_extremes
3. test_normalize_0_to_1000
4. test_normalize_clamps_outliers
5. test_normalize_handles_zero_range (sem div/0)
6. test_reset_clears
7. test_weighted_position_center → posição ≈ 0
8. test_weighted_position_left → posição < -2000
9. test_weighted_position_right → posição > +2000
10. test_line_lost_returns_last_known

Code Execution → FALHAM → GREEN → PASSAM.

═══ VISUALIZAR ═══
Use Code Execution (Python): dados sintéticos de 8 sensores simulando
robô passando sobre linha. Alimentar no PositionCalculator.
Use Chart Display: barras de sensores + posição calculada vs real.
```

### Prompt 1.4 — DifferentialDrive TDD

**Skills: File Creation + Code Execution**

```
Leia o CLAUDE.md.

═══ RED ═══
File Creation → test/test_motors/test_differential.cpp
1. test_zero_correction_equal_speeds
2. test_positive_correction_turns_right
3. test_negative_correction_turns_left
4. test_clamps_to_pwm_range (±255)
5. test_allows_reverse_in_tight_curve
6. test_zero_base_spin_in_place

Code Execution → FALHAM → GREEN → PASSAM.
```

### Prompt 1.5 — Integração lógica end-to-end com mock

**Skills: File Creation + Code Execution + Chart Display**

```
Leia o CLAUDE.md. TODOS testes 1.1-1.4 passando.

═══ RED ═══
File Creation → test/test_line_follower/test_line_follower.cpp
Pipeline completo: raw[] → Calibration → Position → PID → SpeedProfile → Differential

1. test_straight_line → velocidades iguais e altas
2. test_slight_deviation → diferencial proporcional
3. test_sharp_curve → velocidade reduzida + forte diferencial
4. test_line_lost → mantém última direção
5. test_pid_update_changes_behavior
6. test_sequential_derivative

Code Execution → FALHAM → GREEN → PASSAM.

═══ VISUALIZAR (o mais importante da Fase 1) ═══
Use Code Execution (Python): simular CORRIDA COMPLETA.
  - Pista: reta 1m → curva 90° R=15cm → reta 0.5m → curva oposta
  - Modelo do robô com inércia
  - Pipeline LineFollower inteiro por 10 segundos

Use Chart Display:
  1) Vista de cima: trajetória robô vs pista
  2) Velocidade ao longo do tempo
  3) Erro ao longo do tempo
  4) Velocidades L/R

Karpathy Step 2 completo: baseline burra funciona end-to-end.
```

---

## FASE 2 — "Overfit" (Hardware Real)

> **Karpathy Step 3**: "Make it work perfectly on ONE example."
> Conectar hardware. Funcionar perfeitamente em UMA reta.

### Prompt 2.1 — MotorDriver no ESP32

**Skills: File Creation**

```
File Creation:
  src/motors/MotorDriver.h/.cpp (TB6612FNG + LEDC PWM)
  examples/motor_smoke_test.cpp (gira A frente/ré, B frente/ré)

Teste manual: flash e observar motores.
```

### Prompt 2.2 — SensorArray + MCP3008

**Skills: File Creation**

```
File Creation:
  src/sensors/SensorArray.h/.cpp (MCP3008 SPI, readAll < 100µs)
  examples/sensor_raw_test.cpp (CSV no Serial)
  examples/sensor_calibration_test.cpp (calibração + posição normalizada)

Teste manual: passar sobre linha, observar valores.
```

### Prompt 2.3 — Teste de movimento diferencial

**Skills: File Creation**

```
File Creation:
  examples/differential_hw_test.cpp
  (reto → curva dir → curva esq → spin CW → spin CCW → para)

Teste manual: observar 5 movimentos distintos.
```

---

## FASE 3 — "Regularize" (Integração Robusta)

> **Karpathy Step 4**: "Handle edge cases. Make it robust."

### Prompt 3.1 — main.cpp completo

**Skills: File Creation + Code Execution (compilação)**

```
File Creation → src/main.cpp definitivo:
  - Dependency injection de todos os módulos
  - setup(): calibração → buzzer → botão → começa
  - loop(): leitura → PID → motores (< 2ms)
  - Core 0: controle | Core 1: BLE
  - Estados: IDLE → CALIBRATING → READY → RUNNING → ERROR
  - Robustez: perda de linha, cruzamento, watchdog, low battery

Code Execution: `pio run -e esp32dev` → compila sem erros/warnings.
```

### Prompt 3.2 — BLE Tuner

**Skills: File Creation**

```
File Creation:
  src/comm/BLETuner.h/.cpp (BLE service, read/write Kp/Ki/Kd/speeds)
  examples/ble_test.cpp

Teste: nRF Connect, muda Kp, Serial confirma.
```

### Prompt 3.3 — Logger com telemetria

**Skills: File Creation**

```
File Creation:
  src/comm/Logger.h/.cpp
  (buffer circular 50 linhas, CSV via BLE notify, #ifdef DEBUG_LOG)
```

---

## FASE 4 — Hardware 3D

> **Karpathy**: "Your model is only as good as your data pipeline."

### Prompt 4.1 — Chassis paramétrico OpenSCAD

**Skills: File Creation + Code Execution (bash: openscad CLI)**

```
File Creation:
  hardware/chassis/body_f1.scad (F1 paramétrico)
  hardware/chassis/sensor_bar.scad
  hardware/chassis/wheel_hub.scad

Code Execution:
  $ openscad -o body_f1.stl body_f1.scad
  $ openscad -o sensor_bar.stl sensor_bar.scad
  $ openscad -o wheel_hub.stl wheel_hub.scad
  → STL exportam sem erros.

Comentários de fatiamento inclusos em cada .scad.
```

---

## FASE 5 — "Tune"

> **Karpathy Step 5**: "Tune, tune, tune."
> Aqui os skills do Claude brilham com máxima diversidade.

### Prompt 5.1 — Dashboard de telemetria

**Skill: Frontend Design (artifact .jsx)**

```
Leia o CLAUDE.md.
Use a skill Frontend Design para criar um artifact React (.jsx).

Dashboard estilo cockpit F1 (tema escuro, neon sutil):
1. Conexão Web Bluetooth com status
2. Sliders PID (Kp, Ki, Kd) com BLE write em tempo real
3. Sliders de velocidade (Base, Min, Max, Threshold)
4. Gráfico recharts LineChart rolling 5s:
   posição (cyan), correção (amarelo), vel L (verde), vel R (vermelho)
5. Barra visual 8 LEDs simulando sensores (preto→branco)
6. Indicadores: loop time, velocidade, melhor volta
7. Botões: Start, Stop, Calibrar, Reset PID, Dump Log
8. Modo simulação (dados fake) para testar sem hardware

Stack: React + Tailwind + recharts + lucide-react + Web Bluetooth API.
```

### Prompt 5.2 — Planilha de Tuning Log

**Skill: xlsx**

```
Use a skill xlsx para criar tuning_log.xlsx:

Aba 1 "Sessões": Data|Kp|Ki|Kd|Base|Min|Max|Threshold|Resultado|Tempo|Notas
  - Formatação condicional: tempo <10s verde, 10-15 amarelo, >15 vermelho
  - Header congelado, 20 linhas pré-formatadas

Aba 2 "Comparativo":
  - Gráfico dispersão: Kp vs Kd colorido por tempo
  - Gráfico barras: Top 5 configs

Aba 3 "Checklist Pré-Competição":
  - 8 itens com checkbox formatado
```

### Prompt 5.3 — Procedimento de Tuning

**Skill: File Creation (markdown)**

```
File Creation → docs/TUNING.md:
  - Checklist pré-pista
  - Procedimento calibração (5 passos)
  - Procedimento tuning Ziegler-Nichols modificado (10 passos)
  - Template de log (tabela markdown)
  - Troubleshooting (8 problemas comuns)
  - Quick Reference Card (10 linhas, cabe no bolso)
```

### Prompt 5.4 — Análise de telemetria pós-corrida

**Skills: Code Execution (Python + pandas) + Chart Display**

```
Após sessão de testes, Logger exporta CSV via BLE.

Code Execution (Python):
  1. Ler CSV (ts,pos,err,p,i,d,corr,vl,vr,dt)
  2. Métricas: MAE, erro máx, tempo loop médio, % em curva, velocidade média
  3. Detectar "quase perda de linha" (erro > 3000)
  4. Top 3 piores momentos (maior erro + timestamp)

Chart Display:
  1) Posição + erro ao longo da volta
  2) Histograma do erro (bias?)
  3) dt ao longo do tempo (spikes?)
  4) Velocidade estimada (onde freia?)

Output: "Pior momento em t=4.2s, curva direita. Sugestão: Kd +2, threshold -0.05."
```

---

## FASE 6 — "Squeeze the Juice"

> **Karpathy Step 6**: Cada milissegundo conta. Cada otimização é medida.

### Prompt 6.1 — LapTimer TDD

**Skills: File Creation + Code Execution**

```
RED → test/test_laptimer/ (4 testes)
GREEN → src/strategy/LapTimer.h/.cpp
Code Execution → PASSAM.

Detecção de volta: todos 8 sensores ativos > 50ms.
```

### Prompt 6.2 — Velocity Profiling avançado TDD

**Skills: File Creation + Code Execution + Chart Display**

```
RED → 3 testes adicionais em test_speed_profile:
  1. test_rate_of_change_predicts_curve
  2. test_ramp_up_after_curve
  3. test_history_buffer_rolling

GREEN → Evolução do SpeedProfile com buffer circular e predição.
Code Execution → PASSAM.

VISUALIZAR com Chart Display:
  V1 (sem predição) vs V2 (com predição) lado a lado.
  Velocidade, erro, tempo total.
  "V2 é 0.8s mais rápido: freia ANTES da curva."
```

### Prompt 6.3 — Relatório final

**Skills: File Creation (markdown) + Chart Display**

```
File Creation → docs/RELATORIO_FINAL.md:
  Resumo, arquitetura, resultados de testes, métricas,
  hardware, lições aprendidas, próximos passos.

Chart Display:
  - Evolução do melhor tempo ao longo das sessões
  - Comparativo configs PID vs tempo de volta
```

---

## MAPA DE SKILLS POR PROMPT

```
Prompt │ File  │ Code  │ Chart │ Front │ xlsx │
       │ Creat │ Exec  │ Displ │ Desig │      │
───────┼───────┼───────┼───────┼───────┼──────┤
 0.1   │       │  ██   │  ██   │       │      │  ← Simulação PID Python
 0.2   │       │  ██   │  ██   │       │      │  ← Simulação sensores
 0.3   │  ██   │  ██   │       │       │      │  ← Scaffolding PlatformIO
 1.1   │  ██   │  ██   │  ██   │       │      │  ← PID TDD + step response
 1.2   │  ██   │  ██   │  ██   │       │      │  ← SpeedProfile TDD + curva
 1.3   │  ██   │  ██   │  ██   │       │      │  ← Calibration TDD + visual
 1.4   │  ██   │  ██   │       │       │      │  ← Differential TDD
 1.5   │  ██   │  ██   │  ██   │       │      │  ← Integração mock + corrida
 2.1   │  ██   │       │       │       │      │  ← MotorDriver HW
 2.2   │  ██   │       │       │       │      │  ← SensorArray HW
 2.3   │  ██   │       │       │       │      │  ← Differential HW
 3.1   │  ██   │  ██   │       │       │      │  ← main.cpp completo
 3.2   │  ██   │       │       │       │      │  ← BLE Tuner
 3.3   │  ██   │       │       │       │      │  ← Logger
 4.1   │  ██   │  ██   │       │       │      │  ← Chassis OpenSCAD
 5.1   │       │       │       │  ██   │      │  ← Dashboard React
 5.2   │       │       │       │       │  ██  │  ← Planilha tuning
 5.3   │  ██   │       │       │       │      │  ← TUNING.md
 5.4   │       │  ██   │  ██   │       │      │  ← Análise telemetria
 6.1   │  ██   │  ██   │       │       │      │  ← LapTimer TDD
 6.2   │  ██   │  ██   │  ██   │       │      │  ← VelProfile TDD
 6.3   │  ██   │       │  ██   │       │      │  ← Relatório final
───────┼───────┼───────┼───────┼───────┼──────┤
Total  │  17×  │  13×  │  10×  │  1×   │  1×  │
```

**22 prompts. Cada um com skill(s) explícita(s). Cada um com entregável testável.**
