#include <cerrno>
#include <unistd.h>
#include "close.h"
#include "spidevice.h"
#include "util.h"

namespace spi {

// ---------------------------------------------------------------------------
// Low-level helper
// ---------------------------------------------------------------------------

static int CloseFd(SpiDevice* device) {
    int fd = device->Fd();
    device->SetFd(-1);
    return ::close(fd);
}

// ---------------------------------------------------------------------------
// Async worker
// ---------------------------------------------------------------------------

class CloseWorker : public SpiAsyncWorker {
public:
    CloseWorker(Napi::Function callback, SpiDevice* device)
        : SpiAsyncWorker(callback), device_(device) {}

    ~CloseWorker() = default;

    void Execute() override {
        if (CloseFd(device_) == -1) {
            SetErrorNo(errno);
            SetErrorSyscall("close");
            SetError("close failed");
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        Callback().Call({Env().Null()});
    }

private:
    SpiDevice* device_;
};

// ---------------------------------------------------------------------------
// NAPI-level wrappers (called from SpiDevice methods)
// ---------------------------------------------------------------------------

Napi::Value Close(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (device->Fd() == -1) {
        MakeErrnoError(env, EPERM, "close").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsFunction()) {
        MakeErrnoError(env, EINVAL, "close").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    (new CloseWorker(callback, device))->Queue();
    return env.Null();
}

Napi::Value CloseSync(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (device->Fd() == -1) {
        MakeErrnoError(env, EPERM, "closeSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (CloseFd(device) == -1) {
        MakeErrnoError(env, errno, "closeSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return env.Null();
}

} // namespace spi
