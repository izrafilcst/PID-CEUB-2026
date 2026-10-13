import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts import base, components, electronics, motor_clamp  # noqa: E402


class TestBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = carregar()
        cls.base = base.fazer(cls.p)

    def test_vao_livre(self):
        self.assertGreaterEqual(self.base.bounding_box().min.Z, self.p.vao_minimo)

    def test_nao_invade_componentes(self):
        for nome, comp in (("motor E", components.motor(self.p, +1)),
                           ("motor D", components.motor(self.p, -1)),
                           ("suporte", components.suporte_bateria(self.p)),
                           ("perfboard", components.perfboard(self.p))):
            self.assertAlmostEqual((self.base & comp).volume, 0.0, places=3, msg=nome)

    def test_nao_invade_abracadeiras(self):
        for lado in (+1, -1):
            inter = self.base & motor_clamp.fazer(self.p, lado)
            self.assertAlmostEqual(inter.volume, 0.0, places=3)

    def test_aperto_da_abracadeira(self):
        clamp = motor_clamp.fazer(self.p, +1)
        self.assertAlmostEqual(self.base.distance_to(clamp), motor_clamp.APERTO, delta=0.05)

    def test_colunas_do_perfboard_tocam_a_placa(self):
        d = electronics.colunas(self.p).distance_to(components.perfboard(self.p))
        self.assertAlmostEqual(d, 0.0, places=3)

    def test_cabe_na_mesa(self):
        bb = self.base.bounding_box()
        self.assertLessEqual(bb.size.X, self.p.mesa)
        self.assertLessEqual(bb.size.Y, self.p.mesa)


if __name__ == "__main__":
    unittest.main()
