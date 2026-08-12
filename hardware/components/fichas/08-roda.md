# Ficha 08 — Roda ⌀34 mm com pneu O-ring de silicone

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | Roda ⌀34 mm com pneu O-ring de silicone |
| Fabricante | Genérico / cubo impresso em PETG |
| Código | — |
| Fornecedor | AliExpress / Mercado Livre + impressão 3D |
| Preço | R$ 20,00 (par) |
| Massa | 6 g |
| Datasheet | — (sem datasheet oficial) |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| diametro externo | 34 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — WHEEL_OD = 34; bom.csv 'Roda silicone 34mm'<br>Diâmetro sobre o O-ring montado. Define a altura do eixo motriz ao solo: 17 mm (prancha A1). |
| largura | 8 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — WHEEL_WIDTH = 8 |
| diametro furo eixo | 3 mm | ● OFICIAL | https://www.pololu.com/file/0J949/micro-metal-gearmotor-dimensions.pdf — vista lateral: ⌀3,0 mm [0.12 in]<br>Tem que casar com o eixo do N20 (ficha 04). |
| achatado furo eixo | 2,50 mm | ● OFICIAL | https://www.pololu.com/file/0J949/micro-metal-gearmotor-dimensions.pdf — vista lateral: 2,5 mm [0.10 in] entre faces<br>Rebaixo em D do furo, cópia do achatado do eixo. É o que impede a roda de patinar no eixo. |
| profundidade furo eixo | 10 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — AXLE_DEPTH = 10<br>Igual ao comprimento útil do eixo do N20 (10 mm). A roda encosta no colar de ⌀4,0 mm. |
| diametro canal oring | 30 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — ORING_GROOVE_D = 30 (fundo do canal) |
| largura canal oring | 3,50 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — ORING_GROOVE_W = 3.5 |
| secao oring | 3 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — ORING_GROOVE_D_ORING = 3<br>34 - 30 = 4 mm de diâmetro sobrando para um O-ring de 3 mm de seção: ele sobressai 0,5 mm de cada lado quando assentado. |
| diametro cubo | 14 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — HUB_D = 14 |
| diametro furo grub | 3 mm | ○ TIPICO | hardware/chassis/wheel_hub.scad (projeto próprio) — GRUB_D = 3.0, parafuso M3 sem cabeça<br>Trava anti-rotação, radial, contra o achatado do eixo. |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 4 Notas de montagem

- O furo do eixo é a interface crítica: ⌀3,0 mm com achatado de 2,5 mm entre faces, copiado da ficha 04. Imprimir com tolerância de 0,2 mm (TOL no wheel_hub.scad) e ajustar.
- Altura do eixo motriz ao solo = ⌀34/2 = 17 mm. O ball caster (ficha 09) tem que ser regulado para essa mesma altura.
- MASSA ESTIMADA (6,0 g por roda, cubo PETG + O-ring) — PESAR. São duas rodas no cálculo de CG (A1).
- Comprar O-rings de reserva: o silicone é o item de desgaste.

## 5 Prancha

`hardware/drawings/comp/08-roda.svg`
