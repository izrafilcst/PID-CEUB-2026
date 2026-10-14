"""Caminho do chicote do QTR-8A até o perfboard (A-WP2-05).

A barra de pinos do QTR (8×2 ou 11×1, 2,54 mm) é soldada por cima da placa; o conector fêmea
sai por uma janela na viga da barra (sensor_bar.janela_conector). Os fios voltam por cima da
língua, descem pelo rasgo na ponta de trás da língua (e do calço da língua), correm num canal
aberto no topo da base, por baixo do suporte 2×18650 e das paredes do berço (que têm uma
abertura sobre o canal), e sobem entre os conectores dos N20 até o perfboard.

Chicote de 11 fios AWG 30 (⌀ ≤ 0,6 mm) numa camada: cabe no canal de LARGURA × CANAL_PROF.
"""
from __future__ import annotations

from build123d import Part

from params import Params
from parts import battery_cradle
from parts.common import caixa_lim

RASGO_L = 4.0        # rasgo na ponta de trás da língua, ao longo de X
LARGURA = 7.0        # largura do rasgo e do canal (Y)
CANAL_PROF = 1.8     # profundidade do canal no topo da base: sobram 1,2 mm de placa
CANAL_X0 = 2.5       # o canal termina entre os conectores dos N20, onde o chicote sobe
FEIXE_W = 6.0        # envelope do feixe de fios
FEIXE_T = 1.5
SUBIDA_X = (3.5, 6.5)


def _x_rasgo(p: Params) -> tuple[float, float]:
    return p.barra_lingua_x, p.barra_lingua_x + RASGO_L


def rasgo_lingua(p: Params) -> Part:
    """Sólido a subtrair da língua da barra e do calço da língua (passa por toda a altura)."""
    x0, x1 = _x_rasgo(p)
    return caixa_lim(x0 - 1.0, x1, -LARGURA / 2, LARGURA / 2, -20.0, 100.0)


def canal_base(p: Params) -> Part:
    """Sólido a subtrair da base: canal no topo da placa e abertura nas paredes do berço."""
    _, x1 = _x_rasgo(p)
    return caixa_lim(CANAL_X0, x1 + 0.5, -LARGURA / 2, LARGURA / 2, p.base_topo - CANAL_PROF,
                     p.base_topo + battery_cradle.PAREDE_H + 1.0)


def envelope(p: Params, z_topo_barra: float) -> Part:
    """Feixe de fios: rasgo da língua → canal da base → subida até o perfboard."""
    x0, x1 = _x_rasgo(p)
    z0 = p.base_topo - CANAL_PROF + 0.1
    w = FEIXE_W / 2
    descida = caixa_lim(x0 + 0.5, x1 - 0.5, -w, w, z0, z_topo_barra)
    canal = caixa_lim(SUBIDA_X[0], x1 - 0.5, -w, w, z0, z0 + FEIXE_T)
    subida = caixa_lim(SUBIDA_X[0], SUBIDA_X[1], -w, w, z0, p.perf_z - 2.0)
    return descida + canal + subida
