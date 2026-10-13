# Revisão do firmware — LFR v2

**Data**: 2026-10-13 · **Commit revisado**: `60997b2` · **Revisor**: agente code-analyzer (ruflo,
`wp1-revisor`)

Escopo lido: todo `src/`, `test/` (12 suítes), `platformio.ini`, `examples/` (6 programas de
bancada) e `docs/dashboard/` (`lfr-cockpit-mock.html`, `lfr-cockpit.html`, `lfr_sim.py`).
Fontes externas consultadas: página do produto Pololu 960 e guia Pololu 0J12; código do
NimBLE-Arduino nas tags 2.1.0 a 2.5.1; código e `sdkconfig` do arduino-esp32 2.0.17 (o core
usado pela plataforma `espressif32` 6.13.0 instalada nesta máquina); regulamento RoboCore de
Seguidor de Linha (revisão de 03/09/2024).

Nada em `src/` ou `test/` foi alterado. Valores marcados como *estimativa* não foram medidos em
hardware.

## Linha de base

```
$ pio test -e native
...
native         test_velocity_profile    PASSED    00:00:03.533
native         test_encoder             PASSED    00:00:00.374
native         test_ble_tuner           PASSED    00:00:00.368
native         test_velocity_estimator  PASSED    00:00:00.507
native         test_lap_timer           PASSED    00:00:00.372
native         test_pid                 PASSED    00:00:00.378
native         test_smoke               PASSED    00:00:00.353
native         test_motors              PASSED    00:00:00.363
native         test_calibration         PASSED    00:00:00.393
native         test_cascade             PASSED    00:00:00.373
native         test_line_follower       PASSED    00:00:00.360
native         test_speed_profile       PASSED    00:00:00.370
================= 82 test cases: 82 succeeded in 00:00:07.745 =================
```

- São **12 suítes**, não 13 como diz a spec §3. `test_sensors` está em `test_ignore` e só tem
  `.gitkeep`.
- 4 warnings, todos vindos de `test/` (`unused parameter 'argc'/'argv'` em
  `test/test_lap_timer/test_lap_timer.cpp:48` e `test/test_velocity_profile/test_velocity_profile.cpp:50`).
  Nenhum vem de `src/`.
- `pio run -e esp32dev` não foi executado: o framework Arduino e o NimBLE não estão instalados
  nesta máquina (não existe `.pio/libdeps/esp32dev`). O build ESP32 volta a ser exigido na
  Tarefa 18.

## Achados

Resumo: **5 CRIT · 9 HIGH · 15 MED · 6 LOW · 4 INFO** — 39 achados. F-01..F-17 foram todos
confirmados no código; F-18..F-39 são novos.

