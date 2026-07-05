#include <gtest/gtest.h>
#include "io/io_buffer.h"

using namespace widedb::io;

TEST(IoBufferTest, AlignmentAndCapacity) {
    IoBuffer buf1(100);
    EXPECT_EQ(buf1.capacity(), 4096);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(buf1.data()) % 4096, 0);

    IoBuffer buf2(4097);
    EXPECT_EQ(buf2.capacity(), 8192);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(buf2.data()) % 4096, 0);
}
