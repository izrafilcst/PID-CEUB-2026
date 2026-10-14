"""Fixadores (spec §5.6): lista para o relatório de montagem e envelopes do que fica por
baixo da base (cabeças dos M3 da barra e abraçadeira do porta-fusível), que entram na
verificação de vão livre e de interferência.

Os 2 M3 que prendem a língua da barra sobem pela base até os insertos. A cabeça é chata
(ISO 10642, 90°) e assenta num escareado na face de baixo da base: fica rente, sem invadir
o vão livre. Cabeça de botão, panela ou Allen ficaria a 2,0–3,35 mm do chão (A-WP2-04).
"""
from __future__ import annotations

from build123d import Align, Cone, Part, Pos

from params import Params
from parts import electronics, sensor_bar
from parts.common import cil_z

M3_CHATA_DK = 6.72        # ISO 10642 M3: diâmetro máximo da cabeça
M3_CHATA_K = 1.86         # ISO 10642 M3: altura da cabeça (90°)
ESCAREADO_D = 7.0         # escareado de 90° na face de baixo da base (cabeça + 0,28)
COMERCIAIS_M2 = (4, 5, 6, 8, 10, 12, 14, 16)
COMERCIAIS_M3 = (6, 8, 10, 12, 14, 16)
MAX_CALCOS = 6            # 6 calços de 0,5 mm: sensor de 3,0 a 6,0 mm (spec §5.5)
ENGATE_MIN = 3.0          # mm de rosca útil no inserto ou no furo piloto
FOLGA_PONTA = 0.3         # mm entre a ponta do parafuso e o fundo do furo cego

_BASE = (Align.CENTER, Align.CENTER, Align.MIN)


def profundidade_escareado(p: Params) -> float:
    return (ESCAREADO_D - p.tol.furo_m3) / 2


def escareado_m3(p: Params, x: float, y: float) -> Part:
    """Sólido a subtrair da base: cone de 90° na face de baixo + 1 mm de sobra para fora."""
    h = profundidade_escareado(p)
    cone = Pos(x, y, p.base_z) * Cone(ESCAREADO_D / 2, p.tol.furo_m3 / 2, h, align=_BASE)
    return cone + cil_z(ESCAREADO_D, 1.0, x, y, p.base_z - 1.0)


def cabeca_m3_chata(p: Params, x: float, y: float) -> Part:
    """Envelope da cabeça ISO 10642 assentada no escareado (cones coaxiais de 90°)."""
    z0 = p.base_z + (ESCAREADO_D - M3_CHATA_DK) / 2
    return Pos(x, y, z0) * Cone(M3_CHATA_DK / 2, M3_CHATA_DK / 2 - M3_CHATA_K, M3_CHATA_K,
                                align=_BASE)


def envelopes_sob_a_base(p: Params) -> dict[str, Part]:
    lados = {-1: "D", +1: "E"}
    env = {f"cabeça M3 barra {lados[1 if y > 0 else -1]}": cabeca_m3_chata(p, x, y)
           for x, y in sensor_bar.furos_base(p)}
    env["abraçadeira do porta-fusível"] = electronics.abracadeira_porta_fusivel(p)
    return env


def _comercial(minimo: float, maximo: float, serie: tuple[int, ...]) -> int | None:
    """Maior comprimento comercial dentro da faixa (mais rosca sem bater no fundo)."""
    cabem = [c for c in serie if minimo <= c <= maximo]
    return max(cabem) if cabem else None


def _por_calcos(faixa, serie) -> str:
    """'× L (0–4 calços); × L2 (5–6 calços)' a partir de faixa(n) -> (mín, máx), com o
    menor número de comprimentos: em cada passo, o comprimento que serve para mais calços
    seguidos."""
    def serve(c, n):
        lo, hi = faixa(n)
        return lo <= c <= hi

    grupos: list[tuple[int, int, int | None]] = []
    n = 0
    while n <= MAX_CALCOS:
        melhor, fim = None, n
        for c in serie:
            if not serve(c, n):
                continue
            k = n
            while k + 1 <= MAX_CALCOS and serve(c, k + 1):
                k += 1
            if melhor is None or k > fim:
                melhor, fim = c, k
        grupos.append((n, fim, melhor))
        n = fim + 1
    partes = []
    for a, b, c in grupos:
        txt = f"× {c}" if c else "sem comprimento comercial"
        partes.append(f"{txt} ({a}–{b} calços)" if a != b else f"{txt} ({a} calço{'s' * (a != 1)})")
    return "; ".join(partes)


def lista(p: Params) -> list[tuple[int, str, str]]:
    """(quantidade, item, onde) — comprimentos conferidos contra os furos do modelo."""
    t = p.tol
    # M3 da barra: cabeça assentada em base_z + (D - dk)/2; L inclui a cabeça (ISO 10642)
    z_cabeca = p.base_z + (ESCAREADO_D - M3_CHATA_DK) / 2

    def m3_barra(n):
        z_lingua = p.base_topo + 0.5 * n
        return (z_lingua + ENGATE_MIN - z_cabeca,
                z_lingua + t.inserto_m3_prof - FOLGA_PONTA - z_cabeca)

    def m2_patim(n):
        passa = p.barra_t + 0.5 * n
        return passa + ENGATE_MIN, passa + t.autoatarraxante_prof - FOLGA_PONTA

    orelha = 3.0 + 0.3                    # orelha da abraçadeira + aperto
    m2_abr = _comercial(orelha + ENGATE_MIN, orelha + t.autoatarraxante_prof - FOLGA_PONTA,
                        COMERCIAIS_M2)
    m2_qtr = _comercial(p.qtr_t + ENGATE_MIN, p.qtr_t + t.autoatarraxante_prof - FOLGA_PONTA,
                        COMERCIAIS_M2)
    m3_perf = _comercial(p.perf_t + ENGATE_MIN, p.perf_t + t.inserto_m3_prof - FOLGA_PONTA,
                         COMERCIAIS_M3)
    trava = (p.roda_cubo_d - t.eixo_achatado) / 2      # do cubo até perto do achatado
    m2_trava = max(c for c in COMERCIAIS_M2 if c <= trava + 1.0)
    return [
        (2, "M3 cabeça chata ISO 10642 (sextavado interno), "
            + _por_calcos(m3_barra, COMERCIAIS_M3),
         "base → insertos da língua da barra, por baixo, no escareado de 90°"),
        (6, "inserto térmico M3 (OD 4,0 × 5,7 mm)", "4 colunas do perfboard + 2 na língua da barra"),
        (4, f"M3 × {m3_perf} cabeça cilíndrica ou panela", "perfboard → insertos das colunas"),
        (4, f"M2 × {m2_abr} autoatarraxante", "abraçadeiras do N20 → berço da base"),
        (2, "M2 autoatarraxante, " + _por_calcos(m2_patim, COMERCIAIS_M2),
         "patins, de cima para baixo pela barra (com os calços)"),
        (2, f"M2 × {m2_qtr} autoatarraxante cabeça baixa",
         "QTR-8A → postes da barra (a cabeça fica sob a placa do QTR)"),
        (2, f"M2 × {m2_trava} autoatarraxante (opcional)", "trava radial das rodas"),
        (1, "abraçadeira de nylon 2,5 mm (≤ 1,2 mm de espessura)",
         "porta-fusível, pelos rasgos e pelo rebaixo sob a base"),
    ]
