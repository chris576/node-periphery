#pragma once

#include <napi.h>

namespace spi {

class SpiDevice;

// Async: transfer(message, cb)  → returns this
Napi::Value Transfer(SpiDevice* device, const Napi::CallbackInfo& info);

// Sync: transferSync(message)  → returns this
Napi::Value TransferSync(SpiDevice* device, const Napi::CallbackInfo& info);

} // namespace spi
