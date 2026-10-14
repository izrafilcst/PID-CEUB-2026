"""Fonte única de cotas e tolerâncias do CAD (spec §5.2–5.4).

Prioridade: medidas.toml (medido na peça em mãos ou no cupom) > specs.py (fichas) >
valores típicos deste arquivo. Cotas em mm, massas em g.
"""
from __future__ import annotations

import math
import sys
import tomllib
from dataclasses import dataclass, fields, replace
from functools import lru_cache
from pathlib import Path

CAD_DIR = Path(__file__).resolve().parent
REPO = CAD_DIR.parent.parent
TOML_PADRAO = CAD_DIR / "medidas.toml"
sys.path.insert(0, str(REPO / "hardware" / "components"))
sys.path.insert(0, str(REPO / "tools"))

from specs import COMPONENTES  # noqa: E402


def _spec(comp: str, medida: str) -> float:
    return COMPONENTES[comp].medidas[medida].valor


@dataclass(frozen=True)
class Tolerancias:
    deslizante: float = 0.30          # por lado
    pcb: float = 0.20                 # por lado
    furo_m2: float = 2.4
    furo_m3: float = 3.4
    autoatarraxante_m2: float = 1.7
    autoatarraxante_prof: float = 6.0
    inserto_m3_d: float = 4.0
    inserto_m3_prof: float = 6.5
    eixo_d: float = 3.05
    eixo_achatado: float = 2.55
    n20_folga: float = 0.10           # por lado
    comp_furo: float = 0.10           # acréscimo no diâmetro de furos < ⌀6


@dataclass(frozen=True)
class Params:
    tol: Tolerancias
    # ── N20 (ficha 04) ─────────────────────────────────────────────────────
    n20_largura: float                # horizontal (X)
    n20_altura: float                 # vertical (Z) — Pololu: seção 10 × 12 mm
    n20_caixa_l: float
    n20_total_l: float                # face da caixa → fundo do encoder
    n20_conector_l: float             # conector JST-SH além do encoder
    n20_eixo_l: float
    n20_eixo_d: float
    n20_achatado: float
    n20_massa: float
    n20_rpm_6v: float = 600.0
    n20_torque_stall_kgcm: float = 0.5    # 6 V, típico de N20 50:1 genérico
    # ── Bateria ────────────────────────────────────────────────────────────
    celula_d: float = 18.4
    celula_l: float = 65.2
    celula_massa: float = 46.0
    suporte_l: float = 77.0           # ao longo das células (Y no carro)
    suporte_w: float = 41.0           # X no carro
    suporte_h: float = 20.5
    suporte_massa: float = 12.0
    # ── QTR-8A (ficha 03) ──────────────────────────────────────────────────
    qtr_l: float = 0.0
    qtr_w: float = 0.0
    qtr_t: float = 0.0                # PCB + sensores
    qtr_massa: float = 0.0
    qtr_furos_dist: float = 69.85     # entre os furos de fixação nas pontas
    qtr_furo_d: float = 2.2
    qtr_furo_borda: float = 2.4       # centro dos furos → borda dos pinos (não é o eixo)
    # ── Eletrônica ─────────────────────────────────────────────────────────
    perf_l: float = 0.0               # 70 mm, ao longo de X no carro
    perf_w: float = 0.0               # 50 mm, ao longo de Y
    perf_t: float = 0.0
    perf_furo_canto: float = 3.5      # furo de fixação feito pelo montador
    eletronica_massa: float = 0.0     # perfboard + ESP32 + MCP3008 + TB6612 + MP1584 + fios
    eletronica_h: float = 22.0        # altura dos componentes sobre o perfboard
    fios_parafusos_massa: float = 10.0
    interruptor_l: float = 8.5
    interruptor_h: float = 3.6
    # ── Roda / O-ring ──────────────────────────────────────────────────────
    oring_id: float = 34.0
    oring_cs: float = 3.0
    oring_estiramento: float = 0.03
    oring_massa: float = 0.5
    # Aro de 8,2 (não 8,5): a face interna do aro fica a eixo_l − aro_w = 1,8 mm da face da
    # caixa do N20; menos a parede de batente (1,2) sobram 0,6 mm ≥ 0,5 de folga de peça
    # girando para a aba da base sob o motor. Com 8,5 sobravam 0,3 mm.
    roda_aro_w: float = 8.2
    roda_cubo_d: float = 10.0
    # ── Layout (spec §5.4) ─────────────────────────────────────────────────
    lookahead: float = 85.0
    sensor_altura: float = 3.0
    vao_minimo: float = 4.0
    base_z: float = 5.0               # face inferior da base
    base_t: float = 3.0
    base_meia_largura: float = 39.5
    motor_face_y: float = 42.0        # face da caixa de redução (|y|)
    folga_roda_motor: float = 0.6     # > 0,5: folga mínima de peça girando
    parede: float = 2.4
    parede_batente: float = 1.2
    suporte_x_traseira: float = 15.0
    perf_x_frente: float = -10.0
    perf_z: float = 32.0              # face inferior do perfboard
    barra_t: float = 8.0              # espessura da barra: inserto 6,5 + 1,5 por cima
    barra_lingua_x: float = 60.0      # início da língua que apoia na base
    vref: float = 7.4                 # V — tensão de referência do firmware (fábrica)
    # ── Material e máquina ─────────────────────────────────────────────────
    petg_densidade: float = 1.27e-3   # g/mm³
    preenchimento: float = 0.6
    mesa: float = 200.0

    # Derivados
    @property
    def roda_cubo_l(self) -> float:
        return self.n20_eixo_l - self.folga_roda_motor

    @property
    def roda_od(self) -> float:
        fundo = self.oring_id * (1 + self.oring_estiramento)
        return fundo + 2 * self.oring_cs * (1 - self.oring_estiramento / 2)

    @property
    def base_topo(self) -> float:
        return self.base_z + self.base_t


