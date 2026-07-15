## O que muda

Corrige o bug de timing do loop de controle e adiciona tratamento de cruzamento, plumbando o **dt real (variável, em µs/s)** por toda a cadeia de controle sem travar o loop em taxa fixa.

- **dt real em µs** no `VelocityEstimator`, que passa a **acumular** pulsos + tempo por uma janela mínima (`VELOCITY_MIN_WINDOW_US=4000`) antes de estimar RPM — elimina o descarte silencioso de pulsos do encoder em ticks sub-milissegundo.
- **dt real por chamada** para as duas camadas de PID (posição externo + RPM interno por motor); defaults preservam os testes existentes, só o `main.cpp` passa o dt medido.
- **Duas corridas cross-core fechadas:** seção crítica (`lineFollowerMux`) estendida sobre `cascade.compute` (retune vs cascade), e comandos de calibração restritos ao estado `!= RUNNING`.
- **`beep()` não-bloqueante** (`beepStart` + `serviceBuzzer`, wrap-safe) — sem busy-wait no caminho de tempo real.
- **Tratamento de cruzamento:** novo `Calibration::isCrossing()` (≥6 sensores ≥700 normalizado) + `isLineLost()` passa a **dirigir comportamento** — em cruzamento segue reto (correção 0), em linha perdida mantém o heading (hold da última correção).

## Verificação

- `pio test -e native` → **91/91** (era 82; +9 testes dirigidos em velocity/cascade/line_follower/calibration), sem warnings sob `-Wall -Wextra -std=gnu++17`.
- `pio run -e esp32dev` → **SUCCESS**, RAM 12.0% (39.300 B), Flash 46.4% (607.545 B), sem warnings novos em `main.cpp`.

## Como foi feito

TDD por tarefa (RED→GREEN→commit), 4 tarefas com revisão independente por tarefa + revisão final de branch inteira. Decisão registrada em **ADR-001**; plano em `docs/superpowers/plans/2026-07-12-timing-crossing-fixes.md`.

| Commit | Tarefa |
|---|---|
| `1d101c4` | VelocityEstimator: dt µs + acumulação por janela |
| `926b7db` | dt real p/ PID externo + cascade interno |
| `3044eec` | cruzamento + hold em linha perdida (isLineLost dirige) |
| `2f7be91` | integração main.cpp (dt real, corridas, buzzer, telemetria) |
| `369bcac` | docs: ADR-001 + plano |

## Verificação de bancada recomendada (não bloqueia merge)

- `Kd` externo (12) com dt curto/variável → se tremer em reta, reduzir `Kd` via BLE ou subir `VELOCITY_MIN_WINDOW_US`.
- Thresholds de cruzamento (`CROSSING_MIN_ACTIVE=6`, `CROSSING_ACTIVE_LEVEL=700`) vs largura real da linha.
- Duração da seção crítica (agora engloba cascade) no analisador lógico.
- Resíduo TOCTOU do gate de calibração (path do botão) — documentado no ADR-001.

🤖 Generated with [Claude Code](https://claude.com/claude-code)
