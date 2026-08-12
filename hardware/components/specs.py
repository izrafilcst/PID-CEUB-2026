"""Fonte única de dados dimensionais dos componentes comprados.

Toda medida carrega nível de proveniência e, quando OFICIAL, a URL e a
figura/página de onde saiu. Nenhum valor aqui pode ser estimado em silêncio.

Sistema de referência das medidas de cada componente está descrito no campo
`notas_montagem` da respectiva entrada.
"""
from __future__ import annotations

import os
import sys
from dataclasses import dataclass, field

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "tools"))

from techdraw import Prov  # noqa: E402


@dataclass(frozen=True)
class Medida:
    """Uma cota, com o nível de confiança e a origem do valor."""

    valor: float          # mm
    prov: Prov
    fonte: str = ""       # URL + figura/página — obrigatório se OFICIAL
    nota: str = ""


@dataclass(frozen=True)
class Componente:
    """Ficha de um componente comprado."""

    num: str              # "01" .. "10"
    nome: str
    fabricante: str
    codigo: str
    fornecedor: str
    preco: str
    url_datasheet: str
    ref_abnt: str         # referência NBR 6023, reaproveitada na monografia
    massa_g: float
    medidas: dict[str, Medida]
    eletrico: dict[str, str] = field(default_factory=dict)
    notas_montagem: list[str] = field(default_factory=list)


# ═══════════════════════════════════════════════════════════════════════════
# URLs das fontes consultadas em 12 ago. 2026
# ═══════════════════════════════════════════════════════════════════════════
U_WROOM = ("https://documentation.espressif.com/"
           "esp32-wroom-32_datasheet_en.pdf")
U_DEVKIT = ("https://mischianti.org/"
            "doit-esp32-dev-kit-v1-high-resolution-pinout-and-specs/")
U_MCP = "https://ww1.microchip.com/downloads/en/DeviceDoc/21295d.pdf"
U_QTR = "https://www.pololu.com/product/960"
U_N20 = "https://www.pololu.com/file/0J949/micro-metal-gearmotor-dimensions.pdf"
U_TB = "https://www.sparkfun.com/sparkfun-motor-driver-dual-tb6612fng-1a.html"
U_MP = "https://www.makerfabs.com/adjustable-mini-dc-dc-module-mp1584en.html"
U_MPS = "https://www.monolithicpower.com/en/mp1584.html"
U_CASTER = ("https://www.robocore.net/robotica/"
            "bola-de-rodinha-metalica-1-2-polegada")
U_CASTER_REF = "https://www.pololu.com/product/952"
SCAD_RODA = "hardware/chassis/wheel_hub.scad (projeto próprio)"


