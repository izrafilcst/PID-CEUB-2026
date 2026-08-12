# Monografia ABNT — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Produzir uma monografia acadêmica em conformidade ABNT sobre o projeto LFR ESP32, cobrindo funcionamento, arquitetura e funções do firmware da branch `main`, exportável a PDF pelo navegador.

**Architecture:** Documento HTML único com CSS de mídia paginada. `paged.polyfill.js`, vendorizado localmente, implementa `@page`, `target-counter()` e caixas de margem — o que dá sumário, listas e paginação automáticos sem toolchain LaTeX. Um linter em Python confere a estrutura verificável no código-fonte HTML.

**Tech Stack:** HTML5, CSS Paged Media, Paged.js (vendorizado), Python 3.12 stdlib para o linter.

**Spec:** `docs/superpowers/specs/2026-08-12-documentacao-componentes-abnt-design.md`
**Pré-requisito:** `docs/superpowers/plans/2026-08-12-pacote-dimensional-componentes.md` executado — os Apêndices C e D consomem as pranchas e fichas.

## Global Constraints

- **Idioma**: português do Brasil. Exceção: o `Abstract`, obrigatório pela NBR 14724.
- **Papel A4.** Margens: esquerda 3 cm, superior 3 cm, direita 2 cm, inferior 2 cm.
- **Corpo** em 12 pt, entrelinha 1,5, justificado, recuo de primeira linha 1,25 cm.
- **Citações longas** (mais de 3 linhas), legendas, fontes, notas de rodapé e referências: 10 pt, espaçamento simples. Citação longa com recuo de 4 cm da margem esquerda.
- **Paginação**: algarismos arábicos, canto superior direito, a 2 cm da borda superior. Contagem inicia na folha de rosto; exibição inicia na Introdução. A capa não é contada.
- **Seção primária** sempre em página nova, título em caixa alta e negrito.
- **Numeração progressiva** (NBR 6024): sem ponto após o último algarismo, alinhada à esquerda, até a seção quinária.
- **Figuras**: legenda acima no formato `Figura N – Título`, fonte abaixo. Centralizadas.
- **Quadros** (código-fonte e conteúdo textual esquematizado): bordas fechadas, legenda acima `Quadro N – Título`, fonte abaixo, conteúdo monoespaçado 10 pt.
- **Tabelas** (padrão IBGE): sem bordas laterais, apenas traços horizontais de topo, cabeçalho e base. Legenda acima `Tabela N – Título`, fonte abaixo.
- **Referências** conforme NBR 6023, ordenadas alfabeticamente, alinhadas à esquerda (sem justificar), separadas entre si por uma linha em branco simples.
- **Citações** conforme NBR 10520: sistema autor-data.
- **Sem recurso externo**: nada de CDN, fonte remota ou script de rede. O documento tem que abrir e imprimir offline.
- **Nenhum resultado não executado.** Número de teste, tempo de laço ou métrica só entra no documento se tiver sido obtido por execução registrada nesta sessão. O que não puder ser executado é declarado como não verificado.

---

## Estrutura de arquivos

| Arquivo | Responsabilidade |
|---------|------------------|
| `docs/abnt/monografia.html` | Documento completo |
| `docs/abnt/abnt.css` | Formatação NBR 14724 e mídia paginada |
| `docs/abnt/vendor/paged.polyfill.js` | Polyfill de CSS Paged Media (vendorizado) |
| `docs/abnt/figuras/` | Cópias ou links relativos das pranchas usadas como figura |
| `tools/lint_abnt.py` | Linter estrutural |
| `tools/tests/test_lint_abnt.py` | Testes do linter |

---

### Task 1: Esqueleto paginado

**Files:**
- Create: `docs/abnt/abnt.css`
- Create: `docs/abnt/monografia.html`
- Create: `docs/abnt/vendor/paged.polyfill.js`

**Interfaces:**
- Consumes: nada.
- Produces: as classes CSS que todas as tasks seguintes usam — `.capa`, `.pretextual`, `.textual`, `.postextual`, `.sumario`, `.figura`, `.quadro`, `.tabela`, `.citacao-longa`, `.referencias`, e os elementos `h1`–`h5` com numeração automática.

- [ ] **Step 1: Vendorizar o Paged.js**

```bash
mkdir -p /home/swampert/dev/PID-CEUB-2026/docs/abnt/vendor
cd /home/swampert/dev/PID-CEUB-2026/docs/abnt/vendor
python3 - <<'PY'
import urllib.request
url = "https://unpkg.com/pagedjs/dist/paged.polyfill.js"
urllib.request.urlretrieve(url, "paged.polyfill.js")
import os
print("bytes:", os.path.getsize("paged.polyfill.js"))
PY
```

Expected: arquivo de algumas centenas de kB. Confirme que começa com código
JavaScript e não com uma página de erro HTML:

```bash
head -c 200 /home/swampert/dev/PID-CEUB-2026/docs/abnt/vendor/paged.polyfill.js
```

> Se o download falhar, o documento ainda imprime com `@page` puro — perde
> apenas os números do sumário e das listas. Nesse caso, registre a limitação e
> siga; **não** substitua por um CDN, que quebra o requisito de operação offline.

- [ ] **Step 2: Escrever `abnt.css`**

