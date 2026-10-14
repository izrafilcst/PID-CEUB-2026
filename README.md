# LFR ESP32 — Robô Seguidor de Linha Competitivo

Firmware completo em C++17 para robô seguidor de linha baseado em ESP32, desenvolvido com TDD estrito (Red → Green → Refactor). Projetado para competições brasileiras (RoboCore, IronCup, Salão de Robótica).

**271 testes nativos em 23 suítes • 0 warnings de `src/` • laço de controle a 500 Hz no core 1 •
toda a configuração ajustável por BLE e persistida na NVS (firmware 4.0.0)**

---

## Hardware

| Componente | Especificação |
|-----------|--------------|
| MCU | ESP32 DevKit V1 — dual-core 240 MHz |
| Sensores | Pololu QTR-8A — 8 canais via MCP3008 SPI (10-bit) |
| Motores | 2× N20 600 RPM com encoder Hall |
| Driver | TB6612FNG dual H-bridge |
| Bateria | LiPo 2S 7.4V 800 mAh 30C |
| Chassis | PETG impresso 3D, formato F1 |
| Regulador | MP1584 buck → 5V |

Custo estimado: **~R$ 383** mais o QTR-8A (US$ 12,95 na Pololu, importado). BOM completa em [`hardware/bom.csv`](hardware/bom.csv).

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

Saída esperada: `271 test cases: 271 succeeded`

---

## Estrutura do Projeto

```
├── src/
│   ├── config.h                  # Pinagem e constantes de hardware (sem ganhos)
│   ├── main.cpp                  # Só fiação: task control (core 1) e task comm (core 0)
│   ├── config/
│   │   ├── RobotConfig           # HardwareConfig + TuningProfile, faixas, 3 perfis de fábrica
│   │   └── ConfigStore           # Blobs na NVS com magic, versão e CRC32
│   ├── control/
│   │   ├── PIDController         # dt real, filtro no D, reset(medida), anti-windup condicional
│   │   ├── CascadeController     # Feedforward + PID por roda, em fração [-1, +1]
│   │   ├── Mixer                 # base ± correção com prioridade de direção
│   │   ├── SpeedProfile          # vmax/vmin/thr em fração, rampa de aceleração
│   │   └── VelocityProfile       # Predição de curvatura (não integrado na v2)
│   ├── sensors/
│   │   ├── SensorArray           # Leitura MCP3008 via SPI
│   │   ├── Calibration           # Min/max por canal, polaridade da linha, validação
│   │   ├── Encoder               # Quadratura 4× por ISR
│   │   ├── VelocityEstimator     # RPM filtrado, dt em µs, PPR por auto-calibração
│   │   ├── ButtonInput           # Debounce, toque curto e longo
│   │   └── BatteryMonitor        # Tensão filtrada, % por química, corte com retenção
│   ├── motors/
│   │   ├── MotorDriver           # TB6612FNG via LEDC (20 kHz, 10 bits)
│   │   └── OutputStage           # Compensação de tensão, inversão e troca de canais
│   ├── strategy/
│   │   ├── ControlLoop           # Um passo de controle inteiro, testável sem hardware
│   │   ├── RobotStateMachine     # IDLE/CALIBRATING/READY/ARMED/RUNNING/ERROR
│   │   ├── LineFollower          # Erro normalizado + PID de posição, linha perdida
│   │   └── LapTimer              # Cronômetro de volta (sem detecção de marcação na v2)
│   ├── comm/
│   │   ├── BLETuner              # Transporte NimBLE: fila de comandos e notificações
│   │   ├── CommandProtocol       # JSON → comandos tipados; frames ack/cfg/tel/sta/info
│   │   ├── CommandHandler        # Regras de set/save/load/defaults/prof
│   │   ├── CommandQueue, Mailbox # Passagem de dados entre núcleos
│   │   ├── Annunciator           # LED e buzzer sem bloquear
│   │   └── Logger                # (sem uso na v2)
│   └── storage/                  # IPersistentStore: NvsStore (ESP32) e InMemoryStore (testes)
│
├── test/                         # 23 suítes Unity, ambiente native
│
├── examples/                     # 6 testes de bancada, um ambiente bench_* cada
│
├── hardware/chassis/             # OpenSCAD paramétrico
│   ├── body_f1.scad              # Chassis 180×120mm formato F1
│   ├── sensor_bar.scad           # Barra 8 sensores + suporte ajustável
│   └── wheel_hub.scad            # Hub O-ring silicone 34mm, eixo D 3mm
│
├── docs/
│   ├── dashboard/
│   │   ├── lfr-cockpit-mock.html   # Dashboard (Web Bluetooth no robô ou WebSocket no simulador)
│   │   ├── lfr_sim.py              # Simulador Python — protocolo BLE v4 do firmware
│   │   ├── test_lfr_sim.py         # Testes do modelo de configuração do simulador
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
IDLE ──toque longo / scal──► CALIBRATING ──5 s, canais ok──► READY ──toque curto / start──► ARMED
  ▲                               │                           ▲  │                           │
  │                     5 s, canal sem faixa                  │  └──toque longo / scal──►    │ startMs
  │                               ▼                           │       CALIBRATING            ▼
  └──────── reset ────────── ERROR(n) ◄── linha perdida, bateria baixa, laço atrasado ── RUNNING
```

