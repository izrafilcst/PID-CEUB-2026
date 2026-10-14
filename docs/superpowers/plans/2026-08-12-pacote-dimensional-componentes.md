# Pacote Dimensional dos Componentes — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produzir 10 fichas dimensionais e 10 pranchas SVG cotadas dos componentes comprados, mais 2 pranchas de montagem, suficientes para modelar o robô em CAD sem consultar mais nada.

**Architecture:** Uma fonte única de dados (`hardware/components/specs.py`) guarda cotas com nível de proveniência e URL do datasheet. Uma biblioteca de primitivas de desenho técnico (`tools/techdraw.py`) emite SVG conforme as normas ABNT de desenho. Geradores curtos compõem as pranchas sobre essas primitivas. Um verificador confere a aritmética das cotas e renderiza cada prancha em PNG para inspeção visual.

**Tech Stack:** Python 3.12 (biblioteca padrão), `cairosvg` 2.9 para rasterizar, `unittest` para testes. Nenhuma dependência nova.

**Spec:** `docs/superpowers/specs/2026-08-12-documentacao-componentes-abnt-design.md`

## Global Constraints

- **Sem dependências novas.** Só stdlib do Python 3.12 mais `cairosvg` e `jinja2`, que já estão instalados. Não há `pytest`, `numpy`, `svgwrite`, `pyyaml` — use `unittest` da stdlib.
- **Folha A4 paisagem**: 297 × 210 mm. Margem esquerda 25 mm, demais 10 mm (NBR 10068).
- **Unidades**: toda geometria de peça em milímetros reais. A folha mapeia 1 unidade de usuário SVG = 1 mm de papel.
- **Larguras de linha (NBR 8403)**: contorno visível 0,5 mm · oculto 0,35 mm tracejado · linha de centro 0,25 mm traço-ponto · cota e chamada 0,25 mm.
- **Alturas de texto (ISO 3098)**: 2,5 mm para cotas · 3,5 mm para rótulos · 5,0 mm para títulos.
- **Escalas (NBR 8196)**: somente 2:1, 1:1 ou 1:2. Declarada na legenda.
- **Projeção (NBR 10067)**: 1º diedro, com o símbolo na legenda.
- **Cotagem (NBR 10126)**: setas cheias, cota acima da linha de cota, ⌀ para diâmetro, R para raio.
- **Fundo branco explícito** em todo SVG — a prancha não pode sumir em visualizador de tema escuro.
- **Monocromático**: preto sobre branco. Sem cor.
- **Toda cota carrega proveniência**: `OFICIAL` (●), `TIPICO` (○) ou `MEDIR` (△). Cota `OFICIAL` obriga URL de origem.
- **Texto de domínio em português.** Docstrings e nomes de API em português também, seguindo o resto do projeto.
- **Nenhuma cota inventada.** Se o datasheet não informa, é `TIPICO` (consenso entre fornecedores, com fonte) ou `MEDIR` (registra que não pode ser garantida).

---

## Estrutura de arquivos

| Arquivo | Responsabilidade |
|---------|------------------|
| `tools/techdraw/__init__.py` | Exporta a API pública da biblioteca |
| `tools/techdraw/sheet.py` | Folha A4, moldura, legenda, quadro de proveniência |
| `tools/techdraw/geom.py` | Transformação de coordenadas (View) e primitivas geométricas |
| `tools/techdraw/dims.py` | Primitivas de cotagem com símbolo de proveniência |
| `tools/techdraw/annot.py` | Notas, balões, lista de peças, sólidos isométricos |
| `tools/tests/test_sheet.py` | Testes da folha e legenda |
| `tools/tests/test_geom.py` | Testes de transformação e geometria |
| `tools/tests/test_dims.py` | Testes de cotagem |
| `tools/tests/test_annot.py` | Testes de anotação e isométrica |
| `tools/tests/test_specs.py` | Testes de integridade dos dados dimensionais |
| `hardware/components/specs.py` | Fonte única: cotas, proveniência, massas, notas |
| `tools/gen_fichas.py` | Gera as 10 fichas Markdown a partir de `specs.py` |
| `tools/gen_component_drawings.py` | Gera as 10 pranchas de componente |
| `tools/gen_assembly_drawings.py` | Gera A1 e A2 |
| `tools/verify_drawings.py` | Verificação aritmética + rasterização para inspeção |
| `docs/DIVERGENCIAS.md` | Registro das inconsistências do repositório |

`techdraw` é pacote e não módulo único porque a soma das primitivas passa de 600 linhas; separado por responsabilidade, cada arquivo cabe em contexto de uma vez.

---

### Task 1: Folha A4, moldura e legenda

**Files:**
- Create: `tools/techdraw/__init__.py`
- Create: `tools/techdraw/sheet.py`
- Test: `tools/tests/test_sheet.py`

**Interfaces:**
- Consumes: nada.
- Produces: `Prov` (enum), `Sheet` (dataclass com `add(svg: str)` e `render() -> str`), constantes `SHEET_W`, `SHEET_H`, `MARGIN_LEFT`, `MARGIN_OTHER`, `W_VISIBLE`, `W_HIDDEN`, `W_THIN`, `H_DIM`, `H_LABEL`, `H_TITLE`.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_sheet.py
import unittest
from techdraw import Sheet, Prov, SHEET_W, SHEET_H


class TestSheet(unittest.TestCase):
    def _folha(self):
        return Sheet(
            title="MCP3008",
            maker="Microchip",
            code="MCP3008-I/P",
            scale="2:1",
            sheet_id="02",
            date="2026-08-12",
        )

    def test_raiz_svg_em_milimetros_de_papel(self):
        svg = self._folha().render()
        self.assertIn('width="297mm"', svg)
        self.assertIn('height="210mm"', svg)
        self.assertIn('viewBox="0 0 297 210"', svg)

    def test_fundo_branco_explicito(self):
        # Sem isto a prancha desaparece em visualizador de tema escuro.
        svg = self._folha().render()
        self.assertIn(f'<rect x="0" y="0" width="{SHEET_W}" height="{SHEET_H}" fill="#ffffff"', svg)

    def test_legenda_traz_os_campos_da_nbr_10068(self):
        svg = self._folha().render()
        for campo in ("MCP3008", "Microchip", "MCP3008-I/P", "2:1", "mm",
                      "2026-08-12", "Rafael Costa", "02"):
            self.assertIn(campo, svg, f"legenda sem o campo {campo!r}")

    def test_quadro_de_proveniencia_lista_os_tres_niveis(self):
        svg = self._folha().render()
        for simbolo in (Prov.OFICIAL.value, Prov.TIPICO.value, Prov.MEDIR.value):
            self.assertIn(simbolo, svg)
        self.assertIn("PROVENIÊNCIA", svg)

    def test_corpo_adicionado_aparece_no_render(self):
        f = self._folha()
        f.add('<circle cx="100" cy="100" r="5"/>')
        self.assertIn('<circle cx="100" cy="100" r="5"/>', f.render())

    def test_texto_e_escapado(self):
        f = Sheet(title="A & B <teste>", maker="x", code="y",
                  scale="1:1", sheet_id="99", date="2026-08-12")
        svg = f.render()
        self.assertIn("A &amp; B &lt;teste&gt;", svg)
        self.assertNotIn("<teste>", svg)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_sheet -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'techdraw'`

- [ ] **Step 3: Implementar o mínimo**

```python
# tools/techdraw/__init__.py
"""Primitivas de desenho técnico em SVG conforme normas ABNT.

Unidades: toda geometria de peça em milímetros reais.
A folha mapeia 1 unidade de usuário SVG = 1 mm de papel.
"""
from .sheet import (
    Prov, Sheet,
    SHEET_W, SHEET_H, MARGIN_LEFT, MARGIN_OTHER,
    W_VISIBLE, W_HIDDEN, W_THIN,
    H_DIM, H_LABEL, H_TITLE,
)

__all__ = [
    "Prov", "Sheet",
    "SHEET_W", "SHEET_H", "MARGIN_LEFT", "MARGIN_OTHER",
    "W_VISIBLE", "W_HIDDEN", "W_THIN",
    "H_DIM", "H_LABEL", "H_TITLE",
]
```

```python
# tools/techdraw/sheet.py
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

        p.append(txt(x + 2, y + 5.5, self.title, H_TITLE, weight="bold"))
        p.append(txt(x + 2, y + 13, f"{self.maker} — {self.code}", H_DIM))
        p.append(txt(x + 2, y + 21, f"Autor: {self.author}", H_DIM))
        p.append(txt(x + 2, y + 29, f"Data: {self.date}", H_DIM))

        p.append(txt(x + 64, y + 13, f"Escala {self.scale}", H_DIM))
        p.append(txt(x + 64, y + 21, "Unidade: mm", H_DIM))
        p.append(txt(x + 64, y + 29, f"Prancha {self.sheet_id}", H_DIM))
        p.append(_simbolo_primeiro_diedro(x + 88, y + 20))
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
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_sheet -v`
Expected: PASS — 6 testes.

> Se `ModuleNotFoundError: techdraw` persistir, é porque `tools/` não está no
> `sys.path`. Rode sempre a partir de `tools/`, ou crie `tools/tests/__init__.py`
> vazio. Não instale nada nem crie `setup.py`.

- [ ] **Step 5: Inspecionar a folha vazia**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 -c "
import sys; sys.path.insert(0, '.')
from techdraw import Sheet
import cairosvg
s = Sheet(title='TESTE', maker='—', code='—', scale='1:1',
          sheet_id='00', date='2026-08-12')
cairosvg.svg2png(bytestring=s.render().encode(), write_to='/tmp/folha.png', dpi=200)
print('ok')
"
```

