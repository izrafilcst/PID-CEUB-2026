import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts import battery_cradle, components  # noqa: E402


class TestBerco(unittest.TestCase):
    def setUp(self):
        self.p = carregar()
        self.berco = battery_cradle.feicao(self.p)

    def test_nao_invade_o_suporte(self):
        inter = self.berco & components.suporte_bateria(self.p)
        self.assertAlmostEqual(inter.volume, 0.0, places=3)

    def test_folga_deslizante(self):
        paredes = battery_cradle.paredes_e_abas(self.p)
        d = paredes.distance_to(components.suporte_bateria(self.p))
        self.assertAlmostEqual(d, self.p.tol.deslizante, delta=0.01)

    def test_nada_cobre_as_celulas(self):
        inter = self.berco & battery_cradle.area_livre_topo(self.p)
        self.assertAlmostEqual(inter.volume, 0.0, places=3)

    def test_abas_seguram_por_cima(self):
        bb = self.berco.bounding_box()
        topo_suporte = self.p.base_topo + self.p.suporte_h
        self.assertGreater(bb.max.Z, topo_suporte)

    def test_paredes_minimas(self):
        for nome, v in battery_cradle.paredes(self.p).items():
            self.assertGreaterEqual(v, 1.2, nome)


if __name__ == "__main__":
    unittest.main()
