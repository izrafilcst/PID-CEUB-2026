# Manual de montagem — LFR v2

> Para quem: montador. Tempo estimado: não medido — anote o tempo de cada etapa na primeira
> montagem (seção 8). Pré-requisito: nenhum. Depois: [manual do operador](OPERACAO.md) e
> [manual de manutenção](MANUTENCAO.md).

Este manual diz **como montar**. Os números vêm de arquivos do repositório, citados na
primeira vez em que aparecem. Quando uma cota estiver marcada **MEDIR**, vale a medida da peça
em mãos, não a do manual.

Fontes principais: `hardware/bom.csv` (peças), `hardware/cad/out/relatorio_montagem.md`
(peças impressas, fixadores, cotas do modelo), `hardware/cad/medidas.toml` (medidas e
tolerâncias), `hardware/cad/parts/tolerance_coupon.py` (cupom), `hardware/cad/params.py`
(tolerâncias típicas), `docs/WIRING.md` (fiação), `platformio.ini` (testes de bancada) e as
fichas em `hardware/components/fichas/`.

## 1 Antes de começar

### 1.1 Lista de peças

Compras, conforme `hardware/bom.csv`. A coluna "Ficha" aponta para
`hardware/components/fichas/`.

| Item | Qtd. | Ficha | Observação |
|------|------|-------|------------|
| ESP32 DevKit V1 | 1 | [01](../../hardware/components/fichas/01-esp32.md) | Meça a distância entre as fileiras de pinos (22,86 ou 25,4 mm, ficha 01) antes de furar o perfboard |
| MCP3008 (ADC SPI, PDIP-16) | 1 | [02](../../hardware/components/fichas/02-mcp3008.md) | Alimentado em 3,3 V, sem divisor no MISO |
| QTR-8A (Pololu 960) | 1 | [03](../../hardware/components/fichas/03-qtr8a.md) | Importado. Barra de pinos 8×2 ou 11×1 de 2,54 mm |
| Motor N20 600 RPM com encoder | 2 | [04](../../hardware/components/fichas/04-n20.md) | Código do motor a confirmar (D6 em `docs/DIVERGENCIAS.md`) |
| TB6612FNG (módulo SparkFun ROB-14451) | 1 | [05](../../hardware/components/fichas/05-tb6612.md) | — |
| Regulador MP1584EN (módulo mini) | 1 | [07](../../hardware/components/fichas/07-mp1584.md) | Sem furos de fixação: fita dupla-face |
| Suporte 2×18650 série, com fios | 1 | [12](../../hardware/components/fichas/12-suporte18650.md) | MEDIR e preencher o `medidas.toml` |
| Célula 18650 *high-drain* (≥ 20 A) | 2 | [11](../../hardware/components/fichas/11-celula18650.md) | Mesmo modelo, lote e carga |
| Carregador de Li-ion 18650 (2 baias) | 1 | — | Carregar fora do carro |
| O-ring de silicone 34 × 3 mm | 4 | [13](../../hardware/components/fichas/13-oring.md) | 2 em uso + 2 de reserva |
| Inserto térmico M3 (OD 4,0 × 5,7 mm) | 10 | — | 6 no carro + 3 no teste do cupom + 1 de reserva |
| Fusível de lâmina mini 3 A + porta-fusível em linha | 1 | — | Logo depois do suporte |
| P-MOSFET (ex.: AO3401) + zener 12 V + resistor 10 kΩ | 1 | — | Proteção contra inversão (recomendado) |
| Chave SPDT mini (liga/desliga) | 1 | — | Corpo de 8,5 × 3,6 mm no modelo (MEDIR); de preferência 3 A |
| Conector da bateria (suporte → placa) | 1 | — | — |
| Perfboard 5 × 7 cm | 1 | [10](../../hardware/components/fichas/10-perfboard.md) | Furos de fixação feitos pelo montador |
| Capacitor eletrolítico 100 µF 16 V | 1 | — | No VM do TB6612 |
| Capacitor cerâmico 100 nF | 5 | — | 1 por CI (4) + 1 no GPIO 34 |
| Resistor 100 kΩ e resistor 47 kΩ | 1 + 1 | — | Divisor da bateria |
| Resistor 10 kΩ | 4 | — | Pull-up nos GPIO 36 e 39, pull-down no GPIO 12 e no STBY |
| Buzzer passivo 12 mm | 1 | — | Acionado por transistor |
| Transistor NPN de sinal + resistor de base | 1 | — | Acionamento do buzzer; valores a definir |
| LED 5 mm (verde + vermelho) + 2 resistores 330 Ω | opcional | — | O firmware usa só o LED da placa (GPIO 2) |
| Fio de silicone 26 AWG (sortido) | 1 | — | Alimentação e motores |
| Fio AWG 30 (⌀ ≤ 0,6 mm) | 1 (11 vias) | — | Chicote do QTR-8A |
| Barra de pinos 2,54 mm (8×2 ou 11×1) + conector fêmea | 1 | — | Ligação do QTR-8A ao chicote |
| Fita dupla-face VHB | 1 | — | Fixa o MP1584 |
| Filamento PETG 1,75 mm | 100 g | — | Peças do carro ≈ 59 g (seção 2.4) |
| Parafusos e abraçadeira de nylon | — | — | Tabela "Fixadores" abaixo |

