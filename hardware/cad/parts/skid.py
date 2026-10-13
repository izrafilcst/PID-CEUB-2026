"""Patim substituível (spec §5.5): calota de raio RAIO_CALOTA no contato, fixo por um M2 que
desce pela barra. A calota faz o papel da rampa de entrada (≈ 30° num degrau de 0,5 mm)."""
from __future__ import annotations

from build123d import Part, Pos, Rot, Sphere

from params import Params
from parts.common import caixa, caixa_lim, cil_z
from parts.sensor_bar import PATIM_X, PATIM_Y, barra_z0, y_patim

ROT_IMPRESSAO = Rot(180, 0, 0)   # face de fixação na mesa, calota para cima
RAIO_CALOTA = 4.0


def fazer(p: Params, lado: int, calcos: int = 0) -> Part:
    h = barra_z0(p, calcos) - calcos * 0.5        # o calço ocupa o resto da altura
    x = p.lookahead
    y = lado * y_patim(p)
    r = RAIO_CALOTA
    corpo = caixa_lim(x - PATIM_X / 2, x + PATIM_X / 2, y - PATIM_Y / 2, y + PATIM_Y / 2, r, h)
    calota = Pos(x, y, r) * Sphere(r) & caixa_lim(x - r, x + r, y - PATIM_Y / 2,
                                                  y + PATIM_Y / 2, 0, r)
    patim = corpo + calota
    return patim - cil_z(p.tol.autoatarraxante_m2, p.tol.autoatarraxante_prof, x, y,
                         h - p.tol.autoatarraxante_prof)


def calco(p: Params) -> Part:
    """Calço de 0,5 mm entre patim e barra; um furo M2 passante."""
    return caixa(PATIM_X, PATIM_Y, 0.5) - cil_z(p.tol.furo_m2, 2, 0, 0, -1)


def paredes(p: Params) -> dict[str, float]:
    return {"patim_ao_furo": (PATIM_Y - p.tol.autoatarraxante_m2) / 2,
            "calota_sob_furo": barra_z0(p) - p.tol.autoatarraxante_prof}


def paredes_calco(p: Params) -> dict[str, float]:
    """Calços (patim e língua): 0,5 mm por definição — exceção declarada em printability."""
    return {"calco": 0.5}
