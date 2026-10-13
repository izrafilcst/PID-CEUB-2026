import unittest

from gen_fichas import nome_arquivo, render_ficha
from specs import COMPONENTES
from techdraw import Prov


class TestNomeArquivo(unittest.TestCase):
    def test_prefixo_numerico_e_slug(self):
        self.assertEqual(nome_arquivo(COMPONENTES["qtr8a"]), "03-qtr8a.md")


class TestRenderFicha(unittest.TestCase):
    def setUp(self):
        self.md = render_ficha(COMPONENTES["qtr8a"])

    def test_avisa_que_e_gerado(self):
        self.assertIn("gerado automaticamente", self.md.lower())
        self.assertIn("specs.py", self.md)

    def test_traz_identificacao(self):
        self.assertIn("QTR-8A Reflectance Sensor Array", self.md)
        self.assertIn("Pololu", self.md)
        self.assertIn("960", self.md)

    def test_tabela_de_medidas_tem_coluna_de_proveniencia(self):
        self.assertIn("| Medida | Valor | Proveniência | Fonte |", self.md)
        self.assertIn("74,93", self.md)
        self.assertIn(Prov.OFICIAL.value, self.md)

    def test_traz_massa_para_o_calculo_de_cg(self):
        self.assertIn("3,09", self.md)

    def test_traz_referencia_abnt(self):
        self.assertIn("POLOLU CORPORATION", self.md)
        self.assertIn("Acesso em:", self.md)

    def test_traz_notas_de_montagem(self):
        self.assertIn("3,3 V", self.md)

    def test_remete_a_prancha(self):
        self.assertIn("hardware/drawings/comp/03-qtr8a.svg", self.md)

    def test_medida_medir_ganha_destaque(self):
        md = render_ficha(COMPONENTES["lipo"])
        self.assertIn(Prov.MEDIR.value, md)
        self.assertIn("MEDIR", md)

    def test_ficha_obsoleta_traz_aviso(self):
        md = render_ficha(COMPONENTES["caster"])
        self.assertIn("OBSOLETO", md)

    def test_ficha_sem_prancha_aponta_para_o_cad(self):
        md = render_ficha(COMPONENTES["suporte18650"])
        self.assertIn("hardware/cad/", md)


if __name__ == "__main__":
    unittest.main()
