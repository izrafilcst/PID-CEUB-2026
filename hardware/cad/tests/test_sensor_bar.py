import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts import components, sensor_bar, skid  # noqa: E402


class TestBarra(unittest.TestCase):
    def setUp(self):
        self.p = carregar()

    def test_patim_apoia_no_chao(self):
        for lado in (+1, -1):
            bb = skid.fazer(self.p, lado).bounding_box()
            self.assertAlmostEqual(bb.min.Z, 0.0, places=3)

    def test_qtr_encosta_nos_postes_sem_invadir(self):
        inter = sensor_bar.fazer(self.p) & components.qtr(self.p)
        self.assertAlmostEqual(inter.volume, 0.0, places=3)
        d = sensor_bar.fazer(self.p).distance_to(components.qtr(self.p))
        self.assertAlmostEqual(d, 0.0, places=3)

    def test_patins_fora_do_campo_dos_sensores(self):
        self.assertGreater(sensor_bar.y_patim(self.p) - 5.0, self.p.qtr_l / 2)

    def test_barra_respeita_vao_minimo(self):
        bb = sensor_bar.fazer(self.p).bounding_box()
        self.assertGreaterEqual(bb.min.Z, self.p.vao_minimo)

    def test_calco_tem_meio_milimetro(self):
        self.assertAlmostEqual(skid.calco(self.p).bounding_box().size.Z, 0.5, places=3)

    def test_calcos_sobem_barra_e_patim_juntos(self):
        b0 = sensor_bar.barra_z0(self.p, 0)
        b2 = sensor_bar.barra_z0(self.p, 2)
        self.assertAlmostEqual(b2 - b0, 1.0, places=3)
        self.assertAlmostEqual(skid.fazer(self.p, +1, calcos=2).bounding_box().min.Z, 0.0,
                               places=3)

    def test_calota_do_patim_tem_raio_minimo(self):
        self.assertGreaterEqual(skid.RAIO_CALOTA, 3.0)

    def test_calco_da_lingua_tem_meio_milimetro(self):
        self.assertAlmostEqual(sensor_bar.calco_lingua(self.p).bounding_box().size.Z, 0.5,
                               places=3)


if __name__ == "__main__":
    unittest.main()
