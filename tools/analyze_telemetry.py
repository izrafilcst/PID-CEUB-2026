#!/usr/bin/env python3
"""
analyze_telemetry.py — Análise pós-corrida de telemetria LFR

Uso:
    python3 tools/analyze_telemetry.py <arquivo.csv>
    python3 tools/analyze_telemetry.py <arquivo.csv> --plot
    python3 tools/analyze_telemetry.py <arquivo.csv> --report relatorio.md

Formato do CSV (exportado pelo dashboard):
    ts,pos,err,p,i,d,corr,vl,vr,dt

Dependências:
    pip install pandas matplotlib scipy
"""

import sys
import argparse
from pathlib import Path

import pandas as pd
import numpy as np


# ── Leitura e validação ────────────────────────────────────────────────────────

COLUMNS = ["ts", "pos", "err", "p", "i", "d", "corr", "vl", "vr", "dt"]

def load(path: str) -> pd.DataFrame:
    df = pd.read_csv(path, names=COLUMNS, comment="#")
    df["ts"] = pd.to_numeric(df["ts"], errors="coerce")
    for col in COLUMNS[1:]:
        df[col] = pd.to_numeric(df[col], errors="coerce")
    df.dropna(inplace=True)
    df.sort_values("ts", inplace=True)
    df.reset_index(drop=True, inplace=True)
    df["ts_s"] = (df["ts"] - df["ts"].iloc[0]) / 1e6  # µs → s
    return df


# ── Métricas ───────────────────────────────────────────────────────────────────

def metrics(df: pd.DataFrame) -> dict:
    duration = df["ts_s"].iloc[-1] - df["ts_s"].iloc[0]
    loop_ms = df["dt"] / 1000.0

    iae = float(np.trapz(df["err"].abs(), df["ts_s"]))
    ise = float(np.trapz(df["err"] ** 2, df["ts_s"]))
    itae = float(np.trapz(df["ts_s"] * df["err"].abs(), df["ts_s"]))

    speed_avg = (df["vl"].abs() + df["vr"].abs()).mean() / 2.0
    corr_rms = float(np.sqrt((df["corr"] ** 2).mean()))

    slow_samples = int((loop_ms > 2.0).sum())
    slow_pct = 100.0 * slow_samples / len(df)

    crossings = int(((df["pos"].shift(1) * df["pos"]) < 0).sum())

    return {
        "duration_s": round(duration, 3),
        "samples": len(df),
        "loop_mean_ms": round(loop_ms.mean(), 3),
        "loop_max_ms": round(loop_ms.max(), 3),
        "loop_p95_ms": round(loop_ms.quantile(0.95), 3),
        "slow_samples": slow_samples,
        "slow_pct": round(slow_pct, 2),
        "pos_mean": round(df["pos"].mean(), 2),
        "pos_std": round(df["pos"].std(), 2),
        "pos_max_abs": round(df["pos"].abs().max(), 1),
        "err_mean": round(df["err"].mean(), 4),
        "err_std": round(df["err"].std(), 4),
        "IAE": round(iae, 3),
        "ISE": round(ise, 3),
        "ITAE": round(itae, 3),
        "corr_rms": round(corr_rms, 2),
        "speed_avg_pwm": round(speed_avg, 1),
        "zero_crossings": crossings,
    }


# ── Relatório texto ────────────────────────────────────────────────────────────

def print_report(m: dict, path: str):
    print(f"\n{'='*60}")
    print(f"  LFR Telemetria — {Path(path).name}")
    print(f"{'='*60}")
    print(f"  Duração          : {m['duration_s']} s")
    print(f"  Amostras         : {m['samples']}")
    print(f"\n  Loop timing")
    print(f"    Médio          : {m['loop_mean_ms']} ms")
    print(f"    Máximo         : {m['loop_max_ms']} ms")
    print(f"    p95            : {m['loop_p95_ms']} ms")
    slow_warn = " ⚠ ATENÇÃO" if m['slow_pct'] > 1.0 else ""
    print(f"    > 2 ms         : {m['slow_samples']} ({m['slow_pct']}%){slow_warn}")
    print(f"\n  Posição (±3500)")
    print(f"    Média          : {m['pos_mean']}")
    print(f"    Desvio padrão  : {m['pos_std']}")
    print(f"    Máx absoluto   : {m['pos_max_abs']}")
    print(f"    Zero crossings : {m['zero_crossings']}")
    print(f"\n  Erro de controle")
    print(f"    IAE            : {m['IAE']}")
    print(f"    ISE            : {m['ISE']}")
    print(f"    ITAE           : {m['ITAE']}")
    print(f"    Correção RMS   : {m['corr_rms']}")
    print(f"\n  Velocidade")
    print(f"    Média PWM      : {m['speed_avg_pwm']}/255")
    print(f"{'='*60}\n")


