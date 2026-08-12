import unittest
from techdraw import Prov, View, dim_h, dim_v, dim_diam, dim_rad, fmt_valor


class TestFormatacao(unittest.TestCase):
    def test_inteiro_sem_casa_decimal(self):
        self.assertEqual(fmt_valor(19.0), "19")

    def test_fracionario_com_virgula_decimal(self):
        self.assertEqual(fmt_valor(9.525), "9,53")

    def test_arredondamento_que_vira_inteiro_perde_a_virgula(self):
        self.assertEqual(fmt_valor(12.001), "12")


class TestCotas(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=50.0, origin_y=150.0, scale=1.0)

    def test_cota_horizontal_traz_valor_e_simbolo_de_proveniencia(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL)
        self.assertIn("20", svg)
        self.assertIn(Prov.OFICIAL.value, svg)

    def test_cota_medir_usa_o_simbolo_de_medir(self):
        svg = dim_h(self.v, 0.0, 58.0, 0.0, 58.0, Prov.MEDIR)
        self.assertIn(Prov.MEDIR.value, svg)
        self.assertNotIn(Prov.OFICIAL.value, svg)

    def test_cota_usa_linha_estreita(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.TIPICO)
        self.assertIn('stroke-width="0.25"', svg)

    def test_cota_tem_duas_setas(self):
        svg = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL)
        self.assertEqual(svg.count("<polygon"), 2)

    def test_offset_afasta_a_linha_de_cota_da_peca(self):
        perto = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL, offset=8.0)
        longe = dim_h(self.v, 0.0, 20.0, 0.0, 20.0, Prov.OFICIAL, offset=16.0)
        self.assertNotEqual(perto, longe)

    def test_cota_vertical_gira_o_texto(self):
        svg = dim_v(self.v, 0.0, 10.0, 0.0, 10.0, Prov.OFICIAL)
        self.assertIn("rotate(-90", svg)

    def test_diametro_recebe_o_prefixo(self):
        svg = dim_diam(self.v, 10.0, 10.0, 3.4, Prov.OFICIAL)
        self.assertIn("⌀3,4", svg.replace("&#8960;", "⌀"))

    def test_raio_recebe_o_prefixo(self):
        svg = dim_rad(self.v, 10.0, 10.0, 2.5, Prov.TIPICO)
        self.assertIn("R2,5", svg)


if __name__ == "__main__":
    unittest.main()
