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
| Sensores | Pololu QTR-8A — 8 canais IR refletivo analógico via MCP3008 (SPI) |
| Motores | 2× N20 600RPM 6V com encoder magnético 12 CPR |
| Driver | TB6612FNG dual H-bridge |
| Bateria | 2× 18650 high-drain em suporte plástico (2S: 8,4 V cheia, 7,2 V nominal), fusível 3 A |
| Chassi | PETG impresso 3D, modelo paramétrico build123d em `hardware/cad/` (STL + STEP) |
| Rodas e apoio | Cubo PETG + O-ring de silicone 34 × 3 mm; 2 patins PETG sob a barra de sensores |
| Regulador | MP1584 buck → 5V |

---

## Arquitetura do Software

```
src/
├── main.cpp                  # Só fiação: task control (core 1) e task comm (core 0)
├── config.h                  # Pinagem e constantes de hardware (sem ganhos)
│
├── config/
│   ├── RobotConfig.h/.cpp    # HardwareConfig + TuningProfile, faixas, 3 perfis de fábrica
│   └── ConfigStore.h/.cpp    # Blobs na NVS com magic, versão e CRC32
│
├── sensors/
│   ├── SensorArray.h/.cpp        # Leitura MCP3008 via SPI
│   ├── Calibration.h/.cpp        # Min/max por canal, polaridade da linha, validação
│   ├── Encoder.h/.cpp            # Quadratura 4× (ISR ESP32)
│   ├── VelocityEstimator.h/.cpp  # RPM filtrado, dt em µs, PPR por auto-calibração
│   ├── ButtonInput.h/.cpp        # Debounce, toque curto/longo, aperto (E-3)
│   └── BatteryMonitor.h/.cpp     # Tensão filtrada, % por química, corte com retenção
│
├── control/
│   ├── PIDController.h/.cpp      # dt real, filtro no D, reset(medida), anti-windup
│   ├── CascadeController.h/.cpp  # Feedforward + PID por roda, em fração [-1, +1]
│   ├── Mixer.h                   # base ± correção com prioridade de direção
│   ├── SpeedProfile.h/.cpp       # vmax/vmin/thr em fração, rampa acc
│   └── VelocityProfile.h/.cpp    # Não integrado na v2
│
├── motors/
│   ├── MotorDriver.h/.cpp    # Abstração TB6612FNG + PWM LEDC
│   └── OutputStage.h/.cpp    # Inversão/troca de canais, compensação de tensão, duty
│
├── strategy/
│   ├── ControlLoop.h/.cpp        # Um passo de controle inteiro, testável em native
│   ├── RobotStateMachine.h/.cpp  # IDLE/CALIBRATING/READY/ARMED/RUNNING/ERROR(1–5)
│   ├── LineFollower.h/.cpp       # Erro normalizado + PID de posição, linha perdida
│   └── LapTimer.h/.cpp           # Cronômetro de volta (sem detecção de marcação na v2)
│
├── comm/
│   ├── BLETuner.h/.cpp           # Transporte NimBLE: fila de comandos e notificações
│   ├── CommandProtocol.h/.cpp    # JSON → comandos tipados; frames ack/cfg/tel/sta/info
│   ├── CommandHandler.h/.cpp     # Regras de set/save/load/defaults/prof, modo competição
│   ├── CommandQueue.h/.cpp       # Fila callback NimBLE → task comm
│   ├── Mailbox.h                 # Cópia versionada entre núcleos
│   ├── Annunciator.h/.cpp        # LED e buzzer sem bloquear
│   └── Logger.h/.cpp             # Sem uso na v2 (F-37)
│
└── storage/                  # IPersistentStore: NvsStore, InMemoryStore, NvsArbiter

test/                         # 23 suítes Unity (native); test_sensors só tem .gitkeep
├── test_annunciator/  test_battery/        test_ble_tuner/      test_button/
├── test_calibration/  test_cascade/        test_command_handler/ test_command_protocol/
├── test_config_store/ test_control_loop/   test_encoder/        test_lap_timer/
├── test_line_follower/ test_nvs_arbiter/   test_output_stage/   test_periodic_timer/
├── test_pid/          test_robot_config/   test_smoke/          test_speed_profile/
└── test_state_machine/ test_velocity_estimator/ test_velocity_profile/

examples/                     # 6 testes de bancada, um ambiente bench_* cada (platformio.ini)

hardware/
├── cad/                      # build123d: params.py, medidas.toml, build.py, parts/, tests/
│   └── out/                  # STL, STEP e relatorio_montagem.md
├── components/               # specs.py + 13 fichas geradas
├── drawings/                 # Pranchas SVG (A1/A2 da v1, históricas)
└── bom.csv                   # Bill of Materials com links e preços

docs/
├── manual/                   # MONTAGEM.md, OPERACAO.md, MANUTENCAO.md (+ manual-lfr.html)
├── WIRING.md                 # Fiação completa
├── TUNING.md                 # Sintonia e procedimento de competição
├── DIVERGENCIAS.md           # Divergências entre fontes
├── REVISAO_FIRMWARE.md       # Revisão do firmware v2 e regulamento
└── dashboard/                # lfr-cockpit-mock.html (Web Bluetooth) + simulador
```

