#include <cerrno>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "setoptions.h"
#include "spidevice.h"
#include "util.h"

namespace spi {

// ---------------------------------------------------------------------------
// Low-level ioctl wrapper
// ---------------------------------------------------------------------------

int SetOptions(int fd, SpiOptions& opts) {
    if (opts.setMode) {
        SpiDevice::LockOptionAccess();

        uint8_t currentMode;
        int ret = ioctl(fd, SPI_IOC_RD_MODE, &currentMode);
        if (ret != -1) {
            uint8_t nextMode = (currentMode & opts.modeMask) | opts.mode;
            ret = ioctl(fd, SPI_IOC_WR_MODE, &nextMode);
        }

        SpiDevice::UnlockOptionAccess();
        if (ret == -1) return -1;
    }

    if (opts.setBitsPerWord) {
        if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &opts.bitsPerWord) == -1)
            return -1;
    }

    if (opts.setMaxSpeedHz) {
        if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &opts.maxSpeedHz) == -1)
            return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// JS → SpiOptions conversion
// ---------------------------------------------------------------------------

int32_t ToSpiOptions(Napi::Object jsOptions, SpiOptions& opts) {
    Napi::Env env = jsOptions.Env();

    // mode
    Napi::Value mode = jsOptions.Get("mode");
    if (!mode.IsUndefined()) {
        if (!mode.IsNumber()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint32_t spiMode = mode.As<Napi::Number>().Uint32Value();
        if (spiMode > SPI_MODE_3) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        opts.mode     |= static_cast<uint8_t>(spiMode);
        opts.modeMask &= static_cast<uint8_t>(~SPI_MODE_3);
        opts.setMode   = true;
    }

    // chipSelectHigh
    Napi::Value chipSelectHigh = jsOptions.Get("chipSelectHigh");
    if (!chipSelectHigh.IsUndefined()) {
        if (!chipSelectHigh.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = chipSelectHigh.As<Napi::Boolean>().Value() ? SPI_CS_HIGH : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_CS_HIGH);
        opts.setMode   = true;
    }

    // lsbFirst
    Napi::Value lsbFirst = jsOptions.Get("lsbFirst");
    if (!lsbFirst.IsUndefined()) {
        if (!lsbFirst.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = lsbFirst.As<Napi::Boolean>().Value() ? SPI_LSB_FIRST : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_LSB_FIRST);
        opts.setMode   = true;
    }

    // threeWire
    Napi::Value threeWire = jsOptions.Get("threeWire");
    if (!threeWire.IsUndefined()) {
        if (!threeWire.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = threeWire.As<Napi::Boolean>().Value() ? SPI_3WIRE : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_3WIRE);
        opts.setMode   = true;
    }

    // loopback
    Napi::Value loopback = jsOptions.Get("loopback");
    if (!loopback.IsUndefined()) {
        if (!loopback.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = loopback.As<Napi::Boolean>().Value() ? SPI_LOOP : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_LOOP);
        opts.setMode   = true;
    }

    // noChipSelect
    Napi::Value noChipSelect = jsOptions.Get("noChipSelect");
    if (!noChipSelect.IsUndefined()) {
        if (!noChipSelect.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = noChipSelect.As<Napi::Boolean>().Value() ? SPI_NO_CS : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_NO_CS);
        opts.setMode   = true;
    }

    // ready
    Napi::Value ready = jsOptions.Get("ready");
    if (!ready.IsUndefined()) {
        if (!ready.IsBoolean()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint8_t v = ready.As<Napi::Boolean>().Value() ? SPI_READY : 0;
        opts.mode     |= v;
        opts.modeMask &= static_cast<uint8_t>(~SPI_READY);
        opts.setMode   = true;
    }

    // bitsPerWord
    Napi::Value bitsPerWord = jsOptions.Get("bitsPerWord");
    if (!bitsPerWord.IsUndefined()) {
        if (!bitsPerWord.IsNumber()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        uint32_t bits = bitsPerWord.As<Napi::Number>().Uint32Value();
        if (bits > 255) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        opts.bitsPerWord    = static_cast<uint8_t>(bits);
        opts.setBitsPerWord = true;
    }

    // maxSpeedHz
    Napi::Value maxSpeedHz = jsOptions.Get("maxSpeedHz");
    if (!maxSpeedHz.IsUndefined()) {
        if (!maxSpeedHz.IsNumber()) {
            MakeErrnoError(env, EINVAL, "toSpiOptions").ThrowAsJavaScriptException();
            return -1;
        }
        opts.maxSpeedHz    = maxSpeedHz.As<Napi::Number>().Uint32Value();
        opts.setMaxSpeedHz = true;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Async worker
// ---------------------------------------------------------------------------

class SetOptionsWorker : public SpiAsyncWorker {
public:
    SetOptionsWorker(Napi::Function callback, int fd, SpiOptions opts)
        : SpiAsyncWorker(callback), fd_(fd), opts_(opts) {}

    ~SetOptionsWorker() = default;

    void Execute() override {
        if (SetOptions(fd_, opts_) == -1) {
            SetErrorNo(errno);
            SetErrorSyscall("setOptions");
            SetError("setOptions failed");
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        Callback().Call({Env().Null()});
    }

private:
    int        fd_;
    SpiOptions opts_;
};

// ---------------------------------------------------------------------------
// NAPI-level wrappers (called from SpiDevice methods)
// ---------------------------------------------------------------------------

Napi::Value SetOptions(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "setOptions").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 2 || !info[0].IsObject() || !info[1].IsFunction()) {
        MakeErrnoError(env, EINVAL, "setOptions").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object jsOptions = info[0].As<Napi::Object>();
    Napi::Function callback = info[1].As<Napi::Function>();

    SpiOptions opts;
    if (ToSpiOptions(jsOptions, opts) == -1)
        return env.Undefined();

    (new SetOptionsWorker(callback, fd, opts))->Queue();
    return info.This();
}

Napi::Value SetOptionsSync(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "setOptionsSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsObject()) {
        MakeErrnoError(env, EINVAL, "setOptionsSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Object jsOptions = info[0].As<Napi::Object>();

    SpiOptions opts;
    if (ToSpiOptions(jsOptions, opts) == -1)
        return env.Undefined();

    if (SetOptions(fd, opts) == -1) {
        MakeErrnoError(env, errno, "setOptionsSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    return info.This();
}

} // namespace spi
