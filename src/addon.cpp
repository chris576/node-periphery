#include <napi.h>
#include "gpio/gpio.h"
#include "spi/spi.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    Gpio::Init(env, exports);
    exports.Set("spi", spi::Init(env));
    return exports;
}

NODE_API_MODULE(node_periphery, Init)
