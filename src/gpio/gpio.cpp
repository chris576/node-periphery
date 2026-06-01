#include "gpio.h"


#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// AsyncWorker: read
// ---------------------------------------------------------------------------

class GpioReadWorker : public Napi::AsyncWorker {
public:
    GpioReadWorker(Napi::Promise::Deferred deferred, int line_fd)
        : Napi::AsyncWorker(deferred.Env()),
          deferred_(deferred),
          line_fd_(line_fd),
          value_(0) {}

    void Execute() override {
        struct gpio_v2_line_values values = {};
        values.mask = 1;
        if (ioctl(line_fd_, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
            SetError(std::string("GPIO read failed: ") + strerror(errno));
            return;
        }
        value_ = values.bits & 1;
    }

    void OnOK() override {
        deferred_.Resolve(Napi::Number::New(Env(), value_));
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(e.Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    int line_fd_;
    uint32_t value_;
};

// ---------------------------------------------------------------------------
// AsyncWorker: write
// ---------------------------------------------------------------------------

class GpioWriteWorker : public Napi::AsyncWorker {
public:
    GpioWriteWorker(Napi::Promise::Deferred deferred, int line_fd, uint32_t value)
        : Napi::AsyncWorker(deferred.Env()),
          deferred_(deferred),
          line_fd_(line_fd),
          value_(value) {}

    void Execute() override {
        struct gpio_v2_line_values values = {};
        values.mask = 1;
        values.bits = value_ ? 1 : 0;
        if (ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
            SetError(std::string("GPIO write failed: ") + strerror(errno));
        }
    }

    void OnOK() override {
        deferred_.Resolve(Env().Undefined());
    }

    void OnError(const Napi::Error& e) override {
        deferred_.Reject(e.Value());
    }

private:
    Napi::Promise::Deferred deferred_;
    int line_fd_;
    uint32_t value_;
};

// ---------------------------------------------------------------------------
// Gpio::Init
// ---------------------------------------------------------------------------

Napi::Object Gpio::Init(Napi::Env env, Napi::Object exports) {
    Napi::Function func = DefineClass(env, "Gpio", {
        InstanceMethod<&Gpio::ReadSync>("readSync"),
        InstanceMethod<&Gpio::WriteSync>("writeSync"),
        InstanceMethod<&Gpio::Read>("read"),
        InstanceMethod<&Gpio::Write>("write"),
        InstanceMethod<&Gpio::Watch>("watch"),
        InstanceMethod<&Gpio::Unwatch>("unwatch"),
        InstanceMethod<&Gpio::UnwatchAll>("unwatchAll"),
        InstanceMethod<&Gpio::GetDirection>("direction"),
        InstanceMethod<&Gpio::SetDirection>("setDirection"),
        InstanceMethod<&Gpio::GetEdge>("edge"),
        InstanceMethod<&Gpio::SetEdge>("setEdge"),
        InstanceMethod<&Gpio::GetActiveLow>("activeLow"),
        InstanceMethod<&Gpio::SetActiveLow>("setActiveLow"),
        InstanceMethod<&Gpio::Unexport>("unexport"),
        StaticAccessor<&Gpio::Accessible, nullptr>("accessible"),
        StaticValue("HIGH", Napi::Number::New(env, 1)),
        StaticValue("LOW",  Napi::Number::New(env, 0)),
    });

    exports.Set("Gpio", func);
    return exports;
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Gpio::Gpio(const Napi::CallbackInfo& info) : Napi::ObjectWrap<Gpio>(info) {
    Napi::Env env = info.Env();

    if (info.Length() < 2) {
        Napi::TypeError::New(env, "Expected at least 2 arguments: offset, direction")
            .ThrowAsJavaScriptException();
        return;
    }

    if (!info[0].IsNumber()) {
        Napi::TypeError::New(env, "offset must be a number")
            .ThrowAsJavaScriptException();
        return;
    }
    offset_ = info[0].As<Napi::Number>().Uint32Value();

    if (!info[1].IsString()) {
        Napi::TypeError::New(env, "direction must be a string")
            .ThrowAsJavaScriptException();
        return;
    }
    std::string dir = info[1].As<Napi::String>().Utf8Value();
    if (dir != "in" && dir != "out" && dir != "high" && dir != "low") {
        Napi::TypeError::New(env, "direction must be 'in', 'out', 'high', or 'low'")
            .ThrowAsJavaScriptException();
        return;
    }
    direction_ = (dir == "high" || dir == "low") ? "out" : dir;

    // Optional: edge string and/or options object
    uint32_t chip_number = 0;
    uint32_t next_arg = 2;

    if (info.Length() > next_arg && info[next_arg].IsString()) {
        std::string edge = info[next_arg].As<Napi::String>().Utf8Value();
        if (edge != "none" && edge != "rising" && edge != "falling" && edge != "both") {
            Napi::TypeError::New(env, "edge must be 'none', 'rising', 'falling', or 'both'")
                .ThrowAsJavaScriptException();
            return;
        }
        edge_ = edge;
        next_arg++;
    }

    if (info.Length() > next_arg && info[next_arg].IsObject()) {
        Napi::Object opts = info[next_arg].As<Napi::Object>();
        if (opts.Has("chip") && opts.Get("chip").IsNumber()) {
            chip_number = opts.Get("chip").As<Napi::Number>().Uint32Value();
        }
        if (opts.Has("activeLow") && opts.Get("activeLow").IsBoolean()) {
            active_low_ = opts.Get("activeLow").As<Napi::Boolean>().Value();
        }
        if (opts.Has("debounceTimeout") && opts.Get("debounceTimeout").IsNumber()) {
            debounce_timeout_ = opts.Get("debounceTimeout").As<Napi::Number>().Uint32Value();
        }
    }

    // Open the gpiochip character device
    std::string chip_path = "/dev/gpiochip" + std::to_string(chip_number);
    chip_fd_ = open(chip_path.c_str(), O_RDONLY | O_CLOEXEC);
    if (chip_fd_ < 0) {
        Napi::Error::New(env, "Failed to open " + chip_path + ": " + strerror(errno))
            .ThrowAsJavaScriptException();
        return;
    }

    OpenLine(env);
    if (env.IsExceptionPending()) return;

    // For 'high', set the output to 1 after the line is open
    if (dir == "high") {
        struct gpio_v2_line_values values = {};
        values.mask = 1;
        values.bits = 1;
        ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
    }
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

Gpio::~Gpio() {
    // Signal and join the watcher thread first, without holding the mutex
    if (watching_.exchange(false)) {
        if (stop_pipe_[1] >= 0) {
            uint8_t b = 1;
            while (write(stop_pipe_[1], &b, sizeof(b)) == -1 && errno == EINTR) {}
        }
        if (watch_thread_.joinable()) {
            watch_thread_.join();
        }
    }

    // Release all TSSFNs
    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        for (auto& entry : watchers_) {
            entry.tsfn.Release();
        }
        watchers_.clear();
    }

    if (stop_pipe_[0] >= 0) { close(stop_pipe_[0]); stop_pipe_[0] = -1; }
    if (stop_pipe_[1] >= 0) { close(stop_pipe_[1]); stop_pipe_[1] = -1; }
    if (line_fd_ >= 0) { close(line_fd_); line_fd_ = -1; }
    if (chip_fd_ >= 0) { close(chip_fd_); chip_fd_ = -1; }
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

gpio_v2_line_config Gpio::BuildConfig() const {
    struct gpio_v2_line_config config = {};

    if (direction_ == "in") {
        config.flags |= GPIO_V2_LINE_FLAG_INPUT;
        if (edge_ == "rising"  || edge_ == "both") config.flags |= GPIO_V2_LINE_FLAG_EDGE_RISING;
        if (edge_ == "falling" || edge_ == "both") config.flags |= GPIO_V2_LINE_FLAG_EDGE_FALLING;
    } else {
        config.flags |= GPIO_V2_LINE_FLAG_OUTPUT;
    }

    if (active_low_) {
        config.flags |= GPIO_V2_LINE_FLAG_ACTIVE_LOW;
    }

    // Kernel-native debounce (GPIO v2 only)
    if (debounce_timeout_ > 0) {
        config.num_attrs = 1;
        config.attrs[0].attr.id = GPIO_V2_LINE_ATTR_ID_DEBOUNCE;
        config.attrs[0].attr.debounce_period_us = debounce_timeout_ * 1000u; // ms → µs
        config.attrs[0].mask = 1ULL;
    }

    return config;
}

void Gpio::OpenLine(Napi::Env env) {
    struct gpio_v2_line_request req = {};
    req.offsets[0] = offset_;
    req.num_lines = 1;
    strncpy(req.consumer, "node-periphery", sizeof(req.consumer) - 1);
    req.config = BuildConfig();

    if (ioctl(chip_fd_, GPIO_V2_GET_LINE_IOCTL, &req) < 0) {
        Napi::Error::New(env, std::string("GPIO_V2_GET_LINE_IOCTL failed: ") + strerror(errno))
            .ThrowAsJavaScriptException();
        return;
    }
    line_fd_ = req.fd;
}

void Gpio::ReconfigureLine(Napi::Env env) {
    if (line_fd_ < 0) return;
    struct gpio_v2_line_config config = BuildConfig();
    if (ioctl(line_fd_, GPIO_V2_LINE_SET_CONFIG_IOCTL, &config) < 0) {
        Napi::Error::New(env, std::string("GPIO_V2_LINE_SET_CONFIG_IOCTL failed: ") + strerror(errno))
            .ThrowAsJavaScriptException();
    }
}

void Gpio::StartWatcher(Napi::Env env) {
    if (pipe2(stop_pipe_, O_CLOEXEC) < 0) {
        Napi::Error::New(env, std::string("pipe2 failed: ") + strerror(errno))
            .ThrowAsJavaScriptException();
        return;
    }

    watching_ = true;

    watch_thread_ = std::thread([this]() {
        int epfd = epoll_create1(EPOLL_CLOEXEC);
        if (epfd < 0) return;

        struct epoll_event ev_line = {};
        ev_line.events = EPOLLIN | EPOLLPRI;
        ev_line.data.fd = line_fd_;
        epoll_ctl(epfd, EPOLL_CTL_ADD, line_fd_, &ev_line);

        struct epoll_event ev_stop = {};
        ev_stop.events = EPOLLIN;
        ev_stop.data.fd = stop_pipe_[0];
        epoll_ctl(epfd, EPOLL_CTL_ADD, stop_pipe_[0], &ev_stop);

        struct epoll_event events[2];

        while (watching_) {
            int nfds = epoll_wait(epfd, events, 2, -1);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                break;
            }

            for (int i = 0; i < nfds; i++) {
                if (events[i].data.fd == stop_pipe_[0]) {
                    close(epfd);
                    return;
                }

                if (events[i].data.fd == line_fd_) {
                    struct gpio_v2_line_event event = {};
                    ssize_t n = read(line_fd_, &event, sizeof(event));
                    if (n != static_cast<ssize_t>(sizeof(event))) continue;

                    uint32_t value = (event.id == GPIO_V2_LINE_EVENT_RISING_EDGE) ? 1u : 0u;

                    std::lock_guard<std::mutex> lock(watchers_mutex_);
                    for (auto& entry : watchers_) {
                        uint32_t* val_copy = new uint32_t(value);
                        entry.tsfn.NonBlockingCall(val_copy,
                            [](Napi::Env env, Napi::Function callback, uint32_t* val) {
                                callback.Call({env.Null(), Napi::Number::New(env, *val)});
                                delete val;
                            }
                        );
                    }
                }
            }
        }

        close(epfd);
    });
}

void Gpio::StopWatcher() {
    // Caller must NOT hold watchers_mutex_
    if (!watching_.exchange(false)) return;

    if (stop_pipe_[1] >= 0) {
        uint8_t b = 1;
        while (write(stop_pipe_[1], &b, sizeof(b)) == -1 && errno == EINTR) {}
    }
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
    if (stop_pipe_[0] >= 0) { close(stop_pipe_[0]); stop_pipe_[0] = -1; }
    if (stop_pipe_[1] >= 0) { close(stop_pipe_[1]); stop_pipe_[1] = -1; }
}

// ---------------------------------------------------------------------------
// Sync I/O
// ---------------------------------------------------------------------------

Napi::Value Gpio::ReadSync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (line_fd_ < 0) {
        Napi::Error::New(env, "GPIO not open").ThrowAsJavaScriptException();
        return env.Null();
    }
    struct gpio_v2_line_values values = {};
    values.mask = 1;
    if (ioctl(line_fd_, GPIO_V2_LINE_GET_VALUES_IOCTL, &values) < 0) {
        Napi::Error::New(env, std::string("GPIO read failed: ") + strerror(errno))
            .ThrowAsJavaScriptException();
        return env.Null();
    }
    return Napi::Number::New(env, values.bits & 1);
}

void Gpio::WriteSync(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (line_fd_ < 0) {
        Napi::Error::New(env, "GPIO not open").ThrowAsJavaScriptException();
        return;
    }
    if (info.Length() < 1 || !info[0].IsNumber()) {
        Napi::TypeError::New(env, "value must be 0 or 1").ThrowAsJavaScriptException();
        return;
    }
    struct gpio_v2_line_values values = {};
    values.mask = 1;
    values.bits = info[0].As<Napi::Number>().Uint32Value() ? 1 : 0;
    if (ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values) < 0) {
        Napi::Error::New(env, std::string("GPIO write failed: ") + strerror(errno))
            .ThrowAsJavaScriptException();
    }
}

// ---------------------------------------------------------------------------
// Async I/O
// ---------------------------------------------------------------------------

Napi::Value Gpio::Read(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    if (line_fd_ < 0) {
        deferred.Reject(Napi::Error::New(env, "GPIO not open").Value());
        return deferred.Promise();
    }
    auto* worker = new GpioReadWorker(deferred, line_fd_);
    worker->Queue();
    return deferred.Promise();
}

Napi::Value Gpio::Write(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    auto deferred = Napi::Promise::Deferred::New(env);
    if (info.Length() < 1 || !info[0].IsNumber()) {
        deferred.Reject(Napi::TypeError::New(env, "value must be 0 or 1").Value());
        return deferred.Promise();
    }
    if (line_fd_ < 0) {
        deferred.Reject(Napi::Error::New(env, "GPIO not open").Value());
        return deferred.Promise();
    }
    uint32_t val = info[0].As<Napi::Number>().Uint32Value();
    auto* worker = new GpioWriteWorker(deferred, line_fd_, val);
    worker->Queue();
    return deferred.Promise();
}

// ---------------------------------------------------------------------------
// Interrupt watching
// ---------------------------------------------------------------------------

Napi::Value Gpio::Watch(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsFunction()) {
        Napi::TypeError::New(env, "callback must be a function")
            .ThrowAsJavaScriptException();
        return info.This();
    }
    if (line_fd_ < 0) {
        Napi::Error::New(env, "GPIO not open").ThrowAsJavaScriptException();
        return info.This();
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    auto tsfn = Napi::ThreadSafeFunction::New(env, callback, "GpioWatcher", 0, 1);

    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        WatcherEntry entry;
        entry.ref  = Napi::Persistent(callback);
        entry.tsfn = std::move(tsfn);
        watchers_.push_back(std::move(entry));
    }

    if (!watching_) {
        StartWatcher(env);
    }

    return info.This();
}

Napi::Value Gpio::Unwatch(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();

    if (info.Length() == 0 || info[0].IsUndefined()) {
        UnwatchAll(info);
        return info.This();
    }

    if (!info[0].IsFunction()) {
        Napi::TypeError::New(env, "callback must be a function")
            .ThrowAsJavaScriptException();
        return info.This();
    }

    Napi::Function callback = info[0].As<Napi::Function>();
    bool became_empty = false;

    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        auto it = std::find_if(watchers_.begin(), watchers_.end(),
            [&](const WatcherEntry& e) {
                return e.ref.Value().StrictEquals(callback);
            });
        if (it != watchers_.end()) {
            it->tsfn.Release();
            watchers_.erase(it);
        }
        became_empty = watchers_.empty();
    }

