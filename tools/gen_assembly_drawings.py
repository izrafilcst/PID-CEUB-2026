#!/usr/bin/env python3
"""Gera as pranchas de conjunto A1 e A2.

Uso:
    python3 gen_assembly_drawings.py
    python3 verify_drawings.py     # depois, para rasterizar e inspecionar
"""
from __future__ import annotations

import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "hardware", "components"))
sys.path.insert(0, os.path.join(RAIZ, "tools"))

from layout import (LAYOUT, MASSA_CHASSI_G, massas_do_robo,  # noqa: E402
                    resumo_cg)
from specs import COMPONENTES  # noqa: E402
from techdraw import (DASH_CENTER, H_DIM, H_LABEL, ISO_SIN,  # noqa: E402
                      PartItem, Prov, Sheet, View, W_THIN, W_VISIBLE,
                      balloon, dim_h, dim_v, fmt_valor, iso_box, iso_cyl,
                      line, note, parts_list, txt)

DIR_SAIDA = os.path.join(RAIZ, "hardware", "drawings", "assy")
DATA = "2026-08-12"

# Traço-duplo-ponto (NBR 8403): contorno de envelope / limite não material.
DASH_ENVELOPE = "10,2,1,2,1,2"

ENVELOPE = 250.0   # lado do envelope de competição, em mm


def _folha(titulo: str, sheet_id: str, escala: str) -> Sheet:
    return Sheet(title=titulo, maker="Projeto LFR", code="PID-CEUB-2026",
                 scale=escala, sheet_id=sheet_id, date=DATA)


def _bloco_texto(x: float, y: float, linhas: list[tuple[str, bool]],
                 h: float = 2.2, passo: float = 3.4) -> str:
    """Bloco de texto corrido em mm de papel. (texto, negrito) por linha."""
    partes = []
    for i, (t, negrito) in enumerate(linhas):
        partes.append(txt(x, y + i * passo, t, h,
                          weight="bold" if negrito else "normal"))
    return "".join(partes)


def _retangulo_tracejado(v: View, x: float, y: float, w: float, h: float,
                         dash: str) -> str:
    """Retângulo da peça com traço customizado (envelopes e limites)."""
    return v.polyline([(x, y), (x + w, y), (x + w, y + h), (x, y + h)],
                      w=W_THIN, close=True, dash=dash)


def _simbolo_cg(v: View, x: float, y: float, r: float = 3.0) -> str:
    """Símbolo de centro de gravidade: círculo com dois quadrantes cheios."""
    px, py = v.to_paper(x, y)
    return "".join([
        (f'<circle cx="{px:.3f}" cy="{py:.3f}" r="{r}" fill="#ffffff" '
         f'stroke="#000000" stroke-width="{W_VISIBLE}"/>'),
        (f'<path d="M {px:.3f} {py:.3f} L {px:.3f} {py - r:.3f} '
         f'A {r} {r} 0 0 1 {px + r:.3f} {py:.3f} Z" fill="#000000"/>'),
        (f'<path d="M {px:.3f} {py:.3f} L {px:.3f} {py + r:.3f} '
         f'A {r} {r} 0 0 1 {px - r:.3f} {py:.3f} Z" fill="#000000"/>'),
    ])


