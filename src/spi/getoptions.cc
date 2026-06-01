#include <cerrno>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "getoptions.h"
#include "spidevice.h"
#include "util.h"

namespace spi {

// ---------------------------------------------------------------------------
// Internal read-only options struct (distinct from the write SpiOptions)
// ---------------------------------------------------------------------------

struct ReadOptions {
    uint8_t  mode;
    uint8_t  bitsPerWord;
    uint32_t maxSpeedHz;
};

// ---------------------------------------------------------------------------
// Low-level ioctl helper
// ---------------------------------------------------------------------------

static int ReadDeviceOptions(int fd, ReadOptions& opts) {
    if (ioctl(fd, SPI_IOC_RD_MODE,          &opts.mode)        == -1 ||
        ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &opts.bitsPerWord) == -1 ||
        ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ,  &opts.maxSpeedHz)  == -1) {
        return -1;
    }
    return 0;
}

static void ToJsOptions(Napi::Env env, Napi::Object& jsOptions, ReadOptions& opts) {
    jsOptions.Set("mode",
        Napi::Number::New(env, static_cast<uint32_t>(opts.mode & (SPI_CPOL | SPI_CPHA))));
    jsOptions.Set("chipSelectHigh",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_CS_HIGH)));
    jsOptions.Set("lsbFirst",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_LSB_FIRST)));
    jsOptions.Set("threeWire",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_3WIRE)));
    jsOptions.Set("loopback",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_LOOP)));
    jsOptions.Set("noChipSelect",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_NO_CS)));
    jsOptions.Set("ready",
        Napi::Boolean::New(env, static_cast<bool>(opts.mode & SPI_READY)));
    jsOptions.Set("bitsPerWord",
        Napi::Number::New(env, opts.bitsPerWord));
    jsOptions.Set("maxSpeedHz",
        Napi::Number::New(env, opts.maxSpeedHz));
}

// ---------------------------------------------------------------------------
// Async worker
// ---------------------------------------------------------------------------

class GetOptionsWorker : public SpiAsyncWorker {
public:
    GetOptionsWorker(Napi::Function callback, int fd)
        : SpiAsyncWorker(callback), fd_(fd) {}

    ~GetOptionsWorker() = default;

    void Execute() override {
        if (ReadDeviceOptions(fd_, opts_) == -1) {
            SetErrorNo(errno);
            SetErrorSyscall("getOptions");
            SetError("getOptions failed");
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        Napi::Object jsOptions = Napi::Object::New(Env());
        ToJsOptions(Env(), jsOptions, opts_);
        Callback().Call({Env().Null(), jsOptions});
    }

private:
    int         fd_;
    ReadOptions opts_;
};

// ---------------------------------------------------------------------------
// NAPI-level wrappers (called from SpiDevice methods)
// ---------------------------------------------------------------------------

Napi::Value GetOptions(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "getOptions").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsFunction()) {
        MakeErrnoError(env, EINVAL, "getOptions").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    (new GetOptionsWorker(callback, fd))->Queue();
    return info.This();
}

Napi::Value GetOptionsSync(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "getOptionsSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    ReadOptions opts;
    if (ReadDeviceOptions(fd, opts) == -1) {
        MakeErrnoError(env, errno, "getOptionsSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object jsOptions = Napi::Object::New(env);
    ToJsOptions(env, jsOptions, opts);
    return jsOptions;
}

} // namespace spi
