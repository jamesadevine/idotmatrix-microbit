#include "MicroBit.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gattc.h"
#include "nrf_sdh_ble.h"
#include <string.h>
#include <stdio.h>

MicroBit uBit;

// CRC32 lookup table (IEEE 802.3 polynomial: 0xEDB88320)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

// Calculate CRC32 checksum
uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

extern "C" const uint8_t font_data[];
extern "C" const uint32_t BITMAP_SIZE;
extern "C" const uint32_t SEPARATOR_LEN;
extern "C" const uint8_t separator[];

// Characteristic UUIDs (128-bit)
const uint8_t UUID_WRITE_DATA_128[] = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                                        0x00, 0x10, 0x00, 0x00, 0x02, 0xfa, 0x00, 0x00};
const uint8_t UUID_READ_DATA_128[]  = {0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80,
                                        0x00, 0x10, 0x00, 0x00, 0x03, 0xfa, 0x00, 0x00};

static bool connected = false;
static uint16_t write_char_handle = BLE_GATT_HANDLE_INVALID;
static uint8_t uuid_type = 0;
static bool discovery_in_progress = false;
static volatile bool write_in_progress = false;

static uint32_t CHUNK_SIZE = 20; // BLE default MTU size minus overhead

struct iDotMatrixDisplay {
    uint16_t packet_length;
    uint8_t command;
    uint8_t subcommand;
    uint8_t first_or_continuation;
    uint32_t image_data_length;
    uint8_t pixel_data[32 * 32 * 3];
} __attribute__((packed));


struct TextHeader {
    uint16_t total_len;
    uint8_t static_3;
    uint8_t static_0;
    uint8_t static_0_1;
    uint32_t packet_length;
    uint32_t crc;
    uint8_t static_0_2;
    uint8_t static_0_3;
    uint8_t static_12;
} __attribute__((packed));
struct TextMetadata {
    uint16_t number_of_characters;
    uint8_t static_0;
    uint8_t static_1;
    uint8_t text_mode; // Text mode. Defaults to 0. 0 = replace text, 1 = marquee, 2 = reversed marquee, 3 = vertical rising marquee, 4 = vertical lowering marquee, 5 = blinking, 6 = fading, 7 = tetris, 8 = filling
    uint8_t text_speed;
    uint8_t text_color_mode; // Text colour mode. 0 = white, 1 = use given RGB color, 2,3,4,5 = rainbow modes
    uint8_t text_color_r; // Text colour RGB (if colour mode = 1)
    uint8_t text_color_g; // Text colour RGB (if colour mode = 1)
    uint8_t text_color_b; // Text colour RGB (if colour mode = 1)
    uint8_t text_color_bg_mode; // Text Background Mode. Defaults to 0. 0 = black, 1 = use given RGB color
    uint8_t bg_color_r; // Background colour RGB (if background mode = 1)
    uint8_t bg_color_g; // Background colour RGB (if background mode = 1)
    uint8_t bg_color_b; // Background colour RGB (if background mode = 1)
    uint8_t character_bitmaps[];
} __attribute__((packed));;


const uint8_t SCOREBOARD_PACKET[] = {
    8,  // Command start
    0,  // Placeholder
    10,  // Command ID
    128,  // Another command specifier
    0,  // Lower byte of count1
    0,  // Upper byte of count1
    0,  // Lower byte of count2
    0,  // Upper byte of count2
};

const uint8_t IMAGE_MODE_PACKET[] = {
    5,
    0,
    4,
    1,
    1 % 256, // mode 1 = enable DIY
};

extern "C" void log_string(const char* str)
{
    uBit.serial.printf("%s\r\n", str);
}

#define HEADER_SIZE 9

struct iDotMatrixDisplay display_buffer; // RGB buffer for 32x32 image
uint8_t png_buffer[3200 + HEADER_SIZE]; // Buffer for PNG data



// Register the custom 128-bit UUID and discover characteristics
void discover_write_characteristic()
{
    if (discovery_in_progress) return;

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("No connection handle!\r\n");
        return;
    }

    discovery_in_progress = true;

    // Register the custom UUID base
    ble_uuid128_t base_uuid;
    memcpy(base_uuid.uuid128, UUID_WRITE_DATA_128, 16);

    uint32_t err = sd_ble_uuid_vs_add(&base_uuid, &uuid_type);
    if (err != NRF_SUCCESS)
    {
        uBit.serial.printf("UUID add failed: 0x%lx\r\n", err);
        discovery_in_progress = false;
        return;
    }

    uBit.serial.printf("UUID registered, type: %d\r\n", uuid_type);

    // Start discovering all characteristics (we'll filter in the event handler)
    ble_gattc_handle_range_t range;
    range.start_handle = 0x0001;
    range.end_handle = 0xFFFF;

    err = sd_ble_gattc_characteristics_discover(conn_handle, &range);
    if (err != NRF_SUCCESS)
    {
        uBit.serial.printf("Characteristic discovery failed: 0x%lx\r\n", err);
        discovery_in_progress = false;
    }
    else
    {
        uBit.serial.printf("Started characteristic discovery\r\n");
    }
}