def save_report(m: dict, path: str, out: str):
    lines = [
        f"# Relatório de Telemetria — {Path(path).name}\n",
        f"**Gerado em**: {pd.Timestamp.now().strftime('%Y-%m-%d %H:%M')}\n",
        "## Resumo\n",
        f"| Métrica | Valor |",
        f"|---------|-------|",
        f"| Duração | {m['duration_s']} s |",
        f"| Amostras | {m['samples']} |",
        f"| Loop médio | {m['loop_mean_ms']} ms |",
        f"| Loop máximo | {m['loop_max_ms']} ms |",
        f"| Loops > 2 ms | {m['slow_samples']} ({m['slow_pct']}%) |",
        f"| Posição desvio | {m['pos_std']} |",
        f"| Zero crossings | {m['zero_crossings']} |",
        f"| IAE | {m['IAE']} |",
        f"| ISE | {m['ISE']} |",
        f"| ITAE | {m['ITAE']} |",
        f"| Correção RMS | {m['corr_rms']} |",
        f"| Velocidade média | {m['speed_avg_pwm']}/255 |",
        "",
        "## Interpretação\n",
        "- **IAE menor** → erro acumulado menor → melhor rastreamento",
        "- **ISE menor** → penaliza erros grandes → robô mais próximo da linha",
        "- **ITAE menor** → penaliza erros persistentes → resposta mais rápida",
        "- **Zero crossings alto** → robô oscila em torno da linha (Kp/Kd adequados)",
        "- **Zero crossings baixo** → possível bias ou robô distante da linha",
    ]
    Path(out).write_text("\n".join(lines))
    print(f"Relatório salvo em: {out}")


# ── Plots ──────────────────────────────────────────────────────────────────────

def plot(df: pd.DataFrame, path: str):
    try:
        import matplotlib.pyplot as plt
        import matplotlib.gridspec as gridspec
    except ImportError:
        print("matplotlib não instalado. Instale com: pip install matplotlib")
        return

    fig = plt.figure(figsize=(14, 10))
    fig.suptitle(f"LFR Telemetria — {Path(path).name}", fontsize=13, fontweight="bold")
    gs = gridspec.GridSpec(3, 2, figure=fig, hspace=0.4, wspace=0.35)

    t = df["ts_s"]

    # 1. Posição
    ax1 = fig.add_subplot(gs[0, :])
    ax1.plot(t, df["pos"], color="#00E8FF", lw=0.8, label="Posição")
    ax1.axhline(0, color="white", lw=0.5, ls="--", alpha=0.4)
    ax1.fill_between(t, df["pos"], alpha=0.15, color="#00E8FF")
    ax1.set_ylabel("Posição (±3500)")
    ax1.set_xlabel("Tempo (s)")
    ax1.legend(loc="upper right", fontsize=8)
    ax1.set_facecolor("#0A0E1A")
    ax1.grid(color="#1A2035", lw=0.5)

    # 2. Componentes PID
    ax2 = fig.add_subplot(gs[1, 0])
    ax2.plot(t, df["p"], label="P", color="#FFB700", lw=0.8)
    ax2.plot(t, df["i"], label="I", color="#00FF6E", lw=0.8)
    ax2.plot(t, df["d"], label="D", color="#FF3055", lw=0.8)
    ax2.set_ylabel("Componentes PID")
    ax2.legend(fontsize=8)
    ax2.set_facecolor("#0A0E1A")
    ax2.grid(color="#1A2035", lw=0.5)

    # 3. Correção
    ax3 = fig.add_subplot(gs[1, 1])
    ax3.plot(t, df["corr"], color="#FF8C00", lw=0.8)
    ax3.axhline(0, color="white", lw=0.5, ls="--", alpha=0.4)
    ax3.set_ylabel("Correção")
    ax3.set_facecolor("#0A0E1A")
    ax3.grid(color="#1A2035", lw=0.5)

    # 4. Velocidades
    ax4 = fig.add_subplot(gs[2, 0])
    ax4.plot(t, df["vl"], label="vL", color="#00FF6E", lw=0.8)
    ax4.plot(t, df["vr"], label="vR", color="#FF3055", lw=0.8)
    ax4.set_ylabel("PWM (0-255)")
    ax4.set_xlabel("Tempo (s)")
    ax4.legend(fontsize=8)
    ax4.set_facecolor("#0A0E1A")
    ax4.grid(color="#1A2035", lw=0.5)

    # 5. Loop timing
    ax5 = fig.add_subplot(gs[2, 1])
    loop_ms = df["dt"] / 1000.0
    ax5.plot(t, loop_ms, color="#9D7FFF", lw=0.6)
    ax5.axhline(2.0, color="#FF3055", lw=1, ls="--", label="Limite 2 ms")
    ax5.set_ylabel("Loop (ms)")
    ax5.set_xlabel("Tempo (s)")
    ax5.legend(fontsize=8)
    ax5.set_facecolor("#0A0E1A")
    ax5.grid(color="#1A2035", lw=0.5)

    fig.patch.set_facecolor("#030508")
    for ax in [ax1, ax2, ax3, ax4, ax5]:
        ax.tick_params(colors="gray")
        for spine in ax.spines.values():
            spine.set_edgecolor("#1A2035")

    out = Path(path).stem + "_analysis.png"
    plt.savefig(out, dpi=150, bbox_inches="tight", facecolor="#030508")
    print(f"Gráfico salvo em: {out}")
    plt.show()


# ── Entry point ────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="Análise de telemetria LFR")
    parser.add_argument("csv", help="Arquivo CSV exportado pelo dashboard")
    parser.add_argument("--plot", action="store_true", help="Gerar gráficos")
    parser.add_argument("--report", metavar="OUT", help="Salvar relatório em Markdown")
    args = parser.parse_args()

    df = load(args.csv)
    m = metrics(df)
    print_report(m, args.csv)

    if args.report:
        save_report(m, args.csv, args.report)

    if args.plot:
        plot(df, args.csv)


if __name__ == "__main__":
    main()