Abra `/tmp/folha.png` e confirme: moldura fechada, legenda no canto inferior direito com todos os campos legíveis, quadro de proveniência no canto superior direito, símbolo do 1º diedro visível, nada sobreposto, nada cortado pela borda. **Se algo estiver sobreposto, ajuste as coordenadas antes de commitar** — todas as pranchas herdam esta folha.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/techdraw/__init__.py tools/techdraw/sheet.py tools/tests/test_sheet.py
git commit -m "feat(techdraw): A4 sheet with NBR 10068 title block and provenance box"
```

---

### Task 2: Transformação de coordenadas e primitivas geométricas

**Files:**
- Create: `tools/techdraw/geom.py`
- Modify: `tools/techdraw/__init__.py`
- Test: `tools/tests/test_geom.py`

**Interfaces:**
- Consumes: de `sheet.py` — `line`, `W_VISIBLE`, `W_HIDDEN`, `W_THIN`.
- Produces: `View` com `to_paper(x_mm, y_mm) -> tuple[float, float]`, `polyline(pts, w)`, `rect_part(x, y, w, h, lw)`, `circle_part(cx, cy, d, lw)`, `hidden(pts)`, `centerline_h(cx, cy, comprimento)`, `centerline_v(cx, cy, comprimento)`, `cross(cx, cy, tamanho)`.

**Convenção crítica**: no SVG o eixo Y aponta para baixo; no desenho técnico aponta para cima. `View.to_paper` inverte Y. Toda primitiva de `geom.py` recebe coordenadas **da peça em mm reais** e converte internamente.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_geom.py
import unittest
from techdraw import View


class TestView(unittest.TestCase):
    def test_escala_1_1_translada_para_a_origem_da_vista(self):
        v = View(origin_x=50.0, origin_y=150.0, scale=1.0)
        self.assertEqual(v.to_paper(0.0, 0.0), (50.0, 150.0))
        self.assertEqual(v.to_paper(10.0, 0.0), (60.0, 150.0))

    def test_eixo_y_e_invertido(self):
        # Y da peça cresce para cima; Y do papel cresce para baixo.
        v = View(origin_x=50.0, origin_y=150.0, scale=1.0)
        self.assertEqual(v.to_paper(0.0, 20.0), (50.0, 130.0))

    def test_escala_2_1_amplia(self):
        v = View(origin_x=0.0, origin_y=100.0, scale=2.0)
        self.assertEqual(v.to_paper(10.0, 5.0), (20.0, 90.0))

    def test_escala_1_2_reduz(self):
        v = View(origin_x=0.0, origin_y=100.0, scale=0.5)
        self.assertEqual(v.to_paper(10.0, 20.0), (5.0, 90.0))

    def test_comprimento_no_papel_aplica_a_escala(self):
        self.assertEqual(View(0.0, 0.0, 2.0).len_paper(7.5), 15.0)


class TestPrimitivas(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=0.0, origin_y=100.0, scale=1.0)

    def test_retangulo_da_peca_usa_contorno_visivel(self):
        svg = self.v.rect_part(0.0, 0.0, 20.0, 10.0)
        self.assertIn('stroke-width="0.5"', svg)
        self.assertIn('x="0.000"', svg)
        # canto superior no papel = origin_y - altura
        self.assertIn('y="90.000"', svg)

    def test_circulo_recebe_diametro_e_desenha_raio(self):
        svg = self.v.circle_part(10.0, 10.0, d=6.0)
        self.assertIn('r="3.000"', svg)
        self.assertIn('cx="10.000"', svg)
        self.assertIn('cy="90.000"', svg)

    def test_aresta_oculta_e_tracejada_e_mais_fina(self):
        svg = self.v.hidden([(0.0, 0.0), (10.0, 0.0)])
        self.assertIn('stroke-width="0.35"', svg)
        self.assertIn("stroke-dasharray", svg)

    def test_linha_de_centro_e_traco_ponto_e_estreita(self):
        svg = self.v.centerline_h(10.0, 10.0, 20.0)
        self.assertIn('stroke-width="0.25"', svg)
        self.assertIn("stroke-dasharray", svg)
        # ultrapassa o contorno em ambos os lados
        self.assertIn('x1="-1.500"', svg)

    def test_polilinha_fecha_quando_pedido(self):
        svg = self.v.polyline([(0.0, 0.0), (10.0, 0.0), (10.0, 5.0)], close=True)
        self.assertTrue(svg.count("L") >= 2)
        self.assertIn(" Z", svg)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_geom -v`
Expected: FAIL com `ImportError: cannot import name 'View'`

- [ ] **Step 3: Implementar o mínimo**

```python
# tools/techdraw/geom.py
"""Transformação de coordenadas e primitivas geométricas.

Toda função recebe coordenadas DA PEÇA em milímetros reais e converte
para milímetros de papel aplicando escala e inversão do eixo Y.
"""
from __future__ import annotations

from dataclasses import dataclass

from .sheet import W_HIDDEN, W_THIN, W_VISIBLE

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
        return self.polyline([(cx - meia, cy), (cx + meia, cy)],
                             w=W_THIN, dash=DASH_CENTER)

    def centerline_v(self, cx: float, cy: float, comprimento: float) -> str:
        """Linha de centro vertical, ultrapassando o contorno."""
        meia = comprimento / 2.0 + CENTER_OVERSHOOT / self.scale
        return self.polyline([(cx, cy - meia), (cx, cy + meia)],
                             w=W_THIN, dash=DASH_CENTER)

    def cross(self, cx: float, cy: float, tamanho: float) -> str:
        """Cruz de centro de furo (duas linhas de centro curtas)."""
        return (self.centerline_h(cx, cy, tamanho)
                + self.centerline_v(cx, cy, tamanho))
```

Acrescente a `tools/techdraw/__init__.py`:

```python
from .geom import View, DASH_HIDDEN, DASH_CENTER

__all__ += ["View", "DASH_HIDDEN", "DASH_CENTER"]
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_geom -v`
Expected: PASS — 10 testes.

- [ ] **Step 5: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/techdraw/geom.py tools/techdraw/__init__.py tools/tests/test_geom.py
git commit -m "feat(techdraw): view transform with Y-flip and geometry primitives"
```

---

### Task 3: Primitivas de cotagem

**Files:**
- Create: `tools/techdraw/dims.py`
- Modify: `tools/techdraw/__init__.py`
- Test: `tools/tests/test_dims.py`

**Interfaces:**
- Consumes: `View` de `geom.py`; `Prov`, `txt`, `line`, `W_THIN`, `H_DIM` de `sheet.py`.
- Produces: funções livres que recebem `View` como primeiro argumento — `dim_h(view, x1, x2, y, valor, prov, offset)`, `dim_v(view, y1, y2, x, valor, prov, offset)`, `dim_diam(view, cx, cy, d, prov, angulo)`, `dim_rad(view, cx, cy, r, prov, angulo)`, `fmt_valor(v) -> str`.

**Regra de formatação**: inteiros sem casa decimal (`19`), fracionários com duas casas e vírgula decimal (`9,53`) — vírgula porque o documento é em português.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_dims.py
import unittest
from techdraw import Prov, View, dim_h, dim_v, dim_diam, dim_rad, fmt_valor


class TestFormatacao(unittest.TestCase):
    def test_inteiro_sem_casa_decimal(self):
        self.assertEqual(fmt_valor(19.0), "19")

    def test_fracionario_com_virgula_decimal(self):
        self.assertEqual(fmt_valor(9.525), "9,53")

    def test_arredondamento_que_vira_inteiro_perde_a_virgula(self):
        self.assertEqual(fmt_valor(12.001), "12")


class TestCotas(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=50.0, origin_y=150.0, scale=1.0)

    def test_cota_horizontal_traz_valor_e_simbolo_de_proveniencia(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL)
        self.assertIn("20", svg)
        self.assertIn(Prov.OFICIAL.value, svg)

    def test_cota_medir_usa_o_simbolo_de_medir(self):
        svg = dim_h(self.v, 0.0, 58.0, 0.0, 58.0, Prov.MEDIR)
        self.assertIn(Prov.MEDIR.value, svg)
        self.assertNotIn(Prov.OFICIAL.value, svg)

    def test_cota_usa_linha_estreita(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.TIPICO)
        self.assertIn('stroke-width="0.25"', svg)

    def test_cota_tem_duas_setas(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL)
        self.assertEqual(svg.count("<polygon"), 2)

    def test_offset_afasta_a_linha_de_cota_da_peca(self):
        perto = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL, offset=8.0)
        longe = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL, offset=16.0)
        self.assertNotEqual(perto, longe)

    def test_cota_vertical_gira_o_texto(self):
        svg = dim_v(self.v, 0.0, 10.0, 0.0, 10.0, Prov.OFICIAL)
        self.assertIn("rotate(-90", svg)

    def test_diametro_recebe_o_prefixo(self):
        svg = dim_diam(self.v, 10.0, 10.0, 3.4, Prov.OFICIAL)
        self.assertIn("⌀3,4", svg.replace("&#8960;", "⌀"))

    def test_raio_recebe_o_prefixo(self):
        svg = dim_rad(self.v, 10.0, 10.0, 2.5, Prov.TIPICO)
        self.assertIn("R2,5", svg)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_dims -v`
Expected: FAIL com `ImportError: cannot import name 'dim_h'`

- [ ] **Step 3: Implementar o mínimo**

```python
# tools/techdraw/dims.py
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
```

Acrescente a `tools/techdraw/__init__.py`:

```python
from .dims import dim_h, dim_v, dim_diam, dim_rad, fmt_valor

__all__ += ["dim_h", "dim_v", "dim_diam", "dim_rad", "fmt_valor"]
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_dims -v`
Expected: PASS — 11 testes.

- [ ] **Step 5: Inspecionar uma prancha de teste com cotas**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 -c "
import sys; sys.path.insert(0, '.')
import cairosvg
from techdraw import Sheet, View, Prov, dim_h, dim_v, dim_diam
s = Sheet(title='TESTE COTAS', maker='—', code='—', scale='2:1',
          sheet_id='00', date='2026-08-12')
