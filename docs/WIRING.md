# WIRING.md — Diagrama de Fiação LFR v2 (ESP32)

Fiação da v2: bateria 2×18650 em suporte, fusível, proteção contra inversão e chave. Pinos de
`src/config.h`. Montagem passo a passo no [manual de montagem](manual/MONTAGEM.md).

## Visão Geral

```
Suporte 2×18650 (2S, 6,0–8,4 V)
    │
    ├── Fusível 3 A (lâmina mini, porta-fusível em linha)
    │
    ├── P-MOSFET de proteção contra inversão (recomendado)
    │
    └── Chave liga/desliga
            │
            ├── MP1584 buck ──► 5,0 V ──► ESP32 VIN ──► 3V3 ──► MCP3008 + QTR-8A + VCC do TB6612 + encoders
            │
            └── TB6612FNG VM (tensão do pack)
```

> ⚠️ **Ajuste o MP1584 para 5,0 V com a saída sem carga, antes de ligá-la ao trilho de 5 V.**
> O módulo sai de fábrica com tensão arbitrária e a ficha manda ajustar antes de ligar a carga
> (`hardware/components/fichas/07-mp1584.md`). Meça com o fio de saída solto e o ESP32 fora do
> perfboard. O trilho de 5 V alimenta só o VIN do ESP32 e o buzzer: o VCC (lógica) do TB6612
> fica no 3V3 (D17).

---

## Bateria e proteção

**Pack.** Duas células 18650 *high-drain* (≥ 20 A) num suporte plástico **em série** (2S):
8,4 V cheia, 7,2 V nominal (fichas 11 e 12; spec 2026-10-13, DEC-1). O firmware corta em
`vcut` = 6,4 V (fábrica, `src/config/RobotConfig.h`). Carregue as células fora do carro.

**Fusível de 3 A**, logo depois do suporte, antes de tudo. Dois N20 travados a 8,4 V somam pouco
mais de 2 A; o TB6612 aguenta 3,2 A de pico por canal (ficha 05). Um fusível de 3 A passa a
corrente de trabalho e abre num curto da fiação ou numa célula invertida. Nunca troque por um
maior.

**P-MOSFET de proteção contra inversão** (recomendado; ex.: AO3401). Com uma célula invertida
no suporte, o MOSFET não conduz e nada recebe tensão reversa. Ligação:

```
+BAT (suporte, depois do fusível) ── Dreno ┐
                                            P-MOSFET ── Fonte ──► carga (chave → MP1584 e VM)
GND ── 10 kΩ ── Gate ──────────────────────┘
               Zener de 9,1–10 V entre gate e fonte (protege o gate)
```

- Dreno no +BAT, fonte na carga, gate no GND por um resistor de 10 kΩ, zener de **9,1–10 V**
  entre gate e fonte.
- Por que 9,1–10 V: o zener tem que ficar acima dos 8,4 V do pack cheio (não conduz em operação
  normal) e abaixo do V_GS máximo do MOSFET. Um zener de 12 V nunca conduz com o pack e não
  protege um MOSFET de V_GS máximo ±12 V (avaliação do WP3, A-WP3-12).
- **Confira o V_GS máximo no datasheet do MOSFET comprado** (o ±12 V do AO3401 do exemplo está a
  confirmar). Se ele for menor que 10 V, escolha um zener abaixo dele e acima de 8,4 V.

**Chave liga/desliga** em série no positivo, depois do fusível e do MOSFET. Corrente de pelo
menos 2 A (`hardware/bom.csv`); como os motores travados passam disso, prefira uma chave com a
corrente do fusível (3 A). Fica no suporte da lateral esquerda da base.

---

## Pinagem ESP32 DevKit V1

### SPI → MCP3008

| ESP32 GPIO | MCP3008 | Descrição |
|-----------|---------|-----------|
| GPIO 13 (MOSI) | DIN (pin 11) | Dados para ADC |
| GPIO 12 (MISO) | DOUT (pin 12) | Dados do ADC — **pull-down de 10 kΩ para o GND** |
| GPIO 14 (CLK)  | CLK (pin 13) | Clock SPI |
| GPIO 15 (CS)   | CS/SHDN (pin 10) | Chip Select ativo-baixo |
| 3V3            | VDD + VREF (pin 16 + 15) | Alimentação e referência — **3,3 V, nunca 5 V** |
| GND            | GND (pin 9) e AGND | Terra |

