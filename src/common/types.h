#pragma once

#include <cstdint>
#include <span>

namespace widedb::common {

using Slice = std::span<const uint8_t>;
using Timestamp = uint64_t;

struct ValuePointer {
    uint32_t segment_id;
    uint64_t offset;
    uint32_t len;
    
    bool operator==(const ValuePointer& other) const {
        return segment_id == other.segment_id && offset == other.offset && len == other.len;
    }
    bool operator!=(const ValuePointer& other) const {
        return !(*this == other);
    }
};

constexpr char kNamespaceSeparator = ':';

} // namespace widedb::common
