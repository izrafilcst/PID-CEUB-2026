# Ficha 03 — QTR-8A Reflectance Sensor Array

> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir de `hardware/components/specs.py`. Não edite este arquivo — altere o `specs.py` e regenere.


## 1 Identificação

| Campo | Valor |
|-------|-------|
| Denominação | QTR-8A Reflectance Sensor Array |
| Fabricante | Pololu |
| Código | 960 |
| Fornecedor | Pololu (importado) |
| Preço | US$ 12,95 |
| Massa | 3,09 g |
| Datasheet | https://www.pololu.com/product/960 |

## 2 Dimensões

| Medida | Valor | Proveniência | Fonte |
|--------|-------|--------------|-------|
| comprimento | 74,93 mm | ● OFICIAL | https://www.pololu.com/product/960 — Specifications, Size: 2.95" x 0.5" x 0.125" |
| largura | 12,70 mm | ● OFICIAL | https://www.pololu.com/product/960 — Specifications, Size |
| espessura | 3,18 mm | ● OFICIAL | https://www.pololu.com/product/960 — Specifications, Size (sem os pinos de header instalados) |
| passo sensores | 9,53 mm | ● OFICIAL | https://www.pololu.com/product/960 — Sensor pitch: 0.375" |
| distancia otima leitura | 3 mm | ● OFICIAL | https://www.pololu.com/product/960 — Optimal sensing distance: 0.125" (3 mm)<br>Máxima recomendada: 6 mm. O config.h usa SENSOR_HEIGHT_MM 4 — dentro do limite, mas acima do ótimo (ver docs/DIVERGENCIAS.md, D8). |

**Legenda de proveniência**: ● cotas do datasheet do fabricante · ○ módulo genérico — valor típico · △ varia por lote — MEDIR a peça


## 3 Características elétricas

| Grandeza | Valor |
|----------|-------|
| tensão de operação | 3,3 – 5,0 V |
| corrente de alimentação | 100 mA (8 emissores) |
| saída | analógica, 0 V a VCC, uma por canal |

## 4 Notas de montagem

- Alimentar em 3,3 V para casar com VDD/VREF do MCP3008. Em 5 V a saída excede a entrada máxima do ADC (VDD+0,3 V).
- Espessura de 3,18 mm é sem os pinos de header. Com header soldado, prever folga adicional no berço.
- Deixar LEDON solto — o pull-up interno mantém os emissores ligados.
- Altura ao solo de projeto: 4 mm (SENSOR_HEIGHT_MM em config.h). A Pololu especifica 3 mm como ótimo e 6 mm como máximo.
- O passo real de 9,525 mm difere do SENSOR_SPACING_MM 10 usado no firmware; a diferença é absorvida pelo Kp e pela calibração (ver docs/DIVERGENCIAS.md, D5).
- Os 8 sensores são simétricos em torno do centro da placa: 7 x 9,525 = 66,675 mm entre o primeiro e o oitavo, sobrando 4,13 mm de cada lado até a borda.

## 5 Prancha

`hardware/drawings/comp/03-qtr8a.svg`


## 6 Referência (NBR 6023)

POLOLU CORPORATION. QTR-8A Reflectance Sensor Array. Las Vegas, 2026. Disponível em: https://www.pololu.com/product/960. Acesso em: 12 ago. 2026.
