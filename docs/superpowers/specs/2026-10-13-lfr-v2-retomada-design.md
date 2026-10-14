# Design — LFR v2: retomada do carro

**Data**: 2026-10-13
**Autor**: Rafael Costa (com Claude)
**Status**: Aprovado em brainstorming, aguardando revisão do spec
**Branch**: `feat/lfr-v2-retomada`, criada a partir de `docs/componentes-e-monografia-abnt`
(commit `720a6ee`, que já contém o pacote dimensional e o `specs.py`)

---

## 1. Objetivo

Deixar o carro pronto para voltar à pista com três entregas e uma avaliação independente:

1. **Firmware revisado e corrigido** (WP1) — o carro opera, troca de parâmetros e de modo de
   controle **sem recompilar**: tudo é ajustável por BLE, validado e persistido na NVS.
2. **Base mecânica redesenhada** (WP2) — 2 rodas motrizes impressas com O-ring, patins PETG
   sob a barra de sensores, suporte 2×18650, encaixes e tolerâncias de PETG sobre as cotas
   reais das peças. Modelo paramétrico em build123d, exportado em STL e STEP.
3. **Manuais por papel** (WP3) — montador, operador e manutenção, em Markdown no repositório
   e num manual HTML interativo publicado.
4. **Avaliação** — um agente avaliador, diferente do executor, revisa cada pacote contra este
   documento e emite recomendações e feedback.

A execução é orquestrada pelo ruflo (`claude-flow` MCP), com agentes especializados.

---

## 2. Decisões travadas

| # | Decisão | Valor |
|---|---------|-------|
| DEC-1 | Bateria | **2× 18650 em suporte plástico comprado pronto** (2S: 8,4 V cheia, 7,2 V nominal). Substitui a LiPo 2S 800 mAh |
| DEC-2 | Apoio | **2 rodas motrizes + 2 patins PETG sob a barra de sensores**. O ball caster sai do projeto |
| DEC-3 | Rodas | **Cubo PETG impresso + O-ring de silicone** |
| DEC-4 | Motor | **N20 600 RPM @ 6 V com encoder** (o par atual) |
| DEC-5 | CAD | **build123d (Python)** → STL + STEP. Os `.scad` atuais são removidos (ficam no histórico do git) |
| DEC-6 | Firmware | **Revisão + correção com TDD + configuração em runtime** |
| DEC-7 | Núcleos | **Laço de controle em task própria no core 1 a 500 Hz; BLE e telemetria no core 0**, junto da pilha Bluetooth. Inverte o que o CLAUDE.md diz hoje |
| DEC-8 | Persistência | Ajuste ao vivo **não** persiste sozinho; só o comando `save` grava |
| DEC-9 | Compensação de tensão | PWM escalado por V_ref/V_bat, V_ref padrão **7,4 V** |
| DEC-10 | Manuais | Markdown em `docs/manual/` (fonte) + manual HTML publicado (cópia no repo para uso offline) |
| DEC-11 | Orquestração | Swarm ruflo hierárquico; WP1 ∥ WP2 em worktrees isolados; WP3 depois; avaliador independente por pacote |
| DEC-12 | Impressão | PETG, bico 0,4 mm, mesa ≥ 200 × 200 mm |
| DEC-13 | Escala do controle | Erro de posição e saídas dos PIDs **normalizados em [−1, +1]**: os ganhos não dependem da resolução do PWM, do número de sensores nem do modo de controle |

### Justificativa de DEC-13

Hoje o PID externo recebe posição em [−3500, +3500] e devolve PWM em [−1023, +1023]. Com
`KP_DEFAULT 3.0`, o PID satura com erro de 341 unidades — um décimo do curso. As velocidades
`BASE/MAX/MIN_SPEED` estão em escala 0–255, mas o PWM é de 10 bits: no modo PWM direto o carro
nunca passaria de 22 % do duty. Normalizar resolve as duas coisas e permite trocar entre PWM
direto e cascata sem reajustar os ganhos externos. Como não há sintonia de pista registrada
(`docs/tuning_log.csv` só tem a linha de base de 2026-05-14), não se perde histórico.

---

## 3. Situação de partida

- **82/82 testes nativos passam** (`pio test -e native`, 13 suítes). Nenhum teste cobre o
  `main.cpp`, que é onde a maior parte dos defeitos abaixo mora.
- Achados já confirmados na leitura do código. O relatório de revisão (§4.1) parte desta lista
  e a completa:

