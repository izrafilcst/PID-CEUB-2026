# Guia de Leitura do Código — LFR (branch `main`)

> **Para quem é este guia:** o humano que vai abrir o firmware para entender o que
> acontece e **mexer nos parâmetros à mão** (na bancada ou na pista). Não é um
> tutorial de C++ — é um mapa de "onde as coisas moram" e "o que acontece quando".
>
> Se você só quer tunar, pule para **§4 (Onde ficam os botões)** e **§7 (Ajustes manuais)**.

---

## 1. Modelo mental em 30 segundos

O robô faz, ~centenas de vezes por segundo, este ciclo:

```
Onde está a linha?  →  Quanto devo corrigir o rumo?  →  Que velocidade cada roda precisa ter?
   (8 sensores)          (PID de posição)                  (2 PIDs de RPM, um por roda)
                                                                     ↓
                                                            PWM nos motores
```

Há **duas malhas de controle em cascata**:

1. **Malha externa (posição):** olha os sensores, decide "vira mais para a esquerda/direita".
   Saída = uma *correção*.
2. **Malha interna (velocidade):** transforma essa correção em um **alvo de RPM** para
   cada roda, e usa os *encoders* para garantir que cada roda realmente gire naquele RPM.

A malha interna é o que diferencia este robô de um seguidor de linha simples: em vez de
mandar "PWM cru" para o motor, ele mede a velocidade real da roda e a controla. O preço
disso é mais coisas para dar certo (encoders, timing, calibração).

---

## 2. Os dois núcleos (dual-core)

O ESP32 tem 2 núcleos. O projeto separa por criticidade:

| Núcleo | Arquivo/Função | O que faz | Cadência |
|--------|----------------|-----------|----------|
| **Core 0** | `loop()` em `src/main.cpp` | Sensores → PID → cascade → motores. **Tempo real.** | Livre (o mais rápido possível) |
| **Core 1** | `taskComm()` em `src/main.cpp` | BLE (tuning + telemetria), bateria, buzzer | ~100 Hz (poll de 10 ms) |

**Regra de ouro ao editar:** nada que demore (BLE, prints, esperas) pode entrar no Core 0.
O Core 1 conversa com o Core 0 só por duas estruturas compartilhadas, protegidas por
"muxes" (travas): `g_snap` (telemetria) e `lineFollowerMux` (parâmetros de tuning).

---

## 3. O pipeline de controle, passo a passo

Tudo abaixo é o `case RobotState::RUNNING:` dentro de `loop()` em `src/main.cpp`.
Siga na ordem — cada número é um estágio:

1. **Mede o tempo do ciclo (`dt`)** — quanto passou desde a última volta do loop.
   É a base de tempo para estimar RPM e para os termos I/D dos PIDs.
   *(É exatamente aqui que morava o bug de timing — ver §6.)*

2. **Atualiza a velocidade das rodas** — `velL.update(...)` / `velR.update(...)`
   (`src/sensors/VelocityEstimator.cpp`). Lê os pulsos do encoder acumulados e
   calcula RPM filtrado.

3. **Lê os 8 sensores de linha** — `sensors.readAll(raw)`
   (`src/sensors/SensorArray.cpp`). São 8 leituras via SPI no chip MCP3008.

4. **Decide a correção de rumo (PID externo)** — dentro de `lineFollower.update(...)`
   (`src/strategy/LineFollower.cpp`):
   - normaliza os sensores (calibração min/max por canal),
   - calcula a **posição da linha** (`weightedPosition`, faixa −3500…+3500),
   - roda o PID de posição → **correção**.

5. **Traduz correção para RPM** — a correção (em escala de PWM) vira uma correção de RPM:
   `setpoint da roda esquerda = base + correção`, `direita = base − correção`.

6. **Fecha a malha de velocidade (cascade)** — `cascade.compute(...)`
   (`src/control/CascadeController.cpp`): para cada roda, um PID interno compara o RPM
   *alvo* com o RPM *medido* e produz o PWM.

7. **Aplica o PWM** — `motors.setSpeed(...)` (`src/motors/MotorDriver.cpp`), que fala com
   o driver TB6612.

8. **Publica telemetria** — copia posição, correção, RPMs, setpoints etc. para `g_snap`,
   que o Core 1 envia via BLE a 30 Hz.

**Diagrama de arquivos** (quem chama quem):

```
main.cpp loop()
 ├─ VelocityEstimator ── Encoder            (RPM medido)
 ├─ SensorArray ─────── Calibration         (posição da linha)
 ├─ LineFollower ─────┬ Calibration
 │                    ├ PIDController        (PID externo de posição)
 │                    ├ SpeedProfile         (velocidade-base por erro)
 │                    └ DifferentialDrive
 ├─ CascadeController ─ 2× PIDController      (PID interno de RPM por roda)
 └─ MotorDriver                              (PWM final)
```

---

## 4. Onde ficam os botões (parâmetros de tuning)

