# Registro de divergências

Inconsistências entre as fontes do repositório (`bom.csv`, `src/config.h`,
`hardware/chassis/*.scad`, `docs/`) e os dados dimensionais levantados nas
fichas de `hardware/components/fichas/`.

Levantado em 12 ago. 2026, durante a execução do plano
`docs/superpowers/plans/2026-08-12-pacote-dimensional-componentes.md`.

**Como ler**: `Resolvido` = corrigido no repositório. `Aceito` = a diferença
existe e foi decidido conviver com ela, com a razão registrada. `Aberto` =
ainda precisa de trabalho.

| ID | Arquivos afetados | Divergência | Evidência | Decisão | Situação |
|----|-------------------|-------------|-----------|---------|----------|
| D1 | `hardware/bom.csv`, `CLAUDE.md`, `README.md`, `docs/RELATORIO_FINAL.md` | Sensor de linha citado como 8× TCRT5000 discretos em vez do array único QTR-8A | Commit `cf16a1c` corrigiu bom.csv, CLAUDE.md e README.md | Prevalece o QTR-8A (Pololu 960). `docs/RELATORIO_FINAL.md` linhas 35 e 261 ficam como registro histórico datado | **Resolvido** |
| D2 | `hardware/chassis/sensor_bar.scad` | A barra modela 8 bolsões de 5,5 × 1,8 mm com passo 10 mm para TCRT5000 discretos; o QTR-8A é uma PCB única de 74,93 × 12,70 × 3,18 mm com passo 9,525 mm | `sensor_bar.scad` linhas 7-12 (`SENSOR_W 5.5`, `SENSOR_SPACING 10`); ficha 03 | Remodelar como berço para uma PCB única. `SENSOR_BAR_L = 90` também sobra sobre os 74,93 mm reais | **Aberto** |
| D3 | `hardware/chassis/body_f1.scad`, `src/config.h` | Lookahead geométrico do chassi é de 7 mm, contra `LOOKAHEAD_MM 70` do firmware | `body_f1.scad`: `BODY_LENGTH 180`, motores em `BODY_LENGTH - MOTOR_OFFSET` = 170, slot da barra em `BODY_LENGTH - 3` = 177 → 177 − 170 = 7 mm | Prevalece o layout da prancha A1: sensores em X = +70 mm, caster em X = −55 mm | **Aberto** |
| D4 | `hardware/chassis/body_f1.scad`, `hardware/bom.csv` | Diâmetro do ball caster: `CASTER_D = 15` mm no SCAD contra ½ pol = 12,7 mm na BOM | `body_f1.scad` linha 37; `bom.csv` "Ball caster metálico … 1/2 pol (12.7 mm)" | Prevalece 12,7 mm, que é o produto efetivamente listado na Robocore. Corrigir o SCAD | **Aberto** |
| D5 | `src/config.h`, ficha 03 | `SENSOR_SPACING_MM 10` no firmware contra passo real de 9,525 mm do QTR-8A | Pololu 960: *sensor pitch* 0,375 in = 9,525 mm | Manter 10 no firmware: a diferença de 4,75 % é absorvida pelo Kp e pela calibração, e o valor só escala o erro de posição | **Aceito** |
| D6 | `hardware/bom.csv` | Três códigos/especificações incorretos de fornecedor | (a) SKU Pololu 3081 é o *Magnetic Encoder Pair Kit*, 12 CPR — o par de encoders, não o motor; (b) encoder descrito como "Hall 7 PPR", é magnético de 12 CPR no eixo do motor; (c) driver citado como SparkFun ROB-14450, o SKU correto é **ROB-14451** | Corrigir a `bom.csv`. O código do motor N20 continua **a confirmar**: a forma foi documentada pelo desenho dimensional da Pololu, mas a unidade adquirida é genérica | **Aberto** |
| D7 | `hardware/components/specs.py`, prancha A1 | Sete das dez massas são estimadas, não pesadas: ESP32 9,5 g · MCP3008 1,0 g · N20 10,5 g · TB6612 2,5 g · LiPo 48 g · MP1584 1,8 g · roda 6,0 g · caster 2,0 g · perfboard 12,0 g. Só o QTR-8A (3,09 g) é oficial. O chassi entra com 40 g estimados | `notas_montagem` de cada ficha marcam "MASSA ESTIMADA — PESAR" | O CG da prancha A1 (−20,10 mm) é **indicativo**. Pesar cada peça e regenerar A1 antes de fechar o chassi | **Aberto** |
| D8 | `src/config.h`, ficha 03 | `SENSOR_HEIGHT_MM 4` contra distância ótima de leitura de 3 mm do QTR-8A | Pololu 960: *optimal sensing distance* 0,125 in = 3 mm; máxima recomendada 6 mm | Aceito: 4 mm está dentro da faixa útil e dá folga contra a manta. Se a leitura ficar fraca na calibração, baixar para 3 mm pelo slot de ajuste do suporte | **Aceito** |
| D9 | `hardware/chassis/wheel_hub.scad` | Furo do eixo com 10 mm de profundidade num cubo de 8 mm de largura — não fecha | `wheel_hub.scad`: `WHEEL_WIDTH = 8`, `AXLE_DEPTH = 10` | Acrescentar ressalto de 2 mm na face interna do cubo: mantém os 10 mm úteis do eixo do N20 (ficha 04) totalmente encaixados sem alargar a roda | **Aberto** |
| D10 | `hardware/chassis/body_f1.scad`, prancha A1 | Bitola: `MOTOR_SPACING = 80` mm no SCAD contra 76 mm adotados na A1. `BODY_WIDTH = 120` mm também sobra sobre a largura total de 84 mm do conjunto rodas | `body_f1.scad` linhas 9 e 29 | Prevalece a A1 enquanto o chassi não for remodelado. A bitola final sai do par motor + roda medido | **Aberto** |

---

## O que fechar antes de modelar o chassi

1. **Pesar as dez peças** e substituir as massas em `hardware/components/specs.py`
   (D7). Sem isso o CG da A1 não passa de estimativa.
2. **Medir o pack LiPo** — as três cotas estão marcadas `MEDIR` (ficha 06).
3. **Confirmar o código do motor N20** e a distância entre fileiras do
   ESP32 DevKit V1, que também está `MEDIR` (fichas 04 e 01).
4. **Resolver D9** no `wheel_hub.scad` antes de imprimir a roda.

Depois de cada um, regenerar os artefatos:

```bash
cd tools
python3 gen_fichas.py && python3 gen_component_drawings.py \
    && python3 gen_assembly_drawings.py && python3 verify_drawings.py
```