---

## Regras de Competição (Referência)

- **Dimensões máx**: 250 × 250 × 200 mm
- **Linha**: branca 19±1 mm sobre manta preta
- **Pista**: retas + curvas + cruzamentos, ~5 × 2.8 m
- **Tentativas**: 3 de 3-5 min, menor tempo vence
- **Autonomia**: 100% autônomo
- **Volta válida (RoboCore 5.7)**: o robô para sozinho na área de partida-chegada por ≥ 2 s. A v2
  não faz isso (F-34 em `docs/REVISAO_FIRMWARE.md`): é a primeira atividade da próxima versão

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
- **Loop de controle**: período de 2000 µs (500 Hz, `vTaskDelayUntil`); 5 ciclos seguidos acima
  disso → ERROR(1)
- **Leitura sensores**: 8 canais × 24 bits a 1 MHz = 192 µs só de clock (F-36); medir na bancada
- **PWM motores**: 20 kHz, 10 bits (LEDC canais 0 e 1); buzzer no canal 2 a 2,5 kHz
- **Core 1**: task `control` — sensores, PID, motores, estados, botão, LED/buzzer (tempo real)
- **Core 0**: task `comm` — BLE (junto da pilha NimBLE), comandos e telemetria
- **NVS**: nenhuma escrita em ARMED/RUNNING

---

## Parâmetros de Tuning

Não há mais ganhos no `config.h`. Os valores de fábrica são os inicializadores de
`src/config/RobotConfig.h` (`HardwareConfig` e `TuningProfile`, 3 perfis que diferem em `vmax`),
com faixas em `src/config/RobotConfig.cpp`. Tudo é ajustável por BLE (`{"t":"set",...}`) e só
grava na NVS com `save`. Erro e saídas dos PIDs são normalizados em [−1, +1] (DEC-13). Tabela
completa e uso: `docs/manual/OPERACAO.md`, seções 5 e 6.

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
  └─ MotorDriver → OutputStage → SensorArray (real)

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

## BOM Resumida (v2: ~R$325 estimado + QTR-8A importado + itens sem preço)

Fonte: `hardware/bom.csv`.

| Componente | Preço |
|-----------|-------|
| ESP32 DevKit V1 | R$45 |
| MCP3008 ADC (a 3,3 V) | R$25 |
| QTR-8A array 8ch (Pololu #960) | US$12.95 + importação |
| 2× N20 600RPM encoder (par) | R$80 |
| TB6612FNG módulo | R$18 |
| MP1584 buck | R$8 |
| Suporte 2×18650 | R$5 (estimado) |
| 2× célula 18650 high-drain | R$60 (estimado) |
| 4× O-ring 34 × 3 | R$4 (estimado) |
| PETG filamento | R$25 |
| Parafusos (lista do relatório de montagem) | R$10 (estimado) |
| Fios | R$15 |
| Capacitores, resistores, buzzer, LED | R$11 |
| Chave SPDT + conector da bateria | R$5 |
| Perfboard 5×7 | R$6 |
| Fita VHB | R$8 |
| Carregador 18650, insertos M3, fusível 3 A, P-MOSFET, transistor, fio AWG 30, barra de pinos | a confirmar |
| **TOTAL** | **~R$325 + QTR-8A + itens a confirmar** |

---

## Links de Referência

- [Semreh V2 — Campeão brasileiro (UFABC/Tamandutech)](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
- [ESP32 Line Follower — RoboChallenge 2024](https://github.com/dandominicstaicu/esp32-line-follower)
- [PID Line Follower com ESP32](https://github.com/felipemmattia/PID_line_follower_robot)
- [Regras RoboCore — Seguidor de Linha](https://robocore-eventos.s3.sa-east-1.amazonaws.com/public/Regras+-+Seguidor+de+Linha.pdf)
- [PID Tuning Guide — Zbotic](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [Pololu Micro Metal Gearmotors](https://www.pololu.com/category/60/micro-metal-gearmotors)
