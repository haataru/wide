#include "io/async_file.h"
#include <fcntl.h>
#include <unistd.h>
#include <system_error>

namespace widedb::io {

AsyncFile::AsyncFile(EventLoop* loop, const std::string& path, int flags, mode_t mode, bool use_direct_io)
    : loop_(loop) {
    if (use_direct_io) {
        flags |= O_DIRECT;
    }
    fd_ = ::open(path.c_str(), flags, mode);
    if (fd_ < 0) {
        // Retry without O_DIRECT if it was enabled (filesystem may not support it)
        if (use_direct_io) {
            flags &= ~O_DIRECT;
            fd_ = ::open(path.c_str(), flags, mode);
        }
    }
}

AsyncFile::~AsyncFile() {
    if (fd_ >= 0) {
        loop_->context().flush();
        ::close(fd_);
    }
}

void AsyncFile::read(uint64_t offset, std::span<uint8_t> buffer, IoCallback cb) {
    struct io_uring_sqe* sqe = loop_->context().get_sqe();
    if (!sqe) {
        if (cb) cb(-EAGAIN, 0);
        return;
    }
    
    io_uring_prep_read(sqe, fd_, buffer.data(), buffer.size(), offset);
    uint64_t user_data = loop_->register_callback(std::move(cb));
    io_uring_sqe_set_data64(sqe, user_data);
}

void AsyncFile::write(uint64_t offset, std::span<const uint8_t> data, IoCallback cb) {
    struct io_uring_sqe* sqe = loop_->context().get_sqe();
    if (!sqe) {
        if (cb) cb(-EAGAIN, 0);
        return;
    }
    
    io_uring_prep_write(sqe, fd_, data.data(), data.size(), offset);
    uint64_t user_data = loop_->register_callback(std::move(cb));
    io_uring_sqe_set_data64(sqe, user_data);
}

void AsyncFile::fsync(IoCallback cb) {
    struct io_uring_sqe* sqe = loop_->context().get_sqe();
    if (!sqe) {
        if (cb) cb(-EAGAIN, 0);
        return;
    }
    
    io_uring_prep_fsync(sqe, fd_, 0);
    uint64_t user_data = loop_->register_callback(std::move(cb));
    io_uring_sqe_set_data64(sqe, user_data);
}

} // namespace widedb::io
