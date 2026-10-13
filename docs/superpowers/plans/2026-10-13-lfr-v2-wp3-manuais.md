# LFR v2 — WP3 Manuais Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Escrever os manuais do montador, do operador e da manutenção (Markdown no repo),
atualizar fiação, BOM, README e CLAUDE.md para a v2, e produzir o manual HTML interativo que
abre offline e é publicado como página.

**Architecture:** Os Markdown em `docs/manual/` são a fonte. Todo número citado vem de um
arquivo da branch (nunca de memória): parâmetros de `src/config/RobotConfig.cpp`, protocolo de
`src/comm/CommandProtocol.cpp`, peças e cotas de `hardware/cad/out/relatorio_montagem.md` e
`hardware/cad/params.py`, componentes das fichas. Um verificador (`tools/check_manuais.py`,
com testes) confere links, arquivos, chaves BLE, ambientes de bancada e STLs citados. O HTML é
uma página única derivada dos Markdown.

**Tech Stack:** Markdown (GFM), HTML/CSS/JS sem dependências externas, Python 3 `unittest`.

**Spec:** `docs/superpowers/specs/2026-10-13-lfr-v2-retomada-design.md` (§6).
Plano mestre: `docs/superpowers/plans/2026-10-13-lfr-v2-00-mestre.md`.

**Pré-requisito:** WP1 e WP2 integrados na branch `feat/lfr-v2-retomada` (fase P4 do plano
mestre). Conferir antes de começar:

```bash
test -f src/config/RobotConfig.cpp && test -f hardware/cad/out/relatorio_montagem.md \
  && echo "pronto para o WP3"
```

## Global Constraints

- Português do Brasil, frases curtas, voz ativa, imperativo nos procedimentos ("Aperte",
  "Meça"). Um passo = uma ação verificável.
- **Nenhum número sem fonte na branch.** Ao citar um valor, citar também de onde ele vem
  (arquivo) na primeira vez em cada documento.
- Unidades no SI com vírgula decimal no texto ("7,4 V"); valores de código e JSON ficam como
  no código (`7.4`).
- Segurança de Li-ion em destaque (bloco `> **ATENÇÃO**`) onde a ação tiver risco: carga,
  inserção de células, curto, célula danificada.
- Os manuais não repetem a spec: explicam **como fazer**. Arquitetura e decisões ficam no
  README e na spec.
