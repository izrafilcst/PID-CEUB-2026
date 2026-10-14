"""Base principal (spec §5.4). Impressa com a face de baixo na mesa.

Composição: placa; abas sob os motores até a parede de batente; berço do motor (metade de
baixo da caixa de redução) com furos piloto M2; apoio do motor sob o corpo; parede de batente
com furo ⌀FURO_CUBO (o cubo da roda passa por dentro); berço da bateria; colunas do perfboard
com insertos M3; furos de passagem M3 da barra de sensores; suporte da chave; rasgos para
abraçadeira do porta-fusível.
"""
from __future__ import annotations

from functools import lru_cache

from build123d import Part, Plane, Rot, mirror

from params import Params, altura_eixo, carregar
from parts import battery_cradle, chicote, electronics, fixadores, motor_clamp, sensor_bar
from parts.common import caixa_lim, cil_y, cil_z

ROT_IMPRESSAO = Rot(0, 0, 0)
FURO_CUBO = 11.4   # passagem do cubo ⌀10 da roda pela parede de batente (0,7 mm de folga)


def _x_tras(p: Params) -> float:
    return p.perf_x_frente - p.perf_l - 4.0


def _x_frente(p: Params) -> float:
    return sensor_bar.furos_base(p)[0][0] + 6.0


def _motor_lado(p: Params, lado: int) -> Part:
    zc = altura_eixo(p)
    w = p.n20_largura / 2 + p.tol.n20_folga
    ox = motor_clamp.orelha_x(p) + motor_clamp.ORELHA_L / 2
    yf = p.motor_face_y
    y0 = yf - p.n20_caixa_l
    topo_berco = zc - motor_clamp.APERTO
    # Aba da placa sob o motor até a parede de batente
    aba = caixa_lim(-ox, ox, p.base_meia_largura - 1, yf + p.parede_batente, p.base_z,
                    p.base_topo)
    # Berço da caixa de redução (metade de baixo) com furos piloto dos M2
    berco = caixa_lim(-ox, ox, y0, yf, p.base_topo, topo_berco) - \
        caixa_lim(-w, w, y0 - 1, yf + 1, zc - p.n20_altura / 2 - p.tol.n20_folga, topo_berco + 1)
    for x, y in motor_clamp.furos_parafuso(p, +1):
        berco -= cil_z(p.tol.autoatarraxante_m2, p.tol.autoatarraxante_prof, x, y,
                       topo_berco - p.tol.autoatarraxante_prof)
    # Apoio sob o corpo do motor (perto do encoder)
    y_apoio = yf - p.n20_total_l + 6.0
    apoio = caixa_lim(-w, w, y_apoio - 3, y_apoio + 3, p.base_topo,
                      zc - p.n20_altura / 2 - p.tol.n20_folga)
    # Parede de batente com passagem do cubo
    batente = caixa_lim(-w - p.parede, w + p.parede, yf, yf + p.parede_batente, p.base_topo,
                        zc + p.n20_altura / 2) - \
        cil_y(FURO_CUBO, p.parede_batente + 2, 0, yf - 1, zc)
    lado_esq = aba + berco + apoio + batente
    return lado_esq if lado > 0 else mirror(lado_esq, about=Plane.XZ)


@lru_cache(maxsize=None)
def fazer(p: Params | None = None) -> Part:
    p = p or carregar()
    placa = caixa_lim(_x_tras(p), _x_frente(p), -p.base_meia_largura, p.base_meia_largura,
                      p.base_z, p.base_topo)
    b = placa + _motor_lado(p, +1) + _motor_lado(p, -1)
    b += battery_cradle.feicao(p)
    b += electronics.colunas(p)
    b += electronics.suporte_chave(p)
    b -= electronics.rasgos_abracadeira(p)
    for x, y in sensor_bar.furos_base(p):
        # M3 cabeça chata por baixo: furo de passagem + escareado de 90° (A-WP2-04)
        b -= cil_z(p.tol.furo_m3, p.base_t + 2, x, y, p.base_z - 1)
        b -= fixadores.escareado_m3(p, x, y)
    # Canal do chicote do QTR no topo da placa, por baixo do suporte (A-WP2-05)
    return b - chicote.canal_base(p)


def paredes(p: Params) -> dict[str, float]:
    return {"placa": p.base_t, "batente": p.parede_batente, "berco": p.parede,
            "coluna_ao_inserto": (electronics.COLUNA_D - p.tol.inserto_m3_d) / 2,
            "placa_sob_escareado": p.base_t - fixadores.profundidade_escareado(p),
            "placa_sob_rebaixo": p.base_t - electronics.REBAIXO_ABRACADEIRA,
            "placa_sob_canal": p.base_t - chicote.CANAL_PROF}
