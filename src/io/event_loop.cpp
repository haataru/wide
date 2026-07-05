#include "io/event_loop.h"
#include <cstdlib>
#include <utility>
#include <memory>

namespace widedb::io {

EventLoop::EventLoop(unsigned entries, int core_id) 
    : ctx_(entries), core_id_(core_id) {
}

auto EventLoop::register_callback(IoCallback cb) -> uint64_t {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        uint64_t idx = (next_slot_ + i) % slots_.size();
        if (!slots_[idx].active) {
            slots_[idx].cb = std::move(cb);
            slots_[idx].active = true;
            next_slot_ = (idx + 1) % slots_.size();
            return (idx << 32) | slots_[idx].generation;
        }
    }
    std::abort();
}

void EventLoop::unregister_callback(uint64_t slot_idx) {
    if (slot_idx < slots_.size()) {
        slots_[slot_idx].active = false;
        slots_[slot_idx].cb = nullptr;
        slots_[slot_idx].generation++; // Invalidate pending CQEs for this slot
    }
}

auto EventLoop::run_once() -> int {
    int processed = 0;
    struct io_uring_cqe* cqe;
    
    while (ctx_.peek_cqe(&cqe) == 0 && cqe != nullptr) {
        uint64_t user_data = io_uring_cqe_get_data64(cqe);
        uint64_t slot_idx = user_data >> 32;
        uint32_t gen = user_data & 0xFFFFFFFF;
        
        int res = cqe->res;
        uint32_t flags = cqe->flags;
        
        ctx_.advance_cqe(1);
        
        if (slot_idx < slots_.size() && slots_[slot_idx].active && slots_[slot_idx].generation == gen) {
            if (cqe->flags & IORING_CQE_F_MORE) {
                slots_[slot_idx].cb(res, flags);
            } else {
                auto cb = std::move(slots_[slot_idx].cb);
                unregister_callback(slot_idx);
                cb(res, flags);
            }
        }
        processed++;
    }
    
    ctx_.flush();
    return processed;
}

void EventLoop::run() {
    running_ = true;
    while (running_) {
        int processed = run_once();
        if (processed == 0) {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ volatile("pause" ::: "memory");
#elif defined(__aarch64__)
            __asm__ volatile("yield" ::: "memory");
#endif
        }
    }
}

void EventLoop::stop() {
    running_ = false;
}

void EventLoop::schedule_timer(uint64_t ms, IoCallback cb) {
    struct io_uring_sqe* sqe = ctx_.get_sqe();
    if (!sqe) {
        if (cb) cb(-EAGAIN, 0);
        return;
    }

    struct TimerContext {
        __kernel_timespec ts;
        IoCallback cb;
    };
    auto ctx = std::make_shared<TimerContext>();
    ctx->ts.tv_sec = ms / 1000;
    ctx->ts.tv_nsec = (ms % 1000) * 1000000;
    ctx->cb = std::move(cb);

    io_uring_prep_timeout(sqe, &ctx->ts, 0, 0);
    auto wrapped_cb = [ctx](int res, uint32_t flags) mutable {
        if (ctx->cb) ctx->cb(res, flags);
    };

    uint64_t user_data = register_callback(std::move(wrapped_cb));
    io_uring_sqe_set_data64(sqe, user_data);
}

} // namespace widedb::io
