"""Folha A4 paisagem com moldura, legenda e quadro de proveniência.

Referências: NBR 10068 (leiaute e legenda), NBR 8403 (larguras de linha),
NBR 10067 (símbolo do 1º diedro), ISO 3098 (alturas de texto).
"""
from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from xml.sax.saxutils import escape

# ═══ Folha A4 paisagem (NBR 10068) ═══
SHEET_W = 297.0
SHEET_H = 210.0
MARGIN_LEFT = 25.0      # margem de arquivamento
MARGIN_OTHER = 10.0

# ═══ Larguras de linha em mm (NBR 8403) ═══
W_VISIBLE = 0.5
W_HIDDEN = 0.35
W_THIN = 0.25

# ═══ Alturas de texto em mm (ISO 3098) ═══
H_DIM = 2.5
H_LABEL = 3.5
H_TITLE = 5.0

FONT = "DejaVu Sans, Arial, Helvetica, sans-serif"

# ═══ Legenda ═══
TB_W = 110.0            # largura da legenda (< 178 mm, NBR 10068)
TB_H = 32.0
TB_SYMBOL_X = 84.0      # início da célula do símbolo de projeção

# ═══ Quadro de proveniência ═══
PB_W = 62.0
PB_H = 20.0


class Prov(Enum):
    """Nível de confiança de uma cota."""

    OFICIAL = "●"   # extraída de datasheet do fabricante
    TIPICO = "○"    # módulo genérico, valor de consenso entre fornecedores
    MEDIR = "△"     # varia por lote — medir a peça em mãos

    @property
    def descricao(self) -> str:
        return {
            Prov.OFICIAL: "cotas do datasheet do fabricante",
            Prov.TIPICO: "módulo genérico — valor típico",
            Prov.MEDIR: "varia por lote — MEDIR a peça",
        }[self]


def txt(x: float, y: float, s: str, h: float = H_DIM,
        anchor: str = "start", weight: str = "normal") -> str:
    """Texto posicionado em milímetros de papel."""
    return (
        f'<text x="{x:.3f}" y="{y:.3f}" font-family="{FONT}" '
        f'font-size="{h:.2f}" font-weight="{weight}" '
        f'text-anchor="{anchor}" fill="#000000">{escape(s)}</text>'
    )


def line(x1: float, y1: float, x2: float, y2: float,
         w: float = W_THIN, dash: str = "") -> str:
    """Segmento de reta em milímetros de papel."""
    d = f' stroke-dasharray="{dash}"' if dash else ""
    return (
        f'<line x1="{x1:.3f}" y1="{y1:.3f}" x2="{x2:.3f}" y2="{y2:.3f}" '
        f'stroke="#000000" stroke-width="{w}"{d}/>'
    )


def rect(x: float, y: float, w: float, h: float, lw: float = W_THIN) -> str:
    """Retângulo vazado em milímetros de papel."""
    return (
        f'<rect x="{x:.3f}" y="{y:.3f}" width="{w:.3f}" height="{h:.3f}" '
        f'fill="none" stroke="#000000" stroke-width="{lw}"/>'
    )


def _simbolo_primeiro_diedro(x: float, y: float) -> str:
    """Símbolo do 1º diedro (NBR 10067): tronco de cone em duas vistas."""
    partes = [
        # vista lateral do tronco de cone
        f'<path d="M {x:.2f} {y - 3:.2f} L {x + 8:.2f} {y - 4.5:.2f} '
        f'L {x + 8:.2f} {y + 4.5:.2f} L {x:.2f} {y + 3:.2f} Z" '
        f'fill="none" stroke="#000000" stroke-width="{W_THIN}"/>',
        # vista frontal: dois círculos concêntricos
        f'<circle cx="{x + 15:.2f}" cy="{y:.2f}" r="4.5" fill="none" '
        f'stroke="#000000" stroke-width="{W_THIN}"/>',
        f'<circle cx="{x + 15:.2f}" cy="{y:.2f}" r="3" fill="none" '
        f'stroke="#000000" stroke-width="{W_THIN}"/>',
    ]
    return "".join(partes)


