#pragma once

#include "io/event_loop.h"
#include <span>
#include <string>

namespace widedb::io {

class AsyncFile {
public:
    AsyncFile(EventLoop* loop, const std::string& path, int flags, mode_t mode = 0644, bool use_direct_io = false);
    ~AsyncFile();

    AsyncFile(const AsyncFile&) = delete;
    AsyncFile& operator=(const AsyncFile&) = delete;

    auto is_open() const -> bool { return fd_ >= 0; }
    int fd() const { return fd_; }

    void read(uint64_t offset, std::span<uint8_t> buffer, IoCallback cb);
    void write(uint64_t offset, std::span<const uint8_t> data, IoCallback cb);
    void fsync(IoCallback cb);

private:
    EventLoop* loop_;
    int fd_{-1};
};

} // namespace widedb::io
