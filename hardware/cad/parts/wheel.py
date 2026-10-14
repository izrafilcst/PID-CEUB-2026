"""Roda impressa em PETG com canal para O-ring (spec §5.4).

Referencial da peça: eixo de rotação = Z, face externa em z = 0 (vai na mesa), cubo
crescendo para +Z (lado do motor). O furo em D tem o achatado voltado para +Y.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from functools import lru_cache

from build123d import Align, Axis, Box, Circle, Cylinder, Part, Plane, Polygon, Pos, Rot, revolve

from params import Params, carregar
from parts.common import MIN_Z

ROT_IMPRESSAO = Rot(0, 0, 0)          # face externa na mesa
FAIXA_OD = (38.0, 44.0)               # spec §5.4
ORINGS_METRICOS = [(30, 3), (31, 3), (32, 3), (33, 3), (34, 3), (35, 3), (36, 3),
                   (32, 2.5), (33, 2.5), (34, 2.5), (35, 2.5), (36, 2.5)]
CARGA = 0.8                            # rotação sob carga / sem carga
TAN_TETO = math.tan(math.radians(55))  # teto do canal a 55° da horizontal (35° de balanço)
Z_CANAL = 1.2                          # flanco de baixo do canal, acima da face externa


@dataclass(frozen=True)
class GeometriaORing:
    id: float
    cs: float
    fundo_d: float
    cs_estirada: float
    diametro_externo: float
    profundidade: float
    largura: float
    aro_d: float


def geometria_oring(id_mm: float, cs_mm: float, estiramento: float) -> GeometriaORing:
    fundo = id_mm * (1 + estiramento)
    cs_e = cs_mm * (1 - estiramento / 2)
    prof = 0.72 * cs_mm                       # 70–75 % da seção (spec §5.4)
    return GeometriaORing(id_mm, cs_mm, fundo, cs_e, fundo + 2 * cs_e, prof, 1.25 * cs_mm,
                          fundo + 2 * prof)


def escolher_oring(alvo_od: float = 41.0, estiramento: float = 0.03,
                   faixa: tuple[float, float] = FAIXA_OD) -> GeometriaORing:
    candidatos = [geometria_oring(i, c, estiramento) for i, c in ORINGS_METRICOS]
    validos = [g for g in candidatos if faixa[0] <= g.diametro_externo <= faixa[1]]
    return min(validos, key=lambda g: abs(g.diametro_externo - alvo_od))


def verificar_oring(p: Params) -> GeometriaORing:
    g = geometria_oring(p.oring_id, p.oring_cs, p.oring_estiramento)
    if not FAIXA_OD[0] <= g.diametro_externo <= FAIXA_OD[1]:
        raise ValueError(f"O-ring {p.oring_id} × {p.oring_cs} dá roda de "
                         f"{g.diametro_externo:.1f} mm, fora de {FAIXA_OD[0]:.0f}–"
                         f"{FAIXA_OD[1]:.0f} mm")
    return g


def velocidade_m_s(od_mm: float, rpm: float) -> float:
    return math.pi * od_mm / 1000.0 * rpm / 60.0


def rpm_sob_carga(p: Params) -> float:
    """Rotação de saída na tensão de referência do firmware (vref), com carga típica."""
    return p.n20_rpm_6v * (p.vref / 6.0) * CARGA


def aceleracao_m_s2(p: Params, massa_g: float, fracao_velocidade: float = 0.5) -> float:
    """Aceleração das duas rodas a uma fração da rotação livre (curva torque × rotação linear)."""
    torque_nm = p.n20_torque_stall_kgcm * 0.0980665 * (p.vref / 6.0)
    forca = 2 * torque_nm * (1 - fracao_velocidade) / (p.roda_od / 2000.0)
    return forca / (massa_g / 1000.0)


@lru_cache(maxsize=None)
def fazer_roda(p: Params | None = None) -> Part:
    p = p or carregar()
    g = verificar_oring(p)
    r_aro = g.aro_d / 2
    parede_aro = g.profundidade + 1.2                    # 1,2 mm de PETG sob o canal
    aro = Cylinder(r_aro, p.roda_aro_w, align=MIN_Z) - \
        Cylinder(r_aro - parede_aro, p.roda_aro_w, align=MIN_Z)
    disco = Cylinder(r_aro - parede_aro + 0.5, 2.0, align=MIN_Z)
    cubo = Cylinder(p.roda_cubo_d / 2, p.roda_cubo_l, align=MIN_Z)
    roda = aro + disco + cubo

    # Canal do O-ring: flanco de baixo horizontal, teto a 55° → imprime sem suporte
    z0 = Z_CANAL
    zt = z0 + g.largura - g.profundidade / 2
    r_in = r_aro - g.profundidade
    perfil = Plane.XZ * Polygon((r_in, z0), (r_aro + 1, z0),
                                (r_aro + 1, zt + (g.profundidade + 1) * TAN_TETO), (r_in, zt),
                                align=None)
    roda -= revolve(perfil, Axis.Z)

    # Furo em D passante (achatado voltado para +Y)
    r = p.tol.eixo_d / 2
    manter = Pos(0, -r, -1) * Box(2 * r + 1, p.tol.eixo_achatado, p.roda_cubo_l + 4,
                                  align=(Align.CENTER, Align.MIN, Align.MIN))
    furo_d = Pos(0, 0, -1) * Cylinder(r, p.roda_cubo_l + 2, align=MIN_Z) & manter
    roda -= furo_d

    # Furo radial para M2 autoatarraxante (trava opcional contra o achatado)
    trava = Pos(0, 0, p.roda_cubo_l - 3.0) * Rot(-90, 0, 0) * \
        Cylinder(p.tol.autoatarraxante_m2 / 2, p.roda_cubo_d, align=MIN_Z)
    return roda - trava


@lru_cache(maxsize=None)
def fazer_oring(p: Params | None = None) -> Part:
    """O-ring montado (toro), no mesmo referencial da roda."""
    p = p or carregar()
    g = verificar_oring(p)
    perfil = Plane.XZ * Pos(g.fundo_d / 2 + g.cs_estirada / 2, p.roda_aro_w / 2) * \
        Circle(g.cs_estirada / 2)
    return revolve(perfil, Axis.Z)


def paredes(p: Params | None = None) -> dict[str, float]:
    p = p or carregar()
    g = verificar_oring(p)
    topo_canal = Z_CANAL + g.largura - g.profundidade / 2 + g.profundidade * TAN_TETO
    return {
        "aro_sob_canal": 1.2,
        "disco": 2.0,
        "cubo_ao_furo": (p.roda_cubo_d - p.tol.eixo_d) / 2,
        "ressalto_aro": p.roda_aro_w - topo_canal,
        "flanco_externo": Z_CANAL,
    }