**Configuração MCP3008:**
- Modo single-ended: 8 canais (CH0–CH7), um sensor por canal
- SPI @ 1 MHz, modo 0,0
- Resolução: 10 bits (0–1023)
- Capacitor de 100 nF entre VDD e AGND, junto ao CI (ficha 02)

> ⚠️ **MCP3008 a 3,3 V.** Alimentado em 5 V, o DOUT põe 5 V no GPIO 12, que não tolera 5 V e
> ainda é pino de *strapping* (ver abaixo). A 3,3 V não precisa de divisor no MISO
> (`docs/REVISAO_FIRMWARE.md`, item 1; `docs/DIVERGENCIAS.md`, D15).

### Motores → TB6612FNG

| ESP32 GPIO | TB6612FNG | Descrição |
|-----------|-----------|-----------|
| GPIO 25 (PWM-A) | PWMA | PWM motor esquerdo |
| GPIO 26 | AIN1 | Direção A bit 1 |
| GPIO 27 | AIN2 | Direção A bit 2 |
| GPIO 33 (PWM-B) | PWMB | PWM motor direito |
| GPIO 32 | BIN1 | Direção B bit 1 |
| GPIO 4  | BIN2 | Direção B bit 2 |
| GPIO 23 | STBY | Standby (HIGH = ativo) — pull-down de 10 kΩ: motores parados até o firmware liberar |
| 3V3 | VCC | Lógica do driver: 3,3 V do ESP32 (faixa 2,7–5,5 V, ficha 05). Não vai no 5 V do MP1584 (D17) |
| +BAT (depois da chave) | VM | Tensão do pack (6,0–8,4 V) — capacitor de 100 µF junto à placa |
| GND | GND | Terra |

PWM por LEDC: canais 0 (A) e 1 (B), 20 kHz, 10 bits (`src/config.h`). Se um motor girar ao
contrário ou os lados estiverem trocados, **não refaça a fiação**: ajuste `invL`, `invR` ou
`swap` pelo BLE (manual do operador, seção 7).

**Modos de operação TB6612:**

| IN1 | IN2 | PWM | Modo |
|-----|-----|-----|------|
| H | L | PWM | Forward |
| L | H | PWM | Reverse |
| H | H | – | Brake |
| L | L | – | Coast |

### N20 Motores + Encoders — Quadratura 4×

A classe `Encoder` (`src/sensors/Encoder.h`) lê os 2 canais via interrupt
`CHANGE` em ambos os pinos (4 transições por período do encoder). Alimente os encoders em
3,3 V (a ficha 04 aceita 2,7–18 V): assim nenhuma saída põe 5 V num GPIO (F-24).

| Encoder | ESP32 GPIO | Notas |
|---------|-----------|-------|
| Esq. CH-A | GPIO 18 | Pull-up interno (`INPUT_PULLUP` no firmware) |
| Esq. CH-B | GPIO 19 | Pull-up interno (`INPUT_PULLUP` no firmware) |
| Dir. CH-A | GPIO 36 (VP) | Input-only, **pull-up 10kΩ externo obrigatório** |
| Dir. CH-B | GPIO 39 (VN) | Input-only, **pull-up 10kΩ externo obrigatório** |

> ⚠️ **HARDWARE CHECKLIST OBRIGATÓRIO** — GPIO 36 e 39 são input-only RTC pins
> sem pull-up interno. `pinMode(36, INPUT_PULLUP)` é silenciosamente ignorado
> pelo HAL do Arduino-ESP32; o firmware usa `pinMode(36, INPUT)` para deixar
> isso explícito. **Sem o resistor 10kΩ externo entre o pino e VCC, os pinos
> ficam flutuando e o ISR dispara em ruído elétrico, "girando" o contador
> sozinho e destruindo a estabilidade do PID interno.**

```
VCC (3.3V) ─── 10kΩ ─── GPIO 36 ─── Encoder Dir CH-A
VCC (3.3V) ─── 10kΩ ─── GPIO 39 ─── Encoder Dir CH-B
```

Confira no teste de bancada `pio run -e bench_encoder -t upload`: a contagem só muda com a roda
girando.

### Controles e Sensores Auxiliares

