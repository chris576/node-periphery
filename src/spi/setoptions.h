#pragma once

#include <napi.h>
#include <cstdint>

namespace spi {

class SpiDevice;

// Options used when writing configuration to a SPI device.
struct SpiOptions {
    SpiOptions()
        : setMode(false), setBitsPerWord(false), setMaxSpeedHz(false),
          modeMask(0xff), mode(0), bitsPerWord(0), maxSpeedHz(0) {}

    bool     setMode;
    bool     setBitsPerWord;
    bool     setMaxSpeedHz;
    uint8_t  modeMask;
    uint8_t  mode;
    uint8_t  bitsPerWord;
    uint32_t maxSpeedHz;
};

// Applies spiOptions to an open file descriptor via ioctl.
// Returns 0 on success, -1 on error (errno set).
int SetOptions(int fd, SpiOptions& options);

// Converts a JS options object into SpiOptions.
// Returns 0 on success, -1 if a JS exception was thrown.
int32_t ToSpiOptions(Napi::Object jsOptions, SpiOptions& options);

// Async: setOptions(options, cb)
Napi::Value SetOptions(SpiDevice* device, const Napi::CallbackInfo& info);

// Sync: setOptionsSync(options)
Napi::Value SetOptionsSync(SpiDevice* device, const Napi::CallbackInfo& info);

} // namespace spi
