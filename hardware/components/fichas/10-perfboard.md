# Ficha 10 — Perfboard 50 × 70 mm, passo 2,54 mm

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | Perfboard 50 × 70 mm, passo 2,54 mm |
| Fabricante | Genérico |
| Código | — |
| Fornecedor | Mercado Livre / AliExpress |
| Preço | R$ 6,00 |
| Massa | 12 g |
| Datasheet | — (sem datasheet oficial) |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| comprimento | 70 mm | ○ TIPICO | bom.csv — 'Perfboard 5×7 cm', formato padrão de mercado<br>Placa cortável. O contorno final sai do alojamento do chassi, não do fornecedor. |
| largura | 50 mm | ○ TIPICO | bom.csv — 'Perfboard 5×7 cm', formato padrão de mercado |
| espessura | 1,60 mm | ○ TIPICO | FR4 de 1,6 mm, espessura padrão de mercado |
| passo furos | 2,54 mm | ○ TIPICO | padrão consolidado de 0,1 in — casa com MCP3008, ESP32 DevKit V1 e TB6612FNG |
| diametro furo | 1 mm | ○ TIPICO | padrão de mercado para perfboard de 2,54 mm |
| margem ate primeiro furo | 1,50 mm | △ MEDIR | varia por fabricante<br>**MEDIR. Define quantas colunas sobram depois do corte.** |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 4 Notas de montagem

- Grade útil: 70 / 2,54 ≈ 27 colunas por 50 / 2,54 ≈ 19 linhas, descontadas as margens. CONFERIR a contagem na placa em mãos.
- O MCP3008 em PDIP-16 ocupa 8 furos por fileira, com as duas fileiras separadas por 3 colunas (7,62 mm).
- Recortar conforme o alojamento do chassi — a placa inteira de 50 × 70 mm não é obrigatória.
- MASSA ESTIMADA (12,0 g com a placa inteira) — PESAR depois do corte, já com os componentes soldados.

## 5 Prancha

`hardware/drawings/comp/10-perfboard.svg`
