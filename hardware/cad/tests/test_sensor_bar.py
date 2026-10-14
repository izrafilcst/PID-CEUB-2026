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

    # Diagrama dimensional da Pololu: os furos de fixação ficam junto da borda dos pinos,
    # não no eixo da placa. O QTR monta com a borda dos pinos para trás (−X).
    def test_postes_nos_furos_reais_do_qtr(self):
        p = self.p
        x = p.lookahead - p.qtr_w / 2 + p.qtr_furo_borda
        self.assertEqual(sorted(sensor_bar.furos_qtr(p)),
                         sorted([(x, -p.qtr_furos_dist / 2), (x, p.qtr_furos_dist / 2)]))
        self.assertGreater(p.lookahead - x, 3.0)

    def test_postes_assentam_dentro_da_placa_do_qtr(self):
        qtr = components.qtr(self.p).bounding_box()
        for x, y in sensor_bar.furos_qtr(self.p):
            self.assertTrue(qtr.min.X < x < qtr.max.X and qtr.min.Y < y < qtr.max.Y)

    # A-WP2-05: dá para ligar o QTR
    def test_janela_deixa_passar_o_conector(self):
        for n in (0, 1, 2):
            barra = sensor_bar.fazer(self.p, n)
            con = components.conector_qtr(self.p, n)
            self.assertAlmostEqual((barra & con).volume, 0.0, places=3, msg=n)
            self.assertGreater(con.bounding_box().max.Z, barra.bounding_box().max.Z)

    def test_conector_cobre_o_campo_de_pinos(self):
        p = self.p
        bb = components.conector_qtr(p).bounding_box()
        x_borda = p.lookahead - p.qtr_w / 2
        self.assertAlmostEqual(bb.min.X, x_borda + p.qtr_pinos_borda - 1.27, places=3)
        self.assertAlmostEqual(bb.max.X, x_borda + p.qtr_pinos_borda + 2.54 + 1.27, places=3)
        self.assertAlmostEqual(bb.size.Y, p.qtr_pinos_fim - p.qtr_pinos_ini, places=3)
        self.assertAlmostEqual(bb.min.Z, p.sensor_altura + p.qtr_t, places=3)

    def test_viga_mantem_parede_atras_da_janela(self):
        self.assertGreaterEqual(sensor_bar.paredes(self.p)["viga_atras_da_janela"], 1.2)

    def test_calco_da_lingua_tem_meio_milimetro(self):
        self.assertAlmostEqual(sensor_bar.calco_lingua(self.p).bounding_box().size.Z, 0.5,
                               places=3)


if __name__ == "__main__":
    unittest.main()
