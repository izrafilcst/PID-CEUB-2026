# Design — Esquema de fiação ISO e guia de solda do QTR-8A

**Data**: 2026-10-22
**Autor**: Rafael Costa (com Claude)
**Status**: Rascunho, aguardando aprovação
**Escopo**: branch `feat/lfr-v2-retomada` (LFR v2, ESP32 DevKit V1 de 30 pinos)

---

## 1. Intenção

Hoje o repositório diz quais GPIO vão em cada peça (`docs/WIRING.md`, tabelas e um bloco
ASCII), mas **não há desenho de ligação**: nenhum SVG mostra as peças, os fios e os GPIO, e
o guia de montagem manda soldar o QTR-8A sem mostrar onde ficam os pontos de solda nem como
cada saída chega ao MCP3008. Quem monta precisa conferir três documentos e adivinhar a
ordem dos pinos.

O produto é um **desenho técnico de ligações, consultado na tela** durante a montagem do
perfboard, mais um **guia de solda do QTR-8A** que usa o mesmo desenho.

## 2. Objetivos, não-objetivos, anti-objetivos

**Objetivos**
- Uma prancha geral com todas as ligações do carro, cada fio rotulado com o GPIO e com a
  macro de `src/config.h` (ex.: `GPIO 25 · PIN_MOTOR_A_PWM`).
- Uma prancha do QTR-8A com os pontos de solda numerados, as duas variantes de cabeçote
  (11×1 e 8×2) e a ligação ao MCP3008 pino a pino.
- Guia passo a passo da preparação e solda do QTR-8A, com teste de continuidade antes de
  energizar, nos manuais (`MONTAGEM.md` e `manual-lfr.html`).
- Tabela de ligações do `WIRING.md` corrigida e com a ordem dos pinos do QTR-8A.

**Não-objetivos**
- Layout de PCB, netlist de EDA (KiCad etc.) ou roteamento do perfboard.
- Visualizador interativo (destaque de rede ao passar o mouse). A prancha é SVG estático.
- Migrar as 12 pranchas NBR existentes (`hardware/drawings/comp`, `assy`) para ISO.
- Alterar firmware ou o CAD.
- **Republicar a página privada do manual** (`claude.ai/artifact/9DdBtHQb1D2rkHPKL3ijsV`).
  O README manda republicar ao mudar os manuais, mas é efeito externo: ao fim do trabalho eu
  **pergunto antes** (decisão minha).
- **Acrescentar mitigação de hardware** (capacitores por canal etc.) nas pranchas pela
  hipótese da impedância de fonte (§7, item 4): as pranchas mostram o projeto como está.

**Anti-objetivos** (falha mesmo que "funcione")
- Um desenho que diverge de `src/config.h` sem ninguém perceber.
- Instrução de solda que, seguida à risca, queime os LEDs do QTR-8A (jumper 3.3V BYPASS
  com Vcc em 5 V) ou deixe a leitura invertida.
- Dado de pinagem copiado de fonte não oficial e apresentado como oficial.

## 3. Decisões do usuário

| # | Decisão |
|---|---------|
| U-1 | Desenho técnico **no padrão ISO**. |
| U-2 | Mostrar os **GPIO de cada peça**. |
| U-3 | Guia de **configuração do QTR-8A com pontos de solda** e **ligação ao MCP3008**. |
| U-4 | Consulta **na tela**, não impressa (sem limite de papel). |
| U-5 | Execução com **agentes especializados e um avaliador independente** no fim, que reexecuta as verificações (preferência permanente do usuário para trabalho em várias partes), com TDD. |

**Chamada minha sobre o ruflo.** Quando o documento nasceu, `claude-flow` e `ruv-swarm`
não conectavam. O `claude-flow` voltou durante a sessão (o `ruv-swarm` e o `flow-nexus`
seguem fora). Seguindo a preferência permanente, o trabalho é registrado no ruflo
(`claude-flow`), o executor roda em worktree isolado (com `git merge --ff-only` até a base
antes da guarda) e o avaliador é de **outro tipo** que o executor.

## 4. Fatos verificados do QTR-8A (Pololu #960)

Fontes: guia https://www.pololu.com/docs/0J12 (seções 1, 5 e 6) e figuras rotuladas
`0J621` (11×1), `0J622` (8×2) e esquema `0J614`, lidas em 2026-10-22.

