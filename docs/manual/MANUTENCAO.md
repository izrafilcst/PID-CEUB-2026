# Manual de manutenção — LFR v2

> Para quem: manutenção (box e oficina). Firmware 4.0.0 (`src/main.cpp`). Pré-requisito:
> conhecer o [manual de montagem](MONTAGEM.md) e o [manual do operador](OPERACAO.md).

Fontes deste manual: fichas em `hardware/components/fichas/`,
`hardware/cad/out/relatorio_montagem.md`, `platformio.ini`, `docs/REVISAO_FIRMWARE.md`,
`docs/DIVERGENCIAS.md`, `README.md` e `hardware/bom.csv`.

## 1 Inspeção periódica

"Bateria" = uma carga do par de células. Anote cada inspeção no registro da seção 8.

| Item | Quando | O que olhar | Critério de troca |
|------|--------|-------------|-------------------|
| Patins | A cada sessão | Desgaste da calota; trincas | Desgaste maior que 0,5 mm ou calota achatada → trocar (seção 2.1) |
| O-rings | A cada sessão | Superfície, trincas, folga no canal | Liso, rachado ou frouxo → trocar o par (seção 2.3; ficha 13) |
| Células 18650 | A cada sessão, antes de carregar | Invólucro, amassados, aquecimento; tensão em repouso de cada uma | Invólucro rasgado, amassada ou quente sem motivo → descarte. Tensões muito diferentes entre as duas → carregar separadas e conferir de novo (ficha 11) |
| Parafusos | A cada 10 baterias | Abraçadeiras, barra, patins, perfboard | Folgado → reapertar sem esmagar; rosca espanada → ver insertos |
| Conectores JST-SH dos encoders | A cada 10 baterias | Encaixe firme, fios sem tensão | Solto ou com fio partido → refazer e testar no `bench_encoder` |
| Fios perto das rodas e dos motores | A cada 10 baterias | Isolação, atrito com a roda, solda dos motores | Fio esfolado ou solda trincada → refazer |
| Insertos M3 | Mensal | Inserto girando em falso ou saindo | Solto → prensar de novo a quente ou trocar a peça |
| Molas do suporte 2×18650 | Mensal | Pressão das molas, oxidação | Célula com folga ou mau contato → trocar o suporte (seção 2.7; ficha 12) |
| Chicote do QTR-8A | Mensal | Fios esmagados no canal da base, conector firme | Fio partido → refazer com AWG 30 (⌀ ≤ 0,6 mm) |

## 2 Troca de peças

Depois de qualquer troca, faça o teste de bancada indicado e rode uma volta curta no perfil 0.

### 2.1 Patim

1. Solte o M2 do patim, por cima da barra.
2. Tire o patim e os calços que estiverem sob ele. Anote quantos calços havia.
3. Ponha o patim novo (`patim.stl`) com o mesmo número de calços.
4. Aperte o M2 (× 12 com 0–2 calços, × 14 com 3–6, tabela "Fixadores" do relatório de
   montagem).
5. Faça o mesmo do outro lado se o desgaste for parecido: os dois patins têm que ter a mesma
   altura.

### 2.2 Calço (altura do sensor)

1. Cada passo de 0,5 mm usa um `calco.stl` sob cada patim e um `calco_lingua.stl` sob a língua
   da barra (`hardware/cad/parts/sensor_bar.py`).
2. Solte os 2 M2 dos patins e os 2 M3 da língua (por baixo da base).
3. Acrescente ou tire os calços **nos três pontos ao mesmo tempo**, para o chassi continuar
   nivelado.
4. Troque o comprimento dos parafusos se mudar de faixa (M3 × 8 até 4 calços, × 10 com 5–6;
   M2 × 12 até 2 calços, × 14 com 3–6).
5. Aperte e **recalibre os sensores** (manual do operador, seção 3).

### 2.3 O-ring

1. Tire o O-ring velho do canal da roda com os dedos (sem ferramenta de ponta, para não riscar
   o canal).