**Fixadores** (tabela do `hardware/cad/out/relatorio_montagem.md`; comprimentos conferidos
contra os furos do modelo):

| Qtd. | Item | Onde |
|------|------|------|
| 2 | M3 cabeça chata ISO 10642 (sextavado interno): × 8 com 0–4 calços; × 10 com 5–6 calços | Base → insertos da língua da barra, por baixo, no escareado de 90° |
| 6 | Inserto térmico M3 (OD 4,0 × 5,7 mm) | 4 colunas do perfboard + 2 na língua da barra |
| 4 | M3 × 6 cabeça cilíndrica ou panela | Perfboard → insertos das colunas |
| 4 | M2 × 8 autoatarraxante | Abraçadeiras do N20 → berço da base |
| 2 | M2 autoatarraxante: × 12 com 0–2 calços; × 14 com 3–6 calços | Patins, de cima para baixo pela barra |
| 2 | M2 × 8 autoatarraxante cabeça baixa | QTR-8A → postes da barra |
| 2 | M2 × 4 autoatarraxante (opcional) | Trava radial das rodas |
| 1 | Abraçadeira de nylon 2,5 mm (≤ 1,2 mm de espessura) | Porta-fusível, pelos rasgos e pelo rebaixo sob a base |

Use cabeça chata ISO 10642 nos 2 M3 da barra. Outra cabeça fica abaixo da base e invade o vão
livre (achado A-WP2-04 em `docs/avaliacao/2026-10-13-WP2.md`).

### 1.2 Ferramentas

- Ferro de solda com ponta fina, estanho e termo-retrátil.
- Ferro para insertos (ou ponta cônica no ferro de solda).
- Paquímetro.
- Multímetro com teste de continuidade.
- Chaves hexagonais M2 e M3 e chave de fenda pequena.
- Alicate de corte e alicate de bico.
- Furadeira com broca ⌀3,2 mm para os furos do perfboard.
- Impressora 3D com bico de 0,4 mm e mesa de pelo menos 200 × 200 mm (DEC-12 em
  `docs/superpowers/specs/2026-10-13-lfr-v2-retomada-design.md`).
- Computador com PlatformIO e Python 3 (para o `build.py` do modelo).

### 1.3 Segurança

> **ATENÇÃO — células Li-ion.** Carregue as 18650 **fora do carro**, no carregador próprio,
> sem deixar sem supervisão. Use sempre duas células do mesmo modelo, lote e carga. Célula com
> invólucro rasgado ou amassada vai para o descarte (ficha 11). Um curto entre os polos de uma
> célula carregada aquece o fio e a célula em segundos: nunca deixe ferramenta ou fio solto
> sobre o suporte.