# ═══════════════════════════════════════════════════════════════════════
# A1 — layout funcional e balanço de CG
# ═══════════════════════════════════════════════════════════════════════
def prancha_a1() -> str:
    """Planta funcional, vista lateral e balanço de massas."""
    total, cg, margem = resumo_cg()
    L = LAYOUT
    fator = 0.5

    s = _folha("A1 - Layout funcional e balanco de CG", "A1", "1:2")

    # ── planta ─────────────────────────────────────────────────────────
    pl = View(origin_x=100.0, origin_y=95.0, scale=fator)

    # envelope de competição, centrado na caixa envolvente do robô
    cx_rob = (L["lookahead"] + L["x_caster"]) / 2.0
    s.add(_retangulo_tracejado(pl, cx_rob - ENVELOPE / 2.0, -ENVELOPE / 2.0,
                               ENVELOPE, ENVELOPE, DASH_ENVELOPE))

    # eixos do sistema de coordenadas
    s.add(pl.centerline_h(0.0, 0.0, 210.0))
    s.add(pl.centerline_v(0.0, 0.0, 150.0))

    # rodas esquerda e direita, vistas de cima
    d_roda = COMPONENTES["roda"].medidas["diametro_externo"].valor
    l_roda = COMPONENTES["roda"].medidas["largura"].valor
    meia_bitola = L["bitola"] / 2.0
    for lado in (+1.0, -1.0):
        s.add(pl.rect_part(-d_roda / 2.0,
                           lado * meia_bitola - l_roda / 2.0,
                           d_roda, l_roda))

    # barra de sensores no lookahead
    c_qtr = COMPONENTES["qtr8a"].medidas["comprimento"]
    w_qtr = COMPONENTES["qtr8a"].medidas["largura"]
    s.add(pl.rect_part(L["x_sensores"] - w_qtr.valor / 2.0,
                       -c_qtr.valor / 2.0, w_qtr.valor, c_qtr.valor))

    # ball caster
    d_cas = COMPONENTES["caster"].medidas["diametro_esfera"]
    s.add(pl.circle_part(L["x_caster"], 0.0, d=d_cas.valor))
    s.add(pl.cross(L["x_caster"], 0.0, 20.0))

    # perfboard e ESP32 sobre o deck; LiPo por baixo (aresta oculta)
    c_pf = COMPONENTES["perfboard"]
    s.add(pl.rect_part(L["x_perfboard"] - c_pf.medidas["comprimento"].valor / 2.0,
                       -c_pf.medidas["largura"].valor / 2.0,
                       c_pf.medidas["comprimento"].valor,
                       c_pf.medidas["largura"].valor))
    c_esp = COMPONENTES["esp32"]
    s.add(pl.rect_part(L["x_esp32"] - c_esp.medidas["comprimento"].valor / 2.0,
                       -c_esp.medidas["largura"].valor / 2.0,
                       c_esp.medidas["comprimento"].valor,
                       c_esp.medidas["largura"].valor))
    c_lipo = COMPONENTES["lipo"]
    cl, wl = (c_lipo.medidas["comprimento"].valor,
              c_lipo.medidas["largura"].valor)
    s.add(pl.hidden([(L["x_lipo"] - cl / 2.0, -wl / 2.0),
                     (L["x_lipo"] + cl / 2.0, -wl / 2.0),
                     (L["x_lipo"] + cl / 2.0, wl / 2.0),
                     (L["x_lipo"] - cl / 2.0, wl / 2.0),
                     (L["x_lipo"] - cl / 2.0, -wl / 2.0)]))

    s.add(_simbolo_cg(pl, cg, 0.0))

    # cotas da planta
    y_cota = -meia_bitola - l_roda / 2.0
    s.add(dim_h(pl, 0.0, L["lookahead"], y_cota, L["lookahead"],
                Prov.TIPICO, offset=14.0))
    s.add(dim_h(pl, L["x_caster"], 0.0, y_cota, -L["x_caster"],
                Prov.TIPICO, offset=24.0))
    # Cota do envelope acima da planta: embaixo ela disputaria espaço com o
    # rótulo da vista e com a legenda.
    s.add(dim_h(pl, cx_rob - ENVELOPE / 2.0, cx_rob + ENVELOPE / 2.0,
                ENVELOPE / 2.0, ENVELOPE, Prov.OFICIAL, offset=-12.0))
    s.add(dim_v(pl, -meia_bitola, meia_bitola,
                L["x_sensores"] + w_qtr.valor / 2.0, L["bitola"],
                Prov.TIPICO, offset=-26.0))
    s.add(dim_v(pl, -c_qtr.valor / 2.0, c_qtr.valor / 2.0,
                L["x_sensores"] - w_qtr.valor / 2.0, c_qtr.valor,
                c_qtr.prov, offset=14.0))

    s.add(txt(44.0, 165.0, "PLANTA - envelope de competicao 250 x 250 mm",
              H_LABEL, weight="bold"))
    # A origem é identificada por um "O" junto ao cruzamento dos eixos; a
    # definição completa está no painel do sistema de coordenadas.
    s.add(txt(103.0, 91.5, "O", H_LABEL, weight="bold"))
    s.add(note(pl, cg, 0.0, f"CG calculado: x = {fmt_valor(cg)} mm",
               dx=28.0, dy=-40.0))
    s.add(note(pl, L["x_lipo"], wl / 2.0,
               "LiPo por baixo do deck (aresta oculta)", dx=30.0, dy=34.0))

    # ── vista lateral ──────────────────────────────────────────────────
    lat = View(origin_x=225.0, origin_y=92.0, scale=fator)
    s.add(lat.polyline([(L["x_caster"] - 14.0, 0.0),
                        (L["lookahead"] + 14.0, 0.0)], w=W_VISIBLE))
    s.add(lat.circle_part(0.0, L["altura_eixo_motriz"], d=d_roda))
    s.add(lat.cross(0.0, L["altura_eixo_motriz"], d_roda + 8.0))
    s.add(lat.circle_part(L["x_caster"], d_cas.valor / 2.0, d=d_cas.valor))
    esp_qtr = COMPONENTES["qtr8a"].medidas["espessura"].valor
    s.add(lat.rect_part(L["x_sensores"] - w_qtr.valor / 2.0,
                        L["altura_sensores"], w_qtr.valor, esp_qtr))

    s.add(dim_v(lat, 0.0, L["altura_eixo_motriz"], L["x_caster"] - 4.0,
                L["altura_eixo_motriz"], Prov.TIPICO, offset=8.0))
    s.add(dim_v(lat, 0.0, L["altura_sensores"], L["lookahead"] + 6.0,
                L["altura_sensores"], Prov.TIPICO, offset=-10.0))
    s.add(txt(186.0, 62.0, "VISTA LATERAL", H_LABEL, weight="bold"))
    s.add(note(lat, L["x_caster"], d_cas.valor,
               "caster regulado para 17 mm", dx=10.0, dy=-14.0))

    # ── painel de texto ────────────────────────────────────────────────
    s.add(_bloco_texto(178.0, 100.0, [
        ("SISTEMA DE COORDENADAS", True),
        ("Origem no ponto medio do eixo motriz, ao nivel do solo.", False),
        ("X positivo no sentido de marcha; Y positivo para a esquerda;", False),
        ("Z positivo para cima. Projecao no 1o diedro.", False),
        ("", False),
        ("BALANCO DE CG", True),
        (f"Massa total: {fmt_valor(total)} g", False),
        (f"CG longitudinal: x = {fmt_valor(cg)} mm", False),
        (f"Margem de estabilidade: {fmt_valor(margem)} mm", False),
    ]))

    s.add(_bloco_texto(178.0, 132.0, passo=3.1, linhas=[
        ("RESTRICOES DE PROJETO", True),
        ("1. O CG cai dentro do triangulo de apoio, entre o caster em",
         False),
        (f"   x = {fmt_valor(L['x_caster'])} mm e o eixo motriz em x = 0.",
         False),
        (f"2. CG mais perto do eixo motriz ({fmt_valor(abs(cg))} mm) que do "
         f"caster", False),
        (f"   ({fmt_valor(cg - L['x_caster'])} mm): carrega a tracao.",
         False),
        ("", False),
        ("Massas em MEDIR ou estimadas (LiPo, chassi, placas) tornam", False),
        (f"este CG INDICATIVO. Chassi entrou com {fmt_valor(MASSA_CHASSI_G)} "
         f"g estimados.", False),
        ("Nesta prancha o simbolo ○ marca decisao de projeto, nao", False),
        ("valor de fornecedor.", False),
    ]))
    return s.render()