| De | Evento | Para | Condição / ação |
|----|--------|------|-----------------|
| (boot) | — | READY ou IDLE | READY se houver calibração válida na NVS |
| IDLE | toque longo ou `scal` | CALIBRATING | zera min/max |
| IDLE | toque curto | READY | só com calibração válida |
| CALIBRATING | 5 s | READY | todos os canais com max − min ≥ 200 → salva na NVS |
| CALIBRATING | 5 s | ERROR(4) | algum canal estreito; `calres` lista os canais |
| READY | toque curto ou `start` | ARMED | V_bat ≥ `vcut`; senão ERROR(3). Recusado com calibração de encoder aberta (E-4) |
| READY | toque longo ou `scal` | CALIBRATING | — |
| ARMED | `startMs` | RUNNING | PIDs alinhados à medida atual; rampa parte de 0 |
| ARMED / RUNNING | aperto do botão ou `stop` | READY | motores freiam; o aperto é consumido (E-3) |
| RUNNING | linha perdida por `lostMs` | ERROR(2) | motores param |
| RUNNING | V_bat < `vcut` por 1 s | ERROR(3) | motores param |
| RUNNING | 5 ciclos seguidos acima de 2000 µs | ERROR(1) | motores param |
| RUNNING | `mode` = 1 e uma roda com duty ≥ 60 % e \|RPM\| < 5 % de `rpmMax` por 200 ms | ERROR(5) | motores param (E-4) |
| ERROR | toque curto | READY ou IDLE | READY se a calibração for válida |
| qualquer | `reset` | IDLE | motores param |

Códigos de erro: 1 = laço atrasado, 2 = linha perdida, 3 = bateria baixa, 4 = calibração
inválida, 5 = encoder sem leitura no modo cascata (E-4).

**Botão (GPIO 0)**: debounce de 30 ms. **Toque curto** = soltar antes de 800 ms. **Toque longo**
= segurar 1,5 s (dispara ao completar o tempo, sem esperar soltar; ao soltar não gera toque
curto). Entre 800 ms e 1,5 s, nada. **Em ARMED e RUNNING o botão age no aperto** (E-3): o carro
para assim que o aperto passa do debounce, sem esperar soltar — é a parada de emergência. Esse
aperto é consumido: soltá-lo não arma de novo e segurá-lo não entra em CALIBRATING. Ligar o carro com o botão apertado entra no modo de gravação
do ESP32 (GPIO 0 é pino de *strapping*), não é um toque longo.

