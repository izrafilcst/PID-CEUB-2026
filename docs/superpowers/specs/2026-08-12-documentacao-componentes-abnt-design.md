# Design — Documentação dimensional dos componentes e monografia ABNT

**Data**: 2026-08-12
**Autor**: Rafael Costa (com Claude)
**Status**: Aprovado em brainstorming, aguardando revisão do spec
**Escopo**: branch `main` (ESP32 DevKit V1)

---

## 1. Objetivo

Produzir dois conjuntos de documentos:

1. **Pacote dimensional** — uma ficha e uma prancha SVG cotada para cada componente
   comprado, mais duas pranchas de montagem, suficientes para modelar o robô em CAD
   sem consultar mais nada.
2. **Monografia ABNT** — documento acadêmico completo sobre o projeto, com explicação
   do funcionamento, da arquitetura e das funções do firmware da branch `main`.

O pacote dimensional é insumo para modelagem futura do chassi. A monografia é
entregável acadêmico.

---

## 2. Decisões travadas

| # | Decisão | Valor |
|---|---------|-------|
| DEC-1 | Array de sensores | **Pololu QTR-8A** (passo 9,525 mm, 8 saídas analógicas) |
| DEC-2 | Formato da monografia | **HTML + CSS de impressão**, exportado a PDF pelo navegador |
| DEC-3 | Escopo das fichas/pranchas | **Somente as 10 peças compradas** |
| DEC-4 | Diagrama de montagem | **Duas pranchas**: explodida + layout funcional cotado |
| DEC-5 | Profundidade da monografia | Monografia completa **+ apêndice de referência de API** |
| DEC-6 | Base geométrica da montagem | **Layout funcional**, não vinculado ao chassi atual |

### Justificativa de DEC-6

O `body_f1.scad` posiciona os motores em Y=170 e o slot da barra de sensores em
Y=177 — mesma extremidade —, com o ball caster em Y=135 entre eles. Isso contradiz
`LOOKAHEAD_MM 70` do `config.h`, que pressupõe os sensores 70 mm à frente do eixo
motriz. Desenhar a montagem sobre essa geometria propagaria o erro para o modelo CAD.
A prancha A1 define as relações funcionais com origem no eixo motriz; o chassi é
modelado depois, em volta delas.

---

## 3. Componentes cobertos

| # | Componente | Fabricante / código | Datasheet oficial? |
|---|-----------|--------------------|--------------------|
| 01 | ESP32 DevKit V1 (38 pinos) | DOIT / clone WROOM-32 | Parcial — módulo WROOM-32 tem datasheet Espressif; a placa é clone |
| 02 | MCP3008 | Microchip, DIP-16 | Sim — desenho de encapsulamento no datasheet |
| 03 | QTR-8A | Pololu #960 | Sim — desenho dimensional Pololu |
| 04 | Micro metal gearmotor N20 + encoder | Pololu (código a confirmar) | Sim — desenho dimensional Pololu |
| 05 | TB6612FNG breakout | SparkFun ROB-14450 | Placa: dimensões SparkFun; CI: datasheet Toshiba |
| 06 | LiPo 2S 800 mAh 30C | Genérico | Não — dimensões variam por fabricante |
| 07 | MP1584EN buck | Módulo genérico; CI da MPS | CI sim; módulo não |
| 08 | Roda silicone ⌀34 mm | Genérico | Não |
| 09 | Ball caster ½" | Pololu / SparkFun (código a confirmar) | Sim |
| 10 | Perfboard 50×70 mm | Genérico, passo 2,54 mm | Não — dimensões nominais |

### Proveniência das cotas

Toda medida recebe um de três níveis, propagado para a ficha e para a prancha:

| Símbolo | Nível | Significado |
|---------|-------|-------------|
| ● | `OFICIAL` | Extraída de datasheet do fabricante. Registra URL e figura/página de origem. |
| ○ | `TIPICO` | Módulo genérico sem datasheet. Valor de consenso entre fornecedores. |
| △ | `MEDIR` | Varia por lote ou fornecedor. **Medir a peça em mãos antes de modelar.** |

Uma cota `MEDIR` não é uma falha do documento — é a informação de que aquela medida
não pode ser garantida à distância. O LiPo é o caso central: "2S 800 mAh 30C" não
define dimensão alguma.

---

## 4. Estrutura de arquivos

