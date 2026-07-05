#pragma once

#include "common/types.h"
#include "io/async_file.h"
#include "io/io_buffer.h"
#include "storage/bloom_filter.h"
#include <string_view>
#include <span>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <expected>
#include <system_error>
#include <optional>

namespace widedb::storage {

using widedb::common::Slice;
using widedb::common::Timestamp;

enum class StorageError {
    kOk = 0,
    kNotFound,
    kCorruption,
    kChecksumMismatch,
    kIoError
};

const std::error_category& storage_error_category();
auto make_error_code(StorageError e) -> std::error_code;

using EmptyCallback = std::function<void(std::expected<void, std::error_code>)>;

using SstableGetCallback = std::function<void(std::optional<std::vector<uint8_t>>)>;

class SstableBuilder : public std::enable_shared_from_this<SstableBuilder> {
public:
    SstableBuilder(widedb::io::EventLoop* loop, const std::string& filepath);
    auto add(Slice key, Slice value, Timestamp ts) -> std::expected<void, std::error_code>;
    void finish(EmptyCallback cb);

private:
    widedb::io::EventLoop* loop_;
    std::string filepath_;
    std::unique_ptr<widedb::io::AsyncFile> file_;
    BloomFilter filter_;
    
    struct Entry {
        std::string key;
        std::vector<uint8_t> value;
        Timestamp ts;
    };
    std::vector<Entry> current_block_entries_;
    size_t current_block_size_ = 0;
    
    struct IndexEntry {
        std::string first_key;
        uint64_t offset;
    };
    std::vector<IndexEntry> index_;
    uint64_t current_offset_ = 0;
    
    // Asynchronous writing state
    std::vector<std::shared_ptr<widedb::io::IoBuffer>> pending_writes_;
    int in_flight_writes_ = 0;
    EmptyCallback finish_cb_;
    bool finish_called_ = false;
    
    void flush_data_block();
    void try_finish();
};

class SstableReader : public std::enable_shared_from_this<SstableReader> {
public:
    SstableReader(widedb::io::EventLoop* loop, const std::string& filepath);
    void init(EmptyCallback cb);
    void get(Slice key, SstableGetCallback cb);

private:
    widedb::io::EventLoop* loop_;
    std::string filepath_;
    std::shared_ptr<widedb::io::AsyncFile> file_;
    BloomFilter filter_;
    
    struct IndexEntry {
        std::string first_key;
        uint64_t offset;
    };
    std::vector<IndexEntry> index_;
};

} // namespace widedb::storage
