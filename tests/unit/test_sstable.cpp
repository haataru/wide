#include <gtest/gtest.h>
#include "storage/sstable.h"
#include <filesystem>

using namespace widedb::storage;
using namespace widedb::io;

TEST(SstableTest, WriteAndRead) {
    EventLoop loop(1024);
    std::string filepath = "test_sstable.sst";
    
    {
        auto builder = std::make_shared<SstableBuilder>(&loop, filepath);
        for (int i = 0; i < 10000; i++) {
            std::string key = "key" + std::string(8 - std::to_string(i).length(), '0') + std::to_string(i);
            std::string val = "val" + std::string(8 - std::to_string(i).length(), '0') + std::to_string(i);
            Slice k(reinterpret_cast<const uint8_t*>(key.data()), key.size());
            Slice v(reinterpret_cast<const uint8_t*>(val.data()), val.size());
            auto res = builder->add(k, v, i);
            ASSERT_TRUE(res.has_value());
        }
        bool finish_done = false;
        builder->finish([&](auto res) {
            (void)res;
            finish_done = true;
            loop.stop();
        });
        if (!finish_done) loop.run();
        EXPECT_TRUE(finish_done);
    }

    {
        auto reader = std::make_shared<SstableReader>(&loop, filepath);
        bool init_done = false;
        reader->init([&](auto res) {
            ASSERT_TRUE(res.has_value());
            init_done = true;
            loop.stop();
        });
        if (!init_done) loop.run();
        EXPECT_TRUE(init_done);

        std::string key_exist = "key00005000";
        Slice k_e(reinterpret_cast<const uint8_t*>(key_exist.data()), key_exist.size());
        bool get_e_done = false;
        reader->get(k_e, [&](auto res_e) {
            ASSERT_TRUE(res_e.has_value());
            std::string expected_val = "val00005000";
            EXPECT_EQ(std::string(res_e->begin(), res_e->end()), expected_val);
            get_e_done = true;
            loop.stop();
        });
        if (!get_e_done) loop.run();
        EXPECT_TRUE(get_e_done);

        std::string key_not_exist = "key99999999";
        Slice k_n(reinterpret_cast<const uint8_t*>(key_not_exist.data()), key_not_exist.size());
        bool get_n_done = false;
        reader->get(k_n, [&](auto res_n) {
            (void)res_n;
            EXPECT_FALSE(res_n.has_value());
            get_n_done = true;
            loop.stop();
        });
        if (!get_n_done) loop.run();
        EXPECT_TRUE(get_n_done);
    }

    std::filesystem::remove(filepath);
}
