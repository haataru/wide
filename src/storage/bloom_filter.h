#pragma once

#include <vector>
#include <cstdint>
#include <span>
#include "common/types.h"

namespace widedb::storage {

using Slice = widedb::common::Slice;

class BloomFilter {
public:
    BloomFilter(size_t expected_keys, size_t bits_per_key);
    BloomFilter(Slice serialized_data, size_t bits_per_key);
    ~BloomFilter() = default;

    void add(Slice key);
    auto may_contain(Slice key) const -> bool;
    auto empty() const -> bool;
    auto size_bytes() const -> size_t;
    void clear();

    auto data() const -> Slice;

private:
    auto hash(Slice key) const -> uint32_t;

    std::vector<uint8_t> data_;
    size_t bits_per_key_;
    size_t k_;
};

} // namespace widedb::storage
