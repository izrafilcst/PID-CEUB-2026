import unittest
from techdraw import (PartItem, View, balloon, iso_box, iso_cyl, note,
                      parts_list)


class TestAnotacoes(unittest.TestCase):
    def setUp(self):
        self.v = View(origin_x=50.0, origin_y=150.0, scale=1.0)

    def test_nota_tem_linha_guia_e_texto(self):
        svg = note(self.v, 10.0, 10.0, "furo M3 passante", dx=15.0, dy=-10.0)
        self.assertIn("furo M3 passante", svg)
        self.assertIn("<line", svg)

    def test_balao_e_circulo_com_numero(self):
        svg = balloon(self.v, 10.0, 10.0, 7, dx=12.0, dy=-8.0)
        self.assertIn("<circle", svg)
        self.assertIn(">7<", svg)


class TestListaDePecas(unittest.TestCase):
    def test_lista_traz_cabecalho_e_uma_linha_por_item(self):
        itens = [
            PartItem(1, 1, "Chassi", "PETG impresso", "—"),
            PartItem(2, 2, "Motor N20 c/ encoder", "Pololu", "04"),
        ]
        svg = parts_list(30.0, 40.0, itens)
        for c in ("ITEM", "QTD", "DENOMINAÇÃO", "REFERÊNCIA", "PRANCHA"):
            self.assertIn(c, svg)
        self.assertIn("Chassi", svg)
        self.assertIn("Motor N20 c/ encoder", svg)

    def test_lista_numera_de_baixo_para_cima(self):
        # Convenção NBR 10068: item 1 na linha imediatamente acima do cabeçalho.
        itens = [PartItem(1, 1, "A", "—", "—"), PartItem(2, 1, "B", "—", "—")]
        svg = parts_list(0.0, 100.0, itens)
        y_a = svg.index(">A<")
        y_b = svg.index(">B<")
        self.assertLess(y_a, y_b)


class TestIsometrica(unittest.TestCase):
    def test_caixa_isometrica_desenha_tres_faces(self):
        svg = iso_box(100.0, 100.0, l=20.0, w=10.0, h=5.0)
        self.assertEqual(svg.count("<path"), 3)

    def test_cilindro_isometrico_usa_elipses(self):
        svg = iso_cyl(100.0, 100.0, d=10.0, h=20.0, eixo="z")
        self.assertIn("<ellipse", svg)

    def test_cilindro_rejeita_eixo_invalido(self):
        with self.assertRaises(ValueError):
            iso_cyl(0.0, 0.0, d=10.0, h=20.0, eixo="q")


if __name__ == "__main__":
    unittest.main()