| ID | Sev. | Onde | Defeito | Efeito |
|----|------|------|---------|--------|
| F-01 | CRIT | `src/main.cpp:227,353` → `src/strategy/LineFollower.cpp:38` | `lineFollower.reset()` chama `_cal.reset()` na transição READY→RUNNING | A calibração recém-feita é apagada no start; o carro corre sem calibração |
| F-02 | CRIT | `src/sensors/Calibration.cpp` (`normalize`) | Depois de `reset()`, `_max − _min` = `INT_MIN − INT_MAX`: overflow de inteiro com sinal (UB) | Normalização indefinida até a próxima calibração |
| F-03 | CRIT (a confirmar) | `src/sensors/Calibration.cpp` (`weightedPosition`) | O QTR-8A dá tensão **menor** sob maior refletância; a pista tem linha **branca** sobre preto. A média ponderada favorece o fundo preto | Posição invertida e atenuada. O revisor confirma no material da Pololu |
| F-04 | CRIT | `src/main.cpp:132,326,351,459` | Botão sem debounce nem detecção de borda; o mesmo aperto que dá start é lido como stop no ciclo seguinte | O carro alterna RUNNING/READY enquanto o botão está apertado |
| F-05 | HIGH | `src/strategy/LineFollower.cpp:23`, `src/main.cpp:405` | No modo cascata a base vem de `g_baseRpm` fixo; o `SpeedProfile` é calculado e descartado | Não reduz a velocidade nas curvas |
| F-06 | HIGH | `src/main.cpp:366`, `src/sensors/VelocityEstimator.cpp:68` | `dtMs` inteiro: com o laço abaixo de 1 ms, `dtMs == 0` e o estimador descarta a janela | O RPM congela; a malha interna fica cega |
| F-07 | HIGH | `src/main.cpp:39` | PID externo construído com `dt = 0,01 s` fixo, mas o laço não tem período fixo (`LOOP_PERIOD_US` não é usado) | Termos I e D em escala errada |
| F-08 | HIGH | `src/control/CascadeController.cpp` | Malha interna só com PID (Kp 0,8, Ki 0), sem feedforward | Erro de regime grande: o motor não atinge o RPM pedido |
| F-09 | HIGH | `src/main.cpp:314` + `loop()` do Arduino | O `loop()` roda no core 1 (padrão do Arduino-ESP32) e a task de comunicação também foi fixada no core 1 | Controle e BLE disputam o mesmo núcleo, ao contrário do que o código comenta |
| F-10 | HIGH | `src/main.cpp:116-121` | Leitura de bateria calibrada para LiPo (6,4–8,4 V), `raw/4095 × 3,3` sem a calibração de ADC do eFuse | Percentual errado para Li-ion; erro de até ~10 % na tensão |
| F-11 | MED | `src/main.cpp:125-129` | `beep()` faz espera ativa (até 500 ms), chamada nos dois núcleos | Trava a comunicação; atrasa o start |
| F-12 | MED | `src/control/PIDController.cpp` (`reset`) | `reset()` zera `_prevMeasurement`; o primeiro cálculo depois do reset gera pico de derivada se o carro não estiver centrado | Tranco na largada |
| F-13 | MED | `src/strategy/LapTimer.cpp` | `notifyLineCrossing()` nunca é chamado | Voltas nunca registradas |
| F-14 | MED | `src/main.cpp` (RUNNING) | Linha perdida mantém a última posição, que pode ser pequena; não há timeout | O carro sai reto da pista e segue andando |
| F-15 | MED | `src/control/SpeedProfile.cpp:11` | `baseSpeed` é guardado mas nunca usado; erro zero devolve `maxSpeed` | Parâmetro morto que confunde a sintonia |
| F-16 | LOW | geral | Nenhum parâmetro ajustado por BLE é persistido (só o PPR dos encoders) | Toda sintonia se perde ao desligar |
| F-17 | HIGH | `src/config.h` (`ENCODER_DEFAULT_PPR_X4 28`) | O padrão conta pulsos por volta do eixo do **motor** (7 PPR × 4) e ignora a redução (~50:1 no N20 de 600 RPM) | Sem a auto-calibração, o RPM medido no eixo de saída sai ~50× alto e a cascata freia o carro |

---

## 4. WP1 — Firmware

### 4.1 Relatório de revisão

`docs/REVISAO_FIRMWARE.md`, escrito antes de qualquer correção:

- Cobre todo `src/`, `test/`, `platformio.ini`, `docs/dashboard/` e `examples/`.
- Cada achado: ID (continua a numeração F-xx), severidade, arquivo:linha, cenário de falha
  concreto, correção proposta e o requisito FW-xx que a resolve (ou "fora do escopo" com razão).
- Severidade: **CRIT** impede o carro de correr ou arrisca dano; **HIGH** degrada desempenho ou
  operação sem contorno simples; **MED**; **LOW**; **INFO**.
- Itens obrigatórios de verificação, além de F-01..F-17: pinos de *strapping* (GPIO 0, 2, 4,
  12, 15) usados no pinout; ISRs do encoder em IRAM; tamanho de pilha das tasks; uso de `float`
  em ISR; tamanho das mensagens BLE frente ao MTU; acesso concorrente a dados entre núcleos.
- Seção "Regulamento": o que o regulamento RoboCore vigente exige sobre parada após a chegada e
  marcadores laterais, com a fonte. A implementação disso **não** faz parte da v2 (§9).

### 4.2 Arquitetura alvo

A lógica sai do `main.cpp` para classes puras, testáveis no ambiente `native`. O `main.cpp` fica
só com a fiação (instâncias, tasks e chamadas aos drivers).

| Módulo (novo ou alterado) | Responsabilidade | Testável em `native` |
|---------------------------|------------------|----------------------|
| `config/RobotConfig` (novo) | `HardwareConfig` + `TuningProfile`, valores de fábrica, validação por chave, `set(chave, valor)` | Sim |
| `config/ConfigStore` (novo) | Serializa com versão + CRC32, 3 perfis + perfil ativo, calibração dos sensores | Sim (com `InMemoryStore`) |
| `comm/CommandProtocol` (novo) | Interpreta JSON recebido → comandos tipados; monta respostas `ack`/`cfg` | Sim |
| `comm/BLETuner` (alterado) | Só transporte NimBLE: recebe bytes, entrega ao `CommandProtocol`, notifica | Não (hardware) |
| `strategy/RobotStateMachine` (novo) | Estados, eventos e transições de §4.6 | Sim |
| `strategy/ControlLoop` (novo) | Um passo de controle: sensores + RPM + V_bat + config + dt → saídas + snapshot de telemetria | Sim |
| `sensors/ButtonInput` (novo) | Debounce e eventos curto/longo | Sim |
| `sensors/BatteryMonitor` (novo) | mV → V filtrado, %, detecção de bateria baixa com tempo de retenção | Sim |
| `motors/OutputStage` (novo) | Inversão/troca de motores, compensação de tensão, saturação → duty inteiro | Sim |
| `comm/Annunciator` (novo) | LED e buzzer por padrão temporal, sem bloquear | Sim |
| `control/PIDController` (alterado) | dt real em segundos, filtro no termo D, `reset(medida)`, anti-windup condicional | Sim |
| `control/CascadeController` (alterado) | Feedforward + PID por motor, em fração [−1, +1] | Sim |
| `control/SpeedProfile` (alterado) | `vmin`/`vmax`/`thr` em fração; rampa de aceleração `acc` | Sim |
| `sensors/Calibration` (alterado) | Polaridade da linha, sem overflow, validação de faixa por canal, carga/salvamento | Sim |
| `sensors/VelocityEstimator` (alterado) | `update(dtUs)` com dt em microssegundos | Sim |
| `strategy/LineFollower` (alterado) | Não reseta mais a calibração; linha perdida satura para o último lado | Sim |
| `motors/DifferentialDrive` (removido) | Substituído pelo `ControlLoop` + `OutputStage`; a mistura base ± correção passa a preservar a autoridade de direção quando satura | — |
| `comm/CommandQueue` (novo) | Fila de comandos recebidos pelo BLE, entre o callback NimBLE e a task `comm` | Sim |
| `storage/IPersistentStore` (alterado) | Ganha `getBlob`/`putBlob` | Sim (fake) |