# Cotas △ MEDIR (ou típicas de peça genérica) que o montador deve conferir no medidas.toml
MEDIR_TOML = (
    "suporte_l", "suporte_w", "suporte_h", "celula_d", "n20_altura", "n20_conector_l",
    "qtr_furos_dist", "qtr_furo_borda", "perf_furo_canto", "interruptor_l", "interruptor_h",
    "oring_id", "oring_cs", "fios_parafusos_massa",
)


def altura_eixo(p: Params) -> float:
    return p.roda_od / 2


def _base() -> Params:
    massa_eletronica = sum(COMPONENTES[c].massa_g
                           for c in ("esp32", "mcp3008", "tb6612", "mp1584", "perfboard"))
    return Params(
        tol=Tolerancias(),
        n20_largura=_spec("n20", "largura_caixa"),
        n20_altura=10.0,
        n20_caixa_l=_spec("n20", "comprimento_caixa_reducao"),
        n20_total_l=_spec("n20", "comprimento_total"),
        n20_conector_l=5.0,
        n20_eixo_l=_spec("n20", "comprimento_eixo"),
        n20_eixo_d=_spec("n20", "diametro_eixo"),
        n20_achatado=_spec("n20", "achatado_eixo"),
        n20_massa=COMPONENTES["n20"].massa_g,
        qtr_l=_spec("qtr8a", "comprimento"),
        qtr_w=_spec("qtr8a", "largura"),
        qtr_t=_spec("qtr8a", "espessura"),
        qtr_massa=COMPONENTES["qtr8a"].massa_g,
        qtr_furos_dist=_spec("qtr8a", "entre_furos"),
        qtr_furo_borda=_spec("qtr8a", "furo_borda"),
        perf_l=_spec("perfboard", "comprimento"),
        perf_w=_spec("perfboard", "largura"),
        perf_t=_spec("perfboard", "espessura"),
        eletronica_massa=massa_eletronica,
    )


def _aplicar(p: Params, secao: dict, alvo: str) -> Params:
    nomes = {f.name for f in fields(Tolerancias if alvo == "tolerancias" else Params)}
    for chave, valor in secao.items():
        if chave not in nomes or chave == "tol":
            raise KeyError(f"medidas.toml [{alvo}]: chave desconhecida '{chave}'")
        if (isinstance(valor, bool) or not isinstance(valor, (int, float))
                or not math.isfinite(valor) or valor <= 0):
            raise ValueError(f"medidas.toml [{alvo}]: '{chave}' = {valor!r} inválido")
    if alvo == "tolerancias":
        return replace(p, tol=replace(p.tol, **{k: float(v) for k, v in secao.items()}))
    return replace(p, **{k: float(v) for k, v in secao.items()})


def _carregar(caminho: Path) -> Params:
    p = _base()
    if caminho.exists():
        dados = tomllib.loads(caminho.read_text())
        p = _aplicar(p, dados.get("medidas", {}), "medidas")
        p = _aplicar(p, dados.get("tolerancias", {}), "tolerancias")
    return p


@lru_cache(maxsize=None)
def _carregar_padrao() -> Params:
    return _carregar(TOML_PADRAO)


def carregar(caminho_toml: Path | None = None) -> Params:
    """Params com overrides. Sem argumento: medidas.toml padrão (em cache)."""
    if caminho_toml is None:
        return _carregar_padrao()
    return _carregar(Path(caminho_toml))
