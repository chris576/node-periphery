#include <linux/spi/spidev.h>
#include "spi.h"
#include "spidevice.h"

namespace spi {

Napi::Object Init(Napi::Env env) {
    Napi::Object exports = Napi::Object::New(env);

    // Register the SpiDevice class (internal; instances are created via open/openSync)
    SpiDevice::Init(env);

    // open(busNumber, deviceNumber[, options], cb) → SpiDevice
    exports.Set("open", Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
        return SpiDevice::Open(info);
    }));

    // openSync(busNumber, deviceNumber[, options]) → SpiDevice
    exports.Set("openSync", Napi::Function::New(env, [](const Napi::CallbackInfo& info) -> Napi::Value {
        return SpiDevice::OpenSync(info);
    }));

    // SPI mode constants
    exports.Set("MODE0", Napi::Number::New(env, SPI_MODE_0));
    exports.Set("MODE1", Napi::Number::New(env, SPI_MODE_1));
    exports.Set("MODE2", Napi::Number::New(env, SPI_MODE_2));
    exports.Set("MODE3", Napi::Number::New(env, SPI_MODE_3));

    return exports;
}

} // namespace spi