| ESP32 GPIO | Componente | Notas |
|-----------|------------|-------|
| GPIO 0 | Botão START/CAL (BOOT da placa) | Pull-up interno; LOW = pressionado. **Não ligar o carro com ele apertado** (modo de gravação) |
| GPIO 22 | Buzzer passivo | Tom por LEDC, canal 2, 2,5 kHz (`src/config.h`), **por transistor** |
| GPIO 2 | LED status onboard | HIGH = aceso. Sem pull-up externo (pino de *strapping*) |
| GPIO 34 | Divisor VBAT | 100kΩ / 47kΩ → ESP32 (input-only), **100 nF para o GND** |

**Divisor de tensão VBAT:**
```
+BAT (depois da chave) ─── 100kΩ ─── GPIO34 ─── 47kΩ ─── GND
                                        └────── 100nF ─── GND
Tensão no GPIO = Vbat × 47 / (100+47) = Vbat × 0.32
2S cheia (8.4V)  → GPIO34 = 2.69V ✓ (< 3.3V, dentro do ADC com atenuação de 11 dB)
2S vazia (6.0V)  → GPIO34 = 1.92V
```

- Conferido para 8,4 V: 2,69 V fica dentro da faixa do ADC com atenuação de 11 dB
  (`VBAT_DIVIDER_RATIO` em `src/config.h`).
- O firmware lê com `analogReadMilliVolts()` (calibração do eFuse) × 147/47 × `vbk`, filtrado,
  a cada 100 ms. Afira `vbk` com um multímetro (manual do operador, seção 10).
- O capacitor de 100 nF no GPIO 34 estabiliza a leitura: o divisor tem impedância alta
  (`docs/REVISAO_FIRMWARE.md`, F-10).
- Ligue o divisor depois da chave, para ele não descarregar as células com o carro desligado.

**Buzzer:** acionado por PWM no LEDC, canal 2, a 2,5 kHz — funciona com buzzer passivo (o da
BOM) e com ativo. Um buzzer passivo direto no GPIO pode puxar mais corrente que o pino aguenta
(F-26), por isso vai num transistor:

```
5V ─── Buzzer ─── Coletor ┐
                          NPN
GPIO 22 ── R base ── Base ┘   Emissor ─── GND
```

O tipo do transistor e o valor do resistor de base ficam a definir na montagem.

### Pinos de *strapping*

Conclusão do `docs/REVISAO_FIRMWARE.md` (item 1). O ESP32 clássico tem cinco: GPIO 0, 2, 5, 12
(MTDI) e 15 (MTDO). O GPIO 4 **não** é um deles e o GPIO 5 não é usado.

| GPIO | Uso | Risco no boot | O que fazer |
|------|-----|---------------|-------------|
| 0 | Botão | Nível baixo no reset = modo de gravação: o firmware não roda | Não ligar o carro com o botão apertado |
| 2 | LED | Precisa ficar baixo ou flutuante para gravar; o LED da placa não atrapalha | Não pôr pull-up externo |
| 12 | MISO do MCP3008 | Nível alto no reset seleciona flash de 1,8 V e o WROOM-32 (3,3 V) não liga | MCP3008 a 3,3 V (o DOUT fica em alta impedância com o CS alto) e **pull-down de 10 kΩ** no GPIO 12. Se ainda falhar no boot: fixar o eFuse de VDD_SDIO em 3,3 V (`espefuse.py set_flash_voltage 3.3V`, **irreversível**) |
| 15 | CS do MCP3008 | Nível baixo só silencia o log da ROM | O pull-up interno mantém o MCP3008 desselecionado no boot e protege o GPIO 12 |
| 4 | BIN2 | Não é *strapping* | — (motores parados até `motors.begin()`; STBY com pull-down) |

---

## Array de Sensores — Pololu QTR-8A (analógico)

8 sensores em linha (passo 9,525 mm), saídas analógicas lidas pelo MCP3008.
A placa QTR-8A já traz os LEDs IR, fototransistores e pull-ups embutidos — basta
alimentar e ligar as 8 saídas ao ADC. **Não há resistores discretos a montar.**

```
QTR-8A  OUT1 OUT2 OUT3 OUT4 OUT5 OUT6 OUT7 OUT8
         │    │    │    │    │    │    │    │
MCP3008  CH0  CH1  CH2  CH3  CH4  CH5  CH6  CH7
              │  SPI  │
           ESP32 GPIO 12-15
```

