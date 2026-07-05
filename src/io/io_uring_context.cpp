#include "io/io_uring_context.h"
#include <cstdlib>
#include <cstring>

namespace widedb::io {

IoUringContext::IoUringContext(unsigned entries) {
    struct io_uring_params params = {};
    // Try SQPOLL first (requires CAP_SYS_NICE or root)
    params.flags = IORING_SETUP_SQPOLL;
    params.sq_thread_idle = 2000;
    
    int ret = io_uring_queue_init_params(entries, &ring_, &params);
    if (ret < 0) {
        // Fallback: plain io_uring without SQPOLL (works without privileges)
        std::memset(&params, 0, sizeof(params));
        ret = io_uring_queue_init_params(entries, &ring_, &params);
        if (ret < 0) {
            std::abort();
        }
    }
}

IoUringContext::~IoUringContext() {
    io_uring_queue_exit(&ring_);
}

auto IoUringContext::get_sqe() -> struct io_uring_sqe* {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        // SQ is full. Flush pending ones to the kernel to make room.
        flush();
        sqe = io_uring_get_sqe(&ring_);
    }
    if (sqe) {
        pending_sqes_++;
    }
    return sqe;
}

auto IoUringContext::submit() -> std::expected<int, std::error_code> {
    int ret = io_uring_submit(&ring_);
    pending_sqes_ = 0;
    if (ret < 0) {
        return std::unexpected(std::error_code(-ret, std::system_category()));
    }
    return ret;
}

void IoUringContext::flush() {
    if (pending_sqes_ > 0) {
        io_uring_submit(&ring_);
        pending_sqes_ = 0;
    }
}

auto IoUringContext::peek_cqe(struct io_uring_cqe** cqe) -> int {
    return io_uring_peek_cqe(&ring_, cqe);
}

void IoUringContext::advance_cqe(unsigned count) {
    io_uring_cq_advance(&ring_, count);
}

} // namespace widedb::io