**Sinalização**: IDLE pisca 1 Hz · CALIBRATING pisca 5 Hz e bipa ao entrar e ao sair · READY
aceso e bipa ao entrar · ARMED pisca 10 Hz e bipa a cada 250 ms · RUNNING apagado · ERROR(n)
pisca n vezes e pausa, com n bipes ao entrar. O buzzer é tocado por LEDC a 2,5 kHz (canal 2).

### Pipeline de controle (task `control`, core 1, 500 Hz)

Um ciclo (`ControlLoop::step`, spec §4.7):

1. Lê os 8 canais do MCP3008 e normaliza com a polaridade `pol` (linha branca = leitura baixa).
2. Posição ponderada → erro normalizado em [−1, +1] (`sinv` inverte a ordem). Linha perdida:
   erro = ±1 para o lado onde ela foi vista por último, e o temporizador de `lostMs` corre.
3. PID de posição com o dt real do ciclo (s), D filtrado (`dlp`) → `u` em [−1, +1].
4. Base = `SpeedProfile(|erro|)` com rampa `acc`; com a linha perdida, |erro| = 1 (anda em
   `vmin` procurando a linha).
5. Modo PWM direto (`mode` 0): esquerdo = base + u, direito = base − u, com prioridade de
   direção quando satura. Modo cascata (`mode` 1): setpoints (base ± u) × `rpmMax` → por roda,
   feedforward sp/`rpmMax` + PID(sp − rpm medido).
6. `OutputStage`: inversões (`invL`/`invR`), troca de canais (`swap`), compensação × `vref`/V_bat
   (se `vcomp`), saturação em [−1, +1] e duty inteiro (× 1023).
7. Snapshot de telemetria, em todos os estados (sensores aparecem mesmo parado).

A bateria é lida a cada 100 ms com `analogReadMilliVolts()` (calibração do eFuse) × 147/47 ×
`vbk`, filtrada com constante de ~0,5 s.

### Núcleos e tasks

| Core | Task | Período | Prioridade | Faz |
|------|------|---------|-----------|-----|
| Core 1 | `control` | 2000 µs (`vTaskDelayUntil`) | `configMAX_PRIORITIES − 2` | sensores, PID, motores, estados, botão, LED/buzzer |
| Core 0 | `comm` | 10 ms | 2 | BLE (junto da pilha NimBLE), comandos, telemetria |

O `loop()` do Arduino se apaga. A configuração passa do core 0 para o core 1 por cópia
versionada (`Mailbox`); a telemetria volta do mesmo jeito; comandos de estado vão por fila
FreeRTOS. Só a task `control` toca nos motores, e fora de RUNNING ela os freia a cada ciclo.
Nenhuma escrita na NVS acontece em ARMED ou RUNNING.

---

## Módulos

### PIDController

Derivada calculada sobre a **medição** (não sobre o erro), com filtro passa-baixa de 1ª ordem.
O dt real (s) entra a cada chamada; `reset(medida)` alinha a memória da derivada e evita o
tranco na largada. Anti-windup por integração condicional + clamp da integral.

```cpp
PIDController pid(kp, ki, kd, -1.0f, 1.0f);
pid.setDerivativeFilter(0.5f);
pid.reset(erroAtual);
float u = pid.compute(setpoint, medida, dtS);
```

### VelocityProfile

> Não integrado ao firmware 4.0.0 (fica no repositório para uma próxima versão).

Mantém um buffer circular de 8 leituras de posição. Calcula a taxa de variação média (curvature score) para antecipar curvas e reduzir velocidade antes de entrar nelas — não apenas reagir ao erro atual.

```cpp
VelocityProfile vp(minSpeed, baseSpeed, maxSpeed, threshold);
vp.update(position, timestampMs);
int speed = vp.computeSpeed();  // considera erro E curvatura
```

### LapTimer

