# Manual do operador — LFR v2

> Para quem: operador (bancada, pista e box). Firmware 4.0.0 (`src/main.cpp`). Pré-requisito:
> carro montado e com os testes de bancada aprovados ([manual de montagem](MONTAGEM.md)).

> **ATENÇÃO — volta válida na RoboCore.** A v2 **não para sozinha depois da chegada**. A regra
> 5.7 da RoboCore só aceita a volta se o robô parar sozinho, sobre a linha, dentro da área de
> partida-chegada, e ficar parado por pelo menos 2 s. Com este firmware o carro **não faz volta
> válida na RoboCore** (achado F-34, seção "Regulamento" de `docs/REVISAO_FIRMWARE.md`). A
> parada automática é a primeira atividade da próxima versão. Use a v2 para treino, sintonia e
> provas que não exijam a parada.

Fontes deste manual: `src/config/RobotConfig.cpp` (chaves, faixas e valores de fábrica),
`src/strategy/RobotStateMachine.cpp` (estados), `src/comm/Annunciator.cpp` (LED e bipes),
`src/comm/CommandProtocol.cpp` e `src/comm/CommandHandler.cpp` (comandos e respostas),
`src/sensors/ButtonInput.h` (tempos do botão), `docs/TUNING.md` e
`docs/dashboard/lfr-cockpit-mock.html`.

## 1 Ligar, desligar e o botão

**Ligar.** Ligue a chave liga/desliga na lateral esquerda. O carro começa em:

- **PRONTO** (LED aceso) se houver calibração dos sensores salva;
- **IDLE** (LED piscando devagar) se não houver.

Três bipes curtos logo ao ligar avisam que a configuração salva estava ausente ou inválida e o
carro está nos valores de fábrica (seção 6).

> **ATENÇÃO — não segure o botão ao ligar.** O botão do carro é o BOOT da placa (GPIO 0). Ligar
> com ele apertado põe o ESP32 em modo de gravação: o firmware não roda
> (`docs/REVISAO_FIRMWARE.md`, item 1). Solte o botão, desligue e ligue de novo.

**O botão** (`src/sensors/ButtonInput.h`):

| Gesto | Tempo | Uso |
|-------|-------|-----|
| Toque curto | soltar antes de 0,8 s | Arma o carro (PRONTO → ARMADO); sai do ERRO |
| Toque longo | segurar 1,5 s (age ao completar o tempo, sem esperar soltar) | Calibra os sensores (em IDLE ou PRONTO) |
| Entre 0,8 s e 1,5 s | — | Nada acontece |
| Aperto com o carro ARMADO ou CORRENDO | assim que o aperto é detectado (30 ms) | **Para o carro** na hora |

O aperto que para o carro é consumido: soltar não arma de novo e segurar não começa a calibrar.
É a parada de emergência — funciona também com o modo competição ligado.

**Desligar.** Pare o carro (aperto do botão), confira que ele está em PRONTO e desligue a chave.
Ajuste feito pelo BLE e não salvo se perde ao desligar (seção 6).

## 2 Estados, LED e bipes

Padrões do `src/comm/Annunciator.cpp`. O dashboard mostra o nome em português (coluna
"Dashboard"); o campo `st` da telemetria traz o número.

| Estado (`st`) | Dashboard | LED (GPIO 2) | Bipe | O que significa | O que fazer |
|---------------|-----------|--------------|------|-----------------|-------------|
| IDLE (0) | IDLE | Pisca 1 vez por segundo | — | Sem calibração dos sensores | Calibrar: toque longo ou **CALIBRAR SENSORES** |
| CALIBRATING (1) | CALIBRANDO | Pisca 5 vezes por segundo | 1 bipe ao entrar | Janela de calibração de 5 s | Varrer a linha (seção 3) |
| READY (2) | PRONTO | Aceso | 1 bipe ao entrar | Calibrado, motores parados | Toque curto (ou `start`) para armar |
| ARMED (3) | ARMADO | Pisca 10 vezes por segundo | Bipe curto a cada 250 ms | O carro parte depois de `startMs` | Tirar a mão. Aperto do botão cancela |
| RUNNING (4) | CORRENDO | Apagado | — | Seguindo a linha | Aperto do botão para |
| ERROR (5) | ERRO | *n* piscadas, pausa de 1,2 s, repete | *n* bipes ao entrar | Parou com o código *n* | Ver a seção 11; toque curto sai do erro |