v = View(origin_x=90.0, origin_y=120.0, scale=2.0)
s.add(v.rect_part(0, 0, 19.3, 7.62))
s.add(v.circle_part(4, 3.81, d=2.0)); s.add(v.cross(4, 3.81, 4))
s.add(dim_h(v, 0, 19.3, 0, 19.3, Prov.OFICIAL, offset=12))
s.add(dim_v(v, 0, 7.62, 0, 7.62, Prov.OFICIAL, offset=12))
s.add(dim_diam(v, 4, 3.81, 2.0, Prov.TIPICO))
cairosvg.svg2png(bytestring=s.render().encode(), write_to='/tmp/cotas.png', dpi=200)
print('ok')
"
```

Abra `/tmp/cotas.png`. Confirme: setas cheias apontando para as linhas de extensão, texto da cota acima da linha e centralizado, cota vertical legível de baixo para cima, símbolo de proveniência ao lado de cada valor, nada sobrepondo o retângulo da peça.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/techdraw/dims.py tools/techdraw/__init__.py tools/tests/test_dims.py
git commit -m "feat(techdraw): NBR 10126 dimensioning with provenance symbols"
```

---

### Task 4: Anotações, balões, lista de peças e sólidos isométricos

**Files:**
- Create: `tools/techdraw/annot.py`
- Modify: `tools/techdraw/__init__.py`
- Test: `tools/tests/test_annot.py`

**Interfaces:**
- Consumes: `View` de `geom.py`; `txt`, `line`, `rect`, `W_THIN`, `W_VISIBLE`, `H_DIM`, `H_LABEL` de `sheet.py`.
- Produces: `note(view, x, y, texto, dx, dy)`, `balloon(view, x, y, numero, dx, dy)`, `parts_list(x, y, itens)` onde `itens` é `list[PartItem]`, `PartItem(num, qtd, denominacao, referencia, prancha)`, `iso_box(ix, iy, l, w, h)`, `iso_cyl(ix, iy, d, h, eixo)`, `ISO_COS`, `ISO_SIN`.

**Projeção isométrica**: usada só em A2. Ponto 3D `(x, y, z)` da peça vai para papel por
`px = ix + (x - y) · cos(30°)` e `py = iy - (z + (x + y) · sen(30°))`.
Escala isométrica não é aplicada (isometria de desenho, não de projeção verdadeira).

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_annot.py
import unittest
from techdraw import (PartItem, View, balloon, iso_box, iso_cyl, note,
                      parts_list)


class TestAnotacoes(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=50.0, origin_y=150.0, scale=1.0)

    def test_nota_tem_linha_guia_e_texto(self):
        svg = note(self.v, 10.0, 10.0, "furo M3 passante", dx=15.0, dy=-10.0)
        self.assertIn("furo M3 passante", svg)
        self.assertIn("<line", svg)

    def test_balao_e_circulo_com_numero(self):
        svg = balloon(self.v, 10.0, 10.0, 7, dx=12.0, dy=-8.0)
        self.assertIn("<circle", svg)
        self.assertIn(">7<", svg)


class TestListaDePecas(unittest.TestCase):
    def test_lista_traz_cabecalho_e_uma_linha_por_item(self):
        itens = [
            PartItem(1, 1, "Chassi", "PETG impresso", "—"),
            PartItem(2, 2, "Motor N20 c/ encoder", "Pololu", "04"),
        ]
        svg = parts_list(30.0, 40.0, itens)
        for c in ("ITEM", "QTD", "DENOMINAÇÃO", "REFERÊNCIA", "PRANCHA"):
            self.assertIn(c, svg)
        self.assertIn("Chassi", svg)
        self.assertIn("Motor N20 c/ encoder", svg)

    def test_lista_numera_de_baixo_para_cima(self):
        # Convenção NBR 10068: item 1 na linha imediatamente acima do cabeçalho.
        itens = [PartItem(1, 1, "A", "—", "—"), PartItem(2, 1, "B", "—", "—")]
        svg = parts_list(0.0, 100.0, itens)
        y_a = svg.index(">A<")
        y_b = svg.index(">B<")
        self.assertLess(y_a, y_b)


class TestIsometrica(unittest.TestCase):
    def test_caixa_isometrica_desenha_tres_faces(self):
        svg = iso_box(100.0, 100.0, l=20.0, w=10.0, h=5.0)
        self.assertEqual(svg.count("<path"), 3)

    def test_cilindro_isometrico_usa_elipses(self):
        svg = iso_cyl(100.0, 100.0, d=10.0, h=20.0, eixo="z")
        self.assertIn("<ellipse", svg)

    def test_cilindro_rejeita_eixo_invalido(self):
        with self.assertRaises(ValueError):
            iso_cyl(0.0, 0.0, d=10.0, h=20.0, eixo="q")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_annot -v`
Expected: FAIL com `ImportError: cannot import name 'PartItem'`

- [ ] **Step 3: Implementar o mínimo**

```python
# tools/techdraw/annot.py
"""Anotações, balões, lista de peças e sólidos isométricos simplificados."""
from __future__ import annotations

import math
from dataclasses import dataclass

from .geom import View
from .sheet import (FONT, H_DIM, H_LABEL, W_THIN, W_VISIBLE, line, rect, txt)

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
```

Acrescente a `tools/techdraw/__init__.py`:

```python
from .annot import (ISO_COS, ISO_SIN, PartItem, balloon, iso_box, iso_cyl,
                    note, parts_list)

__all__ += ["ISO_COS", "ISO_SIN", "PartItem", "balloon", "iso_box",
            "iso_cyl", "note", "parts_list"]
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_annot -v`
Expected: PASS — 7 testes.

- [ ] **Step 5: Rodar a suíte inteira**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest discover -s tests -v`
Expected: PASS — 34 testes.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/techdraw/annot.py tools/techdraw/__init__.py tools/tests/test_annot.py
git commit -m "feat(techdraw): notes, balloons, parts list and isometric solids"
```

---

### Task 5: Verificador de pranchas

**Files:**
- Create: `tools/verify_drawings.py`
- Test: `tools/tests/test_verify.py`

**Interfaces:**
- Consumes: `cairosvg`.
- Produces: `checar_soma(total, parciais, tol=0.01) -> list[str]`, `checar_dentro(contorno, furos) -> list[str]`, `rasterizar(svg_path, png_path, dpi=200) -> None`, `main(argv) -> int`.

Este verificador não valida estética — isso é inspeção humana. Ele pega erro
aritmético e produz o PNG para a inspeção.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_verify.py
import os
import tempfile
import unittest

from verify_drawings import checar_dentro, checar_soma, rasterizar


class TestChecagemAritmetica(unittest.TestCase):
    def test_soma_correta_nao_reporta(self):
        self.assertEqual(checar_soma(74.93, [10.0, 54.93, 10.0]), [])

    def test_soma_incorreta_reporta_a_diferenca(self):
        erros = checar_soma(75.0, [10.0, 50.0, 10.0])
        self.assertEqual(len(erros), 1)
        self.assertIn("5", erros[0])

    def test_tolerancia_absorve_arredondamento(self):
        self.assertEqual(checar_soma(9.525, [9.53], tol=0.01), [])


class TestFurosNoContorno(unittest.TestCase):
    def test_furo_interno_passa(self):
        self.assertEqual(
            checar_dentro((0.0, 0.0, 20.0, 10.0), [(5.0, 5.0, 2.0)]), [])

    def test_furo_estourando_a_borda_reporta(self):
        erros = checar_dentro((0.0, 0.0, 20.0, 10.0), [(19.5, 5.0, 2.0)])
        self.assertEqual(len(erros), 1)
        self.assertIn("19.5", erros[0])

    def test_furo_tangente_a_borda_reporta(self):
        erros = checar_dentro((0.0, 0.0, 20.0, 10.0), [(1.0, 5.0, 2.0)])
        self.assertEqual(len(erros), 1)


class TestRasterizacao(unittest.TestCase):
    def test_gera_png_nao_vazio(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="297mm" '
               'height="210mm" viewBox="0 0 297 210">'
               '<rect x="0" y="0" width="297" height="210" fill="#ffffff"/>'
               '<circle cx="100" cy="100" r="20" fill="none" '
               'stroke="#000" stroke-width="0.5"/></svg>')
        with tempfile.TemporaryDirectory() as d:
            sp = os.path.join(d, "a.svg")
            pp = os.path.join(d, "a.png")
            with open(sp, "w", encoding="utf-8") as fh:
                fh.write(svg)
            rasterizar(sp, pp, dpi=100)
            self.assertTrue(os.path.exists(pp))
            self.assertGreater(os.path.getsize(pp), 1000)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_verify -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'verify_drawings'`

- [ ] **Step 3: Implementar o mínimo**

```python
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
    """
    x, y, w, h = contorno
    erros: list[str] = []
    for cx, cy, d in furos:
        r = d / 2.0
        if cx - r < x or cx + r > x + w or cy - r < y or cy + r > y + h:
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
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_verify -v`
Expected: PASS — 6 testes.

- [ ] **Step 5: Ignorar a pasta de build**

Acrescente `build/` a `.gitignore` se ainda não estiver lá:

```bash
cd /home/swampert/dev/PID-CEUB-2026
grep -qx "build/" .gitignore || echo "build/" >> .gitignore
```

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/verify_drawings.py tools/tests/test_verify.py .gitignore
git commit -m "feat(tools): drawing verifier with arithmetic checks and rasterization"
```

---

### Task 6: Dados dimensionais — esquema e componentes 01 a 05

**Files:**
- Create: `hardware/components/specs.py`
- Test: `tools/tests/test_specs.py`

**Interfaces:**
- Consumes: `Prov` de `techdraw`.
- Produces: `Medida(valor, prov, fonte, nota)`, `Componente(num, nome, fabricante, codigo, fornecedor, preco, url_datasheet, ref_abnt, massa_g, medidas: dict[str, Medida], eletrico: dict[str, str], notas_montagem: list[str])`, `COMPONENTES: dict[str, Componente]`.

**Regra inviolável**: nenhuma cota inventada. Uma `Medida` com `prov=Prov.OFICIAL`
**tem** que ter `fonte` não vazia identificando URL e a figura ou página.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_specs.py
import sys
import os
import unittest

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "hardware", "components"))

from specs import COMPONENTES  # noqa: E402
from techdraw import Prov  # noqa: E402


class TestIntegridade(unittest.TestCase):
    def test_toda_medida_oficial_declara_a_fonte(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                if m.prov is Prov.OFICIAL:
                    self.assertTrue(
                        m.fonte.strip(),
                        f"{chave}.{nome} é OFICIAL mas não tem fonte")

    def test_toda_medida_oficial_aponta_para_uma_url(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                if m.prov is Prov.OFICIAL:
                    self.assertIn(
                        "http", m.fonte,
                        f"{chave}.{nome}: fonte sem URL — {m.fonte!r}")

    def test_nenhuma_medida_com_valor_nao_positivo(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                self.assertGreater(m.valor, 0.0, f"{chave}.{nome}")

    def test_todo_componente_tem_numero_unico_de_dois_digitos(self):
        nums = [c.num for c in COMPONENTES.values()]
        self.assertEqual(len(nums), len(set(nums)), "números repetidos")
        for n in nums:
            self.assertRegex(n, r"^\d{2}$")

    def test_componentes_01_a_05_presentes(self):
        for chave in ("esp32", "mcp3008", "qtr8a", "n20", "tb6612"):
            self.assertIn(chave, COMPONENTES)

    def test_qtr8a_bate_com_o_datasheet_pololu(self):
        # Conferido em https://www.pololu.com/product/960 nesta sessão.
        q = COMPONENTES["qtr8a"]
        self.assertAlmostEqual(q.medidas["comprimento"].valor, 74.93, places=2)
        self.assertAlmostEqual(q.medidas["largura"].valor, 12.70, places=2)
        self.assertAlmostEqual(q.medidas["espessura"].valor, 3.18, places=2)
        self.assertAlmostEqual(q.medidas["passo_sensores"].valor, 9.525, places=3)
        self.assertAlmostEqual(q.massa_g, 3.09, places=2)
        self.assertIs(q.medidas["passo_sensores"].prov, Prov.OFICIAL)

    def test_massa_declarada_para_o_calculo_de_cg(self):
        # A prancha A1 depende disto. Zero significa "ainda não levantado".
        for chave, c in COMPONENTES.items():
            self.assertGreater(c.massa_g, 0.0,
                               f"{chave} sem massa — A1 não fecha")


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_specs -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'specs'`

- [ ] **Step 3: Levantar os dados dos componentes 01 a 05**

Consulte, **nesta ordem**, e registre a URL e a figura/página de cada cota:

| # | Chave | Fonte a consultar |
|---|-------|-------------------|
| 01 | `esp32` | Datasheet do módulo ESP32-WROOM-32 (Espressif) para o módulo. A placa DevKit V1 é clone DOIT — as cotas da PCB e o passo entre fileiras de pinos são `TIPICO`, com fonte apontando para a documentação da placa que você usar |
| 02 | `mcp3008` | Datasheet Microchip do MCP3008, seção "Package Information", encapsulamento PDIP-16 |
| 03 | `qtr8a` | https://www.pololu.com/product/960 — já conferido: 74,93 × 12,70 × 3,18 mm, passo 9,525 mm, 3,09 g, 3,3–5,0 V, 100 mA |
| 04 | `n20` | Pololu — micro metal gearmotor com encoder. **Confirme o código real** (a BOM cita "3081", que precisa ser verificado). Registre corpo, caixa de redução, eixo em D, PCB do encoder e comprimento total |
| 05 | `tb6612` | SparkFun ROB-14450 para as cotas da placa; datasheet Toshiba TB6612FNG para os limites elétricos |

Se uma cota não existir na fonte oficial, marque `Prov.TIPICO` com a fonte que
você usou, ou `Prov.MEDIR` se ela variar por fornecedor. **Não preencha por
estimativa silenciosa.**

- [ ] **Step 4: Implementar o mínimo**

```python
"""Fonte única de dados dimensionais dos componentes comprados.

Toda medida carrega nível de proveniência e, quando OFICIAL, a URL e a
figura/página de onde saiu. Nenhum valor aqui pode ser estimado em silêncio.

Sistema de referência das medidas de cada componente está descrito no campo
`notas_montagem` da respectiva entrada.
"""
from __future__ import annotations

import os
import sys
from dataclasses import dataclass, field

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "tools"))

from techdraw import Prov  # noqa: E402


@dataclass(frozen=True)
class Medida:
    """Uma cota, com o nível de confiança e a origem do valor."""

    valor: float          # mm
    prov: Prov
    fonte: str = ""       # URL + figura/página — obrigatório se OFICIAL
    nota: str = ""


@dataclass(frozen=True)
class Componente:
    """Ficha de um componente comprado."""

    num: str              # "01" .. "10"
    nome: str
    fabricante: str
    codigo: str
    fornecedor: str
    preco: str
    url_datasheet: str
    ref_abnt: str         # referência NBR 6023, reaproveitada na monografia
    massa_g: float
    medidas: dict[str, Medida]
    eletrico: dict[str, str] = field(default_factory=dict)
    notas_montagem: list[str] = field(default_factory=list)


COMPONENTES: dict[str, Componente] = {
    # Preencha 01 a 05 nesta task, com os valores levantados no Step 3.
    # O exemplo abaixo é o único já conferido nesta sessão e serve de modelo
    # de preenchimento — mantenha o mesmo rigor nos demais.
    "qtr8a": Componente(
        num="03",
        nome="QTR-8A Reflectance Sensor Array",
        fabricante="Pololu",
        codigo="960",
        fornecedor="Pololu (importado)",
        preco="US$ 12,95",
        url_datasheet="https://www.pololu.com/product/960",
        ref_abnt=(
            "POLOLU CORPORATION. QTR-8A Reflectance Sensor Array. "
            "Las Vegas, 2026. Disponível em: "
            "https://www.pololu.com/product/960. Acesso em: 12 ago. 2026."
        ),
        massa_g=3.09,
        medidas={
            "comprimento": Medida(
                74.93, Prov.OFICIAL,
                "https://www.pololu.com/product/960 — Specifications, "
                'Size: 2.95" x 0.5" x 0.125"'),
            "largura": Medida(
                12.70, Prov.OFICIAL,
                "https://www.pololu.com/product/960 — Specifications, Size"),
            "espessura": Medida(
                3.18, Prov.OFICIAL,
                "https://www.pololu.com/product/960 — Specifications, Size "
                "(sem os pinos de header instalados)"),
            "passo_sensores": Medida(
                9.525, Prov.OFICIAL,
                'https://www.pololu.com/product/960 — Sensor pitch: 0.375"'),
        },
        eletrico={
            "tensão de operação": "3,3 – 5,0 V",
            "corrente de alimentação": "100 mA (8 emissores)",
            "saída": "analógica, 0 V a VCC, uma por canal",
        },
        notas_montagem=[
            "Alimentar em 3,3 V para casar com VDD/VREF do MCP3008. "
            "Em 5 V a saída excede a entrada máxima do ADC (VDD+0,3 V).",
            "Espessura de 3,18 mm é sem os pinos de header. Com header "
            "soldado, prever folga adicional no berço.",
            "Deixar LEDON solto — o pull-up interno mantém os emissores ligados.",
            "Altura ao solo de projeto: 4 mm (SENSOR_HEIGHT_MM em config.h).",
            "O passo real de 9,525 mm difere do SENSOR_SPACING_MM 10 usado no "
            "firmware; a diferença é absorvida pelo Kp e pela calibração "
            "(ver docs/DIVERGENCIAS.md, D5).",
        ],
    ),
}
```

- [ ] **Step 5: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_specs -v`
Expected: PASS — 7 testes, com `esp32`, `mcp3008`, `qtr8a`, `n20` e `tb6612` preenchidos.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add hardware/components/specs.py tools/tests/test_specs.py
git commit -m "feat(specs): dimensional data for components 01-05 with provenance"
```

---

### Task 7: Dados dimensionais — componentes 06 a 10

**Files:**
- Modify: `hardware/components/specs.py`
- Modify: `tools/tests/test_specs.py`

**Interfaces:**
- Consumes: `Medida`, `Componente`, `COMPONENTES` da Task 6.
- Produces: chaves `lipo`, `mp1584`, `roda`, `caster`, `perfboard` em `COMPONENTES`.

- [ ] **Step 1: Escrever o teste que falha**

Acrescente a `tools/tests/test_specs.py`:

```python
class TestComponentes06a10(unittest.TestCase):
    def test_componentes_06_a_10_presentes(self):
        for chave in ("lipo", "mp1584", "roda", "caster", "perfboard"):
            self.assertIn(chave, COMPONENTES)

    def test_lipo_marcado_como_medir(self):
        # "2S 800 mAh 30C" não define dimensão. Qualquer cota OFICIAL aqui
        # seria invenção.
        lipo = COMPONENTES["lipo"]
        for nome in ("comprimento", "largura", "altura"):
            self.assertIs(lipo.medidas[nome].prov, Prov.MEDIR,
                          f"lipo.{nome} não pode ser OFICIAL nem TIPICO")

    def test_perfboard_declara_o_passo_de_2_54(self):
        self.assertAlmostEqual(
            COMPONENTES["perfboard"].medidas["passo_furos"].valor, 2.54,
            places=2)

    def test_roda_declara_diametro_externo_e_largura(self):
        roda = COMPONENTES["roda"]
        self.assertIn("diametro_externo", roda.medidas)
        self.assertIn("largura", roda.medidas)

    def test_sao_dez_componentes(self):
        self.assertEqual(len(COMPONENTES), 10)
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_specs -v`
Expected: FAIL com `KeyError: 'lipo'` ou `AssertionError: 'lipo' not found in ...`