> **ATENÇÃO — polaridade.** Inverter uma célula no suporte põe tensão reversa no circuito. O
> fusível de 3 A é obrigatório e o P-MOSFET de proteção é recomendado
> ([fiação](../WIRING.md)). Confira a marcação do suporte antes de fechar a chave.

> **ATENÇÃO — solda e insertos.** A ponta do ferro e o inserto prensado ficam muito quentes.
> Solde em local ventilado e sem as células no suporte.

## 2 Impressão

### 2.1 Perfil de PETG

Perfil de partida (plano do WP3, `docs/superpowers/plans/2026-10-13-lfr-v2-wp3-manuais.md`):

| Ajuste | Valor |
|--------|-------|
| Bico | 0,4 mm |
| Camada | 0,2 mm |
| Perímetros | 3 |
| Preenchimento | 40–60 % (o relatório de montagem estima as massas com 60 %) |
| Mesa | 75–85 °C |
| Bico (temperatura) | 230–245 °C |
| Ventoinha | 30–50 % |
| Suporte | nenhum |

Os STL já saem na orientação de impressão (`hardware/cad/build.py`). Não gire as peças no
fatiador.

### 2.2 Cupom de tolerância

Imprima o cupom **antes** de qualquer peça do carro. Ele mostra como a sua impressora reproduz
furos e encaixes, e o resultado vai para a seção `[tolerancias]` do `hardware/cad/medidas.toml`.

1. Imprima `cupom.stl` e `cupom_chave.stl` (`hardware/cad/out/`) com o perfil de 2.1.
2. Segure o cupom com os bolsões abertos para cima e a borda com entalhes virada para você.
3. Teste cada feição na ordem da tabela (a mesma de `FEICOES` em
   `hardware/cad/parts/tolerance_coupon.py`).
4. Para cada chave, escreva no `medidas.toml` o valor **nominal** da feição que deu certo e
   tire o `#` da linha.

| Feição (ordem do cupom) | Onde fica | Chave no `[tolerancias]` | Como decidir |
|-------------------------|-----------|--------------------------|--------------|
| 7 furos ⌀2,8 · 2,9 · 3,0 · 3,1 · 3,2 · 3,3 · 3,4 mm | Fileira de cima; 1 entalhe = ⌀2,8 … 7 entalhes = ⌀3,4 | `furo_m3` (típico 3,4) | O menor furo em que um parafuso M3 passa inteiro, sem forçar |
| 3 furos em D: 3,00/2,50 · 3,05/2,55 · 3,10/2,60 mm | Fileira do meio, os 3 da esquerda | `eixo_d` / `eixo_achatado` (típicos 3,05 / 2,55) | O furo em que o eixo do N20 entra com pressão leve e não gira dentro |
| 3 furos de inserto ⌀3,9 · 4,0 · 4,1 mm | Fileira do meio, os 3 da direita | `inserto_m3_d` (típico 4,0) | O furo que segura o inserto prensado a quente, rente à face, sem rachar a parede |
| 3 bolsões para a chave de 10 mm, folga 0,2 · 0,3 · 0,4 mm por lado | Fileira de baixo, à esquerda | `deslizante` (típico 0,30) | O bolsão em que a chave entra e sai deslizando, sem folga visível. Para tirar a chave de um bolsão justo, empurre pelo furo central com uma chave de fenda |
| 3 rasgos de PCB 1,8 · 2,0 · 2,2 mm | Fileira de baixo, à direita | `pcb` (típico 0,20) | Registro apenas: na v2 nenhuma peça usa essa chave (A-WP2-06) |

O cupom não calibra `furo_m2` (2,4) nem `autoatarraxante_m2` (1,7): deixe os valores típicos do
`hardware/cad/params.py` (A-WP2-16).

