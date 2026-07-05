#pragma once

#include "io/io_uring_context.h"
#include <array>
#include <cstdint>
#include <new>
#include <utility>

namespace widedb::io {

class IoCallback {
    alignas(16) char storage_[128];
    void (*invoke_)(char*, int, uint32_t){nullptr};
    void (*destroy_)(char*){nullptr};
    void (*move_)(char*, char*){nullptr};

public:
    IoCallback() = default;
    IoCallback(std::nullptr_t) : invoke_(nullptr), destroy_(nullptr), move_(nullptr) {}

    template <typename Lambda, typename = std::enable_if_t<!std::is_same_v<std::decay_t<Lambda>, IoCallback> && !std::is_same_v<std::decay_t<Lambda>, std::nullptr_t>>>
    IoCallback(Lambda&& lambda) {
        using DecayedLambda = std::decay_t<Lambda>;
        static_assert(sizeof(DecayedLambda) <= 128, "Lambda is too large for inline IoCallback!");
        
        new (storage_) DecayedLambda(std::forward<Lambda>(lambda));
        
        invoke_ = [](char* storage, int res, uint32_t flags) {
            auto* obj = reinterpret_cast<DecayedLambda*>(storage);
            (*obj)(res, flags);
        };
        
        destroy_ = [](char* storage) {
            auto* obj = reinterpret_cast<DecayedLambda*>(storage);
            obj->~DecayedLambda();
        };
        
        move_ = [](char* src, char* dst) {
            auto* src_obj = reinterpret_cast<DecayedLambda*>(src);
            new (dst) DecayedLambda(std::move(*src_obj));
        };
    }

    ~IoCallback() {
        if (destroy_) {
            destroy_(storage_);
        }
    }

    IoCallback(const IoCallback&) = delete;
    IoCallback& operator=(const IoCallback&) = delete;

    IoCallback(IoCallback&& other) noexcept : invoke_(other.invoke_), destroy_(other.destroy_), move_(other.move_) {
        if (move_) {
            move_(other.storage_, storage_);
        }
        other.invoke_ = nullptr;
        other.destroy_ = nullptr;
        other.move_ = nullptr;
    }

    IoCallback& operator=(IoCallback&& other) noexcept {
        if (this != &other) {
            if (destroy_) destroy_(storage_);
            
            invoke_ = other.invoke_;
            destroy_ = other.destroy_;
            move_ = other.move_;
            
            if (move_) {
                move_(other.storage_, storage_);
            }
            
            other.invoke_ = nullptr;
            other.destroy_ = nullptr;
            other.move_ = nullptr;
        }
        return *this;
    }

    void operator()(int res, uint32_t flags) {
        if (invoke_) {
            invoke_(storage_, res, flags);
        }
    }

    explicit operator bool() const {
        return invoke_ != nullptr;
    }
};

class EventLoop {
public:
    explicit EventLoop(unsigned entries = 1024, int core_id = -1);
    ~EventLoop() = default;

    auto context() -> IoUringContext& { return ctx_; }

    auto register_callback(IoCallback cb) -> uint64_t;
    void unregister_callback(uint64_t slot_idx);

    auto run_once() -> int;
    void run();
    void stop();

    void schedule_timer(uint64_t ms, IoCallback cb);

private:
    IoUringContext ctx_;
    
    struct Slot {
        IoCallback cb;
        uint32_t generation{0};
        bool active{false};
    };
    
    std::array<Slot, 1024> slots_;
    uint64_t next_slot_{0};
    bool running_{false};
    int core_id_{-1};
};

} // namespace widedb::io
