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
| D2 | `hardware/chassis/sensor_bar.scad` | A barra modela 8 bolsões de 5,5 × 1,8 mm com passo 10 mm para TCRT5000 discretos; o QTR-8A é uma PCB única de 74,93 × 12,70 × 3,18 mm com passo 9,525 mm | `sensor_bar.scad` linhas 7-12 (`SENSOR_W 5.5`, `SENSOR_SPACING 10`); ficha 03 | Remodelar como berço para uma PCB única. `SENSOR_BAR_L = 90` também sobra sobre os 74,93 mm reais. **Resolução (13 out. 2026)**: modelo build123d em `hardware/cad/` (commit `44e3f4f`) — `parts/sensor_bar.py` prende a PCB única do QTR-8A por dois postes M2 nos furos de 69,85 mm; `.scad` removidos | **Resolvido** |
| D3 | `hardware/chassis/body_f1.scad`, `src/config.h` | Lookahead geométrico do chassi é de 7 mm, contra `LOOKAHEAD_MM 70` do firmware | `body_f1.scad`: `BODY_LENGTH 180`, motores em `BODY_LENGTH - MOTOR_OFFSET` = 170, slot da barra em `BODY_LENGTH - 3` = 177 → 177 − 170 = 7 mm | Prevalece o layout da prancha A1: sensores em X = +70 mm, caster em X = −55 mm. **Resolução (13 out. 2026)**: modelo build123d em `hardware/cad/` (commit `44e3f4f`); `.scad` removidos. Lookahead final de **85 mm** (com 70 mm a barra colide com o berço da bateria e as abraçadeiras); o `config.h` é atualizado no merge (D12) | **Resolvido** |
| D4 | `hardware/chassis/body_f1.scad`, `hardware/bom.csv` | Diâmetro do ball caster: `CASTER_D = 15` mm no SCAD contra ½ pol = 12,7 mm na BOM | `body_f1.scad` linha 37; `bom.csv` "Ball caster metálico … 1/2 pol (12.7 mm)" | Prevalece 12,7 mm, que é o produto efetivamente listado na Robocore. Corrigir o SCAD. **Resolução (13 out. 2026)**: ball caster removido (DEC-2); o apoio dianteiro é feito por patins PETG na barra de sensores (`hardware/cad/parts/skid.py`) | **Resolvido** |
| D5 | `src/config.h`, ficha 03 | `SENSOR_SPACING_MM 10` no firmware contra passo real de 9,525 mm do QTR-8A | Pololu 960: *sensor pitch* 0,375 in = 9,525 mm | Manter 10 no firmware: a diferença de 4,75 % é absorvida pelo Kp e pela calibração, e o valor só escala o erro de posição | **Aceito** |
| D6 | `hardware/bom.csv` | Três códigos/especificações incorretos de fornecedor | (a) SKU Pololu 3081 é o *Magnetic Encoder Pair Kit*, 12 CPR — o par de encoders, não o motor; (b) encoder descrito como "Hall 7 PPR", é magnético de 12 CPR no eixo do motor; (c) driver citado como SparkFun ROB-14450, o SKU correto é **ROB-14451** | Corrigir a `bom.csv`. O código do motor N20 continua **a confirmar**: a forma foi documentada pelo desenho dimensional da Pololu, mas a unidade adquirida é genérica | **Aberto** |
| D7 | `hardware/components/specs.py`, prancha A1 | Sete das dez massas são estimadas, não pesadas: ESP32 9,5 g · MCP3008 1,0 g · N20 10,5 g · TB6612 2,5 g · LiPo 48 g · MP1584 1,8 g · roda 6,0 g · caster 2,0 g · perfboard 12,0 g. Só o QTR-8A (3,09 g) é oficial. O chassi entra com 40 g estimados | `notas_montagem` de cada ficha marcam "MASSA ESTIMADA — PESAR" | O CG da prancha A1 (−20,10 mm) é **indicativo**. Pesar cada peça e regenerar A1 antes de fechar o chassi. Na v2, o relatório `hardware/cad/out/relatorio_montagem.md` recalcula o CG a cada `build.py`; pesar e preencher `fios_parafusos_massa` no `medidas.toml` (e as massas no `specs.py`) | **Aberto** |
| D8 | `src/config.h`, ficha 03 | `SENSOR_HEIGHT_MM 4` contra distância ótima de leitura de 3 mm do QTR-8A | Pololu 960: *optimal sensing distance* 0,125 in = 3 mm; máxima recomendada 6 mm | Aceito: 4 mm está dentro da faixa útil e dá folga contra a manta. Se a leitura ficar fraca na calibração, baixar para 3 mm pelo slot de ajuste do suporte | **Aceito** |
| D9 | `hardware/chassis/wheel_hub.scad` | Furo do eixo com 10 mm de profundidade num cubo de 8 mm de largura — não fecha | `wheel_hub.scad`: `WHEEL_WIDTH = 8`, `AXLE_DEPTH = 10` | Acrescentar ressalto de 2 mm na face interna do cubo: mantém os 10 mm úteis do eixo do N20 (ficha 04) totalmente encaixados sem alargar a roda. **Resolução (13 out. 2026)**: modelo build123d em `hardware/cad/` (commit `44e3f4f`) — `parts/wheel.py` tem cubo de 9,4 mm (eixo de 10 mm com 0,6 mm de folga para o colar) num aro de 8,2 mm; `.scad` removidos | **Resolvido** |
| D10 | `hardware/chassis/body_f1.scad`, prancha A1 | Bitola: `MOTOR_SPACING = 80` mm no SCAD contra 76 mm adotados na A1. `BODY_WIDTH = 120` mm também sobra sobre a largura total de 84 mm do conjunto rodas | `body_f1.scad` linhas 9 e 29 | Prevalece a A1 enquanto o chassi não for remodelado. A bitola final sai do par motor + roda medido. **Resolução (13 out. 2026)**: modelo build123d em `hardware/cad/` (commit `44e3f4f`); `.scad` removidos. Bitola de 95,8 mm entre os planos médios dos O-rings, largura total de 104 mm sobre as rodas (envelope 178 × 104 × 56 mm) | **Resolvido** |
| D11 | `hardware/components/specs.py` (n20), ficha 04 | `altura_caixa = 13 mm` contra a seção de 10 × 12 mm que a Pololu anuncia para o micro metal gearmotor | Pololu, *micro metal gearmotors*: seção da caixa 10 × 12 mm | O CAD usa 10 mm (`medidas.toml: n20_altura`). MEDIR a caixa de redução e corrigir a ficha 04 | **Aberto** |
| D12 | `src/config.h`, `hardware/cad/params.py` | Lookahead do firmware (`LOOKAHEAD_MM 70`) contra 85 mm do modelo | `hardware/cad/out/relatorio_montagem.md`; memória ruflo `lfr-2026/wp2/params` | O WP2 não edita o firmware: o coordenador atualiza o `config.h` para 85 no merge | **Resolvido no merge** |
| D13 | `hardware/drawings/assy/A1-layout-funcional.svg`, `A2-vista-explodida.svg` | Pranchas A1/A2 desenhadas para o layout com LiPo e ball caster | DEC-1 e DEC-2 (spec 2026-10-13) | Ficam como registro histórico; a referência é o modelo CAD em `hardware/cad/` | **Aceito** |

---

## O que fechar antes de imprimir

1. **Imprimir o cupom de tolerância** (`hardware/cad/out/cupom.stl` e
   `cupom_chave.stl`) e preencher a seção `[tolerancias]` do
   `hardware/cad/medidas.toml`.
2. **Medir** o suporte 2×18650, as células, o O-ring e a seção da caixa de
   redução do N20 (D11) e preencher a seção `[medidas]` do `medidas.toml`.
3. **Rodar o `build.py`** e conferir o relatório de montagem (carga nos patins
   entre 10 e 20 %, folga das rodas ≥ 0,5 mm):

```bash
cd hardware/cad
.venv/bin/python -m unittest discover -s tests && .venv/bin/python build.py
```

Fichas e pranchas de componente continuam sendo regeneradas a partir do
`specs.py`:

```bash
cd tools
python3 gen_fichas.py && python3 gen_component_drawings.py && python3 verify_drawings.py
```
