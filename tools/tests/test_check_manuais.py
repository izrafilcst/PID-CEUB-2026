import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from check_manuais import codigos_erro, comandos_firmware, verificar  # noqa: E402

RAIZ = Path(__file__).resolve().parents[2]


def _arq(texto: str, nome: str = "x.md") -> Path:
    d = Path(tempfile.mkdtemp())
    f = d / nome
    f.write_text(texto, encoding="utf-8")
    return f


def _md(texto: str) -> Path:
    return _arq(texto, "x.md")


class TestVerificador(unittest.TestCase):
    def test_arquivo_existente_passa(self):
        self.assertEqual(verificar([_md("Veja `src/main.cpp`.")], RAIZ), [])

    def test_arquivo_inexistente_e_acusado(self):
        problemas = verificar([_md("Veja `src/nao_existe.cpp`.")], RAIZ)
        self.assertTrue(any("nao_existe.cpp" in p for p in problemas))

    def test_chave_ble_existente_passa(self):
        self.assertEqual(verificar([_md('Envie `{"t":"set","k":"kp","v":1.2}`.')], RAIZ), [])

    def test_chave_ble_inexistente_e_acusada(self):
        problemas = verificar([_md('Envie `{"t":"set","k":"kpp","v":1.2}`.')], RAIZ)
        self.assertTrue(any("kpp" in p for p in problemas))

    def test_chave_na_tabela_de_parametros_inexistente_e_acusada(self):
        problemas = verificar([_md("| `kpp` | ganho | — |\n")], RAIZ)
        self.assertTrue(any("kpp" in p for p in problemas))

    def test_ack_de_comando_passa(self):
        texto = 'Resposta: `{"t":"ack","k":"save","ok":1}` e `{"t":"ack","k":"lock","ok":1,"v":1}`.'
        self.assertEqual(verificar([_md(texto)], RAIZ), [])

    def test_ack_com_chave_inexistente_e_acusado(self):
        problemas = verificar([_md('`{"t":"ack","k":"turbo","ok":0,"err":"key"}`')], RAIZ)
        self.assertTrue(any("turbo" in p for p in problemas))

    def test_comando_inexistente_e_acusado(self):
        problemas = verificar([_md('Envie `{"t":"voar"}`.')], RAIZ)
        self.assertTrue(any("voar" in p for p in problemas))

    def test_ambiente_bench_inexistente_e_acusado(self):
        problemas = verificar([_md("Rode `pio run -e bench_foguete -t upload`.")], RAIZ)
        self.assertTrue(any("bench_foguete" in p for p in problemas))

    def test_stl_inexistente_e_acusado(self):
        problemas = verificar([_md("Imprima `turbina.stl`.")], RAIZ)
        self.assertTrue(any("turbina.stl" in p for p in problemas))

    def test_link_relativo_quebrado_e_acusado(self):
        problemas = verificar([_md("[fiação](../NAO.md)")], RAIZ)
        self.assertTrue(any("NAO.md" in p for p in problemas))

    def test_url_externa_e_ignorada(self):
        self.assertEqual(verificar([_md("[Pololu](https://www.pololu.com/product/960)")], RAIZ),
                         [])

    def test_html_com_recurso_externo_e_acusado(self):
        html = '<title>x</title><script src="https://cdn.example.com/a.js"></script>'
        problemas = verificar([_arq(html, "x.html")], RAIZ)
        self.assertTrue(any("recurso externo" in p for p in problemas))

    def test_html_com_link_externo_em_texto_passa(self):
        html = '<title>x</title><p><a href="https://www.pololu.com/product/960">Pololu</a></p>'
        self.assertEqual(verificar([_arq(html, "x.html")], RAIZ), [])

    def test_operacao_sem_linha_de_um_codigo_e_acusada(self):
        codigos = sorted(codigos_erro(RAIZ))
        linhas = "".join(f"| {c} | x | y | z |\n" for c in codigos[:-1])
        problemas = verificar([_arq(linhas, "OPERACAO.md")], RAIZ)
        self.assertTrue(any(f"código de erro {codigos[-1]}" in p for p in problemas))

    def test_codigos_de_erro_lidos_do_firmware(self):
        codigos = codigos_erro(RAIZ)
        self.assertNotIn(0, codigos)
        for c in (1, 2, 3, 4, 5):  # E-4 acrescentou o 5 (ENC)
            self.assertIn(c, codigos)

    def test_comandos_lidos_do_firmware(self):
        cmds = comandos_firmware(RAIZ)
        for c in ("set", "get", "save", "load", "defaults", "prof", "scal", "start", "stop",
                  "reset", "cal", "pid", "pidv", "spd", "rpm"):
            self.assertIn(c, cmds)
        self.assertNotIn("voar", cmds)

    def test_todos_os_codigos_de_erro_documentados(self):
        operacao = RAIZ / "docs" / "manual" / "OPERACAO.md"
        if not operacao.exists():
            self.skipTest("OPERACAO.md ainda não escrito (Tarefa 3)")
        texto = operacao.read_text(encoding="utf-8")
        for codigo in sorted(codigos_erro(RAIZ)):
            self.assertIn(f"| {codigo} |", texto, f"código {codigo}")
        manutencao = RAIZ / "docs" / "manual" / "MANUTENCAO.md"
        if manutencao.exists():
            texto = manutencao.read_text(encoding="utf-8")
            for codigo in sorted(codigos_erro(RAIZ)):
                self.assertIn(f"ERRO({codigo})", texto, f"diagnóstico do código {codigo}")


if __name__ == "__main__":
    unittest.main()
