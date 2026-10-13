"""Montagem do LFR v2 e verificações da spec §5.8.

Origem no centro do eixo motriz, no chão; X para a frente, Y para a esquerda, Z para cima.
"""
from __future__ import annotations

from itertools import combinations

from build123d import CenterOf, Part, Pos, Rot

from params import Params, altura_eixo
from parts import base, components, motor_clamp, sensor_bar, skid, wheel
from parts.common import caixa

TOL_VOLUME = 1e-3   # mm³ — abaixo disso é contato de face, não interferência
# Roda e o próprio O-ring se tocam por construção (o toro é só para folga e contato).
PARES_PERMITIDOS = {frozenset(("roda E", "O-ring E")), frozenset(("roda D", "O-ring D"))}


def pecas_impressas(p: Params, calcos: int = 0) -> dict[str, Part]:
    # base.fazer e wheel.fazer_roda usam lru_cache com Params congelado como chave
    pecas = {
        "base": base.fazer(p),
        "abraçadeira E": motor_clamp.fazer(p, +1),
        "abraçadeira D": motor_clamp.fazer(p, -1),
        "barra de sensores": sensor_bar.fazer(p, calcos),
        "patim E": skid.fazer(p, +1, calcos),
        "patim D": skid.fazer(p, -1, calcos),
    }
    if calcos:
        h = sensor_bar.barra_z0(p, 0)                 # topo do patim
        for lado, nome in ((+1, "calços patim E"), (-1, "calços patim D")):
            pecas[nome] = caixa(sensor_bar.PATIM_X, sensor_bar.PATIM_Y, 0.5 * calcos,
                                p.lookahead, lado * sensor_bar.y_patim(p), h)
        pecas["calços língua"] = Pos(0, 0, p.base_topo) * sensor_bar.calco_lingua(p, calcos)
    return pecas


def rodas(p: Params) -> dict[str, Part]:
    zc = altura_eixo(p)
    y = p.motor_face_y + p.folga_roda_motor + p.roda_cubo_l
    roda, oring = wheel.fazer_roda(p), wheel.fazer_oring(p)
    esq = Pos(0, y, zc) * Rot(90, 0, 0)     # +Z local (cubo) → -Y
    dir_ = Pos(0, -y, zc) * Rot(-90, 0, 0)  # +Z local (cubo) → +Y
    return {"roda E": esq * roda, "O-ring E": esq * oring,
            "roda D": dir_ * roda, "O-ring D": dir_ * oring}


def componentes(p: Params, calcos: int = 0) -> dict[str, Part]:
    return {
        "motor E": components.motor(p, +1),
        "motor D": components.motor(p, -1),
        "suporte 2×18650": components.suporte_bateria(p),
        "QTR-8A": components.qtr(p, calcos),
        "perfboard": components.perfboard(p),
    }


def _todas(p: Params, calcos: int) -> dict[str, Part]:
    return {**pecas_impressas(p, calcos), **rodas(p), **componentes(p, calcos)}


def interferencias(p: Params, calcos: int = 0) -> list[tuple[str, str, float]]:
    """Pares com volume de interseção > TOL_VOLUME (mm³)."""
    todas = _todas(p, calcos)
    ruins = []
    for (na, a), (nb, b) in combinations(todas.items(), 2):
        if frozenset((na, nb)) in PARES_PERMITIDOS:
            continue
        v = (a & b).volume
        if v > TOL_VOLUME:
            ruins.append((na, nb, round(v, 3)))
    return ruins


def folga_minima_rodas(p: Params) -> float:
    """Menor distância entre uma roda (ou O-ring) e qualquer peça fixa."""
    giram = rodas(p)
    fixas = {**pecas_impressas(p), **componentes(p)}
    return min(r.distance_to(f) for r in giram.values() for f in fixas.values())


def massa_e_cg(p: Params):
    """(massa total em g, CG (x, y, z) em mm, [(item, g)])."""
    itens: list[tuple[str, float, tuple[float, float, float]]] = []
    for nome, peca in {**pecas_impressas(p), **rodas(p)}.items():
        if nome.startswith("O-ring"):
            continue  # components.massas já inclui os O-rings
        m = peca.volume * p.petg_densidade * p.preenchimento
        c = peca.center(CenterOf.MASS)
        itens.append((nome, m, (c.X, c.Y, c.Z)))
    for nome, m, solido in components.massas(p):
        c = solido.center(CenterOf.MASS)
        itens.append((nome, m, (c.X, c.Y, c.Z)))
    total = sum(m for _, m, _ in itens)
    cg = tuple(sum(m * pos[i] for _, m, pos in itens) / total for i in range(3))
    return total, cg, [(n, round(m, 1)) for n, m, _ in itens]


def carga_patins(p: Params) -> float:
    """Fração do peso nos patins: momento em torno do eixo motriz (x = 0)."""
    _, (x_cg, _, _), _ = massa_e_cg(p)
    return x_cg / p.lookahead


def envelope(p: Params) -> tuple[float, float, float]:
    tudo = list(_todas(p, 0).values())
    xs = [b.bounding_box() for b in tudo]
    return (max(b.max.X for b in xs) - min(b.min.X for b in xs),
            max(b.max.Y for b in xs) - min(b.min.Y for b in xs),
            max(b.max.Z for b in xs) - min(b.min.Z for b in xs))