```css
/* abnt.css — formatação conforme ABNT NBR 14724, 6024, 6027, 6023, 10520.
   Mídia paginada via Paged.js. Nenhum recurso externo. */

/* ═══ Página (NBR 14724) ═══════════════════════════════════════════════
   Margens: superior 3, direita 2, inferior 2, esquerda 3 (cm).           */
@page {
    size: A4;
    margin: 3cm 2cm 2cm 3cm;

    @top-right {
        content: counter(page);
        font-family: "Times New Roman", "Liberation Serif", serif;
        font-size: 10pt;
        margin-top: 1cm;   /* número a 2 cm da borda superior */
    }
}

/* A capa não é contada nem numerada. */
@page capa {
    @top-right { content: none; }
}

/* Elementos pré-textuais: contados, não numerados. */
@page pretextual {
    @top-right { content: none; }
}

/* ═══ Tipografia base ═════════════════════════════════════════════════ */
html {
    font-family: "Times New Roman", "Liberation Serif", "Nimbus Roman", serif;
    font-size: 12pt;
}

body {
    line-height: 1.5;
    text-align: justify;
    hyphens: auto;
    color: #000;
    background: #fff;
}

p {
    text-indent: 1.25cm;
    margin: 0;
    orphans: 2;
    widows: 2;
}

/* Primeiro parágrafo após título não recua. */
h1 + p, h2 + p, h3 + p, h4 + p, h5 + p { text-indent: 0; }

/* ═══ Seções (NBR 6024) ═══════════════════════════════════════════════ */
.textual { counter-reset: sec; }

.textual h1 {
    counter-reset: sub;
    counter-increment: sec;
    font-size: 12pt;
    font-weight: bold;
    text-transform: uppercase;
    text-align: left;
    text-indent: 0;
    margin: 0 0 1.5em 0;
    break-before: page;
}
.textual h1::before { content: counter(sec) " "; }

.textual h2 {
    counter-reset: subsub;
    counter-increment: sub;
    font-size: 12pt;
    font-weight: bold;
    text-align: left;
    text-indent: 0;
    margin: 1.5em 0 1em 0;
    break-after: avoid;
}
.textual h2::before { content: counter(sec) "." counter(sub) " "; }

.textual h3 {
    counter-increment: subsub;
    font-size: 12pt;
    font-weight: normal;
    font-style: italic;
    text-align: left;
    text-indent: 0;
    margin: 1.2em 0 0.8em 0;
    break-after: avoid;
}
.textual h3::before {
    content: counter(sec) "." counter(sub) "." counter(subsub) " ";
}

/* Seções sem numeração: resumo, sumário, referências, apêndices. */
.sem-numero h1 {
    text-align: center;
    break-before: page;
}
.sem-numero h1::before { content: none; }

/* ═══ Capa e pré-textuais ═════════════════════════════════════════════ */
.capa {
    page: capa;
    text-align: center;
    text-indent: 0;
    break-after: page;
}
.capa p { text-indent: 0; text-align: center; }

.pretextual { page: pretextual; }

.pendente {
    background: #ffff00;
    font-weight: bold;
}

/* ═══ Sumário e listas (NBR 6027) ═════════════════════════════════════ */
.sumario ol { list-style: none; padding-left: 0; }
.sumario li { text-indent: 0; margin-bottom: 0.3em; }
.sumario a {
    text-decoration: none;
    color: #000;
    display: block;
}
/* Preenche com pontos e puxa o número da página do alvo. */
.sumario a::after {
    content: " " leader('.') " " target-counter(attr(href url), page);
}

/* ═══ Figuras ═════════════════════════════════════════════════════════ */
.figura {
    text-align: center;
    text-indent: 0;
    margin: 1.5em 0;
    break-inside: avoid;
}
.figura img, .figura svg {
    max-width: 100%;
    height: auto;
}
.figura .legenda {
    font-size: 10pt;
    line-height: 1;
    text-indent: 0;
    text-align: center;
    margin-bottom: 0.4em;
}
.figura .fonte {
    font-size: 10pt;
    line-height: 1;
    text-indent: 0;
    text-align: center;
    margin-top: 0.4em;
}

/* Prancha em página inteira, deitada (Apêndice C). */
.prancha-cheia {
    break-before: page;
    break-after: page;
}

/* ═══ Quadros (código-fonte) ══════════════════════════════════════════ */
.quadro {
    margin: 1.5em 0;
    text-indent: 0;
    break-inside: avoid;
}
.quadro .legenda,
.quadro .fonte {
    font-size: 10pt;
    line-height: 1;
    text-indent: 0;
    text-align: center;
}
.quadro pre {
    border: 1px solid #000;
    padding: 0.5em;
    font-family: "DejaVu Sans Mono", "Courier New", monospace;
    font-size: 10pt;
    line-height: 1.15;
    white-space: pre-wrap;
    overflow-wrap: break-word;
    text-align: left;
}

/* ═══ Tabelas (padrão IBGE — sem bordas laterais) ═════════════════════ */
.tabela { margin: 1.5em 0; text-indent: 0; break-inside: avoid; }
.tabela .legenda,
.tabela .fonte {
    font-size: 10pt;
    line-height: 1;
    text-indent: 0;
    text-align: center;
}
.tabela table {
    border-collapse: collapse;
    width: 100%;
    font-size: 10pt;
    line-height: 1.15;
}
.tabela th, .tabela td {
    padding: 0.3em 0.5em;
    text-align: left;
    border: none;
}
.tabela thead tr:first-child th { border-top: 1px solid #000; }
.tabela thead tr:last-child th  { border-bottom: 1px solid #000; }
.tabela tbody tr:last-child td  { border-bottom: 1px solid #000; }

/* ═══ Citações (NBR 10520) ════════════════════════════════════════════ */
.citacao-longa {
    margin-left: 4cm;
    font-size: 10pt;
    line-height: 1;
    text-indent: 0;
    text-align: justify;
}

/* ═══ Referências (NBR 6023) ══════════════════════════════════════════ */
.referencias p {
    text-indent: 0;
    text-align: left;
    line-height: 1;
    margin-bottom: 1em;   /* separadas por espaço simples */
}

/* ═══ Tela: aproxima a folha do resultado impresso ════════════════════ */
@media screen {
    body { background: #999; }
    .pagedjs_page { background: #fff; margin: 8px auto; box-shadow: 0 0 6px #0006; }
}
```

- [ ] **Step 3: Escrever o esqueleto de `monografia.html`**

```html
<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>Robô Seguidor de Linha Competitivo com ESP32</title>
<link rel="stylesheet" href="abnt.css">
</head>
<body>

<section class="capa">
  <p>CENTRO UNIVERSITÁRIO DE BRASÍLIA</p>
  <p class="pendente">[CURSO — preencher]</p>
  <p class="pendente">[NOME COMPLETO DO AUTOR — preencher]</p>
  <p>ROBÔ SEGUIDOR DE LINHA COMPETITIVO COM ESP32:<br>
     projeto, firmware e verificação</p>
  <p>BRASÍLIA<br>2026</p>
</section>

<section class="pretextual" id="pretextual">
  <!-- folha de rosto, folha de aprovação, resumo, abstract,
       listas e sumário entram na Task 3 -->
</section>

<section class="textual" id="textual">
  <!-- capítulos 1 a 6 entram nas Tasks 4 a 8 -->
</section>

<section class="postextual" id="postextual">
  <!-- referências e apêndices entram nas Tasks 9 e 10 -->
</section>

<script src="vendor/paged.polyfill.js"></script>
</body>
</html>
```

- [ ] **Step 4: Verificar que pagina**

Sirva por HTTP — `file://` pode bloquear o polyfill:

```bash
cd /home/swampert/dev/PID-CEUB-2026/docs/abnt && python3 -m http.server 8770
```

Abra `http://localhost:8770/monografia.html` no Chrome. Confirme:

- [ ] A página aparece como folha A4 branca sobre fundo cinza (o Paged.js rodou)
- [ ] A capa não exibe número de página
- [ ] Sem erro no console do navegador
- [ ] Ctrl+P mostra pré-visualização com as margens corretas

> Se a folha **não** aparecer paginada, o polyfill não carregou. Cheque o
> console. Sem ele, o sumário automático da Task 3 não funciona e você precisa
> registrar a limitação no fechamento.