| ID | Sev. | Arquivo:linha | Defeito | Cenário de falha | Correção | Requisito / Tarefa |
|----|------|---------------|---------|------------------|----------|--------------------|
| F-01 | CRIT | `src/main.cpp:227`, `src/main.cpp:353` → `src/strategy/LineFollower.cpp:38` | `LineFollower::reset()` chama `_cal.reset()` e é chamado nas duas transições READY→RUNNING (BLE e botão) | Calibra 5 s → READY → start: min/max voltam a INT_MAX/INT_MIN antes do 1º ciclo de controle. O carro corre sem calibração; na prática (F-02) todos os canais saem 1000, a posição fica em 0 e o carro segue reto até sair na primeira curva | O reset do seguidor não toca na calibração; a calibração só muda em CALIBRATING | Tarefa 4 (cenário em T16) |
| F-02 | CRIT | `src/sensors/Calibration.cpp:32`, `:36` | Sem calibração, `range = _max − _min` = INT_MIN − INT_MAX: overflow com sinal (UB). A linha 36 estoura de novo em `(raw − _min) × 1000` | Com wrap de 32 bits (o que o Xtensa produz na prática; a UB permite outro resultado), `range` = 1 e qualquer leitura de 0 a 1023 normaliza em 1000: soma 8000, posição 0, linha nunca "perdida". Junto com F-01, o carro anda reto ignorando a linha | Marcar o canal como não calibrado e devolver 0 sem aritmética; validar a faixa de cada canal | Tarefa 3 |
| F-03 | CRIT | `src/sensors/Calibration.cpp:42-60`; testes `test_weighted_position_*` e `test/test_line_follower/` | **Confirmado.** A média ponderada assume "linha = leitura alta". Pololu 960: *"Lower output voltage is an indication of greater reflection."* Guia 0J12: *"over a white surface, its output voltage will tend towards 0 V; … over a black surface, … towards Vcc."* A pista é linha branca sobre preto | Linha sob os sensores 6–7 (posição esperada +3000), calibração 100–900: o cálculo dá −1000 — sinal invertido e 3× atenuado. O PID vira para o lado oposto (realimentação positiva) e o carro abandona a linha. Sobre preto puro a soma é máxima e a linha nunca é dada como perdida. Hoje fica escondido atrás de F-01/F-02 | Inverter a leitura normalizada conforme `pol` (padrão 1 = linha branca) antes da média; reescrever os testes que usam "linha = alto" | Tarefa 3 (cenário em T16) |
| F-04 | CRIT | `src/main.cpp:132-134`, `:326`, `:351`, `:459`, `:477` | Botão lido por nível, sem debounce nem detecção de borda | Toque normal (100–300 ms) em READY: vai a RUNNING, `beep(50)` bloqueia, na iteração seguinte o botão ainda está baixo → `motors.stop()` + READY + `beep(300)`. O carro não sai. Segurando mais de 350 ms, alterna RUNNING/READY. Em ERROR, o toque leva a IDLE e, se o botão ainda estiver apertado depois do `beep(100)`, a CALIBRATING — apaga a calibração com o carro parado | `ButtonInput` com debounce de 30 ms e eventos curto/longo por borda; a máquina de estados reage a eventos | Tarefas 11 e 14 |
| F-05 | HIGH | `src/strategy/LineFollower.cpp:23-24`, `src/main.cpp:379-384`, `:405` | O firmware só tem o modo cascata e a base vem de `g_baseRpm`, fixo. O `SpeedProfile` é calculado dentro de `LineFollower::update` e o PWM resultante é descartado (`discardL/R`) | Curva fechada com base 600 RPM: a base não cai, a roda externa satura (F-25) e o carro escapa da curva. O comando BLE `spd` não tem efeito nenhum no robô | Base = `SpeedProfile(abs(erro))` com rampa, aplicada nos dois modos | Tarefa 5 (perfil) + Tarefa 16 (uso) |
| F-06 | HIGH | `src/main.cpp:366`, `src/sensors/VelocityEstimator.cpp:64-72` | `dtMs` inteiro, em ms. `update()` drena o delta do encoder **antes** de testar `dtMs == 0` e descarta a janela | Laço RUNNING de 0,3–0,5 ms (*estimativa*: só o clock SPI dos 8 canais leva 192 µs): `dtMs` = 0 em quase todo ciclo, os pulsos vão fora e o RPM fica parado no último valor (0 depois do start). Quando a task comm estica um ciclo, `dtMs` = 1 com janela real de 1–2 ms (erro de até 2×) e 1 pulso vale 2143 RPM (PPR 28). A malha interna fica cega e vira PWM = Kp × setpoint | `update(dtUs)` com dt em µs | Tarefa 6 |
| F-07 | HIGH | `src/main.cpp:39-43` (dt padrão 0,01 s, `src/control/PIDController.h:5`), `src/main.cpp:72-77` (0,002 s), `src/config.h:78` | PIDs com dt fixo num laço sem período: `LOOP_PERIOD_US` não é usado em lugar nenhum | Com laço de ~0,4 ms, o termo D externo fica ~25× menor que o pretendido (Kd 12 age como ~0,5) e o I ~25× maior; nos PIDs internos, o D fica ~5× menor. Qualquer mudança de carga no laço muda a sintonia | dt real, em segundos, a cada chamada; laço de período fixo (2000 µs, `vTaskDelayUntil`) | Tarefa 2 + Tarefa 18 |
| F-08 | HIGH | `src/control/CascadeController.cpp:30-31`, `src/config.h:55-57` | Malha interna só com PID (Kp 0,8, Ki 0, Kd 0,05), sem feedforward | Modelo linear do N20 (740 RPM com PWM 1023 a 7,4 V, sem carga): regime em 37 % do setpoint (600 → 220 RPM), menos com carga. O carro anda devagar e a diferença L/R pedida pela malha externa sai comprimida | `ff = sp / rpmMax` + PID do erro, em fração; anti-windup condicional | Tarefa 7 |
| F-09 | HIGH | `src/main.cpp:314`, `:2-3`, `:321`; arduino-esp32 2.0.17, `sdkconfig`: `CONFIG_ARDUINO_RUNNING_CORE=1` | O `loop()` (controle) roda no core 1 com prioridade 1, e a task `Comm` foi fixada no mesmo core com a mesma prioridade. O core 0 fica só com a pilha BLE (controlador e host NimBLE fixados no core 0) | Controle e comunicação dividem o core 1 por *time slicing* de 1 ms: a cada 10 ms a comm entra no meio de um ciclo (JSON com ~20 `%f`, `notify`, `beep()` em espera ativa), estica o ciclo e abre as corridas de F-19. Os comentários dizem o contrário | Task `control` no core 1, prioridade alta, período fixo; `comm` no core 0 (DEC-7) | Tarefa 18 |
| F-10 | HIGH | `src/main.cpp:116-122` | `raw / 4095 × 3,3` sem a calibração de eFuse e reta LiPo de 6,4 a 8,4 V | Sem calibração, o ADC a 11 dB erra até ~10 %: 7,4 V podem ser lidos entre ~6,7 e ~8,1 V. Com 2× 18650 (DEC-1) a curva de Li-ion não é linear e o percentual não serve para decidir a troca. O divisor tem 32 kΩ de impedância de Thévenin e não há capacitor no GPIO 34 | `analogReadMilliVolts()` × 147/47 × `vbk`, filtro de ~0,5 s, curva por química; capacitor de 100 nF no GPIO 34 (manual de montagem) | Tarefa 12 + Tarefa 18 |
| F-11 | MED | `src/main.cpp:125-130`; chamadas em `:210`, `:213`, `:232`, `:238`, `:247`, `:318`, `:331`, `:344`, `:358`, `:463`, `:479` | `beep()` faz espera ativa de 50 a 500 ms, nas duas tasks | `beep(50)` atrasa o 1º ciclo depois do start; `beep(500)` na comm (fim de calibração de encoder) toma metade do core 1 por 0,5 s, e nesse tempo nenhum comando é lido | Sinalização por padrão temporal, sem bloquear (`Annunciator`) | Tarefa 15 |
| F-12 | MED | `src/control/PIDController.cpp:29-32`; `src/strategy/LineFollower.cpp:27-30`; `src/control/CascadeController.cpp:45-53` | `reset()` zera `_prevMeasurement`; `setPID` e `setInnerGains` chamam `reset()` | Start com o carro a 1500 unidades do centro: D = −12 × 1500 / 0,01, saída saturada no 1º ciclo (tranco). Ajuste ao vivo: cada `pid`/`pidv` reseta o PID com o carro andando; na malha interna, a 300 RPM, D = −0,05 × 300 / 0,002 = −7500 → PWM −1023 por um ciclo. O comentário "MS-2" diz que o reset evita o pico, mas é ele que o causa. O dashboard mock manda `pid` a cada evento `input` do slider | `reset(medida)` alinha a memória da derivada; `setTunings` não reseta | Tarefa 2 |
| F-13 | MED | `src/strategy/LapTimer.cpp:16` (nenhuma chamada em `src/`) | `notifyLineCrossing()` nunca é chamado; não existe detecção de marcação | `lap` e `laps` da telemetria ficam sempre `null` e `[]` | Detectar as marcações da direita (ver Regulamento) | Fora do escopo (spec §9; depende de F-34) |
| F-14 | MED | `src/sensors/Calibration.cpp:53-56`, `src/strategy/LineFollower.cpp:8-25`, `src/main.cpp:362-471` | Linha perdida mantém a última posição; não há temporizador nem parada | Saída de curva com última posição −200: o carro segue quase reto para fora da pista e continua a ~1 m/s até bater ou acabar a bateria. Com F-03 ele nem chega a "perder" a linha: sobre o preto a soma é máxima | Erro satura em ±1 para o último lado visto; ERROR(LINE) depois de `lostMs` | Tarefa 4 + Tarefas 14 e 16 |
| F-15 | MED | `src/control/SpeedProfile.cpp:11-13`, `:23-27` | `_baseSpeed` é guardado e nunca usado; erro zero devolve `maxSpeed` | O operador muda `base` pelo `spd` e nada acontece; quem sintoniza acha que a reta corre em `base` | `vmax`/`vmin`/`thr` em fração; `base` sai do protocolo | Tarefa 5 (+ T10, legado `spd`) |
| F-16 | LOW | `src/sensors/VelocityEstimator.cpp:115` (única escrita na NVS) | Só o PPR é persistido; ganhos, velocidades e calibração dos sensores não | Toda a sintonia e a calibração se perdem ao desligar; cada vez que liga são mais 5 s de varredura | `ConfigStore` com versão + CRC32, 3 perfis e calibração | Tarefa 9 |
| F-17 | HIGH | `src/config.h:48-50` | `ENCODER_DEFAULT_PPR_X4 28` conta por volta do eixo do **motor** (7 PPR × 4) e ignora a redução | Em 50:1, o RPM do eixo de saída sai 21× alto com o encoder magnético Pololu de 12 CPR (ficha 04, `docs/DIVERGENCIAS.md` D6) ou 50× alto com 7 PPR. Sem a auto-calibração, a cascata vê o motor "rápido demais" e corta o PWM; hoje parte disso fica escondida atrás de F-06 | Padrão no eixo de saída (600, provisório) e `cal` obrigatório antes de `mode` = 1 | Tarefa 6 |
| F-18 | HIGH | `src/comm/BLETuner.cpp:9`, `:14`, `:18`, `:26`, `:208` | `BLETuner::_instance` nunca recebe `this`. A atribuição ficava no construtor e saiu no commit `456d00d` (Fase 7); o construtor atual é vazio | Todos os callbacks NimBLE retornam logo no início: nenhum comando é aplicado e `_clientConnected` nunca vira `true`, então `notifyInfo` e `notifyTelemetry` não enviam nada. O robô anuncia e aceita conexão, mas o BLE não funciona: sem ajuste, sem telemetria, sem `stop` remoto. `test_ble_tuner` só cobre os flags e não pegou a regressão | `_instance = this` no `begin()` + teste nativo que injeta comando pelo caminho do callback | Tarefa 17 |
| F-19 | HIGH (latente) | `src/main.cpp:234-248` (comm) × `src/main.cpp:362-411` (loop) | `stop` e `reset` pelo BLE chamam `motors.stop()` e trocam o estado na task comm, enquanto o `loop()` pode estar no meio de um ciclo RUNNING. READY e IDLE não voltam a comandar os motores | A comm entra por *time slicing* depois de o loop ler RUNNING e antes do `motors.setSpeed()`: o stop é aplicado, o loop retoma e escreve PWM ≠ 0, e o carro segue em READY/IDLE com o último PWM, sem controle. A janela cobre a maior parte do ciclo. Hoje fica escondido atrás de F-18; **vira CRIT se F-18 for corrigido sem as Tarefas 17 e 18** | Só a task de controle toca nos motores; comandos de estado por fila; fora de RUNNING os motores são freados a cada ciclo | Tarefas 14, 17 e 18 |
| F-20 | MED | `src/main.cpp:205-222`, `src/sensors/VelocityEstimator.cpp:95-121` | O comando `cal` (encoder) é aceito em qualquer estado | Em RUNNING, `cal start` zera o RPM e suspende o estimador (malha interna cega); `cal finish` grava na NVS com o carro andando. A escrita na flash suspende o cache dos dois núcleos, a ISR do encoder (que não é IRAM-safe, F-32) fica parada e o ciclo passa de `dtUs > 4000` → ERROR. E ainda há um `beep(500)` bloqueante | Recusar `cal` em ARMED/RUNNING com `err:"running"` | Tarefa 17 (`CommandHandler`, já previsto) |
| F-21 | MED | `src/comm/BLETuner.cpp:56-68`, `:85-95`, `:109` | `pid` e `spd` são aplicados sem validação (o hardening da Wave 3 cobriu só `pidv` e `rpm`): `atof` aceita `nan`, `inf` e negativos, e `static_cast<int>` de float fora da faixa é UB (`spd`, `rot`). O parser exige `"t":"` sem espaço e não responde com ack | `{"t":"pid","kp":nan}` escrito num app BLE genérico: a saída do PID vira NaN, `_clamp` devolve `outMax` (`std::min`/`std::max` com NaN) e a correção fica saturada para sempre — o carro gira no lugar. Um cliente que mande `{"t": "pid", …}` (padrão do `json.dumps` do Python) é ignorado em silêncio | `CommandProtocol` + `RobotConfig::set` validando valor finito e faixa; ack com código de erro | Tarefas 8 e 10 |
| F-22 | MED | `src/comm/BLETuner.cpp:132-136`, `:177-186`; `platformio.ini:15` | `setValue(buf)` com `char buf[128]` e `char buf[400]`. Para array não-const, o template `setValue(const T&)` casa melhor que `setValue(const char*)`. No NimBLE-Arduino 2.1.0 a 2.4.0, esse template envia `sizeof(T)` bytes; só a 2.5.0 (2026-04-02) trata `char[]` com `strnlen` | Com a biblioteca instalada antes de abril de 2026, cada `info` sai com 128 bytes e cada `tel` com 400 (cortado no MTU), com lixo depois do `\0`: o `JSON.parse` falha e o dashboard descarta tudo. Como o `lib_deps` não tem versão, o comportamento depende da data do `pio pkg install` | `setValue(ptr, strlen(ptr))` explícito; fixar a versão do NimBLE-Arduino | Tarefa 17 (já usa ponteiro + tamanho) + F-30 |
| F-23 | MED | `src/comm/BLETuner.cpp:142-187` | Frame `tel` único com 165 a 258 bytes, medidos reproduzindo o `snprintf`: 165 parado, 191 correndo, 221 com 5 voltas, 258 no pior caso | Passa do limite de 180 bytes da spec em qualquer corrida. O pior caso passa até do máximo do próprio firmware (MTU preferido de 255 do NimBLE → 252 bytes de carga). Clientes com MTU ~185 (iOS/macOS) recebem quase todo frame cortado pela pilha → JSON inválido | Dividir em `tel` (30 Hz) e `sta` (2 Hz), cada um ≤ 180 bytes; `setMTU(185)` | Tarefa 10 + Tarefa 17 |
| F-24 | MED (a confirmar) | `src/sensors/Encoder.cpp:107-108`; `docs/WIRING.md:65-66` | O encoder esquerdo (GPIO 18/19) é configurado como `INPUT`, sem pull-up. O WIRING.md diz "pull-up interno" e só prevê resistor externo para 36/39 | Se a saída do encoder for de coletor aberto (o encoder comprado é genérico), os GPIO 18/19 flutuam: a ISR dispara com ruído, o contador gira sozinho e a rajada de interrupções rouba tempo do core 1. Se o encoder for alimentado em 5 V com saída push-pull, entram 5 V no GPIO | `INPUT_PULLUP` em 18/19 (ou 10 kΩ externos, como em 36/39); alimentar o encoder em 3,3 V; conferir no `bench_encoder` | **Sem tarefa no plano** — sugerir acréscimo na Tarefa 18 e checagem na Tarefa 19 |
| F-25 | MED | `src/config.h:59-62`, `src/comm/BLETuner.h:28-31` | Padrões de RPM feitos para "N20 1500 RPM @ 6 V": `BASE_RPM` 600 e `MAX_RPM` 1200. O motor do projeto é de 600 RPM @ 6 V (DEC-4; ≈ 740 RPM a 7,4 V sem carga) | A base pede 100 % da rotação livre a 6 V e a roda externa da curva pede até 1200 RPM, o que não existe. A malha interna satura e a curva só acontece freando a roda interna: perde-se autoridade de direção justamente quando ela faz falta | Velocidades em fração de `rpmMax` = 700 (spec §4.3) | Tarefa 8 + Tarefa 18 |
| F-26 | MED | `src/main.cpp:125-130`; `hardware/bom.csv:18`; `docs/WIRING.md:87` | O buzzer da BOM é passivo ("PWM tone"), mas o firmware o aciona com nível DC (`digitalWrite`) | Buzzer passivo só estala na borda: não há bip audível no fim da calibração, no start, no stop nem no erro. Se o buzzer for magnético (~16 Ω) e estiver direto no GPIO 22, o nível alto contínuo por até 500 ms puxa bem mais que os 40 mA de limite do pino | Tom por LEDC (~2,5 kHz, canal 2); transistor de acionamento — conferir a fiação | Tarefas 15 e 18 (LEDC já previsto); fiação → WP3 (manual de montagem) |
| F-27 | MED | `docs/dashboard/lfr-cockpit-mock.html:829`, `:844`, `:953`; `docs/dashboard/lfr-cockpit.html:248-253`, `:385`; `docs/dashboard/lfr_sim.py:11-30` | Nenhum dashboard fala com o robô. O `lfr-cockpit-mock.html` (o documentado no README) não tem Web Bluetooth: só WebSocket para `ws://localhost:8766` (simulador). O `lfr-cockpit.html` usa outro protocolo BLE (serviço `12345678-…`, uma característica por parâmetro, telemetria em CSV) | Para ajustar o robô hoje só resta escrever JSON à mão num app BLE genérico — que ainda esbarra em F-18. A docstring do `lfr_sim.py` está desatualizada (sensores "TCRT5000", `vL` em ±255) | Web Bluetooth no mock, no protocolo da spec §4.5; remover o `lfr-cockpit.html` | Tarefa 20 |
| F-28 | LOW | `src/main.cpp:416-438` | O snapshot da telemetria só é atualizado em RUNNING; o estado do robô e o motivo do ERROR não são enviados | Em IDLE, CALIBRATING e READY o dashboard mostra os sensores congelados da última corrida: não dá para conferir a calibração antes da largada. Um ERROR por `dtUs > 4000` não se distingue de outro | Ler sensores e atualizar o snapshot em todos os estados; `st` no `tel` e `err` no `sta` | Tarefas 16 a 18 (conferir que o snapshot sai fora de RUNNING) |
| F-29 | LOW | `src/comm/BLETuner.cpp:79-121`; polling em `:212-374` | O parse inteiro (até 4 `atof`) roda dentro de `taskENTER_CRITICAL` no callback NimBLE (core 0), e a comm (core 1) disputa o mesmo spinlock a cada 10 ms | Enquanto o core 0 interpreta o comando, a comm gira no spinlock com as interrupções do core 1 mascaradas: a ISR do encoder atrasa e, acima de alguns milhares de transições por segundo, perde passos (a tabela de quadratura ignora saltos de dois estados) | O callback só copia os bytes para a fila; o parse fica na task | Tarefas 10 e 17 |
| F-30 | MED | `platformio.ini:9`, `:13-15` | `platform = espressif32` e `NimBLE-Arduino` sem versão | O build muda com a data da instalação: F-22 depende da versão do NimBLE, e uma plataforma com Arduino core 3.x remove `ledcSetup`/`ledcAttachPin` (`src/motors/MotorDriver.cpp:15-16`). A plataforma instalada hoje é a 6.13.0 (core 2.0.17) | Fixar as versões que compilarem na Tarefa 18 (ex.: `espressif32@6.13.0`; NimBLE-Arduino ≥ 2.5.0) | **Sem tarefa no plano** — sugerir acréscimo na Tarefa 19 |
| F-31 | LOW | `examples/differential_hw_test.cpp:17`; `platformio.ini:6` | Nenhum ambiente compila `examples/`. O `differential_hw_test.cpp` chama `DifferentialDrive(BASE_SPEED_DEFAULT, MIN_SPEED, PWM_MAX)`, construtor que não existe (só há `DifferentialDrive(int)`) | Os testes de bancada se degradam sem ninguém ver; este já não compila. `test_ignore = test_sensors` aponta para uma suíte vazia | Um ambiente `bench_*` por exemplo, todos compilando | Tarefa 19 |
| F-32 | LOW | `src/sensors/Encoder.cpp:68`, `:120-124`; `src/sensors/Encoder.h:59-65` | `IRAM_ATTR` nas ISRs não as torna seguras com o cache desligado: no core 2.0.17 `CONFIG_ARDUINO_ISR_IRAM` está desligado, então `digitalRead` fica na flash e o serviço de ISR de GPIO é instalado sem `ESP_INTR_FLAG_IRAM`. `QUAD_TABLE` fica em `.rodata` (flash) | Durante escrita na NVS a ISR fica suspensa e pulsos se perdem; fora disso, cada ISR pode pegar *cache miss*. Aceitável enquanto não houver escrita em RUNNING (spec §4.4) | Ler os pinos por registrador e pôr a tabela em `DRAM_ATTR`, ou usar o periférico PCNT em modo quadratura (sem ISR) | Fora do escopo (funciona com a regra de §4.4; PCNT fica para a próxima versão) |
| F-33 | MED | `src/main.cpp:158-248`; spec §4.3 (coluna "Ao vivo em RUNNING") | `stop` e ajustes ao vivo pelo BLE são aceitos durante a corrida, no firmware atual e na spec | Regra RoboCore 5.9: quem usa comunicação remota tem que demonstrar aos juízes que ela não pode parar o robô na área de partida-chegada nem alterar parâmetros durante a tentativa; a 6.2.1 desclassifica controle externo depois do início. Hoje isso só se demonstra desconectando o celular | Modo competição: recusar todo comando BLE em ARMED/RUNNING (`err:"locked"`), ligado por uma chave `lock` | Fora do escopo do plano (exige decisão e mudança na spec §4.3/§4.5); recomendação ao avaliador |
| F-34 | CRIT (regulamento) | — (funcionalidade ausente) | Não há detecção das marcações laterais nem parada automática depois da chegada | Regra 5.7: a volta só vale se o robô parar sozinho e completamente dentro da área de partida-chegada (1 m entre as marcações de chegada e de partida, ±200 mm da linha), sobre a linha, e ficar parado por pelo menos 2 s. Sem isso, **nenhuma volta é válida na RoboCore**. As marcações (19 × 40 mm, a 40 mm da linha) ficam fora do alcance do QTR-8A (sensores extremos a ±33 mm do centro) e o MCP3008 não tem canal livre | Sensores laterais dedicados, contagem de marcações e parada na área (ver Regulamento) | Fora do escopo (spec §9); confirma o risco da spec §10 — primeira atividade da próxima versão |
| F-35 | LOW | `src/comm/BLETuner.cpp:36-52`, `src/main.cpp:138` | BLE sem pareamento nem autenticação; nome fixo `LFR-RACER-01`; o anúncio não leva o UUID do serviço | Qualquer celular no alcance conecta e manda `start` ou `pid`; dois carros com o mesmo firmware ficam indistinguíveis; um `requestDevice` filtrado por serviço pode não achar o robô | Sufixo do MAC no nome; UUID no anúncio; pareamento opcional | UUID no anúncio: Tarefa 17. O resto: fora do escopo (a spec não pede) |
| F-36 | INFO | `src/sensors/SensorArray.h:16`, `src/sensors/SensorArray.cpp:16-39`; `CLAUDE.md:125` | O orçamento declarado (< 100 µs para os 8 canais) não cabe em 1 MHz: 8 × 24 bits = 192 µs só de clock (≈ 250–300 µs com as transações, *estimativa*) | Não compromete o laço de 2 ms. A medir: a saída do QTR-8A é de alta impedância (fototransistor com pull-up) e o MCP3008 amostra por 1,5 ciclo de clock; pode haver arrasto entre canais vizinhos | Medir em bancada com um só canal sobre a linha; se houver arrasto, baixar o clock ou descartar a 1ª conversão | Tarefa 19 (bancada); texto → Tarefa 21 |
| F-37 | INFO | `src/comm/Logger.h`, `src/comm/Logger.cpp`; `src/main.cpp:54`, `:289`, `:440-457` | `DEBUG_LOG` nunca é definido e o `drain` usa um callback vazio: o Logger não produz saída. O `main.cpp` da Tarefa 18 não o usa | Código morto compilado no `native` (`test_build_src = yes`) | Remover numa limpeza futura ou reaproveitar no `sta` | Sem tarefa (fica sem uso depois do plano) |
| F-38 | INFO | `src/control/VelocityProfile.h`, `src/control/VelocityProfile.cpp` | Só é referenciado pelo próprio teste | Nenhum efeito no robô | Manter fora desta versão | Fora do escopo (spec §9) |
| F-39 | INFO | `test/test_calibration/` (`test_weighted_position_*`, `test_reset_clears`, `test_initial_min_max_inverted`); `test/test_line_follower/` (`test_line_lost_maintains_last_direction`); `test/test_pid/` (`test_pid_reset_clears_state`); `test/test_velocity_estimator/` (`test_velocity_handles_zero_dt_safely`); `test/test_ble_tuner/` | Testes que fixam os defeitos: "linha = leitura alta" (F-03), sentinela INT_MAX/INT_MIN (F-02), linha perdida sem tempo limite (F-14), reset zerando a derivada (F-12), janela descartada com dt = 0 (F-06); `test_ble_tuner` não passa pelos callbacks (F-18) | Os 82 testes verdes não indicam um carro funcional | Reescrever junto com cada correção | Tarefas 2, 3, 4, 6 e 17 |

