#pragma once

#include <napi.h>

namespace spi {

// Initialises the SPI module and returns an object with:
//   open, openSync, MODE0, MODE1, MODE2, MODE3
Napi::Object Init(Napi::Env env);

} // namespace spi