```
hardware/
├── components/
│   ├── specs.py                      # fonte única: cotas + proveniência + massas
│   └── fichas/
│       ├── 01-esp32-devkit-v1.md
│       ├── 02-mcp3008.md
│       ├── 03-qtr-8a.md
│       ├── 04-motor-n20-encoder.md
│       ├── 05-tb6612fng.md
│       ├── 06-lipo-2s-800mah.md
│       ├── 07-mp1584.md
│       ├── 08-roda-34mm.md
│       ├── 09-ball-caster.md
│       └── 10-perfboard-5x7.md
└── drawings/
    ├── comp/
    │   ├── 01-esp32-devkit-v1.svg
    │   ├── … (02 a 09)
    │   └── 10-perfboard-5x7.svg
    └── assy/
        ├── A1-layout-funcional.svg
        └── A2-vista-explodida.svg

tools/
├── techdraw.py                       # primitivas de desenho técnico
├── gen_component_drawings.py         # gera as 10 pranchas de componente
├── gen_assembly_drawings.py          # gera A1 e A2
├── verify_drawings.py                # aritmética + render SVG→PNG
└── lint_abnt.py                      # linter estrutural da monografia

docs/
├── abnt/
│   ├── monografia.html
│   ├── abnt.css
│   └── vendor/paged.polyfill.js
└── DIVERGENCIAS.md
```

### Conteúdo de cada ficha

1. Identificação — denominação, fabricante, código, fornecedor, preço da BOM
2. Link do datasheet e referência ABNT NBR 6023 correspondente
3. Tabela de dimensões com coluna de proveniência
4. Massa (necessária para o cálculo de CG na prancha A1)
5. Pinagem / interface elétrica
6. Características elétricas relevantes ao projeto
7. Notas de montagem — furação, folga, altura ao solo, orientação
8. Remissão à prancha correspondente

A referência NBR 6023 de cada datasheet é reaproveitada nas Referências da monografia.

---

## 5. Convenções de desenho técnico

As pranchas seguem as normas ABNT aplicáveis:

| Norma | Objeto | Aplicação |
|-------|--------|-----------|
| NBR 10068 | Leiaute da folha | A4 paisagem (297×210 mm), margem esquerda 25 mm, demais 10 mm, legenda no canto inferior direito |
| NBR 10067 | Representação | Projeção ortográfica em 1º diedro, com símbolo do diedro na legenda |
| NBR 8403 | Larguras de linha | Contorno visível 0,5 mm · oculto 0,35 mm tracejado · centro 0,25 mm traço-ponto · cota e chamada 0,25 mm |
| NBR 10126 | Cotagem | Setas cheias, cota acima da linha, leitura pela esquerda e por baixo, ⌀ para diâmetro, R para raio |
| NBR 8196 | Escalas | Somente escalas normalizadas (2:1, 1:1, 1:2). Escolhida por peça de modo a ocupar a folha sem apertar as cotas, e declarada na legenda. Peças pequenas (MCP3008, ball caster) tendem a 2:1; as demais a 1:1 |

**Vistas**: superior, frontal e lateral esquerda quando a peça exige (N20, roda,
caster); superior + lateral para as PCBs planas. Furos de fixação cotados a partir de
uma aresta de referência, com ⌀ e linha de centro.

**Legenda** (NBR 10068): denominação da peça, fabricante e código, escala, unidade
(mm), data, autor, número da prancha, símbolo do 1º diedro.

**Quadro de proveniência**: canto superior direito de cada prancha, com a legenda dos
três símbolos. Cada cota na prancha carrega o símbolo do seu nível.

**Apresentação**: monocromático, fundo branco explícito no SVG — a prancha não pode
desaparecer em visualizador de tema escuro. Folga generosa entre textos, porque
`cairosvg` e o navegador têm métricas de fonte diferentes.

### `tools/techdraw.py`

Biblioteca de primitivas, escrita uma vez e usada por todas as pranchas:

| Primitiva | Função |
|-----------|--------|
| `Sheet` | Moldura A4, margens, legenda, quadro de proveniência |
| `view()` | Contexto de vista com escala e origem próprias |
| `dim_h` / `dim_v` | Cota linear horizontal / vertical, com símbolo de proveniência |
| `dim_diam` / `dim_rad` | Cota de diâmetro / raio |
| `centerline` | Linha de centro traço-ponto |
| `hole` | Furo com linhas de centro cruzadas |
| `hidden` | Aresta oculta tracejada |
| `note` | Chamada com linha-guia |
| `balloon` | Balão numerado para a lista de peças (A2) |
| `iso_box` / `iso_cyl` | Sólidos simplificados em isométrica (A2) |
| `parts_list` | Tabela de peças (item, qtd, denominação, referência, prancha) |