2. Encaixe o novo (34 × 3 mm, ficha 13), sem torcer o cordão.
3. Troque os dois lados juntos: diâmetros diferentes fazem o carro puxar para um lado.

### 2.4 Roda

1. Solte a trava radial M2 × 4, se houver.
2. Puxe a roda para fora do eixo, segurando o motor pelo lado de dentro.
3. Encaixe a roda nova com o achatado do furo em D alinhado com o do eixo.
4. Confira que a face da caixa do motor continua encostada no batente; se saiu, solte a
   abraçadeira, encoste e aperte de novo.
5. Gire a roda com a mão: não pode tocar a base nem o motor.

### 2.5 Abraçadeira e motor N20

1. Desligue a chave e tire as células.
2. Tire a roda (seção 2.4).
3. Dessolde ou desconecte os fios do motor e o conector JST-SH do encoder.
4. Solte os 2 M2 × 8 da abraçadeira e tire o motor.
5. Monte o motor novo como no manual de montagem, seção 3, passos 3 a 8 (a roda entra antes
   do aperto da abraçadeira).
6. Rode o `bench_motor` e o `bench_encoder`.
7. Refaça a calibração do encoder daquele lado (`cal`, manual do operador, seção 9) e confira o
   sentido (`invL`/`invR`, `einvL`/`einvR`).

### 2.6 QTR-8A

1. Desligue a chave.
2. Desconecte o conector do chicote pela janela da barra.
3. Solte os 2 M2 × 8 cabeça baixa por baixo e tire a placa.
4. Una os dois pinos **3.3V BYPASS** da placa nova com um pedaço de fio (manual de montagem,
   seção 3, passo 11). Sem isso, a 3,3 V os LEDs ficam fracos e a calibração pode dar ERRO(4).
5. Monte a placa nova com os sensores para baixo e a borda dos pinos para trás.
6. Rode o `bench_sensor_raw`: os 8 canais têm que responder.
7. **Recalibre os sensores** no firmware principal (manual do operador, seção 3).

### 2.7 Suporte 2×18650

1. Desligue a chave e tire as células.
2. Desconecte os fios do suporte da placa.
3. Puxe o suporte para fora das abas do berço.
4. Meça o suporte novo (comprimento, largura, altura). Se for diferente do anterior, preencha
   o `[medidas]` do `hardware/cad/medidas.toml` e rode o `build.py`: o berço pode não servir
   (manual de montagem, seção 2.3).
5. Confira que o suporte novo é **série** (2S): com as células, 7–8,4 V entre os fios
   (ficha 12).
6. Encaixe pelas abas, ligue os fios respeitando a polaridade e confira com o multímetro antes
   de ligar a chave.

### 2.8 Fusível

> **ATENÇÃO.** Fusível queimado indica curto ou motor travado. Ache a causa antes de trocar.

1. Desligue a chave e tire as células.
2. Procure a causa: fio esfolado encostando no chassi, solda em curto, célula invertida, roda
   travada.
3. Meça a resistência entre +BAT e GND: tem que ser maior que 1 kΩ (manual de montagem,
   seção 5).
4. Troque por um fusível de lâmina mini **de 3 A** — nunca maior (`docs/WIRING.md`).

## 3 Diagnóstico

Para cada sintoma, faça os testes na ordem. O primeiro "sim" leva à ação.

### 3.1 Não liga (LED apagado, nenhum bipe)

1. O fusível está inteiro? **Não** → seção 2.8.
2. As células estão na polaridade certa e o pack tem 6,0–8,4 V entre os fios do suporte? **Não**
   → recolocar ou carregar as células.
3. A chave passa corrente (mesma tensão antes e depois)? **Não** → trocar a chave.
4. A saída do MP1584 está em 5,0 V? **Não** → reajustar com o ESP32 desconectado (manual de
   montagem, seção 5).
5. O botão BOOT estava apertado ao ligar? **Sim** → soltar e religar: com o GPIO 0 em nível
   baixo no boot a placa entra em modo de gravação (`docs/REVISAO_FIRMWARE.md`, item 1).