# ═══════════════════════════════════════════════════════════════════════
# A2 — vista explodida
# ═══════════════════════════════════════════════════════════════════════
ITENS: list[PartItem] = [
    PartItem(1, 1, "Chassi (a modelar)", "PETG impresso", "—"),
    PartItem(2, 1, "QTR-8A Reflectance Sensor Array", "Pololu 960", "03"),
    PartItem(3, 2, "Micro metal gearmotor N20 c/ encoder", "Pololu", "04"),
    PartItem(4, 2, "Roda silicone ⌀34 mm", "Genérico", "08"),
    PartItem(5, 1, "Ball caster ½\"", "Robocore / Pololu", "09"),
    PartItem(6, 1, "ESP32 DevKit V1", "DOIT", "01"),
    PartItem(7, 1, "Perfboard 50×70 mm", "Genérico", "10"),
    PartItem(8, 1, "MCP3008 (no perfboard)", "Microchip", "02"),
    PartItem(9, 1, "TB6612FNG breakout", "SparkFun ROB-14451", "05"),
    PartItem(10, 1, "MP1584EN buck", "Genérico", "07"),
    PartItem(11, 1, "Bateria LiPo 2S 800 mAh", "Genérico", "06"),
]

# Escala dos sólidos isométricos. Não é a escala da folha: a A2 mostra
# envelopes, e a geometria cotada vive nas pranchas individuais.
ESC_ISO = 0.25

# View neutra para ancorar balões diretamente em mm de papel.
_V_PAPEL = View(origin_x=0.0, origin_y=0.0, scale=1.0)


def _balao_papel(px: float, py: float, numero: int,
                 dx: float, dy: float) -> str:
    """Balão ancorado num ponto dado em mm de papel."""
    return balloon(_V_PAPEL, px, -py, numero, dx, dy)


def _e(v: float) -> float:
    return v * ESC_ISO