Timestamps injetados externamente (testável sem `millis()`). Registra cada cruzamento da linha de chegada e mantém o melhor tempo.

> Na v2 nada chama `notifyLineCrossing()`: a detecção das marcações laterais exige sensores
> novos (F-13/F-34 em `docs/REVISAO_FIRMWARE.md`). `lap` e `laps` do frame `sta` ficam vazios.

```cpp
LapTimer lt;
lt.start(millis());
lt.notifyLineCrossing(millis());  // a cada cruzamento detectado
uint32_t best = lt.getBestLapMs();
```

## Protocolo BLE v4

Serviço `0xABCC` (`0000abcc-0000-1000-8000-00805f9b34fb`), anunciado pelo nome `LFR-RACER-01`
e pelo UUID do serviço. MTU preferido 185; **toda notificação cabe em 180 bytes**.

| Característica | UUID | Direção | Conteúdo |
|----------------|------|---------|----------|
| Telemetria | `0xABCD` | ESP32 → app (Notify) | JSON: `info`, `tel`, `sta`, `cfg`, `ack`, `calres`, `ecal` |
| Comando | `0xABCE` | app → ESP32 (Write / Write sem resposta) | JSON: um comando por escrita |

### Comandos

| Comando | Efeito | Resposta |
|---------|--------|----------|
| `{"t":"set","k":"kp","v":1.2}` | Valida e aplica ao vivo (não persiste) | `{"t":"ack","k":"kp","ok":1,"v":1.2}` ou `{"t":"ack","k":"kp","ok":0,"err":"range"}` |
| `{"t":"get"}` | Pede a identificação e a configuração inteira | `info`, depois `cfg` dos grupos `hw`, `pid`, `vel`, `spd`, `misc` e `meta` |
| `{"t":"save"}` | Grava `HardwareConfig` + perfil ativo na NVS | `ack` com `k:"save"` |
| `{"t":"load"}` | Descarta mudanças não salvas e relê o perfil ativo | `ack` + sequência `cfg` |
| `{"t":"defaults"}` | Valores de fábrica ao vivo (não persiste) | `ack` + sequência `cfg` |
| `{"t":"prof","n":2}` | Troca o perfil ativo; com mudança não salva recusa com `err:"dirty"`, a menos que venha `"f":1` | `ack` + sequência `cfg` |
| `{"t":"scal"}` | Calibração dos sensores (mesmo efeito do toque longo) | `ack`; ao fim, `calres` |
| `{"t":"start"}` / `{"t":"stop"}` / `{"t":"reset"}` | Eventos da máquina de estados | `ack` |
| `{"t":"cal","op":"start","side":"L"}` / `{"t":"cal","op":"finish","side":"L","rot":10}` | Auto-calibração do PPR do encoder (girar a roda à mão) | `ack`; no `finish`, `ecal` |

Erros do `ack`: `range` (fora da faixa ou valor não finito), `key` (chave desconhecida),
`running` (não permitido em ARMED/RUNNING), `dirty` (salve ou descarte antes de trocar de
perfil), `store` (falha ao gravar), `parse` (JSON inválido; `k:"?"`), `locked` (modo
competição), `cal` (`start` com calibração de encoder aberta: termine com `cal finish` ou
`reset`).

Regras: chaves de `HardwareConfig` e `mode`, `save`/`load`/`defaults`/`prof` e `cal` são
recusados em ARMED/RUNNING (`err:"running"`). Gravar na flash suspende o cache dos dois
núcleos e atrasaria o laço de controle.

**Modo competição** (`lock` = 1): em ARMED ou RUNNING, **todo** comando recebido pelo BLE é
recusado com `{"t":"ack","k":<comando ou chave>,"ok":0,"err":"locked"}` — inclusive `stop`,
`set` e `get` — e nada muda no robô. A telemetria continua saindo. Fora desses estados nada
muda (`start` em READY continua aceito). O botão continua parando o carro.