Outros sinais sonoros (`src/main.cpp`):

| Sinal | Quando | Significado |
|-------|--------|-------------|
| 3 bipes curtos | Ao ligar | Configuração salva ausente ou inválida: valores de fábrica |
| 1 bipe curto | `cal start` | Calibração de encoder começou |
| 1 bipe longo | `cal finish` | PPR do encoder medido e gravado |
| 3 bipes | `cal finish` | Calibração de encoder falhou (nenhuma contagem) |

## 3 Calibração dos sensores

**Quando refazer:** pista ou iluminação nova; troca de calços (altura do sensor); troca ou
remontagem do QTR-8A; depois de um **ERRO(4)**; antes da prova, na pista oficial.

1. Ponha o carro na pista com a barra de sensores sobre a linha.
2. Em IDLE ou PRONTO, faça um **toque longo** (1,5 s) — ou toque **CALIBRAR SENSORES** na aba
   CONFIG (`{"t":"scal"}`). O LED pisca 5 vezes por segundo.
3. Durante os **5 s** da janela (`src/strategy/RobotStateMachine.h`), deslize o carro devagar
   de um lado ao outro, até cada um dos 8 sensores passar pela linha e pelo fundo preto.
4. Espere o fim da janela:
   - todos os canais com faixa (máximo − mínimo) de pelo menos 200 contagens
     (`src/sensors/Calibration.h`) → **PRONTO**, com 1 bipe. O dashboard recebe
     `{"t":"calres","ok":1}`;
   - algum canal com menos → **ERRO(4)**. O dashboard recebe, por exemplo,
     `{"t":"calres","ok":0,"ch":[3,4]}`: os canais sem contraste, contados de 0 (`s0` a `s7`).

A calibração válida fica **salva sozinha** na memória do carro. Ao ligar de novo, o carro já
começa em PRONTO.

Confira na faixa de sensores do dashboard: sobre a linha, valor alto; sobre o fundo, baixo
(`docs/TUNING.md`, seção 2).

## 4 Dashboard

1. Abra `docs/dashboard/lfr-cockpit-mock.html` no **Chrome ou Edge**. O Web Bluetooth exige
   página em HTTPS ou em `localhost` (README, "Dashboard de Telemetria"). No computador:

   ```bash
   cd docs/dashboard
   python3 -m http.server 8765
   # abrir http://localhost:8765/lfr-cockpit-mock.html
   ```

2. Toque **CONECTAR BLE** (barra de cima) e escolha `LFR-RACER-01`. Android com Chrome
   funciona. **iPhone não tem Web Bluetooth.**
3. Ao conectar, o dashboard recebe o `info` (versão, aviso de configuração, folga de pilha) e a
   configuração inteira.

**Abas:**

| Aba | Para quê |
|-----|----------|
| GRÁFICO | Posição, correção e velocidades dos últimos 5 s |
| PID | Sliders de `kp`, `ki`, `kd` (enviam `set` ao soltar) |
| VELOCIDADE | Sliders de `vmax`, `vmin` (em %) e `thr` |
| CONFIG | Todos os parâmetros da seção 5, seletor de perfil, **SALVAR**, **RECARREGAR**, **FÁBRICA**, **CALIBRAR SENSORES** e o interruptor **Modo competição** |
| SISTEMA | Nome, firmware, ESTADO, ERRO, TENSÃO, PERFIL, MODO COMPETIÇÃO, LAÇO MÁX (`ovr`) e PILHA LIVRE (`stk`) |

**Indicadores:**

- **NÃO SALVO** (barra de cima e aba CONFIG): há ajuste aplicado ao vivo que não foi gravado.
  Some depois de **SALVAR** ou **RECARREGAR**.
- **COMPETIÇÃO**: o modo competição está ligado (`lock` = 1, seção 12).
- "Configuração salva ausente ou inválida — usando valores de fábrica": aviso `cfgerr` do
  `info`.

