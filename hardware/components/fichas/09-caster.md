# Ficha 09 — Ball caster com esfera de ½ pol

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | Ball caster com esfera de ½ pol |
| Fabricante | Genérico (Robocore) |
| Código | ROB-09259 (a confirmar) |
| Fornecedor | Robocore / Mercado Livre |
| Preço | R$ 8,00 |
| Massa | 2 g |
| Datasheet | https://www.robocore.net/robotica/bola-de-rodinha-metalica-1-2-polegada |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| diametro esfera | 12,70 mm | ● OFICIAL | https://www.robocore.net/robotica/bola-de-rodinha-metalica-1-2-polegada — 'bola de rodinha metálica 1/2 polegada'; ½ in = 12,70 mm por definição |
| altura base | 13,46 mm | ○ TIPICO | https://www.pololu.com/product/952 — ball caster de ½ pol: base 0,53 in sem espaçadores<br>Cota do conjunto Pololu equivalente. O corpo Robocore pode diferir — MEDIR. |
| altura maxima | 20,32 mm | ○ TIPICO | https://www.pololu.com/product/952 — faixa montada 0,5 a 0,8 in conforme os espaçadores<br>A altura de projeto é 17 mm (⌀roda/2), que cai dentro da faixa de regulagem. |
| entre furos | 14,73 mm | ○ TIPICO | https://www.pololu.com/product/952 — 0,58 in entre os furos de fixação<br>MEDIR antes de furar o chassi. |
| diametro furo fixacao | 2,20 mm | ○ TIPICO | https://www.pololu.com/product/952 — parafusos #2-56 acompanham o kit<br>Passagem para M2. A bom.csv prevê espaçadores M3 — conferir (ver docs/DIVERGENCIAS.md, D4). |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 4 Notas de montagem

- ALTURA É A COTA CRÍTICA: tem que casar com ⌀roda/2 = 17 mm para o chassi ficar nivelado e a barra de sensores manter os 4 mm ao solo. Regular com os espaçadores.
- CONFLITO DE DIÂMETRO: body_f1.scad usa CASTER_D = 15 mm e a bom.csv especifica ½ pol = 12,7 mm. Prevalece 12,7 mm, que é o produto efetivamente listado (ver docs/DIVERGENCIAS.md, D4).
- CÓDIGO A CONFIRMAR: a bom.csv cita ROB-09259, SKU da SparkFun, com link para a Robocore. Conferir o que chega.
- Montar atrás do eixo motriz. A posição longitudinal sai do balanço de CG da prancha A1.
- MASSA ESTIMADA (2,0 g) — PESAR.

## 5 Prancha

`hardware/drawings/comp/09-caster.svg`


## 6 Referência (NBR 6023)

POLOLU CORPORATION. Pololu ball caster with 1/2" ball. Las Vegas, 2026. Disponível em: https://www.pololu.com/product/952. Acesso em: 12 ago. 2026.
