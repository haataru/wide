#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace widedb::io {

class IoBuffer {
public:
    explicit IoBuffer(std::size_t capacity);
    ~IoBuffer();

    IoBuffer(const IoBuffer&) = delete;
    IoBuffer& operator=(const IoBuffer&) = delete;

    IoBuffer(IoBuffer&& other) noexcept;
    IoBuffer& operator=(IoBuffer&& other) noexcept;

    auto data() -> uint8_t*;
    auto data() const -> const uint8_t*;
    auto capacity() const -> std::size_t;

    auto span() -> std::span<uint8_t>;
    auto span() const -> std::span<const uint8_t>;

private:
    uint8_t* data_{nullptr};
    std::size_t capacity_{0};
};

} // namespace widedb::io