- [ ] **Step 3: Levantar os dados dos componentes 06 a 10**

| # | Chave | Fonte a consultar | Proveniência esperada |
|---|-------|-------------------|----------------------|
| 06 | `lipo` | Nenhuma oficial — "2S 800 mAh 30C" não define geometria | Todas as cotas `MEDIR`. Registre em `nota` a faixa que cabe no projeto (a BOM já anota máx. 65×35×12 mm) |
| 07 | `mp1584` | Datasheet MPS do MP1584EN para o CI; módulo genérico para a PCB | CI `OFICIAL`, PCB `TIPICO` |
| 08 | `roda` | Genérico "⌀34 mm, eixo D 3 mm". Confronte com `wheel_hub.scad` | `TIPICO`, exceto o eixo que casa com o N20 (`OFICIAL` via ficha 04) |
| 09 | `caster` | **Confirme o fornecedor real** — a BOM cita "ROB-09259" (SKU SparkFun) com link Robocore. Pololu publica desenho dimensional dos seus ball casters | `OFICIAL` se achar o desenho; senão `TIPICO`. Note o conflito ⌀15 (SCAD) vs 12,7 (½", BOM) — D4 |
| 10 | `perfboard` | Padrão de mercado: 50 × 70 mm, passo 2,54 mm | `TIPICO`, exceto o passo que é padrão consolidado |

- [ ] **Step 4: Implementar o mínimo**

Acrescente as cinco entradas ao dicionário `COMPONENTES` seguindo exatamente o
formato da entrada `qtr8a` da Task 6: `num`, `nome`, `fabricante`, `codigo`,
`fornecedor`, `preco`, `url_datasheet`, `ref_abnt` (NBR 6023), `massa_g`,
`medidas` (cada uma com `Medida(valor, prov, fonte, nota)`), `eletrico` e
`notas_montagem`.

Para o LiPo, use o padrão abaixo — é o caso onde a honestidade sobre a
incerteza é o conteúdo principal:

```python
    "lipo": Componente(
        num="06",
        nome="Bateria LiPo 2S 800 mAh 30C",
        fabricante="Genérico",
        codigo="—",
        fornecedor="Mercado Livre / Hobbyking",
        preco="R$ 55,00",
        url_datasheet="",
        ref_abnt="",
        massa_g=0.0,   # substitua pela massa do SEU pack, medida na balança
        medidas={
            "comprimento": Medida(
                58.0, Prov.MEDIR,
                "valor de projeto do bom.csv",
                nota="MEDIR o pack adquirido. A designação '2S 800 mAh 30C' "
                     "não define geometria; packs comerciais variam de "
                     "aproximadamente 50 a 65 mm. Limite do projeto: 65 mm."),
            "largura": Medida(
                32.0, Prov.MEDIR, "valor de projeto do bom.csv",
                nota="MEDIR. Limite do projeto: 35 mm."),
            "altura": Medida(
                12.0, Prov.MEDIR, "valor de projeto do bom.csv",
                nota="MEDIR. Limite do projeto: 12 mm."),
        },
        eletrico={
            "tensão nominal": "7,4 V (2 células)",
            "tensão plena": "8,4 V",
            "tensão mínima segura": "6,0 V (3,0 V/célula)",
            "capacidade": "800 mAh",
            "descarga contínua": "30C = 24 A",
        },
        notas_montagem=[
            "MEDIR o pack antes de modelar o alojamento. Todas as cotas aqui "
            "são de projeto, não do fornecedor.",
            "Massa é insumo do cálculo de CG na prancha A1 — pese o pack.",
            "Posicionar atrás do eixo motriz para manter o CG dentro do "
            "triângulo de apoio (ver prancha A1).",
            "Prever acesso ao conector de balanceamento sem desmontar o chassi.",
        ],
    ),
```

> **`massa_g=0.0` faz o teste `test_massa_declarada_para_o_calculo_de_cg` da
> Task 6 falhar de propósito.** É o portão: sem a massa real do pack, a prancha
> A1 não fecha. Pese o pack e substitua o valor. Se ainda não tiver o pack em
> mãos, use a massa nominal de um pack 2S 800 mAh equivalente, marque em
> `notas_montagem` que é estimada, e registre o item no `docs/DIVERGENCIAS.md`.

- [ ] **Step 5: Rodar os testes e confirmar que passam**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_specs -v`
Expected: PASS — 12 testes, `len(COMPONENTES) == 10`.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add hardware/components/specs.py tools/tests/test_specs.py
git commit -m "feat(specs): dimensional data for components 06-10"
```

---

### Task 8: Gerador de fichas

**Files:**
- Create: `tools/gen_fichas.py`
- Create: `hardware/components/fichas/*.md` (10 arquivos, gerados)
- Test: `tools/tests/test_gen_fichas.py`

**Interfaces:**
- Consumes: `COMPONENTES`, `Medida`, `Componente` de `specs.py`; `Prov` de `techdraw`.
- Produces: `nome_arquivo(c: Componente) -> str`, `render_ficha(c: Componente) -> str`, `main(argv) -> int`.

As fichas são **geradas**, nunca editadas à mão — `specs.py` é a fonte única.
Cada ficha abre com um aviso disso.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_gen_fichas.py
import unittest

from gen_fichas import nome_arquivo, render_ficha
from specs import COMPONENTES
from techdraw import Prov


class TestNomeArquivo(unittest.TestCase):
    def test_prefixo_numerico_e_slug(self):
        self.assertEqual(nome_arquivo(COMPONENTES["qtr8a"]), "03-qtr8a.md")


class TestRenderFicha(unittest.TestCase):
    def setUp(self):
        self.md = render_ficha(COMPONENTES["qtr8a"])

    def test_avisa_que_e_gerado(self):
        self.assertIn("gerado automaticamente", self.md.lower())
        self.assertIn("specs.py", self.md)

    def test_traz_identificacao(self):
        self.assertIn("QTR-8A Reflectance Sensor Array", self.md)
        self.assertIn("Pololu", self.md)
        self.assertIn("960", self.md)

    def test_tabela_de_medidas_tem_coluna_de_proveniencia(self):
        self.assertIn("| Medida | Valor | Proveniência | Fonte |", self.md)
        self.assertIn("74,93", self.md)
        self.assertIn(Prov.OFICIAL.value, self.md)

    def test_traz_massa_para_o_calculo_de_cg(self):
        self.assertIn("3,09", self.md)

    def test_traz_referencia_abnt(self):
        self.assertIn("POLOLU CORPORATION", self.md)
        self.assertIn("Acesso em:", self.md)

    def test_traz_notas_de_montagem(self):
        self.assertIn("3,3 V", self.md)

    def test_remete_a_prancha(self):
        self.assertIn("hardware/drawings/comp/03-qtr8a.svg", self.md)

    def test_medida_medir_ganha_destaque(self):
        md = render_ficha(COMPONENTES["lipo"])
        self.assertIn(Prov.MEDIR.value, md)
        self.assertIn("MEDIR", md)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_fichas -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'gen_fichas'`

> `tests/test_gen_fichas.py` importa `specs`. Garanta que `tools/tests/__init__.py`
> ou o `sys.path` do teste inclua `hardware/components/`, do mesmo modo que
> `tests/test_specs.py` faz.

- [ ] **Step 3: Implementar o mínimo**

```python
#!/usr/bin/env python3
"""Gera as fichas dimensionais em Markdown a partir de specs.py.

Uso:
    python3 gen_fichas.py

As fichas são artefato gerado — NÃO edite os .md. Altere specs.py e regenere.
"""
from __future__ import annotations

import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "hardware", "components"))
sys.path.insert(0, os.path.join(RAIZ, "tools"))

from specs import COMPONENTES, Componente  # noqa: E402
from techdraw import Prov, fmt_valor  # noqa: E402

DIR_FICHAS = os.path.join(RAIZ, "hardware", "components", "fichas")

AVISO = (
    "> **Arquivo gerado automaticamente** por `tools/gen_fichas.py` a partir "
    "de `hardware/components/specs.py`. Não edite este arquivo — altere o "
    "`specs.py` e regenere.\n"
)


def nome_arquivo(c: Componente) -> str:
    """Nome do arquivo da ficha: <num>-<chave>.md"""
    chave = next(k for k, v in COMPONENTES.items() if v is c)
    return f"{c.num}-{chave}.md"


def _linha_medida(nome: str, m) -> str:
    rotulo = nome.replace("_", " ")
    fonte = m.fonte or "—"
    if m.nota:
        fonte = f"{fonte}<br>**{m.nota}**" if m.prov is Prov.MEDIR \
            else f"{fonte}<br>{m.nota}"
    return (f"| {rotulo} | {fmt_valor(m.valor)} mm | "
            f"{m.prov.value} {m.prov.name} | {fonte} |")