- [ ] **Step 5: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/
git commit -m "feat(abnt): paged skeleton with NBR 14724 stylesheet and vendored Paged.js"
```

---

### Task 2: Linter estrutural

**Files:**
- Create: `tools/lint_abnt.py`
- Test: `tools/tests/test_lint_abnt.py`

**Interfaces:**
- Consumes: `html.parser` da stdlib.
- Produces: `Achado(regra, detalhe)`, `checar_secoes(html) -> list[Achado]`, `checar_figuras(html) -> list[Achado]`, `checar_referencias_cruzadas(html) -> list[Achado]`, `checar_siglas(html) -> list[Achado]`, `checar_referencias(html) -> list[Achado]`, `checar_pendencias(html) -> list[Achado]`, `lint(html) -> list[Achado]`, `main(argv) -> int`.

- [ ] **Step 1: Escrever o teste que falha**

```python
# tools/tests/test_lint_abnt.py
import unittest

from lint_abnt import (checar_figuras, checar_pendencias,
                       checar_referencias_cruzadas, checar_secoes, lint)


class TestSecoes(unittest.TestCase):
    def test_sequencia_correta_nao_reporta(self):
        html = ('<section class="textual"><h1>INTRODUÇÃO</h1>'
                '<h2>Contextualização</h2><h2>Problema</h2>'
                '<h1>FUNDAMENTAÇÃO</h1></section>')
        self.assertEqual(checar_secoes(html), [])

    def test_salto_de_nivel_reporta(self):
        # h1 seguido de h3 pula a subseção intermediária
        html = '<section class="textual"><h1>A</h1><h3>B</h3></section>'
        achados = checar_secoes(html)
        self.assertEqual(len(achados), 1)
        self.assertIn("salto", achados[0].regra)


class TestFiguras(unittest.TestCase):
    def test_figura_completa_nao_reporta(self):
        html = ('<div class="figura"><p class="legenda">Figura 1 – X</p>'
                '<img src="a.svg"><p class="fonte">Fonte: o autor (2026).</p>'
                '</div>')
        self.assertEqual(checar_figuras(html), [])

    def test_figura_sem_fonte_reporta(self):
        html = ('<div class="figura"><p class="legenda">Figura 1 – X</p>'
                '<img src="a.svg"></div>')
        achados = checar_figuras(html)
        self.assertEqual(len(achados), 1)
        self.assertIn("fonte", achados[0].regra)

    def test_figura_sem_legenda_reporta(self):
        html = ('<div class="figura"><img src="a.svg">'
                '<p class="fonte">Fonte: o autor (2026).</p></div>')
        self.assertEqual(len(checar_figuras(html)), 1)

    def test_numeracao_de_figuras_deve_ser_sequencial(self):
        html = ('<div class="figura"><p class="legenda">Figura 1 – A</p>'
                '<p class="fonte">F</p></div>'
                '<div class="figura"><p class="legenda">Figura 3 – B</p>'
                '<p class="fonte">F</p></div>')
        achados = checar_figuras(html)
        self.assertTrue(any("sequencial" in a.regra for a in achados))


class TestReferenciasCruzadas(unittest.TestCase):
    def test_citacao_de_figura_existente_nao_reporta(self):
        html = ('<p>conforme a Figura 1</p>'
                '<div class="figura"><p class="legenda">Figura 1 – A</p>'
                '<p class="fonte">F</p></div>')
        self.assertEqual(checar_referencias_cruzadas(html), [])

    def test_citacao_de_figura_inexistente_reporta(self):
        html = '<p>conforme a Figura 9</p>'
        achados = checar_referencias_cruzadas(html)
        self.assertEqual(len(achados), 1)
        self.assertIn("9", achados[0].detalhe)


class TestPendencias(unittest.TestCase):
    def test_marcador_pendente_reporta(self):
        html = '<p class="pendente">[NOME — preencher]</p>'
        self.assertEqual(len(checar_pendencias(html)), 1)

    def test_documento_sem_pendencia_nao_reporta(self):
        self.assertEqual(checar_pendencias("<p>texto</p>"), [])


class TestLintCompleto(unittest.TestCase):
    def test_agrega_todos_os_achados(self):
        html = ('<section class="textual"><h1>A</h1><h3>B</h3></section>'
                '<p class="pendente">[X]</p>')
        self.assertGreaterEqual(len(lint(html)), 2)


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Rodar o teste e confirmar que falha**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_lint_abnt -v`
Expected: FAIL com `ModuleNotFoundError: No module named 'lint_abnt'`

- [ ] **Step 3: Implementar**

Base do módulo, com duas checagens completas como modelo. Use `re` e
`html.parser` da stdlib — nenhuma dependência nova.

```python
#!/usr/bin/env python3
"""Linter estrutural da monografia ABNT.

Cobre o que é verificável no código-fonte HTML. NÃO verifica paginação —
isso exige renderização e fica com a conferência humana.

Uso:
    python3 lint_abnt.py ../docs/abnt/monografia.html
"""
from __future__ import annotations

import re
import sys
from dataclasses import dataclass

RE_TAG = re.compile(r"<[^>]+>")
RE_H = re.compile(r"<h([1-5])\b[^>]*>(.*?)</h\1>", re.S | re.I)
RE_BLOCO = re.compile(
    r'<(?:div|figure)[^>]*class="[^"]*\b(figura|quadro|tabela)\b[^"]*"[^>]*>'
    r"(.*?)</(?:div|figure)>", re.S | re.I)
RE_LEGENDA = re.compile(
    r'class="legenda"[^>]*>\s*(Figura|Quadro|Tabela)\s+(\d+)\s*[–-]', re.I)
RE_FONTE = re.compile(r'class="fonte"[^>]*>\s*Fonte\s*:', re.I)
RE_PENDENTE = re.compile(r'class="[^"]*\bpendente\b[^"]*"', re.I)
RE_MENCAO = re.compile(r"\b(Figura|Quadro|Tabela)\s+(\d+)\b")


@dataclass(frozen=True)
class Achado:
    """Uma violação encontrada."""

    regra: str
    detalhe: str

    def __str__(self) -> str:
        return f"[{self.regra}] {self.detalhe}"


def _texto(html: str) -> str:
    """Remove marcação, preservando o texto corrido."""
    return RE_TAG.sub(" ", html)


def checar_secoes(html: str) -> list[Achado]:
    """Níveis de título não podem saltar (h1 → h3)."""
    achados: list[Achado] = []
    anterior = 0
    for m in RE_H.finditer(html):
        nivel = int(m.group(1))
        titulo = _texto(m.group(2)).strip()
        if anterior and nivel > anterior + 1:
            achados.append(Achado(
                "salto-de-nivel",
                f"h{anterior} seguido de h{nivel} em {titulo!r}"))
        anterior = nivel
    return achados


