import os
import tempfile
import unittest

from verify_drawings import checar_dentro, checar_soma, rasterizar


class TestChecagemAritmetica(unittest.TestCase):
    def test_soma_correta_nao_reporta(self):
        self.assertEqual(checar_soma(74.93, [10.0, 54.93, 10.0]), [])

    def test_soma_incorreta_reporta_a_diferenca(self):
        erros = checar_soma(75.0, [10.0, 50.0, 10.0])
        self.assertEqual(len(erros), 1)
        self.assertIn("5", erros[0])

    def test_tolerancia_absorve_arredondamento(self):
        self.assertEqual(checar_soma(9.525, [9.53], tol=0.01), [])


class TestFurosNoContorno(unittest.TestCase):
    def test_furo_interno_passa(self):
        self.assertEqual(
            checar_dentro((0.0, 0.0, 20.0, 10.0), [(5.0, 5.0, 2.0)]), [])

    def test_furo_estourando_a_borda_reporta(self):
        erros = checar_dentro((0.0, 0.0, 20.0, 10.0), [(19.5, 5.0, 2.0)])
        self.assertEqual(len(erros), 1)
        self.assertIn("19.5", erros[0])

    def test_furo_tangente_a_borda_reporta(self):
        erros = checar_dentro((0.0, 0.0, 20.0, 10.0), [(1.0, 5.0, 2.0)])
        self.assertEqual(len(erros), 1)


class TestRasterizacao(unittest.TestCase):
    def test_gera_png_nao_vazio(self):
        svg = ('<svg xmlns="http://www.w3.org/2000/svg" width="297mm" '
               'height="210mm" viewBox="0 0 297 210">'
               '<rect x="0" y="0" width="297" height="210" fill="#ffffff"/>'
               '<circle cx="100" cy="100" r="20" fill="none" '
               'stroke="#000" stroke-width="0.5"/></svg>')
        with tempfile.TemporaryDirectory() as d:
            sp = os.path.join(d, "a.svg")
            pp = os.path.join(d, "a.png")
            with open(sp, "w", encoding="utf-8") as fh:
                fh.write(svg)
            rasterizar(sp, pp, dpi=100)
            self.assertTrue(os.path.exists(pp))
            self.assertGreater(os.path.getsize(pp), 1000)


if __name__ == "__main__":
    unittest.main()
