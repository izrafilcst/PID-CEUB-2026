import sys
import os
import unittest

sys.path.insert(0, os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "hardware", "components"))

from specs import COMPONENTES  # noqa: E402
from techdraw import Prov  # noqa: E402


class TestIntegridade(unittest.TestCase):
    def test_toda_medida_oficial_declara_a_fonte(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                if m.prov is Prov.OFICIAL:
                    self.assertTrue(
                        m.fonte.strip(),
                        f"{chave}.{nome} é OFICIAL mas não tem fonte")

    def test_toda_medida_oficial_aponta_para_uma_url(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                if m.prov is Prov.OFICIAL:
                    self.assertIn(
                        "http", m.fonte,
                        f"{chave}.{nome}: fonte sem URL — {m.fonte!r}")

    def test_nenhuma_medida_com_valor_nao_positivo(self):
        for chave, c in COMPONENTES.items():
            for nome, m in c.medidas.items():
                self.assertGreater(m.valor, 0.0, f"{chave}.{nome}")

    def test_todo_componente_tem_numero_unico_de_dois_digitos(self):
        nums = [c.num for c in COMPONENTES.values()]
        self.assertEqual(len(nums), len(set(nums)), "números repetidos")
        for n in nums:
            self.assertRegex(n, r"^\d{2}$")

    def test_componentes_01_a_05_presentes(self):
        for chave in ("esp32", "mcp3008", "qtr8a", "n20", "tb6612"):
            self.assertIn(chave, COMPONENTES)

    def test_qtr8a_bate_com_o_datasheet_pololu(self):
        # Conferido em https://www.pololu.com/product/960 nesta sessão.
        q = COMPONENTES["qtr8a"]
        self.assertAlmostEqual(q.medidas["comprimento"].valor, 74.93, places=2)
        self.assertAlmostEqual(q.medidas["largura"].valor, 12.70, places=2)
        self.assertAlmostEqual(q.medidas["espessura"].valor, 3.18, places=2)
        self.assertAlmostEqual(q.medidas["passo_sensores"].valor, 9.525, places=3)
        self.assertAlmostEqual(q.massa_g, 3.09, places=2)
        self.assertIs(q.medidas["passo_sensores"].prov, Prov.OFICIAL)

    def test_massa_declarada_para_o_calculo_de_cg(self):
        # A prancha A1 depende disto. Zero significa "ainda não levantado".
        for chave, c in COMPONENTES.items():
            self.assertGreater(c.massa_g, 0.0,
                               f"{chave} sem massa — A1 não fecha")


class TestComponentes06a10(unittest.TestCase):
    def test_componentes_06_a_10_presentes(self):
        for chave in ("lipo", "mp1584", "roda", "caster", "perfboard"):
            self.assertIn(chave, COMPONENTES)

    def test_lipo_marcado_como_medir(self):
        # "2S 800 mAh 30C" não define dimensão. Qualquer cota OFICIAL aqui
        # seria invenção.
        lipo = COMPONENTES["lipo"]
        for nome in ("comprimento", "largura", "altura"):
            self.assertIs(lipo.medidas[nome].prov, Prov.MEDIR,
                          f"lipo.{nome} não pode ser OFICIAL nem TIPICO")

    def test_perfboard_declara_o_passo_de_2_54(self):
        self.assertAlmostEqual(
            COMPONENTES["perfboard"].medidas["passo_furos"].valor, 2.54,
            places=2)

    def test_roda_declara_diametro_externo_e_largura(self):
        roda = COMPONENTES["roda"]
        self.assertIn("diametro_externo", roda.medidas)
        self.assertIn("largura", roda.medidas)

    def test_sao_dez_componentes(self):
        self.assertEqual(len(COMPONENTES), 10)


if __name__ == "__main__":
    unittest.main()
