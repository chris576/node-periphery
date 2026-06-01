#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "transfer.h"
#include "spidevice.h"
#include "util.h"

namespace spi {

// ---------------------------------------------------------------------------
// Low-level ioctl helper
// ---------------------------------------------------------------------------

static int DoTransfer(int fd, spi_ioc_transfer* transfers, uint32_t count) {
    int totalLen = 0;
    for (uint32_t i = 0; i < count; ++i)
        totalLen += static_cast<int>(transfers[i].len);

    int ret = ioctl(fd, SPI_IOC_MESSAGE(count), transfers);
    if (ret != -1 && ret != totalLen) {
        errno = EINVAL;
        ret   = -1;
    }
    return ret;
}

// ---------------------------------------------------------------------------
// JS message array → spi_ioc_transfer* conversion
// Returns 0 on success, -1 if a JS exception was thrown.
// Caller owns the allocated array and must free() it.
// ---------------------------------------------------------------------------

static int32_t ToSpiTransfers(
    Napi::Array         message,
    spi_ioc_transfer*   transfers
) {
    Napi::Env env = message.Env();

    for (uint32_t i = 0; i < message.Length(); ++i) {
        Napi::Value transferVal = message.Get(i);
        if (!transferVal.IsObject()) {
            MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
            return -1;
        }
        Napi::Object msg = transferVal.As<Napi::Object>();

        // byteLength (required)
        Napi::Value byteLength = msg.Get("byteLength");
        if (byteLength.IsUndefined() || !byteLength.IsNumber()) {
            MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
            return -1;
        }
        uint32_t length = byteLength.As<Napi::Number>().Uint32Value();
        transfers[i].len = length;

        // sendBuffer (optional)
        Napi::Value sendBuffer = msg.Get("sendBuffer");
        if (sendBuffer.IsNull() || sendBuffer.IsUndefined()) {
            // tx_buf already zero-initialised
        } else if (sendBuffer.IsBuffer()) {
            auto buf = sendBuffer.As<Napi::Buffer<uint8_t>>();
            if (buf.ByteLength() < static_cast<size_t>(length)) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].tx_buf = static_cast<__u64>(
                reinterpret_cast<uintptr_t>(buf.Data()));
        } else {
            MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
            return -1;
        }

        // receiveBuffer (optional)
        Napi::Value receiveBuffer = msg.Get("receiveBuffer");
        if (receiveBuffer.IsNull() || receiveBuffer.IsUndefined()) {
            // rx_buf already zero-initialised
        } else if (receiveBuffer.IsBuffer()) {
            auto buf = receiveBuffer.As<Napi::Buffer<uint8_t>>();
            if (buf.ByteLength() < static_cast<size_t>(length)) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].rx_buf = static_cast<__u64>(
                reinterpret_cast<uintptr_t>(buf.Data()));
        } else {
            MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
            return -1;
        }

        // At least one of sendBuffer / receiveBuffer must be set
        if (transfers[i].tx_buf == 0 && transfers[i].rx_buf == 0) {
            MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
            return -1;
        }

        // speedHz (optional)
        Napi::Value speedHz = msg.Get("speedHz");
        if (!speedHz.IsUndefined()) {
            if (!speedHz.IsNumber()) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].speed_hz = speedHz.As<Napi::Number>().Uint32Value();
        }

        // microSecondDelay (optional)
        Napi::Value microSecondDelay = msg.Get("microSecondDelay");
        if (!microSecondDelay.IsUndefined()) {
            if (!microSecondDelay.IsNumber()) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            uint32_t delay = microSecondDelay.As<Napi::Number>().Uint32Value();
            if (delay >= 65536) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].delay_usecs = static_cast<__u16>(delay);
        }

        // bitsPerWord (optional)
        Napi::Value bitsPerWord = msg.Get("bitsPerWord");
        if (!bitsPerWord.IsUndefined()) {
            if (!bitsPerWord.IsNumber()) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            uint32_t bits = bitsPerWord.As<Napi::Number>().Uint32Value();
            if (bits >= 256) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].bits_per_word = static_cast<__u8>(bits);
        }

        // chipSelectChange (optional)
        Napi::Value chipSelectChange = msg.Get("chipSelectChange");
        if (!chipSelectChange.IsUndefined()) {
            if (!chipSelectChange.IsBoolean()) {
                MakeErrnoError(env, EINVAL, "toSpiTransfers").ThrowAsJavaScriptException();
                return -1;
            }
            transfers[i].cs_change = chipSelectChange.As<Napi::Boolean>().Value() ? 1 : 0;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Async worker
// ---------------------------------------------------------------------------

class TransferWorker : public SpiAsyncWorker {
public:
    TransferWorker(
        Napi::Function     callback,
        int                fd,
        Napi::Array        message,
        spi_ioc_transfer*  transfers,
        uint32_t           count
    )
        : SpiAsyncWorker(callback),
          fd_(fd),
          transfers_(transfers),
          count_(count),
          message_(Napi::Persistent(message)) {}

    ~TransferWorker() = default;

    void Execute() override {
        int ret = DoTransfer(fd_, transfers_, count_);
        free(transfers_);
        transfers_ = nullptr;
        if (ret == -1) {
            SetErrorNo(errno);
            SetErrorSyscall("transfer");
            SetError("transfer failed");
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        Callback().Call({Env().Null(), message_.Value()});
    }

private:
    int                         fd_;
    spi_ioc_transfer*           transfers_;
    uint32_t                    count_;
    Napi::Reference<Napi::Array> message_;
};

// ---------------------------------------------------------------------------
// NAPI-level wrappers (called from SpiDevice methods)
// ---------------------------------------------------------------------------

Napi::Value Transfer(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "transfer").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 2 || !info[0].IsArray() || !info[1].IsFunction()) {
        MakeErrnoError(env, EINVAL, "transfer").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Array    message  = info[0].As<Napi::Array>();
    Napi::Function callback = info[1].As<Napi::Function>();

    uint32_t count = message.Length();
    auto* transfers = static_cast<spi_ioc_transfer*>(
        malloc(count * sizeof(spi_ioc_transfer)));
    memset(transfers, 0, count * sizeof(spi_ioc_transfer));

    if (ToSpiTransfers(message, transfers) == -1) {
        free(transfers);
        return env.Undefined();
    }

    (new TransferWorker(callback, fd, message, transfers, count))->Queue();
    return info.This();
}

Napi::Value TransferSync(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    int fd = device->Fd();

    if (fd == -1) {
        MakeErrnoError(env, EPERM, "transferSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    if (info.Length() < 1 || !info[0].IsArray()) {
        MakeErrnoError(env, EINVAL, "transferSync").ThrowAsJavaScriptException();
        return env.Undefined();
    }

    Napi::Array message = info[0].As<Napi::Array>();
    uint32_t    count   = message.Length();

    auto* transfers = static_cast<spi_ioc_transfer*>(
        malloc(count * sizeof(spi_ioc_transfer)));
    memset(transfers, 0, count * sizeof(spi_ioc_transfer));

    if (ToSpiTransfers(message, transfers) == 0) {
        if (DoTransfer(fd, transfers, count) == -1) {
            MakeErrnoError(env, errno, "transferSync").ThrowAsJavaScriptException();
        }
    }

    free(transfers);
    return info.This();
}

} // namespace spi
