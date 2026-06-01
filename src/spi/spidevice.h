#pragma once

#include <napi.h>
#include <uv.h>

namespace spi {

class SpiDevice : public Napi::ObjectWrap<SpiDevice> {
public:
    // Registers the class and returns the constructor function.
    // The returned function is exposed as spi.SpiDevice (internal use only).
    static Napi::Function Init(Napi::Env env);

    // Creates a new JS SpiDevice instance from C++ and queues an async open.
    static Napi::Object Open(const Napi::CallbackInfo& info);

    // Creates a new JS SpiDevice instance from C++ and opens synchronously.
    static Napi::Object OpenSync(const Napi::CallbackInfo& info);

    static void LockOptionAccess();
    static void UnlockOptionAccess();

    explicit SpiDevice(const Napi::CallbackInfo& info);
    ~SpiDevice() override = default;

    void SetFd(int fd) { fd_ = fd; }
    int  Fd()   const  { return fd_; }

private:
    Napi::Value Close(const Napi::CallbackInfo& info);
    Napi::Value CloseSync(const Napi::CallbackInfo& info);
    Napi::Value Transfer(const Napi::CallbackInfo& info);
    Napi::Value TransferSync(const Napi::CallbackInfo& info);
    Napi::Value GetOptions(const Napi::CallbackInfo& info);
    Napi::Value GetOptionsSync(const Napi::CallbackInfo& info);
    Napi::Value SetOptions(const Napi::CallbackInfo& info);
    Napi::Value SetOptionsSync(const Napi::CallbackInfo& info);

    static Napi::FunctionReference constructor;
    static uv_mutex_t              optionAccessLock;

    int fd_;
};

} // namespace spi
