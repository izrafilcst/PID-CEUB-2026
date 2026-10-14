import unittest

from gen_assembly_drawings import ITENS, prancha_a1, prancha_a2


class TestA1(unittest.TestCase):
    def test_declara_o_sistema_de_coordenadas(self):
        svg = prancha_a1()
        self.assertIn("eixo motriz", svg.lower())
        self.assertIn("sentido de marcha", svg.lower())

    def test_mostra_o_envelope_de_competicao(self):
        self.assertIn("250", prancha_a1())

    def test_cota_o_lookahead(self):
        self.assertIn("70", prancha_a1())


class TestA2(unittest.TestCase):
    def test_todo_item_tem_balao(self):
        svg = prancha_a2()
        for it in ITENS:
            self.assertIn(f">{it.num}<", svg,
                          f"item {it.num} ({it.denominacao}) sem balão")

    def test_lista_de_pecas_presente(self):
        svg = prancha_a2()
        self.assertIn("DENOMINAÇÃO", svg)
        self.assertIn("PRANCHA", svg)

    def test_numeracao_dos_itens_e_sequencial_a_partir_de_1(self):
        self.assertEqual([it.num for it in ITENS],
                         list(range(1, len(ITENS) + 1)))

    def test_declara_a_representacao_simplificada(self):
        # Sem esta nota a prancha promete um detalhe que não entrega.
        self.assertIn("simplificad", prancha_a2().lower())


if __name__ == "__main__":
    unittest.main()
