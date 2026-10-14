import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import printability as pr  # noqa: E402
from params import carregar  # noqa: E402


class TestImprimibilidade(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = carregar()

    def test_cada_peca_cabe_na_mesa(self):
        for nome in pr.PECAS:
            bb = pr.na_orientacao(nome, self.p).bounding_box()
            self.assertLessEqual(bb.size.X, self.p.mesa, nome)
            self.assertLessEqual(bb.size.Y, self.p.mesa, nome)

    def test_cada_peca_apoia_na_mesa(self):
        for nome in pr.PECAS:
            self.assertAlmostEqual(pr.na_orientacao(nome, self.p).bounding_box().min.Z, 0.0,
                                   places=3, msg=nome)

    def test_paredes_minimas(self):
        for nome, (_, _, paredes) in pr.PECAS.items():
            if nome in pr.EXCECOES_PAREDE:
                continue
            for parede, v in paredes(self.p).items():
                self.assertGreaterEqual(v, 1.2, f"{nome}.{parede}")

    def test_balanco_dentro_do_limite(self):
        for nome in pr.PECAS:
            area = pr.area_balanco(pr.na_orientacao(nome, self.p))
            self.assertLessEqual(area, pr.LIMITE_BALANCO[nome], f"{nome}: {area:.1f} mm²")

    def test_area_balanco_detecta_teto_suspenso(self):
        from build123d import Box, Pos
        mesa = Pos(0, 0, 1) * Box(20, 20, 2)     # placa na mesa
        poste = Pos(0, 0, 4) * Box(2, 2, 6)
        teto = Pos(0, 0, 7) * Box(20, 20, 2)     # placa suspensa: 396 mm² para baixo
        self.assertGreater(pr.area_balanco(mesa + poste + teto), 350.0)


if __name__ == "__main__":
    unittest.main()
