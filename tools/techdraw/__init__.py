"""Primitivas de desenho técnico em SVG conforme normas ABNT.

Unidades: toda geometria de peça em milímetros reais.
A folha mapeia 1 unidade de usuário SVG = 1 mm de papel.
"""
from .sheet import (
    Prov, Sheet,
    SHEET_W, SHEET_H, MARGIN_LEFT, MARGIN_OTHER,
    W_VISIBLE, W_HIDDEN, W_THIN,
    H_DIM, H_LABEL, H_TITLE,
)

__all__ = [
    "Prov", "Sheet",
    "SHEET_W", "SHEET_H", "MARGIN_LEFT", "MARGIN_OTHER",
    "W_VISIBLE", "W_HIDDEN", "W_THIN",
    "H_DIM", "H_LABEL", "H_TITLE",
]

from .geom import View, DASH_HIDDEN, DASH_CENTER

__all__ += ["View", "DASH_HIDDEN", "DASH_CENTER"]

from .dims import dim_h, dim_v, dim_diam, dim_rad, fmt_valor

__all__ += ["dim_h", "dim_v", "dim_diam", "dim_rad", "fmt_valor"]

from .annot import (ISO_COS, ISO_SIN, PartItem, balloon, iso_box, iso_cyl,
                    note, parts_list)

__all__ += ["ISO_COS", "ISO_SIN", "PartItem", "balloon", "iso_box",
            "iso_cyl", "note", "parts_list"]
