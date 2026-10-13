import sys
import unittest
from dataclasses import replace
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import assembly  # noqa: E402
from params import carregar  # noqa: E402


class TestMontagem(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = carregar()

    def test_sem_interferencia(self):
        self.assertEqual(assembly.interferencias(self.p), [])

    # Review Focus 5
    def test_roda_tem_folga_minima(self):
        self.assertGreaterEqual(assembly.folga_minima_rodas(self.p), 0.5)

    def test_envelope_da_regra(self):
        x, y, z = assembly.envelope(self.p)
        self.assertLessEqual(x, 250.0)
        self.assertLessEqual(y, 250.0)
        self.assertLessEqual(z, 200.0)

    def test_sensor_a_3mm(self):
        qtr = assembly.componentes(self.p)["QTR-8A"]
        self.assertAlmostEqual(qtr.bounding_box().min.Z, 3.0, delta=0.1)

    def test_apoios_no_chao_e_vao_livre(self):
        for nome, peca in assembly.pecas_impressas(self.p).items():
            zmin = peca.bounding_box().min.Z
            if nome.startswith("patim"):
                self.assertAlmostEqual(zmin, 0.0, places=2, msg=nome)
            else:
                self.assertGreaterEqual(zmin, self.p.vao_minimo, nome)
        for nome, peca in assembly.rodas(self.p).items():
            if nome.startswith("O-ring"):
                self.assertAlmostEqual(peca.bounding_box().min.Z, 0.0, places=2, msg=nome)

    def test_carga_nos_patins_entre_10_e_20_porcento(self):
        c = assembly.carga_patins(self.p)
        self.assertTrue(0.10 <= c <= 0.20, f"{c:.3f}")

    def test_nao_empina(self):
        _, (x, _, z), _ = assembly.massa_e_cg(self.p)
        self.assertGreaterEqual(x, 0.3 * z)

    # Review Focus 3
    def test_calcos_sobem_o_sensor_sem_colisao(self):
        for n in (1, 2):
            self.assertEqual(assembly.interferencias(self.p, calcos=n), [])
            qtr = assembly.componentes(self.p, calcos=n)["QTR-8A"]
            self.assertAlmostEqual(qtr.bounding_box().min.Z, 3.0 + 0.5 * n, delta=0.1)

    # Review Focus 4
    def test_suporte_maior_ainda_cabe(self):
        grande = replace(self.p, suporte_l=79.0, suporte_w=42.0)
        self.assertEqual(assembly.interferencias(grande), [])


if __name__ == "__main__":
    unittest.main()