- Commits Conventional Commits terminando com
  `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Ruflo: `hooks_pre-task` / `hooks_post-task` por tarefa; `memory_store` em
  `lfr-2026/wp3/status`.

## Review Focus

1. **Comando ou chave BLE citado que não existe no firmware** — o verificador acusa. Tarefa 1
   (`test_chave_ble_inexistente_e_acusada`).
2. **Arquivo, ambiente `bench_*` ou STL citado que não existe** — o verificador acusa. Tarefa 1
   (`test_arquivo_inexistente_e_acusado`, `test_ambiente_bench_inexistente_e_acusado`).
3. **Ordem de energização perigosa** — ajustar o MP1584 para 5,0 V **antes** de ligar o ESP32 e
   conferir a polaridade das células antes de fechar a chave. Tarefa 2 (checagem explícita na
   revisão do passo a passo).
4. **Operador sem internet na pista** — o HTML abre do disco, sem rede, e as caixas marcadas
   sobrevivem a recarregar a página. Tarefa 7.
5. **Código de erro sem ação** — todo código (1 a 4) tem sintoma, causa provável e ação no
   manual do operador e na árvore de diagnóstico. Tarefa 1 (`test_todos_os_codigos_de_erro_documentados`).

---

## Mapa de arquivos

| Arquivo | Ação | Tarefa |
|---------|------|--------|
| `tools/check_manuais.py`, `tools/tests/test_check_manuais.py` | Criar | 1 |
| `docs/manual/MONTAGEM.md` | Criar | 2 |
| `docs/manual/OPERACAO.md` | Criar | 3 |
| `docs/manual/MANUTENCAO.md` | Criar | 4 |
| `docs/WIRING.md`, `hardware/bom.csv` | Alterar | 5 |
| `README.md`, `CLAUDE.md`, `docs/DIVERGENCIAS.md`, `docs/TUNING.md` | Alterar | 6 |
| `docs/manual/manual-lfr.html` | Criar | 7 |

---

### Task 1: Verificador dos manuais

**Files:**
- Create: `tools/check_manuais.py`, `tools/tests/test_check_manuais.py`

**Interfaces:**
- Produces: `check_manuais.verificar(arquivos: list[Path], raiz: Path) -> list[str]` (lista de
  problemas, vazia = ok); `python3 tools/check_manuais.py` verifica `docs/manual/*.md`,
  `docs/manual/manual-lfr.html`, `docs/WIRING.md`, `README.md` e sai com código 1 se houver
  problema.

Regras:
1. Link Markdown relativo `[texto](caminho)` e `href="caminho"` relativo → o arquivo existe
   (âncoras `#…` e URLs `http…` são ignoradas).
2. Caminho entre crases com `/` e extensão conhecida (`.md .cpp .h .py .ini .csv .stl .step
   .html .toml .svg`) → existe (relativo à raiz do repo).
3. `"k":"<chave>"` e linhas de tabela de parâmetros `| \`<chave>\` |` → a chave existe em
   `src/config/RobotConfig.cpp` (macros `HW_KEY(`/`TP_KEY(`).
4. `{"t":"<cmd>"` → `<cmd>` é um comando aceito pelo `parseCommand`: `set get save load
   defaults prof scal start stop reset cal pid pidv spd rpm`.
5. `bench_<nome>` → existe `[env:bench_<nome>]` no `platformio.ini`.
6. `<nome>.stl` → existe em `hardware/cad/out/`.
7. Os códigos de erro 1, 2, 3 e 4 aparecem, cada um, numa linha de tabela de
   `docs/manual/OPERACAO.md` que começa com `| 1 |`, `| 2 |` etc.

- [ ] **Step 1: Escrever o teste**

`tools/tests/test_check_manuais.py`:

```python
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from check_manuais import verificar  # noqa: E402

RAIZ = Path(__file__).resolve().parents[2]


def _md(texto: str) -> Path:
    d = Path(tempfile.mkdtemp())
    f = d / "x.md"
    f.write_text(texto, encoding="utf-8")
    return f


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

    def test_todos_os_codigos_de_erro_documentados(self):
        operacao = RAIZ / "docs" / "manual" / "OPERACAO.md"
        if not operacao.exists():
            self.skipTest("OPERACAO.md ainda não escrito (Tarefa 3)")
        texto = operacao.read_text(encoding="utf-8")
        for codigo in ("1", "2", "3", "4"):
            self.assertIn(f"| {codigo} |", texto, f"código {codigo}")


if __name__ == "__main__":
    unittest.main()
```

Run: `cd tools && python3 -m unittest tests.test_check_manuais -v`
Expected: FAIL (`check_manuais` não existe).

- [ ] **Step 2: Implementar `tools/check_manuais.py`**

```python
#!/usr/bin/env python3
"""Confere se os manuais só citam arquivos, chaves, comandos, ambientes e STLs que existem.

Uso: python3 tools/check_manuais.py   (sai com 1 se houver problema)
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]
COMANDOS = {"set", "get", "save", "load", "defaults", "prof", "scal", "start", "stop",
            "reset", "cal", "pid", "pidv", "spd", "rpm"}
EXTENSOES = (".md", ".cpp", ".h", ".py", ".ini", ".csv", ".stl", ".step", ".html", ".toml",
             ".svg")
PADRAO_ARQUIVOS = [
    "docs/manual/*.md", "docs/manual/manual-lfr.html", "docs/WIRING.md", "README.md",
]


def chaves_firmware(raiz: Path) -> set[str]:
    fonte = (raiz / "src" / "config" / "RobotConfig.cpp").read_text(encoding="utf-8")
    return set(re.findall(r"(?:HW_KEY|TP_KEY)\((\w+)", fonte))


def ambientes(raiz: Path) -> set[str]:
    ini = (raiz / "platformio.ini").read_text(encoding="utf-8")
    return set(re.findall(r"\[env:(bench_\w+)\]", ini))


def verificar(arquivos: list[Path], raiz: Path = RAIZ) -> list[str]:
    chaves = chaves_firmware(raiz)
    envs = ambientes(raiz)
    stls = {p.name for p in (raiz / "hardware" / "cad" / "out").glob("*.stl")}
    problemas: list[str] = []
    for arq in arquivos:
        texto = arq.read_text(encoding="utf-8")
        nome = arq.name
        for alvo in re.findall(r"\]\(([^)\s#]+)(?:#[^)]*)?\)", texto) + \
                re.findall(r'href="([^"#]+)"', texto):
            if alvo.startswith(("http://", "https://", "mailto:")):
                continue
            if not (arq.parent / alvo).resolve().exists():
                problemas.append(f"{nome}: link quebrado → {alvo}")
        for caminho in re.findall(r"`([\w./-]+/[\w.-]+)`", texto):
            if caminho.endswith(EXTENSOES) and not (raiz / caminho).exists():
                problemas.append(f"{nome}: arquivo citado não existe → {caminho}")
        for chave in re.findall(r'"k"\s*:\s*"(\w+)"', texto) + \
                re.findall(r"^\|\s*`(\w+)`\s*\|", texto, flags=re.M):
            if chave not in chaves:
                problemas.append(f"{nome}: chave de configuração desconhecida → {chave}")
        for cmd in re.findall(r'\{\s*"t"\s*:\s*"(\w+)"', texto):
            if cmd not in COMANDOS and cmd not in ("tel", "sta", "cfg", "ack", "info",
                                                    "calres", "ecal"):
                problemas.append(f"{nome}: comando BLE desconhecido → {cmd}")
        for env in re.findall(r"\b(bench_\w+)", texto):
            if env not in envs:
                problemas.append(f"{nome}: ambiente PlatformIO inexistente → {env}")
        for stl in re.findall(r"\b([\w-]+\.stl)\b", texto):
            if stl not in stls:
                problemas.append(f"{nome}: STL inexistente em hardware/cad/out → {stl}")
    return problemas


def main() -> int:
    arquivos = [p for padrao in PADRAO_ARQUIVOS for p in RAIZ.glob(padrao)]
    problemas = verificar(arquivos)
    for p in problemas:
        print(p)
    print(f"{len(arquivos)} arquivo(s), {len(problemas)} problema(s)")
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
```

A regra da tabela de parâmetros (`| \`chave\` |`) vale para tabelas cuja primeira coluna é a
chave. Tabelas de outra natureza não devem começar com um nome entre crases que não seja
chave de configuração (ex.: na tabela de arquivos, usar o caminho com `/`).

- [ ] **Step 3: Rodar os testes**

Run: `cd tools && python3 -m unittest tests.test_check_manuais -v`
Expected: 9 OK e 1 skip (`OPERACAO.md` ainda não existe).

- [ ] **Step 4: Commit**

```bash
git add tools/check_manuais.py tools/tests/test_check_manuais.py
git commit -m "feat(tools): manual checker for files, BLE keys, commands, bench envs and STLs

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 2: Manual de montagem

**Files:**
- Create: `docs/manual/MONTAGEM.md`

**Fontes obrigatórias:** `hardware/bom.csv` (depois da Tarefa 5, ou a lista da spec §6 se a
Tarefa 5 ainda não rodou — nesse caso reconciliar no fim da Tarefa 5),
`hardware/cad/out/relatorio_montagem.md` (peças, quantidades, massas, roda, O-ring),
`hardware/cad/medidas.toml`, `hardware/cad/parts/tolerance_coupon.py` (ordem das feições),
`docs/WIRING.md`, `hardware/components/fichas/*.md`, `platformio.ini` (ambientes `bench_*`),
`hardware/cad/params.py` (tolerâncias).

Estrutura obrigatória (títulos exatamente assim):

```markdown
# Manual de montagem — LFR v2

> Para quem: montador. Tempo estimado: <somar as etapas>. Pré-requisito: nenhum.

## 1 Antes de começar
### 1.1 Lista de peças            ← tabela: item, quantidade, ficha, observação (da BOM)
### 1.2 Ferramentas               ← ferro de solda + ponta fina, ferro para insertos (ou ponta
                                    cônica), paquímetro, multímetro, chaves hex M2/M3, alicate,
                                    termo-retrátil, broca ⌀3,2 para o perfboard
### 1.3 Segurança                 ← bloco ATENÇÃO sobre Li-ion e solda
## 2 Impressão
### 2.1 Perfil de PETG            ← bico 0,4; camada 0,2; 3 perímetros; preenchimento 40–60 %;
                                    mesa 75–85 °C; bico 230–245 °C; ventoinha 30–50 %; sem suporte
### 2.2 Cupom de tolerância       ← imprimir `cupom.stl` + `cupom_chave.stl`; tabela que liga cada
                                    feição (na ordem de `FEICOES`) à chave do [tolerancias] do
                                    medidas.toml; como decidir (o furo em que o parafuso entra
                                    justo, o furo em D que o eixo do N20 entra com pressão leve,
                                    o furo de inserto que segura o inserto sem rachar etc.)
### 2.3 Medir as peças compradas  ← suporte 2×18650, células, O-ring, seção do N20, furos do
                                    QTR-8A → [medidas] do medidas.toml; rodar
                                    `.venv/bin/python build.py` em `hardware/cad/`
### 2.4 Peças e quantidades       ← tabela tirada do relatório de montagem, com a orientação de
                                    impressão de cada STL (já vem orientado)
## 3 Montagem mecânica            ← um passo por ação, na ordem:
                                    insertos M3 (colunas, língua da barra) → motores no berço +
                                    abraçadeiras (aperto até encostar, sem esmagar) → rodas
                                    (alinhar o achatado; trava M2 opcional) → O-rings → QTR-8A nos
                                    postes → patins → barra na base (2× M3 por baixo) → berço:
                                    suporte 2×18650 encaixado pelas abas → perfboard nas colunas
## 4 Eletrônica e fiação          ← resumo do WIRING.md com o diagrama de alimentação nova;
                                    resistores de 10 kΩ nos GPIO 36/39; montagem do perfboard
## 5 Checagens antes de energizar ← lista numerada, cada item com o valor esperado:
                                    continuidade GND; sem curto entre +BAT e GND (> 1 kΩ);
                                    polaridade das células no suporte; fusível instalado;
                                    MP1584 ajustado para 5,0 V **com o ESP32 desconectado**;
                                    tensão no VM do TB6612 = tensão do pack; STBY do TB6612
## 6 Primeira energização e firmware ← `pio run -e esp32dev -t upload`; o que deve acontecer
                                    (bipes, LED piscando 1 Hz = IDLE sem calibração; 3 bipes curtos
                                    = sem configuração salva, normal na 1ª vez)
## 7 Testes de bancada            ← os seis `pio run -e bench_* -t upload`, um por subseção, com
                                    o que observar e o que fazer se falhar (ex.: lado invertido →
                                    `invL`/`invR` pelo BLE, não refazer fiação)
## 8 Lista final de conferência   ← checklist com caixas `- [ ]`
```

- [ ] **Step 1: Escrever** seguindo a estrutura acima, com os números das fontes.
- [ ] **Step 2: Conferir a ordem segura** — revisar o documento procurando: (a) o ajuste do
  MP1584 para 5,0 V aparece **antes** de qualquer passo que conecte o ESP32; (b) a polaridade
  das células é conferida antes de fechar a chave; (c) existe o bloco ATENÇÃO sobre carga
  fora do carro. Corrigir se faltar.
- [ ] **Step 3: Rodar o verificador**

Run: `python3 tools/check_manuais.py`
Expected: `0 problema(s)` (os arquivos ainda não escritos não entram na lista).

- [ ] **Step 4: Commit**

```bash
git add docs/manual/MONTAGEM.md
git commit -m "docs(manual): assembly manual for LFR v2

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 3: Manual do operador

**Files:**
- Create: `docs/manual/OPERACAO.md`

**Fontes obrigatórias:** `src/config/RobotConfig.cpp` (chaves, faixas, fábrica),
`src/strategy/RobotStateMachine.cpp` (estados e transições), `src/comm/Annunciator.cpp`
(padrões de LED/bipe), `src/comm/CommandProtocol.cpp` (comandos e respostas),
`src/sensors/ButtonInput.h` (tempos do botão), `docs/TUNING.md`,
`docs/dashboard/lfr-cockpit-mock.html`.

Estrutura obrigatória:

```markdown
# Manual do operador — LFR v2

## 1 Ligar, desligar e o botão     ← toque curto (< 0,8 s) e longo (≥ 1,5 s); o botão é o BOOT
                                    da placa: não segurar ao ligar (entra em modo de gravação)
## 2 Estados, LED e bipes         ← tabela: estado | LED | bipe | o que significa | o que fazer
## 3 Calibração dos sensores      ← quando refazer; como varrer a linha nos 5 s; resultado
                                    (`calres`); a calibração fica salva
## 4 Dashboard                    ← abrir; CONECTAR BLE (Chrome/Edge; Android ok; iPhone não tem
                                    Web Bluetooth); abas; aba CONFIG; indicador "NÃO SALVO"
## 5 Parâmetros                   ← tabela completa: | `chave` | descrição | unidade | fábrica |
                                    faixa | muda com o carro andando? | (uma linha por chave)
## 6 Perfis e salvamento          ← 0 seguro / 1 treino / 2 corrida; set não salva; Salvar,
                                    Recarregar, Fábrica; trocar de perfil com mudança pendente
## 7 Primeira volta               ← roteiro curto: perfil 0, `mode` 0, calibrar, start com 1 s de
                                    atraso, observar; corrigir sentido de motor/sensor pelo BLE
                                    (`invL`, `invR`, `swap`, `sinv`, `pol`) sem recompilar
## 8 Sintonia na pista            ← resumo do TUNING.md (link) com os passos e o registro no CSV
## 9 Modo cascata (encoders)      ← só depois de `{"t":"cal","op":"start",...}` e `finish` nos
                                    dois lados; conferir `ecal`; aí `mode` = 1
## 10 Bateria                     ← % e tensão no dashboard; corte em `vcut`; aferir `vbk` com
                                    multímetro; ATENÇÃO: carga fora do carro, nunca abaixo de
                                    2,5 V por célula, célula amassada → descarte
## 11 Códigos de erro             ← tabela com a primeira coluna = código:
                                    | 1 | laço atrasado | … | ação |
                                    | 2 | linha perdida | … | ação |
                                    | 3 | bateria baixa | … | ação |
                                    | 4 | calibração inválida | … | ação |
## 12 Checklist de box (competição) ← caixas `- [ ]`: carga das células, parafusos, O-rings,
                                    patins, calibração na pista oficial, perfil certo salvo,
                                    BLE desconectado na largada se o regulamento exigir (ver
                                    docs/REVISAO_FIRMWARE.md, seção Regulamento)
```

- [ ] **Step 1: Escrever** seguindo a estrutura, gerando a tabela da seção 5 a partir do
  `RobotConfig.cpp` (mesma ordem das chaves).
- [ ] **Step 2: Rodar o verificador e o teste dos códigos**

Run: `python3 tools/check_manuais.py && cd tools && python3 -m unittest tests.test_check_manuais -v`
Expected: `0 problema(s)` e 10 OK (o teste dos códigos de erro deixa de ser pulado).

- [ ] **Step 3: Commit**

```bash
git add docs/manual/OPERACAO.md
git commit -m "docs(manual): operator manual with states, parameters, profiles and error codes

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 4: Manual de manutenção

**Files:**
- Create: `docs/manual/MANUTENCAO.md`

**Fontes obrigatórias:** fichas `hardware/components/fichas/*.md`,
`hardware/cad/out/relatorio_montagem.md`, `platformio.ini`, `docs/REVISAO_FIRMWARE.md`,
`docs/DIVERGENCIAS.md`, `README.md` (versão do firmware), `hardware/bom.csv`.

Estrutura obrigatória:

```markdown
# Manual de manutenção — LFR v2

## 1 Inspeção periódica            ← tabela: item | quando (a cada sessão / a cada 10 baterias /
                                    mensal) | o que olhar | critério de troca:
                                    patins (desgaste > 0,5 mm ou calota achatada → trocar),
                                    O-rings (liso, rachado, frouxo), parafusos e insertos,
                                    conectores JST-SH dos encoders, molas do suporte 2×18650,
                                    células (invólucro, tensão em repouso), fios no pivô das rodas
## 2 Troca de peças                ← um procedimento curto por peça: patim, calço, O-ring, roda,
                                    abraçadeira/motor, QTR-8A, suporte 2×18650, fusível
## 3 Diagnóstico                   ← árvore sintoma → causa → teste → ação, cobrindo no mínimo:
                                    não liga; liga e reinicia ao acelerar; LED em erro (1 a 4);
                                    gira em círculos; oscila na reta; sai da pista nas curvas;
                                    um motor não gira; RPM zerado no modo cascata; BLE não
                                    conecta; dashboard "NÃO SALVO" que não some
## 4 Firmware                       ← versão atual; regravar (`pio run -e esp32dev -t upload`);
                                    a configuração e a calibração sobrevivem à regravação (NVS);
                                    como voltar aos valores de fábrica (`defaults` + `save`)
## 5 Testes de bancada              ← tabela: ambiente `bench_*` | quando usar | resultado esperado
## 6 Aferição                       ← leitura de bateria (`vbk`); PPR dos encoders (`cal`);
                                    altura do sensor (calços)
## 7 Peças de reposição             ← tabela: item | quantidade a ter na caixa | fonte (BOM/STL)
## 8 Registro de manutenção         ← modelo de tabela para anotar data, o que foi feito, por quem
```

- [ ] **Step 1: Escrever** seguindo a estrutura.
- [ ] **Step 2: Rodar o verificador** — `python3 tools/check_manuais.py` → `0 problema(s)`.
- [ ] **Step 3: Commit**

```bash
git add docs/manual/MANUTENCAO.md
git commit -m "docs(manual): maintenance manual with inspections, diagnosis tree and bench tests

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 5: Fiação e BOM da v2

**Files:**
- Modify: `docs/WIRING.md`, `hardware/bom.csv`

- [ ] **Step 1: `docs/WIRING.md`**

- Trocar o diagrama de visão geral por:

```
Suporte 2×18650 (2S, 6,0–8,4 V)
    │
    ├── Fusível 3 A (lâmina mini, porta-fusível em linha)
    │
    ├── P-MOSFET de proteção contra inversão (recomendado)
    │
    └── Chave liga/desliga
            │
            ├── MP1584 buck ──► 5,0 V ──► ESP32 VIN ──► 3V3 ──► MCP3008 + QTR-8A
            │
            └── TB6612FNG VM (tensão do pack)
```

- Seção "Bateria e proteção": por que fusível de 3 A (dois N20 em travamento a 8,4 V somam
  pouco mais de 2 A, TB6612 aguenta 3,2 A de pico por canal); como ligar o P-MOSFET (dreno no
  +BAT, fonte na carga, gate no GND por resistor de 10 kΩ, com zener de 12 V gate-fonte).
- Divisor de bateria: conferir 100 kΩ / 47 kΩ → 8,4 V vira 2,69 V, dentro do ADC com
  atenuação de 11 dB; o firmware usa a calibração do eFuse e o fator `vbk`.
- Buzzer: acionado por PWM (LEDC canal 2, 2,5 kHz) — funciona com buzzer passivo e ativo.
- Resistores de pull-up de 10 kΩ nos GPIO 36 e 39 (mantém o aviso existente).
- Pinos de *strapping*: copiar a conclusão do `docs/REVISAO_FIRMWARE.md` (GPIO 12 do MISO e o
  que fazer).

- [ ] **Step 2: `hardware/bom.csv`**

Sair: LiPo 2S 800 mAh, carregador B3, ball caster, "Roda silicone 34mm", "Kit O-rings
silicone 34mm" (substituído). Entrar (mesmas colunas do CSV; preço sem fonte com
"estimado" nas notas):

| qty | component | notas obrigatórias |
|-----|-----------|--------------------|
| 1 | Suporte 2×18650 série com fios | Ficha 12; MEDIR e preencher `medidas.toml` |
| 2 | Célula 18650 high-drain (≥ 20 A) | Ficha 11; mesmo modelo e lote |
| 1 | Carregador de Li-ion 18650 (2 baias) | Carregar fora do carro |
| 4 | O-ring silicone 34 × 3 mm | Ficha 13; 2 em uso + 2 reserva |
| 10 | Inserto térmico M3 (OD 4,0 × 5,7 mm) | Colunas do perfboard, língua da barra |
| 1 | Fusível lâmina mini 3 A + porta-fusível em linha | Logo após o suporte |
| 1 | P-MOSFET (ex.: AO3401) + zener 12 V + resistor 10 kΩ | Proteção contra inversão (recomendado) |
| — | Parafusos | Conferir a lista do relatório de montagem: 4× M2 (abraçadeiras), 2× M2 (patins), 2× M2 (QTR-8A), 2× M3 (barra), 4× M3 (perfboard), 2× M2 trava das rodas (opcional) |

Atualizar o total estimado com a soma das linhas e a nota "NÃO inclui o QTR-8A".

- [ ] **Step 3: Reconciliar o manual de montagem** — a tabela 1.1 do `MONTAGEM.md` tem que bater
  com a BOM nova (quantidades e nomes).

- [ ] **Step 4: Verificar e commitar**

Run: `python3 tools/check_manuais.py && cd tools && python3 -m unittest discover -s tests 2>&1 | tail -2`
Expected: `0 problema(s)` e `OK`.

```bash
git add docs/WIRING.md hardware/bom.csv docs/manual/MONTAGEM.md
git commit -m "docs(wiring,bom): 2x18650 power path with fuse and reverse-polarity protection

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 6: README, CLAUDE.md, DIVERGENCIAS e TUNING

**Files:**
- Modify: `README.md`, `CLAUDE.md`, `docs/DIVERGENCIAS.md`, `docs/TUNING.md`

- [ ] **Step 1: README** — seções de hardware para a v2: tabela de hardware (bateria 2×18650,
  rodas impressas com O-ring, patins); "Chassi 3D" vira "Modelo 3D (build123d)" com
  `hardware/cad/`, `medidas.toml`, `build.py`, `out/`; nova seção "Manuais" com links para os
  três Markdown e o HTML; remover instruções de OpenSCAD. Não reescrever as seções de firmware
  que o WP1 atualizou — só conferir que batem com o código.
- [ ] **Step 2: CLAUDE.md** — tabela da stack (bateria, chassi, rodas), árvore de arquitetura
  com os módulos novos (`config/`, `comm/CommandProtocol`, `CommandHandler`, `Mailbox`,
  `strategy/ControlLoop`, `RobotStateMachine`, `sensors/ButtonInput`, `BatteryMonitor`,
  `motors/OutputStage`) e sem `DifferentialDrive`; árvore de testes atual
  (`ls test/`); "Hardware Constraints": **Core 1: controle; Core 0: BLE + telemetria**;
  "Parâmetros de Tuning (Defaults Iniciais)" substituído por um parágrafo apontando para
  `src/config/RobotConfig.h` (valores de fábrica) e o manual do operador; BOM resumida da v2.
- [ ] **Step 3: DIVERGENCIAS** — conferir que todas as linhas refletem o estado da branch
  (D1–D13); fechar as que o WP1 resolveu (D5 continua "Aceito"; D8: sensor agora a 3 mm,
  marcar **Resolvido**).
- [ ] **Step 4: TUNING** — conferir links para `docs/manual/OPERACAO.md` (seções 7–9).
- [ ] **Step 5: Verificar e commitar**

Run: `python3 tools/check_manuais.py`
Expected: `0 problema(s)`.

```bash
git add README.md CLAUDE.md docs/DIVERGENCIAS.md docs/TUNING.md
git commit -m "docs: README, CLAUDE.md and divergence log updated for LFR v2

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

---

### Task 7: Manual HTML interativo

**Files:**
- Create: `docs/manual/manual-lfr.html`

**Antes de escrever:** o coordenador roda o `quickstart` do Artifact e repassa as diretrizes
de design no prompt desta tarefa (plano mestre, fase P5). Seguir essas diretrizes.

Requisitos (spec §6):

1. **Um arquivo só**, que abre com duplo clique (sem servidor, sem rede) e também pode ser
   publicado como página. Começa com `<title>Manual LFR</title>` e `<style>`, **sem**
   `<html>`, `<head>` nem `<body>` (a publicação acrescenta o esqueleto; navegadores abrem o
   arquivo assim mesmo). Nenhum recurso externo: CSS e JS embutidos, diagramas em SVG
   embutido, nada de fonte da web.
2. **Abas por papel**: Montador, Operador, Manutenção, mais "Referência" (parâmetros e
   protocolo). A aba ativa fica no `#hash` da URL.
3. **Checklists marcáveis** (montagem §8, box §12, inspeção periódica) com estado salvo em
   `localStorage` dentro de `try/catch` (a página funciona sem ele) e um botão "Limpar
   marcações" por lista, com confirmação na própria página (sem `confirm()`).
4. **Diagramas SVG**: (a) fiação de alimentação da Tarefa 5; (b) vista de cima do carro com
   eixo motriz, rodas, berço da bateria, barra de sensores, patins e cotas principais —
   lookahead, bitola e comprimento total, tirados do relatório de montagem; (c) máquina de
   estados IDLE → CALIBRATING → READY → ARMED → RUNNING → ERROR com os eventos.
5. **Tabela de parâmetros** igual à seção 5 do `OPERACAO.md`, com filtro por texto.
6. **Árvore de diagnóstico** navegável (sintoma → perguntas → ação), com o mesmo conteúdo do
   `MANUTENCAO.md` §3.
7. **Tema claro e escuro** por `prefers-color-scheme` e por `data-theme` no elemento raiz;
   legível em celular (≥ 360 px de largura, sem rolagem horizontal fora das tabelas).
8. Conteúdo **derivado** dos três Markdown: nenhuma informação que não esteja neles. Rodapé
   com "Fonte: docs/manual/*.md — commit <hash>".

- [ ] **Step 1: Escrever o HTML.**
- [ ] **Step 2: Verificar offline** — abrir `file://…/docs/manual/manual-lfr.html` com o
  Playwright (`mcp__plugin_playwright_playwright__browser_navigate`) ou, sem ele, conferir que
  `grep -n "http" docs/manual/manual-lfr.html` só encontra URLs em texto/links de referência,
  nunca em `src=` ou `@import`. Marcar uma caixa, recarregar, conferir que continua marcada;
  testar largura de 375 px (`browser_resize`).
- [ ] **Step 3: Rodar o verificador** — `python3 tools/check_manuais.py` → `0 problema(s)`.
- [ ] **Step 4: Commit**

```bash
git add docs/manual/manual-lfr.html
git commit -m "docs(manual): offline interactive HTML manual by role

Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"
```

- [ ] **Step 5: Registrar o fim do WP3 no ruflo** — `memory_store` (`lfr-2026`,
  `wp3/status`) e `task_complete` na tarefa do WP3.