def checar_figuras(html: str) -> list[Achado]:
    """Todo bloco tem legenda no formato certo, fonte, e numeração sequencial."""
    achados: list[Achado] = []
    contagem: dict[str, int] = {"figura": 0, "quadro": 0, "tabela": 0}
    esperado = {"figura": "Figura", "quadro": "Quadro", "tabela": "Tabela"}

    for m in RE_BLOCO.finditer(html):
        tipo, corpo = m.group(1).lower(), m.group(2)
        leg = RE_LEGENDA.search(corpo)
        if not leg:
            achados.append(Achado(
                "legenda-ausente",
                f"{tipo} sem legenda no formato '{esperado[tipo]} N – Título'"))
        else:
            if leg.group(1).lower() != tipo:
                achados.append(Achado(
                    "legenda-trocada",
                    f"bloco {tipo} rotulado como {leg.group(1)}"))
            contagem[tipo] += 1
            num = int(leg.group(2))
            if num != contagem[tipo]:
                achados.append(Achado(
                    "numeracao-nao-sequencial",
                    f"{esperado[tipo]} {num} onde se esperava "
                    f"{esperado[tipo]} {contagem[tipo]}"))
        if not RE_FONTE.search(corpo):
            achados.append(Achado(
                "fonte-ausente",
                f"{tipo} sem indicação de fonte"))
    return achados


def checar_referencias_cruzadas(html: str) -> list[Achado]:
    """Toda menção 'Figura N' no corpo aponta para um bloco existente."""
    existentes: set[tuple[str, int]] = set()
    for m in RE_BLOCO.finditer(html):
        leg = RE_LEGENDA.search(m.group(2))
        if leg:
            existentes.add((leg.group(1).lower(), int(leg.group(2))))

    achados: list[Achado] = []
    for m in RE_MENCAO.finditer(_texto(html)):
        chave = (m.group(1).lower(), int(m.group(2)))
        if chave not in existentes:
            achados.append(Achado(
                "referencia-orfa",
                f"texto cita {m.group(1)} {m.group(2)}, que não existe"))
    return achados


def checar_pendencias(html: str) -> list[Achado]:
    """Nenhum marcador de preenchimento pode sobrar."""
    n = len(RE_PENDENTE.findall(html))
    if n:
        return [Achado("pendencia",
                       f"{n} marcador(es) class=\"pendente\" por preencher")]
    return []


def checar_siglas(html: str) -> list[Achado]:
    """Toda sigla da Lista de Siglas é usada no corpo, e vice-versa."""
    raise NotImplementedError("implementar nesta task")


def checar_referencias(html: str) -> list[Achado]:
    """Todo item das Referências é citado, e toda citação está listada."""
    raise NotImplementedError("implementar nesta task")


def lint(html: str) -> list[Achado]:
    """Roda todas as checagens."""
    achados: list[Achado] = []
    for fn in (checar_secoes, checar_figuras, checar_referencias_cruzadas,
               checar_siglas, checar_referencias, checar_pendencias):
        try:
            achados.extend(fn(html))
        except NotImplementedError:
            continue
    return achados


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print("uso: lint_abnt.py <arquivo.html>", file=sys.stderr)
        return 2
    with open(argv[1], encoding="utf-8") as fh:
        achados = lint(fh.read())
    for a in achados:
        print(a)
    print(f"\n{len(achados)} achado(s)")
    return 1 if achados else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
```

Substitua os dois `raise NotImplementedError` por implementações reais antes de
encerrar a task — o `lint()` os ignora de propósito para você poder rodar as
outras checagens durante o desenvolvimento, mas um linter que pula regra em
silêncio no fechamento não serve. Regras completas:

| Função | Regra |
|--------|-------|
| `checar_secoes` | Níveis de título não podem saltar (h1→h3). Toda seção primária dentro de `.textual` abre com `h1` |
| `checar_figuras` | Todo `.figura` tem `.legenda` começando por `Figura N – ` e `.fonte` começando por `Fonte:`. Numeração sequencial a partir de 1. Mesma regra para `.quadro` (`Quadro N – `) e `.tabela` (`Tabela N – `) |
| `checar_referencias_cruzadas` | Toda menção `Figura N` / `Quadro N` / `Tabela N` no corpo corresponde a um elemento existente |
| `checar_siglas` | Toda sigla listada na Lista de Siglas aparece no corpo; toda sigla em caixa alta com 2+ letras usada no corpo consta da lista |
| `checar_referencias` | Todo item de `.referencias` é citado no corpo (autor-data); toda citação autor-data tem item correspondente |
| `checar_pendencias` | Nenhum elemento com classe `pendente` sobrou |

`main(argv)` recebe o caminho do HTML, imprime os achados agrupados por regra e
devolve `0` se limpo, `1` se houver achado.

- [ ] **Step 4: Rodar o teste e confirmar que passa**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest tests.test_lint_abnt -v`
Expected: PASS — 11 testes.

- [ ] **Step 5: Confirmar que nenhuma checagem ficou por implementar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && grep -n "NotImplementedError" lint_abnt.py
```

Expected: **nenhuma saída.** Se `checar_siglas` ou `checar_referencias` ainda
levantarem `NotImplementedError`, o `lint()` as pula em silêncio e o portão de
saída do plano fica falsamente verde.

- [ ] **Step 6: Rodar contra o esqueleto**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html`
Expected: reporta as pendências da capa (esperado nesta altura) e nada mais.

- [ ] **Step 7: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add tools/lint_abnt.py tools/tests/test_lint_abnt.py
git commit -m "feat(tools): ABNT structural linter"
```

---

### Task 3: Elementos pré-textuais

**Files:**
- Modify: `docs/abnt/monografia.html`

**Interfaces:**
- Consumes: classes de `abnt.css`.
- Produces: âncoras `id` de todas as seções, consumidas pelo sumário via `target-counter`.

- [ ] **Step 1: Escrever os elementos pré-textuais**

Na ordem exigida pela NBR 14724, dentro de `<section class="pretextual">`:

1. **Folha de rosto** — autor, título, natureza do trabalho (tipo, objetivo, instituição, área), orientador, local e ano. Campos desconhecidos com `class="pendente"`.
2. **Folha de aprovação** — banca com linhas de assinatura, data. Campos com `class="pendente"`.
3. **Resumo** (NBR 6028) — parágrafo único, sem recuo, 150 a 500 palavras, seguido de 3 a 5 palavras-chave. Conteúdo: objetivo, método (TDD + PID em cascata), resultados e conclusão.
4. **Abstract** — tradução fiel do resumo, com *keywords*.
5. **Lista de figuras** — `<ol>` com `target-counter`.
6. **Lista de quadros** — idem.
7. **Lista de tabelas** — idem.
8. **Lista de siglas** — ABNT, ADC, BLE, CG, DI, GPIO, IAE, ISE, ITAE, ISR, LEDC, LiPo, NVS, PID, PWM, RPM, SPI, SPSC, TDD.
9. **Lista de símbolos** — Kp, Ki, Kd, e(t), u(t), ω, Δt.
10. **Sumário** (NBR 6027) — `<nav class="sumario">` com um `<a href="#id">` por seção, indentado por nível.

O sumário usa a mesma numeração das seções e recebe os números de página do
`target-counter` no CSS. Escreva-o com todas as seções que os capítulos das
Tasks 4 a 10 vão criar; os `id` precisam bater.

- [ ] **Step 2: Verificar no navegador**

```bash
cd /home/swampert/dev/PID-CEUB-2026/docs/abnt && python3 -m http.server 8770
```

Abra `http://localhost:8770/monografia.html`. Confirme:

