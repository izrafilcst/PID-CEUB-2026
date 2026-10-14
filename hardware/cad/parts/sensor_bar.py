"""Barra do QTR-8A (spec §5.4–5.5).

Placa em T com espessura barra_t: a viga transversal segura o QTR por dois postes M2 e os
patins nas pontas; a língua vai para trás e apoia na base, presa por 2 M3 que sobem pela base
até insertos térmicos na língua. Imprime com a face de cima na mesa.

Calços e nivelamento: cada passo de ajuste usa um calço sob cada patim e um sob a língua
(calco_lingua). A barra sobe 0,5 mm em relação à base e aos patins ao mesmo tempo, e o
chassi continua nivelado.
"""
from __future__ import annotations

from build123d import Part, Rot

from params import Params
from parts import chicote
from parts.common import caixa_lim, cil_z
from parts.components import CALCO_T, campo_pinos_qtr

ROT_IMPRESSAO = Rot(180, 0, 0)
VIGA_X = 17.5          # largura da viga em X
PATIM_Y = 6.0          # largura do patim em Y
PATIM_X = 10.0
POSTE_D = 5.0
LINGUA_Y = 30.0
FOLGA_JANELA = 0.5     # em volta do conector do QTR, por lado


def barra_z0(p: Params, calcos: int = 0) -> float:
    """Face de baixo da barra: QTR (sensor_altura + qtr_t) + postes de 1,8 mm."""
    return p.sensor_altura + p.qtr_t + 1.82 + calcos * CALCO_T


def y_patim(p: Params) -> float:
    return p.qtr_l / 2 + 2.0 + PATIM_Y / 2 + 3.0


def furos_qtr(p: Params) -> list[tuple[float, float]]:
    """Centros dos furos de fixação do QTR-8A, na montagem.

    O QTR monta com os componentes para cima e a borda dos pinos para trás (−X); os furos
    ficam a qtr_furo_borda dessa borda (diagrama dimensional da Pololu), não no eixo da placa.
    """
    x = p.lookahead - p.qtr_w / 2 + p.qtr_furo_borda
    return [(x, -p.qtr_furos_dist / 2), (x, p.qtr_furos_dist / 2)]


def janela_conector(p: Params) -> Part:
    """Janela passante na viga sobre o campo de pinos do QTR (A-WP2-05)."""
    x0, x1, y0, y1, _ = campo_pinos_qtr(p)
    f = FOLGA_JANELA
    return caixa_lim(x0 - f, x1 + f, y0 - f, y1 + f, -20.0, 100.0)


def furos_base(p: Params) -> list[tuple[float, float]]:
    x = p.barra_lingua_x + 6.0
    return [(x, -8.0), (x, 8.0)]


def fazer(p: Params, calcos: int = 0) -> Part:
    z0 = barra_z0(p, calcos)
    z1 = z0 + p.barra_t
    xa = p.lookahead
    ya = y_patim(p) + PATIM_Y / 2
    viga = caixa_lim(xa - VIGA_X / 2, xa + VIGA_X / 2, -ya, ya, z0, z1)
    lingua = caixa_lim(p.barra_lingua_x, xa, -LINGUA_Y / 2, LINGUA_Y / 2,
                       max(z0, p.base_topo), z1)
    barra = viga + lingua
    # Postes do QTR (descem até a face de cima do QTR) com furo para M2 autoatarraxante
    z_qtr_topo = p.sensor_altura + calcos * CALCO_T + p.qtr_t
    for x, y in furos_qtr(p):
        barra += cil_z(POSTE_D, z0 - z_qtr_topo, x, y, z_qtr_topo)
        barra -= cil_z(p.tol.autoatarraxante_m2, p.tol.autoatarraxante_prof, x, y, z_qtr_topo)
    # Furos dos patins (M2 de cima para baixo) e insertos M3 da língua (de baixo)
    for y in (-y_patim(p), y_patim(p)):
        barra -= cil_z(p.tol.furo_m2, p.barra_t + 2, xa, y, z0 - 1)
    for x, y in furos_base(p):
        barra -= cil_z(p.tol.inserto_m3_d, p.tol.inserto_m3_prof, x, y, max(z0, p.base_topo))
    # Ligação do QTR: janela do conector na viga e rasgo do chicote na ponta da língua
    return barra - janela_conector(p) - chicote.rasgo_lingua(p)


def paredes(p: Params) -> dict[str, float]:
    x0_janela = campo_pinos_qtr(p)[0] - FOLGA_JANELA
    x_ins, y_ins = furos_base(p)[1]
    dx = max(x_ins - (p.barra_lingua_x + chicote.RASGO_L), 0.0)
    dy = max(abs(y_ins) - chicote.LARGURA / 2, 0.0)
    return {"barra": p.barra_t, "poste_ao_furo": (POSTE_D - p.tol.autoatarraxante_m2) / 2,
            "lingua_sobre_inserto": p.barra_t - p.tol.inserto_m3_prof,
            "viga_atras_da_janela": x0_janela - (p.lookahead - VIGA_X / 2),
            "rasgo_ao_inserto": (dx * dx + dy * dy) ** 0.5 - p.tol.inserto_m3_d / 2}


def calco_lingua(p: Params, n: int = 1) -> Part:
    """n calços de 0,5 mm entre a língua e a base, com os dois furos M3 de passagem."""
    x_fim = furos_base(p)[0][0] + 6.0
    c = caixa_lim(p.barra_lingua_x, x_fim, -LINGUA_Y / 2, LINGUA_Y / 2, 0.0, 0.5 * n)
    for x, y in furos_base(p):
        c -= cil_z(p.tol.furo_m3, 2, x, y, -1)
    return c - chicote.rasgo_lingua(p)
