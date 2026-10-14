"""Montagem do LFR v2 e verificações da spec §5.8.

Origem no centro do eixo motriz, no chão; X para a frente, Y para a esquerda, Z para cima.
"""
from __future__ import annotations

from dataclasses import dataclass
from itertools import combinations

from build123d import CenterOf, Part, Pos, Rot

from params import Params, altura_eixo
from parts import base, components, fixadores, motor_clamp, sensor_bar, skid, wheel
from parts.common import caixa

TOL_VOLUME = 1e-3   # mm³ — abaixo disso é contato de face, não interferência
FOLGA_GIRANDO = 0.5          # mm — folga mínima entre roda (com O-ring) e peça fixa
CARGA_PATINS = (0.10, 0.20)  # fração do peso nos patins (spec §5.4)
ENVELOPE_MAX = (250.0, 250.0, 200.0)
TOL_SENSOR = 0.1             # mm em torno de sensor_altura
TOL_APOIO = 0.01             # mm — patins e O-rings tocam o chão
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
        **fixadores.envelopes_sob_a_base(p),
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


def _distancia_caixas(a, b) -> float:
    """Distância entre duas bounding boxes (0 se se sobrepõem): filtro rápido."""
    dx = max(a.min.X - b.max.X, b.min.X - a.max.X, 0.0)
    dy = max(a.min.Y - b.max.Y, b.min.Y - a.max.Y, 0.0)
    dz = max(a.min.Z - b.max.Z, b.min.Z - a.max.Z, 0.0)
    return (dx * dx + dy * dy + dz * dz) ** 0.5


def folgas_por_par(p: Params, limite: float = 2.0,
                   calcos: int = 0) -> list[tuple[str, str, float]]:
    """(peça A, peça B, distância em mm) de todo par a menos de `limite`, em ordem crescente.

    Distância 0 = contato (apoio, encosto) ou interferência — esta última aparece em
    interferencias(). Os pares roda–próprio O-ring ficam de fora.
    """
    todas = _todas(p, calcos)
    caixas = {n: s.bounding_box() for n, s in todas.items()}
    folgas = []
    for (na, a), (nb, b) in combinations(todas.items(), 2):
        if frozenset((na, nb)) in PARES_PERMITIDOS:
            continue
        if _distancia_caixas(caixas[na], caixas[nb]) >= limite:
            continue
        d = a.distance_to(b)
        if d < limite:
            folgas.append((na, nb, round(d, 3) + 0.0))
    return sorted(folgas, key=lambda t: (t[2], t[0], t[1]))


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


def vao_livre(p: Params, calcos: int = 0) -> tuple[float, str]:
    """(menor z, nome da peça) entre tudo que não pode chegar perto do chão.

    Entram as peças impressas e os envelopes dos componentes, inclusive as cabeças de
    parafuso e a abraçadeira que ficam sob a base. Ficam de fora as rodas e os patins (são
    o apoio) e o QTR-8A, cuja face tem verificação própria (sensor_altura).
    """
    pecas = {n: s for n, s in {**pecas_impressas(p, calcos), **componentes(p, calcos)}.items()
             if not n.startswith("patim") and not n.startswith("calços patim")
             and n != "QTR-8A"}
    return min(((s.bounding_box().min.Z, n) for n, s in pecas.items()), key=lambda t: t[0])


@dataclass(frozen=True)
class Verificacao:
    nome: str
    criterio: str
    valor: str
    ok: bool


def verificacoes(p: Params) -> list[Verificacao]:
    """Critérios de aceite da spec §5.8 que dependem das medidas do montador."""
    ruins = interferencias(p)
    folga = folga_minima_rodas(p)
    _, (x_cg, _, z_cg), _ = massa_e_cg(p)
    carga = x_cg / p.lookahead
    z_vao, peca_vao = vao_livre(p)
    z_sensor = componentes(p)["QTR-8A"].bounding_box().min.Z
    apoios = {n: s.bounding_box().min.Z for n, s in {**pecas_impressas(p), **rodas(p)}.items()
              if n.startswith("patim") or n.startswith("O-ring")}
    env = envelope(p)
    return [
        Verificacao("Interferência entre peças e envelopes",
                    f"volume de interseção ≤ {TOL_VOLUME:g} mm³ em todo par",
                    "nenhuma" if not ruins else
                    "; ".join(f"{a} × {b}: {v:.2f} mm³" for a, b, v in ruins),
                    not ruins),
        Verificacao("Folga mínima das rodas", f"≥ {FOLGA_GIRANDO:.1f} mm", f"{folga:.2f} mm",
                    folga >= FOLGA_GIRANDO),
        Verificacao("Carga nos patins",
                    f"{CARGA_PATINS[0]:.0%}–{CARGA_PATINS[1]:.0%} do peso", f"{carga:.1%}",
                    CARGA_PATINS[0] <= carga <= CARGA_PATINS[1]),
        Verificacao("Não empina (CG à frente do eixo)", "x_CG ≥ 0,3 × z_CG",
                    f"x_CG = {x_cg:.1f} mm, 0,3 × z_CG = {0.3 * z_cg:.1f} mm",
                    x_cg >= 0.3 * z_cg),
        Verificacao("Vão livre", f"≥ {p.vao_minimo:.1f} mm (exceto rodas, patins e QTR-8A)",
                    f"{z_vao:.2f} mm ({peca_vao})", z_vao >= p.vao_minimo - 1e-6),
        Verificacao("Face do sensor ao chão", f"{p.sensor_altura:.1f} ± {TOL_SENSOR:.1f} mm",
                    f"{z_sensor:.2f} mm", abs(z_sensor - p.sensor_altura) <= TOL_SENSOR),
        Verificacao("Patins e O-rings tocam o chão", f"z mín = 0 ± {TOL_APOIO} mm",
                    ", ".join(f"{n} {round(z, 3) + 0.0:.3f}" for n, z in apoios.items()),
                    all(abs(z) <= TOL_APOIO for z in apoios.values())),
        Verificacao("Envelope montado", "≤ {:.0f} × {:.0f} × {:.0f} mm".format(*ENVELOPE_MAX),
                    "{:.1f} × {:.1f} × {:.1f} mm".format(*env),
                    all(e <= m for e, m in zip(env, ENVELOPE_MAX))),
    ]