> **O "ok" de `start`, `stop` e `scal` só confirma o recebimento.** A resposta
> `{"t":"ack","k":"start","ok":1}` quer dizer que o carro recebeu o comando, não que ele armou.
> Um `start` em IDLE, um `stop` em PRONTO ou um `scal` com o carro correndo respondem `ok` e
> não fazem nada (A-WP1-07 em `docs/avaliacao/2026-10-13-WP1.md`). O estado real é o campo
> `st` da telemetria: olhe o ESTADO no dashboard.

Sem o dashboard, qualquer app BLE serve: serviço `0xABCC`, comandos em JSON escritos na
característica `0xABCE`, respostas e telemetria por notificação na `0xABCD` (README, "Protocolo
BLE v4").

## 5 Parâmetros

Tabela gerada do `src/config/RobotConfig.cpp`, na mesma ordem das chaves. Para mudar um valor:
`{"t":"set","k":"<chave>","v":<valor>}` ou o campo da aba CONFIG. Valor fora da faixa volta com
`err:"range"` e não muda nada. Chaves de 0/1 e de milissegundos aceitam só inteiros.

"Com o carro andando" = estados ARMADO e CORRENDO. Com o modo competição ligado (`lock` = 1),
**nenhuma** chave muda nesses estados.

| Chave | Descrição | Unidade | Fábrica | Faixa | Muda com o carro andando? |
|-------|-----------|---------|---------|-------|---------------------------|
| `invL` | Inverte o sentido do motor esquerdo | — | 0 | 0 ou 1 | não |
| `invR` | Inverte o sentido do motor direito | — | 0 | 0 ou 1 | não |
| `swap` | Troca os canais A/B do TB6612 entre esquerdo e direito | — | 0 | 0 ou 1 | não |
| `einvL` | Inverte o sentido de contagem do encoder esquerdo | — | 0 | 0 ou 1 | não |
| `einvR` | Inverte o sentido de contagem do encoder direito | — | 0 | 0 ou 1 | não |
| `pol` | Cor da linha: 0 = linha preta, 1 = linha branca | — | 1 | 0 ou 1 | não |
| `sinv` | Inverte a ordem dos sensores (barra montada espelhada) | — | 0 | 0 ou 1 | não |
| `chem` | Química da bateria (curva de %): 0 = LiPo, 1 = Li-ion | — | 1 | 0 ou 1 | não |
| `lock` | Modo competição: 1 = recusa todo comando BLE com o carro andando | — | 0 | 0 ou 1 | não |
| `vcut` | Tensão de corte: abaixo dela por 1 s o carro não arma e, correndo, para | V | 6,4 | 5,6–7,6 | não |
| `vbk` | Correção da leitura de tensão (aferida com multímetro, seção 10) | — | 1,0 | 0,8–1,2 | não |
| `pprL` | Contagens ×4 por volta do eixo de saída, encoder esquerdo (também pelo `cal`) | — | 600 | 1–10000 | não |
| `pprR` | Idem, encoder direito | — | 600 | 1–10000 | não |
| `kp` | Proporcional da malha de posição | fração / erro | 1,0 | 0–20 | sim |
| `ki` | Integral da malha de posição | 1/s | 0,0 | 0–50 | sim |
| `kd` | Derivativo da malha de posição | s | 0,03 | 0–2 | sim |
| `dlp` | Filtro do termo D (1 = sem filtro; menor = mais filtro) | — | 0,5 | 0,05–1 | sim |
| `mode` | 0 = PWM direto, 1 = cascata com encoders | — | 0 | 0 ou 1 | não |
| `kpv` | Proporcional da malha de velocidade (cascata) | fração / RPM | 0,002 | 0–0,05 | sim |
| `kiv` | Integral da malha de velocidade | fração / (RPM·s) | 0,005 | 0–1 | sim |
| `kdv` | Derivativo da malha de velocidade | fração·s / RPM | 0,0 | 0–0,01 | sim |
| `rpmMax` | RPM que vale a fração 1,0 no modo cascata | RPM | 700 | 100–2000 | sim |
| `vmax` | Velocidade-base em reta (erro zero) | fração | 0,35 / 0,50 / 0,70 (perfis 0 / 1 / 2) | 0–1, ≥ `vmin` | sim |
| `vmin` | Velocidade-base com erro ≥ `thr` | fração | 0,25 | 0–1, ≤ `vmax` | sim |
| `thr` | Erro a partir do qual a base é `vmin` | — | 0,6 | 0,05–1 | sim |
| `acc` | Rampa máxima de subida da base (a descida é imediata) | fração/s | 2,0 | 0,1–50 | sim |
| `vref` | Tensão de referência da compensação de bateria | V | 7,4 | 5,0–8,4 | sim |
| `vcomp` | Liga a compensação de tensão (× `vref` / tensão da bateria) | — | 1 | 0 ou 1 | sim |
| `lostMs` | Tempo com a linha perdida até parar em ERRO(2) | ms | 300 | 50–5000 | sim |
| `startMs` | Atraso entre armar e partir (estado ARMADO) | ms | 1000 | 0–5000 | sim |

Escala: no modo PWM direto, a fração 1,0 é o duty máximo (1023, `src/config.h`); no modo
cascata, 1,0 é `rpmMax`. O erro de posição vai de −1 (linha toda à esquerda) a +1 (toda à
direita). Os valores de fábrica são ponto de partida para a pista, não valores sintonizados.

## 6 Perfis e salvamento

Há **3 perfis** de sintonia, iguais de fábrica exceto em `vmax`:

| Perfil | Nome | `vmax` de fábrica |
|--------|------|-------------------|
| 0 | seguro | 0,35 |
| 1 | treino | 0,50 |
| 2 | corrida | 0,70 |

As chaves de hardware (`invL` a `pprR`, as 13 primeiras da tabela) valem para todos os perfis.
As de sintonia (`kp` a `startMs`) são de cada perfil.

- **Ajuste ao vivo não grava.** Um `set` (ou um slider) muda o carro na hora e acende o
  **NÃO SALVO**. Ao desligar, volta o que estava salvo.
- **SALVAR** (`{"t":"save"}`) grava as chaves de hardware, o perfil ativo e qual perfil está
  ativo. Só funciona com o carro parado: armado ou correndo, volta `err:"running"`.
- **RECARREGAR** (`{"t":"load"}`) descarta o que não foi salvo e relê o perfil ativo.
- **FÁBRICA** (`{"t":"defaults"}`) carrega os valores de fábrica do hardware e do perfil ativo,
  ao vivo, sem gravar. Só grava se você **SALVAR** depois.

> **ATENÇÃO — FÁBRICA apaga correções e a calibração dos encoders.** FÁBRICA volta todas as
> chaves de hardware para a fábrica — inclusive `invL`, `invR`, `swap`, `sinv`, `pol`, `lock` e
> `vbk` — e devolve `pprL`/`pprR` para 600. Um **SALVAR** em seguida grava 600 por cima do PPR medido no `cal` (A-WP1-04 em
> `docs/avaliacao/2026-10-13-WP1.md`). Depois de FÁBRICA, refaça as correções da seção 7 e o
> `cal` da seção 9 (ou digite de novo o PPR do último `ecal`) **antes** de salvar.

**Trocar de perfil:** botões **0 SEGURO / 1 TREINO / 2 CORRIDA** na aba CONFIG, ou
`{"t":"prof","n":2}`. Com mudança pendente, o carro recusa (`err:"dirty"`) e o dashboard
pergunta: **DESCARTAR E TROCAR** (envia `"f":1` e perde o que não foi salvo) ou **CANCELAR**
(salve antes, se quiser manter). A troca também relê as chaves de hardware salvas.

O carro liga no último perfil **salvo**. Para mudar o perfil de partida, troque de perfil e
toque **SALVAR**. Na primeira vez, liga no perfil 0.

## 7 Primeira volta

Faça na primeira vez que o carro vai para a pista, e depois de qualquer remontagem.

1. Pista livre, bateria carregada (seção 10).
2. Ligue o carro e conecte o dashboard.
3. Confira na aba CONFIG: perfil **0 SEGURO** e `mode` = 0. Mude se preciso e toque **SALVAR**.
4. Calibre os sensores (seção 3).
5. Mova o carro com a mão sobre a linha e confira na faixa de sensores e na aba SISTEMA:

   | O que você vê | Corrija com | Valor |
   |---------------|-------------|-------|
   | Sensores altos sobre o fundo e baixos sobre a linha | `pol` | 1 para linha branca, 0 para preta |
   | Linha sob o lado esquerdo do carro aparece à direita na faixa | `sinv` | 1 |

6. Ponha o carro sobre a linha e faça um **toque curto**. O LED pisca rápido e bipa durante
   `startMs` (1 s de fábrica); depois o carro parte devagar (`vmax` 0,35).
7. Observe a primeira reta e a primeira curva. **Aperte o botão** para parar quando quiser.
8. Corrija pelo BLE, **sem recompilar**, com o carro parado:

   | Sintoma | Corrija com | Valor |
   |---------|-------------|-------|
   | Uma roda gira para trás | `invL` (esquerda) ou `invR` (direita) | 1 |
   | O comando de um lado move a roda do outro (visto no `bench_motor`: canal A deveria ser a esquerda) | `swap` | 1 |
   | O carro vira para o lado oposto ao da linha e sai | `sinv` (se a faixa de sensores estiver espelhada) ou `invL`/`invR`/`swap` (se as rodas estiverem trocadas) | 1 |
   | RPM negativo com a roda andando para frente (modo cascata) | `einvL` ou `einvR` | 1 |

9. Com o carro correto, toque **SALVAR**.

Se o carro parar sozinho, veja o código na seção 11. O mais comum na primeira volta é o
**ERRO(2)**: a linha ficou fora dos sensores por mais de `lostMs` (300 ms de fábrica).

## 8 Sintonia na pista

O procedimento completo está em [`docs/TUNING.md`](../TUNING.md). Resumo:

1. Perfil 0, `mode` 0, `ki` = 0 e `kd` = 0.
2. Suba `kp` de 0,5 em 0,5 até o carro oscilar na reta com amplitude constante: esse é o
   **Ku**. Meça o período da oscilação no gráfico de posição: **Tu**, em segundos.
3. Aplique `kp` = 0,6 · Ku, `kd` = 0,075 · Ku · Tu e `ki` = 0.
4. Correção tremida no gráfico: ajuste `dlp` entre 0,3 e 0,7.
5. Suba `vmax` de 0,05 em 0,05 por sessão. Para as curvas, baixe `vmin` ou `thr`.
6. Se a frente levantar na largada ou na saída das curvas (patins fora do chão), baixe `acc`:
   o motor acelera mais do que o carro aguenta sem empinar (A-WP2-18 em
   `docs/avaliacao/2026-10-13-WP2.md`).
7. `ki` só se houver desvio constante na reta: comece com 0,1.
8. Ao fim de cada sessão boa, **SALVAR** e registre em `docs/tuning_log.csv`, uma linha por
   sessão, nas colunas
   `data,perfil,mode,kp,ki,kd,dlp,vmax,vmin,thr,acc,kpv,kiv,melhor_volta_s,observacoes`.

## 9 Modo cascata (encoders)

O modo cascata (`mode` = 1) controla a rotação de cada roda pelos encoders. O PPR de fábrica
(600) é provisório: **calibre os dois encoders antes**.

1. Carro parado (IDLE, PRONTO ou ERRO), rodas fora do chão.
2. Envie `{"t":"cal","op":"start","side":"L"}`. Um bipe curto confirma.
3. Gire a roda **esquerda** 10 voltas completas com a mão, sempre para frente.
4. Envie `{"t":"cal","op":"finish","side":"L","rot":10}` (`rot` = número de voltas, inteiro
   de 1 a 1000).
5. Confira a resposta: `{"t":"ecal","side":"L","ok":1,"ppr":612.5}` (exemplo) e um bipe longo.
   `ok` 0 com 3 bipes = nenhuma contagem: veja o `bench_encoder` no manual de montagem.
6. Repita os passos 2 a 5 com `"side":"R"` e a roda direita.
7. O PPR medido já fica gravado (não precisa de **SALVAR**, e o NÃO SALVO não acende) e passa
   a valer como `pprL`/`pprR`.
8. Com o carro parado, `{"t":"set","k":"mode","v":1}` e **SALVAR**.
9. Comece com `kpv` 0,002, `kiv` 0,005 e `kdv` 0. No gráfico, `rpmL`/`rpmR` têm que
   acompanhar `spL`/`spR` na reta. `kp` e `kd` continuam valendo: a escala é a mesma.

Com uma calibração de encoder aberta (`start` sem `finish`), o carro não arma: o `start` volta
`err:"cal"` e o botão não faz nada. Termine com `finish` ou envie `{"t":"reset"}`.

Se uma roda ficar com duty de 60 % ou mais e rotação abaixo de 5 % de `rpmMax` por 200 ms
(`src/config.h`), o carro para em **ERRO(5)** — encoder solto ou sem alimentação.

## 10 Bateria

O dashboard mostra a carga em % e a tensão (barra de cima e aba SISTEMA), atualizadas 2 vezes
por segundo pelo frame `sta`. O % segue a curva de Li-ion de 2S (`chem` = 1) do
`src/sensors/BatteryMonitor.cpp` e serve só como indicação:

| Tensão do pack | 6,0 V | 6,8 V | 7,2 V | 7,6 V | 8,0 V | 8,4 V |
|----------------|-------|-------|-------|-------|-------|-------|
| Carga indicada | 0 % | 10 % | 40 % | 70 % | 90 % | 100 % |

**Corte:** com a tensão abaixo de `vcut` (6,4 V de fábrica) por 1 s, o carro não arma (vai para
ERRO(3)) e, se estiver correndo, para em ERRO(3).

**Aferir a leitura (`vbk`)** — faça uma vez e depois de trocar o divisor ou a placa:

1. Carro ligado e parado, células no suporte.
2. Meça com o multímetro a tensão entre os fios do suporte.
3. Espere alguns segundos e leia a TENSÃO no dashboard.
4. Calcule `vbk` novo = `vbk` atual × tensão do multímetro ÷ tensão do dashboard. Exemplo:
   1,0 × 7,80 ÷ 7,62 = 1,024.
5. Envie `{"t":"set","k":"vbk","v":1.024}` (faixa 0,8–1,2) e **SALVAR**.

> **ATENÇÃO — células Li-ion.**
> - Carregue as 18650 **fora do carro**, no carregador de Li-ion, sem deixar sem supervisão.
> - Nunca descarregue uma célula abaixo de 2,5 V (o mínimo do fabricante fica entre 2,5 e
>   3,0 V, ficha 11 em `hardware/components/fichas/11-celula18650.md`). O corte de 6,4 V no
>   pack protege durante a corrida; não deixe o carro ligado parado por horas.
> - Use sempre duas células do mesmo modelo, lote e carga.
> - Célula amassada, com invólucro rasgado ou quente sem motivo → descarte. Não use.
> - Ponha as células conforme a marcação do suporte, com a chave desligada.

## 11 Códigos de erro

O carro para os motores e fica em ERRO. O LED pisca *n* vezes e pausa; ao entrar, bipa *n*
vezes. O dashboard mostra o código e o nome em ERRO (aba SISTEMA). **Para sair:** toque curto
(volta a PRONTO, ou a IDLE se não houver calibração) ou `{"t":"reset"}` (vai a IDLE).

| Código | Nome no dashboard | Sintoma | Causa provável | Ação |
|--------|-------------------|---------|----------------|------|
| 1 | LAÇO ATRASADO | Para no meio da corrida; 1 piscada | O laço de controle passou de 2000 µs em 5 ciclos seguidos (`src/main.cpp`) | Anote o LAÇO MÁX (`ovr`) da aba SISTEMA. Toque curto e tente de novo. Se repetir, regrave o firmware e relate ao desenvolvimento (manual de manutenção) |
| 2 | LINHA PERDIDA | Para ao sair de uma curva ou cruzamento; 2 piscadas | A linha ficou fora dos sensores por mais de `lostMs` | Recoloque o carro na linha e dê toque curto. Baixe `vmax`/`vmin`; recalibre se a luz mudou; suba `lostMs` com cuidado |
| 3 | BATERIA BAIXA | Não arma, ou para correndo; 3 piscadas | Tensão abaixo de `vcut` por 1 s | Troque por células carregadas. Confira a leitura com o multímetro e afira `vbk` (seção 10) |
| 4 | CALIBRAÇÃO INVÁLIDA | Fim da calibração com 4 piscadas | Algum canal com faixa menor que 200 contagens; o `calres` lista os canais | Refaça a calibração passando todos os sensores pela linha. Confira a altura do sensor e o canal no `bench_sensor_raw` |
| 5 | ENCODER SEM LEITURA | Para no modo cascata; 5 piscadas | `mode` = 1 e uma roda com duty alto e rotação ≈ 0 por 200 ms | Confira o conector do encoder e o `bench_encoder`; refaça o `cal` (seção 9). Até resolver, use `mode` = 0 |

**Respostas de erro do BLE** (campo `err` do `ack`, `src/comm/CommandHandler.cpp`):

| Erro | Significa | O que fazer |
|-------|-----------|-------------|
| `err:"range"` | Valor fora da faixa (ou não numérico) | Use a faixa da seção 5 |
| `err:"key"` | Chave desconhecida | Confira o nome na seção 5 (maiúsculas contam) |
| `err:"running"` | Não permitido com o carro armado ou correndo | Pare o carro e repita |
| `err:"dirty"` | Troca de perfil com mudança não salva | Salve ou descarte (seção 6) |
| `err:"store"` | Falha ao gravar | Repita; se persistir, veja o manual de manutenção |
| `err:"parse"` | Comando com JSON inválido | Confira aspas e vírgulas |
| `err:"locked"` | Modo competição: recusado com o carro armado ou correndo | Pare o carro pelo botão (seção 12) |
| `err:"cal"` | `start` com calibração de encoder aberta | Termine com `cal finish` ou envie `reset` |

## 12 Checklist de box (competição)

Regulamento RoboCore (seção "Regulamento" de `docs/REVISAO_FIRMWARE.md`): ajustes e troca de
perfil só **entre** tentativas (regra 3.1.1). Durante a tentativa, nenhum comando remoto pode
parar o robô nem mudar parâmetro, e o celular fica onde o juiz mandar (regra 5.9). Lembre que a
v2 não faz volta válida na RoboCore (aviso no início deste manual).

**Tomada de tempo com modo competição (regra 5.9):**

1. Com o carro parado (PRONTO), ligue o interruptor **Modo competição** na aba CONFIG
   (`{"t":"set","k":"lock","v":1}`) e toque **SALVAR**. O selo **COMPETIÇÃO** aparece.
2. Demonstre ao juiz: arme o carro (ou deixe-o correr na pista) e envie um ajuste ou
   `{"t":"stop"}`. A resposta é
   `{"t":"ack","k":"stop","ok":0,"err":"locked"}` e nada muda no carro. A telemetria continua.
3. Pare o carro pelo botão.
4. **Entregue o celular ao juiz** (ou deixe no local que ele indicar) antes da tomada de tempo.
5. Largue pelo botão (toque curto).
6. Entre tentativas, com o carro parado pelo botão, os comandos voltam a valer, inclusive
   desligar o `lock`.

**Antes de cada tomada de tempo:**

- [ ] Células carregadas fora do carro; pack acima de 7,0 V (`docs/TUNING.md`, seção 1)
- [ ] Parafusos firmes (abraçadeiras, barra, patins, perfboard)
- [ ] O-rings no canal, sem desgaste nem superfície lisa
- [ ] Patins inteiros, sem desgaste visível
- [ ] Sensores calibrados **na pista oficial**, com a iluminação da prova
- [ ] Perfil certo selecionado e **salvo** (sem NÃO SALVO)
- [ ] `lock` = 1 salvo e selo COMPETIÇÃO visível
- [ ] Botão testado: o carro para no aperto
- [ ] Celular entregue ao juiz; BLE desconectado se o regulamento do evento exigir
