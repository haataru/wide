#include <gtest/gtest.h>
#include "storage/bloom_filter.h"
#include <string>

using namespace widedb::storage;

static auto to_slice(const std::string& s) -> Slice {
    return Slice(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

TEST(BloomFilterTest, NewFilterHasStorage) {
    BloomFilter filter(100, 10);
    EXPECT_FALSE(filter.empty());
    EXPECT_GT(filter.size_bytes(), 0u);
}

TEST(BloomFilterTest, AddAndMayContain) {
    BloomFilter filter(100, 10);
    std::string key = "hello";
    EXPECT_FALSE(filter.may_contain(to_slice(key)));
    filter.add(to_slice(key));
    EXPECT_TRUE(filter.may_contain(to_slice(key)));
}

TEST(BloomFilterTest, ClearResets) {
    BloomFilter filter(100, 10);
    std::string key = "hello";
    filter.add(to_slice(key));
    ASSERT_TRUE(filter.may_contain(to_slice(key)));
    size_t before = filter.size_bytes();
    filter.clear();
    EXPECT_FALSE(filter.may_contain(to_slice(key)));
    EXPECT_EQ(filter.size_bytes(), before);
}