O `VelocityProfile` (predição de curvatura) **não** é integrado nesta versão; o relatório
registra que ele está sem uso.

### 4.3 Parâmetros em runtime

Duas estruturas. `HardwareConfig` descreve o robô físico e é global. `TuningProfile` descreve
um jeito de correr e existe em 3 perfis. A chave BLE é única entre as duas.

**`HardwareConfig`** (global):

| Chave | Descrição | Unid. | Fábrica | Faixa |
|-------|-----------|-------|---------|-------|
| `invL` / `invR` | Inverte o sentido do motor esquerdo / direito | — | 0 | {0, 1} |
| `swap` | Troca os canais A/B do TB6612 entre esquerdo e direito | — | 0 | {0, 1} |
| `einvL` / `einvR` | Inverte o sentido de contagem do encoder | — | 0 | {0, 1} |
| `pol` | Polaridade da linha: 0 = linha preta, 1 = linha branca | — | 1 | {0, 1} |
| `sinv` | Inverte a ordem dos sensores (barra montada espelhada) | — | 0 | {0, 1} |
| `chem` | Química da bateria (curva de %): 0 = LiPo, 1 = Li-ion | — | 1 | {0, 1} |
| `lock` | Modo competição: 1 = recusa todo comando BLE em ARMED/RUNNING (§4.5, E-1) | — | 0 | {0, 1} |
| `vcut` | Tensão de corte (abaixo dela por 1 s: não dá start / para) | V | 6,4 | 5,6–7,6 |
| `vbk` | Fator de correção da leitura de tensão (ajuste com multímetro) | — | 1,000 | 0,800–1,200 |
| `pprL` / `pprR` | Pulsos ×4 por volta do **eixo de saída** (também pela auto-calibração `cal`) | — | 600 | 1–10000 |

`pprL`/`pprR` = 600 corresponde a um encoder de 12 CPR no eixo do motor × redução de 50:1. É
**provisório**: o encoder genérico pode ser de outro tipo. A auto-calibração (`cal`) é
obrigatória antes de usar `mode` = 1, e o manual do operador diz isso.

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
| `vmax` | Velocidade-base em reta (erro zero) | fração | ver abaixo | 0–1 | sim |
| `vmin` | Velocidade-base com erro ≥ `thr` | fração | 0,25 | 0–1, ≤ `vmax` | sim |
| `thr` | Erro normalizado a partir do qual a base é `vmin` | — | 0,6 | 0,05–1 | sim |
| `acc` | Rampa máxima de subida da base (descida livre) | fração/s | 2,0 | 0,1–50 | sim |
| `rpmMax` | RPM que corresponde à fração 1,0 no modo cascata | RPM | 700 | 100–2000 | sim |
| `vref` | Tensão de referência da compensação | V | 7,4 | 5,0–8,4 | sim |
| `vcomp` | Liga a compensação de tensão | — | 1 | {0, 1} | sim |
| `lostMs` | Tempo com a linha perdida até parar (ERROR LINE) | ms | 300 | 50–5000 | sim |
| `startMs` | Atraso entre o start e a partida (estado ARMED) | ms | 1000 | 0–5000 | sim |

Perfis de fábrica — iguais, exceto `vmax`: **0 "seguro"** = 0,35 · **1 "treino"** = 0,50 ·
**2 "corrida"** = 0,70. O perfil ativo ao ligar é o último salvo; na primeira vez, o 0.

Os valores de fábrica são **ponto de partida para a sintonia em pista**, não valores sintonizados.
`rpmMax` = 700 vem de 600 RPM @ 6 V escalados para V_ref 7,4 V (≈ 740 RPM sem carga), com margem.

Fração de escala: no modo PWM direto, 1,0 = `PWM_MAX` (1023); no modo cascata, 1,0 = `rpmMax`.

### 4.4 Persistência (NVS)

- Namespace `lfr`. Chaves: `hw` (blob `HardwareConfig`), `prof0`/`prof1`/`prof2` (blobs
  `TuningProfile`), `prof_act` (u8), `cal` (blob min/max por canal), e as chaves de PPR que já
  existem (`left_ppr`, `right_ppr`), lidas e escritas pelo `ConfigStore` para manter
  compatibilidade com a calibração salva.
- Formato de cada blob: `magic u16` · `versão u16` · `tamanho u16` · conteúdo · `CRC32`.
- Blob ausente, com magic/versão/tamanho diferentes ou CRC errado → valores de fábrica
  daquele bloco + aviso (`"cfgerr":1` no `info` e 3 bipes curtos ao ligar). O carro continua
  utilizável.
- **Nenhuma escrita na NVS durante RUNNING ou ARMED**: escrever na flash suspende o cache dos
  dois núcleos e atrasaria o laço de controle. `save` nesses estados responde `err:"running"`.
- A calibração dos sensores é salva automaticamente ao fim de uma calibração válida (§4.6).

### 4.5 Protocolo BLE

Mesmo serviço e características de hoje (`0xABCC`/`0xABCD`/`0xABCE`). Comandos novos:

| Comando | Efeito | Resposta |
|---------|--------|----------|
| `{"t":"set","k":"kp","v":1.2}` | Valida e aplica ao vivo (não persiste) | `{"t":"ack","k":"kp","ok":1,"v":1.2}` ou `{"t":"ack","k":"kp","ok":0,"err":"range"\|"key"\|"running"}` |
| `{"t":"get"}` | Pede a configuração inteira | Uma notificação por grupo: `{"t":"cfg","g":"hw",…}`, `{"t":"cfg","g":"pid",…}`, `{"t":"cfg","g":"vel",…}`, `{"t":"cfg","g":"spd",…}`, `{"t":"cfg","g":"misc",…}`, `{"t":"cfg","g":"meta","prof":1,"dirty":0,"fw":"4.0.0"}` |
| `{"t":"save"}` | Grava `HardwareConfig` + perfil ativo | `ack` com `k:"save"` |
| `{"t":"load"}` | Descarta mudanças não salvas e relê o perfil ativo | `ack` + sequência `cfg` |
| `{"t":"defaults"}` | Carrega valores de fábrica ao vivo (não persiste) | `ack` + sequência `cfg` |
| `{"t":"prof","n":2}` | Troca o perfil ativo. Recusa com `err:"dirty"` se houver mudança não salva, a menos que venha `"f":1` | `ack` + sequência `cfg` |
| `{"t":"scal"}` | Inicia a calibração dos sensores (mesmo efeito do toque longo) | `ack`; ao fim, `{"t":"calres","ok":1}` ou `{"t":"calres","ok":0,"ch":[3,4]}` |

- **Retrocompatibilidade**: `pid`, `pidv`, `spd`, `rpm`, `cal`, `start`, `stop`, `reset`
  continuam aceitos e viram `set` das chaves equivalentes, com a mesma validação.
  `pid`/`pidv` passam a ser interpretados nas unidades novas (§4.3). `spd` converte de 0–255
  para fração (`v/255`); seu campo `base` é ignorado (F-15). `rpm` mapeia `max` → `rpmMax`.
- Chaves de `HardwareConfig` e `mode` só são aceitas fora de RUNNING/ARMED (`err:"running"`).
- **Modo competição** (`lock` = 1, E-1): em ARMED ou RUNNING, **todo** comando recebido pelo
  BLE é recusado com `{"t":"ack","k":<comando ou chave>,"ok":0,"err":"locked"}`, inclusive
  `stop`, `set` e `get`. A telemetria continua saindo. Fora desses estados, nada muda (`start`
  em READY continua aceito). O botão continua parando o carro. Atende a regra RoboCore 5.9.
- Cada notificação cabe em 180 bytes. O relatório (§4.1) verifica o MTU negociado.
- A telemetria atual já passa de 180 bytes, então é dividida em dois frames:
  - `tel` a 30 Hz: `pos` (erro normalizado), `corr` (saída do PID externo em fração), `vL`,
    `vR` (duty), `dt` (µs), `rpmL`, `rpmR`, `spL`, `spR` (inteiros), `s` (8 sensores em %),
    `st` (estado), `lost` (0/1);
  - `sta` a 2 Hz: `bat` (%), `vb` (V), `lap`, `laps`, `err` (código), `prof`, `dirty` (0/1),
    `ovr` (maior tempo de execução do laço na janela, µs).
  `bat`, `lap` e `laps` saem do `tel` e passam para o `sta`.
- `info` passa a enviar `"fw":"4.0.0"` e `"cfgerr"`, e também `"stk":[<control>,<comm>]`:
  a menor folga de pilha de cada task desde o boot, em bytes (E-2).

### 4.6 Estados, botão e sinalização

Estados: `IDLE`, `CALIBRATING`, `READY`, `ARMED`, `RUNNING`, `ERROR(código)`.

| De | Evento | Para | Condição / ação |
|----|--------|------|-----------------|
| (boot) | — | READY | Calibração válida na NVS |
| (boot) | — | IDLE | Sem calibração válida |
| IDLE | toque longo ou `scal` | CALIBRATING | Zera min/max |
| IDLE | toque curto | READY | Só se houver calibração válida |
| CALIBRATING | 5 s decorridos | READY | Todos os canais com max − min ≥ 200 contagens → salva na NVS |
| CALIBRATING | 5 s decorridos | ERROR(CAL) | Algum canal com faixa < 200; informa os canais |
| READY | toque curto ou `start` | ARMED | V_bat ≥ `vcut`; senão ERROR(BAT). Recusado enquanto a calibração de encoder estiver aberta (E-4) |
| READY | toque longo ou `scal` | CALIBRATING | — |
| ARMED | `startMs` decorrido | RUNNING | Reseta PIDs com a medida atual, rampa parte de 0 |
| ARMED | aperto do botão (E-3) ou `stop` | READY | Cancela |
| RUNNING | aperto do botão (E-3) ou `stop` | READY | Motores param (freio) |
| RUNNING | linha perdida por mais de `lostMs` | ERROR(LINE) | Motores param |
| RUNNING | V_bat < `vcut` por 1 s | ERROR(BAT) | Motores param |
| RUNNING | 5 ciclos seguidos acima de 2000 µs | ERROR(OVR) | Motores param |
| RUNNING | `mode` = 1 e uma roda com duty alto e RPM ≈ 0 por 200 ms (E-4) | ERROR(ENC) | Motores param |
| ERROR | toque curto | READY ou IDLE | READY se a calibração for válida |
| qualquer | `reset` | IDLE | Motores param |

Códigos de erro: 1 = OVR (laço atrasado), 2 = LINE (linha perdida), 3 = BAT (bateria baixa),
4 = CAL (calibração inválida), 5 = ENC (encoder sem leitura no modo cascata, E-4).

Botão (GPIO 0): debounce de 30 ms; **toque curto** = soltar antes de 800 ms; **toque longo** =
segurar 1500 ms (o evento dispara ao atingir o tempo, sem esperar soltar).
**Em ARMED e RUNNING o botão age no aperto (E-3):** o carro para assim que o aperto passa do
debounce, sem esperar soltar. Esse aperto consome o gesto: soltá-lo não gera toque curto e
segurá-lo não gera toque longo (senão o carro, já em READY, voltaria a ARMED ou entraria em
CALIBRATING).