## Itens obrigatórios (spec §4.1)

**1. Pinos de *strapping*.** O ESP32 clássico tem cinco: GPIO 0, 2, 5, 12 (MTDI) e 15 (MTDO)
(datasheet ESP32, seção *Strapping Pins*). O GPIO 4 **não** é um deles — a spec §4.1 o lista por
engano — e o GPIO 5 não é usado.

- **GPIO 0, botão** (`src/config.h:25`, `INPUT_PULLUP` em `src/main.cpp:308`). Nível baixo no reset
  entra em modo download: ligar o carro com o botão apertado faz o firmware não rodar.
  Mitigação: aviso no manual do operador. Para o plano: no caso "botão segurado desde o boot"
  (Review Focus 4), o aperto só pode começar depois da amostragem da ROM; ligar com o botão já
  apertado é modo download, não um toque longo.
- **GPIO 2, LED** (`src/config.h:27`). Tem que estar baixo ou flutuante para entrar em download; o
  LED do DevKit (ligado ao GND) não atrapalha. Não ligar pull-up externo nesse pino.
- **GPIO 4, BIN2** (`src/config.h:14`). Sem função de *strapping*. No boot fica como entrada, e o
  TB6612 só sai do standby em `motors.begin()` (STBY com pull-down, conforme a BOM): os motores
  ficam parados no boot.
