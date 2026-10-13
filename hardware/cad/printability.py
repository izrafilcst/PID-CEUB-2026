"""Verificações de impressão (spec §5.3, §5.8): mesa, apoio, paredes e balanço."""
from __future__ import annotations

import math

from build123d import Part, Rot

from params import Params
from parts import base, motor_clamp, sensor_bar, skid, tolerance_coupon, wheel
from parts.common import na_mesa

PECAS = {
    "base": (lambda p: base.fazer(p), base.ROT_IMPRESSAO, base.paredes),
    "abraçadeira": (lambda p: motor_clamp.fazer(p, +1), motor_clamp.ROT_IMPRESSAO,
                    motor_clamp.paredes),
    "barra de sensores": (lambda p: sensor_bar.fazer(p), sensor_bar.ROT_IMPRESSAO,
                          sensor_bar.paredes),
    "patim": (lambda p: skid.fazer(p, +1), skid.ROT_IMPRESSAO, skid.paredes),
    "calço": (lambda p: skid.calco(p), Rot(0, 0, 0), skid.paredes_calco),
    "calço língua": (lambda p: sensor_bar.calco_lingua(p), Rot(0, 0, 0),
                     skid.paredes_calco),
    "roda": (lambda p: wheel.fazer_roda(p), wheel.ROT_IMPRESSAO, wheel.paredes),
    "cupom": (lambda p: tolerance_coupon.fazer(p), tolerance_coupon.ROT_IMPRESSAO,
              lambda p: {"fundo_bolsao": 1.2}),
    "cupom chave": (lambda p: tolerance_coupon.chave_teste(), tolerance_coupon.ROT_IMPRESSAO,
                    lambda p: {"chave": 2.0}),
}

EXCECOES_PAREDE = {
    "calço": "peça de 0,5 mm por definição (ajuste fino da altura do sensor)",
    "calço língua": "idem, sob a língua da barra",
}

# Área máxima voltada para baixo (> 45°) acima da mesa, por peça, com a feição que justifica.
LIMITE_BALANCO = {
    "base": 100.0,       # abas do berço (4 × 6 × 1,5) + topo do furo ⌀11,4 nos batentes
                         # + teto da janela da chave (ponte de 8,8 mm)
    "abraçadeira": 20.0,  # topo dos furos M2 das orelhas (horizontais nesta orientação)
    "barra de sensores": 10.0,
    "patim": 5.0,
    "calço": 0.0,
    "calço língua": 0.0,
    "roda": 15.0,        # topo do furo radial M2
    "cupom": 0.0,        # furos passantes verticais e bolsões abertos para cima
    "cupom chave": 0.0,
}


def na_orientacao(nome: str, p: Params) -> Part:
    fabrica, rot, _ = PECAS[nome]
    return na_mesa(fabrica(p), rot)


def area_balanco(part: Part, angulo: float = 45.0, tolerancia: float = 0.05) -> float:
    """Soma a área dos triângulos voltados para baixo além de `angulo` e acima da mesa."""
    vertices, triangulos = part.tessellate(tolerancia)
    limite = -math.cos(math.radians(angulo))
    z_mesa = part.bounding_box().min.Z + 0.2
    area = 0.0
    for i, j, k in triangulos:
        a, b, c = vertices[i], vertices[j], vertices[k]
        n = (b - a).cross(c - a)
        dupla = n.length
        if dupla == 0:
            continue
        if n.Z / dupla < limite and min(a.Z, b.Z, c.Z) > z_mesa:
            area += dupla / 2
    return area
