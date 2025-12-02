#ifndef MINIMAL_PNG_H
#define MINIMAL_PNG_H

#include <stdint.h>

/**
 * Minimal PNG encoder for 32x32 RGB images
 * Uses uncompressed DEFLATE to minimize code size and complexity
 * Output buffer must be at least 3200 bytes
 */
class MinimalPNG {
private:
    static const uint32_t crc_table[256];

    uint8_t* output;
    uint32_t offset;

    void writeBytes(const uint8_t* data, uint32_t len);
    void writeU32(uint32_t val);
    uint32_t calculateCRC(const uint8_t* data, uint32_t len);
    void writeChunk(const char* type, const uint8_t* data, uint32_t len);
    uint32_t adler32(const uint8_t* data, uint32_t len);

public:
    /**
     * Encode a 32x32 RGB image to PNG format
     * @param pixels - RGB pixel data (3072 bytes: 32*32*3)
     * @param outBuffer - Output buffer (must be at least 3200 bytes)
     * @return Size of PNG data in bytes
     */
    static uint32_t encodePNG32x32RGB(const uint8_t* pixels, uint8_t* outBuffer);
};

#endif // MINIMAL_PNG_H
