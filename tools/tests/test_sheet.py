import unittest
from techdraw import Sheet, Prov, SHEET_W, SHEET_H


class TestSheet(unittest.TestCase):
    def _folha(self):
        return Sheet(
            title="MCP3008",
            maker="Microchip",
            code="MCP3008-I/P",
            scale="2:1",
            sheet_id="02",
            date="2026-08-12",
        )

    def test_raiz_svg_em_milimetros_de_papel(self):
        svg = self._folha().render()
        self.assertIn('width="297mm"', svg)
        self.assertIn('height="210mm"', svg)
        self.assertIn('viewBox="0 0 297 210"', svg)

    def test_fundo_branco_explicito(self):
        # Sem isto a prancha desaparece em visualizador de tema escuro.
        svg = self._folha().render()
        self.assertIn(f'<rect x="0" y="0" width="{SHEET_W}" height="{SHEET_H}" fill="#ffffff"', svg)

    def test_legenda_traz_os_campos_da_nbr_10068(self):
        svg = self._folha().render()
        for campo in ("MCP3008", "Microchip", "MCP3008-I/P", "2:1", "mm",
                      "2026-08-12", "Rafael Costa", "02"):
            self.assertIn(campo, svg, f"legenda sem o campo {campo!r}")

    def test_quadro_de_proveniencia_lista_os_tres_niveis(self):
        svg = self._folha().render()
        for simbolo in (Prov.OFICIAL.value, Prov.TIPICO.value, Prov.MEDIR.value):
            self.assertIn(simbolo, svg)
        self.assertIn("PROVENIÊNCIA", svg)

    def test_corpo_adicionado_aparece_no_render(self):
        f = self._folha()
        f.add('<circle cx="100" cy="100" r="5"/>')
        self.assertIn('<circle cx="100" cy="100" r="5"/>', f.render())

    def test_texto_e_escapado(self):
        f = Sheet(title="A & B <teste>", maker="x", code="y",
                  scale="1:1", sheet_id="99", date="2026-08-12")
        svg = f.render()
        self.assertIn("A &amp; B &lt;teste&gt;", svg)
        self.assertNotIn("<teste>", svg)


if __name__ == "__main__":
    unittest.main()
