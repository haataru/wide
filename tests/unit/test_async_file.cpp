#include <gtest/gtest.h>
#include "io/async_file.h"
#include "io/io_buffer.h"
#include <fcntl.h>
#include <filesystem>
#include <cstring>

using namespace widedb::io;

TEST(AsyncFileTest, WriteFsyncRead) {
    EventLoop loop(1024);
    std::string path = "test_async_file.txt";
    AsyncFile file(&loop, path, O_CREAT | O_RDWR | O_TRUNC);

    IoBuffer write_buf(4096);
    std::memset(write_buf.data(), 'A', 4096);

    bool write_done = false;
    file.write(0, write_buf.span(), [&](int res, uint32_t) {
        EXPECT_EQ(res, 4096);
        write_done = true;
        loop.stop();
    });
    loop.run();

    bool fsync_done = false;
    file.fsync([&](int res, uint32_t) {
        EXPECT_EQ(res, 0);
        fsync_done = true;
        loop.stop();
    });
    loop.run();

    IoBuffer read_buf(4096);
    bool read_done = false;
    file.read(0, read_buf.span(), [&](int res, uint32_t) {
        EXPECT_EQ(res, 4096);
        EXPECT_EQ(read_buf.data()[0], 'A');
        EXPECT_EQ(read_buf.data()[4095], 'A');
        read_done = true;
        loop.stop();
    });
    loop.run();

    EXPECT_TRUE(write_done);
    EXPECT_TRUE(fsync_done);
    EXPECT_TRUE(read_done);

    std::filesystem::remove(path);
}
