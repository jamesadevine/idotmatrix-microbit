#!/usr/bin/env uv run -s
# /// script
# dependencies = [
#     "Pillow"
# ]
# ///

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FONT_WIDTH = 16  # 16
FONT_HEIGHT = 32  # 32

FONT_PATH = "/Users/jamesdevine/Downloads/Rain-DRM3.otf"

OUTPUT_PATH = f"font-{FONT_WIDTH}x{FONT_HEIGHT}.cpp"

SEPARATOR_8_16 = bytearray([0x02, 0xFF, 0xFF, 0xFF])
SEPARATOR_16_32 = bytearray([0x05, 0xFF, 0xFF, 0xFF])

# Calculate bytes per character
BYTES_PER_ROW = (FONT_WIDTH + 7) // 8  # Round up to nearest byte
BYTES_PER_CHAR = BYTES_PER_ROW * FONT_HEIGHT

font = ImageFont.truetype(FONT_PATH, 20)
write_handle = Path(OUTPUT_PATH).open("w")

# Generate all 256 ASCII characters (or at least printable + your set)
write_handle.write(
    f"// Generated font data: {FONT_WIDTH}x{FONT_HEIGHT} pixels, {BYTES_PER_CHAR} bytes per character\n\n"
)
write_handle.write("#include <stdint.h>\n\n")

write_handle.write("// Separator bytes between characters\n")


if FONT_WIDTH == 8 and FONT_HEIGHT == 16:
    write_handle.write('extern "C" const uint32_t BITMAP_SIZE = 16;\n')
    write_handle.write(
        f'extern "C" const uint32_t SEPARATOR_LEN = {len(SEPARATOR_8_16)};\n'
    )
    write_handle.write(
        f'extern "C" const uint8_t separator[{len(SEPARATOR_8_16)}] = {{'
    )
    separator = SEPARATOR_8_16
elif FONT_WIDTH == 16 and FONT_HEIGHT == 32:
    write_handle.write('extern "C" const uint32_t BITMAP_SIZE = 64;\n')
    write_handle.write(
        f'extern "C" const uint32_t SEPARATOR_LEN = {len(SEPARATOR_16_32)};\n'
    )
    write_handle.write(
        f'extern "C" const uint8_t separator[{len(SEPARATOR_16_32)}] = {{'
    )
    separator = SEPARATOR_16_32

for i, byte in enumerate(separator):
    write_handle.write(f"0x{byte:02X}")
    if i < len(separator) - 1:
        write_handle.write(", ")
write_handle.write("};\n\n")

write_handle.write(
    f'extern "C" const uint8_t font_data[{126 - 32 + 1}][{BYTES_PER_CHAR}] = {{\n'
)

# Generate for all ASCII characters (0-255)
for ascii_code in range(256):
    char = chr(ascii_code)

    # Create blank image
    image = Image.new("1", (FONT_WIDTH, FONT_HEIGHT), 0)
    draw = ImageDraw.Draw(image)

    # Only render printable characters
    if 32 <= ascii_code <= 126:
        _, _, text_width, text_height = draw.textbbox((0, 0), text=char, font=font)
        text_x = (FONT_WIDTH - text_width) // 2
        text_y = (FONT_HEIGHT - text_height) // 2
        draw.text((text_x, text_y), char, fill=1, font=font)
    else:
        continue

    bitmap = bytearray()

    # Pack bits row by row
    for y in range(FONT_HEIGHT):
        byte = 0
        bit_pos = 0

        for x in range(FONT_WIDTH):
            pixel = image.getpixel((x, y))
            byte |= (pixel & 1) << bit_pos
            bit_pos += 1

            # When we've filled a byte (or reached end of row), append it
            if bit_pos == 8 or x == FONT_WIDTH - 1:
                bitmap.append(byte)
                byte = 0
                bit_pos = 0

    # Write the byte array
    write_handle.write("    {")
    for i, byte in enumerate(bitmap):
        write_handle.write(f"0x{byte:02X}")
        if i < len(bitmap) - 1:
            write_handle.write(", ")

    # Add readable comment
    if 32 <= ascii_code <= 126:
        write_handle.write(f"}}, // 0x{ascii_code:02X} '{char}'\n")
    else:
        write_handle.write(f"}}, // 0x{ascii_code:02X}\n")
write_handle.write("};\n")
write_handle.close()
print(f"Font data written to {OUTPUT_PATH}")