def _alt_iso(l: float, w: float, h: float) -> float:
    """Altura que um prisma isométrico ocupa no papel."""
    return (l + w) * ISO_SIN + h


def prancha_a2() -> str:
    """Vista explodida: envelopes isométricos, balões e lista de peças."""
    s = _folha("A2 - Vista explodida do conjunto", "A2", "1:1")
    m = COMPONENTES

    def env(chave: str, *nomes: str) -> tuple[float, ...]:
        return tuple(m[chave].medidas[n].valor for n in nomes)

    # (num, ix, iy, desenho, altura isométrica ocupada)
    itens_desenho: list[tuple[int, float, float, str, float]] = []

    def caixa(num: int, ix: float, iy: float,
              l: float, w: float, h: float) -> None:
        l, w, h = _e(l), _e(w), _e(h)
        itens_desenho.append(
            (num, ix, iy, iso_box(ix, iy, l, w, h), _alt_iso(l, w, h)))

    def cilindro(num: int, ix: float, iy: float, d: float, h: float,
                 eixo: str) -> None:
        d, h = _e(d), _e(h)
        itens_desenho.append(
            (num, ix, iy, iso_cyl(ix, iy, d, h, eixo), d + _e(6.0)))

    # ── fila superior: estrutura e tração ──────────────────────────────
    caixa(1, 44.0, 78.0, 100.0, 60.0, 3.0)          # chassi
    caixa(2, 96.0, 78.0, *env("qtr8a", "comprimento", "largura",
                              "espessura"))
    cilindro(3, 146.0, 74.0, *env("n20", "largura_caixa",
                                  "comprimento_total"), "x")
    cilindro(4, 190.0, 74.0, *env("roda", "diametro_externo", "largura"), "y")
    cilindro(5, 232.0, 74.0, *env("caster", "diametro_esfera",
                                  "altura_base"), "z")

    # ── fila inferior: eletrônica e energia ────────────────────────────
    caixa(6, 44.0, 114.0, *env("esp32", "comprimento", "largura",
                               "espessura_pcb"))
    caixa(7, 94.0, 114.0, *env("perfboard", "comprimento", "largura",
                               "espessura"))
    caixa(8, 140.0, 114.0, *env("mcp3008", "comprimento", "largura_corpo",
                                "altura_corpo"))
    caixa(9, 172.0, 114.0, *env("tb6612", "comprimento", "largura",
                                "espessura_pcb"))
    caixa(10, 206.0, 114.0, *env("mp1584", "comprimento", "largura",
                                 "altura_com_indutor"))
    caixa(11, 240.0, 114.0, *env("lipo", "comprimento", "largura", "altura"))

    # trajetórias de montagem em traço-ponto, ligando os itens de cada fila
    for iy, faixa in ((78.0, (44.0, 232.0)), (114.0, (44.0, 240.0))):
        s.add(line(faixa[0] - 6.0, iy + 4.0, faixa[1] + 22.0, iy + 4.0,
                   W_THIN, DASH_CENTER))

    for num, ix, iy, svg, alt in itens_desenho:
        s.add(svg)
        s.add(_balao_papel(ix + 4.0, iy - alt, num,
                           dx=2.0, dy=-9.0))

    # ── lista de peças ─────────────────────────────────────────────────
    s.add(parts_list(27.0, 196.0, ITENS))

    s.add(txt(27.0, 34.0, "VISTA EXPLODIDA - envelopes dimensionais",
              H_LABEL, weight="bold"))
    s.add(_bloco_texto(180.0, 133.0, [
        ("Componentes em representacao simplificada (envelopes", False),
        ("dimensionais). Geometria detalhada nas pranchas", False),
        ("individuais indicadas na coluna PRANCHA.", False),
        ("", False),
        ("Fila de cima: estrutura e tracao. Fila de baixo:", False),
        ("eletronica e energia. As trajetorias em traco-ponto", False),
        ("indicam a ordem de montagem, nao a posicao final —", False),
        ("essa esta na prancha A1.", False),
    ], h=2.2, passo=3.2))
    return s.render()


PRANCHAS = {
    "A1-layout-funcional": prancha_a1,
    "A2-vista-explodida": prancha_a2,
}


def main(argv: list[str]) -> int:
    os.makedirs(DIR_SAIDA, exist_ok=True)
    for nome, fn in sorted(PRANCHAS.items()):
        destino = os.path.join(DIR_SAIDA, f"{nome}.svg")
        with open(destino, "w", encoding="utf-8") as fh:
            fh.write(fn())
        print(f"gerado: {destino}")
    print(f"\n{len(PRANCHAS)} prancha(s). Rode verify_drawings.py e INSPECIONE.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