**Comandos legados** (continuam aceitos e viram `set`, com a mesma validação):
`{"t":"pid","kp":…,"ki":…,"kd":…}` → `kp`/`ki`/`kd` nas unidades novas ·
`{"t":"pidv",…}` → `kpv`/`kiv`/`kdv` · `{"t":"spd","min":…,"max":…,"thrs":…}` → `vmin`/`vmax`
(valor ÷ 255) e `thr`; o campo `base` é ignorado · `{"t":"rpm","max":…}` → `rpmMax` ·
`cal`, `start`, `stop`, `reset` mantêm o significado.

### Frames enviados pelo robô

`info` — quando o cliente ativa as notificações da `0xABCD` (junto com a sequência `cfg`) e em
resposta a cada `get`; `cfgerr` = 1 quando a configuração salva estava ausente ou inválida
(o carro está nos valores de fábrica); `stk` = menor folga de pilha das tasks `control` e
`comm` desde o boot, em bytes:
```json
{"t":"info","name":"LFR-RACER-01","fw":"4.0.0","mode":"RUN","cfgerr":0,"stk":[3120,4870]}
```

`tel` — 30 Hz: erro normalizado, saída do PID de posição, duty por lado, tempo de execução do
ciclo (µs), RPM medido e pedido, sensores em %, estado (0 IDLE … 5 ERROR) e linha perdida:
```json
{"t":"tel","pos":0.125,"corr":-0.062,"vL":402,"vR":312,"dt":612,"rpmL":0,"rpmR":0,"spL":0,"spR":0,"s":[0,2,10,85,96,20,1,0],"st":4,"lost":0}
```

`sta` — 2 Hz: bateria (% e V), voltas, código de erro, perfil ativo, mudança não salva e maior
tempo de execução do laço na janela (µs):
```json
{"t":"sta","bat":78,"vb":7.86,"lap":0.00,"laps":[],"err":0,"prof":1,"dirty":0,"ovr":742}
```

`cfg` — um frame por grupo, mais o `meta`:
```json
{"t":"cfg","g":"pid","kp":1,"ki":0,"kd":0.03,"dlp":0.5}
{"t":"cfg","g":"meta","prof":1,"dirty":0,"fw":"4.0.0"}
```

`ack` — resposta a cada comando (um por chave nos comandos com vários itens):
```json
{"t":"ack","k":"vmin","ok":0,"err":"range"}
```

`calres` — fim da calibração dos sensores (`ch` = canais sem contraste):
```json
{"t":"calres","ok":0,"ch":[3,4]}
```

`ecal` — fim da calibração de um encoder:
```json
{"t":"ecal","side":"R","ok":1,"ppr":612.5}
```

---

## Parâmetros em runtime

Duas estruturas (spec §4.3). `HardwareConfig` descreve o robô físico e é global;
`TuningProfile` descreve um jeito de correr e existe em 3 perfis. Erro e saídas dos PIDs são
normalizados: no modo PWM direto, a fração 1,0 = duty 1023; no modo cascata, 1,0 = `rpmMax`.

**`HardwareConfig`** (global; só com o carro parado):

| Chave | Descrição | Unid. | Fábrica | Faixa |
|-------|-----------|-------|---------|-------|
| `invL` / `invR` | Inverte o sentido do motor esquerdo / direito | — | 0 | {0, 1} |
| `swap` | Troca os canais A/B do TB6612 entre esquerdo e direito | — | 0 | {0, 1} |
| `einvL` / `einvR` | Inverte o sentido de contagem do encoder | — | 0 | {0, 1} |
| `pol` | Polaridade da linha: 0 = linha preta, 1 = linha branca | — | 1 | {0, 1} |
| `sinv` | Inverte a ordem dos sensores (barra montada espelhada) | — | 0 | {0, 1} |
| `chem` | Química da bateria (curva de %): 0 = LiPo, 1 = Li-ion | — | 1 | {0, 1} |
| `lock` | Modo competição: 1 = recusa todo comando BLE em ARMED/RUNNING | — | 0 | {0, 1} |
| `vcut` | Tensão de corte (abaixo dela por 1 s: não dá start / para) | V | 6,4 | 5,6–7,6 |
| `vbk` | Fator de correção da leitura de tensão (ajuste com multímetro) | — | 1,000 | 0,800–1,200 |
| `pprL` / `pprR` | Pulsos ×4 por volta do eixo de saída (também pela auto-calibração `cal`) | — | 600 | 1–10000 |

