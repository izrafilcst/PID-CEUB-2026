#!/usr/bin/env python3
"""Gera as 10 pranchas de componente a partir de specs.py.

Uso:
    python3 gen_component_drawings.py
    python3 verify_drawings.py     # depois, para rasterizar e inspecionar

Projeção no 1º diedro (NBR 10067): a vista superior fica ABAIXO da vista
frontal, alinhada pelo mesmo X.
"""
from __future__ import annotations

import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "hardware", "components"))
sys.path.insert(0, os.path.join(RAIZ, "tools"))

from specs import COMPONENTES, Componente  # noqa: E402
from techdraw import (H_LABEL, MARGIN_LEFT, MARGIN_OTHER, SHEET_H,  # noqa: E402
                      SHEET_W, Prov, Sheet, View, dim_diam, dim_h, dim_v,
                      fmt_valor, note, txt)

DIR_SAIDA = os.path.join(RAIZ, "hardware", "drawings", "comp")
DATA = "2026-08-12"

# Área útil da folha descontando legenda e quadro de proveniência
UTIL_W = SHEET_W - MARGIN_LEFT - MARGIN_OTHER - 20.0   # 242 mm
UTIL_H = SHEET_H - 2 * MARGIN_OTHER - 45.0             # 145 mm

# Faixa reservada de cada lado da peça para linhas de extensão, texto de
# cota e chamadas. Sem ela a escolha de escala encosta a peça na moldura.
ORLA_COTAS = 45.0

# Cotas que não fazem parte do contorno da peça e por isso não podem
# governar a escolha de escala.
NAO_E_CONTORNO = ("comprimento_chicote", "distancia_otima_leitura")

# ═══ Grade de posicionamento das vistas (mm de papel) ═══
X0 = 55.0           # abscissa da origem das vistas
Y_TOP = 140.0       # linha de base da vista superior
GAP_VISTAS = 28.0   # folga entre a vista frontal e a superior


def _maior_dimensao(c: Componente) -> float:
    """Maior cota do CONTORNO da peça, em mm reais."""
    return max(m.valor for nome, m in c.medidas.items()
               if nome not in NAO_E_CONTORNO)


def escala_para(c: Componente) -> tuple[float, str]:
    """Escolhe a maior escala normalizada que ainda cabe na área útil.

    O critério inclui a orla de cotagem: a peça desenhada mais a faixa de
    cotas dos dois lados tem que caber em 75 % da largura útil.
    """
    maior = _maior_dimensao(c)
    for fator, rotulo in ((2.0, "2:1"), (1.0, "1:1"), (0.5, "1:2")):
        if maior * fator + 2 * ORLA_COTAS <= UTIL_W * 0.75:
            return fator, rotulo
    return 0.5, "1:2"


def _folha(c: Componente, rotulo: str) -> Sheet:
    return Sheet(title=c.nome, maker=c.fabricante, code=c.codigo,
                 scale=rotulo, sheet_id=c.num, date=DATA)


def _rotulo_vista(x: float, y: float, texto: str) -> str:
    """Título de uma vista, em mm de papel."""
    return txt(x, y, texto, H_LABEL, weight="bold")


# Largura média de um caractere a 2,5 mm de altura, medida na DejaVu Sans.
CHAR_W = 1.35
BORDA_ESQ = MARGIN_LEFT + 2.0        # 27 mm
BORDA_DIR = SHEET_W - MARGIN_OTHER - 2.0   # 285 mm


def _nota(v: View, x: float, y: float, texto: str,
          dx: float, dy: float) -> str:
    """Nota com linha-guia que não deixa o texto sair da moldura.

    `note()` ancora o texto pelo lado do deslocamento: dx positivo escreve
    para a direita, negativo para a esquerda. Numa peça encostada na borda
    isso joga o texto para fora da folha, então aqui o lado é invertido
    quando o texto não couber.
    """
    px, _ = v.to_paper(x, y)
    largura = len(texto) * CHAR_W
    if dx >= 0:
        if px + dx + 6.0 + largura > BORDA_DIR:
            dx = -abs(dx)
    if dx < 0:
        if px + dx - 6.0 - largura < BORDA_ESQ:
            dx = abs(dx)
    return note(v, x, y, texto, dx=dx, dy=dy)


