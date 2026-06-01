#include <cerrno>
#include <fcntl.h>
#include "open.h"
#include "setoptions.h"
#include "spidevice.h"
#include "util.h"

namespace spi {

// ---------------------------------------------------------------------------
// Low-level helper: opens the spidev node and applies initial options.
// Returns fd on success, -1 on error (errno set).
// ---------------------------------------------------------------------------

static int OpenFd(uint32_t busNumber, uint32_t deviceNumber, SpiOptions& opts) {
    char file[64];
    snprintf(file, sizeof(file), "/dev/spidev%u.%u", busNumber, deviceNumber);

    int fd = ::open(file, O_RDWR);
    if (fd == -1) return -1;

    if (SetOptions(fd, opts) == -1) {
        int saved = errno;
        ::close(fd);
        errno = saved;
        return -1;
    }

    return fd;
}

static void SetDefaultOpenOptions(SpiOptions& opts) {
    opts.setMode   = true;
    opts.modeMask  = 0;
    opts.mode      = 0;
    opts.setBitsPerWord = true;
    opts.bitsPerWord    = 8;
}

// ---------------------------------------------------------------------------
// Async worker
// ---------------------------------------------------------------------------

class OpenWorker : public SpiAsyncWorker {
public:
    OpenWorker(
        Napi::Function callback,
        SpiDevice*     device,
        uint32_t       busNumber,
        uint32_t       deviceNumber,
        SpiOptions     opts
    )
        : SpiAsyncWorker(callback),
          device_(device),
          busNumber_(busNumber),
          deviceNumber_(deviceNumber),
          opts_(opts),
          fd_(-1) {}

    ~OpenWorker() = default;

    void Execute() override {
        fd_ = OpenFd(busNumber_, deviceNumber_, opts_);
        if (fd_ == -1) {
            SetErrorNo(errno);
            SetErrorSyscall("open");
            SetError("open failed");
        }
    }

    void OnOK() override {
        Napi::HandleScope scope(Env());
        device_->SetFd(fd_);
        Callback().Call({Env().Null()});
    }

private:
    SpiDevice* device_;
    uint32_t   busNumber_;
    uint32_t   deviceNumber_;
    SpiOptions opts_;
    int        fd_;
};

// ---------------------------------------------------------------------------
// NAPI-level wrappers (called from SpiDevice::Open / SpiDevice::OpenSync)
// ---------------------------------------------------------------------------

void Open(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // open(busNumber, deviceNumber[, options], cb)
    bool hasOptions = info.Length() > 3;
    if (info.Length() < 3 ||
        !info[0].IsNumber() ||
        !info[1].IsNumber() ||
        (info.Length() == 3 && !info[2].IsFunction()) ||
        (info.Length() >  3 && (!info[2].IsObject() || !info[3].IsFunction()))) {
        MakeErrnoError(env, EINVAL, "open").ThrowAsJavaScriptException();
        return;
    }

    uint32_t busNumber    = info[0].As<Napi::Number>().Uint32Value();
    uint32_t deviceNumber = info[1].As<Napi::Number>().Uint32Value();
    Napi::Function callback = (hasOptions ? info[3] : info[2]).As<Napi::Function>();

    SpiOptions opts;
    SetDefaultOpenOptions(opts);

    if (hasOptions) {
        Napi::Object jsOptions = info[2].As<Napi::Object>();
        if (ToSpiOptions(jsOptions, opts) == -1) return;
    }

    (new OpenWorker(callback, device, busNumber, deviceNumber, opts))->Queue();
}

void OpenSync(SpiDevice* device, const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    // openSync(busNumber, deviceNumber[, options])
    if (info.Length() < 2 ||
        !info[0].IsNumber() ||
        !info[1].IsNumber() ||
        (info.Length() > 2 && !info[2].IsObject())) {
        MakeErrnoError(env, EINVAL, "openSync").ThrowAsJavaScriptException();
        return;
    }

    uint32_t busNumber    = info[0].As<Napi::Number>().Uint32Value();
    uint32_t deviceNumber = info[1].As<Napi::Number>().Uint32Value();

    SpiOptions opts;
    SetDefaultOpenOptions(opts);

    if (info.Length() > 2) {
        Napi::Object jsOptions = info[2].As<Napi::Object>();
        if (ToSpiOptions(jsOptions, opts) == -1) return;
    }

    int fd = OpenFd(busNumber, deviceNumber, opts);
    if (fd == -1) {
        MakeErrnoError(env, errno, "openSync").ThrowAsJavaScriptException();
        return;
    }

    device->SetFd(fd);
}

} // namespace spi