- **GPIO 12, MISO do MCP3008** (`src/config.h:5`). Nível alto no reset seleciona flash de 1,8 V e o
  WROOM-32 (flash de 3,3 V) não boota. Com a fiação do `docs/WIRING.md` (MCP3008 a 3,3 V), o DOUT
  fica em alta impedância enquanto o CS está alto — e o GPIO 15 tem pull-up no reset —, então
  prevalece o pull-down interno do GPIO 12: risco baixo. Mas a `hardware/bom.csv:3` diz
  *"1 MHz @ 5V; usar divisor de tensão MISO → 3.3V"*, em conflito com o WIRING.md: alimentar o
  MCP3008 em 5 V sem o divisor põe 5 V no GPIO 12, que não tolera 5 V. Mitigação: seguir o
  WIRING.md (3,3 V), pull-down de 10 kΩ no GPIO 12 e, se ainda assim houver falha de boot, fixar o
  eFuse de VDD_SDIO em 3,3 V (`espefuse.py set_flash_voltage 3.3V`, irreversível). A divergência
  BOM × WIRING deve entrar no `docs/DIVERGENCIAS.md`.
- **GPIO 15, CS do MCP3008** (`src/config.h:7`). Nível baixo no reset só silencia o log da ROM. O
  pull-up interno mantém o MCP3008 desselecionado no boot, o que protege o GPIO 12.
