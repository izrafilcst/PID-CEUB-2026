import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from params import carregar  # noqa: E402
from parts import base, components, electronics, fixadores, motor_clamp  # noqa: E402


class TestBase(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p = carregar()
        cls.base = base.fazer(cls.p)

    def test_vao_livre(self):
        self.assertGreaterEqual(self.base.bounding_box().min.Z, self.p.vao_minimo)

    def test_nao_invade_componentes(self):
        for nome, comp in (("motor E", components.motor(self.p, +1)),
                           ("motor D", components.motor(self.p, -1)),
                           ("suporte", components.suporte_bateria(self.p)),
                           ("perfboard", components.perfboard(self.p))):
            self.assertAlmostEqual((self.base & comp).volume, 0.0, places=3, msg=nome)

    def test_nao_invade_abracadeiras(self):
        for lado in (+1, -1):
            inter = self.base & motor_clamp.fazer(self.p, lado)
            self.assertAlmostEqual(inter.volume, 0.0, places=3)

    def test_aperto_da_abracadeira(self):
        clamp = motor_clamp.fazer(self.p, +1)
        self.assertAlmostEqual(self.base.distance_to(clamp), motor_clamp.APERTO, delta=0.05)

    def test_colunas_do_perfboard_tocam_a_placa(self):
        d = electronics.colunas(self.p).distance_to(components.perfboard(self.p))
        self.assertAlmostEqual(d, 0.0, places=3)

    def test_cabe_na_mesa(self):
        bb = self.base.bounding_box()
        self.assertLessEqual(bb.size.X, self.p.mesa)
        self.assertLessEqual(bb.size.Y, self.p.mesa)


class TestFixacaoPorBaixo(unittest.TestCase):
    """A-WP2-04: nada que prende peça por baixo da base pode invadir o vão livre."""

    @classmethod
    def setUpClass(cls):
        cls.p = carregar()
        cls.base = base.fazer(cls.p)
        cls.envelopes = fixadores.envelopes_sob_a_base(cls.p)

    def test_ha_envelope_para_cada_m3_da_barra_e_para_a_abracadeira(self):
        nomes = set(self.envelopes)
        self.assertEqual(sum(n.startswith("cabeça M3") for n in nomes), 2)
        self.assertIn("abraçadeira do porta-fusível", nomes)

    def test_cabecas_ficam_no_escareado(self):
        for nome, env in self.envelopes.items():
            bb = env.bounding_box()
            self.assertGreaterEqual(bb.min.Z, self.p.base_z - 1e-6, nome)
            self.assertAlmostEqual((self.base & env).volume, 0.0, places=3, msg=nome)

    def test_vao_livre_conta_os_fixadores(self):
        import assembly
        nomes = set(assembly.componentes(self.p))
        for nome in self.envelopes:
            self.assertIn(nome, nomes)
        z, _ = assembly.vao_livre(self.p)
        self.assertGreaterEqual(z, self.p.vao_minimo)

    def test_cabeca_sem_escareado_seria_acusada(self):
        from unittest import mock

        import assembly
        from parts.common import cil_z
        exposta = cil_z(5.5, 2.4, 66.0, 8.0, self.p.base_z - 2.4)   # panela ISO 7045 M3
        original = assembly.componentes
        with mock.patch.object(assembly, "componentes",
                               lambda p, calcos=0: {**original(p, calcos),
                                                    "cabeça panela M3": exposta}):
            z, nome = assembly.vao_livre(self.p)
        self.assertEqual(nome, "cabeça panela M3")
        self.assertLess(z, self.p.vao_minimo)

    def test_placa_sob_escareado_e_rebaixo(self):
        paredes = base.paredes(self.p)
        self.assertGreaterEqual(paredes["placa_sob_escareado"], 1.2)
        self.assertGreaterEqual(paredes["placa_sob_rebaixo"], 1.2)

    def test_lista_de_fixadores(self):
        itens = " ".join(f[1] for f in fixadores.lista(self.p))
        for trecho in ("ISO 10642", "inserto", "M2", "abraçadeira"):
            self.assertIn(trecho, itens)


if __name__ == "__main__":
    unittest.main()
