#!/usr/bin/env uv run -s
# /// script
# dependencies = [
#     "Pillow"
# ]
# ///

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FONT_WIDTH = 8  # 16
FONT_HEIGHT = 16  # 32

FONT_PATH = "/Users/jamesdevine/Downloads/trueno-font/TruenoLight-E2pg.otf"

OUTPUT_PATH = "font-8x16.c"

font = ImageFont.truetype(FONT_PATH, 20)
write_handle = Path(OUTPUT_PATH).open("w")

write_handle.write("// Generated font data\n\nconst uint8_t font_data[][16] = {\n")
for char in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789":
    # todo make image the correct size for 16x16, 32x32 and 64x64
    image = Image.new("1", (FONT_WIDTH, FONT_HEIGHT), 0)
    draw = ImageDraw.Draw(image)
    _, _, text_width, text_height = draw.textbbox((0, 0), text=char, font=font)
    text_x = (FONT_WIDTH - text_width) // 2
    text_y = (FONT_HEIGHT - text_height) // 2
    draw.text((text_x, text_y), char, fill=1, font=font)
    bitmap = bytearray()
    for y in range(FONT_HEIGHT):
        for x in range(FONT_WIDTH):
            if x % 8 == 0:
                byte = 0
            pixel = image.getpixel((x, y))
            byte |= (pixel & 1) << (x % 8)
            if x % 8 == 7 or x == FONT_WIDTH - 1:
                bitmap.append(byte)
    write_handle.write("    {")
    for i, byte in enumerate(bitmap):
        write_handle.write(f"0x{byte:02X}")
        if i < len(bitmap) - 1:
            write_handle.write(", ")
    write_handle.write(f"}}, // '{char}'\n")
write_handle.write("};\n")
write_handle.close()
print(f"Font data written to {OUTPUT_PATH}")