    if (became_empty) {
        StopWatcher();
    }

    return info.This();
}

void Gpio::UnwatchAll(const Napi::CallbackInfo& /*info*/) {
    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        for (auto& entry : watchers_) {
            entry.tsfn.Release();
        }
        watchers_.clear();
    }
    StopWatcher();
}

// ---------------------------------------------------------------------------
// Property accessors
// ---------------------------------------------------------------------------

Napi::Value Gpio::GetDirection(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), direction_);
}

void Gpio::SetDirection(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "direction must be a string").ThrowAsJavaScriptException();
        return;
    }
    std::string dir = info[0].As<Napi::String>().Utf8Value();
    if (dir != "in" && dir != "out" && dir != "high" && dir != "low") {
        Napi::TypeError::New(env, "direction must be 'in', 'out', 'high', or 'low'")
            .ThrowAsJavaScriptException();
        return;
    }
    direction_ = (dir == "high" || dir == "low") ? "out" : dir;
    ReconfigureLine(env);
    if (env.IsExceptionPending()) return;

    // Apply initial output value for high/low/out
    if (direction_ == "out") {
        struct gpio_v2_line_values values = {};
        values.mask = 1;
        values.bits = (dir == "high") ? 1u : 0u;
        ioctl(line_fd_, GPIO_V2_LINE_SET_VALUES_IOCTL, &values);
    }
}

