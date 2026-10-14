import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import altura_eixo, carregar  # noqa: E402
from parts import components, motor_clamp  # noqa: E402


class TestEnvelopes(unittest.TestCase):
    def setUp(self):
        self.p = carregar()

    def test_motor_esquerdo_termina_na_face(self):
        bb = components.motor(self.p, +1).bounding_box()
        self.assertAlmostEqual(bb.max.Y, self.p.motor_face_y, places=3)
        self.assertAlmostEqual((bb.min.Z + bb.max.Z) / 2, altura_eixo(self.p), places=3)

    def test_motores_nao_se_tocam_no_meio(self):
        e = components.motor(self.p, +1)
        d = components.motor(self.p, -1)
        self.assertGreater(e.distance_to(d), 2.0)

    def test_qtr_na_altura_de_leitura(self):
        bb = components.qtr(self.p).bounding_box()
        self.assertAlmostEqual(bb.min.Z, 3.0, places=3)
        self.assertAlmostEqual((bb.min.X + bb.max.X) / 2, self.p.lookahead, places=3)

    def test_calco_sobe_o_qtr(self):
        bb = components.qtr(self.p, calcos=2).bounding_box()
        self.assertAlmostEqual(bb.min.Z, 4.0, places=3)

    def test_massas_somam_o_esperado(self):
        total = sum(m for _, m, _ in components.massas(self.p))
        self.assertTrue(150.0 < total < 230.0, total)


class TestAbracadeira(unittest.TestCase):
    def setUp(self):
        self.p = carregar()

    def test_nao_invade_o_motor(self):
        for lado in (+1, -1):
            inter = motor_clamp.fazer(self.p, lado) & components.motor(self.p, lado)
            self.assertAlmostEqual(inter.volume, 0.0, places=3)

    def test_cobre_a_caixa_de_reducao(self):
        bb = motor_clamp.fazer(self.p, +1).bounding_box()
        self.assertAlmostEqual(bb.max.Y, self.p.motor_face_y - motor_clamp.APERTO, places=3)
        self.assertGreater(bb.max.Z, altura_eixo(self.p) + self.p.n20_altura / 2)

    def test_paredes_minimas(self):
        for nome, v in motor_clamp.paredes(self.p).items():
            self.assertGreaterEqual(v, 1.2, nome)


if __name__ == "__main__":
    unittest.main()