Há **dois lugares** para mudar parâmetros: `src/config.h` (padrões, exige recompilar) e
**BLE ao vivo** (muda sem recompilar, mas volta ao padrão no reboot).

### 4.1 `src/config.h` — os padrões

| Parâmetro | O que faz | Efeito se aumentar |
|-----------|-----------|--------------------|
| `KP_DEFAULT` (3.0) | Ganho proporcional do rumo | Corrige mais forte; se demais, oscila |
| `KD_DEFAULT` (12.0) | Ganho derivativo do rumo | Amortece/antecipa curva; se demais, treme com ruído |
| `KI_DEFAULT` (0.0) | Ganho integral do rumo | Corrige viés constante; risco de windup (deixe 0) |
| `BASE_RPM_DEFAULT` (600) | Velocidade-alvo em reta | Mais rápido; menos margem em curva |
| `MAX_RPM_DEFAULT` (1200) | Teto do setpoint de RPM | Permite mais autoridade de correção |
| `KP_VEL_DEFAULT` (0.8) | Ganho P da malha de RPM | Roda persegue o alvo mais forte |
| `KD_VEL_DEFAULT` (0.05) | Ganho D da malha de RPM | Amortece a resposta de RPM |
| `ENCODER_DEFAULT_PPR_X4` (28) | Contagens por volta (padrão) | **Não chute** — calibre (§7.3) |
| `VELOCITY_FILTER_ALPHA` (0.3) | Suavização do RPM (0–1) | Menor = mais suave, mais atraso |
| `ERROR_THRESHOLD` (0.6) | Acima disso, reduz velocidade | Freia mais cedo nas curvas |

### 4.2 BLE ao vivo — o que dá para mudar sem recompilar

Do dashboard/celular você manda comandos que o `taskComm` aplica (ver `src/comm/BLETuner.cpp`
e o bloco de `if (ble.hasNew...())` em `main.cpp`):

- `{"t":"pid"}` → Kp/Ki/Kd do rumo
- `{"t":"spd"}` → velocidades (min/base/max/threshold, perfil legado em PWM)
- `{"t":"pidv"}` → Kp/Ki/Kd da malha de RPM
- `{"t":"rpm"}` → base/max RPM (com teto físico `RPM_HARD_CEILING`)
- `{"t":"cal"}` → inicia/termina calibração de encoder

> **Fluxo recomendado:** tune ao vivo via BLE até achar bom, depois **fixe os valores em
> `config.h`** para sobreviverem ao reboot. (A calibração de PPR é a exceção: é gravada em
> NVS e persiste sozinha.)

---

## 5. A máquina de estados (o que o robô está fazendo)

Definida por `enum class RobotState` e dirigida pelo botão (`PIN_BTN_START`) e pelo BLE:

```
IDLE ──botão──► CALIBRATING ──5 s──► READY ──botão/BLE──► RUNNING
  ▲                                    ▲                     │
  └──────────── RESET (BLE) ───────────┴──botão/BLE (STOP)───┘

RUNNING ──dtUs > 4 ms──► ERROR   (proteção: loop estourou o orçamento de tempo)
```

- **CALIBRATING (5 s):** passe o robô sobre a linha e o fundo para ele aprender o
  min/max de cada sensor. Sem isso, a posição sai errada.
- **READY:** parado, calibrado, esperando largada.
- **RUNNING:** seguindo a linha.
- **ERROR:** o loop passou de 4 ms — para os motores por segurança. Botão volta a IDLE.

O beep sinaliza cada transição (após a correção do plano, ele é **não-bloqueante**).

---

## 6. As armadilhas que o council achou (e onde estão)

Leia isto antes de "consertar" algo que parece estranho — pode ser um ponto já mapeado.

1. **`dt` truncado / perda de pulsos** *(corrigido pelo plano [2026-07-12](superpowers/plans/2026-07-12-timing-crossing-fixes.md))*
   O `dt` era calculado em **milissegundos inteiros**; num loop de <1 ms ele virava **0**,
   e o estimador de velocidade **descartava** os pulsos daquele ciclo. Resultado: RPM
   subestimado, pior em alta velocidade. → O plano passa `dt` **real em microssegundos** e
   faz o estimador **acumular** pulsos até uma janela mínima.

2. **Resolução do encoder (28 PPR).** Em janelas muito curtas você mede 0 ou 1 pulso —
   ruído. A "janela mínima" (`VELOCITY_MIN_WINDOW_US`) resolve isso acumulando tempo real
   suficiente antes de estimar. Se o RPM ficar ruidoso a baixa velocidade, **aumente** essa
   janela; se ficar lento para reagir, **diminua**.

3. **PIDs usavam `dt` fixo.** O PID externo assumia 0,01 s e os internos 0,002 s, mesmo com
   o loop rodando em outra taxa. → O plano injeta o `dt` real medido em cada `compute()`.
   ⚠️ *Efeito colateral do `dt` variável:* em ciclos muito curtos o termo **D** amplifica
   ruído. Se o robô tremer em reta, **reduza `KD`** (rumo) via BLE ou aumente a janela de
   velocidade.