- **Extra — GPIO 36/39, encoder direito** (`src/config.h:22-23`). Pela errata do ESP32, ligar o
  SARADC (o `analogRead` do GPIO 34 usa o ADC1) puxa os GPIO 36 e 39 para baixo por ~80 ns e gera
  interrupção espúria. A ISR relê os dois pinos e a tabela de quadratura devolve 0 para estado
  repetido, então o efeito é nulo. Manter a decodificação por estado (nunca contar bordas).

**2. ISRs do encoder.** `_isrTrampoline` (`src/sensors/Encoder.cpp:120`) e `_applyTransition`
(`:68`) têm `IRAM_ATTR` na definição; `src/sensors/Encoder.h:70-72` explica por que não na
declaração. Não há `float` dentro da ISR: só `uint8_t`, `int8_t` e `uint32_t`. Ressalva em F-32:
`digitalRead` e `QUAD_TABLE` ficam na flash e o serviço de ISR é instalado sem flag IRAM. A ISR
roda no core 1 (onde o `setup()` chama `attachInterruptArg`), o mesmo do controle na arquitetura
alvo. Os consumidores usam `portENTER_CRITICAL`, que na mesma CPU mascara a ISR; a partir do
core 0 o spinlock não exclui a ISR, que não o toma — um `reset()` do encoder vindo do core 0
(calibração, Tarefa 17) pode perder uma contagem simultânea, efeito desprezível. Carga
*estimada*: a 740 RPM e 600 contagens por volta, ~7,4 mil interrupções por segundo por roda.