- [ ] Nenhuma página pré-textual exibe número
- [ ] O sumário mostra números de página reais, não vazios nem zeros
- [ ] Os links de sumário levam à seção certa
- [ ] Resumo e abstract sem recuo de primeira linha

> Sumário com números em branco significa que o `target-counter` não resolveu.
> Duas causas, nesta ordem de probabilidade: (a) o `id` do alvo não existe —
> confira cada `href`; (b) a sintaxe do argumento. O `abnt.css` usa
> `target-counter(attr(href url), page)`; se não resolver, troque por
> `target-counter(attr(href), page)`. As duas formas aparecem na documentação do
> Paged.js e o suporte varia com a versão que você baixou.

- [ ] **Step 3: Rodar o linter**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html`
Expected: reporta apenas as pendências de capa/folha de rosto/aprovação.

- [ ] **Step 4: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): pre-textual elements with auto-numbered summary"
```

---

### Task 4: Capítulos 1 e 2 — Introdução e Fundamentação Teórica

**Files:**
- Modify: `docs/abnt/monografia.html`

**Fontes a ler antes de escrever:** `CLAUDE.md`, `README.md`, `docs/TUNING.md`, `docs/RELATORIO_FINAL.md`, `src/config.h`.

- [ ] **Step 1: Escrever o capítulo 1**

| Seção | Conteúdo |
|-------|----------|
| 1.1 Contextualização | Competições brasileiras de seguidor de linha (RoboCore, IronCup, Salão de Robótica); o problema de velocidade sob precisão |
| 1.2 Problema | Como manter rastreamento estável da linha em velocidade alta, com o compromisso entre ganho proporcional e oscilação |
| 1.3 Objetivos | **Geral**: desenvolver firmware e projeto de hardware para um seguidor de linha competitivo. **Específicos**: sensoriamento com posição ponderada; controle PID com derivada no processo; controle em cascata de velocidade por encoder; sintonia em tempo real por BLE; verificação por testes automatizados; conformidade dimensional com o regulamento |
| 1.4 Justificativa | Por que ESP32 dual-core, por que TDD em firmware embarcado, por que sintonia sem recompilar |
| 1.5 Estrutura do trabalho | Um parágrafo por capítulo |

- [ ] **Step 2: Escrever o capítulo 2**

| Seção | Conteúdo mínimo |
|-------|-----------------|
| 2.1 Robôs seguidores de linha | Arquitetura sensor-controlador-atuador; posição ponderada como medida de erro |
| 2.2 Sensoriamento óptico refletivo | Par LED IR/fototransistor; reflexão diferencial branco/preto; necessidade de calibração min/max por canal; efeito da altura ao solo |
| 2.3 Controle PID | Formulação contínua e discreta; papel de cada termo; **derivada sobre a medição** e por que elimina o *derivative kick*; anti-windup por clamping |
| 2.4 Controle em cascata | Malha externa de posição e interna de velocidade; requisito de separação de banda; saturação e seu efeito sobre a malha externa |
| 2.5 Modulação PWM e ponte H | Frequência, resolução, modos forward/reverse/brake/coast do TB6612FNG |
| 2.6 Encoders em quadratura e odometria | Decodificação 4×; relação pulsos-RPM; filtragem IIR e o compromisso suavidade/atraso |
| 2.7 Sistemas embarcados de tempo real | Orçamento de laço; ISR; separação de tarefas por núcleo; seções críticas |
| 2.8 Desenvolvimento orientado a testes | Ciclo Red-Green-Refactor; injeção de dependências para testabilidade; compilação condicional para teste nativo |

Cada seção com pelo menos uma citação autor-data, e a referência
correspondente reservada para a Task 10. Fórmulas do PID e da cascata entram
como `Quadro`, com legenda e fonte.

- [ ] **Step 3: Rodar o linter**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html`
Expected: sem achados de figura, quadro ou referência cruzada nos capítulos 1 e 2.

- [ ] **Step 4: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): chapters 1 and 2 - introduction and theory"
```

---

### Task 5: Capítulo 3 — Materiais e Métodos

**Files:**
- Modify: `docs/abnt/monografia.html`

**Fontes a ler antes de escrever:** `hardware/components/fichas/*.md`, `hardware/bom.csv`, `docs/WIRING.md`, `docs/DIVERGENCIAS.md`, `platformio.ini`, `hardware/drawings/assy/A1-layout-funcional.svg`.

- [ ] **Step 1: Escrever o capítulo 3**

| Seção | Conteúdo |
|-------|----------|
| 3.1 Requisitos derivados do regulamento | Envelope 250×250×200 mm, linha branca 19±1 mm sobre manta preta, pista ~5×2,8 m, autonomia total. Tabela com requisito → decisão de projeto |
| 3.2 Componentes selecionados | Tabela-resumo dos 10 componentes com justificativa de escolha; remete ao Apêndice D para as cotas. Declara explicitamente os três níveis de proveniência |
| 3.3 Arquitetura elétrica | Cadeia de alimentação (LiPo → MP1584 → 5 V → ESP32 → 3V3); tabela de pinagem completa; **o alerta dos GPIO 36/39 sem pull-up interno** e por que isso destrói o PID se ignorado; divisor de tensão VBAT; consumo estimado e autonomia |
| 3.4 Arquitetura mecânica | Sistema de coordenadas do layout funcional; lookahead de 70 mm; balanço de CG e margem de estabilidade; figura da prancha A1. **Declara que o CG é indicativo enquanto houver massa em nível MEDIR** |
| 3.5 Ferramentas e ambiente | PlatformIO, Arduino framework, C++17, Unity, ambientes `native` e `esp32dev`, flags de compilação |
| 3.6 Metodologia TDD | Ciclo aplicado; ordem bottom-up; critério de "done" por módulo; papel do `NATIVE_BUILD` |

A prancha A1 entra como figura com legenda `Figura N – Layout funcional do
robô` e fonte `Fonte: elaborado pelo autor (2026).`