`pprL`/`pprR` = 600 é provisório (12 CPR × 50:1): a auto-calibração `cal` é obrigatória antes de
usar `mode` = 1.

`lock` atende a regra 5.9 do regulamento RoboCore: quem usa comunicação remota tem que
demonstrar aos juízes que ela não pode parar o robô nem alterar parâmetros durante a tentativa.
Como toda chave de hardware, `lock` só muda com o carro parado — ninguém desliga o modo
competição durante a corrida. O botão do robô continua parando o carro.

**`TuningProfile`** (por perfil):

| Chave | Descrição | Unid. | Fábrica | Faixa | Ao vivo em RUNNING |
|-------|-----------|-------|---------|-------|--------------------|
| `mode` | 0 = PWM direto, 1 = cascata com encoders | — | 0 | {0, 1} | não |
| `kp` | Proporcional da malha de posição | fração / erro | 1,0 | 0–20 | sim |
| `ki` | Integral da malha de posição | 1/s | 0,0 | 0–50 | sim |
| `kd` | Derivativo da malha de posição | s | 0,03 | 0–2 | sim |
| `dlp` | Filtro passa-baixa do termo D (1 = sem filtro) | — | 0,5 | 0,05–1 | sim |
| `kpv` | Proporcional da malha de velocidade | fração / RPM | 0,002 | 0–0,05 | sim |
| `kiv` | Integral da malha de velocidade | fração / (RPM·s) | 0,005 | 0–1 | sim |
| `kdv` | Derivativo da malha de velocidade | fração·s / RPM | 0,0 | 0–0,01 | sim |
| `vmax` | Velocidade-base em reta (erro zero) | fração | 0,35 / 0,50 / 0,70 | 0–1 | sim |
| `vmin` | Velocidade-base com erro ≥ `thr` | fração | 0,25 | 0–1, ≤ `vmax` | sim |
| `thr` | Erro normalizado a partir do qual a base é `vmin` | — | 0,6 | 0,05–1 | sim |
| `acc` | Rampa máxima de subida da base (descida livre) | fração/s | 2,0 | 0,1–50 | sim |
| `rpmMax` | RPM que corresponde à fração 1,0 no modo cascata | RPM | 700 | 100–2000 | sim |
| `vref` | Tensão de referência da compensação | V | 7,4 | 5,0–8,4 | sim |
| `vcomp` | Liga a compensação de tensão | — | 1 | {0, 1} | sim |
| `lostMs` | Tempo com a linha perdida até parar (ERROR 2) | ms | 300 | 50–5000 | sim |
| `startMs` | Atraso entre o start e a partida (estado ARMED) | ms | 1000 | 0–5000 | sim |

Perfis de fábrica — iguais, exceto `vmax`: **0 "seguro"** = 0,35 · **1 "treino"** = 0,50 ·
**2 "corrida"** = 0,70. O perfil ativo ao ligar é o último salvo; na primeira vez, o 0.

Persistência (NVS, namespace `lfr`): `hw`, `prof0`..`prof2`, `prof_act` e `cal` (min/max dos
sensores), cada um com magic, versão, tamanho e CRC32; os PPR continuam em `left_ppr`/
`right_ppr`. Blob ausente ou inválido → valores de fábrica daquele bloco, `cfgerr` = 1 no
`info` e 3 bipes curtos ao ligar.

