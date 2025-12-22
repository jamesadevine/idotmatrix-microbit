#!/usr/bin/env python3
"""Render the packed font in a `font-*.cpp` file as ASCII.

This repo contains at least two formats:
- 8x16: `font_data[95][16]`  (16 bytes per glyph; 1 byte per row)
- 16x32: `font_data[95][64]` (64 bytes per glyph; 2 bytes per row)

The renderer auto-detects the glyph byte width from the C++ `font_data[..][..]`
declaration and chooses a matching geometry.

If the output looks mirrored, try toggling `--lsb-left`.
If it looks rotated, try `--rotate`.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path
from typing import Iterable, List, Sequence, Tuple


def _default_font_path() -> Path:
    candidates = [
        Path("./source") / "font-16x32.cpp",
        Path("./source") / "font-8x16.cpp",
        Path("./font-16x32.cpp"),
        Path("./font-8x16.cpp"),
    ]
    for p in candidates:
        if p.exists():
            return p
    return candidates[0]


FONT_CPP_DEFAULT = _default_font_path()
FIRST_CODEPOINT = 0x20  # font_data[0] is space


class FontFormatError(ValueError):
    pass


def _extract_declared_glyph_byte_len(cpp_text: str) -> int:
    """Return N from: font_data[...][N]."""

    m = re.search(
        r"font_data\s*\[\s*(\d+)\s*\]\s*\[\s*(\d+)\s*\]",
        cpp_text,
    )
    if not m:
        raise FontFormatError("Couldn't find font_data declaration")
    return int(m.group(2))


def _infer_geometry(glyph_bytes_len: int) -> tuple[int, int, int]:
    """Return (width_px, height_px, bytes_per_row)."""

    if glyph_bytes_len == 16:
        return 8, 16, 1
    if glyph_bytes_len == 64:
        return 16, 32, 2
    raise FontFormatError(
        f"Unsupported glyph byte length {glyph_bytes_len}; expected 16 (8x16) or 64 (16x32)"
    )


def _extract_font_rows(cpp_text: str, *, glyph_bytes_len: int) -> List[List[int]]:
    """Return list of glyphs, each a list of glyph_bytes_len ints (0-255)."""

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
        if len(values) != glyph_bytes_len:
            continue
        glyphs.append(values)

    if not glyphs:
        raise ValueError("Parsed 0 glyphs; regex likely failed")

    return glyphs


def _row_value_to_bits(value: int, *, width_px: int, msb_left: bool) -> List[int]:
    if msb_left:
        return [
            1 if (value & (1 << (width_px - 1 - i))) else 0 for i in range(width_px)
        ]
    return [1 if (value & (1 << i)) else 0 for i in range(width_px)]


def _bytes_to_int(row_bytes: Sequence[int], *, endian: str) -> int:
    b = bytes(int(x) & 0xFF for x in row_bytes)
    if endian not in {"little", "big"}:
        raise ValueError("endian must be 'little' or 'big'")
    return int.from_bytes(b, endian)


def render_glyph(
    glyph_bytes: Sequence[int],
    *,
    on: str = "x",
    off: str = "-",
    msb_left: bool = True,
    rotate: bool = False,
    width_px: int,
    height_px: int,
    bytes_per_row: int,
    endian: str,
) -> List[str]:
    """Render to list of lines."""

    if len(glyph_bytes) != height_px * bytes_per_row:
        raise FontFormatError(
            f"Glyph byte length mismatch: got {len(glyph_bytes)}, expected {height_px * bytes_per_row}"
        )

    rows_bits: List[List[int]] = []
    for row in range(height_px):
        start = row * bytes_per_row
        row_bytes = glyph_bytes[start : start + bytes_per_row]
        row_value = _bytes_to_int(row_bytes, endian=endian)
        rows_bits.append(
            _row_value_to_bits(row_value, width_px=width_px, msb_left=msb_left)
        )

    if rotate:
        # Rotate 90deg clockwise: output becomes width x height swapped.
        rotated: List[List[int]] = []
        for y in range(width_px):
            rotated.append([rows_bits[height_px - 1 - x][y] for x in range(height_px)])
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
    ap = argparse.ArgumentParser(description="Render a packed font as ASCII")
    ap.add_argument(
        "--font",
        type=Path,
        default=FONT_CPP_DEFAULT,
        help="Path to C++ font file (default: tries source/font-16x32.cpp then source/font-8x16.cpp)",
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
    ap.add_argument(
        "--width",
        type=int,
        help="Override glyph width in pixels (normally auto-detected)",
    )
    ap.add_argument(
        "--height",
        type=int,
        help="Override glyph height in pixels (normally auto-detected)",
    )
    ap.add_argument(
        "--endian",
        choices=["little", "big"],
        default=None,
        help="Byte order for multi-byte rows (16x32 is typically little-endian). Defaults per auto-detected format.",
    )
    args = ap.parse_args()

    print(f"Rendering font from: {args.font} (exists={args.font.exists()})")

    cpp_text = args.font.read_text(encoding="utf-8", errors="replace")
    declared_glyph_bytes_len = _extract_declared_glyph_byte_len(cpp_text)
    auto_w, auto_h, auto_bpr = _infer_geometry(declared_glyph_bytes_len)

    width_px = args.width if args.width is not None else auto_w
    height_px = args.height if args.height is not None else auto_h
    if declared_glyph_bytes_len % height_px != 0:
        raise FontFormatError(
            f"Glyph byte length {declared_glyph_bytes_len} is not divisible by height {height_px}"
        )
    bytes_per_row = declared_glyph_bytes_len // height_px
    endian = args.endian
    if endian is None:
        endian = "big" if bytes_per_row == 1 else "little"

    glyphs = _extract_font_rows(cpp_text, glyph_bytes_len=declared_glyph_bytes_len)

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
            width_px=width_px,
            height_px=height_px,
            bytes_per_row=bytes_per_row,
            endian=endian,
        )
        print("\n".join(lines))
        print()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
