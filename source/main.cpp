#include "MicroBit.h"
#include "DotMatrix.h"

MicroBit uBit;
static DotMatrixClient dotMatrix(uBit);

static bool connected = false;

extern "C" void log_string(const char *str)
{
    uBit.serial.printf("%s\r\n", str);
}



int main()
{
    uBit.init();
    uBit.serial.setBaudrate(115200);
    uBit.serial.printf("BLE Scanner Starting...\r\n");

    dotMatrix.fillTestPattern();

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DEVICE_FOUND, [](MicroBitEvent) {
        uBit.serial.printf("Device Found!\r\n");
        uBit.display.print('F');
        uBit.bleManager.connectToDevice();
    });

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_CONNECTED, [](MicroBitEvent) {
        uBit.serial.printf("Device Connected!\r\n");
        uBit.display.print('C');

        fiber_sleep(100); // Small delay to let connection stabilize

        dotMatrix.onConnected();

        connected = true;
    });

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DISCONNECTED, [](MicroBitEvent) {
        connected = false;
        dotMatrix.onDisconnected();
        uBit.serial.printf("Device lost!\r\n");
        uBit.display.print('L');
    });

    uBit.messageBus.listen(MICROBIT_ID_BUTTON_A, MICROBIT_BUTTON_EVT_CLICK, [](MicroBitEvent) {
        dotMatrix.setImageModeDiy();
        dotMatrix.clearDisplay();

        uint8_t colors[4][3] = {
            {255, 0, 0},
            {0, 255, 0},
            {0, 0, 255},
            {255, 255, 0},
        };

        for (int i = 0; i < 4; i++)
        {
            uint32_t col_offset = 4 + (i * 4);
            uint32_t row_offset = 4;

            for (int j =0; j < 20; j++)
            {
                dotMatrix.setPixel(col_offset, row_offset+j, colors[i][0], colors[i][1], colors[i][2]);
            }
        }

        dotMatrix.writeImage();
    });

    uBit.bleManager.listenForDevice(ManagedString("IDM-68B955"));


    while (true)
    {
        if (connected && dotMatrix.isReady())
        {
            uBit.serial.printf("Writing image...\r\n");

            ManagedString s = ManagedString("Hello, World!");

            dotMatrix.writeText(s);
            uBit.sleep(1520*s.length()); // 95 * width of each character
            // dotMatrix.setImageModeDiy();
            // dotMatrix.writeImage();
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

