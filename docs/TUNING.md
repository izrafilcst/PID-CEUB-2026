# TUNING.md — Procedimento de Ajuste PID

## Visão Geral

O ajuste fino do PID é a diferença entre um robô que segue a linha e um campeão. Este documento descreve o procedimento sistemático de calibração, desde o zero absoluto até o modo competição.

---

## 1. Antes de Começar

### Checklist de hardware
- [ ] Bateria carregada (> 7.0 V em 2S LiPo)
- [ ] Sensores a **4 mm** do chão (verificar com calibre)
- [ ] Rodas limpas, O-rings sem desgaste
- [ ] Pino STBY habilitado (fio solto = motores mortos)
- [ ] Encoder: pulsos chegando no osciloscópio ou Serial (teste rápido)
- [ ] Dashboard BLE conectado e telemetria fluindo

### Parâmetros de partida (config.h)
```cpp
KP_DEFAULT   = 3.0f
KI_DEFAULT   = 0.0f
KD_DEFAULT   = 12.0f
BASE_SPEED   = 160      // 0-255
MAX_SPEED    = 230
MIN_SPEED    = 60
THRESHOLD    = 0.6f
```

---

## 2. Calibração dos Sensores

### Procedimento obrigatório antes de qualquer tuning
1. Posicionar o robô sobre a pista (linha visível pelos sensores)
2. Pressionar BTN (GPIO 0) — estado muda para `CALIBRATING`
3. Mover o robô **lentamente** de um lado ao outro da linha durante 5 s
4. O LED pisca quando a calibração captura extremos satisfatórios
5. Pressionar BTN novamente → estado `READY`

### Verificar no dashboard
- Todos os 8 sensores devem variar entre 0–1000 (normalizado)
- Sensor sobre linha branca: valor alto (> 700)
- Sensor sobre pista preta: valor baixo (< 200)
- Se algum sensor não varia → checar conexão SPI / MCP3008

---

## 3. Método Ziegler-Nichols Modificado

### Passo 1 — Encontrar Ku (ganho crítico de oscilação)

1. Zerar Ki e Kd via BLE slider (Ki=0, Kd=0)
2. Começar com Kp = 1.0
3. Incrementar Kp em 0.5 a cada tentativa
4. Observar o robô na pista:
   - Kp baixo: robô lento para corrigir, sai da linha em curvas
   - Kp correto: robô oscila com amplitude constante em reta
   - Kp alto demais: oscilação cresce → sai da linha
5. Anotar o Kp onde oscila sem crescer → esse é **Ku**
6. Medir o período de oscilação no gráfico de posição → **Tu** (em ms)

### Passo 2 — Calcular parâmetros iniciais

| Controlador | Kp        | Ki          | Kd         |
|-------------|-----------|-------------|------------|
| P           | 0.50 × Ku | —           | —          |
| PD          | 0.80 × Ku | —           | 0.125 × Tu |
| PID clássico| 0.60 × Ku | 1.2/Tu      | 0.075 × Tu |
| PID agressivo| 0.70 × Ku | 1.75/Tu    | 0.105 × Tu |

> Para LFR: começar com **PD** (Ki=0). Adicionar Ki só se houver erro estático persistente.

### Passo 3 — Ajuste fino iterativo

```
Iteração 1: definir Kp e Kd por Ziegler-Nichols PD
Iteração 2: aumentar velocidade base em 10 pontos → re-testar
Iteração 3: se oscila em reta → aumentar Kd 10%
Iteração 4: se lento em curva → reduzir MIN_SPEED ou THRESHOLD
Iteração 5: se perde linha em S → aumentar Kp 15% ou reduzir MAX_SPEED
```

---

## 4. Guia de Sintomas e Correções

| Sintoma | Causa provável | Ação |
|---------|---------------|------|
| Oscila em reta mesmo em baixa velocidade | Kp alto demais | Reduzir Kp 20% |
| Não segue curva fechada | Kp baixo, MIN_SPEED alto | Aumentar Kp, reduzir MIN_SPEED |
| Atraso visível na resposta | Kd baixo ou loop > 2 ms | Aumentar Kd, verificar timing |
| Erro estático (desvia da linha gradualmente) | Ki = 0 com atrito assimétrico | Adicionar Ki = 0.01–0.05 |
| Perde linha no cruzamento | SpeedProfile agressivo | Aumentar THRESHOLD, reduzir MAX_SPEED |
| Vibração nos motores em baixa velocidade | PWM < deadband | Aumentar MIN_SPEED |
| Sai pela esquerda sistematicamente | Sensores descalibrados | Recalibrar; verificar altura |
| Loop > 2 ms | SPI lento ou cálculo pesado | Verificar SPI_FREQ, checar profiler |

---

## 5. SpeedProfile — Ajuste de Velocidade Adaptativa

O `SpeedProfile` reduz velocidade proporcionalmente ao erro absoluto:

```
erro em [0, THRESHOLD]  →  velocidade em [MAX_SPEED, BASE_SPEED]
erro > THRESHOLD        →  velocidade = MIN_SPEED (curva fechada)
```

### Estratégia por estágio de competição

| Fase | BASE | MAX | MIN | THRESHOLD | Objetivo |
|------|------|-----|-----|-----------|----------|
| Aprendizado | 120 | 160 | 60 | 0.8 | Completar percurso |
| Retas | 160 | 200 | 70 | 0.6 | Tempo competitivo |
| Curvas | 150 | 190 | 55 | 0.5 | Precisão nas curvas |
| Competição | 170 | 230 | 60 | 0.55 | Máximo desempenho |

---

## 6. Log de Sessões de Tuning

Registrar cada sessão na planilha `docs/tuning_log.csv`:

```
data,kp,ki,kd,base_speed,max_speed,min_speed,threshold,melhor_volta_s,observacoes
```

---

## 7. Procedimento de Competição (Dia D)

### 30 minutos antes
1. Calibrar sensores na pista oficial (mesma iluminação)
2. Verificar tensão da bateria (7.2–8.4 V)
3. Rodar 3 voltas em modo conservador (BASE=140, MAX=180)
4. Anotar linha de base

### Progressão nas 3 tentativas
- **Tentativa 1**: configuração conservadora → garantir completar
- **Tentativa 2**: aumentar BASE+10, MAX+15 → buscar melhor tempo
- **Tentativa 3**: máximo desempenho configurado em sessão prévia

### Emergências
- **Bateria caindo**: reduzir MAX_SPEED em 20 pts (tensão baixa = torque baixo)
- **Piso escorregadio**: aumentar Kd 15%, reduzir MAX_SPEED
- **Iluminação diferente**: recalibrar sensores obrigatoriamente

---

## 8. Referências

- [Ziegler-Nichols PID Tuning — Wikipedia](https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method)
- [PID Tuning Guide — Zbotic (LFR específico)](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [Semreh V2 — Parâmetros campeão brasileiro](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
