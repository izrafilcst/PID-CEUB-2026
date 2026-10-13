# LFR v2 — WP1 Firmware Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Corrigir os defeitos F-01..F-17 e deixar o carro operando com toda a configuração
ajustável por BLE, validada e persistida na NVS — sem recompilar para trocar parâmetro, perfil,
modo de controle, sentido de motor ou polaridade da linha.

**Architecture:** A lógica sai do `main.cpp` para classes puras testadas no ambiente `native`
(`ControlLoop`, `RobotStateMachine`, `RobotConfig`, `ConfigStore`, `CommandProtocol`,
`CommandHandler`, `ButtonInput`, `BatteryMonitor`, `OutputStage`, `Annunciator`). O `main.cpp`
só faz a fiação: task `control` no core 1 a 500 Hz e task `comm` no core 0. Configuração e
telemetria cruzam os núcleos por cópia versionada (`Mailbox`); comandos de estado, por fila
FreeRTOS.

**Tech Stack:** C++17 (`gnu++17`), Arduino-ESP32 2.x via PlatformIO (`espressif32`),
NimBLE-Arduino 2.x, Unity (ambiente `native`).

**Spec:** `docs/superpowers/specs/2026-10-13-lfr-v2-retomada-design.md` (§3 e §4).
Plano mestre: `docs/superpowers/plans/2026-10-13-lfr-v2-00-mestre.md`.

## Global Constraints

- Flags `-Wall -Wextra -std=gnu++17`; **zero warnings vindos de `src/`**.
- Estilo do repo: `PascalCase` classes, `camelCase` métodos e variáveis, `SCREAMING_SNAKE`
  constantes, `#pragma once`, 4 espaços, linhas ≤ 100 colunas; comentários de domínio em
  português, comentários de API pública em inglês.
- Sem `delay()` e sem `Serial.print` em `src/` (os testes de bancada em `examples/` podem).
- Todo arquivo em `src/` compila no ambiente `native` (`test_build_src = yes`): código Arduino
  fica atrás de `#ifndef NATIVE_BUILD`.
- `pio test -e native` passa ao fim de **toda** tarefa.
- `pio run -e esp32dev` fica quebrado da Tarefa 2 até a Tarefa 18, porque o `main.cpp` antigo
  usa APIs que mudam. **Não editar `src/main.cpp` antes da Tarefa 18.**
- Faixas e valores de fábrica dos parâmetros: exatamente os da spec §4.3 (com `sinv`).
- Nenhuma escrita na NVS em ARMED ou RUNNING.
- Toda notificação BLE ≤ 180 bytes.
- Nome de teste: `test_<módulo>_<comportamento>`.
- Commits Conventional Commits, mensagem terminando com
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ruflo: início de cada tarefa → `mcp__claude-flow__hooks_pre-task`; fim → 
  `mcp__claude-flow__hooks_post-task` e `mcp__claude-flow__memory_store` (namespace
  `lfr-2026`, chave `wp1/status`, valor `"Tarefa N concluída: <commit>"`).

## Review Focus

1. **`set` com valor não finito, `v` ausente ou JSON truncado** (`{"t":"set","k":"kp"}`,
   `{"t":"set","k":"kp","v":nan}`) — a configuração fica intacta e volta um erro. Testes na
   Tarefa 8 (`test_robot_config_rejects_non_finite`) e na Tarefa 10
   (`test_command_protocol_set_without_value_fails`).
2. **Leitura de bateria 0 mV** (divisor solto) — sem divisão por zero na compensação e start
   recusado com ERROR(BAT). Testes nas Tarefas 12 (`test_battery_zero_reading_goes_low`),
   13 (`test_output_stage_invalid_vbat_disables_compensation`) e 14
   (`test_state_machine_start_with_low_battery_errors`).
3. **Blob da NVS gravado por firmware antigo ou corrompido** — volta para fábrica com aviso,
   nunca lixo. Tarefa 9 (`test_config_store_rejects_blob_of_other_size`,
   `test_config_store_rejects_out_of_range_blob`).
4. **Botão segurado continuamente** (inclusive desde o boot) — um único LONG e nenhum SHORT ao
   soltar. Tarefa 11 (`test_button_held_from_boot_fires_single_long`).
5. **Start com o carro fora da linha** — não sai em velocidade cheia e para em ERROR(LINE)
   depois de `lostMs`. Tarefa 16 (`test_control_loop_start_off_line_times_out`).

---

## Mapa de arquivos

| Arquivo | Ação | Tarefa |
|---------|------|--------|
| `docs/REVISAO_FIRMWARE.md` | Criar | 1 |
| `src/control/PIDController.h/.cpp` | Reescrever | 2 |
| `src/sensors/Calibration.h/.cpp` | Alterar | 3 |
| `src/strategy/LineFollower.h/.cpp` | Reescrever | 4 |
| `src/motors/DifferentialDrive.h/.cpp`, `test/test_motors/` | Remover | 4 |
| `src/control/SpeedProfile.h/.cpp` | Reescrever | 5 |
| `src/sensors/VelocityEstimator.h/.cpp`, `src/config.h` | Alterar | 6 |
| `src/control/Mixer.h` | Criar | 7 |
| `src/control/CascadeController.h/.cpp` | Reescrever | 7 |
| `src/config/RobotConfig.h/.cpp` | Criar | 8 |
| `src/storage/IPersistentStore.h`, `InMemoryStore.h`, `NvsStore.h/.cpp` | Alterar | 9 |
| `src/config/ConfigStore.h/.cpp` | Criar | 9 |
| `src/comm/SpinLock.h`, `src/comm/Telemetry.h` | Criar | 10 |
| `src/comm/CommandQueue.h/.cpp`, `src/comm/CommandProtocol.h/.cpp` | Criar | 10 |
| `src/sensors/ButtonInput.h/.cpp` | Criar | 11 |
| `src/sensors/BatteryMonitor.h/.cpp` | Criar | 12 |
| `src/motors/OutputStage.h/.cpp` | Criar | 13 |
| `src/strategy/RobotStateMachine.h/.cpp` | Criar | 14 |
| `src/comm/Annunciator.h/.cpp` | Criar | 15 |
| `src/strategy/ControlLoop.h/.cpp` | Criar | 16 |
| `src/comm/Mailbox.h`, `src/comm/CommandHandler.h/.cpp` | Criar | 17 |
| `src/comm/BLETuner.h/.cpp` | Reescrever | 17 |
| `src/main.cpp`, `src/config.h` | Reescrever | 18 |
| `platformio.ini`, `examples/*.cpp` | Alterar | 19 |
| `docs/dashboard/lfr-cockpit-mock.html`, `docs/dashboard/lfr_sim.py` | Alterar | 20 |
| `docs/dashboard/lfr-cockpit.html` | Remover | 20 |
| `README.md`, `docs/TUNING.md` | Alterar | 21 |

Testes novos ou reescritos: um diretório `test/test_<suite>/test_<suite>.cpp` por suíte, no
formato Unity já usado no repo (`setUp`/`tearDown` + `main()` com `UNITY_BEGIN`/`RUN_TEST`).

---

### Task 1: Relatório de revisão do firmware

**Executor:** agente revisor (`code-analyzer`). Só lê código e escreve o relatório; não altera
`src/` nem `test/`.

**Files:**
- Create: `docs/REVISAO_FIRMWARE.md`

**Interfaces:**
- Consumes: spec §3 (achados F-01..F-17) e §4.1 (itens obrigatórios).
- Produces: `docs/REVISAO_FIRMWARE.md`, com IDs F-xx que as tarefas seguintes citam nos
  commits; memória ruflo `lfr-2026/wp1/achados`.

- [ ] **Step 1: Registrar a linha de base**

Run: `pio test -e native 2>&1 | tail -20`
Expected: `82 test cases: 82 succeeded`. Copiar a linha de resumo para a seção "Linha de base".

- [ ] **Step 2: Confirmar cada achado F-01..F-17 no código**

Para cada linha da tabela da spec §3, abrir o arquivo e a linha citados, confirmar o defeito e
escrever o cenário de falha concreto. Para F-03, abrir https://www.pololu.com/product/960 e
https://www.pololu.com/docs/0J12 e citar o trecho que diz se a tensão de saída do QTR-8A
**sobe ou desce** com a refletância. Se a fonte não abrir, registrar "a confirmar em bancada
com `pio run -e bench_sensor_raw`" (Tarefa 19).

- [ ] **Step 3: Verificar os itens obrigatórios da spec §4.1 e achados novos**

Checar e registrar, cada um com arquivo:linha:
1. Pinos de *strapping* em uso — GPIO 0 (botão), 2 (LED), 4 (BIN2), 12 (MISO do MCP3008:
   nível alto no boot seleciona flash de 1,8 V), 15 (CS do MCP3008). Para cada um: o risco no
   boot e a mitigação (ex.: resistor de pull-down no GPIO 12 ou eFuse `VDD_SDIO`).
2. ISRs do encoder com `IRAM_ATTR` na definição e sem `float` dentro da ISR
   (`src/sensors/Encoder.cpp`).
3. Pilha das tasks (`xTaskCreatePinnedToCore` com 8192) frente ao uso de `snprintf` com `%f`.
4. `NimBLECharacteristic::setValue(buf)` com `buf` sendo `char[128]`: confirmar no código do
   NimBLE-Arduino 2.x (`.pio/libdeps/esp32dev/NimBLE-Arduino/src/NimBLEAttValue.h`) se o
   template envia `sizeof(buf)` bytes (lixo após o `\0`). Se sim, registrar como achado novo.
5. Tamanho real das mensagens BLE atuais frente ao limite de 180 bytes e ao MTU.
6. Acesso a dados compartilhados entre núcleos (`g_baseRpm`, `lineFollower`, `pid`, `velL`).
7. Dashboards: `docs/dashboard/lfr-cockpit-mock.html` **não tem Web Bluetooth** (só WebSocket
   para o simulador) e `docs/dashboard/lfr-cockpit.html` usa um protocolo BLE antigo (uma
   característica por parâmetro e telemetria em CSV). Registrar como achado: hoje nenhum
   dashboard fala com o robô. Decisão já tomada no plano: a Tarefa 20 leva o BLE para o
   `lfr-cockpit-mock.html` e remove o `lfr-cockpit.html`.
8. `src/comm/Logger.h/.cpp` e `src/control/VelocityProfile.h/.cpp`: confirmar que ficam sem uso
   depois deste plano e registrar como INFO.

- [ ] **Step 4: Levantar o regulamento**

Tentar, nesta ordem: https://www.robocore.net/eventos/regras (procurar "Seguidor de Linha"),
https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf.
Registrar, com citação e URL: (a) se o robô precisa parar sozinho depois da chegada e em que
distância/tempo; (b) formato e posição das marcações laterais; (c) regras sobre comunicação
sem fio durante a tomada de tempo e sobre ajustes entre tentativas. Se nenhuma fonte abrir,
escrever "não verificado" e listar como atividade pendente.

- [ ] **Step 5: Escrever `docs/REVISAO_FIRMWARE.md`**

Estrutura obrigatória:

```markdown
# Revisão do firmware — LFR v2

**Data**: 2026-10-13 · **Commit revisado**: <hash> · **Revisor**: agente code-analyzer (ruflo)

## Linha de base
<saída do pio test>

## Achados

| ID | Sev. | Arquivo:linha | Defeito | Cenário de falha | Correção | Requisito / Tarefa |
|----|------|---------------|---------|------------------|----------|--------------------|
| F-01 | CRIT | `src/main.cpp:227` … | … | … | … | Tarefa 4 |
…

## Itens obrigatórios (spec §4.1)
<um parágrafo curto por item do Step 3>

## Regulamento
<resultado do Step 4>

## Fora do escopo desta versão
<achados que não serão corrigidos e por quê>
```

Severidade: CRIT (impede correr ou arrisca dano), HIGH (degrada desempenho ou operação sem
contorno simples), MED, LOW, INFO. Achados novos continuam a numeração a partir de F-18.

- [ ] **Step 6: Publicar na memória ruflo e commitar**

`mcp__claude-flow__memory_store` com `namespace: "lfr-2026"`, `key: "wp1/achados"`, valor =
tabela de achados (só ID, severidade, uma linha de descrição).

```bash
git add docs/REVISAO_FIRMWARE.md
git commit -m "docs(review): firmware review report for LFR v2

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: PIDController com dt real, filtro no D e reset sem tranco

Resolve F-07 e F-12.

**Files:**
- Modify: `src/control/PIDController.h`, `src/control/PIDController.cpp`
- Modify (transição): `src/control/CascadeController.cpp`, `src/strategy/LineFollower.cpp`,
  `test/test_cascade/test_cascade.cpp`
- Test: `test/test_pid/test_pid.cpp`

**Interfaces:**
- Produces:
  - `PIDController(float kp, float ki, float kd, float outMin, float outMax)`
  - `float compute(float setpoint, float measurement, float dtS)`
  - `void reset(float measurement = 0.0f)`
  - `void setTunings(float kp, float ki, float kd)`
  - `void setDerivativeFilter(float alpha)` — `alpha` em (0, 1]; 1 = sem filtro
  - `void setOutputLimits(float outMin, float outMax)`
  - `float getKp() const`, `getKi()`, `getKd()`
- O `compute` de 2 argumentos e o parâmetro `dt` do construtor deixam de existir.

- [ ] **Step 1: Reescrever o teste**

`test/test_pid/test_pid.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "control/PIDController.h"

void setUp() {}
void tearDown() {}

static constexpr float DT = 0.01f;

void test_pid_zero_error_zero_output() {
    PIDController pid(1.0f, 0.0f, 0.0f, -255.0f, 255.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.compute(0.0f, 0.0f, DT));
}

void test_pid_proportional_scales_linearly() {
    PIDController a(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    PIDController b(2.0f, 0.0f, 0.0f, -1000.0f, 1000.0f);
    const float out1 = a.compute(10.0f, 0.0f, DT);
    const float out2 = b.compute(20.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, out1 * 2.0f, out2);
}

void test_pid_derivative_on_measurement_not_error() {
    PIDController pid(1.0f, 0.0f, 10.0f, -1000.0f, 1000.0f);
    pid.compute(0.0f, 0.0f, DT);
    const float before = pid.compute(0.0f, 0.0f, DT);
    const float after = pid.compute(100.0f, 0.0f, DT);  // setpoint salta, medida parada
    TEST_ASSERT_FLOAT_WITHIN(0.001f, before + 100.0f, after);
}

void test_pid_derivative_reduces_overshoot() {
    PIDController pid(1.0f, 0.0f, 5.0f, -1000.0f, 1000.0f);
    const float out1 = pid.compute(100.0f, 0.0f, DT);
    const float out2 = pid.compute(100.0f, 90.0f, DT);
    TEST_ASSERT_TRUE(out2 < out1);
}

void test_pid_integral_accumulates_over_time() {
    PIDController pid(0.0f, 1.0f, 0.0f, -10000.0f, 10000.0f);
    const float out1 = pid.compute(10.0f, 0.0f, DT);
    const float out2 = pid.compute(10.0f, 0.0f, DT);
    const float out3 = pid.compute(10.0f, 0.0f, DT);
    TEST_ASSERT_TRUE(out3 > out2);
    TEST_ASSERT_TRUE(out2 > out1);
}

void test_pid_integral_windup_clamp() {
    PIDController pid(0.0f, 1.0f, 0.0f, -100.0f, 100.0f);
    for (int i = 0; i < 1000; i++) pid.compute(10.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, pid.compute(10.0f, 0.0f, DT));
}

void test_pid_output_clamped() {
    PIDController pid(100.0f, 0.0f, 0.0f, -50.0f, 50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 50.0f, pid.compute(100.0f, 0.0f, DT));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, -50.0f, pid.compute(-100.0f, 0.0f, DT));
}

void test_pid_reset_clears_state() {
    PIDController pid(0.0f, 1.0f, 1.0f, -1000.0f, 1000.0f);
    pid.compute(10.0f, 5.0f, DT);
    pid.compute(10.0f, 6.0f, DT);
    pid.reset();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, pid.compute(0.0f, 0.0f, DT));
}

void test_pid_symmetry() {
    PIDController a(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    PIDController b(2.0f, 0.1f, 5.0f, -1000.0f, 1000.0f);
    const float pos = a.compute(0.0f, -10.0f, DT);
    const float neg = b.compute(0.0f, 10.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -pos, neg);
}

// F-12: reset alinhado à medida atual não gera pico de derivada no 1º ciclo
void test_pid_reset_with_measurement_avoids_derivative_kick() {
    PIDController pid(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    pid.reset(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(0.0f, 0.5f, 0.002f));
}

void test_pid_derivative_filter_halves_first_step() {
    PIDController pid(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    pid.setDerivativeFilter(0.5f);
    pid.reset(0.0f);
    // derivada bruta = -(0,01 - 0) / 0,01 = -1; filtrada com alpha 0,5 = -0,5
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -0.5f, pid.compute(0.0f, 0.01f, 0.01f));
}

// F-07: o termo D usa o dt informado em cada chamada
void test_pid_derivative_uses_given_dt() {
    PIDController fast(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    PIDController slow(0.0f, 0.0f, 1.0f, -1000.0f, 1000.0f);
    fast.reset(0.0f);
    slow.reset(0.0f);
    const float outFast = fast.compute(0.0f, 0.1f, 0.002f);
    const float outSlow = slow.compute(0.0f, 0.1f, 0.004f);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f * outSlow, outFast);
}

void test_pid_nonpositive_dt_freezes_integral_and_derivative() {
    PIDController pid(1.0f, 1.0f, 1.0f, -100.0f, 100.0f);
    pid.reset(0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, pid.compute(1.0f, 0.0f, 0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, pid.compute(1.0f, 0.0f, -0.01f));
}

void test_pid_non_finite_input_returns_zero() {
    PIDController pid(1.0f, 0.0f, 0.0f, -100.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(NAN, 0.0f, DT));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, pid.compute(0.0f, INFINITY, DT));
}

void test_pid_conditional_integration_stops_when_saturated() {
    PIDController pid(10.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    pid.reset(0.0f);
    for (int i = 0; i < 100; i++) pid.compute(1.0f, 0.0f, DT);  // P = 10 satura
    // sem integral acumulada, a saída acompanha o P assim que o erro troca de sinal
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -0.5f, pid.compute(-0.05f, 0.0f, DT));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_pid_zero_error_zero_output);
    RUN_TEST(test_pid_proportional_scales_linearly);
    RUN_TEST(test_pid_derivative_on_measurement_not_error);
    RUN_TEST(test_pid_derivative_reduces_overshoot);
    RUN_TEST(test_pid_integral_accumulates_over_time);
    RUN_TEST(test_pid_integral_windup_clamp);
    RUN_TEST(test_pid_output_clamped);
    RUN_TEST(test_pid_reset_clears_state);
    RUN_TEST(test_pid_symmetry);
    RUN_TEST(test_pid_reset_with_measurement_avoids_derivative_kick);
    RUN_TEST(test_pid_derivative_filter_halves_first_step);
    RUN_TEST(test_pid_derivative_uses_given_dt);
    RUN_TEST(test_pid_nonpositive_dt_freezes_integral_and_derivative);
    RUN_TEST(test_pid_non_finite_input_returns_zero);
    RUN_TEST(test_pid_conditional_integration_stops_when_saturated);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_pid`
Expected: FAIL de compilação (`setDerivativeFilter` não existe; `reset` não aceita argumento).

- [ ] **Step 3: Implementar**

`src/control/PIDController.h`:

```cpp
#pragma once

/**
 * PID genérico com derivada na medição.
 *
 * - dt real em segundos a cada chamada (F-07).
 * - Filtro passa-baixa de 1ª ordem no termo D: alpha = 1 desliga o filtro.
 * - reset(medida) alinha a memória da derivada à medida atual (F-12).
 * - Anti-windup: integração condicional (não integra enquanto a saída já está saturada no
 *   mesmo sentido do erro) + clamp da integral.
 */
class PIDController {
public:
    PIDController(float kp, float ki, float kd, float outMin, float outMax);

    /** Computes the output. Non-finite inputs return 0; dtS <= 0 freezes I and D. */
    float compute(float setpoint, float measurement, float dtS);

    /** Clears integral and D filter; aligns derivative memory to `measurement`. */
    void reset(float measurement = 0.0f);

    void setTunings(float kp, float ki, float kd);
    /** alpha in (0, 1]; 1 = no filtering. Out-of-range values are clamped. */
    void setDerivativeFilter(float alpha);
    void setOutputLimits(float outMin, float outMax);

    float getKp() const { return _kp; }
    float getKi() const { return _ki; }
    float getKd() const { return _kd; }

private:
    float _kp, _ki, _kd;
    float _outMin, _outMax;
    float _alpha = 1.0f;
    float _integral = 0.0f;
    float _prevMeasurement = 0.0f;
    float _dFiltered = 0.0f;

    static float _clamp(float v, float lo, float hi);
};
```

`src/control/PIDController.cpp`:

```cpp
#include "control/PIDController.h"
#include <algorithm>
#include <cmath>

PIDController::PIDController(float kp, float ki, float kd, float outMin, float outMax)
    : _kp(kp), _ki(ki), _kd(kd), _outMin(outMin), _outMax(outMax) {}

float PIDController::compute(float setpoint, float measurement, float dtS) {
    if (!std::isfinite(setpoint) || !std::isfinite(measurement)) return 0.0f;
    const bool dtOk = std::isfinite(dtS) && dtS > 0.0f;
    const float error = setpoint - measurement;

    // Derivada na medição, filtrada — não reage a saltos de setpoint
    if (dtOk) {
        const float dRaw = -(measurement - _prevMeasurement) / dtS;
        _dFiltered = _alpha * dRaw + (1.0f - _alpha) * _dFiltered;
    }
    _prevMeasurement = measurement;

    const float p = _kp * error;
    const float d = _kd * _dFiltered;

    if (_ki > 0.0f && dtOk) {
        const float unsat = p + _ki * _integral + d;
        const bool pushesHigh = unsat >= _outMax && error > 0.0f;
        const bool pushesLow  = unsat <= _outMin && error < 0.0f;
        if (!pushesHigh && !pushesLow) _integral += error * dtS;
        _integral = _clamp(_integral, _outMin / _ki, _outMax / _ki);
    }

    return _clamp(p + _ki * _integral + d, _outMin, _outMax);
}

void PIDController::reset(float measurement) {
    _integral = 0.0f;
    _dFiltered = 0.0f;
    _prevMeasurement = std::isfinite(measurement) ? measurement : 0.0f;
}

void PIDController::setTunings(float kp, float ki, float kd) {
    if (ki <= 0.0f) _integral = 0.0f;  // integral velha não vale com Ki novo nulo
    _kp = kp;
    _ki = ki;
    _kd = kd;
}

void PIDController::setDerivativeFilter(float alpha) {
    if (!std::isfinite(alpha)) return;
    _alpha = _clamp(alpha, 0.01f, 1.0f);
}

void PIDController::setOutputLimits(float outMin, float outMax) {
    _outMin = outMin;
    _outMax = outMax;
}

float PIDController::_clamp(float v, float lo, float hi) {
    return std::max(lo, std::min(hi, v));
}
```

- [ ] **Step 4: Ajustar os chamadores (transição até as Tarefas 4 e 7)**

Em `src/control/CascadeController.cpp`, trocar as duas chamadas:

```cpp
    float outL = _pidL.compute(setpointL, actualRpmL, 0.002f);  // Tarefa 7 passa o dt real
    float outR = _pidR.compute(setpointR, actualRpmR, 0.002f);  // Tarefa 7 passa o dt real
```

Em `src/strategy/LineFollower.cpp`:

```cpp
    float correction = -_pid.compute(0.0f, position, 0.01f);  // Tarefa 4 reescreve
```

Em `test/test_cascade/test_cascade.cpp`, remover o 6º argumento dos construtores:

Run: `sed -i 's/, 0\.01f);/);/' test/test_cascade/test_cascade.cpp`

- [ ] **Step 5: Rodar todos os testes nativos**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_pid` com 15 casos).

- [ ] **Step 6: Commit**

```bash
git add src/control/PIDController.h src/control/PIDController.cpp \
        src/control/CascadeController.cpp src/strategy/LineFollower.cpp \
        test/test_pid/test_pid.cpp test/test_cascade/test_cascade.cpp
git commit -m "fix(pid): real dt, filtered derivative and kick-free reset (F-07, F-12)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Calibration — polaridade da linha, sem overflow, validação e persistência

Resolve F-02 e F-03.

**Files:**
- Modify: `src/sensors/Calibration.h`, `src/sensors/Calibration.cpp`
- Test: `test/test_calibration/test_calibration.cpp` (acrescentar casos)

**Interfaces:**
- Produces:
  - `struct CalibrationData { int16_t min[8]; int16_t max[8]; };`
  - `Calibration::MIN_VALID_RANGE = 200`, `Calibration::NORM_MAX = 1000`
  - `void setWhiteLine(bool whiteLine)`, `bool whiteLine() const`
  - `bool isValid(uint8_t* badChannels = nullptr, int* badCount = nullptr) const`
  - `CalibrationData exportData() const`, `void importData(const CalibrationData& data)`
  - `int count() const`
  - Mantidos: `update`, `normalize`, `weightedPosition`, `reset`, `getMin`, `getMax`,
    `isLineLost`, `POSITION_SCALE`, `MAX_SENSORS`.

- [ ] **Step 1: Acrescentar os testes**

Adicionar ao fim de `test/test_calibration/test_calibration.cpp`, antes de `main()`:

```cpp
static void calibrateFlat(Calibration& cal) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    cal.update(lo);
    cal.update(hi);
}

// F-02: sem calibração, normalize não pode fazer INT_MIN − INT_MAX
void test_calibration_normalize_after_reset_is_zero() {
    Calibration cal(8);
    int raw[8] = {500, 500, 500, 500, 500, 500, 500, 500};
    int out[8];
    cal.normalize(raw, out);
    for (int i = 0; i < 8; i++) TEST_ASSERT_EQUAL_INT(0, out[i]);
}

void test_calibration_white_line_inverts_normalized_value() {
    Calibration cal(8);
    calibrateFlat(cal);
    cal.setWhiteLine(true);
    int raw[8] = {0, 250, 500, 750, 1000, 1000, 1000, 1000};
    int out[8];
    cal.normalize(raw, out);
    TEST_ASSERT_EQUAL_INT(1000, out[0]);
    TEST_ASSERT_EQUAL_INT(750, out[1]);
    TEST_ASSERT_EQUAL_INT(0, out[4]);
}

// F-03: QTR-8A lê menos tensão sobre o branco; a posição tem que seguir a leitura baixa
void test_calibration_white_line_position_follows_low_reading() {
    Calibration cal(8);
    calibrateFlat(cal);
    cal.setWhiteLine(true);
    int raw[8] = {1000, 1000, 1000, 1000, 1000, 1000, 100, 0};
    int norm[8];
    cal.normalize(raw, norm);
    // (2500 × 900 + 3500 × 1000) / 1900
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 3026.3f, cal.weightedPosition(norm));
}

void test_calibration_is_valid_reports_narrow_channels() {
    Calibration cal(8);
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 150, 1000, 199, 1000, 1000};
    cal.update(lo);
    cal.update(hi);
    uint8_t bad[8] = {};
    int n = 0;
    TEST_ASSERT_FALSE(cal.isValid(bad, &n));
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_UINT8(3, bad[0]);
    TEST_ASSERT_EQUAL_UINT8(5, bad[1]);
}

void test_calibration_is_valid_after_full_sweep() {
    Calibration cal(8);
    calibrateFlat(cal);
    int n = -1;
    TEST_ASSERT_TRUE(cal.isValid(nullptr, &n));
    TEST_ASSERT_EQUAL_INT(0, n);
}

void test_calibration_is_invalid_after_reset() {
    Calibration cal(8);
    TEST_ASSERT_FALSE(cal.isValid());
}

void test_calibration_export_import_roundtrip() {
    Calibration a(8);
    int lo[8] = {10, 20, 30, 40, 50, 60, 70, 80};
    int hi[8] = {900, 910, 920, 930, 940, 950, 960, 970};
    a.update(lo);
    a.update(hi);
    const CalibrationData d = a.exportData();
    Calibration b(8);
    b.importData(d);
    for (int i = 0; i < 8; i++) {
        TEST_ASSERT_EQUAL_INT(a.getMin(i), b.getMin(i));
        TEST_ASSERT_EQUAL_INT(a.getMax(i), b.getMax(i));
    }
    TEST_ASSERT_TRUE(b.isValid());
}

void test_calibration_import_uncalibrated_channel_stays_invalid() {
    CalibrationData d{};
    for (int i = 0; i < 8; i++) { d.min[i] = 0; d.max[i] = 1000; }
    d.max[2] = 0;  // canal 2 sem faixa
    Calibration cal(8);
    cal.importData(d);
    TEST_ASSERT_FALSE(cal.isValid());
    TEST_ASSERT_TRUE(cal.getMin(2) > cal.getMax(2));
}
```

E acrescentar ao `main()`:

```cpp
    RUN_TEST(test_calibration_normalize_after_reset_is_zero);
    RUN_TEST(test_calibration_white_line_inverts_normalized_value);
    RUN_TEST(test_calibration_white_line_position_follows_low_reading);
    RUN_TEST(test_calibration_is_valid_reports_narrow_channels);
    RUN_TEST(test_calibration_is_valid_after_full_sweep);
    RUN_TEST(test_calibration_is_invalid_after_reset);
    RUN_TEST(test_calibration_export_import_roundtrip);
    RUN_TEST(test_calibration_import_uncalibrated_channel_stays_invalid);
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_calibration`
Expected: FAIL de compilação (`setWhiteLine`, `isValid`, `CalibrationData` não existem).

- [ ] **Step 3: Implementar**

`src/sensors/Calibration.h`:

```cpp
#pragma once
#include <cassert>
#include <climits>
#include <cstdint>

/** Min/max por canal — o que vai para a NVS (ConfigStore, chave "cal"). */
struct CalibrationData {
    int16_t min[8];
    int16_t max[8];
};

class Calibration {
public:
    static constexpr int MAX_SENSORS = 8;
    static constexpr float POSITION_SCALE = 1000.0f;
    static constexpr int NORM_MAX = 1000;
    static constexpr int MIN_VALID_RANGE = 200;  // contagens do MCP3008 (0–1023)

    explicit Calibration(int sensorCount);

    void update(const int* rawValues);
    void normalize(const int* rawValues, int* normalizedOut) const;
    float weightedPosition(const int* normalized) const;
    void reset();

    /**
     * true = linha branca sobre fundo preto. O QTR-8A dá menos tensão sobre o branco, então
     * a leitura normalizada é invertida para que "linha" continue sendo valor alto (F-03).
     */
    void setWhiteLine(bool whiteLine) { _whiteLine = whiteLine; }
    bool whiteLine() const { return _whiteLine; }

    /** True when every channel has max − min ≥ MIN_VALID_RANGE; lists failing channels. */
    bool isValid(uint8_t* badChannels = nullptr, int* badCount = nullptr) const;

    CalibrationData exportData() const;
    void importData(const CalibrationData& data);

    int count() const { return _count; }
    int getMin(int idx) const;
    int getMax(int idx) const;
    bool isLineLost() const { return _lineLost; }

private:
    Calibration(const Calibration&) = delete;
    Calibration& operator=(const Calibration&) = delete;
    Calibration(Calibration&&) = delete;
    Calibration& operator=(Calibration&&) = delete;

    int _range(int idx) const;

    int _count;
    int _min[MAX_SENSORS];
    int _max[MAX_SENSORS];
    bool _whiteLine = false;
    mutable float _lastPosition;
    mutable bool _lineLost;

    static constexpr int LINE_LOST_THRESHOLD = 200;
};
```

`src/sensors/Calibration.cpp`:

```cpp
#include "sensors/Calibration.h"
#include <algorithm>

Calibration::Calibration(int sensorCount)
    : _count(sensorCount), _lastPosition(0.0f), _lineLost(false) {
    assert(sensorCount > 0 && sensorCount <= MAX_SENSORS);
    reset();
}

void Calibration::reset() {
    for (int i = 0; i < _count; i++) {
        _min[i] = INT_MAX;
        _max[i] = INT_MIN;
    }
    _lastPosition = 0.0f;
    _lineLost = false;
}

int Calibration::getMin(int idx) const { return _min[idx]; }
int Calibration::getMax(int idx) const { return _max[idx]; }

int Calibration::_range(int i) const {
    // Canal nunca calibrado tem max < min. Comparar antes de subtrair evita o overflow de
    // INT_MIN − INT_MAX (F-02).
    if (_max[i] <= _min[i]) return 0;
    return _max[i] - _min[i];
}

void Calibration::update(const int* rawValues) {
    for (int i = 0; i < _count; i++) {
        _min[i] = std::min(_min[i], rawValues[i]);
        _max[i] = std::max(_max[i], rawValues[i]);
    }
}

void Calibration::normalize(const int* rawValues, int* normalizedOut) const {
    for (int i = 0; i < _count; i++) {
        const int range = _range(i);
        if (range <= 0) {
            normalizedOut[i] = 0;
            continue;
        }
        const long scaled = static_cast<long>(rawValues[i] - _min[i]) * NORM_MAX / range;
        const int v = static_cast<int>(std::max(0L, std::min(static_cast<long>(NORM_MAX), scaled)));
        normalizedOut[i] = _whiteLine ? NORM_MAX - v : v;
    }
}