Napi::Value Gpio::GetEdge(const Napi::CallbackInfo& info) {
    return Napi::String::New(info.Env(), edge_);
}

void Gpio::SetEdge(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsString()) {
        Napi::TypeError::New(env, "edge must be a string").ThrowAsJavaScriptException();
        return;
    }
    std::string edge = info[0].As<Napi::String>().Utf8Value();
    if (edge != "none" && edge != "rising" && edge != "falling" && edge != "both") {
        Napi::TypeError::New(env, "edge must be 'none', 'rising', 'falling', or 'both'")
            .ThrowAsJavaScriptException();
        return;
    }
    edge_ = edge;
    ReconfigureLine(env);
}

Napi::Value Gpio::GetActiveLow(const Napi::CallbackInfo& info) {
    return Napi::Boolean::New(info.Env(), active_low_);
}

void Gpio::SetActiveLow(const Napi::CallbackInfo& info) {
    Napi::Env env = info.Env();
    if (info.Length() < 1 || !info[0].IsBoolean()) {
        Napi::TypeError::New(env, "invert must be a boolean").ThrowAsJavaScriptException();
        return;
    }
    active_low_ = info[0].As<Napi::Boolean>().Value();
    ReconfigureLine(env);
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void Gpio::Unexport(const Napi::CallbackInfo& /*info*/) {
    // Stop watcher (must NOT hold mutex when calling StopWatcher)
    {
        std::lock_guard<std::mutex> lock(watchers_mutex_);
        for (auto& entry : watchers_) {
            entry.tsfn.Release();
        }
        watchers_.clear();
    }
    StopWatcher();

    if (line_fd_ >= 0) { close(line_fd_); line_fd_ = -1; }
    if (chip_fd_ >= 0) { close(chip_fd_); chip_fd_ = -1; }
}

// ---------------------------------------------------------------------------
// Static
// ---------------------------------------------------------------------------

Napi::Value Gpio::Accessible(const Napi::CallbackInfo& info) {
    int fd = open("/dev/gpiochip0", O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        return Napi::Boolean::New(info.Env(), false);
    }
    close(fd);
    return Napi::Boolean::New(info.Env(), true);
}