Rejeitada a alternativa de um motor genérico dirigido por YAML: desenho técnico tem
exceção em toda prancha (uma vista de corte aqui, um detalhe ampliado ali) e um motor
genérico viraria um DSL mal-especificado. Cada script de prancha compõe suas vistas
livremente sobre as primitivas.

---

## 6. Pranchas de montagem

### A1 — Layout funcional cotado (planta)

Sistema de coordenadas explícito na prancha:

- **Origem O** no ponto médio do eixo motriz, ao nível do solo
- **X positivo** no sentido de marcha · **Y positivo** para a esquerda · **Z positivo** para cima

Arranjo triciclo com sensores à frente:

```
   X = +70 ┤ ══════════ plano dos sensores (QTR-8A)
           │
   X =   0 ┤ ──●────────────────●──  eixo motriz (roda E / roda D)
           │
   X <   0 ┤          ○          caster (apoio traseiro)
```

Cotas fixadas pela prancha:

| Grandeza | Origem do valor |
|----------|-----------------|
| Lookahead sensores → eixo motriz | `LOOKAHEAD_MM 70` (`config.h`) — âncora do layout |
| Bitola (plano médio roda E ↔ roda D) | Derivada de `MOTOR_SPACING 80` e do comprimento de eixo do N20 (ficha 04); a definição exata de onde `MOTOR_SPACING` é medido está ambígua no SCAD e é resolvida na Fase 3 |
| Entre-eixos eixo motriz ↔ caster | Calculado na Fase 3 a partir do balanço de CG |
| Posição longitudinal do LiPo | Calculada na Fase 3 a partir do balanço de CG |
| Altura dos sensores ao solo | `SENSOR_HEIGHT_MM 4` |
| Altura do eixo motriz ao solo | ⌀roda / 2 = 17 mm |
| Envelopes ESP32 / LiPo / perfboard | Fichas 01, 06, 10 |
| Envelope de competição 250×250 mm | Linha traço-duplo-ponto — comprova conformidade |

Duas restrições de projeto anotadas na prancha:

1. **O centro de gravidade deve cair dentro do triângulo de apoio** (roda E, roda D,
   caster). Com caster traseiro, isso posiciona o LiPo atrás do eixo motriz.
2. **O CG deve ficar próximo ao eixo motriz** para carregar as rodas de tração.
   Afastado demais, o robô patina na aceleração.

A posição do caster e do LiPo é resolvida a partir das massas registradas nas fichas,
não arbitrada.

**Ressalva sobre o balanço de massas**: poucos fornecedores publicam massa. Pololu
publica a dos seus itens (motor, caster); as demais serão `TIPICO` ou `MEDIR`. O CG
calculado é, portanto, **indicativo** — dimensiona o layout, mas deve ser conferido com
o robô montado antes da competição. A prancha A1 registra essa condição e informa a
margem disponível entre o CG estimado e a borda do triângulo de apoio, para que se
saiba quanto erro de massa o arranjo tolera.

### A2 — Vista explodida

Isométrica, componentes separados ao longo dos eixos de montagem, trajetórias em
traço-ponto, balões numerados ligados a uma lista de peças (item, quantidade,
denominação, referência do fornecedor, prancha correspondente).

**Representação simplificada, declarada na própria prancha**: os componentes aparecem
como prismas e cilindros com as dimensões corretas do envelope, rotulados — não como
réplicas geométricas. Modelar em SVG a carcaça do N20 com sua caixa de redução, ou os
conectores do ESP32 em isométrica, custaria mais que o restante do trabalho sem
auxiliar a montagem. O detalhe fino está nas pranchas individuais.

---

## 7. Monografia ABNT

### Conformidade normativa

| Norma | Objeto |
|-------|--------|
| NBR 14724 | Estrutura e apresentação de trabalhos acadêmicos |
| NBR 6024 | Numeração progressiva das seções |
| NBR 6027 | Sumário |
| NBR 6028 | Resumo |
| NBR 6023 | Referências |
| NBR 10520 | Citações |

Formatação implementada no `abnt.css`:

- A4; margens esquerda e superior 3 cm, direita e inferior 2 cm
- Corpo em 12 pt, entrelinha 1,5, texto justificado, recuo de parágrafo 1,25 cm
- Citações longas, legendas, notas e referências em 10 pt, espaçamento simples
- Paginação em algarismos arábicos no canto superior direito, contagem desde a folha
  de rosto, exibição a partir da introdução