def _placa_plana(c: Componente, k_comp: str, k_larg: str, k_esp: str,
                 notas_sup: list[tuple[str, float, float, float, float]],
                 notas_front: list[tuple[str, float, float, float, float]],
                 extras_sup=None, extras_front=None,
                 rotulo_front: str = "VISTA FRONTAL",
                 rotulo_sup: str = "VISTA SUPERIOR") -> str:
    """Prancha padrão de peça chata: vista frontal (topo) e superior (baixo).

    k_comp, k_larg, k_esp: chaves em `c.medidas` do comprimento, da largura
    e da espessura. As notas são tuplas (texto, x_peça, y_peça, dx, dy).
    """
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    L = c.medidas[k_comp]
    W = c.medidas[k_larg]
    T = c.medidas[k_esp]

    # ── vista superior (embaixo, 1º diedro) ────────────────────────────
    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))
    if extras_sup:
        s.add(extras_sup(s, sup, fator))
    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=12.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=12.0))
    s.add(_rotulo_vista(X0, Y_TOP + 26.0, rotulo_sup))
    for texto, px, py, dx, dy in notas_sup:
        s.add(_nota(sup, px, py, texto, dx=dx, dy=dy))

    # ── vista frontal (em cima) ────────────────────────────────────────
    y_front = Y_TOP - W.valor * fator - GAP_VISTAS
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, 0.0, L.valor, T.valor))
    if extras_front:
        s.add(extras_front(s, front, fator))
    s.add(dim_v(front, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(_rotulo_vista(X0, y_front - T.valor * fator - 5.0, rotulo_front))
    for texto, px, py, dx, dy in notas_front:
        s.add(_nota(front, px, py, texto, dx=dx, dy=dy))

    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 01 — ESP32 DevKit V1
# ═══════════════════════════════════════════════════════════════════════
def prancha_esp32(c: Componente) -> str:
    """ESP32 DevKit V1: contorno da PCB, fileiras de pinos e módulo."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    L = c.medidas["comprimento"]
    W = c.medidas["largura"]
    T = c.medidas["espessura_pcb"]
    FIL = c.medidas["distancia_fileiras"]
    PAS = c.medidas["passo_pinos"]
    MC = c.medidas["modulo_comprimento"]
    ML = c.medidas["modulo_largura"]
    MA = c.medidas["modulo_altura"]

    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))

    # fileiras de pinos: 15 por lado, centradas no comprimento
    y_inf = (W.valor - FIL.valor) / 2.0
    y_sup = y_inf + FIL.valor
    x_ini = (L.valor - 14 * PAS.valor) / 2.0
    for i in range(15):
        cx = x_ini + i * PAS.valor
        s.add(sup.circle_part(cx, y_inf, d=1.0))
        s.add(sup.circle_part(cx, y_sup, d=1.0))
    s.add(sup.centerline_h(L.valor / 2.0, y_inf, L.valor * 0.9))
    s.add(sup.centerline_h(L.valor / 2.0, y_sup, L.valor * 0.9))

    # módulo ESP32-WROOM-32 na extremidade oposta ao USB
    mx = L.valor - MC.valor - 1.0
    my = (W.valor - ML.valor) / 2.0
    s.add(sup.rect_part(mx, my, MC.valor, ML.valor))

    # conector micro-USB, borda x = 0
    s.add(sup.rect_part(0.0, (W.valor - 8.0) / 2.0, 5.5, 8.0))

    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=22.0))
    s.add(dim_h(sup, x_ini, x_ini + PAS.valor, 0.0, PAS.valor, PAS.prov,
                offset=12.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=22.0))
    s.add(dim_v(sup, y_inf, y_sup, 0.0, FIL.valor, FIL.prov, offset=12.0))
    s.add(_rotulo_vista(X0, Y_TOP + 36.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, mx + MC.valor / 2.0, my + ML.valor,
                "modulo ESP32-WROOM-32", dx=40.0, dy=-18.0))
    s.add(_nota(sup, 2.0, W.valor / 2.0 + 4.0,
                "conector micro-USB", dx=16.0, dy=-16.0))

    y_front = Y_TOP - W.valor * fator - GAP_VISTAS
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, 0.0, L.valor, T.valor))
    s.add(front.rect_part(mx, T.valor, MC.valor, MA.valor))
    s.add(dim_v(front, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(dim_v(front, T.valor, T.valor + MA.valor, L.valor, MA.valor,
                MA.prov, offset=-24.0))
    s.add(_rotulo_vista(X0, y_front - T.valor * fator - MA.valor * fator - 6.0,
                        "VISTA FRONTAL"))
    s.add(_nota(front, L.valor / 2.0, 0.0,
                "altura com barra de pinos soldada: prever 12 mm",
                dx=30.0, dy=-30.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 02 — MCP3008
# ═══════════════════════════════════════════════════════════════════════
def prancha_mcp3008(c: Componente) -> str:
    """MCP3008 PDIP-16: corpo, fileiras de terminais e altura."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    D = c.medidas["comprimento"]
    E1 = c.medidas["largura_corpo"]
    FIL = c.medidas["passo_fileiras"]
    PAS = c.medidas["passo_pinos"]
    A2 = c.medidas["altura_corpo"]
    A = c.medidas["altura_total"]
    LP = c.medidas["comprimento_pino"]

    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    # corpo moldado, centrado nas fileiras
    y_corpo = (FIL.valor - E1.valor) / 2.0
    s.add(sup.rect_part(0.0, y_corpo, D.valor, E1.valor))

    x_ini = (D.valor - 7 * PAS.valor) / 2.0
    for i in range(8):
        cx = x_ini + i * PAS.valor
        s.add(sup.rect_part(cx - 0.23, -0.6, 0.46, y_corpo + 0.6))
        s.add(sup.rect_part(cx - 0.23, FIL.valor - 0.0,
                            0.46, y_corpo + 0.6))
    # marcação do pino 1
    s.add(sup.circle_part(x_ini, y_corpo + E1.valor - 1.4, d=1.2))
    s.add(sup.centerline_h(D.valor / 2.0, FIL.valor / 2.0, D.valor * 1.05))

    s.add(dim_h(sup, 0.0, D.valor, y_corpo, D.valor, D.prov, offset=26.0))
    s.add(dim_h(sup, x_ini, x_ini + PAS.valor, y_corpo, PAS.valor, PAS.prov,
                offset=16.0))
    s.add(dim_v(sup, y_corpo, y_corpo + E1.valor, 0.0, E1.valor, E1.prov,
                offset=12.0))
    s.add(dim_v(sup, 0.0, FIL.valor, 0.0, FIL.valor, FIL.prov, offset=24.0))
    s.add(_rotulo_vista(X0, Y_TOP + 40.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, x_ini, y_corpo + E1.valor - 1.4,
               "marcacao do pino 1", dx=-16.0, dy=-20.0))
    s.add(_nota(sup, D.valor, FIL.valor / 2.0,
               "encapsulamento PDIP-16, corpo de 300 mil", dx=16.0, dy=-8.0))

    y_front = Y_TOP - FIL.valor * fator - GAP_VISTAS - 14.0
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, LP.valor, D.valor, A2.valor))
    for i in range(8):
        cx = x_ini + i * PAS.valor
        s.add(front.rect_part(cx - 0.23, 0.0, 0.46, LP.valor))
    s.add(dim_v(front, LP.valor, LP.valor + A2.valor, 0.0, A2.valor,
                A2.prov, offset=12.0))
    s.add(dim_v(front, 0.0, A.valor, D.valor, A.valor, A.prov, offset=-26.0))
    s.add(dim_v(front, 0.0, LP.valor, D.valor, LP.valor, LP.prov,
                offset=-12.0))
    s.add(_rotulo_vista(X0, y_front - (LP.valor + A2.valor) * fator - 5.0,
                        "VISTA FRONTAL"))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 03 — QTR-8A
