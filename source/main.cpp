#include "MicroBit.h"
#include "ble.h"
#include "ble_gap.h"
#include "ble_gattc.h"
#include "nrf_sdh_ble.h"
#include <string.h>
#include <stdio.h>

MicroBit uBit;

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

const uint8_t WRITE_TEXT_PACKET[] = {
    0,0, // number of chars
    0,1, // static values?
    0, // Text mode. Defaults to 0. 0 = replace text, 1 = marquee, 2 = reversed marquee, 3 = vertical rising marquee, 4 = vertical lowering marquee, 5 = blinking, 6 = fading, 7 = tetris, 8 = filling
    95, // Speed of text
    0, // Text colour mode. 0 = white, 1 = use given RGB color, 2,3,4,5 = rainbow modes
    0,0,0, // Text colour RGB (if colour mode = 1)
    0, // Text Background Mode. Defaults to 0. 0 = black, 1 = use given RGB color
    0,0,0, // Background colour RGB (if background mode = 1)
};

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

    uBit.bleManager.listenForDevice(ManagedString("IDM-882E03"));



    while (true)
    {
        if (connected && write_char_handle != BLE_GATT_HANDLE_INVALID)
        {
            uBit.serial.printf("Writing image...\r\n");
            set_mode();
            write_image(display_buffer.pixel_data, sizeof(display_buffer.pixel_data));
        }
        uBit.sleep(5000);
    }

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

