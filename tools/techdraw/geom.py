"""Transformação de coordenadas e primitivas geométricas.

Toda função recebe coordenadas DA PEÇA em milímetros reais e converte
para milímetros de papel aplicando escala e inversão do eixo Y.
"""
from __future__ import annotations

from dataclasses import dataclass

from .sheet import W_HIDDEN, W_THIN, W_VISIBLE, line

DASH_HIDDEN = "2,1"        # aresta oculta
DASH_CENTER = "6,1.2,1,1.2"  # traço-ponto (linha de centro)
CENTER_OVERSHOOT = 1.5     # quanto a linha de centro ultrapassa o contorno


@dataclass
class View:
    """Vista com origem e escala próprias dentro da folha.

    origin_x, origin_y: posição em mm de papel do ponto (0,0) da peça.
    scale: mm_papel / mm_peça. 2.0 para 2:1, 0.5 para 1:2.
    """

    origin_x: float
    origin_y: float
    scale: float

    def to_paper(self, x: float, y: float) -> tuple[float, float]:
        """Converte um ponto da peça para milímetros de papel."""
        return (self.origin_x + x * self.scale,
                self.origin_y - y * self.scale)

    def len_paper(self, v: float) -> float:
        """Converte um comprimento da peça para milímetros de papel."""
        return v * self.scale

    # ── primitivas ─────────────────────────────────────────────────────
    def polyline(self, pts: list[tuple[float, float]],
                 w: float = W_VISIBLE, close: bool = False,
                 dash: str = "") -> str:
        """Polilinha ligando pontos da peça."""
        if not pts:
            return ""
        px, py = self.to_paper(*pts[0])
        d = [f"M {px:.3f} {py:.3f}"]
        for x, y in pts[1:]:
            px, py = self.to_paper(x, y)
            d.append(f"L {px:.3f} {py:.3f}")
        if close:
            d.append("Z")
        da = f' stroke-dasharray="{dash}"' if dash else ""
        return (f'<path d="{" ".join(d)}" fill="none" stroke="#000000" '
                f'stroke-width="{w}"{da}/>')

    def rect_part(self, x: float, y: float, w: float, h: float,
                  lw: float = W_VISIBLE) -> str:
        """Retângulo da peça, ancorado pelo canto inferior esquerdo."""
        px, py = self.to_paper(x, y + h)   # canto superior no papel
        return (f'<rect x="{px:.3f}" y="{py:.3f}" '
                f'width="{self.len_paper(w):.3f}" '
                f'height="{self.len_paper(h):.3f}" '
                f'fill="none" stroke="#000000" stroke-width="{lw}"/>')

    def circle_part(self, cx: float, cy: float, d: float,
                    lw: float = W_VISIBLE) -> str:
        """Círculo da peça, informado pelo DIÂMETRO."""
        px, py = self.to_paper(cx, cy)
        return (f'<circle cx="{px:.3f}" cy="{py:.3f}" '
                f'r="{self.len_paper(d / 2.0):.3f}" '
                f'fill="none" stroke="#000000" stroke-width="{lw}"/>')

    def hidden(self, pts: list[tuple[float, float]]) -> str:
        """Aresta oculta: tracejada, largura média."""
        return self.polyline(pts, w=W_HIDDEN, dash=DASH_HIDDEN)

    def centerline_h(self, cx: float, cy: float, comprimento: float) -> str:
        """Linha de centro horizontal, ultrapassando o contorno."""
        meia = comprimento / 2.0 + CENTER_OVERSHOOT / self.scale
        x1, y1 = self.to_paper(cx - meia, cy)
        x2, y2 = self.to_paper(cx + meia, cy)
        return line(x1, y1, x2, y2, W_THIN, DASH_CENTER)

    def centerline_v(self, cx: float, cy: float, comprimento: float) -> str:
        """Linha de centro vertical, ultrapassando o contorno."""
        meia = comprimento / 2.0 + CENTER_OVERSHOOT / self.scale
        x1, y1 = self.to_paper(cx, cy - meia)
        x2, y2 = self.to_paper(cx, cy + meia)
        return line(x1, y1, x2, y2, W_THIN, DASH_CENTER)

    def cross(self, cx: float, cy: float, tamanho: float) -> str:
        """Cruz de centro de furo (duas linhas de centro curtas)."""
        return (self.centerline_h(cx, cy, tamanho)
                + self.centerline_v(cx, cy, tamanho))