def render_ficha(c: Componente) -> str:
    """Monta o Markdown da ficha de um componente."""
    chave = next(k for k, v in COMPONENTES.items() if v is c)
    p: list[str] = [
        f"# Ficha {c.num} — {c.nome}\n",
        AVISO,
        "\n## 1 Identificação\n",
        "| Campo | Valor |",
        "|-------|-------|",
        f"| Denominação | {c.nome} |",
        f"| Fabricante | {c.fabricante} |",
        f"| Código | {c.codigo} |",
        f"| Fornecedor | {c.fornecedor} |",
        f"| Preço | {c.preco} |",
        f"| Massa | {fmt_valor(c.massa_g)} g |",
        f"| Datasheet | {c.url_datasheet or '— (sem datasheet oficial)'} |",
        "\n## 2 Dimensões\n",
        "| Medida | Valor | Proveniência | Fonte |",
        "|--------|-------|--------------|-------|",
    ]
    for nome, m in c.medidas.items():
        p.append(_linha_medida(nome, m))

    p.append("\n**Legenda de proveniência**: "
             f"{Prov.OFICIAL.value} {Prov.OFICIAL.descricao} · "
             f"{Prov.TIPICO.value} {Prov.TIPICO.descricao} · "
             f"{Prov.MEDIR.value} {Prov.MEDIR.descricao}\n")

    if c.eletrico:
        p.append("\n## 3 Características elétricas\n")
        p.append("| Grandeza | Valor |")
        p.append("|----------|-------|")
        for k, v in c.eletrico.items():
            p.append(f"| {k} | {v} |")

    if c.notas_montagem:
        p.append("\n## 4 Notas de montagem\n")
        for n in c.notas_montagem:
            p.append(f"- {n}")

    p.append("\n## 5 Prancha\n")
    p.append(f"`hardware/drawings/comp/{c.num}-{chave}.svg`\n")

    if c.ref_abnt:
        p.append("\n## 6 Referência (NBR 6023)\n")
        p.append(f"{c.ref_abnt}\n")

    return "\n".join(p)


def main(argv: list[str]) -> int:
    os.makedirs(DIR_FICHAS, exist_ok=True)
    for c in sorted(COMPONENTES.values(), key=lambda x: x.num):
        destino = os.path.join(DIR_FICHAS, nome_arquivo(c))
        with open(destino, "w", encoding="utf-8") as fh:
            fh.write(render_ficha(c))
        print(f"gerado: {destino}")
    print(f"\n{len(COMPONENTES)} ficha(s) geradas.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
```

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_fichas -v`
Expected: PASS — 9 testes.

- [ ] **Step 5: Gerar as fichas e ler duas delas**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 gen_fichas.py
```

Leia `hardware/components/fichas/03-qtr8a.md` e `06-lipo.md` inteiras. Confirme:
tabelas bem formadas, todas as cotas com proveniência, as cotas `MEDIR` do LiPo
visivelmente destacadas, referência NBR 6023 completa no QTR-8A.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/gen_fichas.py tools/tests/test_gen_fichas.py hardware/components/fichas/
git commit -m "feat(fichas): generate 10 dimensional data sheets from specs.py"
```

---

### Task 9: As 10 pranchas de componente

**Files:**
- Create: `tools/gen_component_drawings.py`
- Create: `hardware/drawings/comp/*.svg` (10 arquivos, gerados)
- Test: `tools/tests/test_gen_component_drawings.py`

**Interfaces:**
- Consumes: toda a API de `techdraw`; `COMPONENTES` de `specs.py`.
- Produces: `escala_para(c: Componente) -> tuple[float, str]`, uma função `prancha_<chave>(c) -> str` por componente, `PRANCHAS: dict[str, callable]`, `main(argv) -> int`.

Cada função de prancha compõe suas vistas livremente. A escala sai de
`escala_para`, que escolhe dentre 2:1, 1:1 e 1:2 aquela que faz a peça ocupar
entre 40% e 75% da área útil da folha.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_gen_component_drawings.py
import unittest

from gen_component_drawings import PRANCHAS, escala_para
from specs import COMPONENTES


class TestEscolhaDeEscala(unittest.TestCase):
    def test_peca_pequena_recebe_ampliacao(self):
        fator, rotulo = escala_para(COMPONENTES["mcp3008"])
        self.assertEqual(fator, 2.0)
        self.assertEqual(rotulo, "2:1")

    def test_peca_media_fica_em_escala_natural(self):
        fator, rotulo = escala_para(COMPONENTES["qtr8a"])
        self.assertEqual(fator, 1.0)
        self.assertEqual(rotulo, "1:1")

    def test_so_devolve_escalas_normalizadas_nbr_8196(self):
        for c in COMPONENTES.values():
            fator, _ = escala_para(c)
            self.assertIn(fator, (2.0, 1.0, 0.5))


class TestPranchas(unittest.TestCase):
    def test_existe_uma_prancha_por_componente(self):
        self.assertEqual(set(PRANCHAS), set(COMPONENTES))

    def test_toda_prancha_e_svg_valido_com_fundo_branco(self):
        for chave, fn in PRANCHAS.items():
            svg = fn(COMPONENTES[chave])
            self.assertTrue(svg.startswith("<svg"), chave)
            self.assertTrue(svg.rstrip().endswith("</svg>"), chave)
            self.assertIn('fill="#ffffff"', svg, chave)

    def test_toda_prancha_traz_a_legenda_e_a_proveniencia(self):
        for chave, fn in PRANCHAS.items():
            svg = fn(COMPONENTES[chave])
            self.assertIn("PROVENIÊNCIA", svg, chave)
            self.assertIn("Unidade: mm", svg, chave)
            self.assertIn(COMPONENTES[chave].num, svg, chave)

    def test_prancha_do_qtr8a_cota_o_passo_dos_sensores(self):
        svg = PRANCHAS["qtr8a"](COMPONENTES["qtr8a"])
        self.assertIn("9,53", svg)   # 9,525 arredondado para duas casas
        self.assertIn("74,93", svg)

    def test_prancha_do_lipo_marca_as_cotas_como_medir(self):
        from techdraw import Prov
        svg = PRANCHAS["lipo"](COMPONENTES["lipo"])
        self.assertIn(Prov.MEDIR.value, svg)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_component_drawings -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'gen_component_drawings'`

- [ ] **Step 3: Implementar o mínimo**

Estrutura do arquivo, com uma prancha completa como modelo. Escreva as outras
nove no mesmo padrão, cada uma com as vistas que a peça exige (planas: superior
+ lateral; N20, roda e caster: superior + frontal + lateral).

```python
#!/usr/bin/env python3
"""Gera as 10 pranchas de componente a partir de specs.py.

Uso:
    python3 gen_component_drawings.py
    python3 verify_drawings.py     # depois, para rasterizar e inspecionar
"""
from __future__ import annotations

import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "hardware", "components"))
sys.path.insert(0, os.path.join(RAIZ, "tools"))

from specs import COMPONENTES, Componente  # noqa: E402
from techdraw import (MARGIN_LEFT, MARGIN_OTHER, SHEET_H, SHEET_W, Prov,  # noqa: E402
                      Sheet, View, dim_diam, dim_h, dim_v, note)

DIR_SAIDA = os.path.join(RAIZ, "hardware", "drawings", "comp")
DATA = "2026-08-12"

# Área útil da folha descontando legenda e quadro de proveniência
UTIL_W = SHEET_W - MARGIN_LEFT - MARGIN_OTHER - 20.0
UTIL_H = SHEET_H - 2 * MARGIN_OTHER - 45.0


def _maior_dimensao(c: Componente) -> float:
    return max(m.valor for m in c.medidas.values())


def escala_para(c: Componente) -> tuple[float, str]:
    """Escolhe a maior escala normalizada que ainda cabe na área útil."""
    maior = _maior_dimensao(c)
    for fator, rotulo in ((2.0, "2:1"), (1.0, "1:1"), (0.5, "1:2")):
        if maior * fator <= UTIL_W * 0.75:
            return fator, rotulo
    return 0.5, "1:2"


def prancha_qtr8a(c: Componente) -> str:
    """QTR-8A: vista superior com o passo dos sensores, e vista lateral."""
    fator, rotulo = escala_para(c)
    s = Sheet(title=c.nome, maker=c.fabricante, code=c.codigo,
              scale=rotulo, sheet_id=c.num, date=DATA)

    L = c.medidas["comprimento"]
    W = c.medidas["largura"]
    T = c.medidas["espessura"]
    P = c.medidas["passo_sensores"]

    # ── vista superior ─────────────────────────────────────────────────
    sup = View(origin_x=60.0, origin_y=110.0, scale=fator)
    s.add(sup.rect_part(0.0, 0.0, L.valor, W.valor))

    # centros dos 8 sensores, simétricos em torno do meio da placa
    meio = L.valor / 2.0
    centros = [meio + (i - 3.5) * P.valor for i in range(8)]
    for cx in centros:
        s.add(sup.cross(cx, W.valor / 2.0, 6.0))

    s.add(dim_h(sup, 0.0, L.valor, 0.0, L.valor, L.prov, offset=22.0))
    s.add(dim_h(sup, centros[0], centros[1], 0.0, P.valor, P.prov, offset=12.0))
    s.add(dim_v(sup, 0.0, W.valor, 0.0, W.valor, W.prov, offset=12.0))
    s.add(note(sup, centros[0], W.valor / 2.0,
               "8 × par LED IR / fototransistor", dx=-20.0, dy=-28.0))

    # ── vista lateral ──────────────────────────────────────────────────
    lat = View(origin_x=60.0, origin_y=155.0, scale=fator)
    s.add(lat.rect_part(0.0, 0.0, L.valor, T.valor))
    s.add(dim_v(lat, 0.0, T.valor, 0.0, T.valor, T.prov, offset=12.0))
    s.add(note(lat, L.valor, T.valor,
               "espessura sem header soldado", dx=14.0, dy=-8.0))

    return s.render()


PRANCHAS = {
    "qtr8a": prancha_qtr8a,
    # as outras nove entram aqui — ver a tabela de conteúdo obrigatório abaixo
}


