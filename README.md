# Dot matrix app

References:

* https://github.com/derkalle4/python3-idotmatrix-library/tree/main
* https://github.com/markusressel/idotmatrix-api-client/tree/main/idotmatrix/modules
* https://github.com/8none1/idotmatrix/tree/main



# writing character bitmaps

* Each byte is a vertical column of 8 pixels.
* Each bit represents one pixel
* Scanning goes horizontally

`x` marks set, `-` marks not set
```
xx-
xx-
xx-
xx-   -> translates to 0xff, 0xff, 0x00
xx-
xx-
xx-
xx-
```

## render the font

There is a tiny renderer script that parses `source/font-8x16.cpp` and prints glyphs as ASCII.

```sh
python3 render_font.py --select "Aa0!?"
python3 render_font.py --select 0x41-0x5A   # A-Z
python3 render_font.py --select 0x20-0x7E   # all printable
```

If the output looks mirrored/rotated, try:

```sh
python3 render_font.py --select "A" --lsb-left
python3 render_font.py --select "A" --rotate
```