- Cada seção primária abre em página nova
- Figuras: legenda acima (`Figura N – Título`), fonte abaixo
- Tabelas no padrão IBGE, sem bordas laterais, título acima e fonte abaixo
- Código-fonte em **Quadro** de bordas fechadas, monoespaçado 10 pt

### Paginação via Paged.js

`@page` puro no Chrome não implementa `target-counter()`, o que impossibilitaria um
sumário com números de página reais. A solução é vendorizar `paged.polyfill.js` em
`docs/abnt/vendor/` — mesmo padrão que o projeto já adota com o `motion.js` do
dashboard — obtendo sumário, lista de figuras, lista de quadros e cabeçalhos correntes
com numeração automática.

**Degradação**: sem o JS, o `@page` ainda imprime com formatação correta; perde apenas
os números do sumário. Se o carregamento via `file://` apresentar problema, servir a
pasta com `python3 -m http.server` resolve.

### Estrutura

**Elementos pré-textuais**
Capa · Folha de rosto · Folha de aprovação · Resumo · Abstract · Lista de figuras ·
Lista de quadros · Lista de tabelas · Lista de siglas · Lista de símbolos · Sumário

**1 INTRODUÇÃO**
1.1 Contextualização · 1.2 Problema · 1.3 Objetivos (geral e específicos) ·
1.4 Justificativa · 1.5 Estrutura do trabalho

**2 FUNDAMENTAÇÃO TEÓRICA**
2.1 Robôs seguidores de linha · 2.2 Sensoriamento óptico refletivo · 2.3 Controle PID ·
2.4 Controle em cascata · 2.5 Modulação PWM e ponte H · 2.6 Encoders em quadratura e
odometria · 2.7 Sistemas embarcados de tempo real · 2.8 Desenvolvimento orientado a
testes

**3 MATERIAIS E MÉTODOS**
3.1 Requisitos derivados do regulamento · 3.2 Componentes selecionados (remete ao
Apêndice D) · 3.3 Arquitetura elétrica · 3.4 Arquitetura mecânica (remete a A1 e A2) ·
3.5 Ferramentas e ambiente · 3.6 Metodologia TDD adotada

**4 DESENVOLVIMENTO DO FIRMWARE**
4.1 Visão geral em camadas · 4.2 Configuração (`config.h`) · 4.3 Camada de aquisição
(`SensorArray`, `Calibration`) · 4.4 Camada de odometria (`Encoder`,
`VelocityEstimator`) · 4.5 Camada de controle (`PIDController`, `SpeedProfile`,
`VelocityProfile`, `CascadeController`) · 4.6 Camada de atuação (`MotorDriver`,
`DifferentialDrive`) · 4.7 Camada de estratégia (`LineFollower`, `LapTimer`) ·
4.8 Camada de comunicação (`BLETuner`, `Logger`) · 4.9 Camada de persistência
(`IPersistentStore`, `NvsStore`, `InMemoryStore`) · 4.10 Orquestração (`main.cpp`,
máquina de estados, distribuição dual-core)

**5 VERIFICAÇÃO E RESULTADOS**
5.1 Suítes de teste e cobertura · 5.2 Orçamento temporal do laço de 2 ms ·
5.3 Simulações · 5.4 Procedimento de calibração e sintonia · 5.5 Conformidade com o
regulamento

**6 CONCLUSÃO**
6.1 Considerações finais · 6.2 Trabalhos futuros

**Elementos pós-textuais**
Referências · Apêndice A (referência de API dos módulos) · Apêndice B (pinagem
completa e mapa de GPIO) · Apêndice C (pranchas de desenho técnico) · Apêndice D
(fichas dimensionais)

### Capítulo 4 e Apêndice A

O capítulo 4 é escrito lendo o código-fonte, módulo por módulo: o que faz, como
funciona, decisões de projeto e os trechos que merecem explicação — derivada calculada
sobre a medição em vez do erro, anti-windup por clamping, ISR de quadratura 4×, ring
buffer SPSC do `Logger`, guardas `NATIVE_BUILD`.

O Apêndice A tabela cada função pública dos módulos de `src/`: assinatura, parâmetros,
retorno, pré-condições e efeitos colaterais.

### Dados pendentes

Capa e folha de rosto exigem nome completo do autor, curso, disciplina, orientador e
semestre. Ficam como marcadores visíveis no HTML até serem fornecidos.

