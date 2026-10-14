#!/usr/bin/env python3
"""Gera hardware/cad/out/: STL (orientação de impressão), STEP (montagem) e relatório.

Uso: .venv/bin/python build.py [--medidas outro.toml] [--saida pasta]

Roda as verificações da spec §5.8 (interferência, folga das rodas, carga nos patins, vão
livre, altura do sensor, envelope) e grava ✔/✘ por item no relatório. Se alguma falhar,
exporta mesmo assim, para inspeção, põe a faixa "NÃO IMPRIMIR" no relatório e sai com
código 1. Medida inválida no TOML: mensagem de erro, nada exportado, código 2.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

from build123d import Compound, export_step, export_stl

import assembly
import printability as pr
from params import CAD_DIR, Params, carregar
from parts import motor_clamp, tolerance_coupon, wheel
from parts.common import na_mesa

QUANTIDADE = {"base": 1, "abracadeira_E": 1, "abracadeira_D": 1, "barra_sensores": 1,
              "patim": 2, "calco": 4, "calco_lingua": 2, "roda": 2, "cupom": 1,
              "cupom_chave": 1}


def _pecas(p: Params) -> dict[str, tuple]:
    """nome → (peça na montagem, peça na mesa)."""
    imp = assembly.pecas_impressas(p)
    rodas = assembly.rodas(p)
    return {
        "base": (imp["base"], pr.na_orientacao("base", p)),
        "abracadeira_E": (imp["abraçadeira E"],
                          na_mesa(imp["abraçadeira E"], motor_clamp.ROT_IMPRESSAO)),
        "abracadeira_D": (imp["abraçadeira D"],
                          na_mesa(imp["abraçadeira D"], motor_clamp.ROT_IMPRESSAO)),
        "barra_sensores": (imp["barra de sensores"], pr.na_orientacao("barra de sensores", p)),
        "patim": (imp["patim E"], pr.na_orientacao("patim", p)),
        "calco": (pr.PECAS["calço"][0](p), pr.na_orientacao("calço", p)),
        "calco_lingua": (pr.PECAS["calço língua"][0](p), pr.na_orientacao("calço língua", p)),
        "roda": (rodas["roda E"], pr.na_orientacao("roda", p)),
        "cupom": (tolerance_coupon.fazer(p), na_mesa(tolerance_coupon.fazer(p),
                                                     tolerance_coupon.ROT_IMPRESSAO)),
        "cupom_chave": (tolerance_coupon.chave_teste(),
                        na_mesa(tolerance_coupon.chave_teste(), tolerance_coupon.ROT_IMPRESSAO)),
    }


def _relatorio(p: Params, verifs: list[assembly.Verificacao]) -> str:
    total, (x, y, z), itens = assembly.massa_e_cg(p)
    g = wheel.verificar_oring(p)
    rpm = wheel.rpm_sob_carga(p)
    falhas = [v for v in verifs if not v.ok]
    faixa = [
        f"> **NÃO IMPRIMIR** — {len(falhas)} verificação(ões) falharam: "
        + ", ".join(v.nome for v in falhas) + ". As peças foram exportadas só para "
        "inspeção. Corrija o `medidas.toml` (ou o modelo) e rode o `build.py` de novo.",
        "",
    ] if falhas else []
    linhas = [
        "# Relatório de montagem — LFR v2",
        "",
    ] + faixa + [
        "Gerado por `hardware/cad/build.py`. Massas impressas estimadas (PETG "
        f"{p.petg_densidade * 1000:.2f} g/cm³, preenchimento {p.preenchimento:.0%}); "
        "massas compradas do `specs.py` (estimadas até a pesagem — D7).",
        "",
        "## Verificações",
        "",
        "| Verificação | Critério | Valor | Resultado |",
        "|-------------|----------|-------|-----------|",
    ] + [f"| {v.nome} | {v.criterio} | {v.valor} | {'✔' if v.ok else '✘'} |"
         for v in verifs] + [
        "",
        "## Números do modelo",
        "",
        "| Grandeza | Valor |",
        "|----------|-------|",
        f"| Massa total | {total:.0f} g |",
        f"| CG (x, y, z) | ({x:.1f}, {y:.1f}, {z:.1f}) mm — origem no eixo motriz, no chão |",
        f"| Carga nos patins | {x / p.lookahead:.1%} do peso (meta 10–20 %) |",
        f"| Lookahead (sensores → eixo) | {p.lookahead:.0f} mm |",
        f"| Roda (diâmetro com O-ring) | {p.roda_od:.1f} mm |",
        f"| O-ring | {g.id:.0f} × {g.cs:.1f} mm, fundo do canal ⌀{g.fundo_d:.2f} mm |",
        f"| Velocidade estimada | {wheel.velocidade_m_s(p.roda_od, rpm):.2f} m/s "
        f"(N20 {p.n20_rpm_6v:.0f} RPM @ 6 V, vref {p.vref} V, carga {wheel.CARGA:.0%}) |",
        f"| Aceleração a meia velocidade | {wheel.aceleracao_m_s2(p, total):.1f} m/s² |",
        f"| Folga mínima das rodas | {assembly.folga_minima_rodas(p):.2f} mm |",
        "| Envelope | {:.0f} × {:.0f} × {:.0f} mm |".format(*assembly.envelope(p)),
        "",
        "## Massas",
        "",
        "| Item | g |",
        "|------|---|",
    ] + [f"| {n} | {m:.1f} |" for n, m in itens] + [
        "",
        "## Peças para imprimir",
        "",
        "| Arquivo | Quantidade |",
        "|---------|------------|",
    ] + [f"| `{n}.stl` | {q} |" for n, q in QUANTIDADE.items()]
    return "\n".join(linhas) + "\n"


def executar(destino: Path, p: Params | None = None
             ) -> tuple[dict[str, Path], list[assembly.Verificacao]]:
    """Exporta tudo para `destino` e devolve (STLs gerados, verificações)."""
    p = p or carregar()
    destino.mkdir(parents=True, exist_ok=True)
    saidas = {}
    for nome, (montagem, mesa) in _pecas(p).items():
        export_stl(mesa, str(destino / f"{nome}.stl"))
        export_step(montagem, str(destino / f"{nome}.step"))
        saidas[nome] = destino / f"{nome}.stl"
    tudo = {**assembly.pecas_impressas(p), **assembly.rodas(p), **assembly.componentes(p)}
    export_step(Compound(children=list(tudo.values())), str(destino / "assembly.step"))
    verifs = assembly.verificacoes(p)
    (destino / "relatorio_montagem.md").write_text(_relatorio(p, verifs), encoding="utf-8")
    return saidas, verifs


def gerar(destino: Path, p: Params | None = None) -> dict[str, Path]:
    return executar(destino, p)[0]


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--medidas", type=Path, default=None,
                    help="TOML de medidas (padrão: hardware/cad/medidas.toml)")
    ap.add_argument("--saida", type=Path, default=CAD_DIR / "out",
                    help="pasta de saída (padrão: hardware/cad/out)")
    args = ap.parse_args(argv)
    try:
        p = carregar(args.medidas)
    except (KeyError, ValueError, OSError) as erro:
        print(f"ERRO no medidas.toml: {erro}", file=sys.stderr)
        return 2
    _, verifs = executar(args.saida, p)
    print((args.saida / "relatorio_montagem.md").read_text(encoding="utf-8"))
    falhas = [v for v in verifs if not v.ok]
    if falhas:
        print("NÃO IMPRIMIR: " + "; ".join(f"{v.nome} ({v.valor})" for v in falhas),
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