**3. Pilha das tasks.** A task `Comm` tem 8192 bytes (`src/main.cpp:314`) e o `loopTask` também
(`CONFIG_ARDUINO_LOOP_STACK_SIZE=8192`); o plano mantém 8192 para `comm` e `control`. O maior
consumo é o `notifyTelemetry`: ~620 bytes de buffers locais (`sarr` 96, `larr` 80, `lapVal` 16,
`buf` 400), mais o `_vfprintf_r` do newlib com `%f` (~1–1,5 KB, *estimativa*) e o caminho do
`notify` (~0,5–1 KB). O `NimBLEDevice::init` também roda dentro da comm (`src/main.cpp:138`).
Margem *estimada* acima de 4 KB: adequada, mas sem medição. Recomendação, ainda fora do plano:
publicar `uxTaskGetStackHighWaterMark` das duas tasks no `info` na primeira energização
(Tarefa 18).

**4. `setValue` com `char[N]`.** Confirmado no código-fonte do NimBLE-Arduino (registrado como
F-22). `NimBLELocalValueAttribute` tem `setValue(const char*)` e o template
`setValue(const T&)`. Para `char buf[128]` não-const, o template liga por identidade, enquanto a
sobrecarga não-template exige conversão de qualificação; o template vence. Nas tags 2.1.0 a 2.4.0,
o ramo C++17 de `NimBLEAttValue::setValue(const T&)` termina em
`setValue(reinterpret_cast<const uint8_t*>(&s), sizeof(s))`, isto é, o buffer inteiro. A 2.5.0
acrescenta um ramo para arrays de `char` com `strnlen`. Como não existe
`.pio/libdeps/esp32dev` nesta máquina, a versão efetivamente usada no último build não pôde ser
verificada. O plano já resolve: a Tarefa 17 chama `setValue(ptr, strlen)`.