def main(argv: list[str]) -> int:
    os.makedirs(DIR_SAIDA, exist_ok=True)
    for chave, fn in sorted(PRANCHAS.items(),
                            key=lambda kv: COMPONENTES[kv[0]].num):
        c = COMPONENTES[chave]
        destino = os.path.join(DIR_SAIDA, f"{c.num}-{chave}.svg")
        with open(destino, "w", encoding="utf-8") as fh:
            fh.write(fn(c))
        print(f"gerado: {destino}")
    print(f"\n{len(PRANCHAS)} prancha(s). Rode verify_drawings.py e INSPECIONE.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
```

**Conteúdo obrigatório de cada prancha.** Isto não é "faça igual ao exemplo" —
é o que cada prancha tem que conter para servir à modelagem:

| # | Chave | Vistas | Cotas obrigatórias | Notas obrigatórias |
|---|-------|--------|--------------------|--------------------|
| 01 | `esp32` | superior, lateral | Comprimento, largura, altura da PCB; distância entre as duas fileiras de pinos; passo dos pinos (2,54); posição do conector USB | Altura com pinos soldados; folga para o cabo USB |
| 02 | `mcp3008` | superior, lateral | Comprimento e largura do corpo PDIP-16; passo 2,54; largura entre fileiras 7,62; altura do corpo; comprimento do pino | Encapsulamento PDIP-16; marcação do pino 1 |
| 03 | `qtr8a` | superior, lateral | Comprimento, largura, espessura; passo entre sensores; centro dos 8 sensores | Espessura sem header; alimentar em 3,3 V |
| 04 | `n20` | superior, frontal, lateral | Comprimento total; corpo do motor; caixa de redução; ⌀ e comprimento do eixo; achatado do D; PCB do encoder; entre-eixos de fixação | Sentido do achatado; fixação M2 |
| 05 | `tb6612` | superior, lateral | Comprimento, largura, espessura da PCB; ⌀ e posição dos furos de fixação; passo dos pinos | STBY precisa de nível alto para operar |
| 06 | `lipo` | superior, lateral | Comprimento, largura, altura — **todas com símbolo MEDIR**; comprimento do chicote | Cotas de projeto, não do fornecedor; pesar o pack |
| 07 | `mp1584` | superior, lateral | Comprimento, largura, altura com indutor; ⌀ e posição dos furos | Ajustar a 5,0 V antes de ligar a carga |
| 08 | `roda` | frontal, lateral | ⌀ externo; largura; ⌀ e profundidade do furo do eixo; achatado do D; ⌀ do canal do O-ring | Casar o furo do eixo com a ficha 04 |
| 09 | `caster` | frontal, lateral | ⌀ da esfera; altura total; ⌀ e entre-furos da base; faixa de ajuste com espaçadores | Altura tem que casar com ⌀roda/2 do layout A1 |
| 10 | `perfboard` | superior, lateral | Comprimento, largura, espessura; passo 2,54; contagem de furos; margem até o primeiro furo | Recortar conforme o alojamento do chassi |

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_component_drawings -v`
Expected: PASS — 7 testes, com as 10 pranchas registradas em `PRANCHAS`.

- [ ] **Step 5: Gerar, rasterizar e inspecionar as 10 pranchas**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 gen_component_drawings.py && python3 verify_drawings.py
```

**Abra os 10 PNGs em `build/drawings/` e olhe cada um.** Checklist por prancha:

- [ ] Nenhuma cota sobrepõe outra cota nem o contorno da peça
- [ ] Nenhum texto estoura a moldura ou invade a legenda
- [ ] Todas as vistas presentes e alinhadas entre si
- [ ] Cada cota tem símbolo de proveniência legível
- [ ] Escala da legenda confere com o tamanho aparente da peça
- [ ] Linhas de centro ultrapassam o contorno em ambos os lados

Corrija as coordenadas e regenere até todas passarem. **Não commite prancha
que você não olhou.**

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/gen_component_drawings.py tools/tests/test_gen_component_drawings.py hardware/drawings/comp/
git commit -m "feat(drawings): 10 dimensioned component plates"
```

---

### Task 10: Prancha A1 — layout funcional e balanço de CG

**Files:**
- Create: `tools/layout.py`
- Create: `tools/gen_assembly_drawings.py`
- Create: `hardware/drawings/assy/A1-layout-funcional.svg`
- Test: `tools/tests/test_layout.py`

**Interfaces:**
- Consumes: `COMPONENTES` de `specs.py`; API de `techdraw`.
- Produces: `Massa(nome, massa_g, x_mm)`, `centro_de_gravidade(massas) -> float`, `margem_de_estabilidade(cg_x, x_caster) -> float`, `LAYOUT: dict[str, float]`, `prancha_a1() -> str`.

**Sistema de coordenadas** (declarado na prancha): origem no ponto médio do eixo
motriz ao nível do solo; X positivo no sentido de marcha; Y positivo para a
esquerda; Z positivo para cima.

**Regra de estabilidade**: com caster traseiro em `x_caster < 0`, o CG precisa
satisfazer `x_caster < cg_x < 0` para cair dentro do triângulo de apoio. A
margem é a distância do CG à borda mais próxima.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_layout.py
import unittest

from layout import (LAYOUT, Massa, centro_de_gravidade,
                    margem_de_estabilidade)


class TestCentroDeGravidade(unittest.TestCase):
    def test_massa_unica_fica_na_propria_posicao(self):
        self.assertAlmostEqual(
            centro_de_gravidade([Massa("a", 100.0, -20.0)]), -20.0)

    def test_duas_massas_iguais_ficam_no_meio(self):
        cg = centro_de_gravidade(
            [Massa("a", 50.0, -40.0), Massa("b", 50.0, 0.0)])
        self.assertAlmostEqual(cg, -20.0)

    def test_massa_maior_puxa_o_cg(self):
        cg = centro_de_gravidade(
            [Massa("pesada", 90.0, -10.0), Massa("leve", 10.0, 70.0)])
        self.assertAlmostEqual(cg, -2.0)

    def test_lista_vazia_e_erro(self):
        with self.assertRaises(ValueError):
            centro_de_gravidade([])

    def test_massa_total_nula_e_erro(self):
        with self.assertRaises(ValueError):
            centro_de_gravidade([Massa("fantasma", 0.0, 10.0)])


class TestEstabilidade(unittest.TestCase):
    def test_cg_no_meio_do_vao_tem_margem_maxima(self):
        # caster em -60, eixo motriz em 0 → CG em -30 está mais folgado
        self.assertAlmostEqual(margem_de_estabilidade(-30.0, -60.0), 30.0)

    def test_cg_sobre_o_eixo_motriz_tem_margem_zero(self):
        self.assertAlmostEqual(margem_de_estabilidade(0.0, -60.0), 0.0)

    def test_cg_a_frente_do_eixo_motriz_da_margem_negativa(self):
        # capota para trás? não — sai do triângulo pela frente
        self.assertLess(margem_de_estabilidade(10.0, -60.0), 0.0)

    def test_cg_atras_do_caster_da_margem_negativa(self):
        self.assertLess(margem_de_estabilidade(-70.0, -60.0), 0.0)


class TestLayout(unittest.TestCase):
    def test_lookahead_vem_do_config_h(self):
        self.assertAlmostEqual(LAYOUT["lookahead"], 70.0)

    def test_altura_dos_sensores_vem_do_config_h(self):
        self.assertAlmostEqual(LAYOUT["altura_sensores"], 4.0)

    def test_eixo_motriz_e_a_origem(self):
        self.assertAlmostEqual(LAYOUT["x_eixo_motriz"], 0.0)

    def test_caster_fica_atras_do_eixo_motriz(self):
        self.assertLess(LAYOUT["x_caster"], 0.0)

    def test_cabe_no_envelope_de_competicao(self):
        comprimento = LAYOUT["lookahead"] - LAYOUT["x_caster"]
        self.assertLess(comprimento, 250.0)
        self.assertLess(LAYOUT["bitola"], 250.0)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_layout -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'layout'`

- [ ] **Step 3: Implementar o mínimo**

```python
"""Layout funcional do robô: geometria de referência e balanço de massas.

Sistema de coordenadas:
    origem  — ponto médio do eixo motriz, ao nível do solo
    X > 0   — sentido de marcha (à frente)
    Y > 0   — para a esquerda
    Z > 0   — para cima
"""
from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Massa:
    """Massa concentrada de um componente, na coordenada longitudinal X."""

    nome: str
    massa_g: float
    x_mm: float


def centro_de_gravidade(massas: list[Massa]) -> float:
    """Posição longitudinal do CG, em mm, no sistema do layout."""
    if not massas:
        raise ValueError("lista de massas vazia")
    total = sum(m.massa_g for m in massas)
    if total <= 0.0:
        raise ValueError("massa total nula — preencha massa_g em specs.py")
    return sum(m.massa_g * m.x_mm for m in massas) / total


def margem_de_estabilidade(cg_x: float, x_caster: float) -> float:
    """Distância do CG à borda mais próxima do triângulo de apoio.

    Com caster traseiro (x_caster < 0), o apoio vai de x_caster até 0
    (o eixo motriz). Valor positivo = estável; negativo = fora do apoio.
    """
    return min(cg_x - x_caster, 0.0 - cg_x)


# ═══ Geometria de referência ═════════════════════════════════════════════
# lookahead e altura_sensores vêm de src/config.h (LOOKAHEAD_MM,
# SENSOR_HEIGHT_MM). Mantenha sincronizado.
LAYOUT: dict[str, float] = {
    "x_eixo_motriz": 0.0,
    "lookahead": 70.0,          # LOOKAHEAD_MM
    "altura_sensores": 4.0,     # SENSOR_HEIGHT_MM
    "altura_eixo_motriz": 17.0, # ⌀roda / 2 — confirmar com a ficha 08
    "bitola": 80.0,             # a resolver a partir das fichas 04 e 08
    "x_caster": -55.0,          # resolvido pelo balanço de CG neste Step
    "x_lipo": -40.0,            # idem
}
```

Agora **resolva `x_caster` e `x_lipo` de fato**: monte a lista de `Massa` com as
massas reais de `specs.py`, posicione o LiPo e o caster, calcule o CG e a
margem, e ajuste até a margem ficar confortável (alvo: pelo menos 15 mm, e CG
mais próximo do eixo motriz que do caster para carregar a tração). Registre os
valores finais em `LAYOUT` e o resultado do cálculo na própria prancha.

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_layout -v`
Expected: PASS — 14 testes.

- [ ] **Step 5: Desenhar a prancha A1**

Crie `tools/gen_assembly_drawings.py` com `prancha_a1()`. A prancha traz:

- Planta com os eixos e a origem marcada, e o rótulo do sistema de coordenadas
- Rodas E e D no eixo motriz, plano dos sensores em X=+70, caster em `x_caster`
- Envelopes retangulares de ESP32, LiPo e perfboard, cotados em posição
- Cotas: lookahead, bitola, entre-eixos, alturas ao solo dos sensores e do eixo
- Envelope de competição 250×250 mm em linha traço-duplo-ponto
- Marcação do CG calculado e da margem de estabilidade
- Duas notas com as restrições de projeto: CG dentro do triângulo de apoio; CG
  próximo ao eixo motriz para carregar a tração
- Nota declarando que as massas em `MEDIR` tornam o CG indicativo, e informando
  a margem disponível

Gere e inspecione:

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 gen_assembly_drawings.py && python3 verify_drawings.py
```

Abra `build/drawings/A1-layout-funcional.png` e confirme cada item da lista acima.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/layout.py tools/gen_assembly_drawings.py tools/tests/test_layout.py hardware/drawings/assy/A1-layout-funcional.svg
git commit -m "feat(assy): A1 functional layout plate with CG balance"
```

---

### Task 11: Prancha A2 — vista explodida

**Files:**
- Modify: `tools/gen_assembly_drawings.py`
- Create: `hardware/drawings/assy/A2-vista-explodida.svg`
- Test: `tools/tests/test_gen_assembly.py`

**Interfaces:**
- Consumes: `iso_box`, `iso_cyl`, `balloon`, `parts_list`, `PartItem` de `techdraw`; `LAYOUT` de `layout.py`.
- Produces: `ITENS: list[PartItem]`, `prancha_a2() -> str`.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_gen_assembly.py
import unittest

from gen_assembly_drawings import ITENS, prancha_a1, prancha_a2


class TestA1(unittest.TestCase):
    def test_declara_o_sistema_de_coordenadas(self):
        svg = prancha_a1()
        self.assertIn("eixo motriz", svg.lower())
        self.assertIn("sentido de marcha", svg.lower())

    def test_mostra_o_envelope_de_competicao(self):
        self.assertIn("250", prancha_a1())

    def test_cota_o_lookahead(self):
        self.assertIn("70", prancha_a1())


class TestA2(unittest.TestCase):
    def test_todo_item_tem_balao(self):
        svg = prancha_a2()
        for it in ITENS:
            self.assertIn(f">{it.num}<", svg,
                          f"item {it.num} ({it.denominacao}) sem balão")

    def test_lista_de_pecas_presente(self):
        svg = prancha_a2()
        self.assertIn("DENOMINAÇÃO", svg)
        self.assertIn("PRANCHA", svg)

    def test_numeracao_dos_itens_e_sequencial_a_partir_de_1(self):
        self.assertEqual([it.num for it in ITENS],
                         list(range(1, len(ITENS) + 1)))

    def test_declara_a_representacao_simplificada(self):
        # Sem esta nota a prancha promete um detalhe que não entrega.
        self.assertIn("simplificad", prancha_a2().lower())


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_assembly -v`
Expected: FAIL com `ImportError: cannot import name 'ITENS'`

- [ ] **Step 3: Implementar o mínimo**

Acrescente a `tools/gen_assembly_drawings.py`:

```python
ITENS: list[PartItem] = [
    PartItem(1, 1, "Chassi (a modelar)", "PETG impresso", "—"),
    PartItem(2, 1, "QTR-8A Reflectance Sensor Array", "Pololu 960", "03"),
    PartItem(3, 2, "Micro metal gearmotor N20 c/ encoder", "Pololu", "04"),
    PartItem(4, 2, "Roda silicone ⌀34 mm", "Genérico", "08"),
    PartItem(5, 1, "Ball caster ½\"", "Pololu/SparkFun", "09"),
    PartItem(6, 1, "ESP32 DevKit V1", "DOIT", "01"),
    PartItem(7, 1, "Perfboard 50×70 mm", "Genérico", "10"),
    PartItem(8, 1, "MCP3008 (no perfboard)", "Microchip", "02"),
    PartItem(9, 1, "TB6612FNG breakout", "SparkFun ROB-14450", "05"),
    PartItem(10, 1, "MP1584EN buck", "Genérico", "07"),
    PartItem(11, 1, "Bateria LiPo 2S 800 mAh", "Genérico", "06"),
]
```

E `prancha_a2()`, com:

- Sólidos isométricos (`iso_box`, `iso_cyl`) de cada item, dimensionados pelos
  envelopes de `specs.py`
- Separação ao longo dos eixos de montagem, com trajetórias em traço-ponto
- Um `balloon` por item, ligado ao sólido correspondente
- `parts_list` no canto, com os 11 itens
- Nota explícita: *"Componentes em representação simplificada (envelopes
  dimensionais). Geometria detalhada nas pranchas individuais."*

- [ ] **Step 4: Rodar os testes e confirmar que passam**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_gen_assembly -v`
Expected: PASS — 7 testes.

- [ ] **Step 5: Gerar e inspecionar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 gen_assembly_drawings.py && python3 verify_drawings.py
```

Abra `build/drawings/A2-vista-explodida.png`. Confirme: sólidos legíveis e não
sobrepostos, cada balão ligado ao seu sólido sem cruzar outros, lista de peças
completa e alinhada, trajetórias de montagem claras, nota da representação
simplificada visível.

- [ ] **Step 6: Rodar a suíte inteira e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest discover -s tests -v
cd /home/swampert/dev/PID-CEUB-2026
git add tools/gen_assembly_drawings.py tools/tests/test_gen_assembly.py hardware/drawings/assy/A2-vista-explodida.svg
git commit -m "feat(assy): A2 exploded view with balloons and parts list"
```

---

### Task 12: Registro de divergências e fechamento

**Files:**
- Create: `docs/DIVERGENCIAS.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: os achados confirmados nas Tasks 6, 7 e 10.
- Produces: nada consumido por outra task.

- [ ] **Step 1: Escrever `docs/DIVERGENCIAS.md`**

Um registro em tabela com as colunas: identificador, arquivo(s) afetado(s),
descrição da divergência, evidência, decisão tomada e situação. As seis entradas
vêm do spec (D1 a D6), atualizadas com o que as Tasks 6, 7 e 10 confirmaram:

| ID | Situação a registrar |
|----|----------------------|
| D1 | **Resolvido** — `bom.csv`, `CLAUDE.md` e `README.md` corrigidos para QTR-8A no commit `cf16a1c`. `docs/RELATORIO_FINAL.md` ainda cita TCRT5000 (linhas 35 e 261) e foi deixado como registro histórico datado |
| D2 | **Aberto** — `sensor_bar.scad` modela pockets para TCRT5000 discretos, incompatível com a PCB única de 74,93 × 12,70 mm. Remodelagem é trabalho posterior |
| D3 | **Aberto** — `body_f1.scad` com motores em Y=170 e barra de sensores em Y=177. A prancha A1 define o layout correto |
| D4 | Registrar o desfecho da Task 7: qual diâmetro de caster prevaleceu e por quê |
| D5 | **Aceito** — `SENSOR_SPACING_MM 10` mantido no firmware; ficha 03 e prancha A1 usam 9,525 mm |
| D6 | Registrar os códigos reais do N20 e do ball caster confirmados na Task 6 e na Task 7 |

Acrescente também qualquer divergência nova encontrada durante a execução —
por exemplo, se a massa do LiPo teve que ser estimada na Task 7.

- [ ] **Step 2: Verificar que o D1 está mesmo fechado**

```bash
cd /home/swampert/dev/PID-CEUB-2026
grep -rn -i "tcrt5000" --include="*.md" --include="*.csv" . | grep -v superpowers | grep -v DIVERGENCIAS
```

Expected: apenas as duas linhas de `docs/RELATORIO_FINAL.md`. Se aparecer
qualquer outro arquivo, corrija-o antes de prosseguir.

- [ ] **Step 3: Apontar o README para os novos artefatos**

Acrescente uma seção ao `README.md`, após "Chassi 3D", remetendo a
`hardware/components/fichas/`, `hardware/drawings/comp/`,
`hardware/drawings/assy/` e `docs/DIVERGENCIAS.md`, e explicando que as fichas
são geradas por `tools/gen_fichas.py` e não devem ser editadas à mão.

- [ ] **Step 4: Rodar a suíte inteira uma última vez**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest discover -s tests -v`
Expected: PASS — todas as suítes.

- [ ] **Step 5: Regenerar tudo do zero e conferir que nada mudou**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools
python3 gen_fichas.py && python3 gen_component_drawings.py && python3 gen_assembly_drawings.py
cd /home/swampert/dev/PID-CEUB-2026 && git status --short
```

Expected: nenhuma modificação. Se algum arquivo aparecer como alterado, o
gerador não é determinístico — investigue antes de commitar.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add docs/DIVERGENCIAS.md README.md
git commit -m "docs: register repository divergences and link drawing package"
```

---

## Portão de saída do plano

O pacote dimensional está pronto quando:

- [ ] Suíte inteira passando: `python3 -m unittest discover -s tests`
- [ ] 10 fichas geradas, cada cota com proveniência, cada `OFICIAL` com URL
- [ ] 12 pranchas SVG geradas, **todas inspecionadas visualmente em PNG**
- [ ] A1 com CG dentro do triângulo de apoio e margem declarada na prancha
- [ ] Envelope de competição 250×250 mm verificado em A1
- [ ] `docs/DIVERGENCIAS.md` com as seis entradas e situação de cada uma
- [ ] Regeneração do zero não produz diferença no `git status`

O plano da monografia ABNT (`2026-08-12-monografia-abnt.md`) consome estes
artefatos nos Apêndices C e D e só deve ser executado depois deste.