---

## 8. Verificação

### Desenhos — `tools/verify_drawings.py`

**Nível aritmético**: soma das cotas parciais confere com a cota total; furos caem
dentro do contorno; a escala declarada na legenda corresponde à geometria desenhada.

**Nível visual**: `cairosvg` renderiza cada prancha em PNG a 200 dpi para inspeção
direta — cota sobreposta, texto estourando a moldura, vista ausente, legenda cortada.
Nenhuma prancha é considerada pronta sem essa inspeção.

### Cotas

Toda medida de nível `OFICIAL` no `specs.py` registra a URL do datasheet e a
figura/página de origem, permitindo conferência independente.

### Monografia — `tools/lint_abnt.py`

Não há navegador headless disponível nesta máquina, portanto **a paginação renderizada
não pode ser verificada automaticamente**. O linter cobre o que é verificável no
código-fonte HTML:

- Seções numeradas em sequência, sem saltos
- Toda figura com legenda acima e fonte abaixo
- Nenhuma referência cruzada órfã (`Figura N`, `Quadro N`, `Tabela N` inexistentes)
- Toda sigla da lista efetivamente declarada no texto na primeira ocorrência
- Todo item das Referências citado no corpo, e toda citação presente nas Referências

A conferência visual da paginação fica com o autor, via print preview do navegador.
Isso é declarado explicitamente ao encerrar a Fase 6 — não será apresentado como
verificado.

---

## 9. Fases de execução

| Fase | Entrega | Portão de saída |
|------|---------|-----------------|
| 1 | `specs.py` + 10 fichas; datasheets obtidos de Microchip, Pololu, Toshiba/SparkFun e MPS | Toda cota com nível de proveniência e fonte rastreável |
| 2 | `techdraw.py` + 10 pranchas de componente | 10 PNGs inspecionados; verificação aritmética limpa |
| 3 | A1 (layout funcional) e A2 (explodida) | CG dentro do triângulo de apoio; envelope 250×250 mm conferido; 2 PNGs inspecionados |
| 4 | `abnt.css` + Paged.js + capítulos 1, 2, 3, 5 e 6 | Linter estrutural limpo |
| 5 | Capítulo 4 + Apêndice A, escritos a partir da leitura de `src/` | Toda função pública tabelada |
| 6 | Apêndices C e D, Referências NBR 6023, revisão final | Linter limpo + conferência do print preview pelo autor |

---

## 10. Registro de divergências (`docs/DIVERGENCIAS.md`)

Inconsistências identificadas no repositório durante o levantamento. Nenhuma é
corrigida silenciosamente.

| # | Divergência | Encaminhamento |
|---|-------------|----------------|
| D1 | `bom.csv` e `CLAUDE.md` especificam "8× TCRT5000"; `WIRING.md` especifica QTR-8A | Decidido QTR-8A (DEC-1). Proposta: corrigir `bom.csv` e `CLAUDE.md` |
| D2 | `sensor_bar.scad` modela pockets para TCRT5000 discretos, incompatível com a PCB única de 75×13 mm do QTR-8A | Registrado. Remodelagem é responsabilidade do autor |
| D3 | `body_f1.scad`: motores em Y=170 e barra de sensores em Y=177 (mesma extremidade); caster em Y=135 entre eles — contradiz `LOOKAHEAD_MM 70` | Registrado. A prancha A1 define o layout correto |
| D4 | `CASTER_D 15` no SCAD contra 12,7 mm (½") na BOM | Prevalece a peça real, confirmada na Fase 1 |
| D5 | `SENSOR_SPACING_MM 10` (`config.h`) contra passo real de 9,525 mm do QTR-8A | Firmware permanece como está — `WIRING.md` já justifica o efeito como cosmético. Ficha e A1 usam 9,525 mm |
| D6 | BOM cita "ball caster ROB-09259" (SKU SparkFun) com link Robocore, e "Pololu 3081" para o N20 | Códigos reais verificados na Fase 1 |

---

## 11. Fora de escopo

- Alteração dos arquivos `.scad` — a remodelagem do chassi é trabalho posterior do autor
- Alteração do firmware
- Geração de arquivos STL
- Fichas e pranchas de componentes passivos e elementos de fixação (resistores,
  capacitores, parafusos, porcas, espaçadores, fios, conectores)
- Branches diferentes de `main`

A única alteração proposta fora de documentação nova é a correção de D1 em `bom.csv` e
`CLAUDE.md`, sujeita a confirmação.
