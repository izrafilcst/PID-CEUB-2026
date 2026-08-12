import unittest
from techdraw import View


class TestView(unittest.TestCase):
    def test_escala_1_1_translada_para_a_origem_da_vista(self):
        v = View(origin_x=50.0, origin_y=150.0, scale=1.0)
        self.assertEqual(v.to_paper(0.0, 0.0), (50.0, 150.0))
        self.assertEqual(v.to_paper(10.0, 0.0), (60.0, 150.0))

    def test_eixo_y_e_invertido(self):
        # Y da peça cresce para cima; Y do papel cresce para baixo.
        v = View(origin_x=50.0, origin_y=150.0, scale=1.0)
        self.assertEqual(v.to_paper(0.0, 20.0), (50.0, 130.0))

    def test_escala_2_1_amplia(self):
        v = View(origin_x=0.0, origin_y=100.0, scale=2.0)
        self.assertEqual(v.to_paper(10.0, 5.0), (20.0, 90.0))

    def test_escala_1_2_reduz(self):
        v = View(origin_x=0.0, origin_y=100.0, scale=0.5)
        self.assertEqual(v.to_paper(10.0, 20.0), (5.0, 90.0))

    def test_comprimento_no_papel_aplica_a_escala(self):
        self.assertEqual(View(0.0, 0.0, 2.0).len_paper(7.5), 15.0)


class TestPrimitivas(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=0.0, origin_y=100.0, scale=1.0)

    def test_retangulo_da_peca_usa_contorno_visivel(self):
        svg = self.v.rect_part(0.0, 0.0, 20.0, 10.0)
        self.assertIn('stroke-width="0.5"', svg)
        self.assertIn('x="0.000"', svg)
        # canto superior no papel = origin_y - altura
        self.assertIn('y="90.000"', svg)

    def test_circulo_recebe_diametro_e_desenha_raio(self):
        svg = self.v.circle_part(10.0, 10.0, d=6.0)
        self.assertIn('r="3.000"', svg)
        self.assertIn('cx="10.000"', svg)
        self.assertIn('cy="90.000"', svg)

    def test_aresta_oculta_e_tracejada_e_mais_fina(self):
        svg = self.v.hidden([(0.0, 0.0), (10.0, 0.0)])
        self.assertIn('stroke-width="0.35"', svg)
        self.assertIn("stroke-dasharray", svg)

    def test_linha_de_centro_e_traco_ponto_e_estreita(self):
        svg = self.v.centerline_h(10.0, 10.0, 20.0)
        self.assertIn('stroke-width="0.25"', svg)
        self.assertIn("stroke-dasharray", svg)
        # ultrapassa o contorno em ambos os lados
        self.assertIn('x1="-1.500"', svg)

    def test_polilinha_fecha_quando_pedido(self):
        svg = self.v.polyline([(0.0, 0.0), (10.0, 0.0), (10.0, 5.0)], close=True)
        self.assertTrue(svg.count("L") >= 2)
        self.assertIn(" Z", svg)


if __name__ == "__main__":
    unittest.main()
