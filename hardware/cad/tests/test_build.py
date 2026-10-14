import contextlib
import io
import struct
import sys
import tempfile
import unittest
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build  # noqa: E402

ESPERADOS = ["base", "abracadeira_E", "abracadeira_D", "barra_sensores", "patim", "calco",
             "calco_lingua", "roda", "cupom", "cupom_chave"]


def malha_stl(caminho: Path) -> tuple[int, int, int, int]:
    """Lê um STL binário e devolve (triângulos degenerados, arestas que não são compartilhadas
    por exatamente 2 triângulos, corpos conexos, total de triângulos). Sem dependência nova."""
    dados = caminho.read_bytes()
    n = struct.unpack_from("<I", dados, 80)[0]
    assert len(dados) == 84 + 50 * n, "não é STL binário"
    tris = []
    for i in range(n):
        v = struct.unpack_from("<12f", dados, 84 + 50 * i)[3:]
        tris.append(tuple(tuple(round(c, 5) for c in v[k:k + 3]) for k in (0, 3, 6)))
    degenerados = sum(1 for t in tris if len(set(t)) < 3)
    arestas = Counter(frozenset((t[a], t[b])) for t in tris for a, b in ((0, 1), (1, 2), (2, 0))
                      if t[a] != t[b])
    ruins = sum(1 for c in arestas.values() if c != 2)
    # corpos conexos: union-find pelos vértices
    pai: dict = {}

    def raiz(x):
        while pai.setdefault(x, x) != x:
            pai[x] = pai[pai[x]]
            x = pai[x]
        return x

    for a, b, c in tris:
        for u, w in ((a, b), (b, c)):
            ru, rw = raiz(u), raiz(w)
            if ru != rw:
                pai[ru] = rw
    corpos = len({raiz(v) for t in tris for v in t})
    return degenerados, ruins, corpos, n


class TestBuild(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.dir = Path(tempfile.mkdtemp())
        cls.saidas, cls.verificacoes = build.executar(cls.dir)

    def test_um_stl_e_um_step_por_peca(self):
        for nome in ESPERADOS:
            self.assertTrue((self.dir / f"{nome}.stl").stat().st_size > 1000, nome)
            self.assertTrue((self.dir / f"{nome}.step").stat().st_size > 1000, nome)

    def test_assembly_step(self):
        self.assertTrue((self.dir / "assembly.step").stat().st_size > 10000)

    # A-WP2-03
    def test_cada_stl_e_uma_malha_fechada_de_um_corpo(self):
        for nome in ESPERADOS:
            degenerados, ruins, corpos, n = malha_stl(self.dir / f"{nome}.stl")
            self.assertEqual((degenerados, ruins, corpos), (0, 0, 1),
                             f"{nome}: {n} triângulos")

    def test_verificador_de_malha_acusa_triangulo_solto(self):
        fechado = self.dir / "calco.stl"
        dados = bytearray(fechado.read_bytes())
        n = struct.unpack_from("<I", dados, 80)[0]
        solto = struct.pack("<12fH", 0, 0, 1, 50, 50, 50, 51, 50, 50, 50, 51, 50, 0)
        struct.pack_into("<I", dados, 80, n + 1)
        ruim = self.dir / "ruim.stl"
        ruim.write_bytes(bytes(dados) + solto)
        degenerados, ruins, corpos, _ = malha_stl(ruim)
        self.assertEqual((ruins, corpos), (3, 2))

    def test_relatorio_tem_os_numeros(self):
        texto = (self.dir / "relatorio_montagem.md").read_text()
        for trecho in ("Massa total", "CG", "Carga nos patins", "Roda", "O-ring",
                       "Velocidade estimada", "Folga mínima das rodas", "Lookahead"):
            self.assertIn(trecho, texto)

    # A-WP2-01
    def test_verificacoes_do_modelo_padrao_passam(self):
        falhas = [v.nome for v in self.verificacoes if not v.ok]
        self.assertEqual(falhas, [])
        texto = (self.dir / "relatorio_montagem.md").read_text()
        self.assertIn("## Verificações", texto)
        self.assertIn("✔", texto)
        self.assertNotIn("✘", texto)
        self.assertNotIn("NÃO IMPRIMIR", texto)

    # A-WP2-02
    def test_relatorio_tem_folgas_por_par(self):
        texto = (self.dir / "relatorio_montagem.md").read_text()
        self.assertIn("## Folgas por par", texto)
        self.assertRegex(texto, r"\| abraçadeira E \| motor E \| 0\.100 \|")
        self.assertRegex(texto, r"\| base \| roda E \| 0\.600 \|")

    # A-WP2-04
    def test_relatorio_lista_os_fixadores(self):
        texto = (self.dir / "relatorio_montagem.md").read_text()
        self.assertIn("## Fixadores", texto)
        self.assertIn("ISO 10642", texto)
        self.assertIn("escareado", texto)

    def test_verificacoes_cobrem_a_spec(self):
        nomes = " ".join(v.nome for v in self.verificacoes)
        for trecho in ("Interferência", "Folga mínima das rodas", "Carga nos patins",
                       "Vão livre", "Face do sensor", "Envelope"):
            self.assertIn(trecho, nomes)


class TestBuildComMedidaRuim(unittest.TestCase):
    """A-WP2-01: suporte de 83 × 42 mm empurra o berço contra as rodas."""

    @classmethod
    def setUpClass(cls):
        cls.dir = Path(tempfile.mkdtemp())
        toml = cls.dir / "medidas.toml"
        toml.write_text("[medidas]\nsuporte_l = 83.0\nsuporte_w = 42.0\n", encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()), \
                contextlib.redirect_stderr(io.StringIO()):
            cls.codigo = build.main(["--medidas", str(toml), "--saida", str(cls.dir / "out")])
        cls.relatorio = (cls.dir / "out" / "relatorio_montagem.md").read_text()

    def test_sai_com_codigo_de_erro(self):
        self.assertNotEqual(self.codigo, 0)

    def test_relatorio_avisa_para_nao_imprimir(self):
        self.assertIn("NÃO IMPRIMIR", self.relatorio)
        self.assertIn("✘", self.relatorio)

    def test_acusa_a_colisao_com_as_rodas(self):
        linhas = [l for l in self.relatorio.splitlines() if "✘" in l]
        self.assertTrue(any("Interferência" in l for l in linhas), linhas)
        self.assertTrue(any("Folga mínima das rodas" in l for l in linhas), linhas)

    def test_exporta_mesmo_assim_para_inspecao(self):
        for nome in ESPERADOS:
            self.assertTrue((self.dir / "out" / f"{nome}.stl").exists(), nome)


class TestBuildTomlInvalido(unittest.TestCase):
    def test_medida_invalida_sai_com_erro_sem_exportar(self):
        d = Path(tempfile.mkdtemp())
        toml = d / "medidas.toml"
        toml.write_text("[medidas]\nsuporte_l = 0\n", encoding="utf-8")
        err = io.StringIO()
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(err):
            codigo = build.main(["--medidas", str(toml), "--saida", str(d / "out")])
        self.assertNotEqual(codigo, 0)
        self.assertIn("suporte_l", err.getvalue())
        self.assertFalse((d / "out").exists())


if __name__ == "__main__":
    unittest.main()