6. O ESP32 liga pelo USB sozinho (sem o perfboard)? **Sim** → com o perfboard ele não liga:
   conferir o pull-down de 10 kΩ no GPIO 12 e o MCP3008 em 3,3 V. Com o GPIO 12 em nível alto no
   boot a placa não inicia (`docs/REVISAO_FIRMWARE.md`, item 1).

### 3.2 Liga e reinicia ao acelerar

1. As células são *high-drain* (≥ 20 A) e do mesmo lote? **Não** → trocar (ficha 11).
2. As molas do suporte seguram as células sem folga? **Não** → trocar o suporte (seção 2.7).
3. O capacitor de 100 µF está no VM do TB6612? **Não** → instalar (ficha 05).
4. Os fios de alimentação estão firmes nas soldas e no conector da bateria? **Não** → refazer.

### 3.3 LED em erro

Conte as piscadas antes da pausa (ou leia o ERRO na aba SISTEMA do dashboard). A tabela de
códigos completa está no manual do operador, seção 11.

#### ERRO(1) — laço atrasado

1. O erro se repete na mesma situação? **Não** → toque curto e siga; anote no registro.
2. **Sim** → anote o LAÇO MÁX (`ovr`) e a PILHA LIVRE (`stk`) da aba SISTEMA, regrave o
   firmware (seção 4) e relate ao desenvolvimento com esses números.

#### ERRO(2) — linha perdida

1. A faixa de sensores do dashboard mostra a linha com o carro parado sobre ela? **Não** →
   recalibrar (manual do operador, seção 3).
2. A face dos sensores está a 3,0 mm do chão, patins inteiros? **Não** → seção 2.1 ou 2.2.
3. O erro aparece na saída das curvas rápidas? **Sim** → baixar `vmax`/`vmin` ou subir `kp`
   (manual do operador, seção 8).

#### ERRO(3) — bateria baixa

1. O multímetro mostra o pack abaixo de `vcut` (6,4 V de fábrica)? **Sim** → trocar por células
   carregadas.
2. **Não** (multímetro acima, dashboard abaixo) → aferir `vbk` (seção 6.1).

#### ERRO(4) — calibração inválida

1. No `bench_sensor_raw`, todos os canais variam pouco entre a linha e o fundo? **Sim** →
   conferir os pinos 3.3V BYPASS do QTR-8A unidos (manual de montagem, seção 3, passo 11).
2. O `calres` lista os mesmos canais toda vez? **Sim** → rodar o `bench_sensor_raw`: canal
   parado indica solda no MCP3008 ou fio do chicote partido.
3. **Não** → refazer a varredura passando os 8 sensores pela linha e pelo fundo, com a altura
   certa (seção 2.2).

#### ERRO(5) — encoder sem leitura (modo cascata)

1. O `bench_encoder` conta nas duas rodas? **Não** → conector JST-SH, alimentação do encoder ou
   pull-up de 10 kΩ no GPIO 36/39 (lado direito).
2. **Sim** → refazer o `cal` dos dois lados (manual do operador, seção 9) e conferir
   `einvL`/`einvR`. Até resolver, usar `mode` = 0.

### 3.4 Gira em círculos

1. No `bench_differential`, o "reto" anda reto? **Não** → um motor invertido ou canais trocados:
   corrigir `invL`/`invR`/`swap` pelo BLE no firmware principal (não refazer a fiação).
2. Uma roda está parada? **Sim** → ver 3.7.
3. A faixa de sensores mostra a linha do lado certo? **Não** → `sinv` = 1 (barra espelhada).

### 3.5 Oscila na reta

1. O carro oscila mesmo devagar (perfil 0)? **Sim** → baixar `kp` 20 % (`docs/TUNING.md`,
   "Ajuste fino").
2. A correção (gráfico `corr`) treme? **Sim** → baixar `dlp`.
3. Os O-rings estão lisos ou os patins gastos? **Sim** → trocar (seções 2.1 e 2.3).
4. Nada disso → subir `kd` 10 %.