---

## Testes de bancada

Cada exemplo de `examples/` tem um ambiente próprio que substitui o `main.cpp`:

```bash
pio run -e bench_motor -t upload         # motores: A frente/ré, B frente/ré, ambos
pio run -e bench_encoder -t upload       # contagem dos dois encoders no Serial
pio run -e bench_sensor_raw -t upload    # leitura bruta dos 8 sensores (CSV no Serial)
pio run -e bench_sensor_cal -t upload    # calibração e posição normalizada
pio run -e bench_velocity_cal -t upload  # auto-calibração do PPR e RPM ao vivo
pio run -e bench_differential -t upload  # reto, curvas e giros com mixSteering + OutputStage
```

Abrir o monitor com `pio device monitor -b 115200`. Versões fixas: `espressif32@6.13.0`
(arduino-esp32 2.0.17) e `h2zero/NimBLE-Arduino@2.5.1`.

---

## Dashboard de Telemetria

Abrir `docs/dashboard/lfr-cockpit-mock.html` em Chrome/Edge (Web Bluetooth requer HTTPS ou localhost).

**Funcionalidades:**
- Conecta ao robô por BLE (botão **CONECTAR BLE**, Web Bluetooth) ou ao simulador Python via
  WebSocket — o mesmo JSON do protocolo v4 nos dois
- Aba **CONFIG** com todos os parâmetros de runtime agrupados (faixa e unidade), seletor de
  perfil, **Salvar**, **Recarregar**, **Fábrica**, **Calibrar sensores** e indicador
  **NÃO SALVO**; chaves de hardware travadas com o carro andando; interruptor **Modo
  competição** (`lock`) e selo visível quando ele está ligado
- Estado do robô, código de erro e tensão da bateria na tela; aviso quando a configuração salva
  foi descartada (`cfgerr`)
- Gráfico rolling 5 s: posição, correção, velocidades L/R
- Sliders PID (Kp, Ki, Kd) e velocidade (`vmax`, `vmin` em %, `thr`): enviam `set` ao soltar
- Barra de 8 sensores com brilho proporcional ao valor lido
- Cards: loop µs (alerta > 2 ms), erro, PWM médio, melhor volta
- LapTimer: histórico de últimas 5 voltas, destaque da melhor
- Bateria com indicador visual e alertas críticos
- Modo claro/escuro; layout responsivo (mobile swipe / desktop grid)
- Export de log CSV
- Animações de entrada via Motion.dev v11

