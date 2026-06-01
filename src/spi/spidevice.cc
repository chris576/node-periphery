#include "spidevice.h"
#include "open.h"
#include "close.h"
#include "transfer.h"
#include "getoptions.h"
#include "setoptions.h"

namespace spi {

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------

Napi::FunctionReference SpiDevice::constructor;
uv_mutex_t              SpiDevice::optionAccessLock;

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

SpiDevice::SpiDevice(const Napi::CallbackInfo& info)
    : Napi::ObjectWrap<SpiDevice>(info), fd_(-1) {}

// ---------------------------------------------------------------------------
// Class registration
// ---------------------------------------------------------------------------

Napi::Function SpiDevice::Init(Napi::Env env) {
    Napi::Function func = DefineClass(env, "SpiDevice", {
        InstanceMethod("close",          &SpiDevice::Close),
        InstanceMethod("closeSync",      &SpiDevice::CloseSync),
        InstanceMethod("transfer",       &SpiDevice::Transfer),
        InstanceMethod("transferSync",   &SpiDevice::TransferSync),
        InstanceMethod("getOptions",     &SpiDevice::GetOptions),
        InstanceMethod("getOptionsSync", &SpiDevice::GetOptionsSync),
        InstanceMethod("setOptions",     &SpiDevice::SetOptions),
        InstanceMethod("setOptionsSync", &SpiDevice::SetOptionsSync),
    });

    constructor = Napi::Persistent(func);
    constructor.SuppressDestruct();

    uv_mutex_init(&optionAccessLock);

    return func;
}

// ---------------------------------------------------------------------------
// Static factory methods (create a JS instance from C++)
// ---------------------------------------------------------------------------

Napi::Object SpiDevice::Open(const Napi::CallbackInfo& info) {
    Napi::Object instance = constructor.New({});
    SpiDevice* device = Unwrap(instance);
    spi::Open(device, info);
    return instance;
}

Napi::Object SpiDevice::OpenSync(const Napi::CallbackInfo& info) {
    Napi::Object instance = constructor.New({});
    SpiDevice* device = Unwrap(instance);
    spi::OpenSync(device, info);
    return instance;
}

// ---------------------------------------------------------------------------
// Mutex helpers
// ---------------------------------------------------------------------------

void SpiDevice::LockOptionAccess() {
    uv_mutex_lock(&optionAccessLock);
}

void SpiDevice::UnlockOptionAccess() {
    uv_mutex_unlock(&optionAccessLock);
}

// ---------------------------------------------------------------------------
// Instance method delegates
// ---------------------------------------------------------------------------

Napi::Value SpiDevice::Close(const Napi::CallbackInfo& info) {
    return spi::Close(this, info);
}

Napi::Value SpiDevice::CloseSync(const Napi::CallbackInfo& info) {
    return spi::CloseSync(this, info);
}

Napi::Value SpiDevice::Transfer(const Napi::CallbackInfo& info) {
    return spi::Transfer(this, info);
}

Napi::Value SpiDevice::TransferSync(const Napi::CallbackInfo& info) {
    return spi::TransferSync(this, info);
}

Napi::Value SpiDevice::GetOptions(const Napi::CallbackInfo& info) {
    return spi::GetOptions(this, info);
}

Napi::Value SpiDevice::GetOptionsSync(const Napi::CallbackInfo& info) {
    return spi::GetOptionsSync(this, info);
}

Napi::Value SpiDevice::SetOptions(const Napi::CallbackInfo& info) {
    return spi::SetOptions(this, info);
}

Napi::Value SpiDevice::SetOptionsSync(const Napi::CallbackInfo& info) {
    return spi::SetOptionsSync(this, info);
}

} // namespace spi
