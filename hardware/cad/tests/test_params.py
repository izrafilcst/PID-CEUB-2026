import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import MEDIR_TOML, altura_eixo, carregar  # noqa: E402


class TestParams(unittest.TestCase):
    def test_le_cotas_do_specs(self):
        p = carregar()
        self.assertAlmostEqual(p.n20_largura, 12.0)
        self.assertAlmostEqual(p.n20_total_l, 32.5)
        self.assertAlmostEqual(p.qtr_l, 74.93, places=2)
        self.assertAlmostEqual(p.qtr_massa, 3.09, places=2)

    def test_layout_dentro_da_spec(self):
        p = carregar()
        self.assertGreaterEqual(p.lookahead, 70.0)
        self.assertLessEqual(p.lookahead, 100.0)
        self.assertAlmostEqual(p.sensor_altura, 3.0)
        self.assertGreaterEqual(p.base_z, p.vao_minimo)

    def test_tolerancias_de_partida(self):
        t = carregar().tol
        self.assertAlmostEqual(t.deslizante, 0.30)
        self.assertAlmostEqual(t.furo_m3, 3.4)
        self.assertAlmostEqual(t.eixo_d, 3.05)
        self.assertAlmostEqual(t.eixo_achatado, 2.55)
        self.assertAlmostEqual(t.inserto_m3_d, 4.0)

    def _toml(self, texto):
        f = tempfile.NamedTemporaryFile("w", suffix=".toml", delete=False)
        f.write(texto)
        f.close()
        return Path(f.name)

    def test_override_substitui_valor(self):
        p = carregar(self._toml("[medidas]\nsuporte_l = 78.5\n[tolerancias]\nfuro_m3 = 3.5\n"))
        self.assertAlmostEqual(p.suporte_l, 78.5)
        self.assertAlmostEqual(p.tol.furo_m3, 3.5)

    def test_override_invalido_falha(self):
        with self.assertRaisesRegex(ValueError, "suporte_l"):
            carregar(self._toml("[medidas]\nsuporte_l = 0\n"))

    def test_chave_desconhecida_falha(self):
        with self.assertRaisesRegex(KeyError, "suport_l"):
            carregar(self._toml("[medidas]\nsuport_l = 77\n"))

    def test_toml_lista_todas_as_cotas_medir(self):
        texto = (Path(__file__).resolve().parents[1] / "medidas.toml").read_text()
        for chave in MEDIR_TOML:
            self.assertIn(f"{chave} =", texto, chave)

    def test_furos_do_qtr_vem_do_specs(self):
        p = carregar()
        self.assertAlmostEqual(p.qtr_furos_dist, 69.85, places=2)
        self.assertGreater(p.qtr_furo_borda, p.qtr_furo_d / 2)
        self.assertLess(p.qtr_furo_borda, p.qtr_w / 2)

    def test_altura_do_eixo_e_metade_da_roda(self):
        p = carregar()
        self.assertAlmostEqual(altura_eixo(p), p.roda_od / 2)


if __name__ == "__main__":
    unittest.main()