4. **Corridas entre núcleos.** Retunar ganhos por BLE enquanto o robô corre podia colidir
   com o cálculo do cascade; a calibração (Core 1) podia colidir com a leitura de RPM
   (Core 0). → O plano estende a trava sobre o `cascade.compute()` e só permite calibrar com
   o robô **parado**.

5. **`beep()` bloqueante.** Ele fazia *busy-wait* de até 300–500 ms, travando o loop.
   → O plano troca por um buzzer não-bloqueante.

6. **Cruzamentos não eram tratados.** O array via uma linha perpendicular (interseção) como
   "linha muito à direita/esquerda" e dava um tranco. Havia um `isLineLost()` pronto mas
   **sem uso**. → O plano adiciona detecção de **cruzamento** (muitos sensores acesos → segue
   reto) e passa a **usar** o `isLineLost()` (linha perdida → mantém o rumo anterior para
   reencontrar a linha).

---

## 7. Ajustes manuais — receitas rápidas

### 7.1 O robô oscila (zigue-zague) em reta
- Baixe `KD` (rumo) se estiver tremendo com ruído; **suba** `KD` se ele "passa" da linha e
  volta (overshoot). Ajuste `KP` por último.
- Confira se a **calibração** (estado CALIBRATING) pegou bem o branco e o preto.

### 7.2 Ele sai voando nas curvas / perde a linha
- Baixe `BASE_RPM_DEFAULT` (ou `{"t":"rpm"}` base).
- Baixe `ERROR_THRESHOLD` para ele **frear mais cedo** ao detectar erro.
- Verifique o tratamento de cruzamento/linha-perdida (§6.6) se o problema é em interseções.

### 7.3 O RPM lê errado / a malha interna não segura a velocidade
1. **Calibre o encoder** (o padrão 28 quase certamente está errado para a sua roda):
   - `{"t":"cal"}` START, gire a roda **N voltas exatas** à mão, `{"t":"cal"}` FINISH com N.
   - O PPR efetivo é gravado em NVS e sobrevive ao reboot.
2. Confirme na bancada que, girando a roda **para frente**, o RPM sai **positivo**. Se sair
   negativo, os canais A/B do encoder estão trocados (inverta na fiação) — senão o PID
   "briga" consigo mesmo.
3. Se o RPM estiver ruidoso, aumente `VELOCITY_MIN_WINDOW_US` ou diminua `VELOCITY_FILTER_ALPHA`.

### 7.4 Sinais e convenções (para não inverter as coisas)
- **Posição:** negativa = linha à esquerda, positiva = à direita (faixa ±3500).
- **Correção > 0** → vira à direita (roda esquerda mais rápida).
- **PWM > 0** → frente; **< 0** → ré (o `MotorDriver` decide a direção pelos pinos).
- **Linha:** branca sobre fundo preto → sensor sobre a linha lê **alto**.

### 7.5 Regra de segurança ao mexer no Core 0
Qualquer coisa nova dentro do `case RUNNING` precisa caber no orçamento de **< 4 ms** (senão
cai em ERROR). Não coloque BLE, `Serial.print`, `delay()` nem esperas ali. Telemetria e
comunicação vão para o Core 1 via `g_snap`.

---

## 8. Referência rápida de arquivos

| Quero mexer em… | Abra |
|-----------------|------|
| Pinos, constantes, ganhos padrão | `src/config.h` |
| Orquestração, máquina de estados, os 2 cores | `src/main.cpp` |
| Como a posição da linha é calculada | `src/sensors/Calibration.cpp` |
| Leitura dos sensores (SPI/MCP3008) | `src/sensors/SensorArray.cpp` |
| Contagem de encoder (quadratura, ISR) | `src/sensors/Encoder.cpp` |
| RPM a partir do encoder | `src/sensors/VelocityEstimator.cpp` |
| PID genérico (P, I, D, anti-windup) | `src/control/PIDController.cpp` |
| Malha em cascata (posição → RPM) | `src/control/CascadeController.cpp` |
| Perfil de velocidade por erro | `src/control/SpeedProfile.cpp` |
| Pipeline sensor→PID→motor | `src/strategy/LineFollower.cpp` |
| Cronômetro de volta | `src/strategy/LapTimer.cpp` |
| Driver de motor (PWM/TB6612) | `src/motors/MotorDriver.cpp` |
| BLE (tuning + telemetria) | `src/comm/BLETuner.cpp` |
| Persistência (PPR em NVS) | `src/storage/NvsStore.cpp` |

**Testes** (rodam sem hardware): `pio test -e native`. Cada módulo de lógica tem uma suíte
em `test/test_<módulo>/` — leia os testes quando quiser entender o comportamento esperado de
um módulo sem decifrar a implementação.