**Simulador Python** (sem hardware; responde a `set`/`get`/`save`/`load`/`defaults`/`prof`/
`scal` como o firmware):
```bash
cd docs/dashboard
pip install websockets        # só o servidor precisa
python3 -m unittest test_lfr_sim
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

## Pacote dimensional dos componentes

Tudo que é preciso para modelar o robô em CAD sem consultar mais nada.

| Onde | O quê |
|------|-------|
| `hardware/components/fichas/` | 10 fichas dimensionais em Markdown, uma por componente comprado |
| `hardware/drawings/comp/` | 10 pranchas SVG cotadas (A4 paisagem, normas ABNT) |
| `hardware/drawings/assy/` | A1 — layout funcional e balanço de CG · A2 — vista explodida |
| `docs/DIVERGENCIAS.md` | Registro das inconsistências entre BOM, firmware e modelos 3D |

**Toda cota carrega proveniência**: ● `OFICIAL` (datasheet do fabricante, com
URL) · ○ `TIPICO` (módulo genérico, valor de consenso) · △ `MEDIR` (varia por
lote — medir a peça em mãos). Nenhum valor foi estimado em silêncio.

As fichas e as pranchas são **artefatos gerados** — não edite os `.md` nem os
`.svg`. A fonte única é `hardware/components/specs.py`; altere lá e regenere:

```bash
cd tools
python3 gen_fichas.py              # 10 fichas Markdown
python3 gen_component_drawings.py  # 10 pranchas de componente
python3 gen_assembly_drawings.py   # pranchas A1 e A2
python3 verify_drawings.py         # rasteriza em build/drawings/ para inspeção
python3 layout.py                  # imprime o balanço de massas e o CG
```

Os testes das ferramentas de desenho rodam com `unittest` da stdlib:

```bash
cd tools && python3 -m unittest discover -s tests
```

> As massas ainda são estimativas na maioria dos itens (D7 em
> `docs/DIVERGENCIAS.md`). O CG da prancha A1 é indicativo até que as peças
> sejam pesadas.

---

## Calibração e Tuning

### Calibração de sensores

1. Posicionar o robô com a barra de sensores sobre a linha
2. Toque longo no botão (1,5 s) ou **Calibrar sensores** no dashboard → CALIBRATING
3. Varrer a linha devagar durante a janela de 5 s
4. Fim automático: READY e calibração salva na NVS, ou ERROR(4) com os canais sem contraste

### Tuning PID — Método Ziegler-Nichols

Ganhos normalizados (erro e saídas em [−1, +1]):

1. Perfil 0 "seguro", `mode` 0, `ki` = 0, `kd` = 0
2. Subir `kp` de 0,5 em 0,5 até o robô oscilar com amplitude constante → **Ku**; período no
   gráfico de `pos` → **Tu** (s)
3. Aplicar: `kp = 0,6 · Ku`, `kd = 0,075 · Ku · Tu`, `ki = 0`; `dlp` 0,3–0,7 se o D ficar ruidoso
4. Subir `vmax` de 0,05 em 0,05 por sessão; `vmin`/`thr` para as curvas
5. **Salvar** ao fim de cada sessão boa

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
| test_annunciator | 7 | Annunciator |
| test_battery | 7 | BatteryMonitor |
| test_ble_tuner | 5 | BLETuner (transporte, inscrição nas notificações) |
| test_button | 9 | ButtonInput |
| test_calibration | 18 | Calibration |
| test_cascade | 12 | CascadeController, Mixer |
| test_command_handler | 27 | CommandHandler, Mailbox, modo competição, info, NVS, cal de encoder |
| test_command_protocol | 25 | CommandProtocol, CommandQueue |
| test_config_store | 14 | ConfigStore, InMemoryStore |
| test_control_loop | 17 | ControlLoop (cenários F-01, F-03, F-05, F-06, F-08, F-14; falha de encoder) |
| test_encoder | 9 | Encoder |
| test_lap_timer | 4 | LapTimer |
| test_line_follower | 11 | LineFollower |
| test_nvs_arbiter | 7 | NvsArbiter (NVS × carro armado) |
| test_output_stage | 8 | OutputStage |
| test_periodic_timer | 4 | PeriodicTimer (tel a 30 Hz) |
| test_pid | 15 | PIDController |
| test_robot_config | 14 | RobotConfig |
| test_smoke | 1 | Framework |
| test_speed_profile | 10 | SpeedProfile |
| test_state_machine | 25 | RobotStateMachine (botão no aperto, ERROR(ENC)) |
| test_velocity_estimator | 18 | VelocityEstimator |
| test_velocity_profile | 4 | VelocityProfile |
| **Total** | **271** | |

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

Ordem de desenvolvimento da v4 (bottom-up, plano `docs/superpowers/plans/2026-10-13-lfr-v2-wp1-firmware.md`):
1. PIDController → Calibration → LineFollower → SpeedProfile → VelocityEstimator → CascadeController
2. RobotConfig → ConfigStore → CommandProtocol → ButtonInput → BatteryMonitor → OutputStage
3. RobotStateMachine → Annunciator → ControlLoop → CommandHandler/BLETuner
4. main.cpp — só a fiação das tasks; depois os ambientes `bench_*` e o dashboard

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