Sinalização (`Annunciator`, sem bloqueio):

| Estado | LED (GPIO 2) | Buzzer (GPIO 22) |
|--------|--------------|------------------|
| IDLE | pisca 1 Hz | — |
| CALIBRATING | pisca 5 Hz | bip ao entrar e ao sair |
| READY | aceso | bip ao entrar |
| ARMED | pisca 10 Hz | bip curto a cada 250 ms |
| RUNNING | apagado | — |
| ERROR(n) | n piscadas, pausa, repete | n bipes ao entrar |

O buzzer é acionado por LEDC num terceiro canal (tom de ~2,5 kHz), o que funciona tanto com
buzzer passivo (o da BOM) quanto com ativo.

### 4.7 Laço de controle

- Task `control` fixa no **core 1**, prioridade alta, período de 2000 µs com `vTaskDelayUntil`.
  `loop()` do Arduino fica vazio (ou se deleta). Task `comm` fixa no **core 0**, período de 10 ms.
- A configuração vai do core 0 para o core 1 por cópia: o `comm` escreve uma cópia pendente
  e incrementa um contador de versão sob spinlock; o `control` copia a estrutura inteira no
  início do ciclo quando a versão muda. Nenhuma seção crítica longa.
- Um ciclo (`ControlLoop::step`):
  1. Lê os 8 canais do MCP3008, normaliza com a polaridade `pol`.
  2. Posição ponderada → erro normalizado em [−1, +1]. Linha perdida (soma < limiar): erro =
     ±1 com o sinal do último erro válido, e o temporizador de `lostMs` corre.
  3. PID externo com dt real (s) → `u` em [−1, +1].
  4. Base: `SpeedProfile(|erro|)` com rampa `acc`. Com a linha perdida, o perfil recebe
     |erro| = 1: o carro procura a linha em `vmin`, mesmo sem histórico de lado.
  5. Modo PWM direto: `uL = base + u`, `uR = base − u`.
     Modo cascata: setpoints `(base ± u) × rpmMax` → cada motor: `ff = sp/rpmMax` + PID(sp − rpm)
     → `uL`/`uR`. Anti-windup condicional quando saturado.
  6. `OutputStage`: inversões e troca, compensação `× vref/vbat` (se `vcomp`), saturação em
     [−1, +1], × `PWM_MAX`.
  7. Atualiza o snapshot de telemetria.
- A leitura de V_bat usa `analogReadMilliVolts()` (calibração do eFuse) × razão do divisor
  (147/47) × `vbk`, filtrada (constante de tempo ~0,5 s), feita a cada 100 ms.

### 4.8 Dashboard e simulador

`docs/dashboard/lfr-cockpit-mock.html` (o dashboard documentado no README) e
`docs/dashboard/lfr_sim.py` passam a suportar §4.5:

- painel com todos os parâmetros de §4.3, agrupados, com faixa e unidade;
- botões Salvar, Recarregar e Fábrica, seletor de perfil e indicador de "não salvo";
- estado, código de erro e V_bat na tela;
- o simulador responde a `set`/`get`/`save`/`load`/`defaults`/`prof`/`scal` como o firmware.

O relatório (§4.1) registra o papel de `docs/dashboard/lfr-cockpit.html` (o outro arquivo) e
se ele deve ser removido ou atualizado.

### 4.9 Testes e critérios de aceite do WP1

- TDD: cada correção começa por um teste que falha. Nomes `test_<módulo>_<comportamento>`.
- Suítes novas: `test_robot_config`, `test_config_store`, `test_command_protocol`,
  `test_state_machine`, `test_control_loop`, `test_button`, `test_battery`, `test_output_stage`,
  `test_annunciator`. Suítes existentes ajustadas às interfaces novas.
- `test_control_loop` cobre, sem hardware, os cenários de F-01, F-03, F-05, F-06, F-08 e F-14.
- `pio test -e native`: tudo passando.
- `pio run -e esp32dev`: compila, **sem warnings vindos de `src/`** com `-Wall -Wextra`.
- `examples/*.cpp` atualizados às interfaces novas, cada um com um ambiente `bench_*` no
  `platformio.ini` (`pio run -e bench_motor`, etc.), todos compilando.
- README e `docs/TUNING.md` atualizados com o protocolo e os parâmetros novos.

---

## 5. WP2 — Mecânica

### 5.1 Estrutura de arquivos

```
hardware/cad/
├── requirements.txt        # build123d com versão fixada
├── medidas.toml            # medidas tiradas das peças em mãos + offsets do cupom (editável)
├── params.py               # fonte única: specs.py + peças novas + medidas.toml + tolerâncias
├── parts/
│   ├── base.py             # base principal
│   ├── motor_clamp.py      # abraçadeira do N20 (×2)
│   ├── battery_cradle.py   # berço do suporte 2×18650 (feição da base, com abas)
│   ├── sensor_bar.py       # barra do QTR-8A
│   ├── skid.py             # patim substituível (×2) + calço de ajuste
│   ├── wheel.py            # roda com canal de O-ring (×2)
│   ├── electronics.py      # suportes do perfboard e da chave
│   └── tolerance_coupon.py # cupom de teste de tolerância
├── assembly.py             # montagem, interferências, CG e carga nos patins
├── build.py                # `python build.py` → gera out/
├── out/                    # STL por peça, STEP por peça, assembly.step, relatório de montagem
└── tests/                  # unittest
```

- O venv fica em `hardware/cad/.venv/` (ignorado pelo git). A versão do build123d é a última
  estável publicada há pelo menos duas semanas na data da instalação, fixada no
  `requirements.txt`.
- `out/` é **commitado**: quem só vai imprimir não precisa de Python.
- `python build.py` regenera tudo e imprime o relatório de montagem (massas, CG, carga nos
  patins, folgas mínimas). É o único comando que o montador precisa depois de editar o
  `medidas.toml`.

### 5.2 Peças compradas e cotas

As cotas vêm de `hardware/components/specs.py`, com a mesma proveniência
(● `OFICIAL` · ○ `TIPICO` · △ `MEDIR`). Entram três componentes novos no `specs.py`:

