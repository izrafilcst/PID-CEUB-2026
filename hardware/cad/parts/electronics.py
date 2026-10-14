"""Suportes da eletrônica (spec §5.4): colunas do perfboard com insertos M3, suporte da chave
liga/desliga na lateral e rasgos para abraçadeira do porta-fusível."""
from __future__ import annotations

from build123d import Part

from params import Params
from parts.common import caixa_lim, cil_z

COLUNA_D = 7.0


def furos_perfboard(p: Params) -> list[tuple[float, float]]:
    x1 = p.perf_x_frente - p.perf_furo_canto
    x0 = p.perf_x_frente - p.perf_l + p.perf_furo_canto
    y = p.perf_w / 2 - p.perf_furo_canto
    return [(x0, -y), (x0, y), (x1, -y), (x1, y)]


def colunas(p: Params) -> Part:
    h = p.perf_z - p.base_topo
    peca = None
    for x, y in furos_perfboard(p):
        c = cil_z(COLUNA_D, h, x, y, p.base_topo) - \
            cil_z(p.tol.inserto_m3_d, p.tol.inserto_m3_prof, x, y,
                  p.perf_z - p.tol.inserto_m3_prof)
        peca = c if peca is None else peca + c
    return peca


def suporte_chave(p: Params) -> Part:
    """Aba vertical na lateral esquerda, atrás do eixo, com janela para a chave deslizante."""
    x = p.perf_x_frente - p.perf_l / 2
    y0 = p.base_meia_largura - p.parede
    aba = caixa_lim(x - 10, x + 10, y0, p.base_meia_largura, p.base_topo, p.base_topo + 14)
    j = p.tol.deslizante / 2
    janela = caixa_lim(x - p.interruptor_l / 2 - j, x + p.interruptor_l / 2 + j, y0 - 1,
                       p.base_meia_largura + 1, p.base_topo + 6 - p.interruptor_h / 2 - j,
                       p.base_topo + 6 + p.interruptor_h / 2 + j)
    return aba - janela


REBAIXO_ABRACADEIRA = 1.6   # canal sob a placa entre os rasgos (abraçadeira ≤ 1,2 mm)
ABRACADEIRA_T = 1.2
ABRACADEIRA_W = 2.5


def _centro_rasgos(p: Params) -> tuple[float, float]:
    return p.perf_x_frente - p.perf_l / 2, -(p.base_meia_largura - 8)


def rasgos_abracadeira(p: Params) -> Part:
    """Dois rasgos 3,5 × 1,5 passantes para prender o porta-fusível com abraçadeira, ligados
    por um rebaixo na face de baixo: o trecho da abraçadeira que passa por baixo da placa
    fica dentro da placa, sem invadir o vão livre (A-WP2-04)."""
    x, y = _centro_rasgos(p)
    rasgos = (caixa_lim(x - 9, x - 7.5, y - 1.75, y + 1.75, p.base_z - 1, p.base_topo + 1) +
              caixa_lim(x + 7.5, x + 9, y - 1.75, y + 1.75, p.base_z - 1, p.base_topo + 1))
    rebaixo = caixa_lim(x - 9, x + 9, y - 1.75, y + 1.75, p.base_z - 1,
                        p.base_z + REBAIXO_ABRACADEIRA)
    return rasgos + rebaixo


def abracadeira_porta_fusivel(p: Params) -> Part:
    """Envelope do trecho da abraçadeira sob a placa, puxado contra o teto do rebaixo."""
    x, y = _centro_rasgos(p)
    z1 = p.base_z + REBAIXO_ABRACADEIRA
    return caixa_lim(x - 8.25, x + 8.25, y - ABRACADEIRA_W / 2, y + ABRACADEIRA_W / 2,
                     z1 - ABRACADEIRA_T, z1)
