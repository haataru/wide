#include "io/io_buffer.h"
#include <cstdlib>
#include <utility>

namespace widedb::io {

IoBuffer::IoBuffer(std::size_t capacity) {
    capacity_ = (capacity + 4095) & ~4095;
    void* ptr = nullptr;
    if (posix_memalign(&ptr, 4096, capacity_) != 0) {
        std::abort();
    }
    data_ = static_cast<uint8_t*>(ptr);
}

IoBuffer::~IoBuffer() {
    std::free(data_);
}

IoBuffer::IoBuffer(IoBuffer&& other) noexcept 
    : data_(std::exchange(other.data_, nullptr)),
      capacity_(std::exchange(other.capacity_, 0)) {}

IoBuffer& IoBuffer::operator=(IoBuffer&& other) noexcept {
    if (this != &other) {
        std::free(data_);
        data_ = std::exchange(other.data_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    return *this;
}

auto IoBuffer::data() -> uint8_t* { return data_; }
auto IoBuffer::data() const -> const uint8_t* { return data_; }
auto IoBuffer::capacity() const -> std::size_t { return capacity_; }

auto IoBuffer::span() -> std::span<uint8_t> {
    return {data_, capacity_};
}

auto IoBuffer::span() const -> std::span<const uint8_t> {
    return {data_, capacity_};
}

} // namespace widedb::io