**5. Tamanho das mensagens BLE e MTU.** Medidas reproduzindo os `snprintf` atuais (F-23):

| Frame | Bytes |
|-------|-------|
| `tel` parado, sem volta | 165 |
| `tel` correndo, sem volta | 191 |
| `tel` correndo, 5 voltas | 221 |
| `tel` pior caso (PPR 28, RPM alto, 5 voltas) | 258 |
| `info` na conexão | 61 |
| `info` de calibração de encoder | 58 |

O firmware não chama `setMTU`. O padrão do NimBLE-Arduino é MTU preferido de 255
(`CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU`, `nimconfig.h`), então o MTU efetivo é o menor entre o do
cliente e 255, e cada notificação leva no máximo MTU − 3 bytes — a pilha corta o excedente. O
limite de 180 bytes da spec cabe no MTU de 185 dos clientes Apple. Hoje todo `tel` em corrida
passa desse limite.

**6. Dados compartilhados entre tasks.** Hoje as duas tasks estão no mesmo núcleo (F-09), então as
corridas vêm da preempção por *time slicing*, não de paralelismo. Com a DEC-7 (controle no core 1,
comm no core 0) elas viram corridas paralelas, e o desenho de cópia versionada mais fila do plano
(Tarefas 10, 17 e 18) passa a ser obrigatório.

- `g_baseRpm` (`src/main.cpp:97`): escrito sob `lineFollowerMux` (`:198-201`) e lido sem trava em
  `:405` e `:433-434`. A leitura de 32 bits é atômica, mas o mesmo ciclo pode usar dois valores
  diferentes na cascata e no snapshot.
- `lineFollower` e `pid`: `setPID` é protegido (`:161-163`), mas `pid.reset()` e
  `lineFollower.reset()` em `:226-227` rodam sem trava.
- `cascade`: `setInnerGains` e `setMaxRpm` correm sob a trava (`:182-184`, `:198-201`), mas
  `cascade.compute` (`:405`) roda fora dela e pode pegar ganhos pela metade (Kp novo, Ki velho) e
  um reset no meio do cálculo.
- `velL`/`velR`: `update()` no loop (`:370-371`) contra `startCalibration`, `finishCalibration` e
  `getEffectivePPR` na comm (`:207-215`), sem trava, com corrida em `_calibrating`,
  `_rpmFiltered` e `_pprX4` (o `Encoder` em si é protegido por `portMUX`).
- `motors`: `stop()` na comm contra `setSpeed()` no loop — F-19.
- `lapTimer.start()` nas duas tasks (`:229`, `:355`); `logger.setEnabled()` nas duas.
- `robotState` é atômico, mas a comm faz "verifica e depois age" (`:225`, `load() == READY` e
  depois `store(RUNNING)`), em paralelo com a mesma transição feita pelo botão no loop.

**7. Dashboards.** Confirmado e registrado como F-27: hoje nenhum dashboard fala com o robô. O
`lfr-cockpit-mock.html` só tem WebSocket para o simulador e o `lfr-cockpit.html` usa um protocolo
BLE antigo. Decisão já tomada no plano: a Tarefa 20 leva o BLE ao `lfr-cockpit-mock.html` e remove
o `lfr-cockpit.html`.

**8. `Logger` e `VelocityProfile`.** Os dois ficam sem uso depois do plano. O `Logger` já não
produz nada hoje, e o `main.cpp` da Tarefa 18 não o referencia (F-37). O `VelocityProfile` só
aparece no próprio teste (F-38, spec §9).

## Regulamento