COMPONENTES: dict[str, Componente] = {
    # ═══════════════════════════════════════════════════════════════════
    "esp32": Componente(
        num="01",
        nome="ESP32 DevKit V1 (30 pinos)",
        fabricante="DOIT (clone) / módulo Espressif",
        codigo="ESP32-DEVKITV1",
        fornecedor="Mercado Livre / AliExpress",
        preco="R$ 45,00",
        url_datasheet=U_WROOM,
        ref_abnt=(
            "ESPRESSIF SYSTEMS. ESP32-WROOM-32 datasheet: version 3.7. "
            "Xangai, 2023. Disponível em: "
            f"{U_WROOM}. Acesso em: 12 ago. 2026."
        ),
        massa_g=9.5,
        medidas={
            "comprimento": Medida(
                51.8, Prov.TIPICO,
                f"{U_DEVKIT} — resposta do autor: 28,2 mm x 51,8 mm",
                nota="Placa clone: o contorno varia entre fabricantes. "
                     "MEDIR antes de fechar o alojamento no chassi."),
            "largura": Medida(
                28.2, Prov.TIPICO,
                f"{U_DEVKIT} — resposta do autor: 28,2 mm x 51,8 mm"),
            "espessura_pcb": Medida(
                1.6, Prov.TIPICO,
                f"{U_DEVKIT} — FR4 de 1,6 mm, espessura padrão de mercado"),
            "passo_pinos": Medida(
                2.54, Prov.TIPICO,
                f"{U_DEVKIT} — barra de pinos padrão de 0,1 in"),
            "distancia_fileiras": Medida(
                25.4, Prov.MEDIR,
                f"{U_DEVKIT} — 15 pinos por fileira",
                nota="MEDIR. Clones de DevKit V1 aparecem com 22,86 mm "
                     "(0,9 in) e 25,4 mm (1,0 in) entre fileiras. Furar o "
                     "perfboard só depois de conferir na placa em mãos."),
            "modulo_comprimento": Medida(
                25.50, Prov.OFICIAL,
                f"{U_WROOM} — Figura 7 (Physical Dimensions), p. 36: "
                "25,50 ± 0,15 mm"),
            "modulo_largura": Medida(
                18.00, Prov.OFICIAL,
                f"{U_WROOM} — Figura 7 (Physical Dimensions), p. 36: "
                "18,00 ± 0,15 mm"),
            "modulo_altura": Medida(
                3.10, Prov.OFICIAL,
                f"{U_WROOM} — Figura 7 (Physical Dimensions), p. 36: "
                "3,10 ± 0,15 mm (blindagem sobre a PCB do módulo)"),
        },
        eletrico={
            "tensão de alimentação (VIN)": "5 V (regulador AMS1117 embarcado)",
            "tensão lógica": "3,3 V",
            "corrente de pico (WiFi TX)": "≈ 500 mA",
            "ADC interno": "12 bits, 0 – 3,3 V (não usado: ver MCP3008)",
        },
        notas_montagem=[
            "Altura total com barra de pinos soldada: 1,6 mm de PCB + 3,1 mm "
            "de módulo + a barra. Prever no mínimo 12 mm de espaço livre.",
            "O conector micro-USB fica na extremidade oposta ao módulo; "
            "deixar folga para o cabo sem desmontar o chassi.",
            "MASSA ESTIMADA (9,5 g) — PESAR a placa. Entra no cálculo de CG "
            "da prancha A1.",
            "Core 0 roda sensor/PID/motor; core 1 roda BLE e Logger "
            "(ver CLAUDE.md).",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "mcp3008": Componente(
        num="02",
        nome="MCP3008 ADC SPI 8 canais 10 bits",
        fabricante="Microchip Technology",
        codigo="MCP3008-I/P",
        fornecedor="Robocore / Mercado Livre",
        preco="R$ 25,00",
        url_datasheet=U_MCP,
        ref_abnt=(
            "MICROCHIP TECHNOLOGY INC. MCP3004/3008: 2.7V 4-channel/"
            "8-channel 10-bit A/D converters with SPI serial interface. "
            "Chandler, 2008. DS21295D. Disponível em: "
            f"{U_MCP}. Acesso em: 12 ago. 2026."
        ),
        massa_g=1.0,
        medidas={
            "comprimento": Medida(
                19.18, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: D (overall length) "
                "NOM 0,755 in = 19,177 mm"),
            "largura_corpo": Medida(
                6.35, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: E1 (molded package "
                "width) NOM 0,250 in = 6,35 mm"),
            "largura_ombro_a_ombro": Medida(
                7.87, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: E (shoulder to "
                "shoulder width) NOM 0,310 in = 7,874 mm"),
            "passo_fileiras": Medida(
                7.62, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: encapsulamento "
                "designado '300 mil Body [PDIP]' = 0,300 in entre fileiras",
                nota="7,62 mm é o passo das fileiras no furo da PCB. "
                     "A cota E do datasheet (7,87 mm) mede ombro a ombro, "
                     "com os terminais abertos antes da inserção."),
            "passo_pinos": Medida(
                2.54, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: e (pitch) 0,100 in BSC"),
            "altura_corpo": Medida(
                3.30, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: A2 (molded package "
                "thickness) NOM 0,130 in = 3,302 mm"),
            "altura_total": Medida(
                5.33, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: A (top to seating "
                "plane) MAX 0,210 in = 5,334 mm"),
            "comprimento_pino": Medida(
                3.30, Prov.OFICIAL,
                f"{U_MCP} — desenho C04-017B, p. 31: L (tip to seating "
                "plane) NOM 0,130 in = 3,302 mm"),
        },
        eletrico={
            "tensão de operação": "2,7 – 5,5 V",
            "resolução": "10 bits (0 – 1023)",
            "canais": "8 single-ended ou 4 diferenciais",
            "taxa de amostragem": "200 ksps @ 5 V · 75 ksps @ 2,7 V",
            "interface": "SPI modo 0,0 ou 1,1",
        },
        notas_montagem=[
            "Encapsulamento PDIP-16: as fileiras entram em furos de 0,300 in "
            "(7,62 mm) do perfboard — 3 colunas de folga entre elas.",
            "Marcação do pino 1: chanfro semicircular na extremidade. "
            "O índice visual fica dentro da área hachurada da NOTE 1 do "
            "desenho e pode variar de forma.",
            "Alimentar VDD e VREF em 3,3 V para casar com o QTR-8A e "
            "dispensar divisor no MISO.",
            "MASSA ESTIMADA (1,0 g) — PESAR. Entra no cálculo de CG (A1).",
            "Capacitor de 100 nF entre VDD e AGND, junto ao CI.",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "qtr8a": Componente(
        num="03",
        nome="QTR-8A Reflectance Sensor Array",
        fabricante="Pololu",
        codigo="960",
        fornecedor="Pololu (importado)",
        preco="US$ 12,95",
        url_datasheet=U_QTR,
        ref_abnt=(
            "POLOLU CORPORATION. QTR-8A Reflectance Sensor Array. "
            "Las Vegas, 2026. Disponível em: "
            f"{U_QTR}. Acesso em: 12 ago. 2026."
        ),
        massa_g=3.09,
        medidas={
            "comprimento": Medida(
                74.93, Prov.OFICIAL,
                f"{U_QTR} — Specifications, "
                'Size: 2.95" x 0.5" x 0.125"'),
            "largura": Medida(
                12.70, Prov.OFICIAL,
                f"{U_QTR} — Specifications, Size"),
            "espessura": Medida(
                3.18, Prov.OFICIAL,
                f"{U_QTR} — Specifications, Size "
                "(sem os pinos de header instalados)"),
            "passo_sensores": Medida(
                9.525, Prov.OFICIAL,
                f'{U_QTR} — Sensor pitch: 0.375"'),
            "distancia_otima_leitura": Medida(
                3.0, Prov.OFICIAL,
                f'{U_QTR} — Optimal sensing distance: 0.125" (3 mm)',
                nota="Máxima recomendada: 6 mm. O config.h usa "
                     "SENSOR_HEIGHT_MM 4 — dentro do limite, mas acima do "
                     "ótimo (ver docs/DIVERGENCIAS.md, D8)."),
        },
        eletrico={
            "tensão de operação": "3,3 – 5,0 V",
            "corrente de alimentação": "100 mA (8 emissores)",
            "saída": "analógica, 0 V a VCC, uma por canal",
        },
        notas_montagem=[
            "Alimentar em 3,3 V para casar com VDD/VREF do MCP3008. "
            "Em 5 V a saída excede a entrada máxima do ADC (VDD+0,3 V).",
            "Espessura de 3,18 mm é sem os pinos de header. Com header "
            "soldado, prever folga adicional no berço.",
            "Deixar LEDON solto — o pull-up interno mantém os emissores ligados.",
            "Altura ao solo de projeto: 4 mm (SENSOR_HEIGHT_MM em config.h). "
            "A Pololu especifica 3 mm como ótimo e 6 mm como máximo.",
            "O passo real de 9,525 mm difere do SENSOR_SPACING_MM 10 usado no "
            "firmware; a diferença é absorvida pelo Kp e pela calibração "
            "(ver docs/DIVERGENCIAS.md, D5).",
            "Os 8 sensores são simétricos em torno do centro da placa: "
            "7 x 9,525 = 66,675 mm entre o primeiro e o oitavo, sobrando "
            "4,13 mm de cada lado até a borda.",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "n20": Componente(
        num="04",
        nome="Micro metal gearmotor N20 com encoder 12 CPR",
        fabricante="Pololu (forma) / genérico (unidade adquirida)",
        codigo="a confirmar — ver notas",
        fornecedor="Pololu / AliExpress",
        preco="R$ 80,00 (par)",
        url_datasheet=U_N20,
        ref_abnt=(
            "POLOLU CORPORATION. Micro metal gearmotors with 12 CPR "
            "encoder, back connector: dimension diagram. Las Vegas, 2024. "
            f"Disponível em: {U_N20}. Acesso em: 12 ago. 2026."
        ),
        massa_g=10.5,
        medidas={
            "largura_caixa": Medida(
                12.0, Prov.OFICIAL,
                f"{U_N20} — vista frontal: 12,0 mm [0.47 in]"),
            "altura_caixa": Medida(
                13.0, Prov.OFICIAL,
                f"{U_N20} — vista frontal: 13 mm [0.51 in]"),
            "largura_total_abas": Medida(
                13.6, Prov.OFICIAL,
                f"{U_N20} — vista frontal: 13,6 mm [0.54 in], "
                "incluindo as abas de fixação"),
            "entre_furos_vertical": Medida(
                10.0, Prov.OFICIAL,
                f"{U_N20} — vista frontal: 10,0 mm [0.39 in] entre os "
                "dois furos M1.6"),
            "offset_furos_horizontal": Medida(
                4.5, Prov.OFICIAL,
                f"{U_N20} — vista frontal: 4,5 mm [0.18 in] de cada lado "
                "do eixo até o furo de fixação"),
            "diametro_eixo": Medida(
                3.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: ⌀3,0 mm [0.12 in]"),
            "comprimento_eixo": Medida(
                10.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: 10 mm [0.4 in] a partir da "
                "face da caixa de redução"),
            "diametro_colar": Medida(
                4.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: ⌀4,0 mm [0.16 in], colar na "
                "saída da caixa de redução"),
            "achatado_eixo": Medida(
                2.5, Prov.OFICIAL,
                f"{U_N20} — vista lateral: 2,5 mm [0.10 in]",
                nota="Cota entre faces do rebaixo em D: o eixo de ⌀3,0 mm "
                     "fica com 2,5 mm de altura no trecho achatado. É a "
                     "cota que o cubo da roda (ficha 08) tem que copiar."),
            "comprimento_caixa_reducao": Medida(
                9.0, Prov.OFICIAL,
                f"{U_N20} — tabela: L = 9 mm [0.35 in] para todas as "
                "relações exceto 1000:1 (12,5 mm)"),
            "comprimento_motor": Medida(
                15.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: 15 mm [0.6 in], corpo do motor"),
            "comprimento_encoder": Medida(
                8.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: 8 mm [0.3 in], seção do "
                "encoder com conector traseiro"),
            "comprimento_total": Medida(
                32.5, Prov.OFICIAL,
                f"{U_N20} — tabela: OL max = 32,5 mm [1.28 in] para todas "
                "as relações exceto 1000:1 (36 mm)",
                nota="Medido da face da caixa de redução até o fundo do "
                     "encoder. Somando os 10 mm de eixo, o envelope total "
                     "é 42,5 mm."),
            "largura_pcb_encoder": Medida(
                12.5, Prov.OFICIAL,
                f"{U_N20} — vista superior: 12,5 mm [0.49 in]"),
            "largura_conector": Medida(
                9.6, Prov.OFICIAL,
                f"{U_N20} — vista traseira: 9,6 mm [0.38 in], conector "
                "JST SH de 6 vias"),
        },
        eletrico={
            "tensão nominal": "6 V",
            "encoder": "12 CPR por volta do eixo do MOTOR (quadratura, "
                       "contando as duas bordas dos dois canais)",
            "tensão do encoder": "2,7 – 18 V",
            "conector": "JST SH 6 vias, passo 1,0 mm "
                        "(compatível com SHR-06V-S)",
        },
        notas_montagem=[
            "CÓDIGO A CONFIRMAR. A bom.csv cita o SKU Pololu 3081, que na "
            "verdade é o 'Magnetic Encoder Pair Kit for Micro Metal "
            "Gearmotors, 12 CPR' — o par de encoders, não o motor. "
            "Ver docs/DIVERGENCIAS.md, D6.",
            "A bom.csv também cita 'encoder Hall 7 PPR'; o encoder "
            "magnético da Pololu é de 12 CPR no eixo do motor. Multiplicar "
            "pela relação de redução para obter CPR no eixo de saída.",
            "Fixação: 2 x M1.6 nas abas, 10,0 mm entre centros na vertical "
            "e 4,5 mm de cada lado do eixo. A bom.csv prevê parafusos M2 — "
            "conferir antes de comprar (ver D6).",
            "O achatado em D do eixo tem 2,5 mm de comprimento; o cubo da "
            "roda (ficha 08) precisa casar com ele.",
            "MASSA ESTIMADA (10,5 g por motor com encoder) — PESAR. "
            "São dois motores no cálculo de CG da prancha A1.",
            "As cotas vêm do desenho Pololu do micro metal gearmotor, que "
            "define a forma N20. A unidade adquirida é genérica — CONFERIR "
            "o comprimento total, que varia com a relação de redução.",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "tb6612": Componente(
        num="05",
        nome="Driver de motor dual TB6612FNG",
        fabricante="SparkFun Electronics (placa) / Toshiba (CI)",
        codigo="ROB-14451",
        fornecedor="SparkFun / Mercado Livre",
        preco="R$ 18,00",
        url_datasheet=U_TB,
        ref_abnt=(
            "SPARKFUN ELECTRONICS. SparkFun motor driver: dual TB6612FNG "
            "(1A). Niwot, 2026. Disponível em: "
            f"{U_TB}. Acesso em: 12 ago. 2026."
        ),
        massa_g=2.5,
        medidas={
            "comprimento": Medida(
                20.32, Prov.OFICIAL,
                f'{U_TB} — Dimensions: 0.8" x 0.8" = 20,32 x 20,32 mm',
                nota="Clones nacionais da mesma função usam outro contorno. "
                     "MEDIR se a compra não for a placa SparkFun."),
            "largura": Medida(
                20.32, Prov.OFICIAL,
                f'{U_TB} — Dimensions: 0.8" x 0.8"'),
            "espessura_pcb": Medida(
                1.6, Prov.TIPICO,
                f"{U_TB} — FR4 de 1,6 mm, espessura padrão de mercado"),
            "passo_pinos": Medida(
                2.54, Prov.OFICIAL,
                f'{U_TB} — "All pins of the TB6612FNG broken out to 0.1" '
                'spaced pins"'),
            "diametro_furo_fixacao": Medida(
                3.2, Prov.MEDIR,
                f"{U_TB} — a página do produto não cota os furos",
                nota="MEDIR. Presume-se passagem para M3, mas a placa "
                     "SparkFun não publica a cota."),
        },
        eletrico={
            "tensão dos motores (VM)": "máx. 15 V",
            "tensão lógica (VCC)": "2,7 – 5,5 V",
            "corrente contínua": "1,2 A por canal",
            "corrente de pico": "3,2 A (pulso único e curto)",
            "frequência de PWM": "até 100 kHz",
            "proteções": "desligamento térmico e detecção de subtensão",
        },
        notas_montagem=[
            "STBY precisa de nível ALTO para o driver operar. Com STBY em "
            "nível baixo as duas pontes ficam em alta impedância.",
            "A bom.csv cita o SKU ROB-14450; o SKU correto da placa "
            "SparkFun é ROB-14451 (ver docs/DIVERGENCIAS.md, D6).",
            "PWM de 20 kHz do projeto (config.h) está bem abaixo do limite "
            "de 100 kHz do CI.",
            "Capacitor de 100 µF na entrada VM, junto à placa, além do "
            "100 nF de desacoplamento.",
            "MASSA ESTIMADA (2,5 g) — PESAR. Entra no cálculo de CG (A1).",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "lipo": Componente(
        num="06",
        nome="Bateria LiPo 2S 800 mAh 30C",
        fabricante="Genérico",
        codigo="—",
        fornecedor="Mercado Livre / Hobbyking",
        preco="R$ 55,00",
        url_datasheet="",
        ref_abnt="",
        massa_g=48.0,
        medidas={
            "comprimento": Medida(
                58.0, Prov.MEDIR,
                "valor de projeto do bom.csv",
                nota="MEDIR o pack adquirido. A designação '2S 800 mAh 30C' "
                     "não define geometria; packs comerciais variam de "
                     "aproximadamente 50 a 65 mm. Limite do projeto: 65 mm."),
            "largura": Medida(
                32.0, Prov.MEDIR, "valor de projeto do bom.csv",
                nota="MEDIR. Limite do projeto: 35 mm."),
            "altura": Medida(
                12.0, Prov.MEDIR, "valor de projeto do bom.csv",
                nota="MEDIR. Limite do projeto: 12 mm."),
            "comprimento_chicote": Medida(
                100.0, Prov.MEDIR, "valor de projeto",
                nota="MEDIR. Chicote de força mais o conector de "
                     "balanceamento; define o raio de manobra do pack "
                     "dentro do chassi."),
        },
        eletrico={
            "tensão nominal": "7,4 V (2 células)",
            "tensão plena": "8,4 V",
            "tensão mínima segura": "6,0 V (3,0 V/célula)",
            "capacidade": "800 mAh",
            "descarga contínua": "30C = 24 A",
        },
        notas_montagem=[
            "MEDIR o pack antes de modelar o alojamento. Todas as cotas aqui "
            "são de projeto, não do fornecedor.",
            "MASSA ESTIMADA (48 g) — é a massa nominal de um pack 2S "
            "800 mAh equivalente, NÃO a do pack adquirido. PESAR. É a maior "
            "massa isolada do robô e domina o CG da prancha A1 "
            "(ver docs/DIVERGENCIAS.md, D7).",
            "Posicionar atrás do eixo motriz para manter o CG dentro do "
            "triângulo de apoio (ver prancha A1).",
            "Prever acesso ao conector de balanceamento sem desmontar o chassi.",
            "Não descarregar abaixo de 3,0 V/célula. O divisor 100k+47k no "
            "GPIO 34 monitora VBAT (ver docs/WIRING.md).",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "mp1584": Componente(
        num="07",
        nome="Regulador buck MP1584EN (módulo mini)",
        fabricante="Monolithic Power Systems (CI) / genérico (módulo)",
        codigo="MP1584EN",
        fornecedor="Mercado Livre / AliExpress",
        preco="R$ 8,00",
        url_datasheet=U_MPS,
        ref_abnt=(
            "MONOLITHIC POWER SYSTEMS. MP1584: 3A, 1.5MHz, 28V step-down "
            "converter. Kirkland, 2026. Disponível em: "
            f"{U_MPS}. Acesso em: 12 ago. 2026."
        ),
        massa_g=1.8,
        medidas={
            "comprimento": Medida(
                22.0, Prov.TIPICO,
                f"{U_MP} — módulo mini genérico, 22 x 17 x 4 mm",
                nota="Módulo sem fabricante único. MEDIR o exemplar "
                     "adquirido antes de fechar o alojamento."),
            "largura": Medida(
                17.0, Prov.TIPICO,
                f"{U_MP} — módulo mini genérico, 22 x 17 x 4 mm"),
            "altura_com_indutor": Medida(
                4.0, Prov.TIPICO,
                f"{U_MP} — módulo mini genérico, altura total 4 mm",
                nota="Altura sobre a face superior da PCB, com o indutor. "
                     "O trimpot de ajuste é o ponto mais alto em alguns "
                     "lotes — MEDIR."),
            "espessura_pcb": Medida(
                1.0, Prov.MEDIR, f"{U_MP} — não publicado",
                nota="MEDIR. Os módulos mini costumam usar FR4 de 1,0 mm, "
                     "mais fino que o 1,6 mm padrão."),
        },
        eletrico={
            "tensão de entrada": "4,5 – 28 V",
            "tensão de saída": "0,8 – 20 V, ajustável por trimpot",
            "corrente de saída": "3 A máx. (usar até 1,5 A com folga térmica)",
            "frequência de chaveamento": "1,5 MHz (típico 1 MHz)",
            "eficiência": "até 92 %",
        },
        notas_montagem=[
            "AJUSTAR PARA 5,0 V ANTES DE LIGAR A CARGA. O módulo sai de "
            "fábrica em tensão arbitrária; ligar o ESP32 sem ajustar "
            "queima a placa.",
            "O módulo mini normalmente NÃO tem furos de fixação — prever "
            "fita dupla-face VHB ou berço impresso. CONFERIR o exemplar.",
            "Entrada vem direto do LiPo 2S (6,0 – 8,4 V), dentro da faixa "
            "de 4,5 – 28 V.",
            "Conferir o ripple na saída antes de alimentar o ESP32; "
            "chaveamento a 1,5 MHz acopla ruído no ADC se a massa for longa.",
            "MASSA ESTIMADA (1,8 g) — PESAR. Entra no cálculo de CG (A1).",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "roda": Componente(
        num="08",
        nome="Roda ⌀34 mm com pneu O-ring de silicone",
        fabricante="Genérico / cubo impresso em PETG",
        codigo="—",
        fornecedor="AliExpress / Mercado Livre + impressão 3D",
        preco="R$ 20,00 (par)",
        url_datasheet="",
        ref_abnt="",
        massa_g=6.0,
        medidas={
            "diametro_externo": Medida(
                34.0, Prov.TIPICO,
                f"{SCAD_RODA} — WHEEL_OD = 34; bom.csv 'Roda silicone 34mm'",
                nota="Diâmetro sobre o O-ring montado. Define a altura do "
                     "eixo motriz ao solo: 17 mm (prancha A1)."),
            "largura": Medida(
                8.0, Prov.TIPICO,
                f"{SCAD_RODA} — WHEEL_WIDTH = 8"),
            "diametro_furo_eixo": Medida(
                3.0, Prov.OFICIAL,
                f"{U_N20} — vista lateral: ⌀3,0 mm [0.12 in]",
                nota="Tem que casar com o eixo do N20 (ficha 04)."),
            "achatado_furo_eixo": Medida(
                2.5, Prov.OFICIAL,
                f"{U_N20} — vista lateral: 2,5 mm [0.10 in] entre faces",
                nota="Rebaixo em D do furo, cópia do achatado do eixo. "
                     "É o que impede a roda de patinar no eixo."),
            "profundidade_furo_eixo": Medida(
                10.0, Prov.TIPICO,
                f"{SCAD_RODA} — AXLE_DEPTH = 10",
                nota="Igual ao comprimento útil do eixo do N20 (10 mm). "
                     "A roda encosta no colar de ⌀4,0 mm."),
            "diametro_canal_oring": Medida(
                30.0, Prov.TIPICO,
                f"{SCAD_RODA} — ORING_GROOVE_D = 30 (fundo do canal)"),
            "largura_canal_oring": Medida(
                3.5, Prov.TIPICO,
                f"{SCAD_RODA} — ORING_GROOVE_W = 3.5"),
            "secao_oring": Medida(
                3.0, Prov.TIPICO,
                f"{SCAD_RODA} — ORING_GROOVE_D_ORING = 3",
                nota="34 - 30 = 4 mm de diâmetro sobrando para um O-ring "
                     "de 3 mm de seção: ele sobressai 0,5 mm de cada lado "
                     "quando assentado."),
            "diametro_cubo": Medida(
                14.0, Prov.TIPICO, f"{SCAD_RODA} — HUB_D = 14"),
            "diametro_furo_grub": Medida(
                3.0, Prov.TIPICO,
                f"{SCAD_RODA} — GRUB_D = 3.0, parafuso M3 sem cabeça",
                nota="Trava anti-rotação, radial, contra o achatado do eixo."),
        },
        eletrico={},
        notas_montagem=[
            "O furo do eixo é a interface crítica: ⌀3,0 mm com achatado de "
            "2,5 mm entre faces, copiado da ficha 04. Imprimir com "
            "tolerância de 0,2 mm (TOL no wheel_hub.scad) e ajustar.",
            "Altura do eixo motriz ao solo = ⌀34/2 = 17 mm. O ball caster "
            "(ficha 09) tem que ser regulado para essa mesma altura.",
            "MASSA ESTIMADA (6,0 g por roda, cubo PETG + O-ring) — PESAR. "
            "São duas rodas no cálculo de CG (A1).",
            "Comprar O-rings de reserva: o silicone é o item de desgaste.",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "caster": Componente(
        num="09",
        nome="Ball caster com esfera de ½ pol",
        fabricante="Genérico (Robocore) / referência Pololu",
        codigo="ROB-09259 (a confirmar — ver notas)",
        fornecedor="Robocore / Mercado Livre",
        preco="R$ 8,00",
        url_datasheet=U_CASTER,
        ref_abnt=(
            "POLOLU CORPORATION. Pololu ball caster with 1/2\" ball. "
            "Las Vegas, 2026. Disponível em: "
            f"{U_CASTER_REF}. Acesso em: 12 ago. 2026."
        ),
        massa_g=2.0,
        medidas={
            "diametro_esfera": Medida(
                12.7, Prov.OFICIAL,
                f"{U_CASTER} — 'bola de rodinha metálica 1/2 polegada'; "
                "½ in = 12,70 mm por definição"),
            "altura_base": Medida(
                13.46, Prov.TIPICO,
                f"{U_CASTER_REF} — ball caster de ½ pol: base 0,53 in "
                "sem espaçadores",
                nota="Cota do conjunto Pololu equivalente. O corpo Robocore "
                     "pode diferir — MEDIR."),
            "altura_maxima": Medida(
                20.32, Prov.TIPICO,
                f"{U_CASTER_REF} — faixa montada 0,5 a 0,8 in "
                "conforme os espaçadores",
                nota="A altura de projeto é 17 mm (⌀roda/2), que cai dentro "
                     "da faixa de regulagem."),
            "entre_furos": Medida(
                14.73, Prov.TIPICO,
                f"{U_CASTER_REF} — 0,58 in entre os furos de fixação",
                nota="MEDIR antes de furar o chassi."),
            "diametro_furo_fixacao": Medida(
                2.2, Prov.TIPICO,
                f"{U_CASTER_REF} — parafusos #2-56 acompanham o kit",
                nota="Passagem para M2. A bom.csv prevê espaçadores M3 — "
                     "conferir (ver docs/DIVERGENCIAS.md, D4)."),
        },
        eletrico={},
        notas_montagem=[
            "ALTURA É A COTA CRÍTICA: tem que casar com ⌀roda/2 = 17 mm "
            "para o chassi ficar nivelado e a barra de sensores manter os "
            "4 mm ao solo. Regular com os espaçadores.",
            "CONFLITO DE DIÂMETRO: body_f1.scad usa CASTER_D = 15 mm e a "
            "bom.csv especifica ½ pol = 12,7 mm. Prevalece 12,7 mm, que é "
            "o produto efetivamente listado "
            "(ver docs/DIVERGENCIAS.md, D4).",
            "CÓDIGO A CONFIRMAR: a bom.csv cita ROB-09259, SKU da SparkFun, "
            "com link para a Robocore. Conferir o que chega.",
            "Montar atrás do eixo motriz. A posição longitudinal sai do "
            "balanço de CG da prancha A1.",
            "MASSA ESTIMADA (2,0 g) — PESAR.",
        ],
    ),
    # ═══════════════════════════════════════════════════════════════════
    "perfboard": Componente(
        num="10",
        nome="Perfboard 50 × 70 mm, passo 2,54 mm",
        fabricante="Genérico",
        codigo="—",
        fornecedor="Mercado Livre / AliExpress",
        preco="R$ 6,00",
        url_datasheet="",
        ref_abnt="",
        massa_g=12.0,
        medidas={
            "comprimento": Medida(
                70.0, Prov.TIPICO,
                "bom.csv — 'Perfboard 5×7 cm', formato padrão de mercado",
                nota="Placa cortável. O contorno final sai do alojamento "
                     "do chassi, não do fornecedor."),
            "largura": Medida(
                50.0, Prov.TIPICO,
                "bom.csv — 'Perfboard 5×7 cm', formato padrão de mercado"),
            "espessura": Medida(
                1.6, Prov.TIPICO,
                "FR4 de 1,6 mm, espessura padrão de mercado"),
            "passo_furos": Medida(
                2.54, Prov.TIPICO,
                "padrão consolidado de 0,1 in — casa com MCP3008, "
                "ESP32 DevKit V1 e TB6612FNG"),
            "diametro_furo": Medida(
                1.0, Prov.TIPICO,
                "padrão de mercado para perfboard de 2,54 mm"),
            "margem_ate_primeiro_furo": Medida(
                1.5, Prov.MEDIR,
                "varia por fabricante",
                nota="MEDIR. Define quantas colunas sobram depois do corte."),
        },
        eletrico={},
        notas_montagem=[
            "Grade útil: 70 / 2,54 ≈ 27 colunas por 50 / 2,54 ≈ 19 linhas, "
            "descontadas as margens. CONFERIR a contagem na placa em mãos.",
            "O MCP3008 em PDIP-16 ocupa 8 furos por fileira, com as duas "
            "fileiras separadas por 3 colunas (7,62 mm).",
            "Recortar conforme o alojamento do chassi — a placa inteira de "
            "50 × 70 mm não é obrigatória.",
            "MASSA ESTIMADA (12,0 g com a placa inteira) — PESAR depois do "
            "corte, já com os componentes soldados.",
        ],
    ),
}