float Calibration::weightedPosition(const int* normalized) const {
    long sum = 0;
    long weightedSum = 0;
    const int halfRange = (_count - 1) * static_cast<int>(POSITION_SCALE) / 2;

    for (int i = 0; i < _count; i++) {
        const int pos = i * static_cast<int>(POSITION_SCALE) - halfRange;
        sum += normalized[i];
        weightedSum += static_cast<long>(pos) * normalized[i];
    }

    if (sum < LINE_LOST_THRESHOLD) {
        _lineLost = true;
        return _lastPosition;
    }

    _lineLost = false;
    _lastPosition = static_cast<float>(weightedSum) / static_cast<float>(sum);
    return _lastPosition;
}

bool Calibration::isValid(uint8_t* badChannels, int* badCount) const {
    int n = 0;
    for (int i = 0; i < _count; i++) {
        if (_range(i) < MIN_VALID_RANGE) {
            if (badChannels) badChannels[n] = static_cast<uint8_t>(i);
            n++;
        }
    }
    if (badCount) *badCount = n;
    return n == 0;
}

static int16_t toInt16(int v) {
    return static_cast<int16_t>(std::max(-32768, std::min(32767, v)));
}

CalibrationData Calibration::exportData() const {
    CalibrationData d{};
    for (int i = 0; i < _count; i++) {
        if (_max[i] > _min[i]) {
            d.min[i] = toInt16(_min[i]);
            d.max[i] = toInt16(_max[i]);
        }
    }
    return d;
}

void Calibration::importData(const CalibrationData& data) {
    for (int i = 0; i < _count; i++) {
        if (data.max[i] > data.min[i]) {
            _min[i] = data.min[i];
            _max[i] = data.max[i];
        } else {
            _min[i] = INT_MAX;
            _max[i] = INT_MIN;
        }
    }
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_calibration` com 18 casos).

- [ ] **Step 5: Commit**

```bash
git add src/sensors/Calibration.h src/sensors/Calibration.cpp \
        test/test_calibration/test_calibration.cpp
git commit -m "fix(calibration): line polarity, overflow-free normalize, validity and export (F-02, F-03)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: LineFollower — erro normalizado, linha perdida e sem apagar a calibração

Resolve F-01 e F-14. Remove o `DifferentialDrive`.

**Files:**
- Modify: `src/strategy/LineFollower.h`, `src/strategy/LineFollower.cpp`
- Delete: `src/motors/DifferentialDrive.h`, `src/motors/DifferentialDrive.cpp`,
  `test/test_motors/` (diretório inteiro)
- Test: `test/test_line_follower/test_line_follower.cpp` (reescrever)

**Interfaces:**
- Consumes: `Calibration` (Tarefa 3), `PIDController` (Tarefa 2).
- Produces:
  - `struct LineReading { float error; float u; bool lost; };`
  - `LineFollower(Calibration& cal, PIDController& pid)`
  - `LineReading update(const int* rawSensors, float dtS)`
  - `float peekError(const int* rawSensors)`
  - `void reset(float currentError = 0.0f)`
  - `void setSensorOrderInverted(bool inverted)`
- Convenção de sinal: `error > 0` = linha à direita; `u > 0` = virar à direita (lado esquerdo
  mais rápido).

- [ ] **Step 1: Reescrever o teste**

`test/test_line_follower/test_line_follower.cpp`:

```cpp
#include <unity.h>
#include "strategy/LineFollower.h"
#include "sensors/Calibration.h"
#include "control/PIDController.h"

static Calibration*   cal = nullptr;
static PIDController* pid = nullptr;
static LineFollower*  lf  = nullptr;
static constexpr float DT = 0.002f;

static void calibrateFlat(Calibration& c) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    c.update(lo);
    c.update(hi);
}

void setUp() {
    cal = new Calibration(8);
    pid = new PIDController(1.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    lf  = new LineFollower(*cal, *pid);
    calibrateFlat(*cal);
}

void tearDown() {
    delete lf;
    delete pid;
    delete cal;
}

void test_line_follower_centered_line_zero_error() {
    int raw[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.error);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.u);
    TEST_ASSERT_FALSE(r.lost);
}

void test_line_follower_line_right_turns_right() {
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_TRUE(r.error > 0.0f);
    TEST_ASSERT_TRUE(r.u > 0.0f);
}

void test_line_follower_line_left_turns_left() {
    int raw[8] = {0, 800, 700, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(raw, DT);
    TEST_ASSERT_TRUE(r.error < 0.0f);
    TEST_ASSERT_TRUE(r.u < 0.0f);
}

void test_line_follower_error_normalized_to_unit_range() {
    int raw[8] = {0, 0, 0, 0, 0, 0, 0, 1000};
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, lf->update(raw, DT).error);
}

// F-14: linha perdida satura para o lado onde foi vista por último
void test_line_follower_lost_saturates_to_last_side() {
    int right[8] = {0, 0, 0, 0, 0, 0, 300, 700};
    lf->update(right, DT);
    int none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(none, DT);
    TEST_ASSERT_TRUE(r.lost);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, r.error);
    TEST_ASSERT_TRUE(r.u > 0.0f);
}

void test_line_follower_lost_without_history_goes_straight() {
    int none[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    const LineReading r = lf->update(none, DT);
    TEST_ASSERT_TRUE(r.lost);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, r.error);
}

// F-01: reset do seguidor não apaga a calibração
void test_line_follower_reset_keeps_calibration() {
    lf->reset(0.0f);
    TEST_ASSERT_TRUE(cal->isValid());
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error > 0.0f);
}

void test_line_follower_sensor_order_inverted_flips_error() {
    lf->setSensorOrderInverted(true);
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error < 0.0f);
}

void test_line_follower_white_line_follows_low_reading() {
    cal->setWhiteLine(true);
    int raw[8] = {1000, 1000, 1000, 1000, 1000, 300, 200, 1000};
    TEST_ASSERT_TRUE(lf->update(raw, DT).error > 0.0f);
}

void test_line_follower_reset_with_error_avoids_derivative_kick() {
    PIDController dOnly(0.0f, 0.0f, 1.0f, -1.0f, 1.0f);
    LineFollower f(*cal, dOnly);
    int raw[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    f.reset(f.peekError(raw));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, f.update(raw, DT).u);
}

void test_line_follower_derivative_scales_with_dt() {
    PIDController pidA(0.0f, 0.0f, 0.001f, -1.0f, 1.0f);
    PIDController pidB(0.0f, 0.0f, 0.001f, -1.0f, 1.0f);
    LineFollower a(*cal, pidA);
    LineFollower b(*cal, pidB);
    int center[8] = {0, 0, 0, 700, 700, 0, 0, 0};
    int right[8] = {0, 0, 0, 0, 700, 700, 0, 0};
    a.reset(0.0f);
    b.reset(0.0f);
    a.update(center, 0.002f);
    b.update(center, 0.004f);
    const float uA = a.update(right, 0.002f).u;
    const float uB = b.update(right, 0.004f).u;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f * uB, uA);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_line_follower_centered_line_zero_error);
    RUN_TEST(test_line_follower_line_right_turns_right);
    RUN_TEST(test_line_follower_line_left_turns_left);
    RUN_TEST(test_line_follower_error_normalized_to_unit_range);
    RUN_TEST(test_line_follower_lost_saturates_to_last_side);
    RUN_TEST(test_line_follower_lost_without_history_goes_straight);
    RUN_TEST(test_line_follower_reset_keeps_calibration);
    RUN_TEST(test_line_follower_sensor_order_inverted_flips_error);
    RUN_TEST(test_line_follower_white_line_follows_low_reading);
    RUN_TEST(test_line_follower_reset_with_error_avoids_derivative_kick);
    RUN_TEST(test_line_follower_derivative_scales_with_dt);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_line_follower`
Expected: FAIL de compilação (`LineReading` não existe; construtor de 2 argumentos).

- [ ] **Step 3: Implementar**

`src/strategy/LineFollower.h`:

```cpp
#pragma once
#include "sensors/Calibration.h"
#include "control/PIDController.h"

/** Resultado de um ciclo da malha de posição. */
struct LineReading {
    float error;  ///< posição normalizada em [-1, +1]; > 0 = linha à direita
    float u;      ///< saída do PID externo em [-1, +1]; > 0 = virar à direita
    bool  lost;   ///< nenhum sensor vê a linha
};

/**
 * Malha de posição: leitura bruta → erro normalizado → PID externo.
 *
 * Não altera a calibração em nenhuma hipótese (F-01). Com a linha perdida, o erro satura
 * para o lado onde ela foi vista por último (F-14).
 */
class LineFollower {
public:
    LineFollower(Calibration& cal, PIDController& pid);

    LineReading update(const int* rawSensors, float dtS);

    /** Error for `rawSensors` without running the PID (used to align resets). */
    float peekError(const int* rawSensors);

    /** Clears the PID aligned to `currentError` and seeds the lost-line memory. */
    void reset(float currentError = 0.0f);

    void setSensorOrderInverted(bool inverted) { _sensorInvert = inverted; }

private:
    float _errorFrom(const int* rawSensors, bool& lost);

    Calibration&   _cal;
    PIDController& _pid;
    float _positionMax;
    bool  _sensorInvert = false;
    float _lastValidError = 0.0f;
    int   _normalized[Calibration::MAX_SENSORS];
};
```

`src/strategy/LineFollower.cpp`:

```cpp
#include "strategy/LineFollower.h"
#include <algorithm>

LineFollower::LineFollower(Calibration& cal, PIDController& pid)
    : _cal(cal), _pid(pid),
      _positionMax((cal.count() - 1) * Calibration::POSITION_SCALE / 2.0f) {}

float LineFollower::_errorFrom(const int* rawSensors, bool& lost) {
    _cal.normalize(rawSensors, _normalized);
    const float position = _cal.weightedPosition(_normalized);
    lost = _cal.isLineLost();
    if (lost) {
        if (_lastValidError > 0.0f) return 1.0f;
        if (_lastValidError < 0.0f) return -1.0f;
        return 0.0f;
    }
    float e = (_positionMax > 0.0f) ? position / _positionMax : 0.0f;
    if (_sensorInvert) e = -e;
    e = std::max(-1.0f, std::min(1.0f, e));
    _lastValidError = e;
    return e;
}

LineReading LineFollower::update(const int* rawSensors, float dtS) {
    bool lost = false;
    const float e = _errorFrom(rawSensors, lost);
    // Setpoint 0: o PID devolve -kp·e; o sinal é invertido para que u > 0 vire à direita
    const float u = -_pid.compute(0.0f, e, dtS);
    return LineReading{e, u, lost};
}

float LineFollower::peekError(const int* rawSensors) {
    bool lost = false;
    return _errorFrom(rawSensors, lost);
}

void LineFollower::reset(float currentError) {
    _pid.reset(currentError);
    _lastValidError = currentError;
}
```

- [ ] **Step 4: Remover o DifferentialDrive**

Run: `git rm -q src/motors/DifferentialDrive.h src/motors/DifferentialDrive.cpp && git rm -rq test/test_motors`

Run: `grep -rn "DifferentialDrive" src test`
Expected: só `src/main.cpp` (reescrito na Tarefa 18).

- [ ] **Step 5: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED; `test_motors` não existe mais.

- [ ] **Step 6: Commit**

```bash
git add src/strategy/LineFollower.h src/strategy/LineFollower.cpp \
        test/test_line_follower/test_line_follower.cpp
git commit -m "fix(line-follower): keep calibration on reset, saturate lost line, drop DifferentialDrive (F-01, F-14)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: SpeedProfile em fração com rampa de aceleração

Resolve F-15 e prepara F-05.

**Files:**
- Modify: `src/control/SpeedProfile.h`, `src/control/SpeedProfile.cpp`
- Test: `test/test_speed_profile/test_speed_profile.cpp` (reescrever)

**Interfaces:**
- Produces:
  - `SpeedProfile()` (sem argumentos)
  - `void setParams(float vmin, float vmax, float thr, float acc)`
  - `float target(float absError) const`
  - `float step(float absError, float dtS)` — rampa de subida `acc`·dt; descida imediata
  - `void reset(float initial = 0.0f)`, `float current() const`

- [ ] **Step 1: Reescrever o teste**

`test/test_speed_profile/test_speed_profile.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "control/SpeedProfile.h"

static SpeedProfile sp;

void setUp() {
    sp = SpeedProfile();
    sp.setParams(0.25f, 0.5f, 0.6f, 2.0f);
    sp.reset(0.0f);
}
void tearDown() {}

void test_speed_profile_zero_error_returns_vmax() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, sp.target(0.0f));
}

void test_speed_profile_error_at_threshold_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(0.6f));
}

void test_speed_profile_error_beyond_threshold_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(1.0f));
}

void test_speed_profile_midpoint_interpolates() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.375f, sp.target(0.3f));
}

void test_speed_profile_negative_error_uses_magnitude() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.375f, sp.target(-0.3f));
}

void test_speed_profile_nan_error_returns_vmin() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.target(NAN));
}

void test_speed_profile_ramp_limits_rise() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.2f, sp.step(0.0f, 0.1f));  // 2,0/s × 0,1 s
}

void test_speed_profile_ramp_reaches_target_and_holds() {
    for (int i = 0; i < 10; i++) sp.step(0.0f, 0.1f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, sp.current());
}

void test_speed_profile_fall_is_immediate() {
    sp.reset(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, sp.step(0.6f, 0.002f));
}

void test_speed_profile_zero_dt_does_not_rise() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, sp.step(0.0f, 0.0f));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_speed_profile_zero_error_returns_vmax);
    RUN_TEST(test_speed_profile_error_at_threshold_returns_vmin);
    RUN_TEST(test_speed_profile_error_beyond_threshold_returns_vmin);
    RUN_TEST(test_speed_profile_midpoint_interpolates);
    RUN_TEST(test_speed_profile_negative_error_uses_magnitude);
    RUN_TEST(test_speed_profile_nan_error_returns_vmin);
    RUN_TEST(test_speed_profile_ramp_limits_rise);
    RUN_TEST(test_speed_profile_ramp_reaches_target_and_holds);
    RUN_TEST(test_speed_profile_fall_is_immediate);
    RUN_TEST(test_speed_profile_zero_dt_does_not_rise);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_speed_profile`
Expected: FAIL de compilação (construtor sem argumentos, `target`, `step` não existem).

- [ ] **Step 3: Implementar**

`src/control/SpeedProfile.h`:

```cpp
#pragma once

/**
 * Velocidade-base adaptativa, em fração de escala [0, 1] (spec §4.3).
 *   |erro| = 0     → vmax (reta)
 *   |erro| >= thr  → vmin (curva)
 *   entre os dois  → interpolação linear
 * step() limita a subida a `acc` fração/s; a descida é imediata.
 */
class SpeedProfile {
public:
    SpeedProfile() = default;

    void setParams(float vmin, float vmax, float thr, float acc);
    float target(float absError) const;
    float step(float absError, float dtS);
    void reset(float initial = 0.0f) { _current = initial; }
    float current() const { return _current; }

private:
    float _vmin = 0.0f;
    float _vmax = 0.0f;
    float _thr = 1.0f;
    float _acc = 1.0f;
    float _current = 0.0f;
};
```

`src/control/SpeedProfile.cpp`:

```cpp
#include "control/SpeedProfile.h"
#include <algorithm>
#include <cmath>

void SpeedProfile::setParams(float vmin, float vmax, float thr, float acc) {
    _vmin = vmin;
    _vmax = vmax;
    _thr = (thr > 0.0f) ? thr : 1.0f;
    _acc = (acc > 0.0f) ? acc : 0.0f;
}

float SpeedProfile::target(float absError) const {
    const float e = std::fabs(absError);
    if (!std::isfinite(e)) return _vmin;
    if (e <= 0.0f) return _vmax;
    if (e >= _thr) return _vmin;
    return _vmax + (e / _thr) * (_vmin - _vmax);
}

float SpeedProfile::step(float absError, float dtS) {
    const float tgt = target(absError);
    if (tgt <= _current) {
        _current = tgt;
        return _current;
    }
    const float dt = (std::isfinite(dtS) && dtS > 0.0f) ? dtS : 0.0f;
    _current = std::min(tgt, _current + _acc * dt);
    return _current;
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/control/SpeedProfile.h src/control/SpeedProfile.cpp \
        test/test_speed_profile/test_speed_profile.cpp
git commit -m "feat(speed): fractional speed profile with acceleration ramp (F-15)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: VelocityEstimator com dt em microssegundos e PPR ajustável

Resolve F-06 e F-17.

**Files:**
- Modify: `src/sensors/VelocityEstimator.h`, `src/sensors/VelocityEstimator.cpp`, `src/config.h`
- Test: `test/test_velocity_estimator/test_velocity_estimator.cpp` (reescrever)

**Interfaces:**
- Produces:
  - `void update(uint32_t dtUs)` — **dt em microssegundos**
  - `bool setPprX4(float pprX4)` — valida, aplica, **não** persiste
  - `ENCODER_DEFAULT_PPR_X4` = `600.0f` em `config.h`
- Mantidos: `begin`, `getRPM`, `getEffectivePPR`, `setFilterAlpha`, `startCalibration`,
  `finishCalibration`.

- [ ] **Step 1: Reescrever o teste**

`test/test_velocity_estimator/test_velocity_estimator.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "config.h"
#include "sensors/Encoder.h"
#include "sensors/VelocityEstimator.h"
#include "storage/InMemoryStore.h"

static Encoder*           enc   = nullptr;
static InMemoryStore*     store = nullptr;
static VelocityEstimator* ve    = nullptr;

void setUp() {
    enc = new Encoder(0, 1);
    enc->begin();
    store = new InMemoryStore();
    store->begin("vetest");
    ve = new VelocityEstimator(*enc, *store, "left");
    ve->begin();
}

void tearDown() {
    delete ve;    ve = nullptr;
    delete store; store = nullptr;
    delete enc;   enc = nullptr;
}

static void forwardPeriods(Encoder& e, int periods) {
    for (int i = 0; i < periods; i++) {
        e._simulatePulse(false, true);
        e._simulatePulse(true,  true);
        e._simulatePulse(true,  false);
        e._simulatePulse(false, false);
    }
}

// Uma volta do eixo de saída = countsPerRotation contagens (4 por período de quadratura)
static void simulateRotations(Encoder& e, int rotations,
                              int countsPerRotation = static_cast<int>(ENCODER_DEFAULT_PPR_X4)) {
    forwardPeriods(e, rotations * countsPerRotation / 4);
}

void test_velocity_zero_when_no_pulses() {
    ve->update(10000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_converts_pulses_to_rpm() {
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1);
    ve->update(100000);  // 1 volta em 100 ms = 600 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 600.0f, ve->getRPM());
}

void test_velocity_low_pass_smooths_spikes() {
    ve->setFilterAlpha(0.3f);
    for (int i = 0; i < 20; i++) {
        simulateRotations(*enc, 1);
        ve->update(100000);
    }
    const float steady = ve->getRPM();
    TEST_ASSERT_FLOAT_WITHIN(20.0f, 600.0f, steady);
    simulateRotations(*enc, 5);
    ve->update(100000);
    const float afterSpike = ve->getRPM();
    TEST_ASSERT_TRUE(afterSpike < 3000.0f);
    TEST_ASSERT_TRUE(afterSpike > steady);
}

void test_velocity_handles_zero_dt_safely() {
    simulateRotations(*enc, 1);
    ve->update(0);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_signed_with_direction() {
    ve->setFilterAlpha(1.0f);
    for (int i = 0; i < 150; i++) {  // 150 períodos para trás = -600 contagens = -1 volta
        enc->_simulatePulse(true,  false);
        enc->_simulatePulse(true,  true);
        enc->_simulatePulse(false, true);
        enc->_simulatePulse(false, false);
    }
    ve->update(100000);
    TEST_ASSERT_FLOAT_WITHIN(1.0f, -600.0f, ve->getRPM());
}

// F-17: o padrão considera a redução (12 CPR × 50:1)
void test_velocity_default_ppr_when_no_calibration() {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, ve->getEffectivePPR());
}

void test_velocity_start_calibration_resets_count() {
    simulateRotations(*enc, 3);
    ve->startCalibration();
    TEST_ASSERT_EQUAL_INT32(0, enc->getCount());
}

void test_velocity_finish_calibration_stores_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);
    TEST_ASSERT_TRUE(ve->finishCalibration(10));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, ve->getEffectivePPR());
    TEST_ASSERT_TRUE(store->exists("left_ppr"));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 40.0f, store->getFloat("left_ppr", 0.0f));
}

void test_velocity_uses_calibrated_ppr() {
    ve->startCalibration();
    simulateRotations(*enc, 10, 40);
    ve->finishCalibration(10);
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1, 40);
    ve->update(100000);
    TEST_ASSERT_FLOAT_WITHIN(2.0f, 600.0f, ve->getRPM());
}

void test_velocity_begin_loads_persisted_ppr() {
    store->putFloat("left_ppr", 56.0f);
    VelocityEstimator ve2(*enc, *store, "left");
    ve2.begin();
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 56.0f, ve2.getEffectivePPR());
}

// F-06: com laço de 0,5 ms o RPM continua sendo atualizado
void test_velocity_sub_millisecond_dt_updates_rpm() {
    ve->setFilterAlpha(1.0f);
    forwardPeriods(*enc, 1);  // 4 contagens
    ve->update(500);          // 4 × 60e6 / (500 × 600) = 800 RPM
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 800.0f, ve->getRPM());
}

void test_velocity_discards_window_longer_than_limit() {
    ve->setFilterAlpha(1.0f);
    simulateRotations(*enc, 1);
    ve->update(600000);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ve->getRPM());
}

void test_velocity_set_ppr_validates() {
    TEST_ASSERT_FALSE(ve->setPprX4(0.0f));
    TEST_ASSERT_FALSE(ve->setPprX4(NAN));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 600.0f, ve->getEffectivePPR());
    TEST_ASSERT_TRUE(ve->setPprX4(1400.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1400.0f, ve->getEffectivePPR());
    TEST_ASSERT_FALSE(store->exists("left_ppr"));  // não persiste
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_velocity_zero_when_no_pulses);
    RUN_TEST(test_velocity_converts_pulses_to_rpm);
    RUN_TEST(test_velocity_low_pass_smooths_spikes);
    RUN_TEST(test_velocity_handles_zero_dt_safely);
    RUN_TEST(test_velocity_signed_with_direction);
    RUN_TEST(test_velocity_default_ppr_when_no_calibration);
    RUN_TEST(test_velocity_start_calibration_resets_count);
    RUN_TEST(test_velocity_finish_calibration_stores_ppr);
    RUN_TEST(test_velocity_uses_calibrated_ppr);
    RUN_TEST(test_velocity_begin_loads_persisted_ppr);
    RUN_TEST(test_velocity_sub_millisecond_dt_updates_rpm);
    RUN_TEST(test_velocity_discards_window_longer_than_limit);
    RUN_TEST(test_velocity_set_ppr_validates);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_velocity_estimator`
Expected: FAIL — `setPprX4` não existe; com ele comentado, `test_velocity_default_ppr...`
falha (28 ≠ 600).

- [ ] **Step 3: Implementar**

Em `src/config.h`, trocar o bloco do encoder:

```cpp
// ═══ ENCODER / VELOCITY ═══════════════════════════════════════════════════
// Contagens ×4 por volta do EIXO DE SAÍDA. 600 = encoder de 12 CPR no eixo do motor ×
// redução 50:1 (N20 de 600 RPM). PROVISÓRIO: a auto-calibração por BLE ({"t":"cal"}) é
// obrigatória antes de usar o modo cascata (F-17).
#define ENCODER_DEFAULT_PPR_X4   600.0f
#define VELOCITY_FILTER_ALPHA    0.3f  // 0 ≤ α ≤ 1 — menor = mais suave, mais atraso
```

Em `src/sensors/VelocityEstimator.h`, trocar a declaração de `update` e acrescentar
`setPprX4`:

```cpp
    // Atualiza o RPM dado o intervalo desde a última chamada, em MICROSSEGUNDOS (F-06).
    void update(uint32_t dtUs);

    /** Applies a new effective PPR (x4, output shaft). Not persisted. False if insane. */
    bool setPprX4(float pprX4);
```

Em `src/sensors/VelocityEstimator.cpp`, trocar `DT_MAX_MS` e `update`, e acrescentar
`setPprX4`:

```cpp
// dt máximo aceito antes de descartar a janela (jitter, primeira chamada, stall).
static constexpr uint32_t DT_MAX_US = 500000;
```

```cpp
void VelocityEstimator::update(uint32_t dtUs) {
    // Sempre drena delta — evita acumulação que distorceria a próxima janela.
    const int32_t delta = _enc.getDelta();
    if (_calibrating) return;
    if (dtUs == 0 || dtUs > DT_MAX_US || _pprX4 <= 0.0f) return;
    // contagens/µs → rotações/min: × 60 000 000 / pprX4
    const float rpmRaw = (static_cast<float>(delta) * 60000000.0f)
                       / (static_cast<float>(dtUs) * _pprX4);
    _rpmFiltered = _alpha * rpmRaw + (1.0f - _alpha) * _rpmFiltered;
}

bool VelocityEstimator::setPprX4(float pprX4) {
    if (!std::isfinite(pprX4) || pprX4 < PPR_MIN_SANE || pprX4 > PPR_MAX_SANE) return false;
    _pprX4 = pprX4;
    return true;
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/config.h src/sensors/VelocityEstimator.h src/sensors/VelocityEstimator.cpp \
        test/test_velocity_estimator/test_velocity_estimator.cpp
git commit -m "fix(velocity): microsecond dt, output-shaft PPR default, runtime PPR (F-06, F-17)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Cascata com feedforward e mistura com prioridade de direção

Resolve F-08.

**Files:**
- Create: `src/control/Mixer.h`
- Modify: `src/control/CascadeController.h`, `src/control/CascadeController.cpp`
- Test: `test/test_cascade/test_cascade.cpp` (reescrever)

**Interfaces:**
- Consumes: `PIDController` (Tarefa 2).
- Produces:
  - `inline void mixSteering(float base, float u, float& left, float& right)` em `Mixer.h`
  - `struct WheelCommand { float uL; float uR; float spL; float spR; };`
  - `CascadeController(PIDController& innerLeft, PIDController& innerRight)`
  - `WheelCommand compute(float base, float u, float rpmMax, float rpmL, float rpmR, float dtS)`
  - `void setInnerGains(float kp, float ki, float kd)`
  - `void reset(float rpmL, float rpmR)`

- [ ] **Step 1: Reescrever o teste**

`test/test_cascade/test_cascade.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "control/CascadeController.h"
#include "control/Mixer.h"
#include "control/PIDController.h"

static PIDController*     innerL = nullptr;
static PIDController*     innerR = nullptr;
static CascadeController* cc     = nullptr;
static constexpr float RPM_MAX = 1000.0f;
static constexpr float DT = 0.002f;

void setUp() {
    innerL = new PIDController(0.002f, 0.0f, 0.0f, -1.0f, 1.0f);
    innerR = new PIDController(0.002f, 0.0f, 0.0f, -1.0f, 1.0f);
    cc = new CascadeController(*innerL, *innerR);
}

void tearDown() {
    delete cc;     cc = nullptr;
    delete innerR; innerR = nullptr;
    delete innerL; innerL = nullptr;
}

// F-08: no setpoint, o feedforward sozinho mantém o duty
void test_cascade_feedforward_holds_duty_at_setpoint() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, RPM_MAX, 500.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uR);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 500.0f, c.spL);
}

void test_cascade_slow_motor_gets_more_duty() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, RPM_MAX, 300.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.9f, c.uL);  // 0,5 + 0,002 × 200
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, c.uR);
}

void test_cascade_positive_u_splits_setpoints() {
    const WheelCommand c = cc->compute(0.5f, 0.2f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 700.0f, c.spL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 300.0f, c.spR);
}

void test_cascade_steering_has_priority_over_speed() {
    const WheelCommand c = cc->compute(0.9f, 0.3f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1000.0f, c.spL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 400.0f, c.spR);
}

void test_cascade_negative_setpoint_drives_reverse() {
    const WheelCommand c = cc->compute(0.1f, 0.5f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -400.0f, c.spR);
    TEST_ASSERT_TRUE(c.uR < 0.0f);
}

void test_cascade_output_clamped_to_unit() {
    PIDController bigL(10.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    PIDController bigR(10.0f, 0.0f, 0.0f, -1.0f, 1.0f);
    CascadeController big(bigL, bigR);
    const WheelCommand c = big.compute(0.5f, 0.0f, RPM_MAX, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.uR);
}

void test_cascade_no_windup_when_feedforward_saturates() {
    PIDController iL(0.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    PIDController iR(0.0f, 1.0f, 0.0f, -1.0f, 1.0f);
    CascadeController w(iL, iR);
    for (int i = 0; i < 1000; i++) w.compute(1.0f, 0.0f, RPM_MAX, 0.0f, 0.0f, DT);
    const WheelCommand c = w.compute(0.5f, 0.0f, RPM_MAX, 500.0f, 500.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, c.uL);
}

void test_cascade_nan_inputs_yield_zero_command() {
    const WheelCommand c = cc->compute(NAN, NAN, RPM_MAX, NAN, NAN, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uR);
}

void test_cascade_invalid_rpm_max_yields_zero() {
    const WheelCommand c = cc->compute(0.5f, 0.0f, 0.0f, 0.0f, 0.0f, DT);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.uL);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, c.spL);
}

void test_cascade_set_inner_gains_updates_both_pids() {
    cc->setInnerGains(0.01f, 0.1f, 0.001f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.01f, innerL->getKp());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.01f, innerR->getKp());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, innerL->getKi());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.001f, innerR->getKd());
}

void test_mixer_keeps_difference_when_saturating() {
    float l = 0.0f, r = 0.0f;
    mixSteering(0.8f, 0.5f, l, r);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, l);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, r);
}

void test_mixer_shifts_up_when_below_minus_one() {
    float l = 0.0f, r = 0.0f;
    mixSteering(-0.8f, 0.5f, l, r);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, l);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, r);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_cascade_feedforward_holds_duty_at_setpoint);
    RUN_TEST(test_cascade_slow_motor_gets_more_duty);
    RUN_TEST(test_cascade_positive_u_splits_setpoints);
    RUN_TEST(test_cascade_steering_has_priority_over_speed);
    RUN_TEST(test_cascade_negative_setpoint_drives_reverse);
    RUN_TEST(test_cascade_output_clamped_to_unit);
    RUN_TEST(test_cascade_no_windup_when_feedforward_saturates);
    RUN_TEST(test_cascade_nan_inputs_yield_zero_command);
    RUN_TEST(test_cascade_invalid_rpm_max_yields_zero);
    RUN_TEST(test_cascade_set_inner_gains_updates_both_pids);
    RUN_TEST(test_mixer_keeps_difference_when_saturating);
    RUN_TEST(test_mixer_shifts_up_when_below_minus_one);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_cascade`
Expected: FAIL de compilação (`Mixer.h`, `WheelCommand` não existem).

- [ ] **Step 3: Implementar**

`src/control/Mixer.h`:

```cpp
#pragma once
#include <algorithm>

/**
 * Mistura base ± direção em [-1, +1] preservando a diferença entre os lados.
 * Se um lado passar de ±1, os dois são deslocados juntos: a curva tem prioridade sobre a
 * velocidade. Só quando |u| > 1 a diferença deixa de caber e os lados saturam.
 */
inline void mixSteering(float base, float u, float& left, float& right) {
    left  = base + u;
    right = base - u;
    const float hi = std::max(left, right);
    if (hi > 1.0f) {
        left  -= hi - 1.0f;
        right -= hi - 1.0f;
    }
    const float lo = std::min(left, right);
    if (lo < -1.0f) {
        left  += -1.0f - lo;
        right += -1.0f - lo;
    }
    left  = std::max(-1.0f, std::min(1.0f, left));
    right = std::max(-1.0f, std::min(1.0f, right));
}
```

`src/control/CascadeController.h`:

```cpp
#pragma once
#include "control/PIDController.h"

/** Comando por roda produzido pela cascata. Frações em [-1, +1]; setpoints em RPM. */
struct WheelCommand {
    float uL;
    float uR;
    float spL;
    float spR;
};

/**
 * Malha interna de velocidade, um PID por motor.
 *
 *   base, u (fração) → mixSteering → fL, fR → setpoint = f × rpmMax
 *   saída = feedforward f + PID(setpoint − rpm medido)
 *
 * O feedforward entrega o duty de regime; o PID só corrige o erro (F-08). Os limites do PID
 * são deslocados pelo feedforward para o anti-windup enxergar a saturação total.
 */
class CascadeController {
public:
    CascadeController(PIDController& innerLeft, PIDController& innerRight);

    WheelCommand compute(float base, float u, float rpmMax,
                         float rpmL, float rpmR, float dtS);

    void setInnerGains(float kp, float ki, float kd);

    /** Clears both inner loops, aligning derivative memory to the measured RPMs. */
    void reset(float rpmL, float rpmR);

private:
    static float _wheel(PIDController& pid, float f, float rpmMax, float rpm,
                        float dtS, float& setpoint);