- Vcc entre 3,3 e 5 V, mínimo 100 mA. Saídas analógicas de 0 V a Vcc; **tensão menor =
  mais reflexão** (linha branca sobre manta preta: sobre a linha a tensão cai).
- Dois pads **3.3V BYPASS**. Unir os dois contorna um estágio dos resistores dos LEDs e é o
  que deixa os LEDs brilhantes em 3,3 V. **Nunca** unir com Vcc em 5 V (guia, nota).
- **LEDON**: porta de um MOSFET, com pull-up interno. Solto = LEDs acesos.
- Cada saída é um fototransistor com **pull-up de 47 kΩ** (esquema 0J614): impedância de
  saída alta, relevante para o MCP3008 (ver §7, item 4).
- Cabeçote de 25 pinos de 2,54 mm **vai solto na caixa**, para 11×1 ou 8×2.
- Ordem dos pinos, lida nas figuras (vista pela face dos pads, a do cabeçote), com os
  sensores numerados 8…1 da esquerda para a direita:

```
11×1 : OUT8 OUT7 OUT6 OUT5 OUT4 OUT3 OUT2 OUT1 | LEDON  Vcc  GND
8×2  : fileira de cima   OUT8 … OUT1
       fileira de baixo  GND×6  Vcc  LEDON
3.3V BYPASS : 2 pads isolados, no extremo do lado do sensor 1
```

- Os textos do guia **não** trazem a ordem dos pinos nem a posição do bypass; só as figuras.
  A ordem acima vem das figuras (600 px) e deve ser **conferida na placa em mãos** por
  continuidade (passo do guia). Proveniência: figura do fabricante = `OFICIAL`; posição em
  milímetros dos pads = `MEDIR` (já é o caso em `medidas.toml`).

## 5. Entrega

### 5.1 Pranchas (SVG, `hardware/drawings/wiring/`)

| Prancha | Folha | Conteúdo |
|---------|-------|----------|
| E1 — Diagrama geral de ligações | A3 paisagem (decisão minha: cabe tudo com texto ≥ 2,5 mm) | Blocos ESP32 (30 pinos, com a ordem real da placa), MCP3008, QTR-8A, TB6612FNG, 2× N20 + encoder (JST-SH 6 vias), MP1584, cadeia bateria → fusível → P-MOSFET → chave, divisor VBAT, buzzer + transistor, botão e LED da placa, pull-ups/pull-down, capacitores. Fios ortogonais, com código de cores por classe (alimentação, SPI, analógico, PWM/direção, encoder) e legenda. **Lista de ligações** (terminal a terminal) na própria folha. |
| E2 — QTR-8A: solda e ligação ao MCP3008 | A4 ou A3 paisagem (escolha do construtor) | Face dos pads nas duas variantes (11×1 e 8×2), pontos de solda S1…Sn numerados, jumper 3.3V BYPASS, orientação sobre o carro (frente para cima, pinos para trás, OUT8 à esquerda), MCP3008 PDIP-16 com CH0–CH7, SPI e alimentação, tabela QTR → fio → pino do DIP-16 → canal. Vista de cima do QTR cotada pelo passo de 2,54 mm (único dado oficial em milímetros). |

### 5.2 Convenções ISO

Folha ISO 5457 (A3/A4, margem de arquivamento, marcas de centragem e referência de zona);
legenda ISO 7200; larguras de linha ISO 128-20 (série 0,25 / 0,35 / 0,5 / 0,7 mm); escrita
ISO 3098 forma B; cotagem ISO 129-1 e símbolo do 1º diedro (ISO 5456) onde houver vista de
peça; símbolos elétricos IEC 60617; regras de documento de ligação IEC 61082-1. O
`tools/techdraw` atual cita NBR (equivalentes) e só tem A4: o construtor estende `Sheet`
para A3 e legenda ISO 7200 **sem alterar o resultado das 12 pranchas existentes**.

### 5.3 Geração e fonte única

- Gerador em `tools/` (nome livre, sugestão `gen_wiring_drawings.py`) compondo sobre
  `tools/techdraw`, no mesmo estilo de `gen_assembly_drawings.py`.
