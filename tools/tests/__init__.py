"""Suíte de testes das ferramentas de desenho.

Rode sempre a partir de `tools/`:
    python3 -m unittest discover -s tests

Este pacote põe `hardware/components/` no sys.path para que os testes possam
importar `specs` do mesmo jeito que os geradores fazem.
"""
import os
import sys

_RAIZ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

for _p in (os.path.join(_RAIZ, "tools"),
           os.path.join(_RAIZ, "hardware", "components")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
