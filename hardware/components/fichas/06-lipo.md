# Ficha 06 — Bateria LiPo 2S 800 mAh 30C

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | Bateria LiPo 2S 800 mAh 30C |
| Fabricante | Genérico |
| Código | — |
| Fornecedor | Mercado Livre / Hobbyking |
| Preço | R$ 55,00 |
| Massa | 48 g |
| Datasheet | — (sem datasheet oficial) |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| comprimento | 58 mm | △ MEDIR | valor de projeto do bom.csv<br>**MEDIR o pack adquirido. A designação '2S 800 mAh 30C' não define geometria; packs comerciais variam de aproximadamente 50 a 65 mm. Limite do projeto: 65 mm.** |
| largura | 32 mm | △ MEDIR | valor de projeto do bom.csv<br>**MEDIR. Limite do projeto: 35 mm.** |
| altura | 12 mm | △ MEDIR | valor de projeto do bom.csv<br>**MEDIR. Limite do projeto: 12 mm.** |
| comprimento chicote | 100 mm | △ MEDIR | valor de projeto<br>**MEDIR. Chicote de força mais o conector de balanceamento; define o raio de manobra do pack dentro do chassi.** |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 3 Características elétricas

| Grandeza | Valor |
|----------|-------|
| tensão nominal | 7,4 V (2 células) |
| tensão plena | 8,4 V |
| tensão mínima segura | 6,0 V (3,0 V/célula) |
| capacidade | 800 mAh |
| descarga contínua | 30C = 24 A |

## 4 Notas de montagem

- MEDIR o pack antes de modelar o alojamento. Todas as cotas aqui são de projeto, não do fornecedor.
- MASSA ESTIMADA (48 g) — é a massa nominal de um pack 2S 800 mAh equivalente, NÃO a do pack adquirido. PESAR. É a maior massa isolada do robô e domina o CG da prancha A1 (ver docs/DIVERGENCIAS.md, D7).
- Posicionar atrás do eixo motriz para manter o CG dentro do triângulo de apoio (ver prancha A1).
- Prever acesso ao conector de balanceamento sem desmontar o chassi.
- Não descarregar abaixo de 3,0 V/célula. O divisor 100k+47k no GPIO 34 monitora VBAT (ver docs/WIRING.md).

## 5 Prancha

`hardware/drawings/comp/06-lipo.svg`
