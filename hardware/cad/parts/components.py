"""Envelopes das peças compradas, na posição de montagem (spec §5.2, §5.4)."""
from __future__ import annotations

from build123d import Part

from params import Params, altura_eixo
from parts.common import caixa, caixa_lim, cil_z

CALCO_T = 0.5  # mm por calço sob o patim


def motor(p: Params, lado: int) -> Part:
    """Corpo do N20 + conector traseiro. lado = +1 (esquerdo, y > 0) ou -1 (direito)."""
    zc = altura_eixo(p)
    y_face = lado * p.motor_face_y
    y_fundo = y_face - lado * p.n20_total_l
    y_conector = y_fundo - lado * p.n20_conector_l
    corpo = caixa_lim(-p.n20_largura / 2, p.n20_largura / 2, min(y_face, y_fundo),
                      max(y_face, y_fundo), zc - p.n20_altura / 2, zc + p.n20_altura / 2)
    conector = caixa_lim(-4.8, 4.8, min(y_fundo, y_conector), max(y_fundo, y_conector),
                         zc - 2.0, zc + 2.0)
    return corpo + conector


def suporte_bateria(p: Params) -> Part:
    x0 = p.suporte_x_traseira
    return caixa_lim(x0, x0 + p.suporte_w, -p.suporte_l / 2, p.suporte_l / 2,
                     p.base_topo, p.base_topo + p.suporte_h)


def qtr(p: Params, calcos: int = 0) -> Part:
    z0 = p.sensor_altura + calcos * CALCO_T
    return caixa(p.qtr_w, p.qtr_l, p.qtr_t, x=p.lookahead, z=z0)


def perfboard(p: Params) -> Part:
    """Perfboard + volume dos componentes por cima + 2 mm de terminais por baixo."""
    x1 = p.perf_x_frente
    x0 = x1 - p.perf_l
    placa = caixa_lim(x0, x1, -p.perf_w / 2, p.perf_w / 2, p.perf_z, p.perf_z + p.perf_t)
    cima = caixa_lim(x0 + 2, x1 - 2, -p.perf_w / 2 + 2, p.perf_w / 2 - 2,
                     p.perf_z + p.perf_t, p.perf_z + p.perf_t + p.eletronica_h)
    baixo = caixa_lim(x0 + 8, x1 - 8, -p.perf_w / 2 + 8, p.perf_w / 2 - 8,
                      p.perf_z - 2.0, p.perf_z)   # terminais longe das colunas de fixação
    return placa + cima + baixo


def massas(p: Params) -> list[tuple[str, float, Part]]:
    """Peças compradas com massa e sólido de referência (centro de massa = centróide)."""
    zc = altura_eixo(p)
    return [
        ("N20 esquerdo", p.n20_massa, motor(p, +1)),
        ("N20 direito", p.n20_massa, motor(p, -1)),
        ("2 células 18650", 2 * p.celula_massa, suporte_bateria(p)),
        ("Suporte 2×18650", p.suporte_massa, suporte_bateria(p)),
        ("QTR-8A", p.qtr_massa, qtr(p)),
        ("Eletrônica no perfboard", p.eletronica_massa, perfboard(p)),
        ("Fios e parafusos", p.fios_parafusos_massa, perfboard(p)),
        ("O-rings", 2 * p.oring_massa, cil_z(1, 1, 0, 0, zc)),
    ]
