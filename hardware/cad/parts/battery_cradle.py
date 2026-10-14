"""Berço do suporte 2×18650 (spec §5.4). Feição unida à base, impressa junto com ela.

Paredes baixas em volta do suporte, com folga deslizante; quatro colunas nos cantos, sobre as
paredes de ponta do suporte (onde ficam as molas), com uma aba que trava o suporte por cima.
Nada passa sobre as células: elas saem por cima sem soltar nada.
"""
from __future__ import annotations

from build123d import Part

from params import Params
from parts.common import caixa_lim

PAREDE_H = 5.0
COLUNA_X = 6.0
ABA = 1.5
ABA_T = 1.2
PONTA = 6.0      # espessura útil da parede de ponta do suporte (sem célula embaixo)


def _limites(p: Params):
    f = p.tol.deslizante
    x0 = p.suporte_x_traseira - f
    x1 = p.suporte_x_traseira + p.suporte_w + f
    y1 = p.suporte_l / 2 + f
    return x0, x1, y1


def feicao(p: Params) -> Part:
    """Piso (estende a placa da base sob o berço) + paredes e abas."""
    x0, x1, y1 = _limites(p)
    t = p.parede
    piso = caixa_lim(x0 - t, x1 + t, -y1 - t, y1 + t, p.base_z, p.base_topo)
    return piso + paredes_e_abas(p)


def paredes_e_abas(p: Params) -> Part:
    x0, x1, y1 = _limites(p)
    z0 = p.base_topo
    t = p.parede
    fora = caixa_lim(x0 - t, x1 + t, -y1 - t, y1 + t, z0, z0 + PAREDE_H)
    dentro = caixa_lim(x0, x1, -y1, y1, z0 - 1, z0 + PAREDE_H + 1)
    berco = fora - dentro
    z_aba = z0 + p.suporte_h + p.tol.deslizante
    for xc in (x0 + 4 + COLUNA_X / 2, x1 - 4 - COLUNA_X / 2):
        for s in (+1, -1):
            yp = s * (y1 + t / 2)
            coluna = caixa_lim(xc - COLUNA_X / 2, xc + COLUNA_X / 2,
                               min(yp - t / 2, yp + t / 2), max(yp - t / 2, yp + t / 2),
                               z0, z_aba + ABA_T)
            y_aba0 = s * y1
            y_aba1 = s * (y1 - ABA)
            aba = caixa_lim(xc - COLUNA_X / 2, xc + COLUNA_X / 2, min(y_aba0, y_aba1),
                            max(y_aba0, y_aba1), z_aba, z_aba + ABA_T)
            berco += coluna + aba
    return berco


def area_livre_topo(p: Params) -> Part:
    """Prisma acima das células (entre as paredes de ponta): tem que ficar vazio."""
    x0, x1, y1 = _limites(p)
    z = p.base_topo + p.suporte_h
    return caixa_lim(x0, x1, -(y1 - PONTA), y1 - PONTA, z, z + 40.0)


def paredes(p: Params) -> dict[str, float]:
    return {"parede": p.parede, "aba": ABA_T, "coluna": p.parede}