# ═══════════════════════════════════════════════════════════════════════
def prancha_qtr8a(c: Componente) -> str:
    """QTR-8A: vista superior com o passo dos sensores, e vista frontal."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    L = c.medidas["comprimento"]
    W = c.medidas["largura"]
    T = c.medidas["espessura"]
    P = c.medidas["passo_sensores"]

    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))

    # centros dos 8 sensores, simétricos em torno do meio da placa
    meio = L.valor / 2.0
    centros = [meio + (i - 3.5) * P.valor for i in range(8)]
    for cx in centros:
        s.add(sup.cross(cx, W.valor / 2.0, 6.0))
    s.add(sup.centerline_h(meio, W.valor / 2.0, L.valor * 1.02))

    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=22.0))
    s.add(dim_h(sup, centros[0], centros[1], 0.0, P.valor, P.prov,
                offset=12.0))
    s.add(dim_h(sup, centros[0], centros[7], W.valor, P.valor * 7.0, P.prov,
                offset=-14.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=12.0))
    s.add(_rotulo_vista(X0, Y_TOP + 36.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, centros[7], W.valor / 2.0,
                "8 x par LED IR / fototransistor", dx=16.0, dy=-6.0))

    y_front = Y_TOP - W.valor * fator - GAP_VISTAS
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, 0.0, L.valor, T.valor))
    s.add(dim_v(front, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(_rotulo_vista(X0, y_front - T.valor * fator - 5.0, "VISTA FRONTAL"))
    s.add(_nota(front, L.valor, T.valor,
                "espessura sem header soldado", dx=14.0, dy=-12.0))
    s.add(_nota(front, L.valor * 0.5, T.valor,
                "altura de projeto ao solo: 4 mm (otimo Pololu: 3 mm)",
                dx=30.0, dy=-32.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 04 — N20 com encoder
# ═══════════════════════════════════════════════════════════════════════
def prancha_n20(c: Componente) -> str:
    """N20: vista superior (eixo, redutor, motor, encoder) e frontal."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    m = c.medidas
    LCX = m["comprimento_caixa_reducao"].valor
    LMT = m["comprimento_motor"].valor
    LEN = m["comprimento_encoder"].valor
    LEX = m["comprimento_eixo"].valor
    DEX = m["diametro_eixo"]
    DCO = m["diametro_colar"]
    ACH = m["achatado_eixo"]
    LAR = m["largura_caixa"]
    ALT = m["altura_caixa"]
    ABA = m["largura_total_abas"]
    EFV = m["entre_furos_vertical"]
    OFH = m["offset_furos_horizontal"]
    TOT = m["comprimento_total"]

    # ── vista superior: perfil ao longo do eixo ────────────────────────
    # Origem no encontro do eixo com a face da caixa de redução.
    y_top = Y_TOP - 6.0
    sup = View(origin_x=X0 + 24.0, origin_y=y_top, scale=fator)
    meio = LAR.valor / 2.0
    # eixo de saída (para -X)
    s.add(sup.rect_part(-LEX, meio - DEX.valor / 2.0, LEX, DEX.valor))
    # colar
    s.add(sup.rect_part(-0.7, meio - DCO.valor / 2.0, 0.7, DCO.valor))
    # caixa de redução, corpo do motor e encoder
    s.add(sup.rect_part(0.0, 0.0, LCX, LAR.valor))
    s.add(sup.rect_part(LCX, 0.0, LMT, LAR.valor))
    s.add(sup.rect_part(LCX + LMT, 0.0, LEN, LAR.valor))
    s.add(sup.centerline_h(TOT.valor / 2.0 - LEX / 2.0, meio,
                           TOT.valor + LEX))

    s.add(dim_h(sup, 0.0, LCX, 0.0, LCX, m["comprimento_caixa_reducao"].prov,
                offset=12.0))
    s.add(dim_h(sup, LCX, LCX + LMT, 0.0, LMT, m["comprimento_motor"].prov,
                offset=12.0))
    s.add(dim_h(sup, LCX + LMT, LCX + LMT + LEN, 0.0, LEN,
                m["comprimento_encoder"].prov, offset=12.0))
    s.add(dim_h(sup, 0.0, TOT.valor, 0.0, TOT.valor, TOT.prov, offset=22.0))
    s.add(dim_h(sup, -LEX, 0.0, 0.0, LEX, m["comprimento_eixo"].prov,
                offset=32.0))
    s.add(dim_v(sup, meio - DEX.valor / 2.0, meio + DEX.valor / 2.0, -LEX,
                DEX.valor, DEX.prov, offset=12.0))
    s.add(_rotulo_vista(X0 - 20.0, y_top + 42.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, LCX + LMT + LEN / 2.0, LAR.valor,
                "conector JST SH 6 vias", dx=10.0, dy=-22.0))
    s.add(_nota(sup, -LEX / 2.0, meio + DEX.valor / 2.0,
                "achatado em D: 2,5 mm entre faces", dx=-4.0, dy=-30.0))

    # ── vista frontal: face da caixa de redução ────────────────────────
    # 1º diedro: a vista pela esquerda vai à direita, alinhada pelo mesmo
    # eixo horizontal da vista superior.
    y_eixo = y_top - meio * fator
    y_front = y_eixo + ALT.valor / 2.0 * fator
    front = View(origin_x=X0 + 145.0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(-LAR.valor / 2.0, 0.0, LAR.valor, ALT.valor))
    s.add(front.rect_part(-ABA.valor / 2.0, ALT.valor / 2.0 - 3.0,
                          ABA.valor, 6.0))
    s.add(front.circle_part(0.0, ALT.valor / 2.0, d=DCO.valor))
    s.add(front.circle_part(0.0, ALT.valor / 2.0, d=DEX.valor))
    s.add(front.cross(0.0, ALT.valor / 2.0, 10.0))
    # furos M1.6, em diagonal
    for sx, sy in ((+1.0, +1.0), (-1.0, -1.0)):
        fx = sx * OFH.valor
        fy = ALT.valor / 2.0 + sy * EFV.valor / 2.0
        s.add(front.circle_part(fx, fy, d=1.6))
        s.add(front.cross(fx, fy, 5.0))

    s.add(dim_h(front, -OFH.valor, OFH.valor, 0.0, OFH.valor * 2.0,
                OFH.prov, offset=12.0))
    s.add(dim_h(front, -ABA.valor / 2.0, ABA.valor / 2.0, 0.0, ABA.valor,
                ABA.prov, offset=22.0))
    s.add(dim_v(front, ALT.valor / 2.0 - EFV.valor / 2.0,
                ALT.valor / 2.0 + EFV.valor / 2.0,
                -LAR.valor / 2.0, EFV.valor, EFV.prov, offset=12.0))
    s.add(dim_v(front, 0.0, ALT.valor, -LAR.valor / 2.0, ALT.valor,
                ALT.prov, offset=24.0))
    s.add(_rotulo_vista(X0 + 122.0, y_front - ALT.valor * fator - 8.0,
                        "VISTA FRONTAL"))
    s.add(_nota(front, OFH.valor, ALT.valor / 2.0 + EFV.valor / 2.0,
                "2 x M1.6 (bom.csv preve M2 - conferir)",
                dx=12.0, dy=-20.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 05 — TB6612FNG
# ═══════════════════════════════════════════════════════════════════════
def prancha_tb6612(c: Componente) -> str:
    def _extras(sheet, v, fator):
        PAS = c.medidas["passo_pinos"].valor
        L = c.medidas["comprimento"].valor
        W = c.medidas["largura"].valor
        partes = []
        # duas fileiras de 8 pinos, centradas no comprimento
        x_ini = (L - 7 * PAS) / 2.0
        for y in (1.5, W - 1.5):
            for i in range(8):
                partes.append(v.circle_part(x_ini + i * PAS, y, d=1.0))
        return "".join(partes)

    return _placa_plana(
        c, "comprimento", "largura", "espessura_pcb",
        notas_sup=[
            ("STBY precisa de nivel ALTO para o driver operar",
             20.32, 20.32, 24.0, -14.0),
            ("furos de fixacao nao cotados pelo fabricante - MEDIR",
             20.32, 5.0, 16.0, 4.0),
        ],
        notas_front=[
            ("passo dos pinos: 2,54 mm nas duas fileiras",
             20.32, 1.6, 18.0, -8.0),
        ],
        extras_sup=_extras,
    )


# ═══════════════════════════════════════════════════════════════════════
# 06 — LiPo 2S
# ═══════════════════════════════════════════════════════════════════════
def prancha_lipo(c: Componente) -> str:
    """LiPo: todas as cotas são de projeto e estão marcadas MEDIR."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    L = c.medidas["comprimento"]
    W = c.medidas["largura"]
    T = c.medidas["altura"]
    CH = c.medidas["comprimento_chicote"]

    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))
    # chicote saindo pela face frontal, representado dobrado
    s.add(sup.polyline([(L.valor, W.valor / 2.0),
                        (L.valor + 14.0, W.valor / 2.0),
                        (L.valor + 14.0, W.valor / 2.0 - 10.0)]))
    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=12.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=12.0))
    s.add(_rotulo_vista(X0, Y_TOP + 26.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, L.valor + 14.0, W.valor / 2.0 - 6.0,
               "chicote de forca + balanceamento: 100 mm (MEDIR)",
               dx=12.0, dy=8.0))
    s.add(_nota(sup, L.valor / 2.0, W.valor,
               "envelope maximo do projeto: 65 x 35 x 12 mm",
               dx=0.0, dy=-16.0))

    y_front = Y_TOP - W.valor * fator - GAP_VISTAS
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, 0.0, L.valor, T.valor))
    s.add(dim_v(front, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(_rotulo_vista(X0, y_front - T.valor * fator - 5.0, "VISTA FRONTAL"))
    s.add(_nota(front, L.valor, T.valor / 2.0,
               "TODAS as cotas sao de projeto, nao do fornecedor - MEDIR",
               dx=16.0, dy=-6.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 07 — MP1584EN
# ═══════════════════════════════════════════════════════════════════════
def prancha_mp1584(c: Componente) -> str:
    def _extras(sheet, v, fator):
        partes = [
            # indutor e trimpot, blocos indicativos
            v.rect_part(12.0, 5.0, 6.5, 6.5),
            v.rect_part(3.0, 6.0, 5.0, 5.0),
        ]
        return "".join(partes)

    return _placa_plana(
        c, "comprimento", "largura", "altura_com_indutor",
        notas_sup=[
            ("AJUSTAR PARA 5,0 V ANTES DE LIGAR A CARGA",
             11.0, 17.0, 16.0, -16.0),
            ("modulo mini normalmente SEM furos de fixacao - conferir",
             22.0, 4.0, 16.0, 6.0),
        ],
        notas_front=[
            ("altura sobre a face superior, com o indutor",
             22.0, 4.0, 16.0, -8.0),
        ],
        extras_sup=_extras,
        rotulo_front="VISTA FRONTAL (altura total)",
    )


# ═══════════════════════════════════════════════════════════════════════
# 08 — Roda impressa com O-ring (hardware/cad/parts/wheel.py)
# ═══════════════════════════════════════════════════════════════════════
def prancha_roda(c: Componente) -> str:
    """Roda: vista frontal (circular) e vista lateral cortada."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    m = c.medidas
    DE = m["diametro_externo"]
    LG = m["largura"]
    DF = m["diametro_furo_eixo"]
    ACH = m["achatado_furo_eixo"]
    PF = m["profundidade_furo_eixo"]
    DCA = m["diametro_canal_oring"]
    LCA = m["largura_canal_oring"]
    DCU = m["diametro_cubo"]
    DGR = m["diametro_furo_grub"]

    # ── vista frontal: círculos concêntricos ───────────────────────────
    cx, cy = X0 + 30.0, 78.0
    front = View(origin_x=cx, origin_y=cy, scale=fator)
    s.add(front.circle_part(0.0, 0.0, d=DE.valor))
    s.add(front.circle_part(0.0, 0.0, d=DCA.valor))
    s.add(front.circle_part(0.0, 0.0, d=DCU.valor))
    s.add(front.circle_part(0.0, 0.0, d=DF.valor))
    s.add(front.cross(0.0, 0.0, DE.valor + 6.0))
    # achatado do furo em D
    meia = ACH.valor - DF.valor / 2.0
    s.add(front.polyline([(-DF.valor / 2.0, meia), (DF.valor / 2.0, meia)]))

    s.add(dim_diam(front, 0.0, 0.0, DE.valor, DE.prov, angulo=140.0,
                   guia=12.0))
    s.add(dim_diam(front, 0.0, 0.0, DCA.valor, DCA.prov, angulo=95.0,
                   guia=12.0))
    s.add(dim_diam(front, 0.0, 0.0, DCU.valor, DCU.prov, angulo=-45.0,
                   guia=22.0))
    s.add(dim_diam(front, 0.0, 0.0, DF.valor, DF.prov, angulo=-135.0,
                   guia=40.0))
    s.add(_rotulo_vista(cx - 26.0, cy + DE.valor * fator / 2.0 + 12.0,
                        "VISTA FRONTAL"))
    s.add(_nota(front, 0.0, meia,
                f"achatado em D: {fmt_valor(ACH.valor)} mm (eixo da ficha 04)",
                dx=20.0, dy=42.0))

    # ── vista lateral ──────────────────────────────────────────────────
    lx, ly = X0 + 120.0, 78.0
    lat = View(origin_x=lx, origin_y=ly + DE.valor * fator / 2.0, scale=fator)
    s.add(lat.rect_part(0.0, 0.0, LG.valor, DE.valor))
    # canal do O-ring nas duas faces do diâmetro externo
    for base in (0.0, DE.valor - (DE.valor - DCA.valor) / 2.0):
        s.add(lat.rect_part((LG.valor - LCA.valor) / 2.0, base,
                            LCA.valor, (DE.valor - DCA.valor) / 2.0))
    # Cubo: passa da largura do aro quando o furo do eixo é mais fundo que
    # o aro (roda v2: cubo de 9,4 mm, aro de 8,2 mm — D9 resolvido).
    prof = PF.valor
    if PF.valor > LG.valor:
        s.add(lat.rect_part(LG.valor, DE.valor / 2.0 - DCU.valor / 2.0,
                            PF.valor - LG.valor, DCU.valor))
    s.add(lat.hidden([(0.0, DE.valor / 2.0 - DF.valor / 2.0),
                      (prof, DE.valor / 2.0 - DF.valor / 2.0)]))
    s.add(lat.hidden([(0.0, DE.valor / 2.0 + DF.valor / 2.0),
                      (prof, DE.valor / 2.0 + DF.valor / 2.0)]))
    s.add(lat.centerline_h(LG.valor / 2.0, DE.valor / 2.0, LG.valor * 2.4))

    s.add(dim_h(lat, 0.0, LG.valor, 0.0, LG.valor, LG.prov, offset=14.0))
    s.add(dim_v(lat, 0.0, DE.valor, 0.0, DE.valor, DE.prov, offset=16.0))
    s.add(_rotulo_vista(lx - 6.0, ly + DE.valor * fator / 2.0 + 26.0,
                        "VISTA LATERAL"))
    s.add(_nota(lat, LG.valor, DE.valor * 0.9,
                f"canal do O-ring de {fmt_valor(m['secao_oring'].valor)} mm de secao",
                dx=16.0, dy=6.0))
    s.add(_nota(lat, LG.valor, DE.valor / 2.0,
                f"furo radial p/ M2 autoatarraxante (⌀{fmt_valor(DGR.valor)} mm)",
                dx=18.0, dy=14.0))
    s.add(_nota(lat, LG.valor / 2.0, 0.0,
                f"furo em D: {fmt_valor(PF.valor)} mm no cubo (D9 resolvido)",
                dx=14.0, dy=30.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 09 — Ball caster ½"
# ═══════════════════════════════════════════════════════════════════════
def prancha_caster(c: Componente) -> str:
    """Ball caster: vista frontal e lateral, com a faixa de regulagem."""
    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    m = c.medidas
    DB = m["diametro_esfera"]
    HB = m["altura_base"]
    HM = m["altura_maxima"]
    EF = m["entre_furos"]
    DFX = m["diametro_furo_fixacao"]

    # ── vista frontal ──────────────────────────────────────────────────
    cx, cy = X0 + 20.0, 110.0
    front = View(origin_x=cx, origin_y=cy, scale=fator)
    # esfera apoiada no solo (y = 0)
    s.add(front.circle_part(0.0, DB.valor / 2.0, d=DB.valor))
    s.add(front.cross(0.0, DB.valor / 2.0, DB.valor + 6.0))
    # corpo do suporte
    s.add(front.rect_part(-EF.valor / 2.0 - 3.0, DB.valor * 0.75,
                          EF.valor + 6.0, HB.valor - DB.valor * 0.75))
    for sx in (-1.0, 1.0):
        s.add(front.circle_part(sx * EF.valor / 2.0, HB.valor - 2.0,
                                d=DFX.valor))
        s.add(front.cross(sx * EF.valor / 2.0, HB.valor - 2.0, 5.0))
    # solo
    s.add(front.polyline([(-EF.valor, 0.0), (EF.valor, 0.0)]))

    s.add(dim_h(front, -EF.valor / 2.0, EF.valor / 2.0, 0.0, EF.valor,
                EF.prov, offset=14.0))
    s.add(dim_v(front, 0.0, HB.valor, -EF.valor / 2.0 - 3.0, HB.valor,
                HB.prov, offset=14.0))
    s.add(dim_v(front, 0.0, HM.valor, EF.valor / 2.0 + 3.0, HM.valor,
                HM.prov, offset=-26.0))
    s.add(dim_diam(front, 0.0, DB.valor / 2.0, DB.valor, DB.prov,
                   angulo=-135.0, guia=14.0))
    s.add(_rotulo_vista(cx - 18.0, cy + 22.0, "VISTA FRONTAL"))
    s.add(_nota(front, EF.valor / 2.0, HB.valor - 2.0,
               "2 x passagem M2 (o kit traz parafusos #2-56)",
               dx=16.0, dy=-16.0))

    # ── vista lateral ──────────────────────────────────────────────────
    lx = X0 + 130.0
    lat = View(origin_x=lx, origin_y=cy, scale=fator)
    s.add(lat.circle_part(0.0, DB.valor / 2.0, d=DB.valor))
    s.add(lat.rect_part(-DB.valor / 2.0 - 1.0, DB.valor * 0.75,
                        DB.valor + 2.0, HB.valor - DB.valor * 0.75))
    s.add(lat.centerline_v(0.0, DB.valor / 2.0, DB.valor + 8.0))
    s.add(lat.polyline([(-DB.valor, 0.0), (DB.valor, 0.0)]))
    s.add(_rotulo_vista(lx - 18.0, cy + 22.0, "VISTA LATERAL"))
    s.add(_nota(lat, 0.0, HB.valor,
               "ALTURA DE PROJETO: 17 mm = ⌀roda/2 (ficha 08)",
               dx=14.0, dy=-30.0))
    s.add(_nota(lat, 0.0, 0.0,
               "regulagem 12,7 a 20,3 mm com os espacadores",
               dx=14.0, dy=16.0))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# 10 — Perfboard
# ═══════════════════════════════════════════════════════════════════════
def prancha_perfboard(c: Componente) -> str:
    def _extras(sheet, v, fator):
        PAS = c.medidas["passo_furos"].valor
        DFU = c.medidas["diametro_furo"].valor
        MG = c.medidas["margem_ate_primeiro_furo"].valor
        partes = []
        n_col = int((70.0 - 2 * MG) // PAS) + 1
        n_lin = int((50.0 - 2 * MG) // PAS) + 1
        for i in range(n_col):
            for j in range(n_lin):
                partes.append(v.circle_part(MG + i * PAS, MG + j * PAS,
                                            d=DFU, lw=0.18))
        return "".join(partes)

    fator, rotulo = escala_para(c)
    s = _folha(c, rotulo)

    L = c.medidas["comprimento"]
    W = c.medidas["largura"]
    T = c.medidas["espessura"]
    PAS = c.medidas["passo_furos"]
    MG = c.medidas["margem_ate_primeiro_furo"]

    sup = View(origin_x=X0, origin_y=Y_TOP, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))
    s.add(_extras(s, sup, fator))
    n_col = int((70.0 - 2 * MG.valor) // PAS.valor) + 1
    n_lin = int((50.0 - 2 * MG.valor) // PAS.valor) + 1

    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=22.0))
    s.add(dim_h(sup, MG.valor, MG.valor + PAS.valor, 0.0, PAS.valor,
                PAS.prov, offset=12.0))
    s.add(dim_h(sup, 0.0, MG.valor, W.valor, MG.valor, MG.prov, offset=-14.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=12.0))
    s.add(_rotulo_vista(X0, Y_TOP + 36.0, "VISTA SUPERIOR"))
    s.add(_nota(sup, L.valor, W.valor / 2.0,
               f"grade de {n_col} x {n_lin} furos - CONFERIR na placa",
               dx=16.0, dy=-6.0))
    s.add(_nota(sup, L.valor / 2.0, W.valor,
               "recortar conforme o alojamento do chassi", dx=0.0, dy=-16.0))

    y_front = Y_TOP - W.valor * fator - GAP_VISTAS
    front = View(origin_x=X0, origin_y=y_front, scale=fator)
    s.add(front.rect_part(0.0, 0.0, L.valor, T.valor))
    s.add(dim_v(front, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(_rotulo_vista(X0, y_front - T.valor * fator - 5.0, "VISTA FRONTAL"))
    return s.render()


PRANCHAS = {
    "esp32": prancha_esp32,
    "mcp3008": prancha_mcp3008,
    "qtr8a": prancha_qtr8a,
    "n20": prancha_n20,
    "tb6612": prancha_tb6612,
    "lipo": prancha_lipo,
    "mp1584": prancha_mp1584,
    "roda": prancha_roda,
    "caster": prancha_caster,
    "perfboard": prancha_perfboard,
}


def main(argv: list[str]) -> int:
    os.makedirs(DIR_SAIDA, exist_ok=True)
    for chave, fn in sorted(PRANCHAS.items(),
                            key=lambda kv: COMPONENTES[kv[0]].num):
        c = COMPONENTES[chave]
        destino = os.path.join(DIR_SAIDA, f"{c.num}-{chave}.svg")
        with open(destino, "w", encoding="utf-8") as fh:
            fh.write(fn(c))
        print(f"gerado: {destino}")
    print(f"\n{len(PRANCHAS)} prancha(s). Rode verify_drawings.py e INSPECIONE.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