### 2.3 Medir as peças compradas

Meça com o paquímetro e escreva na seção `[medidas]` do `hardware/cad/medidas.toml`. Linha
comentada vale o valor típico do `hardware/cad/params.py`.

| O que medir | Chave no `[medidas]` | Típico (mm) |
|-------------|----------------------|-------------|
| Suporte 2×18650: comprimento ao longo das células | `suporte_l` | 77,0 |
| Suporte: largura (duas células lado a lado) | `suporte_w` | 41,0 |
| Suporte: altura sem as células | `suporte_h` | 20,5 |
| Célula 18650 com o termo-retrátil (informativa: não muda a geometria, A-WP2-06) | `celula_d` | 18,4 |
| N20: seção da caixa de redução na vertical (D11) | `n20_altura` | 10,0 |
| N20: quanto o conector JST-SH passa do fundo do encoder | `n20_conector_l` | 5,0 |
| QTR-8A: entre centros dos dois furos das pontas | `qtr_furos_dist` | 69,85 |
| QTR-8A: centro dos furos até a borda dos pinos | `qtr_furo_borda` | 2,4 |
| QTR-8A: centro da fileira de pinos mais perto da borda | `qtr_pinos_borda` | 1,7 |
| QTR-8A: início do campo de pinos, a partir da ponta do furo do meio | `qtr_pinos_ini` | 27,2 |
| QTR-8A: fim do campo de pinos, mesma referência | `qtr_pinos_fim` | 55,4 |
| Perfboard: centro do furo de fixação até as bordas | `perf_furo_canto` | 3,5 |
| Chave liga/desliga: comprimento e altura do corpo | `interruptor_l` / `interruptor_h` | 8,5 / 3,6 |
| O-ring: diâmetro interno e seção | `oring_id` / `oring_cs` | 34,0 / 3,0 |
| Carro pronto: massa de fios e parafusos (seção 8) | `fios_parafusos_massa` | 10,0 g |

> **ATENÇÃO — QTR-8A antes da barra.** As quatro cotas `qtr_furo_borda`, `qtr_pinos_borda`,
> `qtr_pinos_ini` e `qtr_pinos_fim` foram estimadas em escala no diagrama da Pololu. A
> avaliação do WP2 mediu 2,65–2,73 mm para `qtr_furo_borda` e 1,27–1,36 mm para
> `qtr_pinos_borda` (A-WP2-21). Meça a placa em mãos e preencha o TOML **antes** de imprimir
> a barra e de rodar o `build.py`: com as cotas erradas, o conector do QTR bate na parede da
> janela.

Confira também, sem chave no TOML:

- **Colar do N20.** Meça quanto o colar ⌀4 sai da face da caixa. O cubo da roda fica a 0,6 mm
  dessa face; se o colar sair mais que isso, a roda raspa (A-WP2-07). Avise o projetista antes
  de imprimir as rodas.
- **Comprimento total do N20.** Varia com a redução (ficha 04).

Gere as peças com as medidas novas:

```bash
cd hardware/cad
python3 -m venv .venv && .venv/bin/pip install -r requirements.txt   # só na primeira vez
.venv/bin/python -m unittest discover -s tests
.venv/bin/python build.py
```

O `build.py` regenera os STL, os STEP e o relatório e sai com um código:

| Código | Significado | O que fazer |
|--------|-------------|-------------|
| 0 | Todas as verificações com ✔ | Imprimir |
| 1 | Alguma verificação com ✘. O relatório abre com a faixa **NÃO IMPRIMIR**; as peças saem só para inspeção. Também sai com 1 se o O-ring der roda fora de 38–44 mm (nada é exportado, A-WP2-20) | Não imprimir. Corrigir a medida (ou chamar o projetista) e rodar de novo |
| 2 | Medida inválida no TOML (chave desconhecida, zero ou negativa). Nada é exportado | Corrigir a linha indicada na mensagem `ERRO no medidas.toml` |