@dataclass
class Sheet:
    """Folha A4 paisagem pronta para receber vistas."""

    title: str
    maker: str
    code: str
    scale: str
    sheet_id: str
    date: str
    author: str = "Rafael Costa"
    _body: list[str] = field(default_factory=list)

    def add(self, svg: str) -> None:
        """Acrescenta SVG bruto ao corpo da prancha."""
        self._body.append(svg)

    # ── moldura ────────────────────────────────────────────────────────
    def _frame(self) -> str:
        x = MARGIN_LEFT
        y = MARGIN_OTHER
        w = SHEET_W - MARGIN_LEFT - MARGIN_OTHER
        h = SHEET_H - 2 * MARGIN_OTHER
        return rect(x, y, w, h, W_VISIBLE)

    # ── legenda (canto inferior direito) ───────────────────────────────
    def _title_block(self) -> str:
        x = SHEET_W - MARGIN_OTHER - TB_W
        y = SHEET_H - MARGIN_OTHER - TB_H
        p = [rect(x, y, TB_W, TB_H, W_VISIBLE)]

        # divisórias horizontais
        for dy in (8.0, 16.0, 24.0):
            p.append(line(x, y + dy, x + TB_W, y + dy, W_THIN))
        # divisória vertical do bloco de escala/unidade/prancha
        p.append(line(x + 62, y + 8, x + 62, y + TB_H, W_THIN))
        # divisória vertical da célula do símbolo de projeção — sem ela o
        # símbolo cruza as divisórias horizontais das linhas de texto
        p.append(line(x + TB_SYMBOL_X, y + 8, x + TB_SYMBOL_X, y + TB_H, W_THIN))

        # Título longo em 5 mm estoura a largura da legenda; cai para 3,5 mm.
        h_titulo = H_TITLE if len(self.title) <= 26 else H_LABEL
        p.append(txt(x + 2, y + 5.5, self.title, h_titulo, weight="bold"))
        # Fabricante e código dividem 62 mm com a coluna da escala. Em 2,5 mm
        # cabem ~43 caracteres e em 2,0 mm ~54 antes de invadir a célula da
        # escala; passando disso, trunca — legenda invadida é ilegível.
        fabricante = f"{self.maker} — {self.code}"
        h_fab, limite = H_DIM, 43
        if len(fabricante) > limite:
            h_fab, limite = 2.0, 54
        if len(fabricante) > limite:
            fabricante = fabricante[:limite - 1] + "…"
        p.append(txt(x + 2, y + 13, fabricante, h_fab))
        p.append(txt(x + 2, y + 21, f"Autor: {self.author}", H_DIM))
        p.append(txt(x + 2, y + 29, f"Data: {self.date}", H_DIM))

        p.append(txt(x + 64, y + 13, f"Escala {self.scale}", H_DIM))
        p.append(txt(x + 64, y + 21, "Unidade: mm", H_DIM))
        p.append(txt(x + 64, y + 29, f"Prancha {self.sheet_id}", H_DIM))

        # símbolo centrado na sua própria célula
        cx = x + (TB_SYMBOL_X + TB_W) / 2.0
        p.append(_simbolo_primeiro_diedro(cx - 9.75, y + 18))
        p.append(txt(cx, y + 29, "1º diedro", 2.0, anchor="middle"))
        return "".join(p)

    # ── quadro de proveniência (canto superior direito) ────────────────
    def _prov_block(self) -> str:
        x = SHEET_W - MARGIN_OTHER - PB_W
        y = MARGIN_OTHER
        p = [rect(x, y, PB_W, PB_H, W_THIN)]
        p.append(txt(x + 2, y + 4.5, "PROVENIÊNCIA", H_DIM, weight="bold"))
        for i, nivel in enumerate(Prov):
            p.append(txt(x + 2, y + 9.5 + i * 3.4,
                         f"{nivel.value} {nivel.name}  {nivel.descricao}", 2.0))
        return "".join(p)

    def render(self) -> str:
        """Serializa a prancha completa."""
        return (
            f'<svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{SHEET_W:.0f}mm" height="{SHEET_H:.0f}mm" '
            f'viewBox="0 0 {SHEET_W:.0f} {SHEET_H:.0f}">'
            f'<rect x="0" y="0" width="{SHEET_W}" height="{SHEET_H}" '
            f'fill="#ffffff"/>'
            f"{self._frame()}"
            f"{''.join(self._body)}"
            f"{self._title_block()}"
            f"{self._prov_block()}"
            f"</svg>"
        )
