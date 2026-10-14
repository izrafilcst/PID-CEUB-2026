# TUNING.md — Procedimento de Ajuste (firmware 4.0.0)

## Visão Geral

Desde o firmware 4.0.0 todo parâmetro de sintonia é ajustado **por BLE, sem recompilar**, e os
ganhos são **normalizados** (spec §4.3, DEC-13):

- o erro de posição vai de −1 (linha toda à esquerda) a +1 (toda à direita);
- a saída do PID de posição e as velocidades são frações de escala em [−1, +1]: no modo PWM
  direto, 1,0 = duty máximo (1023); no modo cascata, 1,0 = `rpmMax`.

Por isso os ganhos não dependem da resolução do PWM, do número de sensores nem do modo de
controle. Os valores de fábrica são **ponto de partida para a pista**, não valores sintonizados.

Ajuste ao vivo **não** persiste: só o botão **Salvar** do dashboard (comando `save`) grava na
NVS, e só com o carro parado. Há 3 perfis (0 "seguro", 1 "treino", 2 "corrida") que diferem,
de fábrica, só em `vmax` (0,35 / 0,50 / 0,70).

---

## 1. Antes de Começar

### Checklist
- [ ] Bateria 2× 18650 (2S) acima de 7,0 V; o corte padrão (`vcut`) é 6,4 V
- [ ] Sensores a **3 mm** do chão (patins apoiados)
- [ ] Rodas limpas, O-rings sem desgaste
- [ ] Dashboard conectado (botão **CONECTAR BLE**) e telemetria fluindo
- [ ] Sentido dos motores conferido no `bench_differential`; se um lado girar ao contrário,
      ajustar `invL`/`invR` (ou `swap`) pela aba CONFIG — não mexer na fiação
- [ ] `info` sem aviso `cfgerr` (senão a configuração salva foi descartada e o carro está nos
      valores de fábrica). O `info` chega quando o dashboard se inscreve nas notificações e a
      cada `get`; a aba SISTEMA mostra também a folga de pilha (`stk`)

### Parâmetros de partida (fábrica, perfil 0)

| Chave | Valor | Chave | Valor |
|-------|-------|-------|-------|
| `mode` | 0 (PWM direto) | `vmax` | 0,35 |
| `kp` | 1,0 | `vmin` | 0,25 |
| `ki` | 0,0 | `thr` | 0,6 |
| `kd` | 0,03 s | `acc` | 2,0 /s |
| `dlp` | 0,5 | `lostMs` | 300 ms |
| `kpv` | 0,002 | `kiv` | 0,005 |

Tabela completa, com faixas e unidades: README, seção "Parâmetros em runtime".

---

## 2. Calibração dos Sensores

### Procedimento obrigatório antes de qualquer sintonia
1. Posicionar o robô sobre a pista, com a barra sobre a linha
2. **Toque longo** no botão (segurar 1,5 s) em IDLE ou PRONTO — ou **Calibrar sensores** no
   dashboard (`scal`). O LED pisca a 5 Hz (CALIBRANDO)
3. Mover o robô **devagar** de um lado ao outro da linha durante os 5 s da janela
4. Ao fim, o firmware confere a faixa de cada canal:
   - todos com max − min ≥ 200 contagens → PRONTO e a calibração é **salva sozinha** na NVS;
   - algum canal estreito → ERRO(4), e o dashboard mostra "Canais sem contraste: …"

### Verificar no dashboard
- Os 8 sensores variam de 0 a 100 % (já com a polaridade `pol` aplicada)
- Sensor sobre a linha: valor alto (> 70 %); sobre o fundo: baixo (< 20 %)
- Linha branca é `pol` = 1 (fábrica). Pista com linha preta: `pol` = 0
- Barra montada espelhada (linha à direita aparece à esquerda): `sinv` = 1
- Um canal que não varia → conferir SPI/MCP3008 com o `bench_sensor_raw`

---

## 3. Sintonia da Malha de Posição (Ziegler–Nichols)

Trabalhar no **perfil 0 "seguro"** (`vmax` 0,35) e no modo PWM direto (`mode` 0).

1. Zerar `ki` e `kd` (aba CONFIG ou sliders da aba PID).
2. Subir `kp` de **0,5 em 0,5** até o robô oscilar em reta com amplitude constante: esse é o
   ganho crítico **Ku**. Medir o período da oscilação no gráfico de `pos` → **Tu** (em
   segundos).
3. Aplicar o PD de Ziegler–Nichols:
   - `kp = 0,6 · Ku`
   - `kd = 0,075 · Ku · Tu` (em segundos)
   - `ki = 0`
4. Se o termo D ficar ruidoso (correção tremida no gráfico de `corr`), ajustar `dlp` entre
   0,3 e 0,7 (menor = mais filtro, mais atraso; 1 = sem filtro).
