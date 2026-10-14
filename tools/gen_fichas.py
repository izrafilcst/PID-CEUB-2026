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
    ]
    if c.obsoleto:
        p.append(f"\n> **OBSOLETO** — {c.obsoleto}\n")
    p += [
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
    prancha = os.path.join(RAIZ, "hardware", "drawings", "comp", f"{c.num}-{chave}.svg")
    if os.path.exists(prancha):
        p.append(f"`hardware/drawings/comp/{c.num}-{chave}.svg`\n")
    else:
        p.append("— sem prancha SVG; a geometria está no modelo CAD em `hardware/cad/`.\n")

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
