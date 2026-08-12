"""Anotações, balões, lista de peças e sólidos isométricos simplificados."""
from __future__ import annotations

import math
from dataclasses import dataclass

from .geom import View
from .sheet import FONT, H_DIM, H_LABEL, W_THIN, W_VISIBLE, line, rect, txt

ISO_COS = math.cos(math.radians(30.0))
ISO_SIN = math.sin(math.radians(30.0))

BALLOON_R = 3.5
ROW_H = 6.0
COL_W = (14.0, 12.0, 62.0, 40.0, 18.0)
HEADERS = ("ITEM", "QTD", "DENOMINAÇÃO", "REFERÊNCIA", "PRANCHA")


def note(v: View, x: float, y: float, texto: str,
         dx: float, dy: float) -> str:
    """Nota com linha-guia partindo do ponto (x, y) da peça.

    dx, dy: deslocamento em mm de PAPEL até o início do texto.
    """
    px, py = v.to_paper(x, y)
    ex, ey = px + dx, py + dy
    tx = ex + (2.0 if dx >= 0 else -2.0)
    anchor = "start" if dx >= 0 else "end"
    return "".join([
        line(px, py, ex, ey, W_THIN),
        line(ex, ey, ex + (4.0 if dx >= 0 else -4.0), ey, W_THIN),
        (f'<text x="{tx + (4.0 if dx >= 0 else -4.0):.3f}" y="{ey - 0.8:.3f}" '
         f'font-family="{FONT}" font-size="{H_DIM:.2f}" '
         f'text-anchor="{anchor}" fill="#000000">{texto}</text>'),
    ])


def balloon(v: View, x: float, y: float, numero: int,
            dx: float, dy: float) -> str:
    """Balão numerado ligado a um componente na vista explodida."""
    px, py = v.to_paper(x, y)
    cx, cy = px + dx, py + dy
    return "".join([
        line(px, py, cx, cy, W_THIN),
        (f'<circle cx="{cx:.3f}" cy="{cy:.3f}" r="{BALLOON_R}" '
         f'fill="#ffffff" stroke="#000000" stroke-width="{W_THIN}"/>'),
        (f'<text x="{cx:.3f}" y="{cy + 1.2:.3f}" font-family="{FONT}" '
         f'font-size="{H_DIM:.2f}" text-anchor="middle" '
         f'fill="#000000">{numero}</text>'),
    ])


@dataclass(frozen=True)
class PartItem:
    """Linha da lista de peças."""

    num: int
    qtd: int
    denominacao: str
    referencia: str
    prancha: str


def parts_list(x: float, y: float, itens: list[PartItem]) -> str:
    """Lista de peças. (x, y) é o canto inferior esquerdo, em mm de papel.

    Convenção NBR 10068: o cabeçalho fica embaixo e os itens crescem
    para cima, item 1 imediatamente acima do cabeçalho.
    """
    largura = sum(COL_W)
    partes: list[str] = []

    def _linha(base_y: float, celulas: tuple[str, ...], negrito: bool) -> None:
        partes.append(rect(x, base_y - ROW_H, largura, ROW_H, W_THIN))
        cx = x
        for w, c in zip(COL_W, celulas):
            partes.append(
                f'<text x="{cx + 1.5:.3f}" y="{base_y - 1.8:.3f}" '
                f'font-family="{FONT}" font-size="{H_DIM:.2f}" '
                f'font-weight="{"bold" if negrito else "normal"}" '
                f'fill="#000000">{c}</text>')
            cx += w
            partes.append(line(cx, base_y - ROW_H, cx, base_y, W_THIN))

    _linha(y, HEADERS, negrito=True)
    for i, it in enumerate(itens):
        _linha(y - ROW_H * (i + 1),
               (str(it.num), str(it.qtd), it.denominacao,
                it.referencia, it.prancha),
               negrito=False)
    return "".join(partes)


def _iso(ix: float, iy: float, x: float, y: float, z: float) -> tuple[float, float]:
    """Projeta um ponto 3D da peça em milímetros de papel."""
    return (ix + (x - y) * ISO_COS,
            iy - (z + (x + y) * ISO_SIN))


def iso_box(ix: float, iy: float, l: float, w: float, h: float) -> str:
    """Prisma retangular em isométrica: face superior, frontal e lateral.

    (ix, iy) é a projeção do vértice (0,0,0) da peça, em mm de papel.
    l ao longo de X, w ao longo de Y, h ao longo de Z.
    """
    def p(x, y, z):
        a, b = _iso(ix, iy, x, y, z)
        return f"{a:.3f} {b:.3f}"

    faces = [
        # topo
        f"M {p(0,0,h)} L {p(l,0,h)} L {p(l,w,h)} L {p(0,w,h)} Z",
        # frontal (y=0)
        f"M {p(0,0,0)} L {p(l,0,0)} L {p(l,0,h)} L {p(0,0,h)} Z",
        # lateral (x=l)
        f"M {p(l,0,0)} L {p(l,w,0)} L {p(l,w,h)} L {p(l,0,h)} Z",
    ]
    return "".join(
        f'<path d="{d}" fill="#ffffff" stroke="#000000" '
        f'stroke-width="{W_VISIBLE}"/>' for d in faces)


def iso_cyl(ix: float, iy: float, d: float, h: float, eixo: str = "z") -> str:
    """Cilindro em isométrica, representado por elipses e geratrizes.

    eixo: "x", "y" ou "z" — direção do eixo do cilindro na peça.
    """
    if eixo not in ("x", "y", "z"):
        raise ValueError(f"eixo inválido: {eixo!r} — use 'x', 'y' ou 'z'")

    r = d / 2.0
    # Elipse isométrica de um círculo: semieixo maior r, menor r·sen(30°).
    rx, ry = r * ISO_COS, r * ISO_SIN
    if eixo == "z":
        x0, y0 = _iso(ix, iy, 0.0, 0.0, 0.0)
        x1, y1 = _iso(ix, iy, 0.0, 0.0, h)
        rot = 0.0
    elif eixo == "x":
        x0, y0 = _iso(ix, iy, 0.0, 0.0, 0.0)
        x1, y1 = _iso(ix, iy, h, 0.0, 0.0)
        rot = -30.0
    else:
        x0, y0 = _iso(ix, iy, 0.0, 0.0, 0.0)
        x1, y1 = _iso(ix, iy, 0.0, h, 0.0)
        rot = 30.0

    def elipse(cx: float, cy: float) -> str:
        return (f'<ellipse cx="{cx:.3f}" cy="{cy:.3f}" '
                f'rx="{rx:.3f}" ry="{ry:.3f}" '
                f'transform="rotate({rot:.0f} {cx:.3f} {cy:.3f})" '
                f'fill="#ffffff" stroke="#000000" '
                f'stroke-width="{W_VISIBLE}"/>')

    return "".join([
        elipse(x0, y0),
        elipse(x1, y1),
        line(x0 - rx, y0, x1 - rx, y1, W_VISIBLE),
        line(x0 + rx, y0, x1 + rx, y1, W_VISIBLE),
    ])
