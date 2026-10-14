"""Cupom de tolerância (spec §5.7). Ler no manual de montagem como preencher o
[tolerancias] do medidas.toml a partir dele.

Feições (FEICOES, na ordem de leitura): 7 furos ⌀2,8–3,4 (passo 0,1) na fileira de cima,
com 1 a 7 entalhes na borda de baixo identificando cada um; 3 furos em D e 3 furos de
inserto M3 na fileira do meio; 3 bolsões para a chave de teste de 10 mm (folga 0,2/0,3/0,4
por lado) e 3 rasgos de PCB (1,8/2,0/2,2 mm) na fileira de baixo. Sem texto, para não
depender de fonte instalada.
"""
from __future__ import annotations

from build123d import Align, Box, Cylinder, Part, Pos, Rot

from params import Params
from parts.common import MIN_Z, caixa, caixa_lim, cil_z

ROT_IMPRESSAO = Rot(0, 0, 0)
L, W, T = 70.0, 34.0, 3.0

FUROS = [2.8, 2.9, 3.0, 3.1, 3.2, 3.3, 3.4]
FUROS_D = [(3.00, 2.50), (3.05, 2.55), (3.10, 2.60)]
INSERTOS = [3.9, 4.0, 4.1]
BOLSOES = [0.2, 0.3, 0.4]
RASGOS = [1.8, 2.0, 2.2]

FEICOES = ([(f"furo {d:.1f}", d) for d in FUROS] +
           [(f"eixo D {d:.2f}/{a:.2f}", d) for d, a in FUROS_D] +
           [(f"inserto {d:.1f}", d) for d in INSERTOS] +
           [(f"bolsão +{f:.1f}", f) for f in BOLSOES] +
           [(f"rasgo PCB {w:.1f}", w) for w in RASGOS])


def _furo_d(d: float, achatado: float, x: float, y: float) -> Part:
    r = d / 2
    manter = Pos(x, y - r, -1) * Box(d + 1, achatado, T + 2,
                                     align=(Align.CENTER, Align.MIN, Align.MIN))
    return (Pos(x, y, -1) * Cylinder(r, T + 2, align=MIN_Z)) & manter


def _entalhes(n: int, x: float) -> Part:
    """n entalhes de 0,8 mm na borda de baixo (y = -W/2) para identificar a feição."""
    peca = None
    for i in range(n):
        e = caixa(0.6, 1.6, T + 2, x - (n - 1) * 0.6 + i * 1.2, -W / 2, -1)
        peca = e if peca is None else peca + e
    return peca


def fazer(p: Params) -> Part:
    c = caixa(L, W, T)
    x0 = -L / 2 + 5
    for i, d in enumerate(FUROS):
        x = x0 + i * 9.5
        c -= cil_z(d, T + 2, x, 9, -1)
        c -= _entalhes(i + 1, x)
    for i, (d, a) in enumerate(FUROS_D):
        c -= _furo_d(d, a, x0 + i * 9.5, 0)
    for i, d in enumerate(INSERTOS):
        c -= cil_z(d, T + 2, x0 + (i + 4) * 9.5, 0, -1)
    for i, f in enumerate(BOLSOES):
        s = 10.0 + 2 * f
        c -= caixa(s, s, T - 1.2, x0 + 4 + i * 14, -9, 1.2)
    for i, w in enumerate(RASGOS):
        c -= caixa_lim(L / 2 - 20 + i * 6 - w / 2, L / 2 - 20 + i * 6 + w / 2, -14, -4, 1.2,
                       T + 1)
    return c


def chave_teste() -> Part:
    """Quadrado de 10 × 10 × 2 mm, impresso junto, para testar os bolsões.

    O furo central de ⌀3 serve para empurrar a chave para fora de um bolsão justo com a
    ponta de uma chave de fenda (o bolsão tem fundo).
    """
    return caixa(10.0, 10.0, 2.0) - cil_z(3.0, 4.0, 0, 0, -1)