### 3.6 Sai da pista nas curvas

1. Sai só nas curvas rápidas? **Sim** → baixar `vmin` ou `thr`, ou baixar `vmax`.
2. Não fecha nem devagar? **Sim** → subir `kp`.
3. O O-ring da roda de fora sai do canal na curva? **Sim** → anotar e avisar o projetista
   (retenção do O-ring, A-WP2-11 em `docs/avaliacao/2026-10-13-WP2.md`).
4. Os O-rings estão lisos? **Sim** → trocar (seção 2.3).

### 3.7 Um motor não gira

1. No `bench_motor`, o motor gira em algum passo? **Não** → medir tensão nos terminais do motor
   durante o passo: com tensão → motor ou redução travados (trocar, seção 2.5); sem tensão →
   solda, TB6612 ou STBY (GPIO 23).
2. Gira no `bench_motor` mas não no carro? **Sim** → conferir `swap` e se o carro está mesmo em
   CORRENDO (estado no dashboard).

### 3.8 RPM zerado no modo cascata

1. Há uma calibração de encoder aberta (`cal start` sem `finish`)? **Sim** → enviar `finish` ou
   `{"t":"reset"}`. Durante a calibração o RPM fica em 0.
2. O `bench_encoder` conta naquele lado? **Não** → conector, alimentação ou pull-up (ver
   ERRO(5)).
3. `pprL`/`pprR` ainda estão em 600 (fábrica)? **Sim** → fazer o `cal` dos dois lados.

### 3.9 BLE não conecta

1. O navegador é Chrome ou Edge, no computador ou no Android? **Não** → trocar; iPhone não tem
   Web Bluetooth (manual do operador, seção 4).
2. A página foi aberta por `http://localhost` ou HTTPS? **Não** → abrir pelo servidor local
   (manual do operador, seção 4).
3. O carro aparece como `LFR-RACER-01` na lista? **Não** → religar o carro; desconectar outros
   apps BLE que estejam ligados a ele.

### 3.10 "NÃO SALVO" que não some

1. Qual foi a resposta do **SALVAR**? `{"t":"ack","k":"save","ok":1}` → o indicador some no
   próximo frame `sta` (2 vezes por segundo).
2. `err:"running"` → o carro está armado ou correndo: pare pelo botão e salve de novo.
3. `err:"locked"` → modo competição com o carro andando: pare pelo botão.
4. `err:"store"` → falha ao gravar: repita; se persistir, regrave o firmware (seção 4) e
   relate ao desenvolvimento.

## 4 Firmware

- **Versão atual:** 4.0.0 (`FW_VERSION` em `src/main.cpp`). Aparece no `info` e na aba SISTEMA.
- **Plataforma:** `espressif32@6.13.0` (arduino-esp32 2.0.17) e NimBLE-Arduino 2.5.1, com versões
  fixas no `platformio.ini`.
- **Regravar:**

  ```bash
  pio run -e esp32dev -t upload
  ```

- A configuração salva, os perfis, a calibração dos sensores e o PPR dos encoders ficam na NVS
  (namespace `lfr`) e **sobrevivem à regravação** (README, "Parâmetros em runtime").
- Configuração salva ausente ou corrompida → o carro liga com os valores de fábrica daquele
  bloco, dá 3 bipes curtos e o dashboard avisa (`cfgerr` = 1).
- **Voltar aos valores de fábrica:** **FÁBRICA** (`{"t":"defaults"}`) e depois **SALVAR**
  (`{"t":"save"}`), com o carro parado. Isso também devolve `pprL`/`pprR` a 600: refaça o `cal`
  antes de salvar se for usar o modo cascata (A-WP1-04). A calibração dos sensores não muda.
- Os testes de bancada (seção 5) substituem o firmware principal: grave-o de novo ao terminar.
- Pendências conhecidas da v2: sem parada automática depois da chegada (F-34); `ack` de
  `start`/`stop`/`scal` só confirma o recebimento (A-WP1-07); `defaults` zera o PPR (A-WP1-04).