| # | Componente | Cotas principais | Proveniência |
|---|-----------|------------------|--------------|
| 11 | Célula 18650 | ⌀18,0–18,6 × 65,0–65,2 mm (topo plano); ~46 g | TIPICO (norma de mercado); **MEDIR** as células compradas |
| 12 | Suporte 2×18650 (lado a lado) | ~77 × 41 × 20 mm, com fios; ~12 g | TIPICO; **MEDIR** — varia por fabricante |
| 13 | O-ring de silicone | escolhido no WP2 (§5.4) | OFICIAL pela norma do tamanho escolhido |

LiPo (06), roda de silicone ⌀34 mm comprada (08) e ball caster (09) ficam marcados como
**obsoletos** no `specs.py`. A ficha 08 passa a descrever a roda impressa. As fichas são
regeneradas por `tools/gen_fichas.py`. As pranchas de componente (`hardware/drawings/comp/`)
**não** ganham versões para 11–13, e as pranchas A1/A2 ficam registradas como superadas pelo
modelo CAD (`docs/DIVERGENCIAS.md`). `tools/tests/` continua passando (o teste que fixa 10
componentes passa a refletir 13).

Toda cota △ `MEDIR` usada pelo CAD tem uma entrada correspondente em `medidas.toml`, que
sobrescreve o valor típico quando preenchida.

### 5.3 Tolerâncias para PETG

Valores iniciais. O cupom de §5.7 corrige cada um para a impressora do montador via
`medidas.toml`.

| Encaixe | Folga / ajuste (no diâmetro ou por lado) |
|---------|------------------------------------------|
| Deslizante (berço da bateria, barra de sensores em trilho) | +0,30 mm por lado |
| Alojamento justo de PCB (QTR-8A, perfboard) | +0,20 mm por lado |
| Furo de passagem M2 / M3 | ⌀2,4 / ⌀3,4 mm |
| Furo para autoatarraxar M2 | ⌀1,7 mm, profundidade ≥ 6 mm |
| Furo para inserto térmico M3 (OD 4,0 × 5,7 mm) | ⌀4,0 mm, profundidade 6,5 mm |
| Furo em D do eixo do N20 (⌀3,0 com achatado 2,5) | ⌀3,05 / 2,55 mm (interferência leve) |
| Corpo da caixa de redução do N20 na abraçadeira (12 × 10 mm) | +0,10 mm por lado, aperto pelo parafuso |
| Compensação geral de furos pequenos (< ⌀6) | +0,10 mm no diâmetro, ajustável |

Regras de impressão verificadas: parede mínima 1,2 mm; cada peça tem orientação de impressão
declarada com face plana na mesa; balanços acima de 45° só como pontes ≤ 10 mm; cada peça cabe
em 200 × 200 mm.

### 5.4 Geometria e layout

Origem no centro do eixo motriz, no chão. X para a frente, Y para a esquerda, Z para cima.

- **Rodas**: diâmetro externo (com o O-ring) entre 38 e 44 mm. O diâmetro final sai do
  cálculo de velocidade e torque, registrado no relatório de montagem, e de um O-ring de
  tamanho padrão (métrico ou AS568) com estiramento de 2–5 % no fundo do canal e profundidade
  de canal de 70–75 % da seção. Referência: ⌀40 mm ≈ 1,2 m/s e ⌀44 mm ≈ 1,35 m/s a ~590 RPM
  sob carga. O cubo engata os 10 mm do eixo do N20 sem conflito de profundidade (fecha D9),
  com furo em D (§5.3) e furo radial para parafuso M2 com porca cativa como trava opcional.
- **Motores**: os dois N20 coaxiais, de costas um para o outro, presos por abraçadeiras sobre a
  caixa de redução; a face da caixa encosta numa parede da base, que absorve o empuxo axial.
  Folga para o conector JST-SH traseiro do encoder e para os fios.
- **Bitola**: resultado do pacote motores + rodas (fecha D10). Envelope total ≤ 250 × 250 ×
  200 mm.
- **Sensores**: QTR-8A na frente, em berço de PCB única (fecha D2). Face dos sensores a
  **3,0 mm do chão** quando os patins tocam a manta. Lookahead de **85 mm** (fecha D3): com
  70 mm a barra de sensores colide com o berço da bateria e as abraçadeiras dos motores, que
  ficam logo à frente do eixo. O valor final sai do modelo e vai para o `config.h` no merge.
- **Bateria**: berço do suporte 2×18650 integrado à base, baixo e logo à frente do eixo. As
  células saem **por cima, sem desmontar nada**: nenhuma peça cobre o suporte. Quatro abas
  nos cantos, sobre as paredes de ponta do suporte, impedem que ele salte em impacto; o
  suporte sai puxando as abas, sem ferramenta.
- **Eletrônica**: perfboard 50 × 70 mm em espaçadores M3 com insertos térmicos, sem cobrir o
  suporte; USB do ESP32 acessível com o carro montado; chave liga/desliga e porta-fusível
  acessíveis pela lateral.
- **Vão livre**: base a ≥ 4 mm do chão em todos os pontos, exceto rodas e patins.
- **Massa e CG**: massa de cada peça impressa = volume × 1,27 g/cm³ × fator de preenchimento
  (0,6 por padrão, ajustável). As massas compradas vêm do `specs.py` (ainda estimadas, D7).
  **Meta: 10–20 % do peso sobre os patins** (CG 7–14 mm à frente do eixo com lookahead de
  70 mm) e CG longitudinal ≥ 0,3 × altura do CG à frente do eixo (sem empinar até ~3 m/s²).
  O lookahead pode ficar entre 70 e 100 mm para fechar essa meta ou evitar colisões; o
  `LOOKAHEAD_MM` do `config.h` é atualizado no merge.

### 5.5 Patins

- Dois patins nas pontas da barra de sensores, fora do campo de visão dos 8 sensores.
- Contato em calota com raio ≥ 3 mm; bordo de ataque com rampa de 30° para não enganchar em
  emendas da manta ou na borda da linha.
