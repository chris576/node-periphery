#pragma once

#include <napi.h>
#include <cerrno>
#include <cstring>
#include <string>

namespace spi {

// Creates a Node.js Error with errno metadata (errno, code, syscall).
inline Napi::Error MakeErrnoError(Napi::Env env, int errNo, const char* syscall) {
    std::string msg = std::string(syscall) + ": " + strerror(errNo);
    Napi::Error err = Napi::Error::New(env, msg);
    Napi::Object val = err.Value();
    val.Set("errno",   Napi::Number::New(env, errNo));
    val.Set("code",    Napi::String::New(env, strerror(errNo)));
    val.Set("syscall", Napi::String::New(env, syscall));
    return err;
}

// Base class for all SPI async workers.
// Subclasses set errorNo_/errorSyscall_ and call SetError() in Execute()
// to propagate errno-style errors through the callback.
class SpiAsyncWorker : public Napi::AsyncWorker {
public:
    explicit SpiAsyncWorker(Napi::Function callback)
        : Napi::AsyncWorker(callback), errorNo_(0), errorSyscall_(nullptr) {}

    ~SpiAsyncWorker() override {
        delete[] errorSyscall_;
    }

protected:
    void OnError(const Napi::Error&) override {
        Napi::HandleScope scope(Env());
        Callback().Call({
            MakeErrnoError(Env(), errorNo_, errorSyscall_ ? errorSyscall_ : "").Value()
        });
    }

    void SetErrorNo(int errNo) { errorNo_ = errNo; }
    int  ErrorNo()  const { return errorNo_; }

    void SetErrorSyscall(const char* syscall) {
        delete[] errorSyscall_;
        size_t len = strlen(syscall) + 1;
        errorSyscall_ = new char[len];
        memcpy(errorSyscall_, syscall, len);
    }
    const char* ErrorSyscall() const { return errorSyscall_; }

private:
    int   errorNo_;
    char* errorSyscall_;
};

} // namespace spi
