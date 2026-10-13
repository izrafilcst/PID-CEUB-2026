import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build  # noqa: E402

ESPERADOS = ["base", "abracadeira_E", "abracadeira_D", "barra_sensores", "patim", "calco",
             "calco_lingua", "roda", "cupom", "cupom_chave"]


class TestBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = Path(tempfile.mkdtemp())
        cls.saidas = build.gerar(cls.dir)

    def test_um_stl_e_um_step_por_peca(self):
        for nome in ESPERADOS:
            self.assertTrue((self.dir / f"{nome}.stl").stat().st_size > 1000, nome)
            self.assertTrue((self.dir / f"{nome}.step").stat().st_size > 1000, nome)

    def test_assembly_step(self):
        self.assertTrue((self.dir / "assembly.step").stat().st_size > 10000)

    def test_relatorio_tem_os_numeros(self):
        texto = (self.dir / "relatorio_montagem.md").read_text()
        for trecho in ("Massa total", "CG", "Carga nos patins", "Roda", "O-ring",
                       "Velocidade estimada", "Folga mínima das rodas", "Lookahead"):
            self.assertIn(trecho, texto)


if __name__ == "__main__":
    unittest.main()
