#pragma once

#include <liburing.h>
#include <system_error>
#include <expected>

namespace widedb::io {

class IoUringContext {
public:
    explicit IoUringContext(unsigned entries);
    ~IoUringContext();

    IoUringContext(const IoUringContext&) = delete;
    IoUringContext& operator=(const IoUringContext&) = delete;

    auto get_sqe() -> struct io_uring_sqe*;
    auto submit() -> std::expected<int, std::error_code>;
    auto peek_cqe(struct io_uring_cqe** cqe) -> int;
    void advance_cqe(unsigned count);
    
    void flush();
    
    auto ring() -> struct io_uring* { return &ring_; }

private:
    struct io_uring ring_;
    int pending_sqes_{0};
};

} // namespace widedb::io