- Cada patim é uma peça própria, fixa por um parafuso M2 — troca em menos de 2 minutos.
- Calços impressos de 0,5 mm permitem levar a altura do sensor de 3,0 a até 6,0 mm (D8) sem
  reimprimir a barra. Cada passo usa um calço sob cada patim **e** um sob a língua da barra
  (onde ela apoia na base), para o chassi continuar nivelado.

### 5.6 Fixação

- Insertos térmicos M3 em tudo que se desmonta na manutenção (barra de sensores e
  perfboard). O berço da bateria é parte da base e não leva parafuso.
- M2 autoatarraxante nas abraçadeiras dos motores e nos patins.
- A BOM lista quantidade e comprimento de cada parafuso, inserto e porca.

### 5.7 Cupom de tolerância

Peça pequena (≤ 15 min de impressão) que o montador imprime antes de tudo. Contém: série de
furos ⌀2,8–3,4 mm em passos de 0,1; furos em D com três variações; furos de inserto M3 com três
variações; um trilho deslizante com três folgas; um alojamento de PCB. O manual de montagem
explica como ler o cupom e o que preencher no `medidas.toml`.

### 5.8 Testes e critérios de aceite do WP2

`hardware/cad/tests/` (unittest), todos passando:

- nenhuma interferência (volume de interseção = 0) entre peças impressas e envelopes dos
  componentes; folga mínima reportada por par;
- envelope montado ≤ 250 × 250 × 200 mm;
- face do sensor a 3,0 ± 0,1 mm do plano do chão;
- base a ≥ 4 mm do chão;
- carga nos patins entre 10 e 20 % com as massas atuais;
- paredes ≥ 1,2 mm; cada peça ≤ 200 × 200 mm na orientação de impressão; verificação
  aproximada de balanço (faces da malha voltadas para baixo acima da mesa, área > limiar);
- `build.py` gera um STL e um STEP por peça, mais `assembly.step`;
- `python3 -m unittest discover -s tests` em `tools/` continua passando;
- `docs/DIVERGENCIAS.md`: D2, D3, D4, D9 e D10 marcados como **Resolvido** pelo modelo novo;
  D4 resolvido por remoção do caster.

---

## 6. WP3 — Documentação

Começa depois que WP1 e WP2 estão integrados na branch: os manuais citam os parâmetros, os
comandos e as peças finais.

| Arquivo | Público | Conteúdo mínimo |
|---------|---------|-----------------|
| `docs/manual/MONTAGEM.md` | Montador | BOM atualizada com quantidades; ferramentas; impressão (perfil PETG, orientação de cada peça, cupom e `medidas.toml`); montagem passo a passo na ordem certa; fiação para 2S Li-ion com fusível e chave; checagens antes de energizar (continuidade, ajuste do MP1584 para 5,0 V **antes** de ligar o ESP32, polaridade das células); primeiro upload de firmware; teste de bancada com `examples/` |
| `docs/manual/OPERACAO.md` | Operador | Ligar e desligar; estados, LED e bipes; calibração; dashboard e perfis; salvar e recarregar; roteiro de sintonia em pista; checklist de box para competição; carga e segurança das células Li-ion; o que fazer em cada código de erro |
| `docs/manual/MANUTENCAO.md` | Manutenção | Inspeções periódicas (patins, O-rings, parafusos, conectores, encoders, células); troca de peças; árvore de diagnóstico sintoma → causa → ação; reflash; aferição da leitura de bateria (`vbk`); versões de firmware; peças de reposição |

Atualizações:

- `docs/WIRING.md`: alimentação nova (suporte 2×18650 → fusível → chave → VM do TB6612 e
  MP1584), divisor de bateria conferido para 8,4 V, buzzer por LEDC, proteção contra inversão
  de polaridade recomendada (P-MOSFET).
- `hardware/bom.csv`: sai LiPo, carregador B3, ball caster e rodas compradas; entram suporte
  2×18650, 2 células 18650 *high-drain* (classe 20 A), carregador de 18650, O-rings (2 + 2 de
  reserva), insertos M3, fusível e porta-fusível. Preços sem fonte ficam marcados "estimado".
- `README.md`, `docs/TUNING.md`, `docs/DIVERGENCIAS.md` e `CLAUDE.md` (stack, arquitetura,
  núcleos, parâmetros) refletem a v2.

**Manual HTML**: `docs/manual/manual-lfr.html`, um arquivo único que abre offline no navegador
(para a área de competição sem internet) e é publicado como página. Conteúdo: uma aba para cada
papel; checklists marcáveis; diagramas SVG (fiação, vista superior com cotas principais,
máquina de estados); tabela de parâmetros de §4.3; árvore de diagnóstico. Os Markdown são a
fonte; o HTML não traz informação que não esteja neles.

Critérios de aceite do WP3: todo procedimento cita parâmetros, comandos, peças e arquivos que
existem na branch; links internos válidos; o HTML abre sem rede; nenhuma informação conflita
com o firmware ou o CAD finais.

---

## 7. Avaliação

Cada pacote recebe um avaliador **diferente do agente que executou**, que trabalha sobre o
commit entregue e roda os comandos ele mesmo, sem confiar no relato do executor.

Relatório em `docs/avaliacao/2026-10-13-<WP>.md`:

1. Escopo avaliado e commit.
2. **Matriz de conformidade**: cada requisito deste documento (§4.9, §5.8, §6) → atendido /
   parcial / não atendido, com evidência (arquivo:linha ou comando e saída).
3. **Achados**: ID, severidade (escala de §4.1), descrição, evidência, recomendação.
4. **Feedback ao executor**: o que funcionou e o que ajustar.
5. **Atividades recomendadas**, priorizadas, com responsável (montador, operador, manutenção,
   desenvolvimento).
6. **Veredito**: APROVADO · APROVADO COM RESSALVAS · REPROVADO.

