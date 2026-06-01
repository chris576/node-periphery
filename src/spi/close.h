#pragma once

#include <napi.h>

namespace spi {

class SpiDevice;

// Async: close(cb)  → returns null
Napi::Value Close(SpiDevice* device, const Napi::CallbackInfo& info);

// Sync: closeSync()  → returns null
Napi::Value CloseSync(SpiDevice* device, const Napi::CallbackInfo& info);

} // namespace spi