    PIDController& _pidL;
    PIDController& _pidR;
};
```

`src/control/CascadeController.cpp`:

```cpp
#include "control/CascadeController.h"
#include "control/Mixer.h"
#include <algorithm>
#include <cmath>

static float finiteOrZero(float v) { return std::isfinite(v) ? v : 0.0f; }

CascadeController::CascadeController(PIDController& innerLeft, PIDController& innerRight)
    : _pidL(innerLeft), _pidR(innerRight) {}

float CascadeController::_wheel(PIDController& pid, float f, float rpmMax, float rpm,
                                float dtS, float& setpoint) {
    setpoint = f * rpmMax;
    pid.setOutputLimits(-1.0f - f, 1.0f - f);
    const float out = f + pid.compute(setpoint, rpm, dtS);
    if (!std::isfinite(out)) return 0.0f;
    return std::max(-1.0f, std::min(1.0f, out));
}

WheelCommand CascadeController::compute(float base, float u, float rpmMax,
                                        float rpmL, float rpmR, float dtS) {
    WheelCommand cmd{0.0f, 0.0f, 0.0f, 0.0f};
    if (!std::isfinite(rpmMax) || rpmMax <= 0.0f) return cmd;
    float fL = 0.0f;
    float fR = 0.0f;
    mixSteering(finiteOrZero(base), finiteOrZero(u), fL, fR);
    cmd.uL = _wheel(_pidL, fL, rpmMax, finiteOrZero(rpmL), dtS, cmd.spL);
    cmd.uR = _wheel(_pidR, fR, rpmMax, finiteOrZero(rpmR), dtS, cmd.spR);
    return cmd;
}

void CascadeController::setInnerGains(float kp, float ki, float kd) {
    _pidL.setTunings(kp, ki, kd);
    _pidR.setTunings(kp, ki, kd);
}

void CascadeController::reset(float rpmL, float rpmR) {
    _pidL.reset(finiteOrZero(rpmL));
    _pidR.reset(finiteOrZero(rpmR));
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/control/Mixer.h src/control/CascadeController.h src/control/CascadeController.cpp \
        test/test_cascade/test_cascade.cpp
git commit -m "fix(cascade): feedforward inner loop and steering-priority mixer (F-08)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 8: RobotConfig — parâmetros, faixas e perfis de fábrica

Implementa a spec §4.3.

**Files:**
- Create: `src/config/RobotConfig.h`, `src/config/RobotConfig.cpp`
- Test: `test/test_robot_config/test_robot_config.cpp`

**Interfaces:**
- Produces:
  - `struct HardwareConfig`, `struct TuningProfile`, `struct RobotConfig { hw; tune; profile; }`
  - `constexpr uint8_t PROFILE_COUNT = 3;`
  - `enum class SetResult : uint8_t { OK, UNKNOWN_KEY, OUT_OF_RANGE, LOCKED_WHILE_RUNNING };`
  - namespace `robotcfg`: `FieldType`, `KeyInfo`, `GROUPS[]`, `GROUP_COUNT`,
    `factoryHardware()`, `factoryProfile(uint8_t)`, `factoryConfig()`,
    `keyTable(size_t&)`, `findKey(const char*)`,
    `SetResult set(RobotConfig&, const char* key, float value, bool running)`,
    `bool get(const RobotConfig&, const char* key, float& value)`,
    `bool validate(const RobotConfig&)`.
- Toda chave tem no máximo 7 caracteres (cabe em `SetItem::key[8]`, Tarefa 10).

- [ ] **Step 1: Escrever o teste**

`test/test_robot_config/test_robot_config.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include <cstring>
#include "config/RobotConfig.h"

static RobotConfig cfg;

void setUp() { cfg = robotcfg::factoryConfig(); }
void tearDown() {}

void test_robot_config_factory_hardware_matches_spec() {
    const HardwareConfig hw = robotcfg::factoryHardware();
    TEST_ASSERT_EQUAL_UINT8(0, hw.invL);
    TEST_ASSERT_EQUAL_UINT8(0, hw.swap);
    TEST_ASSERT_EQUAL_UINT8(1, hw.pol);
    TEST_ASSERT_EQUAL_UINT8(0, hw.sinv);
    TEST_ASSERT_EQUAL_UINT8(1, hw.chem);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 6.4f, hw.vcut);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, hw.vbk);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 600.0f, hw.pprL);
}

void test_robot_config_factory_profiles_differ_only_in_vmax() {
    const TuningProfile p0 = robotcfg::factoryProfile(0);
    const TuningProfile p1 = robotcfg::factoryProfile(1);
    const TuningProfile p2 = robotcfg::factoryProfile(2);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.35f, p0.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.50f, p1.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, p2.vmax);
    TEST_ASSERT_EQUAL_UINT8(0, p2.mode);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p2.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, p2.kd);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, p2.dlp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.25f, p2.vmin);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 700.0f, p2.rpmMax);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 7.4f, p2.vref);
    TEST_ASSERT_EQUAL_UINT16(300, p2.lostMs);
    TEST_ASSERT_EQUAL_UINT16(1000, p2.startMs);
}

void test_robot_config_factory_values_are_inside_ranges() {
    size_t n = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(n);
    TEST_ASSERT_EQUAL_UINT32(29, n);
    for (uint8_t p = 0; p < PROFILE_COUNT; p++) {
        RobotConfig c = robotcfg::factoryConfig();
        c.tune = robotcfg::factoryProfile(p);
        for (size_t i = 0; i < n; i++) {
            float v = 0.0f;
            TEST_ASSERT_TRUE(robotcfg::get(c, keys[i].key, v));
            TEST_ASSERT_TRUE_MESSAGE(v >= keys[i].min && v <= keys[i].max, keys[i].key);
        }
        TEST_ASSERT_TRUE(robotcfg::validate(c));
    }
}

void test_robot_config_keys_fit_command_buffer_and_have_known_group() {
    size_t n = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(n);
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_TRUE_MESSAGE(std::strlen(keys[i].key) <= 7, keys[i].key);
        bool known = false;
        for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
            if (std::strcmp(keys[i].group, robotcfg::GROUPS[g]) == 0) known = true;
        }
        TEST_ASSERT_TRUE_MESSAGE(known, keys[i].key);
    }
}

void test_robot_config_set_applies_valid_value() {
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "kp", 2.5f, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.5f, cfg.tune.kp);
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "invR", 1.0f, false));
    TEST_ASSERT_EQUAL_UINT8(1, cfg.hw.invR);
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "lostMs", 500.0f, false));
    TEST_ASSERT_EQUAL_UINT16(500, cfg.tune.lostMs);
}

void test_robot_config_unknown_key() {
    TEST_ASSERT_EQUAL(SetResult::UNKNOWN_KEY, robotcfg::set(cfg, "kq", 1.0f, false));
    TEST_ASSERT_EQUAL(SetResult::UNKNOWN_KEY, robotcfg::set(cfg, nullptr, 1.0f, false));
}

void test_robot_config_out_of_range_keeps_value() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kp", 20.1f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vcut", 5.5f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "mode", 2.0f, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, cfg.tune.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 6.4f, cfg.hw.vcut);
}

// Review Focus 1
void test_robot_config_rejects_non_finite() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kd", NAN, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "kd", INFINITY, false));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, cfg.tune.kd);
}

void test_robot_config_integer_fields_round() {
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "lostMs", 299.6f, false));
    TEST_ASSERT_EQUAL_UINT16(300, cfg.tune.lostMs);
}

void test_robot_config_hardware_and_mode_locked_while_running() {
    TEST_ASSERT_EQUAL(SetResult::LOCKED_WHILE_RUNNING, robotcfg::set(cfg, "invL", 1, true));
    TEST_ASSERT_EQUAL(SetResult::LOCKED_WHILE_RUNNING, robotcfg::set(cfg, "mode", 1, true));
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "kp", 1.5f, true));
}

void test_robot_config_vmin_cannot_exceed_vmax() {
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vmin", 0.4f, false));
    TEST_ASSERT_EQUAL(SetResult::OUT_OF_RANGE, robotcfg::set(cfg, "vmax", 0.2f, false));
    TEST_ASSERT_EQUAL(SetResult::OK, robotcfg::set(cfg, "vmin", 0.35f, false));
}

void test_robot_config_get_unknown_returns_false() {
    float v = -1.0f;
    TEST_ASSERT_FALSE(robotcfg::get(cfg, "nope", v));
}

void test_robot_config_validate_detects_bad_values() {
    cfg.tune.kp = 99.0f;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
    cfg = robotcfg::factoryConfig();
    cfg.tune.vmin = 0.9f;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
    cfg = robotcfg::factoryConfig();
    cfg.profile = 3;
    TEST_ASSERT_FALSE(robotcfg::validate(cfg));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_robot_config_factory_hardware_matches_spec);
    RUN_TEST(test_robot_config_factory_profiles_differ_only_in_vmax);
    RUN_TEST(test_robot_config_factory_values_are_inside_ranges);
    RUN_TEST(test_robot_config_keys_fit_command_buffer_and_have_known_group);
    RUN_TEST(test_robot_config_set_applies_valid_value);
    RUN_TEST(test_robot_config_unknown_key);
    RUN_TEST(test_robot_config_out_of_range_keeps_value);
    RUN_TEST(test_robot_config_rejects_non_finite);
    RUN_TEST(test_robot_config_integer_fields_round);
    RUN_TEST(test_robot_config_hardware_and_mode_locked_while_running);
    RUN_TEST(test_robot_config_vmin_cannot_exceed_vmax);
    RUN_TEST(test_robot_config_get_unknown_returns_false);
    RUN_TEST(test_robot_config_validate_detects_bad_values);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_robot_config`
Expected: FAIL (`config/RobotConfig.h` não existe).

- [ ] **Step 3: Implementar**

`src/config/RobotConfig.h`:

```cpp
#pragma once
#include <cstddef>
#include <cstdint>

/**
 * Configuração em runtime (spec §4.3). Os inicializadores dos membros SÃO os valores de
 * fábrica. HardwareConfig é global; TuningProfile existe em PROFILE_COUNT perfis.
 */
struct HardwareConfig {
    uint8_t invL  = 0;       ///< inverte o motor esquerdo
    uint8_t invR  = 0;       ///< inverte o motor direito
    uint8_t swap  = 0;       ///< troca os canais A/B do TB6612
    uint8_t einvL = 0;       ///< inverte a contagem do encoder esquerdo
    uint8_t einvR = 0;       ///< inverte a contagem do encoder direito
    uint8_t pol   = 1;       ///< 0 = linha preta, 1 = linha branca
    uint8_t sinv  = 0;       ///< barra de sensores montada espelhada
    uint8_t chem  = 1;       ///< 0 = LiPo, 1 = Li-ion
    float   vcut  = 6.4f;    ///< V — corte da bateria
    float   vbk   = 1.0f;    ///< correção da leitura de tensão
    float   pprL  = 600.0f;  ///< contagens ×4 por volta do eixo de saída
    float   pprR  = 600.0f;
};

struct TuningProfile {
    uint8_t  mode    = 0;       ///< 0 = PWM direto, 1 = cascata
    float    kp      = 1.0f;
    float    ki      = 0.0f;
    float    kd      = 0.03f;
    float    dlp     = 0.5f;
    float    kpv     = 0.002f;
    float    kiv     = 0.005f;
    float    kdv     = 0.0f;
    float    vmax    = 0.35f;
    float    vmin    = 0.25f;
    float    thr     = 0.6f;
    float    acc     = 2.0f;
    float    rpmMax  = 700.0f;
    float    vref    = 7.4f;
    uint8_t  vcomp   = 1;
    uint16_t lostMs  = 300;
    uint16_t startMs = 1000;
};

constexpr uint8_t PROFILE_COUNT = 3;

struct RobotConfig {
    HardwareConfig hw;
    TuningProfile  tune;
    uint8_t        profile = 0;
};

enum class SetResult : uint8_t { OK, UNKNOWN_KEY, OUT_OF_RANGE, LOCKED_WHILE_RUNNING };

namespace robotcfg {

enum class FieldType : uint8_t { U8, U16, F32 };

struct KeyInfo {
    const char* key;
    const char* group;      ///< "hw" | "pid" | "vel" | "spd" | "misc"
    FieldType   type;
    float       min;
    float       max;
    bool        hardware;   ///< true → HardwareConfig; false → TuningProfile
    bool        liveInRun;  ///< false → refused while ARMED/RUNNING
    size_t      offset;     ///< offsetof inside its struct
};

constexpr const char* GROUPS[] = {"hw", "pid", "vel", "spd", "misc"};
constexpr size_t GROUP_COUNT = 5;

HardwareConfig factoryHardware();
/** 0 "seguro" (vmax 0.35), 1 "treino" (0.50), 2 "corrida" (0.70). Others → profile 0. */
TuningProfile factoryProfile(uint8_t index);
RobotConfig factoryConfig();

const KeyInfo* keyTable(size_t& count);
const KeyInfo* findKey(const char* key);

/** Validates `value` for `key` and applies it. `running` = state is ARMED or RUNNING. */
SetResult set(RobotConfig& cfg, const char* key, float value, bool running);
/** Reads one key as float. False for unknown keys. */
bool get(const RobotConfig& cfg, const char* key, float& value);
/** True when every key is inside its range, vmin ≤ vmax and profile < PROFILE_COUNT. */
bool validate(const RobotConfig& cfg);

}  // namespace robotcfg
```

`src/config/RobotConfig.cpp`:

```cpp
#include "config/RobotConfig.h"
#include <cmath>
#include <cstring>

namespace robotcfg {
namespace {

using FT = FieldType;

#define HW_KEY(k, t, mn, mx) {#k, "hw", t, mn, mx, true, false, offsetof(HardwareConfig, k)}
#define TP_KEY(k, g, t, mn, mx, live) {#k, g, t, mn, mx, false, live, offsetof(TuningProfile, k)}

const KeyInfo KEYS[] = {
    HW_KEY(invL,  FT::U8, 0.0f, 1.0f),
    HW_KEY(invR,  FT::U8, 0.0f, 1.0f),
    HW_KEY(swap,  FT::U8, 0.0f, 1.0f),
    HW_KEY(einvL, FT::U8, 0.0f, 1.0f),
    HW_KEY(einvR, FT::U8, 0.0f, 1.0f),
    HW_KEY(pol,   FT::U8, 0.0f, 1.0f),
    HW_KEY(sinv,  FT::U8, 0.0f, 1.0f),
    HW_KEY(chem,  FT::U8, 0.0f, 1.0f),
    HW_KEY(vcut,  FT::F32, 5.6f, 7.6f),
    HW_KEY(vbk,   FT::F32, 0.8f, 1.2f),
    HW_KEY(pprL,  FT::F32, 1.0f, 10000.0f),
    HW_KEY(pprR,  FT::F32, 1.0f, 10000.0f),
    TP_KEY(kp,      "pid",  FT::F32, 0.0f,   20.0f,   true),
    TP_KEY(ki,      "pid",  FT::F32, 0.0f,   50.0f,   true),
    TP_KEY(kd,      "pid",  FT::F32, 0.0f,   2.0f,    true),
    TP_KEY(dlp,     "pid",  FT::F32, 0.05f,  1.0f,    true),
    TP_KEY(mode,    "vel",  FT::U8,  0.0f,   1.0f,    false),
    TP_KEY(kpv,     "vel",  FT::F32, 0.0f,   0.05f,   true),
    TP_KEY(kiv,     "vel",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(kdv,     "vel",  FT::F32, 0.0f,   0.01f,   true),
    TP_KEY(rpmMax,  "vel",  FT::F32, 100.0f, 2000.0f, true),
    TP_KEY(vmax,    "spd",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(vmin,    "spd",  FT::F32, 0.0f,   1.0f,    true),
    TP_KEY(thr,     "spd",  FT::F32, 0.05f,  1.0f,    true),
    TP_KEY(acc,     "spd",  FT::F32, 0.1f,   50.0f,   true),
    TP_KEY(vref,    "misc", FT::F32, 5.0f,   8.4f,    true),
    TP_KEY(vcomp,   "misc", FT::U8,  0.0f,   1.0f,    true),
    TP_KEY(lostMs,  "misc", FT::U16, 50.0f,  5000.0f, true),
    TP_KEY(startMs, "misc", FT::U16, 0.0f,   5000.0f, true),
};

#undef HW_KEY
#undef TP_KEY

constexpr size_t KEY_COUNT = sizeof(KEYS) / sizeof(KEYS[0]);

const uint8_t* fieldPtr(const RobotConfig& cfg, const KeyInfo& k) {
    const uint8_t* base = k.hardware ? reinterpret_cast<const uint8_t*>(&cfg.hw)
                                     : reinterpret_cast<const uint8_t*>(&cfg.tune);
    return base + k.offset;
}

uint8_t* fieldPtr(RobotConfig& cfg, const KeyInfo& k) {
    return const_cast<uint8_t*>(fieldPtr(static_cast<const RobotConfig&>(cfg), k));
}

float readField(const RobotConfig& cfg, const KeyInfo& k) {
    const uint8_t* p = fieldPtr(cfg, k);
    switch (k.type) {
        case FT::U8:  { uint8_t v;  std::memcpy(&v, p, sizeof v); return v; }
        case FT::U16: { uint16_t v; std::memcpy(&v, p, sizeof v); return v; }
        case FT::F32: { float v;    std::memcpy(&v, p, sizeof v); return v; }
    }
    return 0.0f;
}

void writeField(RobotConfig& cfg, const KeyInfo& k, float value) {
    uint8_t* p = fieldPtr(cfg, k);
    switch (k.type) {
        case FT::U8: {
            const uint8_t v = static_cast<uint8_t>(value);
            std::memcpy(p, &v, sizeof v);
            break;
        }
        case FT::U16: {
            const uint16_t v = static_cast<uint16_t>(value);
            std::memcpy(p, &v, sizeof v);
            break;
        }
        case FT::F32:
            std::memcpy(p, &value, sizeof value);
            break;
    }
}

}  // namespace

HardwareConfig factoryHardware() { return HardwareConfig{}; }

TuningProfile factoryProfile(uint8_t index) {
    static constexpr float VMAX[PROFILE_COUNT] = {0.35f, 0.50f, 0.70f};
    TuningProfile p{};
    p.vmax = VMAX[index < PROFILE_COUNT ? index : 0];
    return p;
}

RobotConfig factoryConfig() {
    RobotConfig c{};
    c.hw = factoryHardware();
    c.tune = factoryProfile(0);
    c.profile = 0;
    return c;
}

const KeyInfo* keyTable(size_t& count) {
    count = KEY_COUNT;
    return KEYS;
}

const KeyInfo* findKey(const char* key) {
    if (!key) return nullptr;
    for (const KeyInfo& k : KEYS) {
        if (std::strcmp(k.key, key) == 0) return &k;
    }
    return nullptr;
}

SetResult set(RobotConfig& cfg, const char* key, float value, bool running) {
    const KeyInfo* k = findKey(key);
    if (!k) return SetResult::UNKNOWN_KEY;
    if (running && !k->liveInRun) return SetResult::LOCKED_WHILE_RUNNING;
    if (!std::isfinite(value)) return SetResult::OUT_OF_RANGE;
    const float v = (k->type == FT::F32) ? value : std::round(value);
    if (v < k->min || v > k->max) return SetResult::OUT_OF_RANGE;
    if (std::strcmp(k->key, "vmin") == 0 && v > cfg.tune.vmax) return SetResult::OUT_OF_RANGE;
    if (std::strcmp(k->key, "vmax") == 0 && v < cfg.tune.vmin) return SetResult::OUT_OF_RANGE;
    writeField(cfg, *k, v);
    return SetResult::OK;
}

bool get(const RobotConfig& cfg, const char* key, float& value) {
    const KeyInfo* k = findKey(key);
    if (!k) return false;
    value = readField(cfg, *k);
    return true;
}

bool validate(const RobotConfig& cfg) {
    for (const KeyInfo& k : KEYS) {
        const float v = readField(cfg, k);
        if (!std::isfinite(v) || v < k.min || v > k.max) return false;
    }
    return cfg.tune.vmin <= cfg.tune.vmax && cfg.profile < PROFILE_COUNT;
}

}  // namespace robotcfg
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_robot_config` com 13 casos).

- [ ] **Step 5: Commit**

```bash
git add src/config/RobotConfig.h src/config/RobotConfig.cpp test/test_robot_config
git commit -m "feat(config): runtime parameter table with ranges and factory profiles

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 9: Blobs na NVS e ConfigStore com versão e CRC32

Implementa a spec §4.4. Resolve F-16.

**Files:**
- Modify: `src/storage/IPersistentStore.h`, `src/storage/InMemoryStore.h`,
  `src/storage/NvsStore.h`, `src/storage/NvsStore.cpp`
- Create: `src/config/ConfigStore.h`, `src/config/ConfigStore.cpp`
- Test: `test/test_config_store/test_config_store.cpp`

**Interfaces:**
- Consumes: `RobotConfig`, `robotcfg::validate`, `robotcfg::factory*` (Tarefa 8);
  `CalibrationData` (Tarefa 3).
- Produces:
  - `IPersistentStore::putBlob(const char* key, const void* data, size_t len) -> bool`
  - `IPersistentStore::getBlob(const char* key, void* out, size_t maxLen) -> size_t`
    (devolve o tamanho gravado; só copia se couber; 0 se não existir)
  - `ConfigStore(IPersistentStore&)`
  - `struct ConfigStore::LoadReport { bool hardwareOk; bool profileOk; };`
  - `LoadReport load(RobotConfig&)`, `bool loadHardware(HardwareConfig&)`,
    `bool loadProfile(uint8_t index, TuningProfile&)`, `uint8_t loadActiveIndex()`,
    `bool saveActive(const RobotConfig&)`, `bool saveCalibration(const CalibrationData&)`,
    `bool loadCalibration(CalibrationData&)`, `static uint32_t crc32(const uint8_t*, size_t)`
  - Formato do blob: `magic u16 (0x4C46)` · `versão u16` · `tamanho u16` · conteúdo ·
    `CRC32 u32`, tudo little-endian; CRC sobre cabeçalho + conteúdo.

- [ ] **Step 1: Escrever o teste**

`test/test_config_store/test_config_store.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "storage/InMemoryStore.h"

static InMemoryStore* mem = nullptr;
static ConfigStore*   cs  = nullptr;

void setUp() {
    mem = new InMemoryStore();
    mem->begin("lfr");
    cs = new ConfigStore(*mem);
}
void tearDown() {
    delete cs;  cs = nullptr;
    delete mem; mem = nullptr;
}

// Monta um blob no formato do ConfigStore (para forjar casos de erro)
static size_t forgeBlob(uint8_t* buf, uint16_t version, const void* payload, uint16_t size) {
    buf[0] = 0x46; buf[1] = 0x4C;
    buf[2] = version & 0xFF; buf[3] = version >> 8;
    buf[4] = size & 0xFF; buf[5] = size >> 8;
    std::memcpy(buf + 6, payload, size);
    const uint32_t crc = ConfigStore::crc32(buf, 6 + size);
    for (int i = 0; i < 4; i++) buf[6 + size + i] = static_cast<uint8_t>(crc >> (8 * i));
    return 6 + size + 4;
}

void test_config_store_crc32_known_vector() {
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ConfigStore::crc32(data, sizeof data));
}

void test_config_store_in_memory_blob_semantics() {
    const uint8_t data[4] = {1, 2, 3, 4};
    TEST_ASSERT_TRUE(mem->putBlob("b", data, sizeof data));
    TEST_ASSERT_TRUE(mem->exists("b"));
    uint8_t small[2] = {0, 0};
    TEST_ASSERT_EQUAL_UINT32(4, mem->getBlob("b", small, sizeof small));
    TEST_ASSERT_EQUAL_UINT8(0, small[0]);  // não coube: nada copiado
    uint8_t big[8] = {};
    TEST_ASSERT_EQUAL_UINT32(4, mem->getBlob("b", big, sizeof big));
    TEST_ASSERT_EQUAL_UINT8(4, big[3]);
    TEST_ASSERT_EQUAL_UINT32(0, mem->getBlob("none", big, sizeof big));
}

void test_config_store_first_boot_returns_factory_with_warning() {
    RobotConfig c{};
    c.tune.kp = 9.0f;
    const ConfigStore::LoadReport r = cs->load(c);
    TEST_ASSERT_FALSE(r.hardwareOk);
    TEST_ASSERT_FALSE(r.profileOk);
    TEST_ASSERT_EQUAL_UINT8(0, c.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, c.tune.kp);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.35f, c.tune.vmax);
}

void test_config_store_save_load_roundtrip() {
    RobotConfig c = robotcfg::factoryConfig();
    c.profile = 2;
    c.tune = robotcfg::factoryProfile(2);
    c.tune.kp = 2.25f;
    c.hw.invR = 1;
    TEST_ASSERT_TRUE(cs->saveActive(c));
    RobotConfig back{};
    const ConfigStore::LoadReport r = cs->load(back);
    TEST_ASSERT_TRUE(r.hardwareOk);
    TEST_ASSERT_TRUE(r.profileOk);
    TEST_ASSERT_EQUAL_UINT8(2, back.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.25f, back.tune.kp);
    TEST_ASSERT_EQUAL_UINT8(1, back.hw.invR);
}

void test_config_store_profiles_are_independent() {
    RobotConfig c = robotcfg::factoryConfig();
    c.profile = 1;
    c.tune.kp = 2.0f;
    cs->saveActive(c);
    c.profile = 0;
    c.tune.kp = 1.5f;
    cs->saveActive(c);
    TuningProfile p{};
    TEST_ASSERT_TRUE(cs->loadProfile(1, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, p.kp);
    TEST_ASSERT_FALSE(cs->loadProfile(2, p));  // nunca salvo → fábrica do perfil 2
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, p.vmax);
}

void test_config_store_rejects_corrupted_blob() {
    RobotConfig c = robotcfg::factoryConfig();
    c.tune.kp = 3.0f;
    cs->saveActive(c);
    uint8_t buf[128];
    const size_t len = mem->getBlob("prof0", buf, sizeof buf);
    buf[10] ^= 0xFF;
    mem->putBlob("prof0", buf, len);
    TuningProfile p{};
    TEST_ASSERT_FALSE(cs->loadProfile(0, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p.kp);
}

// Review Focus 3: firmware antigo gravou uma struct de outro tamanho
void test_config_store_rejects_blob_of_other_size() {
    uint8_t payload[20] = {};
    uint8_t buf[64];
    const size_t len = forgeBlob(buf, 1, payload, sizeof payload);
    mem->putBlob("hw", buf, len);
    HardwareConfig hw{};
    hw.invL = 1;
    TEST_ASSERT_FALSE(cs->loadHardware(hw));
    TEST_ASSERT_EQUAL_UINT8(0, hw.invL);
}

void test_config_store_rejects_out_of_range_blob() {
    TuningProfile bad = robotcfg::factoryProfile(0);
    bad.kp = 999.0f;  // CRC válido, valor fora da faixa
    uint8_t buf[128];
    const size_t len = forgeBlob(buf, 1, &bad, sizeof bad);
    mem->putBlob("prof0", buf, len);
    TuningProfile p{};
    TEST_ASSERT_FALSE(cs->loadProfile(0, p));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, p.kp);
}

void test_config_store_ppr_keys_override_hardware_blob() {
    RobotConfig c = robotcfg::factoryConfig();
    cs->saveActive(c);
    mem->putFloat("left_ppr", 1234.0f);
    HardwareConfig hw{};
    TEST_ASSERT_TRUE(cs->loadHardware(hw));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1234.0f, hw.pprL);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 600.0f, hw.pprR);
}

void test_config_store_save_writes_ppr_keys() {
    RobotConfig c = robotcfg::factoryConfig();
    c.hw.pprR = 1400.0f;
    cs->saveActive(c);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 1400.0f, mem->getFloat("right_ppr", 0.0f));
}

void test_config_store_refuses_to_save_invalid_config() {
    RobotConfig c = robotcfg::factoryConfig();
    c.tune.kp = 99.0f;
    TEST_ASSERT_FALSE(cs->saveActive(c));
    TEST_ASSERT_FALSE(mem->exists("prof0"));
}

void test_config_store_active_index_out_of_range_falls_back_to_zero() {
    const uint8_t seven = 7;
    uint8_t buf[16];
    const size_t len = forgeBlob(buf, 1, &seven, 1);
    mem->putBlob("prof_act", buf, len);
    TEST_ASSERT_EQUAL_UINT8(0, cs->loadActiveIndex());
}

void test_config_store_calibration_roundtrip() {
    CalibrationData d{};
    for (int i = 0; i < 8; i++) { d.min[i] = static_cast<int16_t>(10 * i); d.max[i] = 900; }
    TEST_ASSERT_TRUE(cs->saveCalibration(d));
    CalibrationData back{};
    TEST_ASSERT_TRUE(cs->loadCalibration(back));
    TEST_ASSERT_EQUAL_INT16(70, back.min[7]);
    TEST_ASSERT_EQUAL_INT16(900, back.max[0]);
}

void test_config_store_missing_calibration() {
    CalibrationData d{};
    TEST_ASSERT_FALSE(cs->loadCalibration(d));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_config_store_crc32_known_vector);
    RUN_TEST(test_config_store_in_memory_blob_semantics);
    RUN_TEST(test_config_store_first_boot_returns_factory_with_warning);
    RUN_TEST(test_config_store_save_load_roundtrip);
    RUN_TEST(test_config_store_profiles_are_independent);
    RUN_TEST(test_config_store_rejects_corrupted_blob);
    RUN_TEST(test_config_store_rejects_blob_of_other_size);
    RUN_TEST(test_config_store_rejects_out_of_range_blob);
    RUN_TEST(test_config_store_ppr_keys_override_hardware_blob);
    RUN_TEST(test_config_store_save_writes_ppr_keys);
    RUN_TEST(test_config_store_refuses_to_save_invalid_config);
    RUN_TEST(test_config_store_active_index_out_of_range_falls_back_to_zero);
    RUN_TEST(test_config_store_calibration_roundtrip);
    RUN_TEST(test_config_store_missing_calibration);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_config_store`
Expected: FAIL (`config/ConfigStore.h` não existe; `putBlob` não existe).

- [ ] **Step 3: Estender a interface de persistência**

`src/storage/IPersistentStore.h` — acrescentar `#include <cstddef>` e, dentro da classe:

```cpp
    /** Stores a binary blob. Returns false on failure. */
    virtual bool   putBlob(const char* key, const void* data, size_t len) = 0;
    /**
     * Returns the stored blob length (0 if missing). Copies into `out` only when the blob
     * fits in `maxLen`.
     */
    virtual size_t getBlob(const char* key, void* out, size_t maxLen) = 0;
```

`src/storage/InMemoryStore.h` — acrescentar `#include <vector>`, o mapa de blobs e os
métodos; `exists` e `clear` passam a considerar os dois mapas:

```cpp
    bool putBlob(const char* key, const void* data, size_t len) override {
        const uint8_t* p = static_cast<const uint8_t*>(data);
        _blobs[std::string(key)] = std::vector<uint8_t>(p, p + len);
        return true;
    }

    size_t getBlob(const char* key, void* out, size_t maxLen) override {
        auto it = _blobs.find(std::string(key));
        if (it == _blobs.end()) return 0;
        if (it->second.size() <= maxLen) std::memcpy(out, it->second.data(), it->second.size());
        return it->second.size();
    }

    bool exists(const char* key) const override {
        const std::string k(key);
        return _data.find(k) != _data.end() || _blobs.find(k) != _blobs.end();
    }

    void clear() override {
        _data.clear();
        _blobs.clear();
    }

private:
    std::map<std::string, float> _data;
    std::map<std::string, std::vector<uint8_t>> _blobs;
```

`src/storage/NvsStore.h` — declarar os dois métodos com `override`. `src/storage/NvsStore.cpp`
— no bloco ESP32:

```cpp
bool NvsStore::putBlob(const char* key, const void* data, size_t len) {
    if (!_open) return false;
    return _prefs.putBytes(key, data, len) == len;
}

size_t NvsStore::getBlob(const char* key, void* out, size_t maxLen) {
    if (!_open || !_prefs.isKey(key)) return 0;
    const size_t len = _prefs.getBytesLength(key);
    if (len > 0 && len <= maxLen) _prefs.getBytes(key, out, len);
    return len;
}
```

e nos stubs `NATIVE_BUILD`:

```cpp
bool   NvsStore::putBlob(const char*, const void*, size_t) { return false; }
size_t NvsStore::getBlob(const char*, void*, size_t) { return 0; }
```

- [ ] **Step 4: Implementar o ConfigStore**

`src/config/ConfigStore.h`:

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "storage/IPersistentStore.h"

/**
 * Persistência da configuração (spec §4.4). Cada bloco é um blob com magic, versão,
 * tamanho e CRC32. Qualquer falha de leitura devolve os valores de fábrica daquele bloco.
 * Chaves: "hw", "prof0".."prof2", "prof_act", "cal"; PPR em "left_ppr"/"right_ppr"
 * (as mesmas do VelocityEstimator, que continuam sendo a fonte da verdade do PPR).
 */
class ConfigStore {
public:
    struct LoadReport {
        bool hardwareOk;
        bool profileOk;
    };