5. Subir `vmax` de **0,05 em 0,05** por sessão. Para as curvas, baixar `vmin` (velocidade com
   erro ≥ `thr`) ou baixar `thr` (a redução começa mais cedo). `acc` limita a subida da
   velocidade na saída das curvas; a descida é imediata.
6. `ki` só se houver erro de regime persistente (desvio constante em reta): começar com 0,1.

### Ajuste fino

| Sintoma | Causa provável | Ação |
|---------|---------------|------|
| Oscila em reta mesmo devagar | `kp` alto | Reduzir `kp` 20 % |
| Não fecha curva | `kp` baixo ou `vmin` alto | Subir `kp` ou baixar `vmin` |
| Resposta atrasada | `kd` baixo | Subir `kd` 10 % |
| Correção tremida | ruído no termo D | Baixar `dlp` |
| Erro de regime em reta | atrito assimétrico | `ki` 0,1–0,5 |
| Perde a linha no cruzamento | perfil agressivo | Subir `thr`, baixar `vmax` |
| Para em ERRO(2) ao sair da curva | linha perdida mais que `lostMs` | Baixar `vmax`/`vmin`; subir `lostMs` com cuidado |
| ERRO(1) | laço passou de 2000 µs por 5 ciclos | Ver `ovr` no frame `sta`; relatar |
| ERRO(3) | bateria abaixo de `vcut` por 1 s | Trocar as células; conferir `vbk` com multímetro |

---

## 4. Modo Cascata (encoders)

Só depois de calibrar **os dois** encoders: com o carro parado, `{"t":"cal","op":"start",
"side":"L"}`, girar a roda esquerda 10 voltas à mão e `{"t":"cal","op":"finish","side":"L",
"rot":10}`; repetir com `"side":"R"`. O PPR medido aparece no `ecal` e passa a valer (`pprL`,
`pprR`). O padrão de fábrica (600) é provisório.

1. `mode` = 1 (só com o carro parado).
2. Começar com `kpv` 0,002 e `kiv` 0,005; `kdv` 0.
3. `rpmMax` (700) é o RPM que vale a fração 1,0. Conferir no gráfico que `rpmL`/`rpmR`
   acompanham `spL`/`spR` em reta.
4. Os ganhos externos (`kp`, `kd`) continuam valendo: a escala é a mesma nos dois modos.

---

## 5. Salvar e Registrar

- Ao fim de cada sessão boa: **Salvar** no dashboard (grava `HardwareConfig` + perfil ativo).
- **Recarregar** descarta o que não foi salvo; **Fábrica** carrega os valores de fábrica ao
  vivo (não grava).
- Trocar de perfil com mudança pendente pede confirmação: salvar antes ou descartar.
- Registrar a sessão em `docs/tuning_log.csv`:

```
data,perfil,mode,kp,ki,kd,dlp,vmax,vmin,thr,acc,kpv,kiv,melhor_volta_s,observacoes
```

---

## 6. Procedimento de Competição (Dia D)

### 30 minutos antes
1. Calibrar os sensores na pista oficial (mesma iluminação)
2. Conferir a tensão no dashboard (`vb`, 7,2–8,4 V)
3. Rodar 3 voltas no perfil 1 "treino"
4. Anotar a linha de base

### Progressão nas 3 tentativas
- **Tentativa 1**: perfil conservador → garantir completar
- **Tentativa 2**: `vmax` + 0,05 → buscar tempo
- **Tentativa 3**: perfil 2 "corrida", sintonizado em sessão prévia

Ajustes e troca de perfil só **entre** tentativas (regra RoboCore 3.1.1). Durante a tentativa,
nenhum comando remoto pode parar o robô nem mudar parâmetro (regra 5.9). Para isso:

1. Com o carro parado, ligar o interruptor **Modo competição** na aba CONFIG (`lock` = 1) e
   **Salvar**. O selo COMPETIÇÃO aparece no dashboard.
2. Demonstrar aos juízes: com o carro armado ou correndo, `stop`, ajustes e até `get` voltam
   com `err:"locked"` e não têm efeito. O `start` em PRONTO continua aceito.
3. O botão do robô continua parando o carro. Para voltar a ajustar entre tentativas, parar o
   carro (botão) — fora de ARMED/RUNNING os comandos voltam a valer, inclusive desligar `lock`.

### Emergências
- **Bateria caindo**: baixar `vmax` 0,1 (ou confiar na compensação `vcomp`)
- **Piso escorregadio**: subir `kd` 15 %, baixar `vmax`
- **Iluminação diferente**: recalibrar os sensores, obrigatoriamente

---

## 7. Referências

- [Ziegler-Nichols PID Tuning — Wikipedia](https://en.wikipedia.org/wiki/Ziegler%E2%80%93Nichols_method)
- [PID Tuning Guide — Zbotic (LFR específico)](https://zbotic.in/pid-line-follower-robot-tuning-speed-competition/)
- [Semreh V2 — Parâmetros campeão brasileiro](https://hackaday.io/project/202208-semreh-advanced-line-follower-robot)