- [ ] **Step 2: Verificar que a figura aparece no PDF**

Sirva e imprima em pré-visualização. Confirme que o SVG da A1 **renderiza**, não
aparece como ícone quebrado. Se quebrar, copie a prancha para
`docs/abnt/figuras/` e ajuste o `src` — caminho relativo atravessando `../../`
às vezes falha na impressão.

- [ ] **Step 3: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html docs/abnt/figuras/
git commit -m "feat(abnt): chapter 3 - materials and methods"
```

---

### Task 6: Capítulo 4, seções 4.1 a 4.5

**Files:**
- Modify: `docs/abnt/monografia.html`

**Fontes a ler integralmente antes de escrever:** `src/config.h`, `src/sensors/SensorArray.{h,cpp}`, `src/sensors/Calibration.{h,cpp}`, `src/sensors/Encoder.{h,cpp}`, `src/sensors/VelocityEstimator.{h,cpp}`, `src/control/PIDController.{h,cpp}`, `src/control/SpeedProfile.{h,cpp}`, `src/control/VelocityProfile.{h,cpp}`, `src/control/CascadeController.{h,cpp}`.

- [ ] **Step 1: Escrever as seções 4.1 a 4.5**

| Seção | Módulos | Pontos que o texto tem que explicar |
|-------|---------|-------------------------------------|
| 4.1 Visão geral em camadas | — | Diagrama das camadas aquisição → controle → atuação, com estratégia, comunicação e persistência ao lado; fluxo de dados do sensor ao PWM |
| 4.2 Configuração | `config.h` | Papel do arquivo; agrupamento de pinagem, ganhos, limites e temporização; `RPM_HARD_CEILING` como proteção contra comando fora da faixa física |
| 4.3 Aquisição | `SensorArray`, `Calibration` | Leitura SPI do MCP3008 (`readAll`, `_readChannel`); `Calibration::update` aprendendo min/max por canal; `normalize` levando a 0–1000; `weightedPosition` produzindo −3500 a +3500; detecção de linha perdida (`isLineLost`) e por que a polaridade branco/preto se resolve sozinha na calibração |
| 4.4 Odometria | `Encoder`, `VelocityEstimator` | Decodificação em quadratura 4× por ISR em `CHANGE` nos dois canais; `_applyTransition` e a tabela de estados; `getDelta` zerando o acumulador; conversão pulsos→RPM em `update`; filtro IIR com `VELOCITY_FILTER_ALPHA` e o compromisso suavidade/atraso; auto-calibração de PPR (`startCalibration`/`finishCalibration`) e persistência |
| 4.5 Controle | `PIDController`, `SpeedProfile`, `VelocityProfile`, `CascadeController` | Sobrecarga de `compute` com e sem `dt`; derivada sobre a medição; anti-windup por clamping da integral; `SpeedProfile::compute` interpolando erro→velocidade; `VelocityProfile` com buffer circular de 8 amostras antecipando curva via `getCurvatureScore`; `CascadeController::compute` distribuindo correção e RPM alvo entre as duas malhas internas, com saturação em `setMaxRpm`/`setMaxPwm` |

Cada módulo com: propósito, interface pública, decisão de projeto e um
`Quadro` com o trecho de código que sustenta a explicação. Trecho curto, com
legenda `Quadro N – <arquivo>: <o que mostra>` e fonte
`Fonte: código-fonte do projeto (2026).`

- [ ] **Step 2: Conferir que cada assinatura citada existe**

```bash
cd /home/swampert/dev/PID-CEUB-2026
for m in compute reset setTunings setOutputLimits update normalize weightedPosition \
         getCount getDelta getRPM getEffectivePPR startCalibration finishCalibration \
         computeSpeed getCurvatureScore setInnerGains setMaxRpm setMaxPwm; do
  printf "%-22s " "$m"; grep -rl "$m" src/ | tr '\n' ' '; echo
done
```

Toda função citada no texto tem que aparecer aqui. **Se o texto menciona uma
função que não aparece, corrija o texto — não invente API.**

- [ ] **Step 3: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): chapter 4.1-4.5 - acquisition, odometry, control"
```

---

### Task 7: Capítulo 4, seções 4.6 a 4.10

**Files:**
- Modify: `docs/abnt/monografia.html`

**Fontes a ler integralmente antes de escrever:** `src/motors/MotorDriver.{h,cpp}`, `src/motors/DifferentialDrive.{h,cpp}`, `src/strategy/LineFollower.{h,cpp}`, `src/strategy/LapTimer.{h,cpp}`, `src/comm/BLETuner.{h,cpp}`, `src/comm/Logger.{h,cpp}`, `src/storage/IPersistentStore.h`, `src/storage/NvsStore.{h,cpp}`, `src/storage/InMemoryStore.h`, `src/main.cpp`.

- [ ] **Step 1: Escrever as seções 4.6 a 4.10**

| Seção | Módulos | Pontos que o texto tem que explicar |
|-------|---------|-------------------------------------|
| 4.6 Atuação | `MotorDriver`, `DifferentialDrive` | `MotorDriver::Config` como injeção de pinagem; `setSpeed` aceitando −1023 a +1023 com o sinal escolhendo o sentido; `MotorDir` e os quatro modos do TB6612FNG; `standby`; PWM por LEDC a 20 kHz e 10 bits; `DifferentialDrive::compute` convertendo correção e velocidade base em PWM esquerdo/direito com saturação |
| 4.7 Estratégia | `LineFollower`, `LapTimer` | `LineFollower` como orquestrador injetado com `Calibration`, `PIDController`, `SpeedProfile` e `DifferentialDrive`; `update` executando o pipeline completo; `getLastCorrection` expondo a correção para a cascata; `LapTimer` com timestamps injetados — e por que isso o torna testável sem `millis()` |
| 4.8 Comunicação | `BLETuner`, `Logger` | Duas características BLE (telemetria por notify, comando por write); as estruturas `PIDParams`, `SpeedParams`, `VelocityPIDParams`, `RpmParams`, `CalibrationCmd`; o padrão consome-a-flag (`hasNewPID`/`getPID`, `consumeStart`/`consumeStop`/`consumeReset`); `Logger` como fila circular SPSC, `push` no produtor e `drain` no consumidor, `entryToCsv` |
| 4.9 Persistência | `IPersistentStore`, `NvsStore`, `InMemoryStore` | Interface abstrata permitindo NVS no alvo e dublê em memória no teste nativo; uso pelo `VelocityEstimator` para guardar o PPR calibrado |
| 4.10 Orquestração | `main.cpp` | Instanciação e injeção de todos os módulos; `RobotState` com `IDLE`, `CALIBRATING`, `READY`, `RUNNING`, `ERROR` e as transições; `std::atomic<RobotState>` e `portMUX_TYPE` protegendo o estado compartilhado entre núcleos; `taskComm` fixada no núcleo não-crítico enquanto o laço de controle roda no de tempo real; `TelemetrySnapshot` publicado sob seção crítica; `LOOP_PERIOD_US` de 2000 µs governando a cadência |