    explicit ConfigStore(IPersistentStore& store);

    LoadReport load(RobotConfig& cfg);
    bool loadHardware(HardwareConfig& hw);
    /** False when absent/invalid; `out` then gets factoryProfile(index). */
    bool loadProfile(uint8_t index, TuningProfile& out);
    uint8_t loadActiveIndex();
    /** Writes hw + the active profile + the active index. Refuses invalid configs. */
    bool saveActive(const RobotConfig& cfg);
    bool saveCalibration(const CalibrationData& data);
    bool loadCalibration(CalibrationData& data);

    static uint32_t crc32(const uint8_t* data, size_t len);

private:
    bool _write(const char* key, uint16_t version, const void* payload, uint16_t size);
    bool _read(const char* key, uint16_t version, void* payload, uint16_t size);

    IPersistentStore& _store;
};
```

`src/config/ConfigStore.cpp`:

```cpp
#include "config/ConfigStore.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint16_t MAGIC = 0x4C46;  // "LF"
constexpr uint16_t HW_VERSION = 1;
constexpr uint16_t PROFILE_VERSION = 1;
constexpr uint16_t ACTIVE_VERSION = 1;
constexpr uint16_t CAL_VERSION = 1;
constexpr size_t HEADER = 6;
constexpr size_t CRC_LEN = 4;
constexpr size_t MAX_BLOB = 128;
constexpr float PPR_MIN = 1.0f;
constexpr float PPR_MAX = 10000.0f;

const char* const KEY_HW = "hw";
const char* const KEY_ACTIVE = "prof_act";
const char* const KEY_CAL = "cal";
const char* const KEY_PPR_L = "left_ppr";
const char* const KEY_PPR_R = "right_ppr";

static_assert(sizeof(HardwareConfig) + HEADER + CRC_LEN <= MAX_BLOB, "HardwareConfig blob");
static_assert(sizeof(TuningProfile) + HEADER + CRC_LEN <= MAX_BLOB, "TuningProfile blob");
static_assert(sizeof(CalibrationData) + HEADER + CRC_LEN <= MAX_BLOB, "CalibrationData blob");

void put16(uint8_t* p, uint16_t v) {
    p[0] = static_cast<uint8_t>(v);
    p[1] = static_cast<uint8_t>(v >> 8);
}
uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
void put32(uint8_t* p, uint32_t v) {
    for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

void profileKey(uint8_t index, char* out, size_t len) {
    std::snprintf(out, len, "prof%u", static_cast<unsigned>(index));
}

bool pprSane(float v) { return std::isfinite(v) && v >= PPR_MIN && v <= PPR_MAX; }

}  // namespace

ConfigStore::ConfigStore(IPersistentStore& store) : _store(store) {}

uint32_t ConfigStore::crc32(const uint8_t* data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

bool ConfigStore::_write(const char* key, uint16_t version, const void* payload, uint16_t size) {
    uint8_t buf[MAX_BLOB];
    put16(buf, MAGIC);
    put16(buf + 2, version);
    put16(buf + 4, size);
    std::memcpy(buf + HEADER, payload, size);
    put32(buf + HEADER + size, crc32(buf, HEADER + size));
    return _store.putBlob(key, buf, HEADER + size + CRC_LEN);
}

bool ConfigStore::_read(const char* key, uint16_t version, void* payload, uint16_t size) {
    uint8_t buf[MAX_BLOB];
    const size_t len = _store.getBlob(key, buf, sizeof buf);
    if (len != HEADER + size + CRC_LEN) return false;
    if (get16(buf) != MAGIC || get16(buf + 2) != version || get16(buf + 4) != size) return false;
    if (get32(buf + HEADER + size) != crc32(buf, HEADER + size)) return false;
    std::memcpy(payload, buf + HEADER, size);
    return true;
}

bool ConfigStore::loadHardware(HardwareConfig& hw) {
    HardwareConfig tmp{};
    RobotConfig probe = robotcfg::factoryConfig();
    bool ok = _read(KEY_HW, HW_VERSION, &tmp, sizeof tmp);
    if (ok) {
        probe.hw = tmp;
        ok = robotcfg::validate(probe);
    }
    hw = ok ? tmp : robotcfg::factoryHardware();
    // PPR: as chaves da auto-calibração do VelocityEstimator prevalecem
    if (_store.exists(KEY_PPR_L)) {
        const float v = _store.getFloat(KEY_PPR_L, 0.0f);
        if (pprSane(v)) hw.pprL = v;
    }
    if (_store.exists(KEY_PPR_R)) {
        const float v = _store.getFloat(KEY_PPR_R, 0.0f);
        if (pprSane(v)) hw.pprR = v;
    }
    return ok;
}

bool ConfigStore::loadProfile(uint8_t index, TuningProfile& out) {
    if (index >= PROFILE_COUNT) {
        out = robotcfg::factoryProfile(0);
        return false;
    }
    char key[8];
    profileKey(index, key, sizeof key);
    TuningProfile tmp{};
    RobotConfig probe = robotcfg::factoryConfig();
    bool ok = _read(key, PROFILE_VERSION, &tmp, sizeof tmp);
    if (ok) {
        probe.tune = tmp;
        ok = robotcfg::validate(probe);
    }
    out = ok ? tmp : robotcfg::factoryProfile(index);
    return ok;
}

uint8_t ConfigStore::loadActiveIndex() {
    uint8_t idx = 0;
    if (!_read(KEY_ACTIVE, ACTIVE_VERSION, &idx, sizeof idx) || idx >= PROFILE_COUNT) return 0;
    return idx;
}

ConfigStore::LoadReport ConfigStore::load(RobotConfig& cfg) {
    LoadReport r{};
    r.hardwareOk = loadHardware(cfg.hw);
    cfg.profile = loadActiveIndex();
    r.profileOk = loadProfile(cfg.profile, cfg.tune);
    return r;
}

bool ConfigStore::saveActive(const RobotConfig& cfg) {
    if (!robotcfg::validate(cfg)) return false;
    char key[8];
    profileKey(cfg.profile, key, sizeof key);
    bool ok = _write(KEY_HW, HW_VERSION, &cfg.hw, sizeof cfg.hw);
    ok = _write(key, PROFILE_VERSION, &cfg.tune, sizeof cfg.tune) && ok;
    ok = _write(KEY_ACTIVE, ACTIVE_VERSION, &cfg.profile, sizeof cfg.profile) && ok;
    _store.putFloat(KEY_PPR_L, cfg.hw.pprL);
    _store.putFloat(KEY_PPR_R, cfg.hw.pprR);
    return ok;
}

bool ConfigStore::saveCalibration(const CalibrationData& data) {
    return _write(KEY_CAL, CAL_VERSION, &data, sizeof data);
}

bool ConfigStore::loadCalibration(CalibrationData& data) {
    return _read(KEY_CAL, CAL_VERSION, &data, sizeof data);
}
```

- [ ] **Step 5: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_config_store` com 14 casos).

- [ ] **Step 6: Commit**

```bash
git add src/storage src/config/ConfigStore.h src/config/ConfigStore.cpp test/test_config_store
git commit -m "feat(storage): versioned CRC32 config blobs with profiles and calibration (F-16)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 10: Protocolo de comandos, frames de telemetria e fila de comandos

Implementa a spec §4.5 (parte pura).

**Files:**
- Create: `src/comm/SpinLock.h`, `src/comm/Telemetry.h`, `src/comm/CommandQueue.h`,
  `src/comm/CommandQueue.cpp`, `src/comm/CommandProtocol.h`, `src/comm/CommandProtocol.cpp`
- Test: `test/test_command_protocol/test_command_protocol.cpp`

**Interfaces:**
- Consumes: `RobotConfig`, `robotcfg::keyTable`, `SetResult` (Tarefa 8).
- Produces:
  - `SpinLock` (`lock()`/`unlock()`; portMUX no ESP32, vazio no native) e `SpinGuard` (RAII)
  - `TelemetryFrame`, `StatusFrame`, `CalReport` (`Telemetry.h`)
  - `CommandQueue::push(const char* msg, size_t len) -> bool`,
    `pop(char* out, size_t outLen) -> bool`, `size()`, `CAPACITY = 4`, `MAX_LEN = 192`
  - `enum class CmdType`, `struct SetItem { char key[8]; float value; }`, `struct Command`
  - `bool parseCommand(const char* json, Command& out)`
  - Formatadores, todos `bool f(char* buf, size_t n, …)` (false se não couber):
    `formatAck(buf, n, key, SetResult, applied)`,
    `formatAckSimple(buf, n, what, ok, err)`,
    `formatCfgGroup(buf, n, const RobotConfig&, group)`,
    `formatCfgMeta(buf, n, profile, dirty, fw)`,
    `formatInfo(buf, n, name, fw, cfgErr)`,
    `formatCalResult(buf, n, const CalReport&)`,
    `formatEncCal(buf, n, const CalReport&)`,
    `formatTelemetry(buf, n, const TelemetryFrame&)`,
    `formatStatus(buf, n, const StatusFrame&)`
  - `constexpr size_t BLE_MAX_MSG = 180;`

- [ ] **Step 1: Escrever o teste**

`test/test_command_protocol/test_command_protocol.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include <cstring>
#include "comm/CommandProtocol.h"
#include "comm/CommandQueue.h"
#include "comm/Telemetry.h"
#include "config/RobotConfig.h"

void setUp() {}
void tearDown() {}

static Command parse(const char* json) {
    Command c;
    parseCommand(json, c);
    return c;
}

void test_command_protocol_parses_set() {
    Command c;
    TEST_ASSERT_TRUE(parseCommand("{\"t\":\"set\",\"k\":\"kp\",\"v\":1.25}", c));
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
    TEST_ASSERT_EQUAL_STRING("kp", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.25f, c.items[0].value);
}

// Review Focus 1
void test_command_protocol_set_without_value_fails() {
    Command c;
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"kp\"}", c));
    TEST_ASSERT_EQUAL(CmdType::NONE, c.type);
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"kp\",\"v\":\"abc\"}", c));
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"set\",\"k\":\"muitolonga\",\"v\":1}", c));
    TEST_ASSERT_FALSE(parseCommand("{\"t\":\"se", c));
    TEST_ASSERT_FALSE(parseCommand("", c));
    TEST_ASSERT_FALSE(parseCommand(nullptr, c));
}

void test_command_protocol_set_accepts_nan_for_config_to_reject() {
    const Command c = parse("{\"t\":\"set\",\"k\":\"kp\",\"v\":nan}");
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_TRUE(std::isnan(c.items[0].value));
}

void test_command_protocol_simple_commands() {
    TEST_ASSERT_EQUAL(CmdType::GET, parse("{\"t\":\"get\"}").type);
    TEST_ASSERT_EQUAL(CmdType::SAVE, parse("{\"t\":\"save\"}").type);
    TEST_ASSERT_EQUAL(CmdType::LOAD, parse("{\"t\":\"load\"}").type);
    TEST_ASSERT_EQUAL(CmdType::DEFAULTS, parse("{\"t\":\"defaults\"}").type);
    TEST_ASSERT_EQUAL(CmdType::SENSOR_CAL, parse("{\"t\":\"scal\"}").type);
    TEST_ASSERT_EQUAL(CmdType::START, parse("{\"t\":\"start\"}").type);
    TEST_ASSERT_EQUAL(CmdType::STOP, parse("{\"t\":\"stop\"}").type);
    TEST_ASSERT_EQUAL(CmdType::RESET, parse("{\"t\":\"reset\"}").type);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"dance\"}").type);
}

void test_command_protocol_profile_switch() {
    const Command c = parse("{\"t\":\"prof\",\"n\":2,\"f\":1}");
    TEST_ASSERT_EQUAL(CmdType::PROFILE, c.type);
    TEST_ASSERT_EQUAL_UINT8(2, c.profile);
    TEST_ASSERT_TRUE(c.force);
    TEST_ASSERT_FALSE(parse("{\"t\":\"prof\",\"n\":1}").force);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"prof\"}").type);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"prof\",\"n\":1.5}").type);
}

void test_command_protocol_legacy_pid_maps_present_fields() {
    const Command c = parse("{\"t\":\"pid\",\"kp\":1.5,\"kd\":0.04}");
    TEST_ASSERT_EQUAL(CmdType::SET, c.type);
    TEST_ASSERT_EQUAL_UINT8(2, c.count);
    TEST_ASSERT_EQUAL_STRING("kp", c.items[0].key);
    TEST_ASSERT_EQUAL_STRING("kd", c.items[1].key);
}

void test_command_protocol_legacy_pidv_maps_to_inner_keys() {
    const Command c = parse("{\"t\":\"pidv\",\"kp\":0.003,\"ki\":0.01,\"kd\":0}");
    TEST_ASSERT_EQUAL_UINT8(3, c.count);
    TEST_ASSERT_EQUAL_STRING("kpv", c.items[0].key);
    TEST_ASSERT_EQUAL_STRING("kiv", c.items[1].key);
    TEST_ASSERT_EQUAL_STRING("kdv", c.items[2].key);
}

void test_command_protocol_legacy_spd_scales_from_255() {
    const Command c = parse("{\"t\":\"spd\",\"base\":160,\"min\":51,\"max\":204,\"thrs\":0.5}");
    TEST_ASSERT_EQUAL_UINT8(3, c.count);
    TEST_ASSERT_EQUAL_STRING("vmax", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.8f, c.items[0].value);
    TEST_ASSERT_EQUAL_STRING("vmin", c.items[1].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.2f, c.items[1].value);
    TEST_ASSERT_EQUAL_STRING("thr", c.items[2].key);
}

void test_command_protocol_legacy_rpm_maps_max_only() {
    const Command c = parse("{\"t\":\"rpm\",\"max\":800,\"base\":400}");
    TEST_ASSERT_EQUAL_UINT8(1, c.count);
    TEST_ASSERT_EQUAL_STRING("rpmMax", c.items[0].key);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 800.0f, c.items[0].value);
}

void test_command_protocol_legacy_pid_without_fields_fails() {
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"pid\"}").type);
}

void test_command_protocol_encoder_calibration() {
    Command c = parse("{\"t\":\"cal\",\"op\":\"finish\",\"side\":\"R\",\"rot\":20}");
    TEST_ASSERT_EQUAL(CmdType::ENC_CAL_FINISH, c.type);
    TEST_ASSERT_FALSE(c.leftSide);
    TEST_ASSERT_EQUAL_INT(20, c.rotations);
    c = parse("{\"t\":\"cal\",\"op\":\"start\",\"side\":\"L\"}");
    TEST_ASSERT_EQUAL(CmdType::ENC_CAL_START, c.type);
    TEST_ASSERT_TRUE(c.leftSide);
    TEST_ASSERT_EQUAL(CmdType::NONE, parse("{\"t\":\"cal\",\"op\":\"x\"}").type);
}

void test_command_protocol_format_ack() {
    char buf[96];
    TEST_ASSERT_TRUE(formatAck(buf, sizeof buf, "kp", SetResult::OK, 1.2f));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"kp\",\"ok\":1,\"v\":1.2}", buf);
    formatAck(buf, sizeof buf, "kp", SetResult::OUT_OF_RANGE, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"kp\",\"ok\":0,\"err\":\"range\"}", buf);
    formatAck(buf, sizeof buf, "zz", SetResult::UNKNOWN_KEY, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"zz\",\"ok\":0,\"err\":\"key\"}", buf);
    formatAck(buf, sizeof buf, "invL", SetResult::LOCKED_WHILE_RUNNING, 0.0f);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"invL\",\"ok\":0,\"err\":\"running\"}", buf);
}

void test_command_protocol_format_ack_simple() {
    char buf[96];
    formatAckSimple(buf, sizeof buf, "save", true, nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"save\",\"ok\":1}", buf);
    formatAckSimple(buf, sizeof buf, "prof", false, "dirty");
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ack\",\"k\":\"prof\",\"ok\":0,\"err\":\"dirty\"}", buf);
}

void test_command_protocol_format_cfg_group_pid() {
    char buf[200];
    const RobotConfig cfg = robotcfg::factoryConfig();
    TEST_ASSERT_TRUE(formatCfgGroup(buf, sizeof buf, cfg, "pid"));
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"cfg\",\"g\":\"pid\",\"kp\":1,\"ki\":0,\"kd\":0.03,\"dlp\":0.5}", buf);
}

void test_command_protocol_cfg_groups_fit_ble_limit() {
    char buf[256];
    RobotConfig cfg = robotcfg::factoryConfig();
    cfg.hw.vcut = 6.4321f;
    cfg.hw.pprL = 9999.99f;
    cfg.hw.pprR = 9999.99f;
    for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
        TEST_ASSERT_TRUE(formatCfgGroup(buf, sizeof buf, cfg, robotcfg::GROUPS[g]));
        TEST_ASSERT_TRUE_MESSAGE(std::strlen(buf) <= BLE_MAX_MSG, robotcfg::GROUPS[g]);
    }
}

void test_command_protocol_format_meta_and_info() {
    char buf[128];
    formatCfgMeta(buf, sizeof buf, 1, true, "4.0.0");
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"cfg\",\"g\":\"meta\",\"prof\":1,\"dirty\":1,\"fw\":\"4.0.0\"}", buf);
    formatInfo(buf, sizeof buf, "LFR-RACER-01", "4.0.0", true);
    TEST_ASSERT_EQUAL_STRING(
        "{\"t\":\"info\",\"name\":\"LFR-RACER-01\",\"fw\":\"4.0.0\",\"mode\":\"RUN\","
        "\"cfgerr\":1}", buf);
}

void test_command_protocol_format_calibration_results() {
    char buf[96];
    CalReport ok{};
    ok.ok = true;
    formatCalResult(buf, sizeof buf, ok);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"calres\",\"ok\":1}", buf);
    CalReport bad{};
    bad.bad[0] = 3;
    bad.bad[1] = 4;
    bad.nBad = 2;
    formatCalResult(buf, sizeof buf, bad);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"calres\",\"ok\":0,\"ch\":[3,4]}", buf);
    CalReport enc{};
    enc.kind = CalReport::Kind::ENCODER;
    enc.ok = true;
    enc.left = false;
    enc.ppr = 612.5f;
    formatEncCal(buf, sizeof buf, enc);
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"ecal\",\"side\":\"R\",\"ok\":1,\"ppr\":612.5}", buf);
}

void test_command_protocol_worst_case_telemetry_fits() {
    TelemetryFrame f{};
    f.pos = -1.0f;
    f.corr = -1.0f;
    f.vL = -1023;
    f.vR = -1023;
    f.dtUs = 65535;
    f.rpmL = f.rpmR = f.spL = f.spR = -32000;  // formatador limita a ±9999
    for (int i = 0; i < 8; i++) f.s[i] = 100;
    f.st = 5;
    f.lost = 1;
    char buf[256];
    TEST_ASSERT_TRUE(formatTelemetry(buf, sizeof buf, f));
    TEST_ASSERT_TRUE(std::strlen(buf) <= BLE_MAX_MSG);
    TEST_ASSERT_NOT_NULL(std::strstr(buf, "\"rpmL\":-9999"));
}

void test_command_protocol_worst_case_status_fits() {
    StatusFrame s{};
    s.bat = 100;
    s.vb = 8.4f;
    s.lap = 123.45f;
    for (int i = 0; i < 5; i++) s.laps[i] = 123.45f;
    s.nLaps = 5;
    s.err = 4;
    s.prof = 2;
    s.dirty = 1;
    s.ovr = 65535;
    char buf[256];
    TEST_ASSERT_TRUE(formatStatus(buf, sizeof buf, s));
    TEST_ASSERT_TRUE(std::strlen(buf) <= BLE_MAX_MSG);
    TEST_ASSERT_NOT_NULL(std::strstr(buf, "\"t\":\"sta\""));
}

void test_command_protocol_formatter_reports_overflow() {
    char tiny[10];
    TEST_ASSERT_FALSE(formatAckSimple(tiny, sizeof tiny, "save", true, nullptr));
}