int write_text(ManagedString s) {
    if (write_char_handle == BLE_GATT_HANDLE_INVALID)
    {
        uBit.serial.printf("Characteristic not discovered yet!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("Not connected!\r\n");
        return DEVICE_INVALID_STATE;
    }

    memset(&display_buffer.pixel_data, 0, sizeof(display_buffer.pixel_data));

    struct TextHeader* hdr = (struct TextHeader*)(display_buffer.pixel_data);
    hdr->total_len = 0;
    hdr->static_3 = 3;
    hdr->static_0 = 0;
    hdr->static_0_1 = 0;
    hdr->packet_length = 0;
    hdr->crc = 0;
    hdr->static_0_2 = 0;
    hdr->static_0_3 = 0;
    hdr->static_12 = 12;

    struct TextMetadata* pkt = (struct TextMetadata*)(display_buffer.pixel_data + sizeof(TextHeader));
    pkt->number_of_characters = s.length();
    pkt->static_0 = 0;
    pkt->static_1 = 1;
    pkt->text_mode = 7;
    pkt->text_speed = 95;
    pkt->text_color_mode = 1;
    pkt->text_color_r = 255;
    pkt->text_color_g = 0;
    pkt->text_color_b = 0;
    pkt->text_color_bg_mode = 0;
    pkt->bg_color_r = 0;
    pkt->bg_color_g = 0;
    pkt->bg_color_b = 0;

    uint32_t max_packet_len = sizeof(display_buffer.pixel_data) - sizeof(TextMetadata) - sizeof(TextHeader);
    uint8_t* write_ptr = pkt->character_bitmaps;
    // const uint8_t separator[] = "\x02\xff\xff\xff";
    // const uint8_t separator[] = "\x05\xff\xff\xff";
    // const uint8_t bitmap[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01, 0xC0, 0x01, 0x40, 0x01, 0x40, 0x03, 0x60, 0x02, 0x20, 0x06, 0x30, 0x04, 0x10, 0x0C, 0xF0, 0x0F, 0x08, 0x08, 0x08, 0x18, 0x0C, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint32_t character_bitmap_size = 0;

    for (int i = 0; i < s.length(); i++) {
        char c = s.charAt(i);

        uBit.serial.printf("Processing character: %c\r\n", c);

        uint32_t char_index = (uint8_t)c;

        if (c>=32 && c<=126)
            char_index -= 32;
        else
            char_index = 0; // Replace unsupported characters with space

        uBit.serial.printf("Character index: %d\r\n", char_index);

        // Add separator
        memcpy(write_ptr, separator, SEPARATOR_LEN);
        write_ptr += SEPARATOR_LEN;

        character_bitmap_size += SEPARATOR_LEN;

        memcpy(write_ptr, &font_data[char_index * BITMAP_SIZE], BITMAP_SIZE);
        write_ptr += BITMAP_SIZE;

        character_bitmap_size += BITMAP_SIZE;
    }

    size_t payload_size =  sizeof(TextMetadata) + character_bitmap_size;
    hdr->total_len = payload_size + sizeof(TextHeader);
    hdr->packet_length = payload_size;
    hdr->crc = calculate_crc32((uint8_t*)pkt, sizeof(TextMetadata) + character_bitmap_size);

    uint32_t dataSize = payload_size + sizeof(TextHeader);
    uint8_t* dataPtr = (uint8_t*) display_buffer.pixel_data;

    uBit.serial.printf("Starting image write of %d bytes\r\n", dataSize);

    for (int i = 0; i < dataSize; i++) {
        uBit.serial.printf("%x ", dataPtr[i]);
    }

    uBit.serial.printf("\r\n");

    while (dataSize > 0)
    {
        ble_gattc_write_params_t params;
        memset(&params, 0, sizeof(params));
        params.write_op = BLE_GATT_OP_WRITE_CMD;  // Write without response
        params.handle = write_char_handle;
        params.len = min(CHUNK_SIZE, dataSize);
        params.p_value = (uint8_t*)dataPtr;
        params.offset = 0;

        uint32_t err;
        // Retry if resources unavailable (TX buffer full)
        do {
            err = sd_ble_gattc_write(conn_handle, &params);
            if (err == NRF_ERROR_RESOURCES)
            {
                // Wait for TX buffer to drain
                fiber_sleep(1);
            }
            else if (err != NRF_SUCCESS)
            {
                uBit.serial.printf("Image write failed: 0x%x\r\n", err);
                return DEVICE_INVALID_STATE;
            }
        } while (err == NRF_ERROR_RESOURCES);

        dataPtr += CHUNK_SIZE;
        dataSize -= min(CHUNK_SIZE, dataSize);

        // Optional: small delay between chunks for reliability
        // fiber_sleep(1);
    }

    uBit.serial.printf("Image write complete\r\n");
    return DEVICE_OK;
}

int set_mode() {
    if (write_char_handle == BLE_GATT_HANDLE_INVALID)
    {
        uBit.serial.printf("Characteristic not discovered yet!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("Not connected!\r\n");
        return DEVICE_INVALID_STATE;
    }

    ble_gattc_write_params_t params;
    memset(&params, 0, sizeof(params));
    params.write_op = BLE_GATT_OP_WRITE_REQ;
    params.handle = write_char_handle;
    params.len = sizeof(IMAGE_MODE_PACKET);
    params.p_value = (uint8_t*)IMAGE_MODE_PACKET;
    params.offset = 0;

    write_in_progress = true;
    uint32_t err = sd_ble_gattc_write(conn_handle, &params);
    if (err == NRF_SUCCESS)
    {
        uBit.serial.printf("Mode write queued\r\n");
        while(write_in_progress)
        {
            fiber_sleep(1);
        }
        return DEVICE_OK;
    }
    else
    {
        uBit.serial.printf("Mode write failed: 0x%lx\r\n", err);
        return DEVICE_INVALID_STATE;
    }
}

int write_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b)
{
    if (write_char_handle == BLE_GATT_HANDLE_INVALID)
    {
        uBit.serial.printf("Characteristic not discovered yet!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("Not connected!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint8_t set_pixel_buffer[] = {
        10,
        0,
        5,
        1,
        0,
        r,
        g,
        b,
        x,
        y,
    };

    ble_gattc_write_params_t params;
    memset(&params, 0, sizeof(params));
    params.write_op = BLE_GATT_OP_WRITE_REQ;
    params.handle = write_char_handle;
    params.len = sizeof(set_pixel_buffer);
    params.p_value = (uint8_t*)set_pixel_buffer;
    params.offset = 0;

    write_in_progress = true;
    uint32_t err = sd_ble_gattc_write(conn_handle, &params);
    if (err != NRF_SUCCESS)
    {
        uBit.serial.printf("Pixel write failed: 0x%lx\r\n", err);
        return DEVICE_INVALID_STATE;
    }
    while(write_in_progress)
    {
        fiber_sleep(1);
    }

    return DEVICE_OK;
}

int write_image(const uint8_t* imageData, size_t dataSize)
{

    // uint32_t pngSize = MinimalPNG::encodePNG32x32RGB(display_buffer.pixel_data, png_buffer);
    uint32_t pngSize = 32 * 32 * 3;

    if (write_char_handle == BLE_GATT_HANDLE_INVALID)
    {
        uBit.serial.printf("Characteristic not discovered yet!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("Not connected!\r\n");
        return DEVICE_INVALID_STATE;
    }

    display_buffer.packet_length = pngSize + HEADER_SIZE;
    display_buffer.command = 0;
    display_buffer.subcommand = 0;
    display_buffer.first_or_continuation = 0;
    display_buffer.image_data_length = pngSize;

    // manually construct header - big endian
    // png_buffer[0] = (pngSize16 >> 8) & 0xFF;
    // png_buffer[1] = (pngSize16 >> 0) & 0xFF;
    // png_buffer[2] = 0;
    // png_buffer[3] = 0;
    // png_buffer[4] = 0; // 2 if i is greater than 0
    // png_buffer[5] = (pngSizeInt >> 24) & 0xFF;
    // png_buffer[6] = (pngSizeInt >> 16) & 0xFF;
    // png_buffer[7] = (pngSizeInt >> 8) & 0xFF;
    // png_buffer[8] = (pngSizeInt >> 0) & 0xFF;

    uint8_t *pngPtr = (uint8_t*)&display_buffer;

    uBit.serial.printf("Starting image write of %d bytes\r\n", pngSize + HEADER_SIZE);

    dataSize = display_buffer.packet_length;

    while (dataSize > 0)
    {
        ble_gattc_write_params_t params;
        memset(&params, 0, sizeof(params));
        params.write_op = BLE_GATT_OP_WRITE_CMD;  // Write without response
        params.handle = write_char_handle;
        params.len = min(CHUNK_SIZE, dataSize);
        params.p_value = (uint8_t*)pngPtr;
        params.offset = 0;

        uint32_t err;
        // Retry if resources unavailable (TX buffer full)
        do {
            err = sd_ble_gattc_write(conn_handle, &params);
            if (err == NRF_ERROR_RESOURCES)
            {
                // Wait for TX buffer to drain
                fiber_sleep(1);
            }
            else if (err != NRF_SUCCESS)
            {
                uBit.serial.printf("Image write failed: 0x%x\r\n", err);
                return DEVICE_INVALID_STATE;
            }
        } while (err == NRF_ERROR_RESOURCES);

        pngPtr += CHUNK_SIZE;
        dataSize -= min(CHUNK_SIZE, dataSize);

        // Optional: small delay between chunks for reliability
        // fiber_sleep(1);
    }

    uBit.serial.printf("Image write complete\r\n");
    return DEVICE_OK;
}

// Write text to the characteristic
int write_score(uint32_t score, uint32_t score1)
{
    if (write_char_handle == BLE_GATT_HANDLE_INVALID)
    {
        uBit.serial.printf("Characteristic not discovered yet!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
    if (conn_handle == BLE_CONN_HANDLE_INVALID)
    {
        uBit.serial.printf("Not connected!\r\n");
        return DEVICE_INVALID_STATE;
    }

    uint8_t packet[sizeof(SCOREBOARD_PACKET)];
    memcpy(packet, SCOREBOARD_PACKET, sizeof(SCOREBOARD_PACKET));

    // Set scores in the packet
    packet[4] = score & 0xFF;          // Lower byte of score
    packet[5] = (score >> 8) & 0xFF;
    packet[6] = score1 & 0xFF;        // Lower byte of score1
    packet[7] = (score1 >> 8) & 0xFF;

    ble_gattc_write_params_t params;
    memset(&params, 0, sizeof(params));
    params.write_op = BLE_GATT_OP_WRITE_REQ;
    params.handle = write_char_handle;
    params.len = sizeof(packet);
    params.p_value = packet;
    params.offset = 0;

    uint32_t err = sd_ble_gattc_write(conn_handle, &params);
    if (err == NRF_SUCCESS)
    {
        uBit.serial.printf("Write queued\r\n");
        return DEVICE_OK;
    }
    else
    {
        uBit.serial.printf("Write failed: 0x%lx\r\n", err);
        return DEVICE_INVALID_STATE;
    }
}

// BLE event handler for GATTC events
// This needs to be registered with NRF_SDH_BLE_OBSERVER
static void gattc_event_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    (void)p_context;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GATTC_EVT_CHAR_DISC_RSP:
        {
            uint16_t count = p_ble_evt->evt.gattc_evt.params.char_disc_rsp.count;
            uBit.serial.printf("Found %d characteristics\r\n", count);

            for (int i = 0; i < count; i++)
            {
                const ble_gattc_char_t* p_char = &p_ble_evt->evt.gattc_evt.params.char_disc_rsp.chars[i];

                // Check if this is our write characteristic (UUID 0xFA02)
                if (p_char->uuid.type == uuid_type && p_char->uuid.uuid == 0xFA02)
                {
                    write_char_handle = p_char->handle_value;
                    uBit.serial.printf("FOUND WRITE CHAR! Handle: 0x%04X\r\n", write_char_handle);
                    uBit.display.print('W');
                }
            }

            discovery_in_progress = false;
            break;
        }

        case BLE_GATTC_EVT_EXCHANGE_MTU_RSP:
        {
            uint16_t mtu = p_ble_evt->evt.gattc_evt.params.exchange_mtu_rsp.server_rx_mtu;
            uBit.serial.printf("Negotiated MTU: %d bytes\r\n", mtu);

            // The actual data payload is MTU - 3 bytes (for ATT header overhead)
            uint16_t max_data_length = mtu - 3;
            uBit.serial.printf("Max data per write: %d bytes\r\n", max_data_length);

            // Update your CHUNK_SIZE accordingly
            CHUNK_SIZE = max_data_length;
            break;
        }

        case BLE_GATTC_EVT_WRITE_RSP:
        {
            write_in_progress = false;
            uBit.serial.printf("Write confirmed!\r\n");
            break;
        }

        default:
            break;
    }
}

// Register the observer with priority 3
NRF_SDH_BLE_OBSERVER(m_gattc_observer, 3, gattc_event_handler, NULL);

int main()
{
    memset(png_buffer, 0, sizeof(png_buffer));

    memset(display_buffer.pixel_data, 255, sizeof(display_buffer.pixel_data)); // Fill input buffer with white pixels

    // set rows alternately red,green,blue
    for (int i = 0 ; i<32; i++)
    {
        for (int j = 0; j < 32; j++)
        {
            if (i % 3 == 0) // Red row
            {
                display_buffer.pixel_data[(i * 32 + j) * 3 + 0] = 255; // R
                display_buffer.pixel_data[(i * 32 + j) * 3 + 1] = 0;   // G
                display_buffer.pixel_data[(i * 32 + j) * 3 + 2] = 0;   // B
            }
            else if (i % 3 == 1) // Green row
            {
                display_buffer.pixel_data[(i * 32 + j) * 3 + 0] = 0;   // R
                display_buffer.pixel_data[(i * 32 + j) * 3 + 1] = 255; // G
                display_buffer.pixel_data[(i * 32 + j) * 3 + 2] = 0;   // B
            }
            else // Blue row
            {
                display_buffer.pixel_data[(i * 32 + j) * 3 + 0] = 0;   // R
                display_buffer.pixel_data[(i * 32 + j) * 3 + 1] = 0;   // G
                display_buffer.pixel_data[(i * 32 + j) * 3 + 2] = 255; // B
            }
        }
    }

    uBit.init();
    uBit.serial.setBaudrate(115200);
    uBit.serial.printf("BLE Scanner Starting...\r\n");

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DEVICE_FOUND, [](MicroBitEvent) {
        uBit.serial.printf("Device Found!\r\n");
        uBit.display.print('F');
        uBit.bleManager.connectToDevice();
    });

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_CONNECTED, [](MicroBitEvent) {
        uBit.serial.printf("Device Connected!\r\n");
        uBit.display.print('C');

        // Start discovering the write characteristic
        fiber_sleep(100); // Small delay to let connection stabilize

        discover_write_characteristic();

        while (discovery_in_progress)
        {
            fiber_sleep(1);
        }

         // Request MTU exchange
        uint16_t conn_handle = uBit.bleManager.getCentralConnectionHandle();
        if (conn_handle != BLE_CONN_HANDLE_INVALID)
        {
            uint32_t err = sd_ble_gattc_exchange_mtu_request(conn_handle, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
            if (err == NRF_SUCCESS)
            {
                uBit.serial.printf("MTU exchange requested\r\n");
            } else {
                uBit.serial.printf("MTU exchange request failed: 0x%x\r\n", err);
            }
        }

        connected = true;
    });

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DISCONNECTED, [](MicroBitEvent) {
        connected = false;
        write_char_handle = BLE_GATT_HANDLE_INVALID;
        uBit.serial.printf("Device lost!\r\n");
        uBit.display.print('L');
    });

    uBit.bleManager.listenForDevice(ManagedString("IDM-68B955"));


    while (true)
    {
        if (connected && write_char_handle != BLE_GATT_HANDLE_INVALID)
        {
            uBit.serial.printf("Writing image...\r\n");

            ManagedString s = ManagedString("Hello, World!");

            write_text(s);
            uBit.sleep(1520*s.length()); // 95 * width of each character
            // set_mode();
            // write_image(display_buffer.pixel_data, sizeof(display_buffer.pixel_data));
        }
        uBit.sleep(5000);
    }



    // while (true)
    // {
    //     if (connected && write_char_handle != BLE_GATT_HANDLE_INVALID)
    //     {
    //         uBit.serial.printf("Writing image...\r\n");
    //         set_mode();
    //         write_image(display_buffer.pixel_data, sizeof(display_buffer.pixel_data));
    //     }
    //     uBit.sleep(5000);
    // }

    uBit.serial.printf("Entering main loop...\r\n");

    // while (true)
    // {
    //     if (connected && write_char_handle != BLE_GATT_HANDLE_INVALID)
    //     {
    //         for (int i = 0; i < 32; i++)
    //         {
    //             for (int j = 0; j < 32; j++)
    //             {
    //                 write_pixel(i, j, 255, 0, 0);
    //             }
    //         }
    //     }
    //     uBit.sleep(500);
    // }


    // uint32_t score = 0;
    // uint32_t score1 = 999;
    // int counter = 0;
    // while (true)
    // {
    //     if (connected && write_char_handle != BLE_GATT_HANDLE_INVALID)
    //     {
    //         // Write some test data every second
    //         write_score(score, score1);
    //         score++;
    //         score1--;

    //         if (score > 9999) score = 0;
    //         if (score1 > 9999) score1 = 9999;
    //     }

    //     uBit.sleep(1000);
    // }
    // Keep running
    release_fiber();
}