## 5 Testes de bancada

Rodas fora do chão, monitor serial aberto (`pio device monitor -b 115200`). Detalhes de cada
teste no manual de montagem, seção 7.

| Comando | Quando usar | Resultado esperado |
|---------|-------------|--------------------|
| `pio run -e bench_motor -t upload` | Motor ou TB6612 suspeito; depois de trocar motor | A frente, A ré, B frente, B ré, ambos, parado — 1,5 s cada |
| `pio run -e bench_encoder -t upload` | ERRO(5), RPM zerado, troca de motor ou conector | Contagem muda só no lado da roda girada; parada quando a roda para |
| `pio run -e bench_sensor_raw -t upload` | ERRO(4), canal suspeito, troca do QTR-8A | 8 valores de 0 a 1023 que caem sobre a linha branca |
| `pio run -e bench_sensor_cal -t upload` | Conferir a posição calculada | `pos` perto de 0 com a linha no centro, mudando de sinal de um lado para o outro |
| `pio run -e bench_velocity_cal -t upload` | Medir o PPR sem o dashboard | `ok=1` e o PPR de cada lado; depois `rpmL`/`rpmR` ao girar |
| `pio run -e bench_differential -t upload` | Gira em círculos; conferir sentido dos motores | Reto, curva à direita, curva à esquerda, giro horário, giro anti-horário, parado |

## 6 Aferição

### 6.1 Leitura da bateria (`vbk`)

Compare a tensão do multímetro com a do dashboard e corrija `vbk` (faixa 0,8–1,2). Passo a
passo no manual do operador, seção 10. Refaça depois de trocar o divisor (100 kΩ / 47 kΩ,
`docs/WIRING.md`) ou o ESP32.

### 6.2 PPR dos encoders (`cal`)

O padrão de 600 contagens por volta do eixo de saída é provisório (`src/config.h`). Meça com o
`cal` pelo BLE (manual do operador, seção 9) ou com o `bench_velocity_cal`. Refaça depois de
trocar um motor.

### 6.3 Altura do sensor (calços)

A face dos sensores fica a 3,0 mm do chão sem calço (relatório de montagem); a Pololu indica
3 mm como ótimo e 6 mm como máximo (ficha 03). Meça a altura com o paquímetro de profundidade
entre o chão e a face do QTR-8A, com o carro apoiado nas rodas e nos patins. Ajuste com calços
de 0,5 mm (seção 2.2) e recalibre os sensores.

## 7 Peças de reposição

Quantidades a ter na caixa do box, iguais às do carro, salvo indicação.

| Item | Quantidade na caixa | Fonte |
|------|---------------------|-------|
| O-ring de silicone 34 × 3 mm | 2 | `hardware/bom.csv` (2 de reserva) |
| Patim | 2 | `patim.stl` |
| Calço e calço de língua | 4 + 2 | `calco.stl`, `calco_lingua.stl` |
| Roda | 2 | `roda.stl` |
| Abraçadeira do N20 (E e D) | 1 + 1 | `abracadeira_E.stl`, `abracadeira_D.stl` |
| Fusível de lâmina mini 3 A | 1 | `hardware/bom.csv` |
| Células 18650 carregadas (mesmo modelo e lote) | 2 | `hardware/bom.csv`, ficha 11 |
| Inserto térmico M3 | 1 | `hardware/bom.csv` (1 de reserva) |
| Parafusos de cada tipo da tabela "Fixadores" | 1 jogo | `hardware/cad/out/relatorio_montagem.md` |
| Fio AWG 30 e conector do QTR-8A | 1 chicote | `hardware/bom.csv` |

## 8 Registro de manutenção

Copie a tabela para uma folha ou continue aqui, uma linha por intervenção.

| Data | Baterias desde a última | O que foi feito | Peças trocadas | Por quem | Observação |
|------|-------------------------|-----------------|----------------|----------|------------|
| | | | | | |
| | | | | | |
| | | | | | |
