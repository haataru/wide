#include "storage/bloom_filter.h"
#define XXH_INLINE_ALL
#include "xxhash.h"

namespace widedb::storage {

BloomFilter::BloomFilter(size_t expected_keys, size_t bits_per_key) 
    : bits_per_key_(bits_per_key) {
    k_ = static_cast<size_t>(bits_per_key_ * 0.69);
    if (k_ < 1) k_ = 1;
    if (k_ > 30) k_ = 30;

    size_t bits = expected_keys * bits_per_key_;
    if (bits < 64) bits = 64;
    size_t bytes = (bits + 7) / 8;
    data_.resize(bytes, 0);
}

BloomFilter::BloomFilter(Slice serialized_data, size_t bits_per_key) 
    : bits_per_key_(bits_per_key) {
    k_ = static_cast<size_t>(bits_per_key_ * 0.69);
    if (k_ < 1) k_ = 1;
    if (k_ > 30) k_ = 30;

    data_.assign(serialized_data.begin(), serialized_data.end());
}

auto BloomFilter::hash(Slice key) const -> uint32_t {
    return XXH32(key.data(), key.size(), 0xBC9F1D34);
}

void BloomFilter::add(Slice key) {
    if (data_.empty()) return;
    uint32_t h = hash(key);
    const uint32_t delta = (h >> 17) | (h << 15);
    for (size_t j = 0; j < k_; j++) {
        const uint32_t bitpos = h % (data_.size() * 8);
        data_[bitpos / 8] |= (1 << (bitpos % 8));
        h += delta;
    }
}

auto BloomFilter::may_contain(Slice key) const -> bool {
    if (data_.empty()) return false;
    uint32_t h = hash(key);
    const uint32_t delta = (h >> 17) | (h << 15);
    for (size_t j = 0; j < k_; j++) {
        const uint32_t bitpos = h % (data_.size() * 8);
        if ((data_[bitpos / 8] & (1 << (bitpos % 8))) == 0) {
            return false;
        }
        h += delta;
    }
    return true;
}

auto BloomFilter::data() const -> Slice {
    return Slice(data_.data(), data_.size());
}

} // namespace widedb::storage
