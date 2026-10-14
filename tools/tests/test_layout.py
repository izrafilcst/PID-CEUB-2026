import unittest

from layout import (LAYOUT, Massa, centro_de_gravidade,
                    margem_de_estabilidade)


class TestCentroDeGravidade(unittest.TestCase):
    def test_massa_unica_fica_na_propria_posicao(self):
        self.assertAlmostEqual(
            centro_de_gravidade([Massa("a", 100.0, -20.0)]), -20.0)

    def test_duas_massas_iguais_ficam_no_meio(self):
        cg = centro_de_gravidade(
            [Massa("a", 50.0, -40.0), Massa("b", 50.0, 0.0)])
        self.assertAlmostEqual(cg, -20.0)

    def test_massa_maior_puxa_o_cg(self):
        cg = centro_de_gravidade(
            [Massa("pesada", 90.0, -10.0), Massa("leve", 10.0, 70.0)])
        self.assertAlmostEqual(cg, -2.0)

    def test_lista_vazia_e_erro(self):
        with self.assertRaises(ValueError):
            centro_de_gravidade([])

    def test_massa_total_nula_e_erro(self):
        with self.assertRaises(ValueError):
            centro_de_gravidade([Massa("fantasma", 0.0, 10.0)])


class TestEstabilidade(unittest.TestCase):
    def test_cg_no_meio_do_vao_tem_margem_maxima(self):
        # caster em -60, eixo motriz em 0 → CG em -30 está mais folgado
        self.assertAlmostEqual(margem_de_estabilidade(-30.0, -60.0), 30.0)

    def test_cg_sobre_o_eixo_motriz_tem_margem_zero(self):
        self.assertAlmostEqual(margem_de_estabilidade(0.0, -60.0), 0.0)

    def test_cg_a_frente_do_eixo_motriz_da_margem_negativa(self):
        # capota para trás? não — sai do triângulo pela frente
        self.assertLess(margem_de_estabilidade(10.0, -60.0), 0.0)

    def test_cg_atras_do_caster_da_margem_negativa(self):
        self.assertLess(margem_de_estabilidade(-70.0, -60.0), 0.0)


class TestLayout(unittest.TestCase):
    def test_lookahead_vem_do_config_h(self):
        self.assertAlmostEqual(LAYOUT["lookahead"], 70.0)

    def test_altura_dos_sensores_vem_do_config_h(self):
        self.assertAlmostEqual(LAYOUT["altura_sensores"], 4.0)

    def test_eixo_motriz_e_a_origem(self):
        self.assertAlmostEqual(LAYOUT["x_eixo_motriz"], 0.0)

    def test_caster_fica_atras_do_eixo_motriz(self):
        self.assertLess(LAYOUT["x_caster"], 0.0)

    def test_cabe_no_envelope_de_competicao(self):
        comprimento = LAYOUT["lookahead"] - LAYOUT["x_caster"]
        self.assertLess(comprimento, 250.0)
        self.assertLess(LAYOUT["bitola"], 250.0)


if __name__ == "__main__":
    unittest.main()