Inclua uma figura com o diagrama da máquina de estados e outra com o pipeline
de controle.

- [ ] **Step 2: Conferir os nomes citados**

```bash
cd /home/swampert/dev/PID-CEUB-2026
grep -nE "RobotState|taskComm|TelemetrySnapshot|portMUX|xTaskCreatePinnedToCore|LOOP_PERIOD_US" src/main.cpp | head -20
```

Confirme que os nomes usados no texto batem exatamente com o código.

- [ ] **Step 3: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): chapter 4.6-4.10 - actuation, strategy, comms, orchestration"
```

---

### Task 8: Capítulos 5 e 6 — Verificação, Resultados e Conclusão

**Files:**
- Modify: `docs/abnt/monografia.html`

> **Portão de honestidade desta task.** O `README.md` afirma "47 testes • 8
> suítes", mas `test/` tem **13 diretórios**. O número do README está
> desatualizado. **Nenhum número entra no capítulo 5 sem execução.**

- [ ] **Step 1: Tentar instalar o PlatformIO e executar a suíte**

```bash
pip install platformio 2>&1 | tail -3
cd /home/swampert/dev/PID-CEUB-2026 && pio test -e native 2>&1 | tail -40
```

Registre a saída literal. Três desfechos possíveis:

- **Instalou e a suíte passou** — use os números reais da saída no capítulo 5 e cite a saída num `Quadro`.
- **Instalou e a suíte falhou** — o capítulo 5 relata as falhas com a saída literal. Não maquie o resultado.
- **Não instalou** (sem rede ou sem permissão) — o capítulo 5 declara explicitamente que a suíte não pôde ser executada neste ambiente, lista as 13 suítes por inspeção de `test/`, e **não afirma nenhuma contagem de testes aprovados**.

- [ ] **Step 2: Escrever o capítulo 5**

| Seção | Conteúdo |
|-------|----------|
| 5.1 Suítes de teste | Tabela suíte → módulo coberto → número de casos, **preenchida a partir da execução ou marcada como não verificada**. Estratégia de teste: lógica pura em `native`, hardware em `esp32dev` |
| 5.2 Orçamento temporal do laço | `LOOP_PERIOD_US` de 2000 µs; decomposição do orçamento entre leitura SPI, PID e escrita de PWM; alvo de leitura do array em menos de 100 µs. Declare quais números são medidos e quais são de projeto |
| 5.3 Simulações | Resultados de `docs/simulations/pid_simulation.py` e `sensor_simulation.py`; as figuras `pid_response.png` e `sensor_linearity.png` já existem no repositório. Rode os scripts para confirmar que os gráficos correspondem ao código atual |
| 5.4 Calibração e sintonia | Procedimento de calibração por varredura; método de Ziegler-Nichols conforme `docs/TUNING.md`; papel do dashboard BLE |
| 5.5 Conformidade com o regulamento | Confronto do envelope do layout A1 com o limite de 250×250×200 mm; autonomia estimada contra a duração da prova |

- [ ] **Step 3: Escrever o capítulo 6**

- 6.1 Considerações finais — objetivos específicos do capítulo 1, um a um, com o que foi atingido e o que ficou pendente. **Inclua o que não foi verificado**: o robô não foi montado nem testado em pista, e as divergências abertas do `docs/DIVERGENCIAS.md` (D2, D3) permanecem.
- 6.2 Trabalhos futuros — remodelagem do chassi sobre o layout A1; sintonia em pista; detecção de cruzamento; mapeamento de pista por odometria.

- [ ] **Step 4: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): chapters 5 and 6 - verification, results, conclusion"
```

---

### Task 9: Apêndice A — Referência de API

**Files:**
- Modify: `docs/abnt/monografia.html`

**Escopo confirmado por inspeção dos headers** — 17 unidades, aproximadamente
85 funções públicas:

| Grupo | Unidades |
|-------|----------|
| Controle | `PIDController`, `CascadeController`, `SpeedProfile`, `VelocityProfile` |
| Sensores | `SensorArray`, `Calibration`, `Encoder`, `VelocityEstimator` |
| Motores | `MotorDriver` (+ `MotorId`, `MotorDir`, `MotorDriver::Config`), `DifferentialDrive` |
| Estratégia | `LineFollower`, `LapTimer` |
| Comunicação | `BLETuner` (+ `PIDParams`, `SpeedParams`, `VelocityPIDParams`, `RpmParams`, `CalibrationCmd`), `Logger` (+ `LogEntry`) |
| Persistência | `IPersistentStore`, `NvsStore`, `InMemoryStore` |

- [ ] **Step 1: Extrair a superfície pública**

```bash
cd /home/swampert/dev/PID-CEUB-2026
for f in $(find src -name "*.h" | sort); do
  echo "═══ $f"
  sed -n '/public:/,/private:/p' "$f"
done > /tmp/api_surface.txt
wc -l /tmp/api_surface.txt
```

Use esta saída como fonte. Para pré-condições e efeitos colaterais, leia o
`.cpp` correspondente — a assinatura sozinha não os revela.

- [ ] **Step 2: Escrever o Apêndice A**

Uma subseção por unidade, cada uma com uma tabela de colunas:

| Coluna | Conteúdo |
|--------|----------|
| Assinatura | Exata, como no header |
| Parâmetros | Nome, tipo, unidade e faixa válida |
| Retorno | Tipo, unidade e faixa |
| Pré-condições | O que precisa ser verdade antes da chamada (por exemplo, `begin()` chamado antes) |
| Efeitos colaterais | Estado interno alterado, escrita em hardware, consumo de flag |

Registre em particular:
- `PIDController::compute` tem duas sobrecargas — documente ambas e a diferença de `dt`
- `Encoder::getDelta` **zera** o acumulador interno; `getCount` não
- `BLETuner::getPID`, `getSpeed`, `consumeStart`, `consumeStop`, `consumeReset` **consomem** a flag
- `Logger::drain` recebe ponteiro de função e esvazia a fila
- `MotorDriver::setSpeed` aceita PWM com sinal, de −1023 a +1023

- [ ] **Step 3: Conferir a cobertura**

```bash
cd /home/swampert/dev/PID-CEUB-2026
python3 - <<'PY'
import re, pathlib
faltando = []
for h in sorted(pathlib.Path("src").rglob("*.h")):
    txt = h.read_text(encoding="utf-8")
    pub = re.search(r"public:(.*?)(private:|protected:|\Z)", txt, re.S)
    if not pub:
        continue
    for m in re.finditer(r"\b(\w+)\s*\(", pub.group(1)):
        nome = m.group(1)
        if nome in ("if", "for", "while", "return", "switch", "sizeof"):
            continue
        faltando.append((str(h), nome))
print(f"{len(faltando)} símbolos públicos a conferir no Apêndice A")
for f, n in faltando:
    print(f"  {f}: {n}")
PY
```

