#!/usr/bin/env python3
"""Render the 8x16 font in `source/font-8x16.cpp` as ASCII.

Assumption (from repo README + common packed-font layout):
- Each glyph is 16 bytes.
- Each byte represents one horizontal row of 8 pixels.
- Bit 7 is the left-most pixel, bit 0 is the right-most pixel.

If the output looks rotated/flipped, run with `--rotate`/`--lsb-left`.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple


FONT_CPP_DEFAULT = Path("font-8x16.cpp")
FIRST_CODEPOINT = 0x20  # font_data[0] is space
GLYPH_W = 8
GLYPH_H = 16


def _extract_font_rows(cpp_text: str) -> List[List[int]]:
    """Return list of glyphs, each a list of 16 ints (0-255)."""

    # Pull out the body of: extern "C" const uint8_t font_data[95][16] = { ... };
    m = re.search(
        r"font_data\s*\[\s*\d+\s*\]\s*\[\s*\d+\s*\]\s*=\s*\{(.*)\}\s*;",
        cpp_text,
        flags=re.S,
    )
    if not m:
        raise ValueError("Couldn't find font_data array in C++ file")

    body = m.group(1)

    # Each glyph row looks like: {0x00, 0x01, ...}, // comment
    glyph_rows = re.findall(r"\{([^}]*)\}\s*,?\s*(?://.*)?$", body, flags=re.M)
    glyphs: List[List[int]] = []
    for row in glyph_rows:
        nums = re.findall(r"0x[0-9A-Fa-f]{1,2}|\d+", row)
        if not nums:
            continue
        values = [int(n, 0) for n in nums]
        if len(values) != GLYPH_H:
            # Some other brace block could match; ignore unless it looks close.
            continue
        glyphs.append(values)

    if not glyphs:
        raise ValueError("Parsed 0 glyphs; regex likely failed")

    return glyphs


def _byte_to_bits(b: int, *, msb_left: bool) -> List[int]:
    if msb_left:
        return [1 if (b & (1 << (7 - i))) else 0 for i in range(GLYPH_W)]
    return [1 if (b & (1 << i)) else 0 for i in range(GLYPH_W)]


def render_glyph(
    glyph_bytes: Sequence[int],
    *,
    on: str = "x",
    off: str = "-",
    msb_left: bool = True,
    rotate: bool = False,
) -> List[str]:
    """Render to list of lines."""

    rows_bits = [_byte_to_bits(b, msb_left=msb_left) for b in glyph_bytes]

    if rotate:
        # Rotate 90deg clockwise: input is 16x8 -> output is 8x16
        rotated: List[List[int]] = []
        for y in range(GLYPH_W):
            rotated.append([rows_bits[GLYPH_H - 1 - x][y] for x in range(GLYPH_H)])
        rows_bits = rotated

    return ["".join(on if bit else off for bit in row) for row in rows_bits]


def iter_selected_glyphs(
    glyphs: Sequence[Sequence[int]],
    selection: str | None,
) -> Iterable[Tuple[int, Sequence[int]]]:
    if not selection:
        for idx, g in enumerate(glyphs):
            yield idx, g
        return

    # selection can be:
    # - a literal string (e.g., "Hello")
    # - a range like "0x20-0x7E" or "32-126"
    s = selection

    range_m = re.fullmatch(r"\s*(0x[0-9A-Fa-f]+|\d+)\s*-\s*(0x[0-9A-Fa-f]+|\d+)\s*", s)
    if range_m:
        a = int(range_m.group(1), 0)
        b = int(range_m.group(2), 0)
        lo, hi = sorted((a, b))
        for codepoint in range(lo, hi + 1):
            idx = codepoint - FIRST_CODEPOINT
            if 0 <= idx < len(glyphs):
                yield idx, glyphs[idx]
        return

    # Otherwise treat as text
    for ch in s:
        idx = ord(ch) - FIRST_CODEPOINT
        if 0 <= idx < len(glyphs):
            yield idx, glyphs[idx]


def main() -> int:
    ap = argparse.ArgumentParser(description="Render the 8x16 font as ASCII")
    ap.add_argument(
        "--font",
        type=Path,
        default=FONT_CPP_DEFAULT,
        help="Path to C++ font file (default: source/font-8x16.cpp)",
    )
    ap.add_argument(
        "--select",
        help='Text to render (e.g. "Hello") OR range (e.g. "0x41-0x5A"). If omitted, renders all glyphs.',
    )
    ap.add_argument("--on", default="x", help="Character for set pixels")
    ap.add_argument("--off", default="-", help="Character for unset pixels")
    ap.add_argument(
        "--lsb-left",
        action="store_true",
        help="Treat bit 0 as left-most pixel (default is MSB-left)",
    )
    ap.add_argument(
        "--rotate",
        action="store_true",
        help="Rotate output 90° clockwise (useful if the bit packing assumption is flipped)",
    )
    args = ap.parse_args()

    cpp_text = args.font.read_text(encoding="utf-8", errors="replace")
    glyphs = _extract_font_rows(cpp_text)

    for idx, glyph in iter_selected_glyphs(glyphs, args.select):
        codepoint = idx + FIRST_CODEPOINT
        ch = chr(codepoint)
        label = (
            f"0x{codepoint:02X} {repr(ch)[1:-1]}"
            if 32 <= codepoint <= 126
            else f"0x{codepoint:02X}"
        )
        print(label)
        lines = render_glyph(
            glyph,
            on=args.on,
            off=args.off,
            msb_left=(not args.lsb_left),
            rotate=args.rotate,
        )
        print("\n".join(lines))
        print()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
