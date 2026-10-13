# Ficha 08 — Roda impressa em PETG com pneu O-ring de silicone

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | Roda impressa em PETG com pneu O-ring de silicone |
| Fabricante | Projeto próprio |
| Código | hardware/cad/parts/wheel.py |
| Fornecedor | Impressão 3D + O-ring (ficha 13) |
| Preço | ≈ R$ 2 (PETG) + O-rings |
| Massa | 5 g |
| Datasheet | — (sem datasheet oficial) |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| diametro externo | 40,90 mm | ○ TIPICO | hardware/cad/parts/wheel.py — O-ring 34 × 3 com 3 % de estiramento<br>Diâmetro sobre o O-ring montado. Define a altura do eixo motriz ao solo (≈ 20,5 mm). Faixa de projeto: 38–44 mm (spec 2026-10-13, §5.4). |
| largura | 8,20 mm | ○ TIPICO | hardware/cad/params.py — Params.roda_aro_w (largura do aro) |
| diametro furo eixo | 3,05 mm | ○ TIPICO | hardware/cad/params.py — Tolerancias.eixo_d<br>Eixo do N20 de ⌀3,0 mm (ficha 04) com folga de PETG. Ajustar pelo cupom de tolerância (medidas.toml). |
| achatado furo eixo | 2,55 mm | ○ TIPICO | hardware/cad/params.py — Tolerancias.eixo_achatado<br>Rebaixo em D do furo, cópia do achatado de 2,5 mm do eixo. É o que impede a roda de patinar no eixo. |
| profundidade furo eixo | 9,40 mm | ○ TIPICO | hardware/cad/params.py — Params.roda_cubo_l<br>Cubo de 9,4 mm: engata o eixo de 10 mm com 0,6 mm de folga para o colar (resolve D9). |
| diametro canal oring | 35 mm | ○ TIPICO | hardware/cad/parts/wheel.py — fundo do canal = ID do O-ring × 1,03 |
| largura canal oring | 3,75 mm | ○ TIPICO | hardware/cad/parts/wheel.py — 1,25 × seção do O-ring |
| secao oring | 3 mm | ○ TIPICO | hardware/cad/parts/wheel.py — O-ring 34 × 3 (ficha 13) |
| diametro cubo | 10 mm | ○ TIPICO | hardware/cad/params.py — Params.roda_cubo_d |
| diametro furo grub | 1,70 mm | ○ TIPICO | hardware/cad/params.py — Tolerancias.autoatarraxante_m2<br>Furo radial para M2 autoatarraxante, trava opcional contra o achatado do eixo. |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 4 Notas de montagem

- Imprimir com a face externa na mesa (o canal do O-ring tem o teto a 55° e imprime sem suporte).
- Antes de imprimir as rodas, testar o encaixe do eixo nos furos em D do cupom de tolerância e preencher eixo_d / eixo_achatado no hardware/cad/medidas.toml.
- O O-ring é item de desgaste: trocar quando a superfície ficar lisa (ficha 13).
- MASSA ESTIMADA (5,0 g por roda, cubo PETG + O-ring) — PESAR.

## 5 Prancha

`hardware/drawings/comp/08-roda.svg`
