#pragma once

#include <napi.h>

namespace spi {

class SpiDevice;

// Async: open(busNumber, deviceNumber[, options], cb)
// Queues an async worker; does not set the fd on device directly.
void Open(SpiDevice* device, const Napi::CallbackInfo& info);

// Sync: openSync(busNumber, deviceNumber[, options])
// Opens the device and sets its fd synchronously.
void OpenSync(SpiDevice* device, const Napi::CallbackInfo& info);

} // namespace spi
