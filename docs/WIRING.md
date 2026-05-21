# WIRING.md — Diagrama de Fiação LFR ESP32

## Visão Geral

```
LiPo 2S 7.4V
    │
    ├─── MP1584 buck ──► 5V ──► ESP32 VIN
    │                         └► MCP3008 VDD / VREF
    │
    └─── TB6612FNG VM (7.4V direto)
```

---

## Pinagem ESP32 DevKit V1

### SPI → MCP3008

| ESP32 GPIO | MCP3008 | Descrição |
|-----------|---------|-----------|
| GPIO 13 (MOSI) | DIN (pin 11) | Dados para ADC |
| GPIO 12 (MISO) | DOUT (pin 12) | Dados do ADC |
| GPIO 14 (CLK)  | CLK (pin 13) | Clock SPI |
| GPIO 15 (CS)   | CS/SHDN (pin 10) | Chip Select ativo-baixo |
| 3V3            | VDD + VREF (pin 16 + 15) | Alimentação e referência |
| GND            | GND (pin 9) | Terra |

**Configuração MCP3008:**
- Modo single-ended: 8 canais (CH0–CH7), um sensor por canal
- SPI @ 1 MHz, modo 0,0
- Resolução: 10 bits (0–1023)

### Motores → TB6612FNG

| ESP32 GPIO | TB6612FNG | Descrição |
|-----------|-----------|-----------|
| GPIO 25 (PWM-A) | PWMA | PWM motor esquerdo |
| GPIO 26 | AIN1 | Direção A bit 1 |
| GPIO 27 | AIN2 | Direção A bit 2 |
| GPIO 33 (PWM-B) | PWMB | PWM motor direito |
| GPIO 32 | BIN1 | Direção B bit 1 |
| GPIO 4  | BIN2 | Direção B bit 2 |
| GPIO 23 | STBY | Standby (HIGH = ativo) |
| 5V | VCC | Lógica do driver |
| LiPo+ | VM | Motor supply (7.4V) |
| GND | GND | Terra |

**Modos de operação TB6612:**

| IN1 | IN2 | PWM | Modo |
|-----|-----|-----|------|
| H | L | PWM | Forward |
| L | H | PWM | Reverse |
| H | H | – | Brake |
| L | L | – | Coast |

### N20 Motores + Encoders Hall — Quadratura 4×

A classe `Encoder` (src/sensors/Encoder.h) lê os 2 canais via interrupt
`CHANGE` em ambos os pinos (4 transições por período do encoder).

| Encoder | ESP32 GPIO | Notas |
|---------|-----------|-------|
| Esq. CH-A | GPIO 18 | Pull-up interno (INPUT) |
| Esq. CH-B | GPIO 19 | Pull-up interno (INPUT) |
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

### Controles e Sensores Auxiliares

| ESP32 GPIO | Componente | Notas |
|-----------|------------|-------|
| GPIO 0 | Botão START/CAL | Pull-up interno; LOW = pressionado |
| GPIO 22 | Buzzer passivo | PWM tone para feedback |
| GPIO 2 | LED status onboard | HIGH = aceso |
| GPIO 34 | Divisor VBAT | 100kΩ / 47kΩ → ESP32 (input-only) |

**Divisor de tensão VBAT:**
```
LiPo+ ─── 100kΩ ─── GPIO34 ─── 47kΩ ─── GND
Tensão no GPIO = Vbat × 47 / (100+47) = Vbat × 0.32
2S full (8.4V) → GPIO34 = 2.69V ✓ (< 3.3V)
2S empty (6.0V) → GPIO34 = 1.92V
```

---

## Array de Sensores TCRT5000

8 sensores em linha, espaçamento 10 mm, conectados ao MCP3008:

```
CH0  CH1  CH2  CH3  CH4  CH5  CH6  CH7
 S0   S1   S2   S3   S4   S5   S6   S7
 │    │    │    │    │    │    │    │
[──────────── MCP3008 ────────────────]
              │  SPI  │
           ESP32 GPIO 12-15
```

**Cada TCRT5000:**
```
VCC (5V) ─── 220Ω ─── LED (pino 1)
GND ─────────────── LED (pino 2)
VCC (5V) ─── 10kΩ ─── Coletor (pino 3) ─── MCP3008 CHx
                       Emissor (pino 4) ─── GND
```

Saída: HIGH (pista preta / baixa reflexão), LOW (linha branca / alta reflexão).

---

## Alimentação

```
LiPo 2S 7.4V ──┬── MP1584 buck converter
               │         │
               │         └── 5V ──── ESP32 VIN
               │               └─── MCP3008 VDD
               │               └─── TCRT5000 VCC (via 220Ω)
               │
               └── TB6612FNG VM (direto 7.4V)

Capacitores de desacoplamento:
  100µF × 1 → LiPo+ / GND (perto do TB6612)
  100nF × 4 → VCC / GND de cada IC
```

**Consumo estimado:**

| Componente | Corrente |
|-----------|---------|
| ESP32 (WiFi off) | ~80 mA |
| 2× N20 em carga | ~400 mA |
| TCRT5000 × 8 | ~120 mA |
| MCP3008 | ~1 mA |
| **Total** | **~600 mA** |

LiPo 800mAh / 600mA ≈ **~8 min** de autonomia em corrida.

---

## Chave Liga/Desliga

Inserir chave SPDT em série no positivo do LiPo, antes de qualquer divisão de alimentação. Posicionar acessível no chassis sem precisar abrir a carroceria.

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