Ciclo de correção: achados CRIT e HIGH voltam ao executor; o avaliador reavalia só esses itens.
No máximo duas rodadas; o que sobrar depois disso vai para o usuário decidir.

Ao final, um **relatório consolidado** (`docs/avaliacao/2026-10-13-consolidado.md`) junta os
três vereditos, os achados abertos e a lista única de próximas atividades — incluindo as que
estão fora do escopo (§9).

---

## 8. Orquestração (ruflo)

- `swarm_init`: topologia hierárquica, até 8 agentes, estratégia especializada.
- Memória compartilhada no namespace `lfr-2026`: `spec/decisoes` (este documento resumido),
  `wp1/achados`, `wp2/params`, `wpN/status`, `avaliacao/<WP>`.
- Cada papel é registrado com `agent_spawn` e cada pacote vira uma tarefa (`task_create`). Os
  agentes usam `hooks_pre-task`/`hooks_post-task`, publicam resultados na memória e encerram a
  tarefa com `task_complete`. `hooks_route` sugere o modelo de cada agente.
- A execução de cada agente é feita pelo Claude Code (ferramenta `Agent`), com o tipo de agente
  correspondente:

| Papel | Pacote | Tipo de agente | Isolamento | Entrega |
|-------|--------|----------------|------------|---------|
| Coordenador | todos | sessão principal | branch `feat/lfr-v2-retomada` | triagem, merges, publicação do HTML |
| Revisor de firmware | WP1 | `code-analyzer` | worktree WP1 | `docs/REVISAO_FIRMWARE.md` |
| Implementador TDD | WP1 | `sparc-coder` | worktree WP1 | `src/`, `test/`, `examples/`, dashboard |
| Projetista CAD | WP2 | `system-architect` | worktree WP2 | `hardware/cad/`, `specs.py`, fichas |
| Redator técnico | WP3 | `general-purpose` | branch principal, após os merges | `docs/manual/`, docs atualizados |
| Avaliador WP1 | WP1 | `analyst` | leitura do commit do WP1 | `docs/avaliacao/…-WP1.md` |
| Avaliador WP2 (validador geométrico) | WP2 | `production-validator` | leitura do commit do WP2 | `docs/avaliacao/…-WP2.md` |
| Avaliador WP3 + consolidado | WP3 | `analyst` | branch principal | `docs/avaliacao/…-WP3.md`, consolidado |

- Fases: **P0** preparação (swarm, memória, venv do CAD) → **P1** revisão do firmware ∥ CAD →
  **P2** implementação do firmware ∥ CAD → **P3** avaliação WP1 e WP2 + correções →
  **P4** merge dos worktrees → **P5** documentação → **P6** avaliação WP3 + consolidado →
  **P7** publicação do manual HTML e relatório final ao usuário.
- WP1 e WP2 não tocam os mesmos arquivos. Os pontos de contato (`CLAUDE.md`, `README.md`,
  `docs/DIVERGENCIAS.md`, `config.h` → `LOOKAHEAD_MM`) são resolvidos no merge (P4) pelo
  coordenador, e os textos finais ficam com o WP3.
- `.gitignore` modificado, `.claude/` e `.mcp.json` (configuração local do ruflo) não entram em
  nenhum commit.

---

## 9. Fora de escopo

- Teste em hardware real, sintonia em pista e pesagem das peças — entram como próximas
  atividades no relatório consolidado.
- Detecção de marcadores laterais e parada automática após a chegada. O revisor documenta o
  que o regulamento exige (§4.1); a implementação fica para uma versão seguinte.
- Integração do `VelocityProfile`.
- Novas pranchas SVG para os componentes 11–13 e regeneração das pranchas A1/A2.
- PCB dedicada (o perfboard continua).

---

## 10. Riscos

| Risco | Mitigação |
|-------|-----------|
| Regulamento exigir parada após a chegada | Relatório de revisão aponta; vira a primeira atividade da próxima versão |
| Cotas do suporte 18650 e das células variarem por fabricante | Cotas △ MEDIR em `medidas.toml`, berço com folga deslizante, cupom de tolerância |
| Massas ainda estimadas (D7) deslocarem o CG | Meta de carga nos patins com faixa; lookahead ajustável até 100 mm; relatório de montagem regenera o CG |
| Ganhos de fábrica não servirem na pista | Perfil "seguro" com velocidade baixa; roteiro de sintonia no manual do operador |
| Mudanças não testadas em hardware | Testes de bancada em `examples/` e checklist de primeira energização no manual |
| Inversão de polaridade ao trocar células | Fusível obrigatório, P-MOSFET recomendado, marcação no berço, item no checklist |

---

## 11. Emendas

Decididas pelo usuário em 2026-10-13 (E-1, E-2), depois do relatório de revisão
(`docs/REVISAO_FIRMWARE.md`), e em 2026-10-14 (E-3, E-4), depois da avaliação do WP1.

| # | Origem | Emenda | Onde |
|---|--------|--------|------|
| E-1 | F-33 (regra RoboCore 5.9) | Chave `lock` em `HardwareConfig` (fábrica 0). Com 1, todo comando BLE em ARMED/RUNNING é recusado com `err:"locked"` | §4.3, §4.5; plano WP1, Tarefa 22 |
| E-2 | Revisão, item 3 (pilha das tasks) | `info` leva `stk` com a menor folga de pilha das tasks `control` e `comm` | §4.5; plano WP1, adendo à Tarefa 18 |
| E-3 | Avaliação WP1, A-WP1-02 | Em ARMED/RUNNING o botão para o carro no aperto; o gesto é consumido | §4.6; plano WP1, Tarefa 23 |
| E-4 | Avaliação WP1, A-WP1-03 | START recusado com a calibração de encoder aberta; ERROR(ENC) = 5 no modo cascata quando uma roda fica com duty alto e RPM ≈ 0 | §4.6; plano WP1, Tarefa 24 |

Também acrescentados ao plano WP1, sem mudar esta spec: pull-up interno nos GPIO 18/19 do
encoder esquerdo (F-24) e versões fixas da plataforma e do NimBLE-Arduino (F-30).
