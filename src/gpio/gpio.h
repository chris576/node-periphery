#pragma once

#include <napi.h>
#include <linux/gpio.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Gpio : public Napi::ObjectWrap<Gpio> {
public:
    static Napi::Object Init(Napi::Env env, Napi::Object exports);

    Gpio(const Napi::CallbackInfo& info);
    ~Gpio();

private:
    // Sync I/O
    Napi::Value ReadSync(const Napi::CallbackInfo& info);
    void WriteSync(const Napi::CallbackInfo& info);

    // Async I/O → Promise
    Napi::Value Read(const Napi::CallbackInfo& info);
    Napi::Value Write(const Napi::CallbackInfo& info);

    // Interrupt watching
    Napi::Value Watch(const Napi::CallbackInfo& info);
    Napi::Value Unwatch(const Napi::CallbackInfo& info);
    void UnwatchAll(const Napi::CallbackInfo& info);

    // Property accessors
    Napi::Value GetDirection(const Napi::CallbackInfo& info);
    void SetDirection(const Napi::CallbackInfo& info);
    Napi::Value GetEdge(const Napi::CallbackInfo& info);
    void SetEdge(const Napi::CallbackInfo& info);
    Napi::Value GetActiveLow(const Napi::CallbackInfo& info);
    void SetActiveLow(const Napi::CallbackInfo& info);

    // Cleanup
    void Unexport(const Napi::CallbackInfo& info);

    // Static
    static Napi::Value Accessible(const Napi::CallbackInfo& info);

    // Internal helpers
    void OpenLine(Napi::Env env);
    void ReconfigureLine(Napi::Env env);
    void StartWatcher(Napi::Env env);
    void StopWatcher();
    gpio_v2_line_config BuildConfig() const;

    // GPIO state
    int chip_fd_{-1};
    int line_fd_{-1};
    uint32_t offset_{0};
    std::string direction_{"in"};
    std::string edge_{"none"};
    bool active_low_{false};
    uint32_t debounce_timeout_{0};

    // Watcher state
    std::atomic<bool> watching_{false};
    std::thread watch_thread_;
    int stop_pipe_[2]{-1, -1};

    struct WatcherEntry {
        Napi::FunctionReference ref;
        Napi::ThreadSafeFunction tsfn;
    };
    std::mutex watchers_mutex_;
    std::vector<WatcherEntry> watchers_;
};