| QTR-8A | Liga em | Notas |
|--------|---------|-------|
| VCC | **3.3V** | Casa com o VREF do MCP3008. **Una os dois pinos 3.3V BYPASS** do QTR-8A com um fio: os resistores dos LEDs são calibrados para 5 V em dois estágios e o *bypass* de um deles permite 3,3 V (Pololu 960; guia 0J12, seção "Vcc, GND, and 3.3V BYPASS"). Sem isso os LEDs ficam fracos e o contraste cai. Nunca una com o VCC em 5 V |
| GND | GND | terra comum |
| OUT1–OUT8 | MCP3008 CH0–CH7 | uma saída analógica por canal |
| LEDON | (deixar solto) | pull-up interno mantém os emissores ligados |

> ⚠️ **Alimente o QTR-8A em 3.3V, não 5V.** O MCP3008 opera com VDD/VREF = 3.3V;
> a 5V as saídas do array chegariam a ~5V e excederiam a tensão de entrada do ADC
> (máx VDD+0,3V), além de perder faixa útil. Em 3.3V a saída varia 0–3.3V → faixa
> cheia e segura.

> ℹ️ O passo físico do QTR-8A é 9,525 mm; `config.h` usa `SENSOR_SPACING_MM 10`.
> A diferença (~5%) é cosmética — apenas escala a magnitude do erro, absorvida pelo
> Kp e pela calibração min/max por canal (D5).

**Chicote.** Barra de pinos 8×2 ou 11×1 de 2,54 mm soldada por cima do QTR; chicote de 11 fios
AWG 30 (⌀ ≤ 0,6 mm, uma camada) pelo canal da base até o perfboard
(`hardware/cad/out/relatorio_montagem.md`, "Ligação do QTR-8A").

Saída analógica: tensão **alta** sobre preto (baixa reflexão), **baixa** sobre a
linha branca (alta reflexão). O firmware inverte a leitura conforme `pol` (1 = linha branca,
fábrica) e a classe `Calibration` aprende min/max por canal.

---

## Alimentação

```
Suporte 2×18650 ── Fusível 3 A ── P-MOSFET ── Chave ──┬── MP1584 buck ── 5V ──┬── ESP32 VIN ──► 3V3 pin ─┬─ MCP3008 VDD/VREF
                                                      │                       │                          ├─ QTR-8A VCC
                                                      │                       │                          ├─ TB6612FNG VCC (lógica)
                                                      │                       │                          └─ VCC dos 2 encoders
                                                      │                       └── Buzzer (por transistor)
                                                      │
                                                      ├── TB6612FNG VM (tensão do pack)
                                                      │
                                                      └── Divisor VBAT (100k / 47k) ── GPIO 34

Capacitores de desacoplamento:
  100µF × 1 → VM do TB6612 / GND (junto à placa)
  100nF × 4 → VCC / GND de cada IC (MCP3008, ESP32, TB6612FNG, MP1584)
  100nF × 1 → GPIO 34 / GND
```

**Consumo estimado:**

| Componente | Corrente |
|-----------|---------|
| ESP32 (WiFi off) | ~80 mA |
| 2× N20 em carga | ~400 mA |
| QTR-8A (8 emissores) | ~100 mA |
| MCP3008 | ~1 mA |
| **Total** | **~600 mA** |

A autonomia depende da capacidade das células escolhidas, que a ficha 11 não fixa. Meça em
pista e anote no `docs/tuning_log.csv`.

---

## Chave Liga/Desliga

Em série no positivo, depois do fusível e do P-MOSFET, antes de qualquer divisão de
alimentação. Fica no suporte da lateral esquerda da base, acessível sem desmontar nada.

---

## Diagrama ASCII

```
               ┌─────────────────────────────────────┐
               │          ESP32 DevKit V1             │
               │                                     │
         ┌─────┤ GPIO13 MOSI   GPIO25 PWM-A ├─────┐
         │     │ GPIO12 MISO   GPIO26 AIN1  │     │
    MCP3008     │ GPIO14 CLK    GPIO27 AIN2  │  TB6612FNG
    8ch ADC     │ GPIO15 CS     GPIO33 PWM-B │     │
         └─────┤               GPIO32 BIN1  ├─────┘
               │               GPIO 4 BIN2  │
               │               GPIO23 STBY  │
               │                            │
               │ GPIO18 ENC-L-A  GPIO 0 BTN │
               │ GPIO19 ENC-L-B  GPIO22 BUZ │
               │ GPIO36 ENC-R-A  GPIO 2 LED │
               │ GPIO39 ENC-R-B  GPIO34 BAT │
               └─────────────────────────────────────┘
                    │             │
               N20 Esq.       N20 Dir.
               + Encoder      + Encoder
```