### 2.4 Peças e quantidades

Tabela "Peças para imprimir" do relatório de montagem, com a orientação em que o STL já vem:

| Arquivo | Qtd. | Orientação na mesa |
|---------|------|--------------------|
| `base.stl` | 1 | Face de baixo na mesa |
| `abracadeira_E.stl` | 1 | De lado (perfil em U de pé) |
| `abracadeira_D.stl` | 1 | De lado (perfil em U de pé) |
| `barra_sensores.stl` | 1 | Face de cima na mesa |
| `patim.stl` | 2 | Face de fixação na mesa, calota para cima |
| `calco.stl` | 4 | Plano (0,5 mm) |
| `calco_lingua.stl` | 2 | Plano (0,5 mm) |
| `roda.stl` | 2 | Face externa na mesa |
| `cupom.stl` | 1 | Plano |
| `cupom_chave.stl` | 1 | Plano |

As peças do carro (base, abraçadeiras, barra, patins e rodas) somam cerca de 59 g de PETG,
pelas massas do relatório.

**Calços.** Sem calço, a face dos sensores fica a 3,0 mm do chão (verificação "Face do sensor
ao chão" do relatório). Cada passo de 0,5 mm usa **um calço sob cada patim e um calço de língua**
(`hardware/cad/parts/sensor_bar.py`): 2 × `calco.stl` + 1 × `calco_lingua.stl`. Os 4 + 2 do
relatório dão 2 passos (até 4,0 mm). Para chegar a 6,0 mm (6 passos, o máximo do modelo)
imprima 12 + 6 (A-WP2-17). Com calços, troque o comprimento dos parafusos conforme a tabela
"Fixadores".

## 3 Montagem mecânica

Monte nesta ordem. Um passo = uma ação. Frente do carro = lado da barra de sensores.

1. Prense com o ferro os **4 insertos das colunas** do perfboard, de cima, até ficarem rentes ao
   topo de cada coluna.
2. Prense os **2 insertos da língua** da barra pela face de baixo da língua (a face que apoia na
   base), rentes à face.
3. Deite cada **N20 no berço** da base, com a face da caixa de redução encostada na parede de
   batente e o eixo passando pelo furo da parede.
4. Ponha a **abraçadeira** (E à esquerda, D à direita) sobre a caixa e comece os 2 M2 × 8 sem
   apertar.
5. Alinhe o achatado do furo em D da **roda** com o achatado do eixo e encaixe a roda,
   segurando o motor por trás para ele não sair do batente. O cubo passa por dentro do furo da
   parede.
6. Confira que a face da caixa continua encostada no batente e **aperte a abraçadeira** até as
   orelhas encostarem no berço, sem esmagar. O motor só fica preso para dentro pelo atrito da
   abraçadeira, por isso a roda entra antes do aperto (avaliação do WP2, atividade 12).
7. (Opcional) Ponha o **M2 × 4 da trava radial** em cada roda, contra o achatado do eixo, com
   pouco torque: a rosca pega só na parede do cubo (A-WP2-08).
8. Gire cada roda com a mão: ela não pode tocar a base nem o motor (folga mínima de 0,60 mm no
   relatório).
9. Encaixe um **O-ring** no canal de cada roda, sem torcer o cordão.
10. Solde a barra de pinos de 2,54 mm **por cima** do QTR-8A (lado oposto aos sensores).
11. Solde o **chicote de 11 fios AWG 30** (⌀ ≤ 0,6 mm) no conector fêmea. Fio mais grosso não
    cabe numa camada no canal da base (relatório, "Ligação do QTR-8A").
12. Prenda o **QTR-8A** sob os postes da barra com 2 M2 × 8 cabeça baixa, de baixo para cima,
    com os sensores para baixo e a borda dos pinos para trás. O conector sai pela janela da
    viga.
13. Passe o chicote por cima da língua e desça pelo rasgo na ponta de trás da língua (e do
    calço de língua, se houver).
14. Ponha cada **patim** (com os calços, se houver) sob a ponta da barra e aperte o M2 × 12 de
    cima para baixo.
15. Apoie a **língua** na base (com o calço de língua, se houver) e prenda com 2 M3 ISO 10642 ×
    8, por baixo, no escareado. A cabeça tem que ficar rente à face de baixo.
16. Deite o chicote no **canal de 7 × 1,8 mm** do topo da base, por baixo do berço, até subir
    entre os conectores dos N20.
17. Encaixe o **suporte 2×18650** no berço, por baixo das quatro abas dos cantos, sem células.
    Ele desliza com folga; as células saem por cima sem soltar nada (ficha 12).
18. Monte a **chave liga/desliga** no suporte da lateral esquerda da base.
19. Prenda o **porta-fusível** sob a base com a abraçadeira de nylon, pelos rasgos e pelo
    rebaixo.
20. Prenda o **perfboard** nas colunas com 4 M3 × 6, com a barra fêmea do ESP32 orientada
    para o USB ficar virado para trás (avaliação do WP2, atividade 12). Não encaixe o ESP32
    ainda: ele só entra depois do ajuste do MP1584 (seção 5).

## 4 Eletrônica e fiação

A fiação completa está em [`docs/WIRING.md`](../WIRING.md). Resumo da alimentação da v2:

```
Suporte 2×18650 (2S, 6,0–8,4 V)
    │
    ├── Fusível 3 A (lâmina mini, porta-fusível em linha)
    │
    ├── P-MOSFET de proteção contra inversão (recomendado)
    │
    └── Chave liga/desliga
            │
            ├── MP1584 buck ──► 5,0 V ──► ESP32 VIN ──► 3V3 ──► MCP3008 + QTR-8A
            │
            └── TB6612FNG VM (tensão do pack)
```

No perfboard:

1. Meça a distância entre as fileiras do ESP32 na placa em mãos (ficha 01) e só então fure e
   solde as barras fêmea.
2. Solde o MCP3008 com as fileiras a 7,62 mm (3 colunas livres entre elas, ficha 02) e o
   capacitor de 100 nF entre VDD e AGND, junto ao CI.
3. Ligue VDD e VREF do MCP3008 e o VCC do QTR-8A no **3V3** do ESP32, nunca em 5 V
   (`docs/WIRING.md`). Assim o DOUT do MCP3008 não põe 5 V no GPIO 12.
4. Solde o resistor de **10 kΩ de pull-down no GPIO 12** (MISO). O GPIO 12 é pino de
   *strapping*: em nível alto no boot a placa não liga (`docs/REVISAO_FIRMWARE.md`, item 1).
5. Solde os resistores de **10 kΩ de pull-up** do GPIO 36 e do GPIO 39 para o 3,3 V. Esses
   pinos não têm pull-up interno; sem o resistor o encoder direito conta sozinho.
6. Monte o divisor da bateria: 100 kΩ do +BAT (depois da chave) ao GPIO 34 e 47 kΩ do GPIO 34
   ao GND, mais o **capacitor de 100 nF** do GPIO 34 ao GND (F-10).
7. Ligue o **buzzer passivo por um transistor** (o GPIO 22 não aciona o buzzer direto, F-26).
8. Solde o resistor de 10 kΩ de pull-down no STBY do TB6612 (motores parados até o firmware
   liberar).
9. Ponha o capacitor de 100 µF no VM do TB6612, junto à placa.
10. Fixe o MP1584 com fita dupla-face: o módulo mini não tem furos (ficha 07).
11. Ligue os encoders aos GPIO 18/19 (esquerdo) e 36/39 (direito) pelo conector JST-SH de 6
    vias (ficha 04).

## 5 Checagens antes de energizar

Faça na ordem, com o multímetro. Não avance com um item fora do esperado.

1. **Continuidade do GND** entre ESP32, MCP3008, TB6612, MP1584 e QTR-8A — esperado: bipe de
   continuidade em todos os pares.
2. **Sem curto entre +BAT e GND**, com a chave ligada e sem células — esperado: mais de 1 kΩ.
3. **Fusível de 3 A instalado** no porta-fusível — esperado: continuidade através dele.
4. **ESP32 fora da barra fêmea** (desconectado) antes do passo 7.
5. **Polaridade das células.** Ponha as duas células conforme a marcação do suporte, **com a
   chave desligada** — esperado entre os fios do suporte: 7–8,4 V, com o + no fio vermelho
   (ficha 12).
6. Ligue a chave.
7. **Ajuste o MP1584 para 5,0 V com o ESP32 desconectado** — gire o trimpot e meça a saída;
   esperado: 5,0 V (ficha 07: o módulo sai de fábrica com tensão arbitrária).
8. **VM do TB6612** — esperado: a mesma tensão do pack medida no passo 5.
9. Desligue a chave, encaixe o ESP32 e ligue de novo.
10. **3V3 no MCP3008 (VDD/VREF) e no VCC do QTR-8A** — esperado: 3,3 V.
11. **STBY do TB6612** com o ESP32 sem firmware ou em reset — esperado: nível baixo (0 V), rodas
    paradas.

## 6 Primeira energização e firmware

1. Deixe a chave do carro **desligada** e ligue o ESP32 ao computador pelo USB. Sem o pack, o VM
   do TB6612 fica sem tensão e as rodas não giram.
2. Não segure o botão BOOT (GPIO 0) ao ligar: com ele apertado no boot a placa entra em modo de
   gravação e o firmware não roda (`docs/REVISAO_FIRMWARE.md`, item 1).
3. Grave o firmware:

   ```bash
   pio run -e esp32dev -t upload
   ```

4. Observe o que deve acontecer na primeira vez (`src/main.cpp`, `src/comm/Annunciator.cpp`):
   - **3 bipes curtos** logo ao ligar: não há configuração salva e o carro usa os valores de
     fábrica. É normal na primeira vez; some depois do primeiro **Salvar** pelo dashboard.
   - **LED piscando a 1 Hz**: estado IDLE, sem calibração dos sensores. É o esperado até a
     primeira calibração ([manual do operador](OPERACAO.md), seção 3).
5. Conecte o dashboard e anote o `stk` do `info` (folga de pilha das duas tasks). Nenhuma task
   deve ficar com menos de ~1 KB (avaliação do WP1, atividade 5).

## 7 Testes de bancada

Cada teste substitui o firmware principal (`platformio.ini`, ambientes `bench_*`). Rode com as
**rodas fora do chão**, a chave ligada (pack com células) e o monitor serial aberto:

```bash
pio device monitor -b 115200
```

Ao terminar, grave de novo o firmware principal (`pio run -e esp32dev -t upload`).

### 7.1 Motores — `pio run -e bench_motor -t upload`

- **Observe:** A frente → A ré → B frente → B ré → ambos frente → parado, 1,5 s cada, com duty
  de 700 em 1023 (`examples/motor_smoke_test.cpp`). O canal A é o motor esquerdo
  (`docs/WIRING.md`).
- **Se falhar:** motor parado → conferir VM, STBY (GPIO 23) e a solda do motor. Lado ou sentido
  trocado → **não refaça a fiação**: corrija depois pelo BLE com `invL`, `invR` ou `swap`
  ([manual do operador](OPERACAO.md), seção 7).

### 7.2 Encoders — `pio run -e bench_encoder -t upload`

- **Observe:** linhas `countL,deltaL,dirL,countR,deltaR,dirR` a cada 100 ms
  (`examples/encoder_smoke_test.cpp`). Gire cada roda com a mão: a contagem daquele lado muda e
  a do outro fica parada.
- **Se falhar:** contagem que corre sozinha no lado direito → falta o pull-up de 10 kΩ no GPIO
  36 ou 39. Contagem sempre zero → conector JST-SH ou alimentação do encoder. Sinal invertido →
  `einvL`/`einvR` pelo BLE.

### 7.3 Sensores, leitura bruta — `pio run -e bench_sensor_raw -t upload`

- **Observe:** 8 colunas `s0…s7` (0–1023) a cada 50 ms (`examples/sensor_raw_test.cpp`).
  Sobre a linha branca o valor cai; sobre o preto sobe (`docs/WIRING.md`).
- **Se falhar:** canal parado em 0 ou 1023 → solda do canal no MCP3008 ou fio do chicote. Todos
  parados → SPI (GPIO 12–15) ou alimentação de 3,3 V.

### 7.4 Calibração dos sensores — `pio run -e bench_sensor_cal -t upload`

- **Observe:** aperte o botão BOOT e varra a linha durante a janela de 3 s deste teste
  (`examples/sensor_calibration_test.cpp`); depois aparecem `pos=` e `lost=`. Com a linha no
  centro, `pos` perto de 0; movendo para um lado, `pos` muda de sinal.
- **Se falhar:** `lost=SIM` sobre a linha → refaça a varredura passando todos os sensores pela
  linha. Sinal de `pos` invertido → `sinv` pelo BLE no firmware principal.

### 7.5 PPR dos encoders — `pio run -e bench_velocity_cal -t upload`

- **Observe:** o teste pede "gire 10 voltas e envie 'f'" para a roda esquerda e depois para a
  direita (`examples/velocity_calibration_test.cpp`). Em seguida mostra `rpmL`/`rpmR` a cada
  200 ms. O PPR medido fica gravado e o firmware principal o usa.
- **Se falhar:** `ok=0` → nenhuma contagem durante as voltas (ver 7.2).

### 7.6 Movimentos — `pio run -e bench_differential -t upload`

- **Observe:** aperte BOOT. Sequência reto → curva à direita → curva à esquerda → giro horário
  → giro anti-horário → parado (`examples/differential_hw_test.cpp`). Cada movimento tem que
  ser visivelmente diferente.
- **Se falhar:** um lado gira ao contrário → anote e corrija no firmware principal com `invL`,
  `invR` ou `swap` pelo BLE. Este teste usa a configuração de fábrica, então continua mostrando
  o lado invertido mesmo depois da correção.

## 8 Lista final de conferência

- [ ] Cupom impresso e `[tolerancias]` preenchido
- [ ] Peças compradas medidas e `[medidas]` preenchido (inclusive as 4 cotas do QTR-8A)
- [ ] `build.py` saiu com código 0, sem a faixa NÃO IMPRIMIR
- [ ] Peças impressas no perfil de PETG, sem suporte
- [ ] Insertos rentes; abraçadeiras apertadas até encostar; rodas girando livres
- [ ] O-rings no canal, sem torção
- [ ] QTR-8A preso com os sensores para baixo; face dos sensores a 3,0 mm do chão
- [ ] Parafusos M3 da barra rentes à face de baixo da base
- [ ] Chicote do QTR no canal, sem fio esmagado
- [ ] Pull-down no GPIO 12, pull-ups nos GPIO 36/39, 100 nF no GPIO 34
- [ ] MCP3008 e QTR-8A no 3,3 V
- [ ] Fusível de 3 A instalado; P-MOSFET instalado (recomendado)
- [ ] MP1584 em 5,0 V medido antes de ligar o ESP32
- [ ] Os 6 testes de bancada passaram
- [ ] Carro pronto pesado; `fios_parafusos_massa` ajustado e `build.py` rodado de novo: carga
      nos patins entre 10 e 20 % (com fios atrás do eixo ela pode cair, A-WP2-19; D7)
- [ ] Tempo de montagem anotado: ______