Confronte a lista com o apêndice. **Todo símbolo desta saída tem que estar
tabelado.** Se algum não estiver, acrescente.

- [ ] **Step 4: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): appendix A - complete API reference"
```

---

### Task 10: Referências e Apêndices B, C, D

**Files:**
- Modify: `docs/abnt/monografia.html`

- [ ] **Step 1: Escrever as Referências (NBR 6023)**

Ordem alfabética por sobrenome do autor ou pela entidade. Duas origens:

1. **Datasheets** — as strings `ref_abnt` já prontas em
   `hardware/components/specs.py`. Extraia com:

```bash
cd /home/swampert/dev/PID-CEUB-2026/hardware/components
python3 -c "
import sys; sys.path.insert(0, '../../tools'); sys.path.insert(0, '.')
from specs import COMPONENTES
for c in sorted(COMPONENTES.values(), key=lambda x: x.num):
    if c.ref_abnt: print(c.ref_abnt, '\n')
"
```

2. **Bibliografia técnica** — as obras citadas nos capítulos 1 e 2, mais os
   links de referência já listados em `CLAUDE.md` e `README.md` (Semreh V2,
   regulamento RoboCore, guia de sintonia Zbotic, NimBLE-Arduino, PlatformIO).

Formato de documento eletrônico: `AUTOR. Título. Local: Editora, ano.
Disponível em: <URL>. Acesso em: DD mmm. AAAA.`

- [ ] **Step 2: Apêndice B — Pinagem completa**

Tabela consolidada de todos os GPIO usados, com função, periférico e
observação, a partir de `src/config.h` e `docs/WIRING.md`. Inclua a advertência
sobre os GPIO 36 e 39 serem input-only sem pull-up interno.

Confira que a tabela bate com o código:

```bash
cd /home/swampert/dev/PID-CEUB-2026 && grep -nE "^#define PIN_" src/config.h
```

- [ ] **Step 3: Apêndice C — Pranchas de desenho técnico**

As 12 pranchas, uma por página, em `<div class="figura prancha-cheia">`. Como
as pranchas são A4 paisagem e a página é retrato, use `transform: rotate(90deg)`
com `transform-origin` ajustado, ou reduza para caber na largura. **Verifique na
pré-visualização de impressão qual das duas fica legível** — prancha ilegível
não serve de apêndice.

Cada uma com legenda `Figura N – <nome da peça>` e fonte
`Fonte: elaborado pelo autor (2026).`

- [ ] **Step 4: Apêndice D — Fichas dimensionais**

As 10 fichas de `hardware/components/fichas/*.md` convertidas para HTML, com as
tabelas no padrão `.tabela` e a legenda de proveniência preservada. Uma
subseção por componente.

- [ ] **Step 5: Rodar o linter e commitar**

```bash
cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html
git commit -m "feat(abnt): references and appendices B, C, D"
```

---

### Task 11: Revisão final e entrega

**Files:**
- Modify: `docs/abnt/monografia.html`
- Modify: `README.md`

- [ ] **Step 1: Rodar o linter completo**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 lint_abnt.py ../docs/abnt/monografia.html`
Expected: **zero achados**, incluindo zero pendências — o que exige os dados de
capa preenchidos. Se ainda faltarem, o documento não está entregável; peça os
dados antes de declarar concluído.

- [ ] **Step 2: Rodar toda a suíte de ferramentas**

Run: `cd /home/swampert/dev/PID-CEUB-2026/tools && python3 -m unittest discover -s tests -v`
Expected: PASS em todas as suítes dos dois planos.

- [ ] **Step 3: Conferir a paginação na pré-visualização**

```bash
cd /home/swampert/dev/PID-CEUB-2026/docs/abnt && python3 -m http.server 8770
```

Abra `http://localhost:8770/monografia.html`, dê Ctrl+P e percorra **todas** as
páginas. Checklist:

- [ ] Margens conferem: 3 cm à esquerda e no topo, 2 cm à direita e embaixo
- [ ] Números de página no canto superior direito, ausentes na capa e nos pré-textuais, presentes a partir da Introdução
- [ ] Sumário com números de página corretos e correspondentes
- [ ] Listas de figuras, quadros e tabelas com números corretos
- [ ] Toda seção primária abre em página nova
- [ ] Nenhuma figura, quadro ou tabela partida entre páginas
- [ ] Pranchas do Apêndice C legíveis
- [ ] Nenhuma linha órfã ou viúva evidente
- [ ] Referências alinhadas à esquerda, não justificadas

> Esta conferência é **humana e obrigatória**. Não há navegador headless neste
> ambiente; o linter não vê paginação. Um agente executando este plano deve
> **parar aqui e pedir a conferência ao autor**, sem declarar o trabalho
> concluído por conta própria.

- [ ] **Step 4: Exportar o PDF**

No diálogo de impressão: destino "Salvar como PDF", tamanho A4, margens
"Padrão", **desmarcar** cabeçalhos e rodapés do navegador (senão eles disputam
com a paginação do Paged.js), **marcar** gráficos de plano de fundo. Salve em
`docs/abnt/monografia.pdf`.

- [ ] **Step 5: Atualizar o README**

Acrescente uma seção "Documentação acadêmica" apontando para
`docs/abnt/monografia.html`, com a instrução de servir por HTTP e exportar pelo
navegador, e a observação de que o PDF é artefato gerado.

Aproveite e **corrija a afirmação desatualizada** de "47 testes • 8 suítes" no
topo do README para o número real apurado na Task 8 — ou remova a afirmação, se
a suíte não pôde ser executada.

- [ ] **Step 6: Commit**

```bash
cd /home/swampert/dev/PID-CEUB-2026
git add docs/abnt/monografia.html docs/abnt/monografia.pdf README.md
git commit -m "docs(abnt): final review, PDF export and README pointers"
```

---

## Portão de saída do plano

A monografia está pronta quando:

- [ ] `lint_abnt.py` sem nenhum achado, incluindo zero pendências de preenchimento
- [ ] Suíte de ferramentas inteira passando
- [ ] Sumário e listas com números de página reais e corretos
- [ ] Todos os capítulos escritos a partir da leitura do código, sem API inventada
- [ ] Apêndice A cobrindo todos os símbolos públicos apurados no script da Task 9
- [ ] Capítulo 5 sem nenhum número não executado, e com declaração explícita do que não pôde ser verificado
- [ ] Paginação conferida página a página **pelo autor**, não pelo agente
- [ ] `docs/abnt/monografia.pdf` exportado
