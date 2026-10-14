import sys
import unittest
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts.wheel import (aceleracao_m_s2, escolher_oring, fazer_oring, fazer_roda,  # noqa: E402
                         geometria_oring, velocidade_m_s, verificar_oring)


class TestORing(unittest.TestCase):
    def test_geometria_34x3(self):
        g = geometria_oring(34.0, 3.0, 0.03)
        self.assertAlmostEqual(g.fundo_d, 35.02, places=2)
        self.assertAlmostEqual(g.diametro_externo, 40.93, places=1)
        self.assertAlmostEqual(g.profundidade, 2.16, places=2)

    def test_escolha_fica_na_faixa_da_spec(self):
        g = escolher_oring()
        self.assertTrue(38.0 <= g.diametro_externo <= 44.0)

    def test_oring_fora_da_faixa_falha(self):
        p = replace(carregar(), oring_id=25.0)
        with self.assertRaisesRegex(ValueError, "38"):
            verificar_oring(p)


class TestDesempenho(unittest.TestCase):
    def test_velocidade_entre_1_1_e_1_5_m_s(self):
        p = carregar()
        from parts.wheel import rpm_sob_carga
        v = velocidade_m_s(p.roda_od, rpm_sob_carga(p))
        self.assertTrue(1.1 <= v <= 1.5, v)

    def test_aceleracao_suficiente_a_meia_velocidade(self):
        self.assertGreaterEqual(aceleracao_m_s2(carregar(), massa_g=250.0), 3.0)


class TestGeometria(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = carregar()
        cls.roda = fazer_roda(cls.p)

    def test_diametro_do_aro_abaixo_do_oring(self):
        bb = self.roda.bounding_box()
        self.assertLess(bb.size.X, self.p.roda_od)
        self.assertGreater(bb.size.X, self.p.roda_od - 2 * self.p.oring_cs)

    def test_cubo_engata_o_eixo(self):
        bb = self.roda.bounding_box()
        self.assertAlmostEqual(bb.max.Z, self.p.roda_cubo_l, places=2)
        self.assertGreaterEqual(self.p.roda_cubo_l, 9.0)

    def test_furo_em_d_aceita_o_eixo(self):
        t = self.p.tol
        self.assertGreater(t.eixo_d, self.p.n20_eixo_d)
        # achatado: distância do lado redondo até a face plana
        self.assertGreater(t.eixo_achatado, self.p.n20_achatado)

    def test_oring_toca_o_chao_quando_montado(self):
        bb = fazer_oring(self.p).bounding_box()
        self.assertAlmostEqual(bb.size.X, self.p.roda_od, delta=0.05)

    def test_volume_plausivel(self):
        massa = self.roda.volume * self.p.petg_densidade * self.p.preenchimento
        self.assertTrue(2.0 < massa < 8.0, massa)


if __name__ == "__main__":
    unittest.main()
