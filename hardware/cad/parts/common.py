"""Primitivas posicionadas no sistema da montagem (mm; X frente, Y esquerda, Z cima)."""
from __future__ import annotations

from build123d import Align, Box, Cylinder, Part, Pos, Rot

MIN_Z = (Align.CENTER, Align.CENTER, Align.MIN)


def caixa(dx: float, dy: float, dz: float, x: float = 0.0, y: float = 0.0,
          z: float = 0.0) -> Part:
    """Caixa centrada em (x, y), com a face de baixo em z."""
    return Pos(x, y, z) * Box(dx, dy, dz, align=MIN_Z)


def caixa_lim(x0: float, x1: float, y0: float, y1: float, z0: float, z1: float) -> Part:
    """Caixa pelos limites."""
    return caixa(x1 - x0, y1 - y0, z1 - z0, (x0 + x1) / 2, (y0 + y1) / 2, z0)


def cil_z(d: float, h: float, x: float = 0.0, y: float = 0.0, z: float = 0.0) -> Part:
    """Cilindro vertical com a base em z."""
    return Pos(x, y, z) * Cylinder(d / 2, h, align=MIN_Z)


def cil_y(d: float, comp: float, x: float = 0.0, y0: float = 0.0, z: float = 0.0) -> Part:
    """Cilindro com eixo ao longo de +Y, de y0 a y0 + comp."""
    return Pos(x, y0, z) * Rot(-90, 0, 0) * Cylinder(d / 2, comp, align=MIN_Z)


def na_mesa(part: Part, rot: Rot) -> Part:
    """Gira para a orientação de impressão e apoia na mesa (z mín = 0), centrado em XY."""
    girada = rot * part
    bb = girada.bounding_box()
    return Pos(-(bb.min.X + bb.max.X) / 2, -(bb.min.Y + bb.max.Y) / 2, -bb.min.Z) * girada
