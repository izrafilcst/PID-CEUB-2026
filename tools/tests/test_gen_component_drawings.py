import unittest

from gen_component_drawings import PRANCHAS, escala_para
from specs import COMPONENTES


class TestEscolhaDeEscala(unittest.TestCase):
    def test_peca_pequena_recebe_ampliacao(self):
        fator, rotulo = escala_para(COMPONENTES["mcp3008"])
        self.assertEqual(fator, 2.0)
        self.assertEqual(rotulo, "2:1")

    def test_peca_media_fica_em_escala_natural(self):
        fator, rotulo = escala_para(COMPONENTES["qtr8a"])
        self.assertEqual(fator, 1.0)
        self.assertEqual(rotulo, "1:1")

    def test_so_devolve_escalas_normalizadas_nbr_8196(self):
        for c in COMPONENTES.values():
            fator, _ = escala_para(c)
            self.assertIn(fator, (2.0, 1.0, 0.5))


class TestPranchas(unittest.TestCase):
    def test_existe_uma_prancha_por_componente(self):
        # Componentes 11–13 (v2) não ganham prancha: a geometria está no CAD
        # (spec 2026-10-13, §5.2).
        com_prancha = {k for k, c in COMPONENTES.items() if int(c.num) <= 10}
        self.assertEqual(set(PRANCHAS), com_prancha)

    def test_toda_prancha_e_svg_valido_com_fundo_branco(self):
        for chave, fn in PRANCHAS.items():
            svg = fn(COMPONENTES[chave])
            self.assertTrue(svg.startswith("<svg"), chave)
            self.assertTrue(svg.rstrip().endswith("</svg>"), chave)
            self.assertIn('fill="#ffffff"', svg, chave)

    def test_toda_prancha_traz_a_legenda_e_a_proveniencia(self):
        for chave, fn in PRANCHAS.items():
            svg = fn(COMPONENTES[chave])
            self.assertIn("PROVENIÊNCIA", svg, chave)
            self.assertIn("Unidade: mm", svg, chave)
            self.assertIn(COMPONENTES[chave].num, svg, chave)

    def test_prancha_do_qtr8a_cota_o_passo_dos_sensores(self):
        svg = PRANCHAS["qtr8a"](COMPONENTES["qtr8a"])
        self.assertIn("9,53", svg)   # 9,525 arredondado para duas casas
        self.assertIn("74,93", svg)

    def test_prancha_do_lipo_marca_as_cotas_como_medir(self):
        from techdraw import Prov
        svg = PRANCHAS["lipo"](COMPONENTES["lipo"])
        self.assertIn(Prov.MEDIR.value, svg)


if __name__ == "__main__":
    unittest.main()
