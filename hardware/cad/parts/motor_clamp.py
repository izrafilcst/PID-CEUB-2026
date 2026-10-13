"""Abraçadeira do N20 sobre a caixa de redução (spec §5.4).

Prende a metade de cima da caixa; a de baixo fica no berço da base. Dois M2 autoatarraxantes
nas orelhas. Entre a orelha e o berço sobra APERTO mm: o parafuso puxa a abraçadeira e prensa
o motor. Imprime de lado (perfil em U de pé): sem ponte nem balanço.
"""
from __future__ import annotations

from build123d import Part, Plane, Rot, mirror

from params import Params, altura_eixo
from parts.common import caixa_lim, cil_z

ROT_IMPRESSAO = Rot(90, 0, 0)   # eixo Y da montagem vira o Z da impressora
APERTO = 0.3                    # mm de folga orelha–berço antes de apertar
ORELHA_L = 5.0                  # largura de cada orelha em X
ORELHA_T = 3.0                  # espessura da orelha


def _meia_largura_canal(p: Params) -> float:
    return p.n20_largura / 2 + p.tol.n20_folga


def orelha_x(p: Params) -> float:
    """Centro do furo da orelha, |x|."""
    return _meia_largura_canal(p) + p.parede + ORELHA_L / 2


def furos_parafuso(p: Params, lado: int) -> list[tuple[float, float]]:
    y = lado * (p.motor_face_y - p.n20_caixa_l / 2)
    return [(-orelha_x(p), y), (orelha_x(p), y)]


def fazer(p: Params, lado: int) -> Part:
    zc = altura_eixo(p)
    w = _meia_largura_canal(p)
    topo_canal = zc + p.n20_altura / 2 + p.tol.n20_folga
    topo = topo_canal + p.parede
    y1 = p.motor_face_y - APERTO     # não encosta na parede de batente da base
    y0 = p.motor_face_y - p.n20_caixa_l
    corpo = caixa_lim(-w - p.parede, w + p.parede, y0, y1, zc, topo)
    canal = caixa_lim(-w, w, y0 - 1, y1 + 1, zc - 1, topo_canal)
    ox = orelha_x(p)
    orelhas = (caixa_lim(-ox - ORELHA_L / 2, -w - p.parede, y0, y1, zc, zc + ORELHA_T) +
               caixa_lim(w + p.parede, ox + ORELHA_L / 2, y0, y1, zc, zc + ORELHA_T))
    peca = corpo + orelhas - canal
    for x, y in furos_parafuso(p, +1):
        peca -= cil_z(p.tol.furo_m2, ORELHA_T + 2, x, y, zc - 1)
    return peca if lado > 0 else mirror(peca, about=Plane.XZ)


def paredes(p: Params) -> dict[str, float]:
    return {"lateral": p.parede, "topo": p.parede, "orelha": ORELHA_T,
            "orelha_ao_furo": (ORELHA_L - p.tol.furo_m2) / 2}
