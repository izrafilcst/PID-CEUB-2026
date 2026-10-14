# Relatório de montagem — LFR v2

Gerado por `hardware/cad/build.py`. Massas impressas estimadas (PETG 1.27 g/cm³, preenchimento 60%); massas compradas do `specs.py` (estimadas até a pesagem — D7).

## Verificações

| Verificação | Critério | Valor | Resultado |
|-------------|----------|-------|-----------|
| Interferência entre peças e envelopes | volume de interseção ≤ 0.001 mm³ em todo par | nenhuma | ✔ |
| Folga mínima das rodas | ≥ 0.5 mm | 0.60 mm | ✔ |
| Carga nos patins | 10%–20% do peso | 16.2% | ✔ |
| Não empina (CG à frente do eixo) | x_CG ≥ 0,3 × z_CG | x_CG = 13.8 mm, 0,3 × z_CG = 6.1 mm | ✔ |
| Vão livre | ≥ 4.0 mm (exceto rodas, patins e QTR-8A) | 5.00 mm (base) | ✔ |
| Face do sensor ao chão | 3.0 ± 0.1 mm | 3.00 mm | ✔ |
| Patins e O-rings tocam o chão | z mín = 0 ± 0.01 mm | patim E 0.000, patim D 0.000, O-ring E 0.000, O-ring D 0.000 | ✔ |
| Envelope montado | ≤ 250 × 250 × 200 mm | 177.8 × 104.0 × 55.6 mm | ✔ |

## Folgas por par (abaixo de 2 mm)

Distância mínima entre cada par de peças impressas, rodas e envelopes dos componentes. 0,000 = contato de apoio ou encosto previsto (a interferência está nas verificações).

| Peça A | Peça B | Folga (mm) |
|--------|--------|------------|
| QTR-8A | conector do QTR-8A | 0.000 |
| barra de sensores | QTR-8A | 0.000 |
| barra de sensores | patim D | 0.000 |
| barra de sensores | patim E | 0.000 |
| base | abraçadeira do porta-fusível | 0.000 |
| base | barra de sensores | 0.000 |
| base | cabeça M3 barra D | 0.000 |
| base | cabeça M3 barra E | 0.000 |
| base | motor D | 0.000 |
| base | motor E | 0.000 |
| base | perfboard | 0.000 |
| base | suporte 2×18650 | 0.000 |
| abraçadeira D | motor D | 0.100 |
| abraçadeira E | motor E | 0.100 |
| base | chicote do QTR-8A | 0.100 |
| suporte 2×18650 | chicote do QTR-8A | 0.200 |
| base | abraçadeira D | 0.300 |
| base | abraçadeira E | 0.300 |
| barra de sensores | chicote do QTR-8A | 0.500 |
| barra de sensores | conector do QTR-8A | 0.500 |
| base | roda D | 0.600 |
| base | roda E | 0.600 |
| roda D | motor D | 0.600 |
| roda E | motor E | 0.600 |
| abraçadeira D | roda D | 0.906 |
| abraçadeira E | roda E | 0.906 |
| barra de sensores | cabeça M3 barra D | 1.118 |
| barra de sensores | cabeça M3 barra E | 1.118 |
| abraçadeira D | suporte 2×18650 | 1.500 |
| abraçadeira E | suporte 2×18650 | 1.500 |
| motor D | chicote do QTR-8A | 1.500 |
| motor E | chicote do QTR-8A | 1.500 |

## Números do modelo

| Grandeza | Valor |
|----------|-------|
| Massa total | 225 g |
| CG (x, y, z) | (13.8, 0.1, 20.4) mm — origem no eixo motriz, no chão |
| Carga nos patins | 16.2% do peso (meta 10–20 %) |
| Lookahead (sensores → eixo) | 85 mm |
| Roda (diâmetro com O-ring) | 40.9 mm |
| O-ring | 34 × 3.0 mm, fundo do canal ⌀35.02 mm |
| Velocidade estimada | 1.27 m/s (N20 600 RPM @ 6 V, vref 7.4 V, carga 80%) |
| Aceleração a meia velocidade | 13.1 m/s² |
| Folga mínima das rodas | 0.60 mm |
| Envelope | 178 × 104 × 56 mm |

## Massas

| Item | g |
|------|---|
| base | 38.9 |
| abraçadeira E | 0.6 |
| abraçadeira D | 0.6 |
| barra de sensores | 11.9 |
| patim E | 0.3 |
| patim D | 0.3 |
| roda E | 3.2 |
| roda D | 3.2 |
| N20 esquerdo | 10.5 |
| N20 direito | 10.5 |
| 2 células 18650 | 92.0 |
| Suporte 2×18650 | 12.0 |
| QTR-8A | 3.1 |
| Eletrônica no perfboard | 26.8 |
| Fios e parafusos | 10.0 |
| O-rings | 1.0 |

## Peças para imprimir

| Arquivo | Quantidade |
|---------|------------|
| `base.stl` | 1 |
| `abracadeira_E.stl` | 1 |
| `abracadeira_D.stl` | 1 |
| `barra_sensores.stl` | 1 |
| `patim.stl` | 2 |
| `calco.stl` | 4 |
| `calco_lingua.stl` | 2 |
| `roda.stl` | 2 |
| `cupom.stl` | 1 |
| `cupom_chave.stl` | 1 |

## Fixadores

Comprimentos conferidos contra os furos do modelo (rosca útil ≥ 3 mm, ponta a ≥ 0.3 mm do fundo).

| Qtd. | Item | Onde |
|------|------|------|
| 2 | M3 cabeça chata ISO 10642 (sextavado interno), × 8 (0–4 calços); × 10 (5–6 calços) | base → insertos da língua da barra, por baixo, no escareado de 90° |
| 6 | inserto térmico M3 (OD 4,0 × 5,7 mm) | 4 colunas do perfboard + 2 na língua da barra |
| 4 | M3 × 6 cabeça cilíndrica ou panela | perfboard → insertos das colunas |
| 4 | M2 × 8 autoatarraxante | abraçadeiras do N20 → berço da base |
| 2 | M2 autoatarraxante, × 12 (0–2 calços); × 14 (3–6 calços) | patins, de cima para baixo pela barra (com os calços) |
| 2 | M2 × 8 autoatarraxante cabeça baixa | QTR-8A → postes da barra (a cabeça fica sob a placa do QTR) |
| 2 | M2 × 4 autoatarraxante (opcional) | trava radial das rodas |
| 1 | abraçadeira de nylon 2,5 mm (≤ 1,2 mm de espessura) | porta-fusível, pelos rasgos e pelo rebaixo sob a base |

## Ligação do QTR-8A

- Barra de pinos 8×2 ou 11×1 de 2,54 mm soldada **por cima** do QTR, que monta com a borda dos pinos para trás. O conector fêmea sai pela janela da viga da barra (envelope de 16.5 mm acima da placa, verificado contra interferência).
- Chicote de 11 fios AWG 30 (⌀ ≤ 0,6 mm, uma camada): volta por cima da língua, desce pelo rasgo na ponta de trás da língua (e do calço da língua), corre no canal de 7 × 1.8 mm no topo da base, por baixo do suporte 2×18650 e das paredes do berço, e sobe entre os conectores dos N20 até o perfboard.
