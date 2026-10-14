#!/usr/bin/env python3
"""Confere se os manuais só citam arquivos, chaves, comandos, ambientes e STLs que existem.

Uso: python3 tools/check_manuais.py   (sai com 1 se houver problema)

Regras (plano WP3, Tarefa 1):
1. Link Markdown relativo e href relativo → o arquivo existe (âncoras e URLs são ignoradas).
2. Caminho entre crases com "/" e extensão conhecida → existe, relativo à raiz do repo.
3. "k":"<chave>" e linhas de tabela | `<chave>` | → chave do RobotConfig.cpp. Num frame
   {"t":"ack",...} o "k" também pode ser o nome de um comando (ack de save, start etc.).
4. {"t":"<cmd>" → comando aceito pelo parseCommand ou frame enviado pelo robô.
5. bench_<nome> → [env:bench_<nome>] no platformio.ini.
6. <nome>.stl → existe em hardware/cad/out/.
7. Cada código de erro do firmware (enum ErrorCode) tem linha | n | no OPERACAO.md e
   aparece como ERRO(n) no MANUTENCAO.md.
8. HTML: nenhum recurso externo carregado (src=, <link href=, @import, url()) — o manual
   tem que abrir sem rede.

Comandos, frames e códigos de erro são lidos do firmware, não fixados aqui.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

RAIZ = Path(__file__).resolve().parents[1]
EXTENSOES = (".md", ".cpp", ".h", ".py", ".ini", ".csv", ".stl", ".step", ".html", ".toml",
             ".svg")
PADRAO_ARQUIVOS = [
    "docs/manual/*.md", "docs/manual/manual-lfr.html", "docs/WIRING.md", "README.md",
]
_EXTERNO = re.compile(r"""(?:\bsrc\s*=\s*["']?|<link\b[^>]*\bhref\s*=\s*["']?|@import\s+(?:url\()?\s*["']?|\burl\(\s*["']?)(?:https?:)?//""",
                      re.I)


def chaves_firmware(raiz: Path) -> set[str]:
    fonte = (raiz / "src" / "config" / "RobotConfig.cpp").read_text(encoding="utf-8")
    return set(re.findall(r"(?:HW_KEY|TP_KEY)\((\w+)", fonte))


def comandos_firmware(raiz: Path) -> set[str]:
    """Valores de "t" aceitos pelo parseCommand (CommandProtocol.cpp)."""
    fonte = (raiz / "src" / "comm" / "CommandProtocol.cpp").read_text(encoding="utf-8")
    return (set(re.findall(r'strcmp\(t,\s*"(\w+)"\)', fonte))
            | set(re.findall(r'\{"(\w+)",\s*CmdType::', fonte)))


def frames_firmware(raiz: Path) -> set[str]:
    """Valores de "t" dos frames que o robô envia (format* do CommandProtocol.cpp)."""
    fonte = (raiz / "src" / "comm" / "CommandProtocol.cpp").read_text(encoding="utf-8")
    return set(re.findall(r'\\"t\\":\\"(\w+)\\"', fonte))


def codigos_erro(raiz: Path) -> dict[int, str]:
    """{código: nome} do enum ErrorCode, sem o NONE (0)."""
    fonte = (raiz / "src" / "strategy" / "RobotStateMachine.h").read_text(encoding="utf-8")
    corpo = re.search(r"enum\s+class\s+ErrorCode[^{]*\{([^}]*)\}", fonte)
    if not corpo:
        return {}
    pares = re.findall(r"(\w+)\s*=\s*(\d+)", corpo.group(1))
    return {int(v): n for n, v in pares if int(v) != 0}


def ambientes(raiz: Path) -> set[str]:
    ini = (raiz / "platformio.ini").read_text(encoding="utf-8")
    return set(re.findall(r"\[env:(bench_\w+)\]", ini))


def _chaves_citadas(texto: str, chaves: set[str], comandos: set[str]) -> list[str]:
    """Chaves desconhecidas em "k":"…" (frames ack aceitam nome de comando)."""
    ruins: list[str] = []
    resto = texto
    for obj in re.findall(r"\{[^{}]*\}", texto):
        validas = chaves | comandos if re.search(r'"t"\s*:\s*"ack"', obj) else chaves
        ruins += [k for k in re.findall(r'"k"\s*:\s*"(\w+)"', obj) if k not in validas]
        resto = resto.replace(obj, " ", 1)
    ruins += [k for k in re.findall(r'"k"\s*:\s*"(\w+)"', resto) if k not in chaves]
    ruins += [k for k in re.findall(r"^\|\s*`(\w+)`\s*\|", texto, flags=re.M)
              if k not in chaves]
    return ruins


def verificar(arquivos: list[Path], raiz: Path = RAIZ) -> list[str]:
    chaves = chaves_firmware(raiz)
    comandos = comandos_firmware(raiz)
    frames = frames_firmware(raiz)
    codigos = codigos_erro(raiz)
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
        for chave in _chaves_citadas(texto, chaves, comandos):
            problemas.append(f"{nome}: chave de configuração desconhecida → {chave}")
        for cmd in re.findall(r'\{\s*"t"\s*:\s*"(\w+)"', texto):
            if cmd not in comandos and cmd not in frames:
                problemas.append(f"{nome}: comando BLE desconhecido → {cmd}")
        for env in re.findall(r"\b(bench_\w+)", texto):
            if env not in envs:
                problemas.append(f"{nome}: ambiente PlatformIO inexistente → {env}")
        for stl in re.findall(r"\b([\w-]+\.stl)\b", texto):
            if stl not in stls:
                problemas.append(f"{nome}: STL inexistente em hardware/cad/out → {stl}")
        if nome == "OPERACAO.md":
            for c, n in sorted(codigos.items()):
                if not re.search(rf"^\|\s*{c}\s*\|", texto, flags=re.M):
                    problemas.append(f"{nome}: código de erro {c} ({n}) sem linha | {c} | "
                                     "na tabela de códigos")
        if nome == "MANUTENCAO.md":
            for c, n in sorted(codigos.items()):
                if f"ERRO({c})" not in texto:
                    problemas.append(f"{nome}: código de erro {c} ({n}) fora do diagnóstico "
                                     f"(falta ERRO({c}))")
        if nome.endswith(".html"):
            for m in _EXTERNO.finditer(texto):
                trecho = texto[m.start():m.end() + 30].split("\n")[0]
                problemas.append(f"{nome}: recurso externo (não abre sem rede) → {trecho}")
    return problemas


def main() -> int:
    arquivos = [p for padrao in PADRAO_ARQUIVOS for p in sorted(RAIZ.glob(padrao))]
    problemas = verificar(arquivos)
    for p in problemas:
        print(p)
    print(f"{len(arquivos)} arquivo(s), {len(problemas)} problema(s)")
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
