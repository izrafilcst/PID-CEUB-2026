"""Layout funcional do robô: geometria de referência e balanço de massas.

Sistema de coordenadas:
    origem  — ponto médio do eixo motriz, ao nível do solo
    X > 0   — sentido de marcha (à frente)
    Y > 0   — para a esquerda
    Z > 0   — para cima
"""
from __future__ import annotations

import os
import sys
from dataclasses import dataclass

_RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
for _p in (os.path.join(_RAIZ, "tools"),
           os.path.join(_RAIZ, "hardware", "components")):
    if _p not in sys.path:
        sys.path.insert(0, _p)

from specs import COMPONENTES  # noqa: E402


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
#
# x_caster e x_lipo NÃO são arbitrários: saíram do balanço abaixo, ajustados
# até a margem de estabilidade passar de 15 mm com o CG mais perto do eixo
# motriz que do caster (para carregar a tração).
LAYOUT: dict[str, float] = {
    "x_eixo_motriz": 0.0,
    "lookahead": 70.0,          # LOOKAHEAD_MM
    "altura_sensores": 4.0,     # SENSOR_HEIGHT_MM
    "altura_eixo_motriz": 17.0,  # ⌀roda / 2, ficha 08
    "bitola": 76.0,             # entre planos médios das rodas
    "x_caster": -55.0,          # resolvido pelo balanço de CG
    "x_lipo": -37.0,            # idem
    "x_esp32": -20.0,
    "x_perfboard": -30.0,
    "x_motores": -14.0,
    "x_chassi": -10.0,
    "x_sensores": 70.0,
}

# Massa do chassi impresso: a peça ainda não existe, então é ESTIMATIVA
# declarada. Entra no balanço porque ignorá-la distorceria o CG tanto
# quanto errá-la.
MASSA_CHASSI_G = 40.0


def massas_do_robo() -> list[Massa]:
    """Monta o balanço de massas a partir de specs.py e do LAYOUT.

    As massas vêm das fichas; as que estão marcadas como estimadas nas
    `notas_montagem` tornam o CG indicativo, não medido.
    """
    def g(chave: str) -> float:
        return COMPONENTES[chave].massa_g

    return [
        Massa("Barra de sensores QTR-8A", g("qtr8a"), LAYOUT["x_sensores"]),
        Massa("Rodas (2)", 2 * g("roda"), LAYOUT["x_eixo_motriz"]),
        Massa("Motores N20 (2)", 2 * g("n20"), LAYOUT["x_motores"]),
        Massa("Ball caster", g("caster"), LAYOUT["x_caster"]),
        Massa("ESP32 DevKit V1", g("esp32"), LAYOUT["x_esp32"]),
        Massa("Perfboard + MCP3008 + TB6612 + MP1584",
              g("perfboard") + g("mcp3008") + g("tb6612") + g("mp1584"),
              LAYOUT["x_perfboard"]),
        Massa("Chassi PETG (estimado)", MASSA_CHASSI_G, LAYOUT["x_chassi"]),
        Massa("Bateria LiPo 2S", g("lipo"), LAYOUT["x_lipo"]),
    ]


def resumo_cg() -> tuple[float, float, float]:
    """Devolve (massa total em g, CG em mm, margem de estabilidade em mm)."""
    massas = massas_do_robo()
    total = sum(m.massa_g for m in massas)
    cg = centro_de_gravidade(massas)
    return total, cg, margem_de_estabilidade(cg, LAYOUT["x_caster"])


if __name__ == "__main__":
    total, cg, margem = resumo_cg()
    print(f"{'componente':44s} {'massa (g)':>10s} {'x (mm)':>8s}")
    for m in massas_do_robo():
        print(f"{m.nome:44s} {m.massa_g:10.2f} {m.x_mm:8.1f}")
    print(f"\nmassa total          : {total:.2f} g")
    print(f"CG longitudinal      : {cg:.2f} mm")
    print(f"margem de estabilidade: {margem:.2f} mm "
          f"(apoio de {LAYOUT['x_caster']:.0f} a 0)")