- **Fonte única**: os GPIO saem de `src/config.h` (lido, não copiado). Um teste compara a
  lista de ligações gerada com as macros `PIN_*` e falha se divergirem.
- A ordem dos pinos do QTR-8A, do MCP3008 e do DevKit entra como dado com proveniência
  (`Prov`), no `specs.py` ou módulo vizinho.

### 5.4 Documentação

- `docs/manual/MONTAGEM.md`: nova subseção de preparo do QTR-8A com os pontos S1…Sn; os
  passos 10–12 atuais passam a remeter a ela.
- `docs/WIRING.md`: tabela do QTR-8A com a ordem real, mapeamento corrigido (§6), links
  para as pranchas.
- `docs/manual/manual-lfr.html`: mesma subseção, com **E1/E2 embutidas como SVG inline**
  (o verificador proíbe recurso externo; o manual tem de abrir sem rede). Regerar respeitando
  as regras 8 e 9 de `tools/check_manuais.py` (hash do commit-fonte no rodapé).

## 6. Decisão técnica minha: mapeamento OUT → canal

O firmware trata o **CH0 como o sensor da esquerda** (`Calibration::weightedPosition`:
`pos` negativo em `i = 0`; `error > 0` = linha à direita). Com o QTR-8A montado como manda o
manual (pinos para trás), o **OUT1 fica à direita** do carro. A tabela atual (OUT1→CH0)
inverteria a leitura.

Decisão: ligar **OUT8→CH0, OUT7→CH1 … OUT1→CH7** e corrigir o `WIRING.md`. O `sinv` (BLE) e
a linha de diagnóstico do `OPERACAO.md` ficam como rede de segurança. Sem mudança de
firmware.

## 7. Fica para o construtor

1. Geometria do layout E1, roteamento dos fios e paleta de cores (acessível, distinguível
   em tela clara e escura, com fundo branco explícito como as pranchas atuais).
2. Quantas variantes de rotulagem de GPIO por bloco (só número ou número + macro).
3. Ordem dos pinos do DevKit V1 de 30 pinos: tomar de fonte rastreável (a ficha 01 cita o
   pinout da Mischianti), marcar `TIPICO` e pôr no guia o aviso de conferir na placa.
4. **Impedância de fonte do QTR-8A no MCP3008.** A saída de 47 kΩ com SPI a 1 MHz
   (amostragem de 1,5 ciclo de clock) pode causar arrasto entre canais vizinhos. O achado
   **já existe**: F-36 em `docs/REVISAO_FIRMWARE.md`, com plano de medição na bancada (um
   canal sobre a linha; se houver arrasto, baixar o clock ou descartar a 1ª conversão).
   Conferir no datasheet (DS21295D, ficha 02) os valores de CSAMPLE e RSS e **acrescentar a
   conta ao F-36** em vez de criar achado novo. Decisão minha: só registro, sem mitigação nas
   pranchas e sem alterar firmware neste trabalho.
5. Nome dos arquivos, estrutura dos testes e divisão em tarefas.
6. **Conector JST-SH de 6 vias do N20** (E1 desenha o encoder): o repositório só diz "6
   vias, passo 1,0 mm" e não traz a ordem dos pinos. Se o usuário não responder a tempo:
   motor da Pololu = `OFICIAL` (ler a ordem na página do produto), motor genérico = `TIPICO`
   com aviso de conferir com o multímetro antes de energizar; os terminais AO1/AO2/BO1/BO2
   do TB6612 entram só por nome.

## 8. Verificação (TDD)

- Testes `unittest` em `tools/tests/` (não há `pytest` nesta máquina): lista de ligações
  igual às macros de `config.h`; cada GPIO usado aparece exatamente uma vez; campos da
  legenda ISO 7200; `Sheet` A4 existente inalterado; tabela OUT→CH coerente com §6.
- Renderização com `cairosvg` e **inspeção visual de cada PNG** (texto estourando, fio
  cruzando bloco, legenda cortada). Nenhuma prancha é dada como pronta sem isso.
- `python3 tools/check_manuais.py` limpo.
- **Avaliador independente** (agente de outro tipo que o executor) reexecuta os comandos,
  confere o desenho contra `config.h`, contra as figuras da Pololu e contra o MCP3008, e
  entrega relatório com matriz de conformidade, achados e veredito em `docs/avaliacao/`.
