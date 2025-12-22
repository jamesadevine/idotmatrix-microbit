#pragma once

#include "MicroBit.h"

#include "nrf.h"

class DotMatrixClient
{
public:
    explicit DotMatrixClient(MicroBit &uBit);

    // Call when the central connection is established.
    void onConnected();

    // Call when the link is lost.
    void onDisconnected();

    bool isReady() const;

    // Convenience for test content.
    void fillTestPattern();

    void clearDisplay();

    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

    // Protocol helpers.
    int writeText(ManagedString &text);
    int setImageModeDiy();
    int writePixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
    int writeImage();
    int writeScore(uint32_t score0, uint32_t score1);

private:
    MicroBit &uBit_;

    bool uuidRegistered_;
    uint8_t uuidType_;

    bool discoveryInProgress_;
    volatile bool writeInProgress_;

    uint32_t chunkSize_;
    uint16_t writeCharHandle_;

    void discoverWriteCharacteristic();
    void requestMtuExchange();

    uint16_t connectionHandle() const;

    static uint32_t calculateCrc32(const uint8_t *data, size_t length);

    // Hooked by a global NRF observer in DotMatrix.cpp.
    void handleGattcEvent(ble_evt_t const *pBleEvt);

    friend void dotmatrix_gattc_event_handler(ble_evt_t const *p_ble_evt, void *p_context);
};
