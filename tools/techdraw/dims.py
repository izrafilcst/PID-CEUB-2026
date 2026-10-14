"""Cotagem conforme NBR 10126.

Setas cheias, cota acima da linha de cota, leitura pela esquerda e por baixo.
Cada cota carrega o símbolo de proveniência do seu valor.
"""
from __future__ import annotations

import math

from .geom import View
from .sheet import FONT, H_DIM, Prov, W_THIN, line, txt

ARROW_L = 2.5      # comprimento da seta em mm de papel
ARROW_W = 0.8      # meia-largura da seta
EXT_GAP = 1.0      # folga entre a peça e o início da linha de extensão
EXT_OVER = 1.5     # quanto a linha de extensão passa da linha de cota
TXT_GAP = 1.0      # folga entre a linha de cota e o texto


def fmt_valor(v: float) -> str:
    """Formata a cota: inteiro sem decimal, fracionário com vírgula."""
    arred = round(v, 2)
    if abs(arred - round(arred)) < 1e-9:
        return str(int(round(arred)))
    return f"{arred:.2f}".replace(".", ",")


def _seta(x: float, y: float, ang: float) -> str:
    """Seta cheia apontando na direção `ang` (radianos, papel)."""
    dx, dy = math.cos(ang), math.sin(ang)
    nx, ny = -dy, dx
    p1 = (x, y)
    p2 = (x - ARROW_L * dx + ARROW_W * nx, y - ARROW_L * dy + ARROW_W * ny)
    p3 = (x - ARROW_L * dx - ARROW_W * nx, y - ARROW_L * dy - ARROW_W * ny)
    pts = " ".join(f"{a:.3f},{b:.3f}" for a, b in (p1, p2, p3))
    return f'<polygon points="{pts}" fill="#000000"/>'


def _rotulo(x: float, y: float, texto: str, prov: Prov,
            rotacao: float = 0.0) -> str:
    """Texto da cota com o símbolo de proveniência anexado."""
    s = f"{texto} {prov.value}"
    t = (f'<text x="{x:.3f}" y="{y:.3f}" font-family="{FONT}" '
         f'font-size="{H_DIM:.2f}" text-anchor="middle" fill="#000000"')
    if rotacao:
        t += f' transform="rotate({rotacao:.0f} {x:.3f} {y:.3f})"'
    return t + f">{s}</text>"


def dim_h(v: View, x1: float, x2: float, y: float, valor: float,
          prov: Prov, offset: float = 10.0) -> str:
    """Cota horizontal entre x1 e x2, medida a partir da altura y da peça.

    offset: distância em mm de PAPEL entre a peça e a linha de cota.
    Positivo desloca para baixo no papel (abaixo da peça).
    """
    px1, py = v.to_paper(x1, y)
    px2, _ = v.to_paper(x2, y)
    yc = py + offset

    partes = [
        # linhas de extensão
        line(px1, py + EXT_GAP, px1, yc + EXT_OVER, W_THIN),
        line(px2, py + EXT_GAP, px2, yc + EXT_OVER, W_THIN),
        # linha de cota
        line(px1, yc, px2, yc, W_THIN),
        # setas apontando para fora, contra as linhas de extensão
        _seta(px1, yc, math.pi),
        _seta(px2, yc, 0.0),
        _rotulo((px1 + px2) / 2.0, yc - TXT_GAP, fmt_valor(valor), prov),
    ]
    return "".join(partes)


def dim_v(v: View, y1: float, y2: float, x: float, valor: float,
          prov: Prov, offset: float = 10.0) -> str:
    """Cota vertical entre y1 e y2, medida a partir da abscissa x da peça.

    offset: distância em mm de PAPEL. Positivo desloca para a esquerda.
    """
    px, py1 = v.to_paper(x, y1)
    _, py2 = v.to_paper(x, y2)
    xc = px - offset

    partes = [
        line(px - EXT_GAP, py1, xc - EXT_OVER, py1, W_THIN),
        line(px - EXT_GAP, py2, xc - EXT_OVER, py2, W_THIN),
        line(xc, py1, xc, py2, W_THIN),
        _seta(xc, py1, math.pi / 2.0),
        _seta(xc, py2, -math.pi / 2.0),
        _rotulo(xc - TXT_GAP, (py1 + py2) / 2.0,
                fmt_valor(valor), prov, rotacao=-90.0),
    ]
    return "".join(partes)


def dim_diam(v: View, cx: float, cy: float, d: float, prov: Prov,
             angulo: float = 45.0, guia: float = 8.0) -> str:
    """Cota de diâmetro com linha-guia saindo do centro."""
    px, py = v.to_paper(cx, cy)
    a = math.radians(angulo)
    r = v.len_paper(d / 2.0)
    x0, y0 = px + r * math.cos(a), py - r * math.sin(a)
    x1, y1 = x0 + guia * math.cos(a), y0 - guia * math.sin(a)
    x2 = x1 + (4.0 if math.cos(a) >= 0 else -4.0)
    anchor = "start" if math.cos(a) >= 0 else "end"
    return "".join([
        line(x0, y0, x1, y1, W_THIN),
        line(x1, y1, x2, y1, W_THIN),
        _seta(x0, y0, a + math.pi),
        (f'<text x="{x2 + (1.0 if anchor == "start" else -1.0):.3f}" '
         f'y="{y1 - 0.8:.3f}" font-family="{FONT}" font-size="{H_DIM:.2f}" '
         f'text-anchor="{anchor}" fill="#000000">'
         f"⌀{fmt_valor(d)} {prov.value}</text>"),
    ])


def dim_rad(v: View, cx: float, cy: float, r: float, prov: Prov,
            angulo: float = 45.0, guia: float = 8.0) -> str:
    """Cota de raio com linha-guia saindo do centro."""
    px, py = v.to_paper(cx, cy)
    a = math.radians(angulo)
    rp = v.len_paper(r)
    x0, y0 = px + rp * math.cos(a), py - rp * math.sin(a)
    x1, y1 = x0 + guia * math.cos(a), y0 - guia * math.sin(a)
    x2 = x1 + (4.0 if math.cos(a) >= 0 else -4.0)
    anchor = "start" if math.cos(a) >= 0 else "end"
    return "".join([
        line(px, py, x1, y1, W_THIN),
        line(x1, y1, x2, y1, W_THIN),
        _seta(x0, y0, a + math.pi),
        (f'<text x="{x2 + (1.0 if anchor == "start" else -1.0):.3f}" '
         f'y="{y1 - 0.8:.3f}" font-family="{FONT}" font-size="{H_DIM:.2f}" '
         f'text-anchor="{anchor}" fill="#000000">'
         f"R{fmt_valor(r)} {prov.value}</text>"),
    ])
