# ADR-001: dt de controle variável real (µs) plumbado a estimador e PIDs + tratamento de cruzamento

- **Status**: accepted
- **Date**: 2026-07-12
- **Deciders**: Rafael Costa (dono do projeto)
- **Tags**: control-loop, timing, concurrency, sensing

## Context

O loop de controle do `main` (Core 0) roda **livre** (não travado em taxa fixa),
mas o cálculo de `dt` truncava o intervalo real para milissegundos inteiros
(`(loopStart - g_prevLoopStart) / 1000`). Em loops sub-milissegundo isso zera o
`dt`, e o `VelocityEstimator` descartava silenciosamente pulsos do encoder entre
ticks curtos — corrompendo a estimativa de RPM e, por consequência, a malha de
cascade. Além disso:

- As duas camadas de PID (posição externo + RPM interno por motor) usavam `dt`
  fixo implícito, ignorando o intervalo real.
- Duas corridas de concorrência entre Core 0 e Core 1: o retune BLE
  (`setInnerGains`/`setMaxRpm`/`g_baseRpm`) tocava o cascade fora da seção
  crítica; comandos de calibração de encoder mexiam em `_pprX4/_calibrating`
  enquanto Core 0 chamava `update()`.
- O `beep()` era bloqueante (busy-wait) no caminho de tempo real.
- Cruzamentos (linha perpendicular) não tinham tratamento; `isLineLost()`
  existia mas só alimentava telemetria, sem dirigir comportamento.

## Decision

**Decisões travadas com o dono do projeto (2026-07-12):**

1. **dt variável real em µs** (NÃO taxa fixa): o loop permanece livre, mas o
   intervalo real medido por `micros()` é plumbado por toda a cadeia —
   `VelocityEstimator.update(uint32_t dtUs)` e ambas as camadas de PID via
   `dtSec` por chamada.
2. O `VelocityEstimator` passa a **acumular** pulsos e tempo real por uma
   **janela mínima** (`VELOCITY_MIN_WINDOW_US`, default 4 ms) antes de estimar
   RPM — elimina a perda de pulsos e a baixa resolução de 28 PPR em alta taxa.
3. **Escopo travado = timing + corridas + cruzamento.** As duas corridas são
   fechadas estendendo `lineFollowerMux` sobre `cascade.compute()` e restringindo
   comandos de calibração ao estado não-`RUNNING`. `beep()` vira não-bloqueante.
4. **`isLineLost()` deve DIRIGIR comportamento**, não só telemetria: em linha
   perdida a última correção é mantida (recuperação de heading); em cruzamento a
   correção é forçada a 0 (segue reto pela interseção), via novo sinal
   `Calibration::isCrossing()`.

Fonte da verdade da implementação:
[docs/superpowers/plans/2026-07-12-timing-crossing-fixes.md](../superpowers/plans/2026-07-12-timing-crossing-fixes.md).

## Consequences

### Positive
- Estimativa de RPM correta sob loop livre; sem descarte silencioso de pulsos.
- Termo derivativo dos PIDs fisicamente correto (usa o `dt` real da amostra).
- Corrida retune-vs-cascade **eliminada** (seção crítica estendida sobre
  `cascade.compute`); corrida de calibração cross-core **fortemente reduzida**
  por gate de estado `!= RUNNING`.
- Robô atravessa cruzamentos reto e recupera heading ao perder a linha.

### Negative
- `Kd` externo (12) com `dt` curto e variável pode amplificar ruído em reta →
  requer verificação de bancada (ajustar `Kd` via BLE ou subir
  `VELOCITY_MIN_WINDOW_US`).
- Thresholds de cruzamento (`CROSSING_MIN_ACTIVE=6`, `CROSSING_ACTIVE_LEVEL=700`)
  precisam de validação contra a pista real.
- O gate de calibração é check-then-act (TOCTOU): resta uma janela teórica se um
  start pelo **botão** (Core 0) coincidir com um `finishCalibration()` em voo
  (Core 1). Mitigado na prática (calibração é operação de bancada com robô
  parado) e reconhecido no plano; considerar comentário/serialização se a
  bancada expuser o caso.
- A seção crítica agora engloba `cascade.compute` (2 PIDs internos), alongando
  modestamente a janela de interrupções desabilitadas no Core 0 — item de
  verificação de timing em bancada, não defeito de correção.

### Neutral
- Defaults nas novas assinaturas (`dtSec=0.01f`/`0.002f`) preservam o
  comportamento das suítes de teste existentes; só o `main.cpp` passa o dt real.
- Surface BLE dos flags `lineLost`/`crossing` fica como follow-up (já no
  `TelemetrySnapshot`).

## Links
- Plan: docs/superpowers/plans/2026-07-12-timing-crossing-fixes.md
- CLAUDE.md — Pipeline de Controle (Core 0, < 2 ms)

<!-- AgentDB graph registration (mcp__claude-flow__agentdb_hierarchical-store,
     memory_search adr-patterns): NOT RUN — claude-flow MCP not connected in this
     session. Re-run `/ruflo-adr:adr index` once the MCP server is wired to
     backfill the causal graph. -->
