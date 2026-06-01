#pragma once

#include <napi.h>

namespace spi {

class SpiDevice;

// Async: getOptions(cb)
Napi::Value GetOptions(SpiDevice* device, const Napi::CallbackInfo& info);

// Sync: getOptionsSync()
Napi::Value GetOptionsSync(SpiDevice* device, const Napi::CallbackInfo& info);

} // namespace spi