**Fonte**: RoboCore, *Seguidor de Linha — Regras*, documento revisado em 03/09/2024 —
<https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf>
(acesso em 13/10/2026). A página <https://www.robocore.net/eventos/regras> redireciona para
<https://events.robocore.net/regras/?evento=regras>, que monta a lista por script e não expõe o
link no HTML; o PDF abriu direto.

**(a) Parada depois da chegada — exigida.**

> 5.7 Uma volta será considerada válida quando o robô percorrer todo o percurso; parar
> automaticamente e completamente dentro da "área partida-chegada", assim como sobre a linha; e
> lá permanecer parado por pelo menos 2 segundos.

> 4.7 […] A linha de chegada está localizada a um metro para trás da linha de partida.

> 4.8 A área da pista a qual se estende entre o ponto de partida e o ponto de chegada,
> considerando 200 mm à direita da linha e 200 mm à esquerda da linha, é denominada "área de
> partida-chegada".

O robô cruza a marcação de chegada e tem no máximo 1 m (até a marcação de partida) para parar
sobre a linha, e precisa ficar parado por 2 s. Também: o robô parte parado sobre a linha, dentro
dessa área (5.5), e uma volta em que o robô sai completamente de cima da linha é invalidada (5.6).
Sem a parada, nenhuma volta é válida (F-34).

**(b) Marcações laterais.**

> 4.7 […] Há marcações no lado direito da linha (em relação ao sentido do percurso) indicando os
> pontos de partida e de chegada (vide figuras 2 e 3).

> 4.15 Haverá uma marcação no lado esquerdo da linha (em relação ao sentido do percurso) no ponto
> em que houver alteração da curvatura (vide figura 4).

Pelas figuras 2 e 4, cada marcação é um retângulo branco de 19 ± 1 mm no sentido do percurso por
40 mm no transversal, a 40 mm da linha; a tolerância de posição é de ± 3 mm (4.17). Há também
cruzamentos a 90 ± 5°, com 250 mm de reta antes e depois (4.6), raio mínimo de 100 mm (4.14) e
rampas de até 5° (4.18). Consequência para o hardware: os sensores extremos do QTR-8A ficam a
±33 mm do centro (7 × 9,525 mm / 2), então a barra atual não enxerga nenhuma marcação; é preciso
um sensor de cada lado, a ~50–90 mm do centro, e os 8 canais do MCP3008 já estão ocupados.

**(c) Comunicação sem fio e ajustes entre tentativas.**

> 3.1 […] Eles não podem ser controlados externamente, com exceção para serem iniciados ou para
> ajustes de parâmetros.
>
> 3.1.1 Os parâmetros do robô, tais como velocidade, aceleração ou qualquer outro, podem ser
> alterados, seja por meio de chaves, botões ou de maneira remota, desde que o software já
> existente assim permita e que isso seja feito apenas entre duas tentativas.

> 5.9 Caso o robô possua função de comunicação remota para ser iniciado, é papel do competidor
> demonstrar aos juízes que essa comunicação não pode ser utilizada para parar o robô dentro da
> "área de partida-chegada" ou alterar qualquer outro parâmetro durante uma tentativa. O
> dispositivo de comunicação remota deverá permanecer em um local predeterminado pelo juiz
> durante a tomada de tempo.

Também: 5.8 (nenhuma interferência externa depois de iniciada a tentativa), 5.10 (proibido
carregar software novo ou alterar o hardware durante a tomada de tempo; manutenção só entre
tentativas, com as mesmas peças) e 6.2.1 (desclassificação por controle externo depois de
começada a tomada de tempo). Cada tomada tem até 5 minutos e 3 tentativas (5.3, 5.11). Para a
v2: o ajuste por BLE e a troca de perfil entre tentativas são permitidos; o start remoto é
permitido; `stop` e ajuste ao vivo durante a tentativa não são, e o competidor precisa
demonstrar isso (F-33).

## Fora do escopo desta versão

- **F-13 e F-34 — marcações e parada depois da chegada.** A spec §9 tira da v2; exige hardware
  novo (sensores laterais e canais de leitura). Consequência: com a v2 o carro não consegue volta
  válida na RoboCore. O risco previsto na spec §10 se confirmou; vira a primeira atividade da
  próxima versão.
- **F-33 — modo competição.** Exige decisão e mudança na spec §4.3/§4.5. Até lá, procedimento
  operacional: desconectar o BLE e entregar o celular ao juiz antes da tomada de tempo (manual do
  operador, WP3).
- **F-32 — ISR do encoder e PCNT.** Funciona com a regra da spec §4.4 (nenhuma escrita na NVS em
  RUNNING). Migrar para o PCNT fica para a próxima versão.
- **F-35 (parte) — autenticação BLE e nome único.** A spec não pede. A Tarefa 17 já põe o UUID do
  serviço no anúncio.
- **F-36 — tempo de leitura e arrasto entre canais.** Só se resolve com medição em bancada.
- **F-37 e F-38 — `Logger` e `VelocityProfile`.** Ficam no repositório sem uso; a remoção fica
  para uma limpeza posterior.
- Teste em hardware real e sintonia em pista (spec §9).

### Lacunas do plano WP1

Achados sem tarefa no plano, para o orquestrador decidir:

- **F-24** — pull-up do encoder esquerdo: acrescentar à Tarefa 18 (fiação) e conferir na Tarefa 19.
- **F-30** — versões fixas da plataforma e do NimBLE-Arduino: acrescentar à Tarefa 19.
- **F-33** — modo competição: decisão de spec.
- **Item 3** — medir a pilha das tasks (`uxTaskGetStackHighWaterMark`): acrescentar à Tarefa 18.
- **Spec** — §4.1 lista o GPIO 4 como pino de *strapping* (não é) e §3 fala em 13 suítes (são 12).