void test_command_queue_fifo_and_capacity() {
    CommandQueue q;
    TEST_ASSERT_TRUE(q.push("a", 1));
    TEST_ASSERT_TRUE(q.push("bb", 2));
    TEST_ASSERT_TRUE(q.push("ccc", 3));
    TEST_ASSERT_TRUE(q.push("dddd", 4));
    TEST_ASSERT_FALSE(q.push("e", 1));  // cheia
    char out[16];
    TEST_ASSERT_TRUE(q.pop(out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("a", out);
    TEST_ASSERT_EQUAL_UINT32(3, q.size());
}

void test_command_queue_rejects_oversized_and_empty() {
    CommandQueue q;
    char big[CommandQueue::MAX_LEN + 1];
    std::memset(big, 'x', sizeof big);
    TEST_ASSERT_FALSE(q.push(big, sizeof big));
    TEST_ASSERT_FALSE(q.push("", 0));
    char out[4];
    TEST_ASSERT_FALSE(q.pop(out, sizeof out));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_command_protocol_parses_set);
    RUN_TEST(test_command_protocol_set_without_value_fails);
    RUN_TEST(test_command_protocol_set_accepts_nan_for_config_to_reject);
    RUN_TEST(test_command_protocol_simple_commands);
    RUN_TEST(test_command_protocol_profile_switch);
    RUN_TEST(test_command_protocol_legacy_pid_maps_present_fields);
    RUN_TEST(test_command_protocol_legacy_pidv_maps_to_inner_keys);
    RUN_TEST(test_command_protocol_legacy_spd_scales_from_255);
    RUN_TEST(test_command_protocol_legacy_rpm_maps_max_only);
    RUN_TEST(test_command_protocol_legacy_pid_without_fields_fails);
    RUN_TEST(test_command_protocol_encoder_calibration);
    RUN_TEST(test_command_protocol_format_ack);
    RUN_TEST(test_command_protocol_format_ack_simple);
    RUN_TEST(test_command_protocol_format_cfg_group_pid);
    RUN_TEST(test_command_protocol_cfg_groups_fit_ble_limit);
    RUN_TEST(test_command_protocol_format_meta_and_info);
    RUN_TEST(test_command_protocol_format_calibration_results);
    RUN_TEST(test_command_protocol_worst_case_telemetry_fits);
    RUN_TEST(test_command_protocol_worst_case_status_fits);
    RUN_TEST(test_command_protocol_formatter_reports_overflow);
    RUN_TEST(test_command_queue_fifo_and_capacity);
    RUN_TEST(test_command_queue_rejects_oversized_and_empty);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_command_protocol`
Expected: FAIL (headers não existem).

- [ ] **Step 3: Implementar `SpinLock.h` e `Telemetry.h`**

`src/comm/SpinLock.h`:

```cpp
#pragma once

#ifndef NATIVE_BUILD
#include <freertos/FreeRTOS.h>

/** Spinlock entre núcleos do ESP32 (portMUX). Seções críticas curtas: só cópias. */
class SpinLock {
public:
    void lock() { taskENTER_CRITICAL(&_mux); }
    void unlock() { taskEXIT_CRITICAL(&_mux); }

private:
    portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;
};
#else
/** Native: os testes rodam numa thread só. */
class SpinLock {
public:
    void lock() {}
    void unlock() {}
};
#endif

class SpinGuard {
public:
    explicit SpinGuard(SpinLock& lock) : _lock(lock) { _lock.lock(); }
    ~SpinGuard() { _lock.unlock(); }
    SpinGuard(const SpinGuard&) = delete;
    SpinGuard& operator=(const SpinGuard&) = delete;

private:
    SpinLock& _lock;
};
```

`src/comm/Telemetry.h`:

```cpp
#pragma once
#include <cstdint>

/** Frame rápido, 30 Hz (spec §4.5). */
struct TelemetryFrame {
    float    pos  = 0.0f;  ///< erro normalizado [-1, +1]
    float    corr = 0.0f;  ///< saída do PID externo [-1, +1]
    int16_t  vL   = 0;     ///< duty do lado esquerdo (antes de inversão/troca de canal)
    int16_t  vR   = 0;
    uint16_t dtUs = 0;     ///< tempo de execução do ciclo
    int16_t  rpmL = 0;
    int16_t  rpmR = 0;
    int16_t  spL  = 0;
    int16_t  spR  = 0;
    uint8_t  s[8] = {};    ///< sensores normalizados, %
    uint8_t  st   = 0;     ///< RobotState
    uint8_t  lost = 0;
};

/** Frame lento, 2 Hz. */
struct StatusFrame {
    uint8_t  bat     = 0;  ///< %
    float    vb      = 0.0f;
    float    lap     = 0.0f;
    float    laps[5] = {};
    uint8_t  nLaps   = 0;
    uint8_t  err     = 0;  ///< ErrorCode
    uint8_t  prof    = 0;
    uint8_t  dirty   = 0;
    uint16_t ovr     = 0;  ///< maior tempo de execução na janela, µs
};

/** Resultado de uma calibração, do core 1 para o core 0. */
struct CalReport {
    enum class Kind : uint8_t { SENSOR, ENCODER };
    Kind    kind   = Kind::SENSOR;
    bool    ok     = false;
    uint8_t bad[8] = {};
    uint8_t nBad   = 0;
    bool    left   = true;
    float   ppr    = 0.0f;
};
```

- [ ] **Step 4: Implementar a fila**

`src/comm/CommandQueue.h`:

```cpp
#pragma once
#include <cstddef>
#include "comm/SpinLock.h"

/** Fila fixa de comandos JSON (produtor: callback NimBLE; consumidor: task comm). */
class CommandQueue {
public:
    static constexpr size_t CAPACITY = 4;
    static constexpr size_t MAX_LEN = 192;

    /** False when full, empty or too long (the message is dropped). */
    bool push(const char* msg, size_t len);
    bool pop(char* out, size_t outLen);
    size_t size() const;

private:
    char   _items[CAPACITY][MAX_LEN] = {};
    size_t _head  = 0;
    size_t _count = 0;
    mutable SpinLock _lock;
};
```

`src/comm/CommandQueue.cpp`:

```cpp
#include "comm/CommandQueue.h"
#include <cstring>

bool CommandQueue::push(const char* msg, size_t len) {
    if (!msg || len == 0 || len >= MAX_LEN) return false;
    SpinGuard guard(_lock);
    if (_count == CAPACITY) return false;
    const size_t idx = (_head + _count) % CAPACITY;
    std::memcpy(_items[idx], msg, len);
    _items[idx][len] = '\0';
    _count++;
    return true;
}

bool CommandQueue::pop(char* out, size_t outLen) {
    if (!out || outLen == 0) return false;
    SpinGuard guard(_lock);
    if (_count == 0) return false;
    std::strncpy(out, _items[_head], outLen - 1);
    out[outLen - 1] = '\0';
    _head = (_head + 1) % CAPACITY;
    _count--;
    return true;
}

size_t CommandQueue::size() const {
    SpinGuard guard(_lock);
    return _count;
}
```

- [ ] **Step 5: Implementar o protocolo**

`src/comm/CommandProtocol.h`:

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "comm/Telemetry.h"
#include "config/RobotConfig.h"

constexpr size_t BLE_MAX_MSG = 180;

enum class CmdType : uint8_t {
    NONE, SET, GET, SAVE, LOAD, DEFAULTS, PROFILE, SENSOR_CAL,
    START, STOP, RESET, ENC_CAL_START, ENC_CAL_FINISH
};

struct SetItem {
    char  key[8];
    float value;
};

struct Command {
    static constexpr uint8_t MAX_ITEMS = 4;
    CmdType type = CmdType::NONE;
    SetItem items[MAX_ITEMS] = {};
    uint8_t count = 0;
    uint8_t profile = 0;
    bool    force = false;
    bool    leftSide = true;
    int     rotations = 0;
};

/**
 * Parses one command (spec §4.5). New: set/get/save/load/defaults/prof/scal.
 * Legacy: pid/pidv/spd/rpm become SET items; cal/start/stop/reset keep their meaning.
 * Returns false (and type NONE) for malformed or unknown commands.
 */
bool parseCommand(const char* json, Command& out);

bool formatAck(char* buf, size_t n, const char* key, SetResult result, float applied);
bool formatAckSimple(char* buf, size_t n, const char* what, bool ok, const char* err);
bool formatCfgGroup(char* buf, size_t n, const RobotConfig& cfg, const char* group);
bool formatCfgMeta(char* buf, size_t n, uint8_t profile, bool dirty, const char* fw);
bool formatInfo(char* buf, size_t n, const char* name, const char* fw, bool cfgErr);
bool formatCalResult(char* buf, size_t n, const CalReport& report);
bool formatEncCal(char* buf, size_t n, const CalReport& report);
bool formatTelemetry(char* buf, size_t n, const TelemetryFrame& f);
bool formatStatus(char* buf, size_t n, const StatusFrame& s);
```

`src/comm/CommandProtocol.cpp`:

```cpp
#include "comm/CommandProtocol.h"
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

/** Escreve em sequência num buffer; ok() fica false se algo não coube. */
class Writer {
public:
    Writer(char* buf, size_t n) : _buf(buf), _n(n) {
        if (n > 0) buf[0] = '\0';
        else _ok = false;
    }

    void add(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        if (!_ok) return;
        va_list ap;
        va_start(ap, fmt);
        const int r = std::vsnprintf(_buf + _len, _n - _len, fmt, ap);
        va_end(ap);
        if (r < 0 || static_cast<size_t>(r) >= _n - _len) {
            _ok = false;
            return;
        }
        _len += static_cast<size_t>(r);
    }

    bool ok() const { return _ok; }

private:
    char*  _buf;
    size_t _n;
    size_t _len = 0;
    bool   _ok = true;
};

const char* findValue(const char* json, const char* key) {
    char pattern[24];
    const int n = std::snprintf(pattern, sizeof pattern, "\"%s\":", key);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof pattern) return nullptr;
    const char* p = std::strstr(json, pattern);
    if (!p) return nullptr;
    p += n;
    while (*p == ' ') p++;
    return p;
}

bool readNumber(const char* json, const char* key, float& out) {
    const char* p = findValue(json, key);
    if (!p) return false;
    char* end = nullptr;
    const float v = std::strtof(p, &end);
    if (end == p) return false;
    out = v;
    return true;
}

bool readString(const char* json, const char* key, char* out, size_t len) {
    const char* p = findValue(json, key);
    if (!p || *p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"') {
        if (i + 1 >= len) return false;
        out[i++] = *p++;
    }
    if (*p != '"') return false;
    out[i] = '\0';
    return true;
}

bool addItem(Command& c, const char* key, float value) {
    const size_t len = std::strlen(key);
    if (c.count >= Command::MAX_ITEMS || len >= sizeof(c.items[0].key)) return false;
    std::memcpy(c.items[c.count].key, key, len + 1);
    c.items[c.count].value = value;
    c.count++;
    return true;
}

void addIfPresent(Command& c, const char* json, const char* jsonKey, const char* cfgKey,
                  float scale) {
    float v = 0.0f;
    if (readNumber(json, jsonKey, v)) addItem(c, cfgKey, v * scale);
}

bool fail(Command& out) {
    out = Command{};
    return false;
}

bool setOf(Command& out) {
    if (out.count == 0) return fail(out);
    out.type = CmdType::SET;
    return true;
}

const char* errText(SetResult r) {
    switch (r) {
        case SetResult::UNKNOWN_KEY: return "key";
        case SetResult::OUT_OF_RANGE: return "range";
        case SetResult::LOCKED_WHILE_RUNNING: return "running";
        case SetResult::OK: break;
    }
    return "";
}

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

}  // namespace

bool parseCommand(const char* json, Command& out) {
    out = Command{};
    if (!json) return false;
    char t[12];
    if (!readString(json, "t", t, sizeof t)) return false;

    if (std::strcmp(t, "set") == 0) {
        char k[8];
        float v = 0.0f;
        if (!readString(json, "k", k, sizeof k) || !readNumber(json, "v", v)) return fail(out);
        if (!addItem(out, k, v)) return fail(out);
        out.type = CmdType::SET;
        return true;
    }
    if (std::strcmp(t, "pid") == 0) {
        addIfPresent(out, json, "kp", "kp", 1.0f);
        addIfPresent(out, json, "ki", "ki", 1.0f);
        addIfPresent(out, json, "kd", "kd", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "pidv") == 0) {
        addIfPresent(out, json, "kp", "kpv", 1.0f);
        addIfPresent(out, json, "ki", "kiv", 1.0f);
        addIfPresent(out, json, "kd", "kdv", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "spd") == 0) {  // legado em 0–255; "base" não existe mais (F-15)
        addIfPresent(out, json, "max", "vmax", 1.0f / 255.0f);
        addIfPresent(out, json, "min", "vmin", 1.0f / 255.0f);
        addIfPresent(out, json, "thrs", "thr", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "rpm") == 0) {
        addIfPresent(out, json, "max", "rpmMax", 1.0f);
        return setOf(out);
    }
    if (std::strcmp(t, "prof") == 0) {
        float n = 0.0f;
        if (!readNumber(json, "n", n) || n < 0.0f || n > 255.0f || n != std::floor(n)) {
            return fail(out);
        }
        float f = 0.0f;
        out.type = CmdType::PROFILE;
        out.profile = static_cast<uint8_t>(n);
        out.force = readNumber(json, "f", f) && f != 0.0f;
        return true;
    }
    if (std::strcmp(t, "cal") == 0) {
        char op[8];
        if (!readString(json, "op", op, sizeof op)) return fail(out);
        if (std::strcmp(op, "start") == 0) out.type = CmdType::ENC_CAL_START;
        else if (std::strcmp(op, "finish") == 0) out.type = CmdType::ENC_CAL_FINISH;
        else return fail(out);
        float rot = 10.0f;
        readNumber(json, "rot", rot);
        out.rotations = static_cast<int>(rot);
        char side[4];
        out.leftSide = !(readString(json, "side", side, sizeof side) && side[0] == 'R');
        return true;
    }

    struct Simple { const char* name; CmdType type; };
    static const Simple SIMPLE[] = {
        {"get", CmdType::GET},           {"save", CmdType::SAVE},
        {"load", CmdType::LOAD},         {"defaults", CmdType::DEFAULTS},
        {"scal", CmdType::SENSOR_CAL},   {"start", CmdType::START},
        {"stop", CmdType::STOP},         {"reset", CmdType::RESET},
    };
    for (const Simple& s : SIMPLE) {
        if (std::strcmp(t, s.name) == 0) {
            out.type = s.type;
            return true;
        }
    }
    return fail(out);
}

bool formatAck(char* buf, size_t n, const char* key, SetResult result, float applied) {
    Writer w(buf, n);
    if (result == SetResult::OK) {
        w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":1,\"v\":%.6g}", key,
              static_cast<double>(applied));
    } else {
        w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":0,\"err\":\"%s\"}", key, errText(result));
    }
    return w.ok();
}

bool formatAckSimple(char* buf, size_t n, const char* what, bool ok, const char* err) {
    Writer w(buf, n);
    if (ok) w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":1}", what);
    else w.add("{\"t\":\"ack\",\"k\":\"%s\",\"ok\":0,\"err\":\"%s\"}", what, err ? err : "fail");
    return w.ok();
}

bool formatCfgGroup(char* buf, size_t n, const RobotConfig& cfg, const char* group) {
    Writer w(buf, n);
    w.add("{\"t\":\"cfg\",\"g\":\"%s\"", group);
    size_t count = 0;
    const robotcfg::KeyInfo* keys = robotcfg::keyTable(count);
    for (size_t i = 0; i < count; i++) {
        if (std::strcmp(keys[i].group, group) != 0) continue;
        float v = 0.0f;
        robotcfg::get(cfg, keys[i].key, v);
        if (keys[i].type == robotcfg::FieldType::F32) {
            w.add(",\"%s\":%.6g", keys[i].key, static_cast<double>(v));
        } else {
            w.add(",\"%s\":%ld", keys[i].key, std::lround(v));
        }
    }
    w.add("}");
    return w.ok();
}

bool formatCfgMeta(char* buf, size_t n, uint8_t profile, bool dirty, const char* fw) {
    Writer w(buf, n);
    w.add("{\"t\":\"cfg\",\"g\":\"meta\",\"prof\":%u,\"dirty\":%u,\"fw\":\"%s\"}",
          static_cast<unsigned>(profile), dirty ? 1u : 0u, fw);
    return w.ok();
}

bool formatInfo(char* buf, size_t n, const char* name, const char* fw, bool cfgErr) {
    Writer w(buf, n);
    w.add("{\"t\":\"info\",\"name\":\"%s\",\"fw\":\"%s\",\"mode\":\"RUN\",\"cfgerr\":%u}",
          name, fw, cfgErr ? 1u : 0u);
    return w.ok();
}

bool formatCalResult(char* buf, size_t n, const CalReport& r) {
    Writer w(buf, n);
    if (r.ok) {
        w.add("{\"t\":\"calres\",\"ok\":1}");
        return w.ok();
    }
    w.add("{\"t\":\"calres\",\"ok\":0,\"ch\":[");
    for (uint8_t i = 0; i < r.nBad && i < 8; i++) {
        w.add(i == 0 ? "%u" : ",%u", static_cast<unsigned>(r.bad[i]));
    }
    w.add("]}");
    return w.ok();
}

bool formatEncCal(char* buf, size_t n, const CalReport& r) {
    Writer w(buf, n);
    w.add("{\"t\":\"ecal\",\"side\":\"%s\",\"ok\":%u,\"ppr\":%.1f}",
          r.left ? "L" : "R", r.ok ? 1u : 0u, static_cast<double>(r.ppr));
    return w.ok();
}

bool formatTelemetry(char* buf, size_t n, const TelemetryFrame& f) {
    Writer w(buf, n);
    w.add("{\"t\":\"tel\",\"pos\":%.3f,\"corr\":%.3f,\"vL\":%d,\"vR\":%d,\"dt\":%u,",
          static_cast<double>(f.pos), static_cast<double>(f.corr),
          clampInt(f.vL, -9999, 9999), clampInt(f.vR, -9999, 9999),
          static_cast<unsigned>(f.dtUs));
    w.add("\"rpmL\":%d,\"rpmR\":%d,\"spL\":%d,\"spR\":%d,\"s\":[",
          clampInt(f.rpmL, -9999, 9999), clampInt(f.rpmR, -9999, 9999),
          clampInt(f.spL, -9999, 9999), clampInt(f.spR, -9999, 9999));
    for (int i = 0; i < 8; i++) {
        w.add(i == 0 ? "%u" : ",%u", static_cast<unsigned>(std::min<uint8_t>(f.s[i], 100)));
    }
    w.add("],\"st\":%u,\"lost\":%u}", static_cast<unsigned>(f.st), f.lost ? 1u : 0u);
    return w.ok();
}

bool formatStatus(char* buf, size_t n, const StatusFrame& s) {
    Writer w(buf, n);
    w.add("{\"t\":\"sta\",\"bat\":%u,\"vb\":%.2f,\"lap\":%.2f,\"laps\":[",
          static_cast<unsigned>(s.bat), static_cast<double>(s.vb), static_cast<double>(s.lap));
    for (uint8_t i = 0; i < s.nLaps && i < 5; i++) {
        w.add(i == 0 ? "%.2f" : ",%.2f", static_cast<double>(s.laps[i]));
    }
    w.add("],\"err\":%u,\"prof\":%u,\"dirty\":%u,\"ovr\":%u}",
          static_cast<unsigned>(s.err), static_cast<unsigned>(s.prof),
          s.dirty ? 1u : 0u, static_cast<unsigned>(s.ovr));
    return w.ok();
}
```

O `-Wformat-nonliteral` não está ativo, mas `w.add(i == 0 ? "%u" : ",%u", …)` passa pelo
atributo `format`; se o GCC reclamar, trocar por dois `if` com literais.

- [ ] **Step 6: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_command_protocol` com 22 casos).

- [ ] **Step 7: Commit**

```bash
git add src/comm/SpinLock.h src/comm/Telemetry.h src/comm/CommandQueue.h \
        src/comm/CommandQueue.cpp src/comm/CommandProtocol.h src/comm/CommandProtocol.cpp \
        test/test_command_protocol
git commit -m "feat(comm): JSON command protocol, telemetry frames and command queue

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 11: ButtonInput — debounce, toque curto e toque longo

Resolve F-04.

**Files:**
- Create: `src/sensors/ButtonInput.h`, `src/sensors/ButtonInput.cpp`
- Test: `test/test_button/test_button.cpp`

**Interfaces:**
- Produces: `enum class ButtonEvent : uint8_t { NONE, SHORT, LONG };`,
  `ButtonInput(uint32_t debounceMs = 30, uint32_t shortMaxMs = 800, uint32_t longMs = 1500)`,
  `ButtonEvent update(bool pressed, uint32_t nowMs)`, `bool isPressed() const`.

- [ ] **Step 1: Escrever o teste**

`test/test_button/test_button.cpp`:

```cpp
#include <unity.h>
#include "sensors/ButtonInput.h"

void setUp() {}
void tearDown() {}

struct Counts {
    int shortCount = 0;
    int longCount = 0;
};

// Alimenta o botão com o mesmo nível de `from` até `to` (exclusivo), em passos de 1 ms
static void feed(ButtonInput& b, bool pressed, uint32_t from, uint32_t to, Counts& c) {
    for (uint32_t t = from; t < to; t++) {
        const ButtonEvent e = b.update(pressed, t);
        if (e == ButtonEvent::SHORT) c.shortCount++;
        if (e == ButtonEvent::LONG) c.longCount++;
    }
}

void test_button_short_press_fires_once_on_release() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 100, c);
    feed(b, true, 100, 300, c);
    feed(b, false, 300, 400, c);
    TEST_ASSERT_EQUAL_INT(1, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

void test_button_bounce_is_filtered() {
    ButtonInput b;
    Counts c;
    for (uint32_t t = 0; t < 20; t++) {
        const ButtonEvent e = b.update((t / 5) % 2 == 0, t);  // quica a cada 5 ms
        if (e != ButtonEvent::NONE) c.shortCount++;
    }
    feed(b, true, 20, 220, c);
    feed(b, false, 220, 320, c);
    TEST_ASSERT_EQUAL_INT(1, c.shortCount);
}

void test_button_glitch_shorter_than_debounce_is_ignored() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 50, c);
    feed(b, true, 50, 60, c);  // 10 ms < 30 ms
    feed(b, false, 60, 200, c);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

void test_button_long_press_fires_while_held_and_not_short() {
    ButtonInput b;
    Counts c;
    feed(b, false, 0, 10, c);
    feed(b, true, 10, 1600, c);  // pressedAt = 40; LONG em 1540
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    feed(b, true, 1600, 2500, c);
    feed(b, false, 2500, 2600, c);
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
}

void test_button_medium_press_fires_nothing() {
    ButtonInput b;
    Counts c;
    feed(b, true, 0, 1000, c);
    feed(b, false, 1000, 1100, c);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
    TEST_ASSERT_EQUAL_INT(0, c.longCount);
}

// Review Focus 4
void test_button_held_from_boot_fires_single_long() {
    ButtonInput b;
    Counts c;
    feed(b, true, 0, 5000, c);
    feed(b, false, 5000, 5100, c);
    TEST_ASSERT_EQUAL_INT(1, c.longCount);
    TEST_ASSERT_EQUAL_INT(0, c.shortCount);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_button_short_press_fires_once_on_release);
    RUN_TEST(test_button_bounce_is_filtered);
    RUN_TEST(test_button_glitch_shorter_than_debounce_is_ignored);
    RUN_TEST(test_button_long_press_fires_while_held_and_not_short);
    RUN_TEST(test_button_medium_press_fires_nothing);
    RUN_TEST(test_button_held_from_boot_fires_single_long);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_button`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/sensors/ButtonInput.h`:

```cpp
#pragma once
#include <cstdint>

enum class ButtonEvent : uint8_t { NONE, SHORT, LONG };

/**
 * Botão com debounce e toque curto/longo (spec §4.6, F-04).
 * SHORT: soltar antes de shortMaxMs. LONG: dispara ao completar longMs segurando, sem esperar
 * soltar. Entre os dois, nada. Depois de um LONG, soltar não gera SHORT.
 */
class ButtonInput {
public:
    explicit ButtonInput(uint32_t debounceMs = 30, uint32_t shortMaxMs = 800,
                         uint32_t longMs = 1500);

    ButtonEvent update(bool pressed, uint32_t nowMs);
    bool isPressed() const { return _stable; }

private:
    uint32_t _debounceMs;
    uint32_t _shortMaxMs;
    uint32_t _longMs;
    bool     _raw = false;
    bool     _stable = false;
    bool     _longFired = false;
    uint32_t _rawSince = 0;
    uint32_t _pressedAt = 0;
};
```

`src/sensors/ButtonInput.cpp`:

```cpp
#include "sensors/ButtonInput.h"

ButtonInput::ButtonInput(uint32_t debounceMs, uint32_t shortMaxMs, uint32_t longMs)
    : _debounceMs(debounceMs), _shortMaxMs(shortMaxMs), _longMs(longMs) {}

ButtonEvent ButtonInput::update(bool pressed, uint32_t nowMs) {
    if (pressed != _raw) {
        _raw = pressed;
        _rawSince = nowMs;
    }
    if (_raw != _stable && nowMs - _rawSince >= _debounceMs) {
        _stable = _raw;
        if (_stable) {
            _pressedAt = nowMs;
            _longFired = false;
        } else {
            const bool wasLong = _longFired;
            _longFired = false;
            if (!wasLong && nowMs - _pressedAt < _shortMaxMs) return ButtonEvent::SHORT;
            return ButtonEvent::NONE;
        }
    }
    if (_stable && !_longFired && nowMs - _pressedAt >= _longMs) {
        _longFired = true;
        return ButtonEvent::LONG;
    }
    return ButtonEvent::NONE;
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/sensors/ButtonInput.h src/sensors/ButtonInput.cpp test/test_button
git commit -m "feat(button): debounced short/long press detection (F-04)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 12: BatteryMonitor — tensão filtrada, % por química e corte com retenção

Resolve F-10.

**Files:**
- Create: `src/sensors/BatteryMonitor.h`, `src/sensors/BatteryMonitor.cpp`
- Test: `test/test_battery/test_battery.cpp`

**Interfaces:**
- Consumes: `HardwareConfig` (`vcut`, `vbk`, `chem`) — Tarefa 8.
- Produces: `BatteryMonitor(float dividerRatio, float tauS = 0.5f, uint32_t lowHoldMs = 1000)`,
  `void update(uint32_t adcMilliVolts, float dtS, uint32_t nowMs, const HardwareConfig& hw)`,
  `bool hasReading() const`, `float volts() const`, `bool isLow() const`,
  `uint8_t percent(uint8_t chem) const`, `static uint8_t percentFor(float volts, uint8_t chem)`.

- [ ] **Step 1: Escrever o teste**

`test/test_battery/test_battery.cpp`:

```cpp
#include <unity.h>
#include "config/RobotConfig.h"
#include "sensors/BatteryMonitor.h"

static constexpr float RATIO = 147.0f / 47.0f;
static HardwareConfig hw;

void setUp() { hw = robotcfg::factoryHardware(); }
void tearDown() {}

void test_battery_first_sample_seeds_filter() {
    BatteryMonitor b(RATIO);
    TEST_ASSERT_FALSE(b.hasReading());
    b.update(2687, 0.1f, 0, hw);  // 2,687 V no ADC → 8,404 V na bateria
    TEST_ASSERT_TRUE(b.hasReading());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.404f, b.volts());
}

void test_battery_correction_factor_applies() {
    BatteryMonitor b(RATIO);
    hw.vbk = 1.1f;
    b.update(2000, 0.1f, 0, hw);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.0f * RATIO * 1.1f, b.volts());
}

void test_battery_filter_follows_step_with_time_constant() {
    BatteryMonitor b(RATIO, 0.5f);
    b.update(2400, 0.1f, 0, hw);
    const float start = b.volts();
    const float target = 2.2f * RATIO;
    uint32_t t = 0;
    for (int i = 0; i < 5; i++) b.update(2200, 0.1f, t += 100, hw);  // 0,5 s = 1 τ
    const float progress = (start - b.volts()) / (start - target);
    TEST_ASSERT_FLOAT_WITHIN(0.08f, 0.63f, progress);
}

void test_battery_low_requires_hold_time() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(1950, 0.1f, 0, hw);  // ≈ 6,1 V < 6,4 V
    TEST_ASSERT_FALSE(b.isLow());
    b.update(1950, 0.1f, 999, hw);
    TEST_ASSERT_FALSE(b.isLow());
    b.update(1950, 0.1f, 1000, hw);
    TEST_ASSERT_TRUE(b.isLow());
}

void test_battery_recovery_clears_low() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(1950, 0.1f, 0, hw);
    b.update(1950, 0.1f, 1500, hw);
    TEST_ASSERT_TRUE(b.isLow());
    b.update(2500, 0.1f, 1600, hw);
    TEST_ASSERT_FALSE(b.isLow());
}

// Review Focus 2: divisor solto → 0 mV
void test_battery_zero_reading_goes_low() {
    BatteryMonitor b(RATIO, 0.01f, 1000);
    b.update(0, 0.1f, 0, hw);
    b.update(0, 0.1f, 1000, hw);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, b.volts());
    TEST_ASSERT_TRUE(b.isLow());
    TEST_ASSERT_EQUAL_UINT8(0, b.percent(1));
}

void test_battery_percent_tables() {
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::percentFor(8.4f, 1));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::percentFor(6.0f, 1));
    TEST_ASSERT_EQUAL_UINT8(40, BatteryMonitor::percentFor(7.2f, 1));
    TEST_ASSERT_EQUAL_UINT8(100, BatteryMonitor::percentFor(9.0f, 1));
    TEST_ASSERT_EQUAL_UINT8(0, BatteryMonitor::percentFor(6.6f, 0));
    TEST_ASSERT_EQUAL_UINT8(40, BatteryMonitor::percentFor(7.4f, 0));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_battery_first_sample_seeds_filter);
    RUN_TEST(test_battery_correction_factor_applies);
    RUN_TEST(test_battery_filter_follows_step_with_time_constant);
    RUN_TEST(test_battery_low_requires_hold_time);
    RUN_TEST(test_battery_recovery_clears_low);
    RUN_TEST(test_battery_zero_reading_goes_low);
    RUN_TEST(test_battery_percent_tables);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_battery`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/sensors/BatteryMonitor.h`:

```cpp
#pragma once
#include <cstdint>
#include "config/RobotConfig.h"

/**
 * Tensão da bateria a partir do ADC (mV já calibrados pelo eFuse, F-10).
 * V = mV/1000 × divisor × vbk, filtrado por um passa-baixa de 1ª ordem (constante tauS).
 * isLow() só fica verdadeiro depois de lowHoldMs contínuos abaixo de vcut.
 */
class BatteryMonitor {
public:
    explicit BatteryMonitor(float dividerRatio, float tauS = 0.5f, uint32_t lowHoldMs = 1000);

    void update(uint32_t adcMilliVolts, float dtS, uint32_t nowMs, const HardwareConfig& hw);

    bool    hasReading() const { return _has; }
    float   volts() const { return _volts; }
    bool    isLow() const { return _low; }
    uint8_t percent(uint8_t chem) const { return percentFor(_volts, chem); }

    /** Piecewise-linear state of charge for a 2S pack. chem: 0 = LiPo, 1 = Li-ion. */
    static uint8_t percentFor(float volts, uint8_t chem);

private:
    float    _ratio;
    float    _tauS;
    uint32_t _lowHoldMs;
    bool     _has = false;
    float    _volts = 0.0f;
    bool     _below = false;
    uint32_t _belowSince = 0;
    bool     _low = false;
};
```

`src/sensors/BatteryMonitor.cpp`:

```cpp
#include "sensors/BatteryMonitor.h"
#include <cmath>

namespace {

struct Point {
    float v;
    float pct;
};

// Curvas de 2S em repouso; valores típicos de mercado (só para indicação)
const Point LIION[] = {{6.0f, 0}, {6.8f, 10}, {7.2f, 40}, {7.6f, 70}, {8.0f, 90}, {8.4f, 100}};
const Point LIPO[]  = {{6.6f, 0}, {7.2f, 15}, {7.4f, 40}, {7.7f, 70}, {8.0f, 90}, {8.4f, 100}};

uint8_t interpolate(const Point* pts, int n, float v) {
    if (!std::isfinite(v) || v <= pts[0].v) return 0;
    if (v >= pts[n - 1].v) return 100;
    for (int i = 1; i < n; i++) {
        if (v <= pts[i].v) {
            const float t = (v - pts[i - 1].v) / (pts[i].v - pts[i - 1].v);
            const float pct = pts[i - 1].pct + t * (pts[i].pct - pts[i - 1].pct);
            return static_cast<uint8_t>(std::lround(pct));
        }
    }
    return 100;
}

}  // namespace

BatteryMonitor::BatteryMonitor(float dividerRatio, float tauS, uint32_t lowHoldMs)
    : _ratio(dividerRatio), _tauS(tauS > 0.0f ? tauS : 0.001f), _lowHoldMs(lowHoldMs) {}

void BatteryMonitor::update(uint32_t adcMilliVolts, float dtS, uint32_t nowMs,
                            const HardwareConfig& hw) {
    const float v = static_cast<float>(adcMilliVolts) / 1000.0f * _ratio * hw.vbk;
    if (!_has) {
        _volts = v;
        _has = true;
    } else {
        const float dt = (std::isfinite(dtS) && dtS > 0.0f) ? dtS : 0.0f;
        _volts += (dt / (_tauS + dt)) * (v - _volts);
    }
    if (_volts < hw.vcut) {
        if (!_below) {
            _below = true;
            _belowSince = nowMs;
        }
        _low = nowMs - _belowSince >= _lowHoldMs;
    } else {
        _below = false;
        _low = false;
    }
}

uint8_t BatteryMonitor::percentFor(float volts, uint8_t chem) {
    return chem == 0 ? interpolate(LIPO, 6, volts) : interpolate(LIION, 6, volts);
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/sensors/BatteryMonitor.h src/sensors/BatteryMonitor.cpp test/test_battery
git commit -m "feat(battery): filtered voltage, Li-ion/LiPo percent and held low-battery flag (F-10)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 13: OutputStage — compensação de tensão, inversão e troca de motores

Implementa a spec §4.7 passo 6.

**Files:**
- Create: `src/motors/OutputStage.h`, `src/motors/OutputStage.cpp`
- Test: `test/test_output_stage/test_output_stage.cpp`

**Interfaces:**
- Consumes: `HardwareConfig`, `TuningProfile` (Tarefa 8).
- Produces: `struct MotorDuties { int a; int b; };`, `explicit OutputStage(int pwmMax)`,
  `MotorDuties compute(float uLeft, float uRight, float vbat, const HardwareConfig&,
  const TuningProfile&) const`, `OutputStage::MIN_VALID_VBAT = 3.0f`.
- Convenção: sem `swap`, esquerdo → canal A e direito → canal B (como no `main.cpp` atual).

- [ ] **Step 1: Escrever o teste**

`test/test_output_stage/test_output_stage.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "config/RobotConfig.h"
#include "motors/OutputStage.h"

static HardwareConfig hw;
static TuningProfile tune;
static const OutputStage out(1023);

void setUp() {
    hw = robotcfg::factoryHardware();
    tune = robotcfg::factoryProfile(0);
}
void tearDown() {}

void test_output_stage_passthrough_without_compensation() {
    tune.vcomp = 0;
    const MotorDuties d = out.compute(0.5f, -0.25f, 8.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(512, d.a);   // round(0,5 × 1023)
    TEST_ASSERT_EQUAL_INT(-256, d.b);
}

void test_output_stage_compensates_full_battery() {
    const MotorDuties d = out.compute(0.5f, 0.5f, 8.4f, hw, tune);  // × 7,4/8,4
    TEST_ASSERT_EQUAL_INT(451, d.a);
}

void test_output_stage_compensates_sagging_battery() {
    const MotorDuties d = out.compute(0.5f, 0.5f, 6.4f, hw, tune);  // × 7,4/6,4
    TEST_ASSERT_EQUAL_INT(591, d.a);
}

void test_output_stage_saturates_after_compensation() {
    const MotorDuties d = out.compute(1.0f, -1.0f, 6.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(1023, d.a);
    TEST_ASSERT_EQUAL_INT(-1023, d.b);
}

// Review Focus 2
void test_output_stage_invalid_vbat_disables_compensation() {
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, 0.0f, hw, tune).a);
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, NAN, hw, tune).a);
    TEST_ASSERT_EQUAL_INT(512, out.compute(0.5f, 0.5f, 2.9f, hw, tune).a);
}

void test_output_stage_inverts_each_side() {
    tune.vcomp = 0;
    hw.invL = 1;
    const MotorDuties d = out.compute(0.5f, 0.5f, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(-512, d.a);
    TEST_ASSERT_EQUAL_INT(512, d.b);
}

void test_output_stage_swaps_channels() {
    tune.vcomp = 0;
    hw.swap = 1;
    const MotorDuties d = out.compute(0.5f, -0.25f, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(-256, d.a);
    TEST_ASSERT_EQUAL_INT(512, d.b);
}

void test_output_stage_non_finite_command_is_zero() {
    const MotorDuties d = out.compute(NAN, INFINITY, 7.4f, hw, tune);
    TEST_ASSERT_EQUAL_INT(0, d.a);
    TEST_ASSERT_EQUAL_INT(0, d.b);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_output_stage_passthrough_without_compensation);
    RUN_TEST(test_output_stage_compensates_full_battery);
    RUN_TEST(test_output_stage_compensates_sagging_battery);
    RUN_TEST(test_output_stage_saturates_after_compensation);
    RUN_TEST(test_output_stage_invalid_vbat_disables_compensation);
    RUN_TEST(test_output_stage_inverts_each_side);
    RUN_TEST(test_output_stage_swaps_channels);
    RUN_TEST(test_output_stage_non_finite_command_is_zero);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_output_stage`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/motors/OutputStage.h`:

```cpp
#pragma once
#include "config/RobotConfig.h"

/** Duty com sinal por canal do TB6612 (A, B), em [-pwmMax, +pwmMax]. */
struct MotorDuties {
    int a;
    int b;
};

/**
 * Última etapa do laço (spec §4.7, passo 6): compensação de tensão (× vref/vbat), saturação
 * em [-1, +1], inversão por lado, troca de canal e conversão para duty inteiro.
 * Leitura de bateria abaixo de MIN_VALID_VBAT é tratada como falha: sem compensação.
 */
class OutputStage {
public:
    static constexpr float MIN_VALID_VBAT = 3.0f;

    explicit OutputStage(int pwmMax) : _pwmMax(pwmMax) {}

    MotorDuties compute(float uLeft, float uRight, float vbat,
                        const HardwareConfig& hw, const TuningProfile& tune) const;

private:
    int _pwmMax;
};
```

`src/motors/OutputStage.cpp`:

```cpp
#include "motors/OutputStage.h"
#include <algorithm>
#include <cmath>

static float clampUnit(float v) {
    if (!std::isfinite(v)) return 0.0f;
    return std::max(-1.0f, std::min(1.0f, v));
}

MotorDuties OutputStage::compute(float uLeft, float uRight, float vbat,
                                 const HardwareConfig& hw, const TuningProfile& tune) const {
    float k = 1.0f;
    if (tune.vcomp && std::isfinite(vbat) && vbat >= MIN_VALID_VBAT) k = tune.vref / vbat;
    float l = clampUnit(clampUnit(uLeft) * k);
    float r = clampUnit(clampUnit(uRight) * k);
    if (hw.invL) l = -l;
    if (hw.invR) r = -r;
    const int dl = static_cast<int>(std::lround(l * static_cast<float>(_pwmMax)));
    const int dr = static_cast<int>(std::lround(r * static_cast<float>(_pwmMax)));
    return hw.swap ? MotorDuties{dr, dl} : MotorDuties{dl, dr};
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/motors/OutputStage.h src/motors/OutputStage.cpp test/test_output_stage
git commit -m "feat(motors): output stage with battery compensation, inversion and channel swap

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 14: RobotStateMachine

Implementa a tabela de estados da spec §4.6.

**Files:**
- Create: `src/strategy/RobotStateMachine.h`, `src/strategy/RobotStateMachine.cpp`
- Test: `test/test_state_machine/test_state_machine.cpp`

**Interfaces:**
- Produces:
  - `enum class RobotState : uint8_t { IDLE, CALIBRATING, READY, ARMED, RUNNING, ERROR };`
  - `enum class ErrorCode : uint8_t { NONE = 0, OVR = 1, LINE = 2, BAT = 3, CAL = 4 };`
  - `enum class SmEvent : uint8_t { BTN_SHORT, BTN_LONG, CMD_START, CMD_STOP, CMD_RESET,
    CMD_SENSOR_CAL, CAL_DONE_OK, CAL_DONE_FAIL, LINE_LOST_TIMEOUT, BATTERY_LOW,
    LOOP_OVERRUN };`
  - `struct SmInputs { bool calibrationValid; bool batteryOk; };`
  - `RobotStateMachine(bool calibrationValidAtBoot, uint32_t nowMs = 0)`
  - `RobotState state() const`, `ErrorCode error() const`, `uint32_t enteredAtMs() const`
  - `bool handle(SmEvent, const SmInputs&, uint32_t nowMs)` — true se mudou de estado
  - `bool tick(uint32_t nowMs, uint16_t startMs)` — ARMED → RUNNING
  - `bool calibrationWindowOver(uint32_t nowMs) const`, `CAL_DURATION_MS = 5000`
  - `bool motorsEnabled() const`, `bool isActive() const`
  - `inline bool isActiveState(uint8_t s)` — para a task `comm` testar o estado publicado

- [ ] **Step 1: Escrever o teste**

`test/test_state_machine/test_state_machine.cpp`:

```cpp
#include <unity.h>
#include "strategy/RobotStateMachine.h"

static const SmInputs OK_IN{true, true};
static const SmInputs NO_CAL{false, true};
static const SmInputs LOW_BAT{true, false};

void setUp() {}
void tearDown() {}

static RobotStateMachine running() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 10);
    sm.tick(1010, 1000);
    return sm;
}

void test_state_machine_boot_with_calibration_is_ready() {
    RobotStateMachine sm(true);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_boot_without_calibration_is_idle() {
    RobotStateMachine sm(false);
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
}

void test_state_machine_idle_long_press_calibrates() {
    RobotStateMachine sm(false);
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_LONG, NO_CAL, 100));
    TEST_ASSERT_EQUAL(RobotState::CALIBRATING, sm.state());
    TEST_ASSERT_FALSE(sm.calibrationWindowOver(5099));
    TEST_ASSERT_TRUE(sm.calibrationWindowOver(5100));
}

void test_state_machine_idle_short_press_needs_valid_calibration() {
    RobotStateMachine sm(false);
    TEST_ASSERT_FALSE(sm.handle(SmEvent::BTN_SHORT, NO_CAL, 0));
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
    TEST_ASSERT_TRUE(sm.handle(SmEvent::BTN_SHORT, OK_IN, 0));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_calibration_result() {
    RobotStateMachine sm(false);
    sm.handle(SmEvent::CMD_SENSOR_CAL, NO_CAL, 0);
    sm.handle(SmEvent::CAL_DONE_OK, OK_IN, 5000);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
    sm.handle(SmEvent::BTN_LONG, OK_IN, 6000);
    sm.handle(SmEvent::CAL_DONE_FAIL, NO_CAL, 11000);
    TEST_ASSERT_EQUAL(RobotState::ERROR, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::CAL, sm.error());
}

void test_state_machine_start_arms_then_runs_after_delay() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::CMD_START, OK_IN, 100);
    TEST_ASSERT_EQUAL(RobotState::ARMED, sm.state());
    TEST_ASSERT_FALSE(sm.motorsEnabled());
    TEST_ASSERT_TRUE(sm.isActive());
    TEST_ASSERT_FALSE(sm.tick(1099, 1000));
    TEST_ASSERT_TRUE(sm.tick(1100, 1000));
    TEST_ASSERT_EQUAL(RobotState::RUNNING, sm.state());
    TEST_ASSERT_TRUE(sm.motorsEnabled());
}

// Review Focus 2
void test_state_machine_start_with_low_battery_errors() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, LOW_BAT, 100);
    TEST_ASSERT_EQUAL(RobotState::ERROR, sm.state());
    TEST_ASSERT_EQUAL(ErrorCode::BAT, sm.error());
}

void test_state_machine_armed_can_be_cancelled() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 100);
    sm.handle(SmEvent::BTN_SHORT, OK_IN, 500);
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_running_stops_on_short_press_or_stop() {
    RobotStateMachine a = running();
    a.handle(SmEvent::BTN_SHORT, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    RobotStateMachine b = running();
    b.handle(SmEvent::CMD_STOP, OK_IN, 2000);
    TEST_ASSERT_EQUAL(RobotState::READY, b.state());
}

void test_state_machine_running_faults() {
    RobotStateMachine a = running();
    a.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::LINE, a.error());
    RobotStateMachine b = running();
    b.handle(SmEvent::BATTERY_LOW, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::BAT, b.error());
    RobotStateMachine c = running();
    c.handle(SmEvent::LOOP_OVERRUN, OK_IN, 2000);
    TEST_ASSERT_EQUAL(ErrorCode::OVR, c.error());
    TEST_ASSERT_FALSE(c.motorsEnabled());
}

void test_state_machine_faults_ignored_outside_running() {
    RobotStateMachine sm(true, 0);
    TEST_ASSERT_FALSE(sm.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 10));
    TEST_ASSERT_FALSE(sm.handle(SmEvent::LOOP_OVERRUN, OK_IN, 10));
    TEST_ASSERT_EQUAL(RobotState::READY, sm.state());
}

void test_state_machine_error_short_press_recovers() {
    RobotStateMachine a = running();
    a.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    a.handle(SmEvent::BTN_SHORT, OK_IN, 3000);
    TEST_ASSERT_EQUAL(RobotState::READY, a.state());
    TEST_ASSERT_EQUAL(ErrorCode::NONE, a.error());
    RobotStateMachine b = running();
    b.handle(SmEvent::LINE_LOST_TIMEOUT, OK_IN, 2000);
    b.handle(SmEvent::BTN_SHORT, NO_CAL, 3000);
    TEST_ASSERT_EQUAL(RobotState::IDLE, b.state());
}

void test_state_machine_reset_from_anywhere() {
    RobotStateMachine sm = running();
    TEST_ASSERT_TRUE(sm.handle(SmEvent::CMD_RESET, OK_IN, 2000));
    TEST_ASSERT_EQUAL(RobotState::IDLE, sm.state());
}

void test_state_machine_ready_long_press_recalibrates() {
    RobotStateMachine sm(true, 0);
    sm.handle(SmEvent::BTN_LONG, OK_IN, 10);
    TEST_ASSERT_EQUAL(RobotState::CALIBRATING, sm.state());
}

void test_state_machine_is_active_state_helper() {
    TEST_ASSERT_TRUE(isActiveState(static_cast<uint8_t>(RobotState::ARMED)));
    TEST_ASSERT_TRUE(isActiveState(static_cast<uint8_t>(RobotState::RUNNING)));
    TEST_ASSERT_FALSE(isActiveState(static_cast<uint8_t>(RobotState::READY)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_state_machine_boot_with_calibration_is_ready);
    RUN_TEST(test_state_machine_boot_without_calibration_is_idle);
    RUN_TEST(test_state_machine_idle_long_press_calibrates);
    RUN_TEST(test_state_machine_idle_short_press_needs_valid_calibration);
    RUN_TEST(test_state_machine_calibration_result);
    RUN_TEST(test_state_machine_start_arms_then_runs_after_delay);
    RUN_TEST(test_state_machine_start_with_low_battery_errors);
    RUN_TEST(test_state_machine_armed_can_be_cancelled);
    RUN_TEST(test_state_machine_running_stops_on_short_press_or_stop);
    RUN_TEST(test_state_machine_running_faults);
    RUN_TEST(test_state_machine_faults_ignored_outside_running);
    RUN_TEST(test_state_machine_error_short_press_recovers);
    RUN_TEST(test_state_machine_reset_from_anywhere);
    RUN_TEST(test_state_machine_ready_long_press_recalibrates);
    RUN_TEST(test_state_machine_is_active_state_helper);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_state_machine`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/strategy/RobotStateMachine.h`:

```cpp
#pragma once
#include <cstdint>

enum class RobotState : uint8_t { IDLE, CALIBRATING, READY, ARMED, RUNNING, ERROR };
enum class ErrorCode : uint8_t { NONE = 0, OVR = 1, LINE = 2, BAT = 3, CAL = 4 };

enum class SmEvent : uint8_t {
    BTN_SHORT, BTN_LONG, CMD_START, CMD_STOP, CMD_RESET, CMD_SENSOR_CAL,
    CAL_DONE_OK, CAL_DONE_FAIL, LINE_LOST_TIMEOUT, BATTERY_LOW, LOOP_OVERRUN
};

struct SmInputs {
    bool calibrationValid = false;
    bool batteryOk = true;
};

/** ARMED or RUNNING, for code that only sees the published state byte. */
inline bool isActiveState(uint8_t s) {
    return s == static_cast<uint8_t>(RobotState::ARMED) ||
           s == static_cast<uint8_t>(RobotState::RUNNING);
}

/** Máquina de estados do robô (spec §4.6). Pura: o chamador fornece tempo e entradas. */
class RobotStateMachine {
public:
    static constexpr uint32_t CAL_DURATION_MS = 5000;

    explicit RobotStateMachine(bool calibrationValidAtBoot, uint32_t nowMs = 0);

    RobotState state() const { return _state; }
    ErrorCode  error() const { return _error; }
    uint32_t   enteredAtMs() const { return _enteredAt; }

    /** Applies one event. Returns true when the state changed. */
    bool handle(SmEvent ev, const SmInputs& in, uint32_t nowMs);
    /** ARMED → RUNNING once `startMs` elapsed. Returns true on that transition. */
    bool tick(uint32_t nowMs, uint16_t startMs);
    bool calibrationWindowOver(uint32_t nowMs) const;

    bool motorsEnabled() const { return _state == RobotState::RUNNING; }
    bool isActive() const { return isActiveState(static_cast<uint8_t>(_state)); }

private:
    bool _enter(RobotState s, ErrorCode e, uint32_t nowMs);

    RobotState _state;
    ErrorCode  _error = ErrorCode::NONE;
    uint32_t   _enteredAt;
};
```

`src/strategy/RobotStateMachine.cpp`:

```cpp
#include "strategy/RobotStateMachine.h"

RobotStateMachine::RobotStateMachine(bool calibrationValidAtBoot, uint32_t nowMs)
    : _state(calibrationValidAtBoot ? RobotState::READY : RobotState::IDLE),
      _enteredAt(nowMs) {}

bool RobotStateMachine::_enter(RobotState s, ErrorCode e, uint32_t nowMs) {
    if (s == _state && e == _error) return false;
    _state = s;
    _error = e;
    _enteredAt = nowMs;
    return true;
}

bool RobotStateMachine::handle(SmEvent ev, const SmInputs& in, uint32_t nowMs) {
    using S = RobotState;
    using E = SmEvent;
    if (ev == E::CMD_RESET) return _enter(S::IDLE, ErrorCode::NONE, nowMs);

    switch (_state) {
        case S::IDLE:
            if (ev == E::BTN_LONG || ev == E::CMD_SENSOR_CAL) {
                return _enter(S::CALIBRATING, ErrorCode::NONE, nowMs);
            }
            if (ev == E::BTN_SHORT && in.calibrationValid) {
                return _enter(S::READY, ErrorCode::NONE, nowMs);
            }
            break;
        case S::CALIBRATING:
            if (ev == E::CAL_DONE_OK) return _enter(S::READY, ErrorCode::NONE, nowMs);
            if (ev == E::CAL_DONE_FAIL) return _enter(S::ERROR, ErrorCode::CAL, nowMs);
            break;
        case S::READY:
            if (ev == E::BTN_SHORT || ev == E::CMD_START) {
                return in.batteryOk ? _enter(S::ARMED, ErrorCode::NONE, nowMs)
                                    : _enter(S::ERROR, ErrorCode::BAT, nowMs);
            }
            if (ev == E::BTN_LONG || ev == E::CMD_SENSOR_CAL) {
                return _enter(S::CALIBRATING, ErrorCode::NONE, nowMs);
            }
            break;
        case S::ARMED:
            if (ev == E::BTN_SHORT || ev == E::CMD_STOP) {
                return _enter(S::READY, ErrorCode::NONE, nowMs);
            }
            break;
        case S::RUNNING:
            if (ev == E::BTN_SHORT || ev == E::CMD_STOP) {
                return _enter(S::READY, ErrorCode::NONE, nowMs);
            }
            if (ev == E::LINE_LOST_TIMEOUT) return _enter(S::ERROR, ErrorCode::LINE, nowMs);
            if (ev == E::BATTERY_LOW) return _enter(S::ERROR, ErrorCode::BAT, nowMs);
            if (ev == E::LOOP_OVERRUN) return _enter(S::ERROR, ErrorCode::OVR, nowMs);
            break;
        case S::ERROR:
            if (ev == E::BTN_SHORT) {
                return _enter(in.calibrationValid ? S::READY : S::IDLE, ErrorCode::NONE, nowMs);
            }
            break;
    }
    return false;
}

bool RobotStateMachine::tick(uint32_t nowMs, uint16_t startMs) {
    if (_state == RobotState::ARMED && nowMs - _enteredAt >= startMs) {
        return _enter(RobotState::RUNNING, ErrorCode::NONE, nowMs);
    }
    return false;
}

bool RobotStateMachine::calibrationWindowOver(uint32_t nowMs) const {
    return _state == RobotState::CALIBRATING && nowMs - _enteredAt >= CAL_DURATION_MS;
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/strategy/RobotStateMachine.h src/strategy/RobotStateMachine.cpp \
        test/test_state_machine
git commit -m "feat(strategy): robot state machine with ARMED delay and error codes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 15: Annunciator — LED e buzzer sem bloquear

Resolve F-11.

**Files:**
- Create: `src/comm/Annunciator.h`, `src/comm/Annunciator.cpp`
- Test: `test/test_annunciator/test_annunciator.cpp`

**Interfaces:**
- Consumes: `RobotState`, `ErrorCode` (Tarefa 14).
- Produces: `struct Indication { bool led; bool buzzer; };`,
  `void onState(RobotState, ErrorCode, uint32_t nowMs)`,
  `void beep(uint8_t count, uint16_t onMs, uint16_t offMs, uint32_t nowMs)`,
  `Indication update(uint32_t nowMs) const`.

- [ ] **Step 1: Escrever o teste**

`test/test_annunciator/test_annunciator.cpp`:

```cpp
#include <unity.h>
#include "comm/Annunciator.h"

void setUp() {}
void tearDown() {}

void test_annunciator_idle_blinks_1hz() {
    Annunciator a;
    a.onState(RobotState::IDLE, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(100).led);
    TEST_ASSERT_FALSE(a.update(600).led);
    TEST_ASSERT_TRUE(a.update(1100).led);
}

void test_annunciator_ready_led_on_running_led_off() {
    Annunciator a;
    a.onState(RobotState::READY, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(5000).led);
    a.onState(RobotState::RUNNING, ErrorCode::NONE, 6000);
    TEST_ASSERT_FALSE(a.update(6500).led);
    TEST_ASSERT_FALSE(a.update(6500).buzzer);
}

void test_annunciator_calibrating_blinks_5hz_and_beeps_on_entry() {
    Annunciator a;
    a.onState(RobotState::CALIBRATING, ErrorCode::NONE, 0);
    TEST_ASSERT_TRUE(a.update(50).led);
    TEST_ASSERT_FALSE(a.update(150).led);
    TEST_ASSERT_TRUE(a.update(10).buzzer);
    TEST_ASSERT_FALSE(a.update(100).buzzer);
}

void test_annunciator_armed_beeps_every_250ms() {
    Annunciator a;
    a.onState(RobotState::ARMED, ErrorCode::NONE, 1000);
    TEST_ASSERT_TRUE(a.update(1010).buzzer);
    TEST_ASSERT_FALSE(a.update(1100).buzzer);
    TEST_ASSERT_TRUE(a.update(1260).buzzer);
}

void test_annunciator_error_blinks_code_count() {
    Annunciator a;
    a.onState(RobotState::ERROR, ErrorCode::LINE, 0);  // código 2: período 2×400 + 1200
    TEST_ASSERT_TRUE(a.update(100).led);
    TEST_ASSERT_FALSE(a.update(300).led);
    TEST_ASSERT_TRUE(a.update(500).led);
    TEST_ASSERT_FALSE(a.update(900).led);
    TEST_ASSERT_TRUE(a.update(2100).led);  // novo ciclo
}

void test_annunciator_error_beeps_code_count_on_entry() {
    Annunciator a;
    a.onState(RobotState::ERROR, ErrorCode::BAT, 0);  // 3 bipes de 120 ms, pausa 120 ms
    int rising = 0;
    bool prev = false;
    for (uint32_t t = 0; t < 2000; t += 5) {
        const bool now = a.update(t).buzzer;
        if (now && !prev) rising++;
        prev = now;
    }
    TEST_ASSERT_EQUAL_INT(3, rising);
}

void test_annunciator_explicit_beep_sequence() {
    Annunciator a;
    a.onState(RobotState::RUNNING, ErrorCode::NONE, 0);
    a.beep(2, 50, 50, 1000);
    TEST_ASSERT_TRUE(a.update(1020).buzzer);
    TEST_ASSERT_FALSE(a.update(1070).buzzer);
    TEST_ASSERT_TRUE(a.update(1120).buzzer);
    TEST_ASSERT_FALSE(a.update(1220).buzzer);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_annunciator_idle_blinks_1hz);
    RUN_TEST(test_annunciator_ready_led_on_running_led_off);
    RUN_TEST(test_annunciator_calibrating_blinks_5hz_and_beeps_on_entry);
    RUN_TEST(test_annunciator_armed_beeps_every_250ms);
    RUN_TEST(test_annunciator_error_blinks_code_count);
    RUN_TEST(test_annunciator_error_beeps_code_count_on_entry);
    RUN_TEST(test_annunciator_explicit_beep_sequence);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_annunciator`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/comm/Annunciator.h`:

```cpp
#pragma once
#include <cstdint>
#include "strategy/RobotStateMachine.h"

struct Indication {
    bool led;
    bool buzzer;
};

/**
 * Padrões de LED e buzzer por estado (spec §4.6), calculados a partir do tempo — nada
 * bloqueia (F-11). onState() deve ser chamado a cada mudança de estado.
 */
class Annunciator {
public:
    void onState(RobotState state, ErrorCode error, uint32_t nowMs);
    void beep(uint8_t count, uint16_t onMs, uint16_t offMs, uint32_t nowMs);
    Indication update(uint32_t nowMs) const;

private:
    bool _led(uint32_t nowMs) const;
    bool _sequenceOn(uint32_t nowMs) const;

    RobotState _state = RobotState::IDLE;
    ErrorCode  _error = ErrorCode::NONE;
    uint32_t   _since = 0;
    uint8_t    _beepCount = 0;
    uint16_t   _beepOn = 0;
    uint16_t   _beepOff = 0;
    uint32_t   _beepStart = 0;
};
```

`src/comm/Annunciator.cpp`:

```cpp
#include "comm/Annunciator.h"

void Annunciator::onState(RobotState state, ErrorCode error, uint32_t nowMs) {
    _state = state;
    _error = error;
    _since = nowMs;
    switch (state) {
        case RobotState::CALIBRATING:
        case RobotState::READY:
            beep(1, 80, 0, nowMs);
            break;
        case RobotState::ERROR:
            beep(static_cast<uint8_t>(error), 120, 120, nowMs);
            break;
        default:
            break;
    }
}

void Annunciator::beep(uint8_t count, uint16_t onMs, uint16_t offMs, uint32_t nowMs) {
    _beepCount = count;
    _beepOn = onMs;
    _beepOff = offMs;
    _beepStart = nowMs;
}

bool Annunciator::_sequenceOn(uint32_t nowMs) const {
    if (_beepCount == 0 || _beepOn == 0) return false;
    const uint32_t t = nowMs - _beepStart;
    const uint32_t period = static_cast<uint32_t>(_beepOn) + _beepOff;
    if (t >= period * _beepCount) return false;
    return (t % period) < _beepOn;
}

bool Annunciator::_led(uint32_t nowMs) const {
    const uint32_t t = nowMs - _since;
    switch (_state) {
        case RobotState::IDLE:        return (t / 500) % 2 == 0;
        case RobotState::CALIBRATING: return (t / 100) % 2 == 0;
        case RobotState::READY:       return true;
        case RobotState::ARMED:       return (t / 50) % 2 == 0;
        case RobotState::RUNNING:     return false;
        case RobotState::ERROR: {
            const uint32_t n = static_cast<uint32_t>(_error);
            const uint32_t phase = t % (n * 400 + 1200);
            return phase < n * 400 && (phase % 400) < 200;
        }
    }
    return false;
}

Indication Annunciator::update(uint32_t nowMs) const {
    const bool armedTick = _state == RobotState::ARMED && ((nowMs - _since) % 250) < 40;
    return Indication{_led(nowMs), _sequenceOn(nowMs) || armedTick};
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/comm/Annunciator.h src/comm/Annunciator.cpp test/test_annunciator
git commit -m "feat(comm): non-blocking LED and buzzer patterns per state (F-11)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---
### Task 16: ControlLoop — o passo de controle inteiro, testável sem hardware

Implementa a spec §4.7 (passos 1–6) e cobre em cenário F-01, F-03, F-05, F-06/F-07, F-08 e
F-14.

**Files:**
- Create: `src/strategy/ControlLoop.h`, `src/strategy/ControlLoop.cpp`
- Test: `test/test_control_loop/test_control_loop.cpp`

**Interfaces:**
- Consumes: `Calibration` (3), `LineFollower`/`LineReading` (4), `SpeedProfile` (5),
  `mixSteering`/`CascadeController`/`WheelCommand` (7), `RobotConfig` (8),
  `OutputStage`/`MotorDuties` (13), `PIDController` (2).
- Produces:
  - `struct ControlInputs { const int* raw; float rpmL; float rpmR; float vbat; float dtS; };`
  - `struct ControlOutputs { MotorDuties duties; float error, u, base, uL, uR, spL, spR;
    bool lost; bool lostTimeout; };`
  - `ControlLoop(Calibration& cal, int pwmMax)`
  - `void applyConfig(const RobotConfig& cfg)` — também aplica `pol` na calibração
  - `void start(const int* raw, float rpmL, float rpmR)` — chamar na transição ARMED→RUNNING
  - `ControlOutputs step(const ControlInputs& in)`

- [ ] **Step 1: Escrever o teste**

`test/test_control_loop/test_control_loop.cpp`:

```cpp
#include <unity.h>
#include <cmath>
#include "config/RobotConfig.h"
#include "sensors/Calibration.h"
#include "strategy/ControlLoop.h"

static Calibration* cal = nullptr;
static ControlLoop* loop = nullptr;
static RobotConfig  cfg;
static constexpr float DT = 0.002f;
static constexpr int PWM_MAX = 1023;

// Linha BRANCA sobre fundo preto: o QTR-8A lê baixo sobre o branco (pol = 1, fábrica)
static const int CENTER[8]  = {1000, 1000, 1000, 100, 100, 1000, 1000, 1000};
static const int RIGHT[8]   = {1000, 1000, 1000, 1000, 1000, 300, 200, 1000};
static const int NO_LINE[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};

static void calibrateFlat(Calibration& c) {
    int lo[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int hi[8] = {1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000};
    c.update(lo);
    c.update(hi);
}

static ControlInputs in(const int* raw, float rpmL = 0.0f, float rpmR = 0.0f,
                        float dt = DT) {
    ControlInputs i;
    i.raw = raw;
    i.rpmL = rpmL;
    i.rpmR = rpmR;
    i.vbat = 7.4f;
    i.dtS = dt;
    return i;
}

static ControlOutputs run(const int* raw, int steps) {
    ControlOutputs o{};
    for (int k = 0; k < steps; k++) o = loop->step(in(raw));
    return o;
}

void setUp() {
    cal = new Calibration(8);
    calibrateFlat(*cal);
    cfg = robotcfg::factoryConfig();
    cfg.tune.vcomp = 0;  // compensação é testada na OutputStage
    loop = new ControlLoop(*cal, PWM_MAX);
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
}

void tearDown() {
    delete loop;
    delete cal;
}

void test_control_loop_centered_line_drives_straight_at_vmax() {
    const ControlOutputs o = run(CENTER, 1000);
    TEST_ASSERT_EQUAL_INT(o.duties.a, o.duties.b);
    TEST_ASSERT_EQUAL_INT(358, o.duties.a);  // 0,35 × 1023
}

void test_control_loop_ramp_limits_start() {
    const ControlOutputs o = run(CENTER, 1);  // 2,0/s × 2 ms = 0,004
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.004f, o.base);
}

// F-01
void test_control_loop_start_keeps_calibration() {
    TEST_ASSERT_TRUE(cal->isValid());
    const ControlOutputs o = run(RIGHT, 10);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

// F-03
void test_control_loop_white_line_on_right_turns_right() {
    run(CENTER, 200);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.error > 0.0f);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

// F-05: a base cai na curva, nos dois modos
void test_control_loop_speed_drops_in_curves_pwm_mode() {
    run(CENTER, 1000);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.base < 0.30f);
}

void test_control_loop_speed_drops_in_curves_cascade_mode() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    run(CENTER, 1000);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE((o.spL + o.spR) / 2.0f < 0.30f * cfg.tune.rpmMax);
}

// F-08: com o motor no RPM pedido, o duty é o do feedforward
void test_control_loop_cascade_feedforward_holds_duty() {
    cfg.tune.mode = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    ControlOutputs o{};
    float rpmL = 0.0f;
    float rpmR = 0.0f;
    for (int k = 0; k < 1000; k++) {
        o = loop->step(in(CENTER, rpmL, rpmR));
        rpmL = o.spL;  // motor ideal: segue o setpoint
        rpmR = o.spR;
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f, o.uL);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.35f, o.uR);
}

void test_control_loop_encoder_inversion_flips_measured_rpm() {
    cfg.tune.mode = 1;
    cfg.tune.kiv = 0.0f;
    cfg.hw.einvL = 1;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    run(CENTER, 1000);  // base chega a 0,35 → setpoint 245 RPM dos dois lados
    // Os dois estimadores informam +245; com einvL o esquerdo vira -245: erro de 490 RPM
    const ControlOutputs o = loop->step(in(CENTER, 245.0f, 245.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.35f, o.uR);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, o.uL);
}

// F-14 + Review Focus 5
void test_control_loop_start_off_line_times_out() {
    loop->start(NO_LINE, 0.0f, 0.0f);
    ControlOutputs o{};
    for (int k = 0; k < 149; k++) o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lost);
    TEST_ASSERT_FALSE(o.lostTimeout);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, o.error);
    TEST_ASSERT_EQUAL_INT(o.duties.a, o.duties.b);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, cfg.tune.vmin, o.base);  // linha perdida: anda devagar
    o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lostTimeout);
}

void test_control_loop_lost_after_right_keeps_turning_right() {
    run(RIGHT, 5);
    const ControlOutputs o = loop->step(in(NO_LINE));
    TEST_ASSERT_TRUE(o.lost);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, o.error);
    TEST_ASSERT_TRUE(o.duties.a > o.duties.b);
}

void test_control_loop_line_found_resets_lost_timer() {
    loop->start(NO_LINE, 0.0f, 0.0f);
    run(NO_LINE, 140);
    run(CENTER, 1);
    const ControlOutputs o = run(NO_LINE, 140);
    TEST_ASSERT_FALSE(o.lostTimeout);
}

void test_control_loop_sensor_invert_flips_turn() {
    cfg.hw.sinv = 1;
    loop->applyConfig(cfg);
    run(CENTER, 200);
    const ControlOutputs o = loop->step(in(RIGHT));
    TEST_ASSERT_TRUE(o.duties.a < o.duties.b);
}

void test_control_loop_black_line_polarity() {
    cfg.hw.pol = 0;
    loop->applyConfig(cfg);
    const int blackRight[8] = {0, 0, 0, 0, 0, 700, 800, 0};
    run(blackRight, 5);
    TEST_ASSERT_TRUE(loop->step(in(blackRight)).error > 0.0f);
}

// F-06/F-07: o termo D usa o dt real informado
void test_control_loop_uses_real_dt() {
    cfg.tune.kp = 0.0f;
    cfg.tune.kd = 0.001f;  // pequeno o bastante para não saturar em ±1
    cfg.tune.dlp = 1.0f;
    loop->applyConfig(cfg);
    loop->start(CENTER, 0.0f, 0.0f);
    loop->step(in(CENTER, 0.0f, 0.0f, 0.002f));
    const float uFast = loop->step(in(RIGHT, 0.0f, 0.0f, 0.002f)).u;
    loop->start(CENTER, 0.0f, 0.0f);
    loop->step(in(CENTER, 0.0f, 0.0f, 0.004f));
    const float uSlow = loop->step(in(RIGHT, 0.0f, 0.0f, 0.004f)).u;
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 2.0f * uSlow, uFast);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_control_loop_centered_line_drives_straight_at_vmax);
    RUN_TEST(test_control_loop_ramp_limits_start);
    RUN_TEST(test_control_loop_start_keeps_calibration);
    RUN_TEST(test_control_loop_white_line_on_right_turns_right);
    RUN_TEST(test_control_loop_speed_drops_in_curves_pwm_mode);
    RUN_TEST(test_control_loop_speed_drops_in_curves_cascade_mode);
    RUN_TEST(test_control_loop_cascade_feedforward_holds_duty);
    RUN_TEST(test_control_loop_encoder_inversion_flips_measured_rpm);
    RUN_TEST(test_control_loop_start_off_line_times_out);
    RUN_TEST(test_control_loop_lost_after_right_keeps_turning_right);
    RUN_TEST(test_control_loop_line_found_resets_lost_timer);
    RUN_TEST(test_control_loop_sensor_invert_flips_turn);
    RUN_TEST(test_control_loop_black_line_polarity);
    RUN_TEST(test_control_loop_uses_real_dt);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_control_loop`
Expected: FAIL (header não existe).

- [ ] **Step 3: Implementar**

`src/strategy/ControlLoop.h`:

```cpp
#pragma once
#include <cstdint>
#include "config/RobotConfig.h"
#include "control/CascadeController.h"
#include "control/PIDController.h"
#include "control/SpeedProfile.h"
#include "motors/OutputStage.h"
#include "sensors/Calibration.h"
#include "strategy/LineFollower.h"

struct ControlInputs {
    const int* raw = nullptr;  ///< leituras brutas, Calibration::count() valores
    float rpmL = 0.0f;         ///< RPM medido (sinal do encoder ainda não corrigido)
    float rpmR = 0.0f;
    float vbat = 0.0f;         ///< V filtrado
    float dtS = 0.0f;          ///< dt real do ciclo, s
};

struct ControlOutputs {
    MotorDuties duties{0, 0};
    float error = 0.0f;
    float u = 0.0f;
    float base = 0.0f;
    float uL = 0.0f;
    float uR = 0.0f;
    float spL = 0.0f;   ///< RPM pedido (0 no modo PWM direto)
    float spR = 0.0f;
    bool  lost = false;
    bool  lostTimeout = false;
};

/**
 * Um passo do controle (spec §4.7): sensores → erro → PID externo → base com rampa →
 * mistura (PWM direto ou cascata) → OutputStage. Sem hardware: testável em native.
 */
class ControlLoop {
public:
    ControlLoop(Calibration& cal, int pwmMax);

    void applyConfig(const RobotConfig& cfg);
    void start(const int* raw, float rpmL, float rpmR);
    ControlOutputs step(const ControlInputs& in);

private:
    static float _signed(float rpm, uint8_t inverted) { return inverted ? -rpm : rpm; }

    Calibration&      _cal;
    PIDController     _outer;
    PIDController     _innerL;
    PIDController     _innerR;
    LineFollower      _line;
    SpeedProfile      _speed;
    CascadeController _cascade;
    OutputStage       _output;
    RobotConfig       _cfg;
    uint32_t          _lostUs = 0;
};
```

`src/strategy/ControlLoop.cpp`:

```cpp
#include "strategy/ControlLoop.h"
#include <cmath>
#include "control/Mixer.h"

ControlLoop::ControlLoop(Calibration& cal, int pwmMax)
    : _cal(cal),
      _outer(1.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _innerL(0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _innerR(0.0f, 0.0f, 0.0f, -1.0f, 1.0f),
      _line(cal, _outer),
      _cascade(_innerL, _innerR),
      _output(pwmMax),
      _cfg(robotcfg::factoryConfig()) {
    applyConfig(_cfg);
}

void ControlLoop::applyConfig(const RobotConfig& cfg) {
    _cfg = cfg;
    _cal.setWhiteLine(cfg.hw.pol == 1);
    _line.setSensorOrderInverted(cfg.hw.sinv == 1);
    _outer.setTunings(cfg.tune.kp, cfg.tune.ki, cfg.tune.kd);
    _outer.setDerivativeFilter(cfg.tune.dlp);
    _cascade.setInnerGains(cfg.tune.kpv, cfg.tune.kiv, cfg.tune.kdv);
    _speed.setParams(cfg.tune.vmin, cfg.tune.vmax, cfg.tune.thr, cfg.tune.acc);
}

void ControlLoop::start(const int* raw, float rpmL, float rpmR) {
    _line.reset(_line.peekError(raw));
    _speed.reset(0.0f);
    _cascade.reset(_signed(rpmL, _cfg.hw.einvL), _signed(rpmR, _cfg.hw.einvR));
    _lostUs = 0;
}

ControlOutputs ControlLoop::step(const ControlInputs& in) {
    ControlOutputs out;
    const float dt = (std::isfinite(in.dtS) && in.dtS > 0.0f) ? in.dtS : 0.0f;

    const LineReading r = _line.update(in.raw, dt);
    out.error = r.error;
    out.u = r.u;
    out.lost = r.lost;

    // Tempo de linha perdida em µs inteiros: comparação exata com lostMs
    _lostUs = r.lost ? _lostUs + static_cast<uint32_t>(std::lround(dt * 1e6f)) : 0;
    out.lostTimeout = _lostUs >= static_cast<uint32_t>(_cfg.tune.lostMs) * 1000u;

    // Linha perdida conta como erro máximo: anda em vmin enquanto procura (Review Focus 5)
    out.base = _speed.step(r.lost ? 1.0f : std::fabs(r.error), dt);

    if (_cfg.tune.mode == 0) {
        mixSteering(out.base, r.u, out.uL, out.uR);
    } else {
        const WheelCommand wc = _cascade.compute(
            out.base, r.u, _cfg.tune.rpmMax,
            _signed(in.rpmL, _cfg.hw.einvL), _signed(in.rpmR, _cfg.hw.einvR), dt);
        out.uL = wc.uL;
        out.uR = wc.uR;
        out.spL = wc.spL;
        out.spR = wc.spR;
    }

    out.duties = _output.compute(out.uL, out.uR, in.vbat, _cfg.hw, _cfg.tune);
    return out;
}
```

- [ ] **Step 4: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_control_loop` com 14 casos).

- [ ] **Step 5: Commit**

```bash
git add src/strategy/ControlLoop.h src/strategy/ControlLoop.cpp test/test_control_loop
git commit -m "feat(strategy): pure ControlLoop covering PWM and cascade modes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 17: Mailbox, CommandHandler e BLETuner como transporte

Implementa a spec §4.5 (regras de `set`/`save`/`load`/`defaults`/`prof`) e a passagem de dados
entre núcleos da §4.7.

**Files:**
- Create: `src/comm/Mailbox.h`, `src/comm/CommandHandler.h`, `src/comm/CommandHandler.cpp`
- Modify: `src/comm/BLETuner.h`, `src/comm/BLETuner.cpp` (reescrever)
- Test: `test/test_command_handler/test_command_handler.cpp`,
  `test/test_ble_tuner/test_ble_tuner.cpp` (reescrever)

**Interfaces:**
- Consumes: `Command`, `parseCommand`, formatadores (10), `ConfigStore` (9),
  `RobotConfig`, `robotcfg::*` (8), `SpinLock` (10), `CommandQueue` (10).
- Produces:
  - `template <typename T> class Mailbox { void post(const T&); bool fetch(T& out,
    uint32_t& seen); }`
  - `class ResponseSink { virtual void send(const char* json) = 0; }`
  - `enum class ControlRequest : uint8_t { NONE, START, STOP, RESET, SENSOR_CAL,
    ENC_CAL_START, ENC_CAL_FINISH };`
  - `struct ControlMsg { ControlRequest req; bool leftSide; int rotations; };`
  - `CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion)`;
    `void handle(const Command&, bool running, ResponseSink&, ControlMsg&)`;
    `void sendConfig(ResponseSink&) const`; `bool dirty() const`;
    `bool consumeConfigChanged()`; `const RobotConfig& live() const`
  - `BLETuner`: `begin(const char*)`, `bool popCommand(char*, size_t)`,
    `bool consumeNewConnection()`, `bool isConnected() const`, `void notify(const char*)`,
    `injectForTest(const char*)` (só `NATIVE_BUILD`); UUIDs inalterados.

- [ ] **Step 1: Escrever os testes**

`test/test_command_handler/test_command_handler.cpp`:

```cpp
#include <unity.h>
#include <cstring>
#include <string>
#include <vector>
#include "comm/CommandHandler.h"
#include "comm/CommandProtocol.h"
#include "comm/Mailbox.h"
#include "config/ConfigStore.h"
#include "storage/InMemoryStore.h"

class VectorSink : public ResponseSink {
public:
    void send(const char* json) override { msgs.emplace_back(json); }
    bool contains(const char* fragment) const {
        for (const std::string& m : msgs) {
            if (m.find(fragment) != std::string::npos) return true;
        }
        return false;
    }
    std::vector<std::string> msgs;
};

static InMemoryStore*  mem = nullptr;
static ConfigStore*    store = nullptr;
static RobotConfig     live;
static CommandHandler* h = nullptr;
static VectorSink      sink;

static void send(const char* json, bool running = false) {
    Command c;
    TEST_ASSERT_TRUE_MESSAGE(parseCommand(json, c), json);
    ControlMsg msg;
    h->handle(c, running, sink, msg);
}

static ControlMsg sendForMsg(const char* json, bool running = false) {
    Command c;
    parseCommand(json, c);
    ControlMsg msg;
    h->handle(c, running, sink, msg);
    return msg;
}

void setUp() {
    mem = new InMemoryStore();
    mem->begin("lfr");
    store = new ConfigStore(*mem);
    live = robotcfg::factoryConfig();
    h = new CommandHandler(live, *store, "4.0.0");
    sink.msgs.clear();
}

void tearDown() {
    delete h;
    delete store;
    delete mem;
}

void test_command_handler_set_applies_and_marks_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":1.5}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.5f, live.tune.kp);
    TEST_ASSERT_TRUE(h->dirty());
    TEST_ASSERT_TRUE(h->consumeConfigChanged());
    TEST_ASSERT_FALSE(h->consumeConfigChanged());
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"kp\",\"ok\":1"));
}

void test_command_handler_set_unknown_key_is_not_dirty() {
    send("{\"t\":\"set\",\"k\":\"zz\",\"v\":1}");
    TEST_ASSERT_FALSE(h->dirty());
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"key\""));
}

void test_command_handler_set_nan_is_rejected() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":nan}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, live.tune.kp);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"range\""));
}

void test_command_handler_hardware_key_locked_while_running() {
    send("{\"t\":\"set\",\"k\":\"invL\",\"v\":1}", true);
    TEST_ASSERT_EQUAL_UINT8(0, live.hw.invL);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"running\""));
}

void test_command_handler_legacy_spd_lowers_both_limits() {
    // vmax 0,2 < vmin 0,25 atual: só passa na 2ª passada, depois de vmin descer
    send("{\"t\":\"spd\",\"min\":25,\"max\":51}");
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.2f, live.tune.vmax);
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.098f, live.tune.vmin);
}

void test_command_handler_save_refused_while_running() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}", true);
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"save\",\"ok\":0,\"err\":\"running\""));
    TEST_ASSERT_FALSE(mem->exists("prof0"));
    TEST_ASSERT_TRUE(h->dirty());
}

void test_command_handler_save_persists_and_clears_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}");
    TEST_ASSERT_FALSE(h->dirty());
    RobotConfig back{};
    store->load(back);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, back.tune.kp);
}

void test_command_handler_load_discards_changes() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"save\"}");
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":3}");
    send("{\"t\":\"load\"}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 2.0f, live.tune.kp);
    TEST_ASSERT_FALSE(h->dirty());
}

void test_command_handler_defaults_restore_factory_live() {
    send("{\"t\":\"set\",\"k\":\"kd\",\"v\":0.5}");
    send("{\"t\":\"defaults\"}");
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.03f, live.tune.kd);
    TEST_ASSERT_TRUE(h->dirty());
}

void test_command_handler_profile_switch_refused_when_dirty() {
    send("{\"t\":\"set\",\"k\":\"kp\",\"v\":2}");
    send("{\"t\":\"prof\",\"n\":2}");
    TEST_ASSERT_EQUAL_UINT8(0, live.profile);
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"dirty\""));
    send("{\"t\":\"prof\",\"n\":2,\"f\":1}");
    TEST_ASSERT_EQUAL_UINT8(2, live.profile);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.70f, live.tune.vmax);
    TEST_ASSERT_FALSE(h->dirty());
}

void test_command_handler_profile_out_of_range() {
    send("{\"t\":\"prof\",\"n\":5}");
    TEST_ASSERT_TRUE(sink.contains("\"err\":\"range\""));
    TEST_ASSERT_EQUAL_UINT8(0, live.profile);
}

void test_command_handler_get_sends_all_groups_within_limit() {
    send("{\"t\":\"get\"}");
    TEST_ASSERT_EQUAL_UINT32(robotcfg::GROUP_COUNT + 1, sink.msgs.size());
    for (const std::string& m : sink.msgs) TEST_ASSERT_TRUE(m.size() <= BLE_MAX_MSG);
    TEST_ASSERT_TRUE(sink.contains("\"g\":\"meta\""));
}

void test_command_handler_state_commands_become_control_requests() {
    TEST_ASSERT_EQUAL(ControlRequest::START, sendForMsg("{\"t\":\"start\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::STOP, sendForMsg("{\"t\":\"stop\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::RESET, sendForMsg("{\"t\":\"reset\"}").req);
    TEST_ASSERT_EQUAL(ControlRequest::SENSOR_CAL, sendForMsg("{\"t\":\"scal\"}").req);
}

void test_command_handler_encoder_calibration_request() {
    const ControlMsg m = sendForMsg("{\"t\":\"cal\",\"op\":\"finish\",\"side\":\"R\",\"rot\":20}");
    TEST_ASSERT_EQUAL(ControlRequest::ENC_CAL_FINISH, m.req);
    TEST_ASSERT_FALSE(m.leftSide);
    TEST_ASSERT_EQUAL_INT(20, m.rotations);
    const ControlMsg r = sendForMsg("{\"t\":\"cal\",\"op\":\"start\"}", true);
    TEST_ASSERT_EQUAL(ControlRequest::NONE, r.req);
    TEST_ASSERT_TRUE(sink.contains("\"k\":\"cal\",\"ok\":0,\"err\":\"running\""));
}

void test_mailbox_delivers_latest_value_once() {
    Mailbox<int> box;
    uint32_t seen = 0;
    int v = 0;
    TEST_ASSERT_FALSE(box.fetch(v, seen));
    box.post(1);
    box.post(2);
    TEST_ASSERT_TRUE(box.fetch(v, seen));
    TEST_ASSERT_EQUAL_INT(2, v);
    TEST_ASSERT_FALSE(box.fetch(v, seen));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_command_handler_set_applies_and_marks_dirty);
    RUN_TEST(test_command_handler_set_unknown_key_is_not_dirty);
    RUN_TEST(test_command_handler_set_nan_is_rejected);
    RUN_TEST(test_command_handler_hardware_key_locked_while_running);
    RUN_TEST(test_command_handler_legacy_spd_lowers_both_limits);
    RUN_TEST(test_command_handler_save_refused_while_running);
    RUN_TEST(test_command_handler_save_persists_and_clears_dirty);
    RUN_TEST(test_command_handler_load_discards_changes);
    RUN_TEST(test_command_handler_defaults_restore_factory_live);
    RUN_TEST(test_command_handler_profile_switch_refused_when_dirty);
    RUN_TEST(test_command_handler_profile_out_of_range);
    RUN_TEST(test_command_handler_get_sends_all_groups_within_limit);
    RUN_TEST(test_command_handler_state_commands_become_control_requests);
    RUN_TEST(test_command_handler_encoder_calibration_request);
    RUN_TEST(test_mailbox_delivers_latest_value_once);
    return UNITY_END();
}
```

`test/test_ble_tuner/test_ble_tuner.cpp` (substitui o atual):

```cpp
#include <unity.h>
#include <cstring>
#include "comm/BLETuner.h"

static BLETuner* t = nullptr;
void setUp() { t = new BLETuner(); }
void tearDown() { delete t; t = nullptr; }

void test_ble_tuner_starts_empty_and_disconnected() {
    char buf[64];
    TEST_ASSERT_FALSE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_FALSE(t->isConnected());
    TEST_ASSERT_FALSE(t->consumeNewConnection());
}

void test_ble_tuner_queues_received_commands_in_order() {
    TEST_ASSERT_TRUE(t->injectForTest("{\"t\":\"get\"}"));
    TEST_ASSERT_TRUE(t->injectForTest("{\"t\":\"save\"}"));
    char buf[64];
    TEST_ASSERT_TRUE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"get\"}", buf);
    TEST_ASSERT_TRUE(t->popCommand(buf, sizeof buf));
    TEST_ASSERT_EQUAL_STRING("{\"t\":\"save\"}", buf);
}

void test_ble_tuner_notify_while_disconnected_is_noop() {
    t->notify("{\"t\":\"tel\"}");
    TEST_PASS();
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_ble_tuner_starts_empty_and_disconnected);
    RUN_TEST(test_ble_tuner_queues_received_commands_in_order);
    RUN_TEST(test_ble_tuner_notify_while_disconnected_is_noop);
    return UNITY_END();
}
```

- [ ] **Step 2: Rodar e ver falhar**

Run: `pio test -e native -f test_command_handler -f test_ble_tuner`
Expected: FAIL (headers novos não existem; `injectForTest` não existe).

- [ ] **Step 3: Implementar o Mailbox**

`src/comm/Mailbox.h`:

```cpp
#pragma once
#include <cstdint>
#include "comm/SpinLock.h"

/**
 * Último valor publicado, com número de versão. Um núcleo publica (post), o outro busca
 * (fetch) só quando muda. A seção crítica é só a cópia do valor.
 */
template <typename T>
class Mailbox {
public:
    void post(const T& value) {
        SpinGuard guard(_lock);
        _value = value;
        _version++;
    }

    /** Copies the value when it changed since `seen`; updates `seen`. */
    bool fetch(T& out, uint32_t& seen) {
        SpinGuard guard(_lock);
        if (_version == seen) return false;
        out = _value;
        seen = _version;
        return true;
    }

private:
    T        _value{};
    uint32_t _version = 0;
    SpinLock _lock;
};
```

- [ ] **Step 4: Implementar o CommandHandler**

`src/comm/CommandHandler.h`:

```cpp
#pragma once
#include <cstdint>
#include "comm/CommandProtocol.h"
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"

class ResponseSink {
public:
    virtual ~ResponseSink() = default;
    virtual void send(const char* json) = 0;
};

enum class ControlRequest : uint8_t {
    NONE, START, STOP, RESET, SENSOR_CAL, ENC_CAL_START, ENC_CAL_FINISH
};

struct ControlMsg {
    ControlRequest req = ControlRequest::NONE;
    bool leftSide = true;
    int  rotations = 0;
};

/**
 * Aplica comandos à configuração viva (spec §4.5). Roda na task comm (core 0).
 * - set: valida chave a chave; marca "dirty".
 * - save/load/defaults/prof: recusados em ARMED/RUNNING (escrita na flash para o cache).
 * - prof: recusa com "dirty" se houver mudança não salva, a menos que venha f:1.
 * Comandos de estado viram ControlMsg para a task control (core 1).
 */
class CommandHandler {
public:
    CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion);

    void handle(const Command& cmd, bool running, ResponseSink& sink, ControlMsg& msg);
    void sendConfig(ResponseSink& sink) const;

    bool dirty() const { return _dirty; }
    bool consumeConfigChanged();
    const RobotConfig& live() const { return _live; }

private:
    void _handleSet(const Command& cmd, bool running, ResponseSink& sink);
    void _ack(ResponseSink& sink, const char* what, bool ok, const char* err) const;
    void _reloadFromStore(uint8_t profile);

    RobotConfig& _live;
    ConfigStore& _store;
    const char*  _fw;
    bool _dirty = false;
    bool _changed = false;
};
```

`src/comm/CommandHandler.cpp`:

```cpp
#include "comm/CommandHandler.h"

namespace {
constexpr size_t BUF = 192;
}

CommandHandler::CommandHandler(RobotConfig& live, ConfigStore& store, const char* fwVersion)
    : _live(live), _store(store), _fw(fwVersion) {}

bool CommandHandler::consumeConfigChanged() {
    const bool c = _changed;
    _changed = false;
    return c;
}

void CommandHandler::_ack(ResponseSink& sink, const char* what, bool ok, const char* err) const {
    char buf[BUF];
    if (formatAckSimple(buf, sizeof buf, what, ok, err)) sink.send(buf);
}

void CommandHandler::sendConfig(ResponseSink& sink) const {
    char buf[BUF];
    for (size_t g = 0; g < robotcfg::GROUP_COUNT; g++) {
        if (formatCfgGroup(buf, sizeof buf, _live, robotcfg::GROUPS[g])) sink.send(buf);
    }
    if (formatCfgMeta(buf, sizeof buf, _live.profile, _dirty, _fw)) sink.send(buf);
}

void CommandHandler::_reloadFromStore(uint8_t profile) {
    _store.loadHardware(_live.hw);
    _store.loadProfile(profile, _live.tune);
    _live.profile = profile;
    _dirty = false;
    _changed = true;
}

void CommandHandler::_handleSet(const Command& cmd, bool running, ResponseSink& sink) {
    SetResult results[Command::MAX_ITEMS];
    for (uint8_t i = 0; i < cmd.count; i++) {
        results[i] = robotcfg::set(_live, cmd.items[i].key, cmd.items[i].value, running);
    }
    // 2ª passada: vmin/vmax chegando juntos (legado "spd") dependem da ordem
    for (uint8_t i = 0; i < cmd.count; i++) {
        if (results[i] == SetResult::OUT_OF_RANGE) {
            results[i] = robotcfg::set(_live, cmd.items[i].key, cmd.items[i].value, running);
        }
    }
    char buf[BUF];
    for (uint8_t i = 0; i < cmd.count; i++) {
        if (results[i] == SetResult::OK) {
            _dirty = true;
            _changed = true;
        }
        float applied = 0.0f;
        robotcfg::get(_live, cmd.items[i].key, applied);
        if (formatAck(buf, sizeof buf, cmd.items[i].key, results[i], applied)) sink.send(buf);
    }
}

void CommandHandler::handle(const Command& cmd, bool running, ResponseSink& sink,
                            ControlMsg& msg) {
    msg = ControlMsg{};
    switch (cmd.type) {
        case CmdType::SET:
            _handleSet(cmd, running, sink);
            break;
        case CmdType::GET:
            sendConfig(sink);
            break;
        case CmdType::SAVE: {
            if (running) { _ack(sink, "save", false, "running"); break; }
            const bool ok = _store.saveActive(_live);
            if (ok) _dirty = false;
            _ack(sink, "save", ok, ok ? nullptr : "store");
            break;
        }
        case CmdType::LOAD:
            if (running) { _ack(sink, "load", false, "running"); break; }
            _reloadFromStore(_live.profile);
            _ack(sink, "load", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::DEFAULTS:
            if (running) { _ack(sink, "defaults", false, "running"); break; }
            _live.hw = robotcfg::factoryHardware();
            _live.tune = robotcfg::factoryProfile(_live.profile);
            _dirty = true;
            _changed = true;
            _ack(sink, "defaults", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::PROFILE:
            if (cmd.profile >= PROFILE_COUNT) { _ack(sink, "prof", false, "range"); break; }
            if (running) { _ack(sink, "prof", false, "running"); break; }
            if (_dirty && !cmd.force) { _ack(sink, "prof", false, "dirty"); break; }
            _reloadFromStore(cmd.profile);
            _ack(sink, "prof", true, nullptr);
            sendConfig(sink);
            break;
        case CmdType::SENSOR_CAL:
            msg.req = ControlRequest::SENSOR_CAL;
            _ack(sink, "scal", true, nullptr);
            break;
        case CmdType::START:
            msg.req = ControlRequest::START;
            _ack(sink, "start", true, nullptr);
            break;
        case CmdType::STOP:
            msg.req = ControlRequest::STOP;
            _ack(sink, "stop", true, nullptr);
            break;
        case CmdType::RESET:
            msg.req = ControlRequest::RESET;
            _ack(sink, "reset", true, nullptr);
            break;
        case CmdType::ENC_CAL_START:
        case CmdType::ENC_CAL_FINISH:
            if (running) { _ack(sink, "cal", false, "running"); break; }
            msg.req = (cmd.type == CmdType::ENC_CAL_START) ? ControlRequest::ENC_CAL_START
                                                           : ControlRequest::ENC_CAL_FINISH;
            msg.leftSide = cmd.leftSide;
            msg.rotations = cmd.rotations;
            _ack(sink, "cal", true, nullptr);
            break;
        case CmdType::NONE:
            break;
    }
}
```

- [ ] **Step 5: Reescrever o BLETuner como transporte**

`src/comm/BLETuner.h`:

```cpp
#pragma once
#include <cstddef>
#include "comm/CommandQueue.h"
#include "comm/SpinLock.h"

#ifndef NATIVE_BUILD
#include <NimBLEDevice.h>
#endif

/**
 * Transporte BLE (NimBLE). Recebe JSON na característica 0xABCE e enfileira; envia JSON
 * por notificação na 0xABCD. Não interpreta nada — isso é do CommandProtocol/CommandHandler.
 */
class BLETuner {
public:
    BLETuner() = default;

    void begin(const char* deviceName);
    bool popCommand(char* out, size_t len) { return _queue.pop(out, len); }
    bool consumeNewConnection();
    bool isConnected() const;
    void notify(const char* json);

#ifdef NATIVE_BUILD
    bool injectForTest(const char* json);
#endif

    static constexpr const char* SERVICE_UUID  = "0000abcc-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_TEL_UUID = "0000abcd-0000-1000-8000-00805f9b34fb";
    static constexpr const char* CHAR_CMD_UUID = "0000abce-0000-1000-8000-00805f9b34fb";

private:
    CommandQueue     _queue;
    mutable SpinLock _lock;
    bool _connected = false;
    bool _newConn = false;

#ifndef NATIVE_BUILD
    NimBLECharacteristic* _charTel = nullptr;
    NimBLECharacteristic* _charCmd = nullptr;
    static BLETuner* _instance;

    class WriteCallback : public NimBLECharacteristicCallbacks {
    public:
        void onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo& connInfo) override;
    };
    class ServerCallback : public NimBLEServerCallbacks {
    public:
        void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override;
        void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override;
    };
    WriteCallback  _writeCallback;
    ServerCallback _serverCallback;
#endif
};
```

`src/comm/BLETuner.cpp`:

```cpp
#include "comm/BLETuner.h"
#include <cstring>

bool BLETuner::consumeNewConnection() {
    SpinGuard guard(_lock);
    const bool n = _newConn;
    _newConn = false;
    return n;
}

bool BLETuner::isConnected() const {
    SpinGuard guard(_lock);
    return _connected;
}

#ifndef NATIVE_BUILD

BLETuner* BLETuner::_instance = nullptr;

void BLETuner::WriteCallback::onWrite(NimBLECharacteristic* pChar, NimBLEConnInfo&) {
    if (!_instance) return;
    const std::string v = pChar->getValue();
    _instance->_queue.push(v.c_str(), v.size());
}

void BLETuner::ServerCallback::onConnect(NimBLEServer*, NimBLEConnInfo&) {
    if (!_instance) return;
    SpinGuard guard(_instance->_lock);
    _instance->_connected = true;
    _instance->_newConn = true;
}

void BLETuner::ServerCallback::onDisconnect(NimBLEServer* pServer, NimBLEConnInfo&, int) {
    if (_instance) {
        SpinGuard guard(_instance->_lock);
        _instance->_connected = false;
    }
    pServer->startAdvertising();
}

void BLETuner::begin(const char* deviceName) {
    _instance = this;
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setMTU(185);
    NimBLEServer* server = NimBLEDevice::createServer();
    server->setCallbacks(&_serverCallback);
    NimBLEService* service = server->createService(SERVICE_UUID);
    _charTel = service->createCharacteristic(CHAR_TEL_UUID,
                                             NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    _charCmd = service->createCharacteristic(CHAR_CMD_UUID,
                                             NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    _charCmd->setCallbacks(&_writeCallback);
    service->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SERVICE_UUID);
    adv->start();
}

void BLETuner::notify(const char* json) {
    if (!json || !_charTel || !isConnected()) return;
    // setValue(ptr, len): envia só os bytes do texto, nunca o buffer inteiro
    _charTel->setValue(reinterpret_cast<const uint8_t*>(json), std::strlen(json));
    _charTel->notify();
}

#else  // NATIVE_BUILD

void BLETuner::begin(const char*) {}
void BLETuner::notify(const char*) {}
bool BLETuner::injectForTest(const char* json) {
    return json && _queue.push(json, std::strlen(json));
}

#endif
```

O `adv->addServiceUUID` faz o serviço aparecer no anúncio, o que permite ao dashboard filtrar
por serviço (`requestDevice({filters:[{services:[…]}]})`, Tarefa 20). Conferir a assinatura de
`NimBLEDevice::setMTU` e `getAdvertising` na versão instalada do NimBLE-Arduino
(`.pio/libdeps/esp32dev/NimBLE-Arduino/src/NimBLEDevice.h`) na Tarefa 18, quando o ESP32 for
compilado.

- [ ] **Step 6: Rodar os testes**

Run: `pio test -e native`
Expected: todas as suítes PASSED (`test_command_handler` 15 casos, `test_ble_tuner` 3).

- [ ] **Step 7: Commit**

```bash
git add src/comm/Mailbox.h src/comm/CommandHandler.h src/comm/CommandHandler.cpp \
        src/comm/BLETuner.h src/comm/BLETuner.cpp \
        test/test_command_handler test/test_ble_tuner
git commit -m "feat(comm): command handler with persistence rules and BLE transport

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 18: main.cpp — só fiação, tasks fixas nos núcleos

Implementa a spec §4.7 (tasks e núcleos) e resolve F-09. Volta a compilar para ESP32.

**Files:**
- Modify: `src/main.cpp` (reescrever), `src/config.h`

**Interfaces:**
- Consumes: tudo das Tarefas 2–17.
- Produces: firmware `4.0.0` que compila em `esp32dev`.

- [ ] **Step 1: Limpar o `config.h`**

Remover os blocos `PID DEFAULTS`, `VELOCIDADE`, `CASCADE PID` (`KP_DEFAULT`…`KD_DEFAULT`,
`BASE_SPEED_DEFAULT`, `MAX_SPEED`, `MIN_SPEED`, `ERROR_THRESHOLD`, `KP_VEL_DEFAULT`…,
`MAX_RPM_DEFAULT`, `BASE_RPM_DEFAULT`, `RPM_HARD_CEILING`) — os valores de fábrica agora
moram em `src/config/RobotConfig.h`. Trocar `SENSOR_HEIGHT_MM` para 3 (patins, spec §5.4) e
acrescentar:

```cpp
// ═══ BATERIA ═══════════════════════════════════════════════════════════════
// Divisor 100 kΩ / 47 kΩ → Vadc = Vbat × 47/147. 8,4 V → 2,69 V (dentro do ADC_11db).
#define VBAT_DIVIDER_RATIO  (147.0f / 47.0f)

// ═══ BUZZER ════════════════════════════════════════════════════════════════
#define BUZZER_LEDC_CHANNEL   2      // canais 0 e 1 são dos motores
#define BUZZER_FREQ_HZ     2500
```

Run: `grep -rn "KP_DEFAULT\|BASE_SPEED_DEFAULT\|MAX_RPM_DEFAULT\|RPM_HARD_CEILING" src test`
Expected: nenhuma ocorrência fora de `src/main.cpp` (que é reescrito no próximo passo).

- [ ] **Step 2: Reescrever `src/main.cpp`**

```cpp
// main.cpp — LFR v2: fiação dos módulos e tasks. Nenhuma regra de negócio aqui.
// Core 1: task "control" a 500 Hz (sensor → PID → motor), prioridade alta.
// Core 0: task "comm" a 100 Hz (BLE, comandos, telemetria), junto da pilha NimBLE.
#ifndef NATIVE_BUILD

#include <Arduino.h>
#include <SPI.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "config.h"
#include "comm/Annunciator.h"
#include "comm/BLETuner.h"
#include "comm/CommandHandler.h"
#include "comm/CommandProtocol.h"
#include "comm/Mailbox.h"
#include "comm/Telemetry.h"
#include "config/ConfigStore.h"
#include "config/RobotConfig.h"
#include "motors/MotorDriver.h"
#include "sensors/BatteryMonitor.h"
#include "sensors/ButtonInput.h"
#include "sensors/Calibration.h"
#include "sensors/Encoder.h"
#include "sensors/SensorArray.h"
#include "sensors/VelocityEstimator.h"
#include "storage/NvsStore.h"
#include "strategy/ControlLoop.h"
#include "strategy/RobotStateMachine.h"

static constexpr const char* DEVICE_NAME = "LFR-RACER-01";
static constexpr const char* FW_VERSION  = "4.0.0";
static constexpr int      OVERRUN_LIMIT  = 5;     // ciclos seguidos acima do período
static constexpr int      BATTERY_EVERY  = 50;    // 50 × 2 ms = 100 ms
static constexpr int      STATUS_EVERY   = 250;   // 250 × 2 ms = 500 ms
static constexpr uint32_t TELEMETRY_MS   = 33;    // ~30 Hz

// ── Hardware ───────────────────────────────────────────────────────────────
static Calibration calibration(SENSOR_COUNT);
static SensorArray sensors(PIN_SPI_CS, SPI_FREQ, SENSOR_COUNT, calibration);
static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
static Encoder encL(PIN_ENC_LEFT_A, PIN_ENC_LEFT_B);
static Encoder encR(PIN_ENC_RIGHT_A, PIN_ENC_RIGHT_B);
static NvsStore nvs;
static VelocityEstimator velL(encL, nvs, "left");
static VelocityEstimator velR(encR, nvs, "right");

// ── Lógica ─────────────────────────────────────────────────────────────────
static ConfigStore    configStore(nvs);
static ControlLoop    control(calibration, PWM_MAX);
static BatteryMonitor battery(VBAT_DIVIDER_RATIO);
static ButtonInput    button;
static Annunciator    annunciator;
static BLETuner       ble;
static RobotConfig    g_liveCfg;          // dono: task comm (depois do setup)

// ── Canais entre núcleos ───────────────────────────────────────────────────
static Mailbox<RobotConfig>    g_cfgBox;  // comm → control
static Mailbox<TelemetryFrame> g_telBox;  // control → comm
static Mailbox<StatusFrame>    g_staBox;  // control → comm
static Mailbox<CalReport>      g_calBox;  // control → comm
static QueueHandle_t           g_ctlQueue = nullptr;  // comm → control (ControlMsg)
static std::atomic<uint8_t>    g_state{0};
static bool g_cfgErr = false;
static bool g_calValidAtBoot = false;

static void applyRequest(const ControlMsg& msg, RobotStateMachine& sm, const SmInputs& in,
                         uint32_t nowMs) {
    switch (msg.req) {
        case ControlRequest::START:      sm.handle(SmEvent::CMD_START, in, nowMs); break;
        case ControlRequest::STOP:       sm.handle(SmEvent::CMD_STOP, in, nowMs); break;
        case ControlRequest::RESET:      sm.handle(SmEvent::CMD_RESET, in, nowMs); break;
        case ControlRequest::SENSOR_CAL: sm.handle(SmEvent::CMD_SENSOR_CAL, in, nowMs); break;
        case ControlRequest::ENC_CAL_START:
            if (!sm.isActive()) {
                (msg.leftSide ? velL : velR).startCalibration();
                annunciator.beep(1, 50, 0, nowMs);
            }
            break;
        case ControlRequest::ENC_CAL_FINISH:
            if (!sm.isActive()) {
                VelocityEstimator& v = msg.leftSide ? velL : velR;
                CalReport r;
                r.kind = CalReport::Kind::ENCODER;
                r.ok = v.finishCalibration(msg.rotations);
                r.left = msg.leftSide;
                r.ppr = v.getEffectivePPR();
                g_calBox.post(r);
                annunciator.beep(r.ok ? 1 : 3, r.ok ? 200 : 120, 120, nowMs);
            }
            break;
        case ControlRequest::NONE:
            break;
    }
}

static TelemetryFrame buildTelemetry(const int* raw, const ControlOutputs& out,
                                     uint32_t execUs, RobotState st, int pwmMax) {
    TelemetryFrame f;
    int norm[SENSOR_COUNT];
    calibration.normalize(raw, norm);
    for (int i = 0; i < SENSOR_COUNT; i++) f.s[i] = static_cast<uint8_t>(norm[i] / 10);
    f.pos  = out.error;
    f.corr = out.u;
    f.vL   = static_cast<int16_t>(std::lround(out.uL * pwmMax));
    f.vR   = static_cast<int16_t>(std::lround(out.uR * pwmMax));
    f.dtUs = static_cast<uint16_t>(std::min<uint32_t>(execUs, 65535));
    f.rpmL = static_cast<int16_t>(std::lround(velL.getRPM()));
    f.rpmR = static_cast<int16_t>(std::lround(velR.getRPM()));
    f.spL  = static_cast<int16_t>(std::lround(out.spL));
    f.spR  = static_cast<int16_t>(std::lround(out.spR));
    f.st   = static_cast<uint8_t>(st);
    f.lost = out.lost ? 1 : 0;
    return f;
}

// ═══ Task control — core 1, 500 Hz ═════════════════════════════════════════
static void taskControl(void*) {
    RobotConfig cfg;
    uint32_t cfgSeen = 0;
    g_cfgBox.fetch(cfg, cfgSeen);
    control.applyConfig(cfg);
    velL.setPprX4(cfg.hw.pprL);
    velR.setPprX4(cfg.hw.pprR);

    RobotStateMachine sm(g_calValidAtBoot, millis());
    annunciator.onState(sm.state(), sm.error(), millis());
    if (g_cfgErr) annunciator.beep(3, 60, 60, millis());

    TickType_t lastWake = xTaskGetTickCount();
    uint32_t prevUs = micros();
    RobotState prevState = sm.state();
    ErrorCode prevError = sm.error();
    int batteryTick = 0;
    int statusTick = 0;
    int overruns = 0;
    uint16_t ovrMax = 0;
    int raw[SENSOR_COUNT] = {};
    ControlOutputs out{};

    for (;;) {
        vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(LOOP_PERIOD_US / 1000));
        const uint32_t t0 = micros();
        const uint32_t dtUs = t0 - prevUs;
        prevUs = t0;
        const uint32_t nowMs = millis();

        if (g_cfgBox.fetch(cfg, cfgSeen)) {
            control.applyConfig(cfg);
            velL.setPprX4(cfg.hw.pprL);
            velR.setPprX4(cfg.hw.pprR);
        }

        if (++batteryTick >= BATTERY_EVERY) {
            batteryTick = 0;
            battery.update(analogReadMilliVolts(PIN_VBAT_ADC), 0.1f, nowMs, cfg.hw);
        }

        SmInputs in;
        in.calibrationValid = calibration.isValid();
        in.batteryOk = battery.hasReading() && battery.volts() >= cfg.hw.vcut;

        ControlMsg msg;
        while (xQueueReceive(g_ctlQueue, &msg, 0) == pdTRUE) applyRequest(msg, sm, in, nowMs);

        const ButtonEvent be = button.update(digitalRead(PIN_BTN_START) == LOW, nowMs);
        if (be == ButtonEvent::SHORT) sm.handle(SmEvent::BTN_SHORT, in, nowMs);
        if (be == ButtonEvent::LONG) sm.handle(SmEvent::BTN_LONG, in, nowMs);

        sensors.readAll(raw);
        velL.update(dtUs);  // sempre: drena o delta mesmo parado
        velR.update(dtUs);

        if (sm.state() == RobotState::CALIBRATING) {
            if (prevState != RobotState::CALIBRATING) calibration.reset();
            calibration.update(raw);
            if (sm.calibrationWindowOver(nowMs)) {
                CalReport r;
                int nBad = 0;
                r.ok = calibration.isValid(r.bad, &nBad);
                r.nBad = static_cast<uint8_t>(nBad);
                // motores parados: gravar na NVS aqui não atrapalha o controle
                if (r.ok) configStore.saveCalibration(calibration.exportData());
                sm.handle(r.ok ? SmEvent::CAL_DONE_OK : SmEvent::CAL_DONE_FAIL, in, nowMs);
                g_calBox.post(r);
            }
        }

        if (sm.tick(nowMs, cfg.tune.startMs)) {
            control.start(raw, velL.getRPM(), velR.getRPM());
        }

        if (sm.state() == RobotState::RUNNING) {
            ControlInputs ci;
            ci.raw = raw;
            ci.rpmL = velL.getRPM();
            ci.rpmR = velR.getRPM();
            ci.vbat = battery.volts();
            ci.dtS = static_cast<float>(dtUs) * 1e-6f;
            out = control.step(ci);
            motors.setSpeed(MotorId::A, out.duties.a);
            motors.setSpeed(MotorId::B, out.duties.b);
            if (out.lostTimeout) sm.handle(SmEvent::LINE_LOST_TIMEOUT, in, nowMs);
            if (battery.isLow()) sm.handle(SmEvent::BATTERY_LOW, in, nowMs);
        } else {
            out = ControlOutputs{};
        }

        const uint32_t execUs = micros() - t0;
        const uint16_t execClamped = static_cast<uint16_t>(std::min<uint32_t>(execUs, 65535));
        ovrMax = std::max(ovrMax, execClamped);
        overruns = (execUs > LOOP_PERIOD_US) ? overruns + 1 : 0;
        if (overruns >= OVERRUN_LIMIT) {
            sm.handle(SmEvent::LOOP_OVERRUN, in, nowMs);
            overruns = 0;
        }
        if (!sm.motorsEnabled()) motors.stop();

        if (sm.state() != prevState || sm.error() != prevError) {
            annunciator.onState(sm.state(), sm.error(), nowMs);
        }
        const Indication ind = annunciator.update(nowMs);
        digitalWrite(PIN_LED_STATUS, ind.led ? HIGH : LOW);
        ledcWrite(BUZZER_LEDC_CHANNEL, ind.buzzer ? 128 : 0);

        g_state.store(static_cast<uint8_t>(sm.state()));
        g_telBox.post(buildTelemetry(raw, out, execUs, sm.state(), PWM_MAX));
        if (++statusTick >= STATUS_EVERY) {
            statusTick = 0;
            StatusFrame s;
            s.bat = battery.percent(cfg.hw.chem);
            s.vb = battery.volts();
            s.err = static_cast<uint8_t>(sm.error());
            s.ovr = ovrMax;
            ovrMax = 0;
            g_staBox.post(s);
        }
        prevState = sm.state();
        prevError = sm.error();
    }
}

// ═══ Task comm — core 0, 100 Hz ════════════════════════════════════════════
class BleSink : public ResponseSink {
public:
    void send(const char* json) override { ble.notify(json); }
};

static void taskComm(void*) {
    ble.begin(DEVICE_NAME);
    BleSink sink;
    CommandHandler handler(g_liveCfg, configStore, FW_VERSION);
    char in[CommandQueue::MAX_LEN];
    char outBuf[192];
    uint32_t telSeen = 0;
    uint32_t staSeen = 0;
    uint32_t calSeen = 0;
    uint32_t lastTel = 0;

    for (;;) {
        const uint32_t nowMs = millis();

        if (ble.consumeNewConnection()) {
            if (formatInfo(outBuf, sizeof outBuf, DEVICE_NAME, FW_VERSION, g_cfgErr)) {
                ble.notify(outBuf);
            }
            handler.sendConfig(sink);
        }

        while (ble.popCommand(in, sizeof in)) {
            Command cmd;
            if (!parseCommand(in, cmd)) {
                if (formatAckSimple(outBuf, sizeof outBuf, "?", false, "parse")) {
                    ble.notify(outBuf);
                }
                continue;
            }
            ControlMsg msg;
            handler.handle(cmd, isActiveState(g_state.load()), sink, msg);
            if (msg.req != ControlRequest::NONE) xQueueSend(g_ctlQueue, &msg, 0);
            if (handler.consumeConfigChanged()) g_cfgBox.post(handler.live());
        }

        CalReport cr;
        if (g_calBox.fetch(cr, calSeen)) {
            const bool ok = (cr.kind == CalReport::Kind::SENSOR)
                ? formatCalResult(outBuf, sizeof outBuf, cr)
                : formatEncCal(outBuf, sizeof outBuf, cr);
            if (ok) ble.notify(outBuf);
            if (cr.kind == CalReport::Kind::ENCODER && cr.ok) {
                // PPR já persistido pelo VelocityEstimator; espelha na config viva sem "dirty"
                RobotConfig& live = g_liveCfg;
                (cr.left ? live.hw.pprL : live.hw.pprR) = cr.ppr;
            }
        }

        if (nowMs - lastTel >= TELEMETRY_MS) {
            lastTel = nowMs;
            TelemetryFrame tf;
            if (g_telBox.fetch(tf, telSeen) && formatTelemetry(outBuf, sizeof outBuf, tf)) {
                ble.notify(outBuf);
            }
        }

        StatusFrame sf;
        if (g_staBox.fetch(sf, staSeen)) {
            sf.prof = handler.live().profile;
            sf.dirty = handler.dirty() ? 1 : 0;
            if (formatStatus(outBuf, sizeof outBuf, sf)) ble.notify(outBuf);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ═══ Setup ═════════════════════════════════════════════════════════════════
void setup() {
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    digitalWrite(PIN_LED_STATUS, LOW);
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_FREQ_HZ, 8);
    ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);

    SPI.begin(PIN_SPI_CLK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_CS);
    sensors.begin();
    motors.begin();
    encL.begin();
    encR.begin();
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_VBAT_ADC, ADC_11db);

    nvs.begin("lfr");
    velL.begin();
    velR.begin();

    RobotConfig cfg = robotcfg::factoryConfig();
    const ConfigStore::LoadReport rep = configStore.load(cfg);
    g_cfgErr = !rep.hardwareOk || !rep.profileOk;
    g_liveCfg = cfg;

    CalibrationData cal{};
    if (configStore.loadCalibration(cal)) calibration.importData(cal);
    g_calValidAtBoot = calibration.isValid();

    g_cfgBox.post(cfg);
    g_ctlQueue = xQueueCreate(8, sizeof(ControlMsg));

    xTaskCreatePinnedToCore(taskComm, "comm", 8192, nullptr, 2, nullptr, 0);
    xTaskCreatePinnedToCore(taskControl, "control", 8192, nullptr,
                            configMAX_PRIORITIES - 2, nullptr, 1);
}

void loop() {
    vTaskDelete(nullptr);  // o trabalho está nas tasks
}

#endif  // NATIVE_BUILD
```

A espelhagem do PPR em `g_liveCfg` dentro da task comm é segura porque `g_liveCfg` só é
tocado pela task comm depois do `setup()`.

- [ ] **Step 3: Compilar para o ESP32**

Run: `pio run -e esp32dev 2>&1 | tee /tmp/claude-1000/build-esp32.log | tail -30`
Expected: `SUCCESS`.

Run: `grep -n "warning:" /tmp/claude-1000/build-esp32.log | grep "/src/" || echo "sem warnings em src/"`
Expected: `sem warnings em src/`.

Se a API do NimBLE-Arduino instalado divergir (`setMTU`, `getAdvertising`, assinatura dos
callbacks, `setValue(const uint8_t*, size_t)`), ajustar `BLETuner.cpp` seguindo o header da
versão em `.pio/libdeps/esp32dev/NimBLE-Arduino/src/` e registrar a versão no commit.

- [ ] **Step 4: Rodar os testes nativos**

Run: `pio test -e native`
Expected: todas as suítes PASSED.

- [ ] **Step 5: Commit**

```bash
git add src/main.cpp src/config.h
git commit -m "refactor(main): wiring-only main with control task on core 1 (F-09)

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 19: Ambientes de bancada e exemplos atualizados

**Files:**
- Modify: `platformio.ini`, `examples/differential_hw_test.cpp` (reescrever),
  `examples/velocity_calibration_test.cpp`

**Interfaces:**
- Consumes: `OutputStage`, `mixSteering`, `RobotConfig` (Tarefas 7, 8, 13);
  `VelocityEstimator::update(dtUs)` (Tarefa 6).
- Produces: ambientes `bench_motor`, `bench_encoder`, `bench_sensor_raw`, `bench_sensor_cal`,
  `bench_velocity_cal`, `bench_differential` — `pio run -e <amb> -t upload` grava o teste.

- [ ] **Step 1: Acrescentar os ambientes ao `platformio.ini`**

```ini
; ── Testes de bancada (examples/) — cada um substitui o main.cpp ──────────────
[bench]
extends = env:esp32dev

[env:bench_motor]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/motor_smoke_test.cpp>

[env:bench_encoder]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/encoder_smoke_test.cpp>

[env:bench_sensor_raw]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/sensor_raw_test.cpp>

[env:bench_sensor_cal]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/sensor_calibration_test.cpp>

[env:bench_velocity_cal]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/velocity_calibration_test.cpp>

[env:bench_differential]
extends = bench
build_src_filter = +<*> -<main.cpp> +<../examples/differential_hw_test.cpp>
```

- [ ] **Step 2: Rodar a compilação e ver quais quebram**

Run: `for e in motor encoder sensor_raw sensor_cal velocity_cal differential; do pio run -e bench_$e >/tmp/claude-1000/bench_$e.log 2>&1 && echo "$e OK" || echo "$e FALHOU"; done`
Expected: `differential FALHOU` (usa `DifferentialDrive`) e `velocity_cal` compila mas com a
unidade errada; os outros OK.

- [ ] **Step 3: Corrigir o `velocity_calibration_test.cpp`**

No `loop()`, trocar o cálculo de `dt` e as chamadas:

```cpp
void loop() {
    static uint32_t last = micros();
    const uint32_t now = micros();
    const uint32_t dtUs = now - last;
    last = now;
    velL.update(dtUs);
    velR.update(dtUs);
    Serial.printf("rpmL=%.1f  rpmR=%.1f\n", velL.getRPM(), velR.getRPM());
    delay(200);
}
```

- [ ] **Step 4: Reescrever o `differential_hw_test.cpp`**

```cpp
// Teste de movimentos diferenciais — valida MotorDriver + mixSteering + OutputStage no ESP32
// Sequência: reto → curva dir → curva esq → giro horário → giro anti-horário → parado
// Cada movimento deve ser visualmente distinto. Se algum lado girar ao contrário, ajustar
// invL/invR (ou swap) pelo BLE — não mexer na fiação.
#ifndef NATIVE_BUILD
#include <Arduino.h>
#include "config.h"
#include "config/RobotConfig.h"
#include "control/Mixer.h"
#include "motors/MotorDriver.h"
#include "motors/OutputStage.h"

static MotorDriver::Config cfgA = {
    PIN_MOTOR_A_PWM, PIN_MOTOR_A_IN1, PIN_MOTOR_A_IN2, 0, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver::Config cfgB = {
    PIN_MOTOR_B_PWM, PIN_MOTOR_B_IN1, PIN_MOTOR_B_IN2, 1, PWM_FREQ, PWM_RESOLUTION
};
static MotorDriver motors(cfgA, cfgB, PIN_MOTOR_STBY);
static const OutputStage output(PWM_MAX);
static RobotConfig cfg = robotcfg::factoryConfig();

struct Movement {
    const char* name;
    float base;
    float u;
    uint32_t durationMs;
};

static const Movement SEQUENCE[] = {
    {"RETO",          0.40f,  0.00f, 1500},
    {"CURVA DIR",     0.30f,  0.20f, 1200},
    {"CURVA ESQ",     0.30f, -0.20f, 1200},
    {"GIRO HORARIO",  0.00f,  0.40f, 1000},
    {"GIRO ANTI-HOR", 0.00f, -0.40f, 1000},
    {"PARADO",        0.00f,  0.00f, 2000},
};
static constexpr int SEQ_LEN = sizeof(SEQUENCE) / sizeof(SEQUENCE[0]);

static int step = 0;
static uint32_t stepStart = 0;
static bool running = false;

static void applyStep(int idx) {
    const Movement& mv = SEQUENCE[idx];
    float l = 0.0f;
    float r = 0.0f;
    mixSteering(mv.base, mv.u, l, r);
    cfg.tune.vcomp = 0;  // bancada: sem leitura de bateria
    const MotorDuties d = output.compute(l, r, 0.0f, cfg.hw, cfg.tune);
    motors.setSpeed(MotorId::A, d.a);
    motors.setSpeed(MotorId::B, d.b);
    Serial.printf("  -> %-14s A=%5d  B=%5d\n", mv.name, d.a, d.b);
}

void setup() {
    Serial.begin(115200);
    motors.begin();
    pinMode(PIN_BTN_START, INPUT_PULLUP);
    pinMode(PIN_LED_STATUS, OUTPUT);
    Serial.println("Pressione o botao BOOT para iniciar a sequencia (rodas fora do chao)");
}

void loop() {
    if (!running && digitalRead(PIN_BTN_START) == LOW) {
        running = true;
        step = 0;
        stepStart = millis();
        digitalWrite(PIN_LED_STATUS, HIGH);
        applyStep(step);
        return;
    }
    if (!running) return;
    if (step >= SEQ_LEN) {
        motors.stop();
        running = false;
        digitalWrite(PIN_LED_STATUS, LOW);
        Serial.println("Sequencia completa. Pressione BOOT para repetir.");
        return;
    }
    if (millis() - stepStart >= SEQUENCE[step].durationMs) {
        stepStart = millis();
        step++;
        if (step < SEQ_LEN) applyStep(step);
    }
}
#endif
```

- [ ] **Step 5: Compilar todos os ambientes de bancada**

Run: `for e in motor encoder sensor_raw sensor_cal velocity_cal differential; do pio run -e bench_$e >/tmp/claude-1000/bench_$e.log 2>&1 && echo "$e OK" || echo "$e FALHOU"; done`
Expected: os seis `OK`. Se `sensor_cal` ou `sensor_raw` usarem constantes removidas do
`config.h`, trocar pelas equivalentes (`Calibration::NORM_MAX`, `SENSOR_COUNT`).

- [ ] **Step 6: Commit**

```bash
git add platformio.ini examples
git commit -m "build(bench): one PlatformIO env per bench example, examples on new APIs

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 20: Dashboard com BLE e simulador no protocolo novo

Implementa a spec §4.8. Hoje nenhum dashboard fala com o robô: o `lfr-cockpit-mock.html` só
tem WebSocket e o `lfr-cockpit.html` usa o protocolo BLE antigo.

**Files:**
- Modify: `docs/dashboard/lfr-cockpit-mock.html`, `docs/dashboard/lfr_sim.py`
- Delete: `docs/dashboard/lfr-cockpit.html`
- Test: `docs/dashboard/test_lfr_sim.py`

**Interfaces:**
- Consumes: protocolo da Tarefa 10 (`set/get/save/load/defaults/prof/scal`, `ack`, `cfg`,
  `tel`, `sta`, `calres`, `ecal`, `info`) e a tabela de chaves da Tarefa 8.
- Produces: dashboard com transporte BLE **ou** WebSocket (mesmo JSON nos dois) e simulador
  que responde como o firmware.

- [ ] **Step 1: Teste do simulador**

`docs/dashboard/test_lfr_sim.py`:

```python
"""Testes do modelo de configuração do simulador (rodar: python3 -m unittest test_lfr_sim)."""
import json
import math
import unittest

from lfr_sim import ConfigModel


class ConfigModelTest(unittest.TestCase):
    def setUp(self):
        self.m = ConfigModel()

    def replies(self, cmd, running=False):
        return [json.loads(r) for r in self.m.command(json.dumps(cmd), running)]

    def test_set_valid_marks_dirty(self):
        r = self.replies({"t": "set", "k": "kp", "v": 1.5})
        self.assertEqual(r[0], {"t": "ack", "k": "kp", "ok": 1, "v": 1.5})
        self.assertTrue(self.m.dirty)

    def test_set_out_of_range(self):
        r = self.replies({"t": "set", "k": "kp", "v": 99})
        self.assertEqual(r[0]["err"], "range")

    def test_set_nan(self):
        r = [json.loads(x) for x in self.m.command('{"t":"set","k":"kp","v":NaN}', False)]
        self.assertEqual(r[0]["err"], "range")

    def test_hardware_locked_while_running(self):
        r = self.replies({"t": "set", "k": "invL", "v": 1}, running=True)
        self.assertEqual(r[0]["err"], "running")

    def test_get_sends_groups_and_meta(self):
        r = self.replies({"t": "get"})
        self.assertEqual([x["g"] for x in r], ["hw", "pid", "vel", "spd", "misc", "meta"])
        for x in r:
            self.assertLessEqual(len(json.dumps(x, separators=(",", ":"))), 180)

    def test_profile_refused_when_dirty(self):
        self.replies({"t": "set", "k": "kp", "v": 2})
        r = self.replies({"t": "prof", "n": 2})
        self.assertEqual(r[0]["err"], "dirty")
        r = self.replies({"t": "prof", "n": 2, "f": 1})
        self.assertEqual(r[0]["ok"], 1)
        self.assertTrue(math.isclose(self.m.tune["vmax"], 0.70))

    def test_save_then_load(self):
        self.replies({"t": "set", "k": "kp", "v": 2})
        self.replies({"t": "save"})
        self.replies({"t": "set", "k": "kp", "v": 3})
        self.replies({"t": "load"})
        self.assertEqual(self.m.tune["kp"], 2)
        self.assertFalse(self.m.dirty)


if __name__ == "__main__":
    unittest.main()
```

Run: `cd docs/dashboard && python3 -m unittest test_lfr_sim -v`
Expected: FAIL (`ImportError: cannot import name 'ConfigModel'`).

- [ ] **Step 2: Modelo de configuração no simulador**

Em `docs/dashboard/lfr_sim.py`, acrescentar (antes de `class LFRPhysics`) a tabela de chaves,
idêntica à de `src/config/RobotConfig.cpp`, e o modelo:

```python
# Tabela de chaves — espelha src/config/RobotConfig.cpp (chave, grupo, mín, máx, hw, ao vivo)
KEYS = [
    ("invL", "hw", 0, 1, True, False), ("invR", "hw", 0, 1, True, False),
    ("swap", "hw", 0, 1, True, False), ("einvL", "hw", 0, 1, True, False),
    ("einvR", "hw", 0, 1, True, False), ("pol", "hw", 0, 1, True, False),
    ("sinv", "hw", 0, 1, True, False), ("chem", "hw", 0, 1, True, False),
    ("vcut", "hw", 5.6, 7.6, True, False), ("vbk", "hw", 0.8, 1.2, True, False),
    ("pprL", "hw", 1, 10000, True, False), ("pprR", "hw", 1, 10000, True, False),
    ("kp", "pid", 0, 20, False, True), ("ki", "pid", 0, 50, False, True),
    ("kd", "pid", 0, 2, False, True), ("dlp", "pid", 0.05, 1, False, True),
    ("mode", "vel", 0, 1, False, False), ("kpv", "vel", 0, 0.05, False, True),
    ("kiv", "vel", 0, 1, False, True), ("kdv", "vel", 0, 0.01, False, True),
    ("rpmMax", "vel", 100, 2000, False, True),
    ("vmax", "spd", 0, 1, False, True), ("vmin", "spd", 0, 1, False, True),
    ("thr", "spd", 0.05, 1, False, True), ("acc", "spd", 0.1, 50, False, True),
    ("vref", "misc", 5.0, 8.4, False, True), ("vcomp", "misc", 0, 1, False, True),
    ("lostMs", "misc", 50, 5000, False, True), ("startMs", "misc", 0, 5000, False, True),
]
INT_KEYS = {"invL", "invR", "swap", "einvL", "einvR", "pol", "sinv", "chem", "mode",
            "vcomp", "lostMs", "startMs"}
GROUPS = ["hw", "pid", "vel", "spd", "misc"]
FACTORY_HW = {"invL": 0, "invR": 0, "swap": 0, "einvL": 0, "einvR": 0, "pol": 1, "sinv": 0,
              "chem": 1, "vcut": 6.4, "vbk": 1.0, "pprL": 600.0, "pprR": 600.0}
FACTORY_TUNE = {"mode": 0, "kp": 1.0, "ki": 0.0, "kd": 0.03, "dlp": 0.5, "kpv": 0.002,
                "kiv": 0.005, "kdv": 0.0, "vmax": 0.35, "vmin": 0.25, "thr": 0.6, "acc": 2.0,
                "rpmMax": 700.0, "vref": 7.4, "vcomp": 1, "lostMs": 300, "startMs": 1000}
FACTORY_VMAX = [0.35, 0.50, 0.70]


def factory_profile(n):
    p = dict(FACTORY_TUNE)
    p["vmax"] = FACTORY_VMAX[n if 0 <= n < 3 else 0]
    return p


class ConfigModel:
    """Mesmas regras do CommandHandler do firmware (src/comm/CommandHandler.cpp)."""

    def __init__(self):
        self.hw = dict(FACTORY_HW)
        self.tune = factory_profile(0)
        self.profile = 0
        self.dirty = False
        self.saved_hw = None
        self.saved = {}

    @staticmethod
    def _ack(k, ok=True, err=None, v=None):
        msg = {"t": "ack", "k": k, "ok": 1 if ok else 0}
        if ok and v is not None:
            msg["v"] = v
        if not ok:
            msg["err"] = err
        return json.dumps(msg, separators=(",", ":"))

    def _group(self, g):
        msg = {"t": "cfg", "g": g}
        for k, grp, *_ in KEYS:
            if grp == g:
                msg[k] = self.hw[k] if k in self.hw else self.tune[k]
        return json.dumps(msg, separators=(",", ":"))

    def config_dump(self):
        out = [self._group(g) for g in GROUPS]
        out.append(json.dumps({"t": "cfg", "g": "meta", "prof": self.profile,
                               "dirty": int(self.dirty), "fw": "4.0.0-sim"},
                              separators=(",", ":")))
        return out

    def _set(self, k, v, running):
        spec = next((s for s in KEYS if s[0] == k), None)
        if spec is None:
            return self._ack(k, False, "key")
        _, _, lo, hi, is_hw, live = spec
        if running and not live:
            return self._ack(k, False, "running")
        if not isinstance(v, (int, float)) or not math.isfinite(v):
            return self._ack(k, False, "range")
        if k in INT_KEYS:
            v = round(v)
        if v < lo or v > hi:
            return self._ack(k, False, "range")
        if k == "vmin" and v > self.tune["vmax"]:
            return self._ack(k, False, "range")
        if k == "vmax" and v < self.tune["vmin"]:
            return self._ack(k, False, "range")
        (self.hw if is_hw else self.tune)[k] = v
        self.dirty = True
        return self._ack(k, True, v=v)

    def _reload(self, n):
        self.hw = dict(self.saved_hw) if self.saved_hw else dict(FACTORY_HW)
        self.tune = dict(self.saved[n]) if n in self.saved else factory_profile(n)
        self.profile = n
        self.dirty = False

    def command(self, raw, running=False):
        """Processa um comando e devolve a lista de respostas JSON (strings)."""
        try:
            data = json.loads(raw)
        except ValueError:
            return [self._ack("?", False, "parse")]
        kind = data.get("t")
        if kind == "set":
            return [self._set(data.get("k", "?"), data.get("v"), running)]
        if kind == "pid":
            return [self._set(k, data[k], running) for k in ("kp", "ki", "kd") if k in data]
        if kind == "pidv":
            mapping = {"kp": "kpv", "ki": "kiv", "kd": "kdv"}
            return [self._set(mapping[k], data[k], running) for k in mapping if k in data]
        if kind == "get":
            return self.config_dump()
        if kind in ("save", "load", "defaults", "prof") and running:
            return [self._ack(kind, False, "running")]
        if kind == "save":
            self.saved_hw = dict(self.hw)
            self.saved[self.profile] = dict(self.tune)
            self.dirty = False
            return [self._ack("save")]
        if kind == "load":
            self._reload(self.profile)
            return [self._ack("load")] + self.config_dump()
        if kind == "defaults":
            self.hw = dict(FACTORY_HW)
            self.tune = factory_profile(self.profile)
            self.dirty = True
            return [self._ack("defaults")] + self.config_dump()
        if kind == "prof":
            n = data.get("n")
            if not isinstance(n, int) or not 0 <= n < 3:
                return [self._ack("prof", False, "range")]
            if self.dirty and not data.get("f"):
                return [self._ack("prof", False, "dirty")]
            self._reload(n)
            return [self._ack("prof")] + self.config_dump()
        if kind in ("start", "stop", "reset", "scal", "cal"):
            return [self._ack(kind)]
        return [self._ack(kind or "?", False, "parse")]
```

Run: `cd docs/dashboard && python3 -m unittest test_lfr_sim -v`
Expected: 7 testes OK.

- [ ] **Step 3: Simulador emite `tel`/`sta` novos e usa o modelo**

Em `LFRPhysics`: trocar `self.pid`/`self.spd` por uma referência ao `ConfigModel` global
(`config = ConfigModel()` criado antes de `physics`); `step()` passa a devolver **dois** frames
quando for a vez do lento:

- `tel`: `pos` = erro normalizado (`err/3500`, limitado a ±1), `corr` = `kp·pos` limitado a ±1,
  `vL`/`vR` = `round((vmax ± corr) × 1023)` limitados a ±1023, `dt`, `rpmL`/`rpmR`/`spL`/`spR`
  inteiros (0 no modo 0), `s` = 8 inteiros 0–100, `st` = 4 rodando / 2 parado, `lost` = 0.
- `sta` a cada 15 frames (2 Hz a 30 Hz): `bat`, `vb` (`6.0 + bat/100 × 2.4`), `lap`, `laps`,
  `err` 0, `prof`, `dirty`, `ovr`.

No `handler`, trocar `physics.command(msg)` por:

```python
            for reply in config.command(msg, physics.running):
                await ws.send(reply)
            kind = json.loads(msg).get("t") if msg.startswith("{") else None
            if kind in ("start", "stop", "reset"):
                physics.command(msg)
```

e, logo depois do frame `info`, enviar `for r in config.config_dump(): await ws.send(r)`.
Atualizar o `info` para `"fw": "4.0.0-sim"` e o docstring do topo com o protocolo novo.

Run: `cd docs/dashboard && timeout 3 python3 lfr_sim.py; echo "exit $?"`
Expected: imprime o cabeçalho e sai com `exit 124` (timeout), sem traceback.

- [ ] **Step 4: Transporte BLE no dashboard**

Em `docs/dashboard/lfr-cockpit-mock.html`, dentro do `<script>` principal:

1. Acrescentar a camada de transporte (antes de `wsConnect`):

```js
// ── Transporte: BLE (robô) ou WebSocket (simulador) — mesmo JSON nos dois ──
const BLE_SERVICE = '0000abcc-0000-1000-8000-00805f9b34fb';
const BLE_TEL     = '0000abcd-0000-1000-8000-00805f9b34fb';
const BLE_CMD     = '0000abce-0000-1000-8000-00805f9b34fb';
const T = { kind: null, cmdChar: null, queue: Promise.resolve() };

async function bleConnect(){
  if(!navigator.bluetooth){ alert('Web Bluetooth indisponível. Use Chrome/Edge em https ou localhost.'); return; }
  const device = await navigator.bluetooth.requestDevice({ filters:[{ services:[BLE_SERVICE] }] });
  const server = await device.gatt.connect();
  const svc = await server.getPrimaryService(BLE_SERVICE);
  const tel = await svc.getCharacteristic(BLE_TEL);
  T.cmdChar = await svc.getCharacteristic(BLE_CMD);
  await tel.startNotifications();
  tel.addEventListener('characteristicvaluechanged', e => {
    const txt = new TextDecoder().decode(e.target.value);
    try { onMessage(JSON.parse(txt)); } catch { /* frame truncado: ignora */ }
  });
  device.addEventListener('gattserverdisconnected', () => { T.kind = null; wsSetStatus('DESCONECTADO','var(--a3)'); });
  T.kind = 'ble';
  if(ws) ws.close();
  wsSetStatus('BLE','var(--a1)');
  send({ t:'get' });
}

function send(obj){
  const txt = JSON.stringify(obj);
  if(T.kind === 'ble' && T.cmdChar){
    // escritas BLE em série: o GATT não aceita duas operações ao mesmo tempo
    T.queue = T.queue.then(() => T.cmdChar.writeValueWithoutResponse(new TextEncoder().encode(txt))).catch(console.warn);
  } else if(ws && ws.readyState === 1){
    ws.send(txt);
  }
}
```

2. Extrair o corpo de `ws.onmessage` para uma função `onMessage(d)` e fazer `ws.onmessage`
   chamar `onMessage(JSON.parse(ev.data))`. `onMessage` trata:
   - `info`: como hoje, mais aviso visível quando `d.cfgerr === 1`
     ("Configuração salva ausente ou inválida — usando valores de fábrica").
   - `tel`: como hoje, mas `pos` e `corr` já vêm em [-1, +1] (multiplicar por 3500 e por 1023
     ao alimentar os gráficos existentes) e `vL`/`vR` em duty 0–1023; mostrar `st` com os
     nomes IDLE/CALIBRANDO/PRONTO/ARMADO/CORRENDO/ERRO e marcar `lost`.
   - `sta`: bateria (`renderBattery(d.bat)` e `d.vb` em volts), voltas (`lap`, `laps`), erro
     (`err` → texto: 1 laço atrasado, 2 linha perdida, 3 bateria baixa, 4 calibração
     inválida), perfil ativo e indicador "NÃO SALVO" quando `dirty === 1`, `ovr` no card de
     loop.
   - `cfg`: preenche os campos do painel de configuração (passo 3) com os valores do grupo; o
     grupo `meta` atualiza perfil, "NÃO SALVO" e versão do firmware.
   - `ack`: se `ok === 0`, toast com a chave e o erro (`range` → "fora da faixa", `key` →
     "chave desconhecida", `running` → "não permitido com o carro andando", `dirty` →
     "salve ou descarte antes de trocar de perfil", `store` → "falha ao gravar").
   - `calres`: toast "Calibração OK" ou "Canais sem contraste: …".
   - `ecal`: toast com o lado, o resultado e o PPR.
3. Painel de configuração: nova aba **CONFIG** (botão em `.tab` e `<section class="panel"
   id="tab-config">`), gerada por JS a partir de uma tabela `CFG_KEYS` idêntica à `KEYS` do
   simulador (chave, grupo, mín, máx, se é inteiro, se é bloqueada em corrida). Cada chave vira
   um `<input type="number" min max step>` com rótulo e unidade (tabela da spec §4.3); ao
   confirmar (`change`), `send({t:'set',k,v:Number(value)})`. Agrupar por `hw/pid/vel/spd/misc`.
   No topo da aba: seletor de perfil (0 seguro, 1 treino, 2 corrida) que envia
   `{t:'prof',n}` (e, se a resposta for `dirty`, pergunta na própria página — sem `confirm()` —
   se quer descartar e reenvia com `f:1`); botões **Salvar** (`save`), **Recarregar** (`load`),
   **Fábrica** (`defaults`), **Calibrar sensores** (`scal`); indicador "NÃO SALVO".
4. Trocar todas as chamadas `wsSend(…)` por `send(…)`. Os sliders antigos de PID e velocidade
   passam a enviar `set` com as chaves novas (`kp`, `ki`, `kd`, `vmax`, `vmin`, `thr`) nas
   escalas novas: Kp 0–20, Ki 0–50, Kd 0–2 (passo 0,001), velocidades em % (0–100, enviadas
   como fração).
5. Botão **CONECTAR BLE** no cabeçalho, chamando `bleConnect()`; o WebSocket continua sendo
   tentado automaticamente para o simulador.

- [ ] **Step 5: Verificação manual com o simulador**

Run (em dois terminais): `cd docs/dashboard && python3 lfr_sim.py` e
`cd docs/dashboard && python3 -m http.server 8765`
Abrir `http://localhost:8765/lfr-cockpit-mock.html` e conferir, anotando o resultado no
commit: (a) a aba CONFIG aparece preenchida; (b) mudar Kp para 99 mostra "fora da faixa";
(c) mudar Kp para 1.5 mostra "NÃO SALVO"; (d) Salvar apaga o indicador; (e) trocar de perfil
com mudança pendente pede confirmação na página; (f) os gráficos andam.

Se houver o Playwright disponível no ambiente (`mcp__plugin_playwright_playwright__*`), fazer
essa verificação com ele e anexar uma captura em `docs/dashboard/captura-config.png`.

- [ ] **Step 6: Remover o dashboard obsoleto e commitar**

```bash
git rm -q docs/dashboard/lfr-cockpit.html
git add docs/dashboard/lfr-cockpit-mock.html docs/dashboard/lfr_sim.py \
        docs/dashboard/test_lfr_sim.py
git commit -m "feat(dashboard): Web Bluetooth transport, config panel and v4 protocol simulator

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 21: README e TUNING atualizados

**Files:**
- Modify: `README.md`, `docs/TUNING.md`, `docs/tuning_log.csv`

**Interfaces:**
- Consumes: protocolo (Tarefa 10), parâmetros (Tarefa 8), estados (Tarefa 14), ambientes de
  bancada (Tarefa 19).

- [ ] **Step 1: README**

Atualizar, sem tocar nas seções de hardware/chassi/BOM (são do WP3):
- Tabela de contagem de testes: rodar `pio test -e native 2>&1 | grep -E "test cases|PASSED"` e
  copiar as suítes e o total reais.
- Seção "Máquina de estados" com IDLE → CALIBRATING → READY → ARMED → RUNNING → ERROR e a
  regra do botão (toque curto / longo).
- Seção "Pipeline de controle" com os passos da spec §4.7 e a divisão de núcleos nova
  (control no core 1, comm no core 0).
- Seção "BLETuner" substituída por "Protocolo BLE v4": tabela de comandos (spec §4.5),
  frames `tel`/`sta`/`cfg`/`ack`/`calres`/`ecal`/`info` com um exemplo cada, e a lista dos
  comandos legados aceitos.
- Nova seção "Parâmetros em runtime" com a tabela da spec §4.3 (chave, descrição, unidade,
  fábrica, faixa, ao vivo).
- Nova seção "Testes de bancada" com os seis `pio run -e bench_* -t upload`.
- "Dashboard": conexão por BLE (botão CONECTAR BLE) ou simulador; remover a menção ao
  `lfr-cockpit.html`.

- [ ] **Step 2: TUNING.md**

Reescrever o procedimento para os ganhos normalizados:
1. Perfil 0 "seguro" (`vmax` 0,35), `mode` 0, `ki` 0, `kd` 0.
2. Subir `kp` de 0,5 em 0,5 até oscilar com amplitude constante → `Ku`; medir o período no
   gráfico de `pos` → `Tu` (s).
3. `kp = 0,6·Ku`, `kd = 0,075·Ku·Tu` (Ziegler–Nichols PD clássico), `ki` = 0.
4. Ajustar `dlp` (0,3–0,7) se o termo D ficar ruidoso.
5. Subir `vmax` de 0,05 em 0,05 por sessão; `vmin`/`thr` para as curvas.
6. Modo cascata só depois de `{"t":"cal"}` nos dois encoders: `mode` = 1, começar com
   `kpv` 0,002 e `kiv` 0,005.
7. Salvar com **Salvar** no dashboard ao fim de cada sessão boa; registrar no CSV.

Trocar o cabeçalho do `docs/tuning_log.csv` para
`data,perfil,mode,kp,ki,kd,dlp,vmax,vmin,thr,acc,kpv,kiv,melhor_volta_s,observacoes` e manter a
linha de 2026-05-14 convertida para a coluna nova com a observação "escala antiga (v3)".

- [ ] **Step 3: Verificar e commitar**

Run: `pio test -e native 2>&1 | tail -3 && pio run -e esp32dev 2>&1 | tail -3`
Expected: testes todos PASSED e `SUCCESS`.

```bash
git add README.md docs/TUNING.md docs/tuning_log.csv
git commit -m "docs: v4 protocol, runtime parameters, state machine and tuning procedure

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 4: Registrar o fim do WP1 no ruflo**

`mcp__claude-flow__memory_store` (`lfr-2026`, `wp1/status`): "WP1 concluído: <hash>; testes
<N>/<N>; esp32dev SUCCESS". `mcp__claude-flow__task_complete` na tarefa do WP1.
