import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts import tolerance_coupon as tc  # noqa: E402


class TestCupom(unittest.TestCase):
    def setUp(self):
        self.cupom = tc.fazer(carregar())

    def test_pequeno_e_rapido(self):
        bb = self.cupom.bounding_box()
        self.assertLessEqual(bb.size.X, 80.0)
        self.assertLessEqual(bb.size.Y, 40.0)
        self.assertLessEqual(bb.size.Z, 4.0)
        self.assertLess(self.cupom.volume, 7000.0)   # ≈ 15 min de impressão

    def test_tem_as_19_feicoes(self):
        self.assertEqual(len(tc.FEICOES), 19)

    def test_chave_de_teste_tem_10mm(self):
        bb = tc.chave_teste().bounding_box()
        self.assertAlmostEqual(bb.size.X, 10.0, places=3)
        self.assertAlmostEqual(bb.size.Y, 10.0, places=3)


if __name__ == "__main__":
    unittest.main()
