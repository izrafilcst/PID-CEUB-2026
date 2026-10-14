#!/usr/bin/env python3
"""Verificação das pranchas: aritmética das cotas e rasterização.

Uso:
    python3 verify_drawings.py                 # todas as pranchas
    python3 verify_drawings.py ../hardware/drawings/comp/02-mcp3008.svg

Os PNGs saem em build/drawings/ para inspeção visual. A verificação
aritmética não substitui a inspeção — ela pega erro de número, não de
sobreposição de texto.
"""
from __future__ import annotations

import os
import sys

import cairosvg

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR_SVG = os.path.join(RAIZ, "hardware", "drawings")
DIR_PNG = os.path.join(RAIZ, "build", "drawings")


def checar_soma(total: float, parciais: list[float],
                tol: float = 0.01) -> list[str]:
    """Confere se as cotas parciais somam a cota total."""
    s = sum(parciais)
    if abs(s - total) > tol:
        return [f"soma das parciais {s:.3f} ≠ total {total:.3f} "
                f"(diferença {abs(s - total):.3f} mm)"]
    return []


def checar_dentro(contorno: tuple[float, float, float, float],
                  furos: list[tuple[float, float, float]]) -> list[str]:
    """Confere se cada furo cabe inteiro dentro do contorno.

    contorno: (x, y, largura, altura) — canto inferior esquerdo.
    furos: lista de (cx, cy, diametro).

    Furo tangente à borda também é reportado: sem material entre o furo e a
    aresta, a peça abre. A comparação é por isso não-estrita.
    """
    x, y, w, h = contorno
    erros: list[str] = []
    for cx, cy, d in furos:
        r = d / 2.0
        if cx - r <= x or cx + r >= x + w or cy - r <= y or cy + r >= y + h:
            erros.append(
                f"furo ⌀{d} em ({cx}, {cy}) estoura o contorno "
                f"({x}, {y}, {w}, {h})")
    return erros


def rasterizar(svg_path: str, png_path: str, dpi: int = 200) -> None:
    """Renderiza o SVG em PNG para inspeção visual."""
    os.makedirs(os.path.dirname(png_path), exist_ok=True)
    cairosvg.svg2png(url=svg_path, write_to=png_path, dpi=dpi)


def _todas() -> list[str]:
    saida: list[str] = []
    for sub in ("comp", "assy"):
        d = os.path.join(DIR_SVG, sub)
        if not os.path.isdir(d):
            continue
        for nome in sorted(os.listdir(d)):
            if nome.endswith(".svg"):
                saida.append(os.path.join(d, nome))
    return saida


def main(argv: list[str]) -> int:
    alvos = argv[1:] or _todas()
    if not alvos:
        print("nenhuma prancha encontrada", file=sys.stderr)
        return 1
    for svg in alvos:
        nome = os.path.splitext(os.path.basename(svg))[0]
        png = os.path.join(DIR_PNG, f"{nome}.png")
        rasterizar(svg, png)
        print(f"{nome}: {png}")
    print(f"\n{len(alvos)} prancha(s) rasterizada(s) em {DIR_PNG}")
    print("INSPECIONE cada PNG antes de considerar a prancha pronta.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
