#include "storage/sstable.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <iostream>

namespace widedb::storage {

class StorageErrorCategory : public std::error_category {
public:
    const char* name() const noexcept override { return "storage"; }
    std::string message(int ev) const override {
        switch (static_cast<StorageError>(ev)) {
            case StorageError::kNotFound: return "Not found";
            case StorageError::kCorruption: return "Data corruption";
            case StorageError::kChecksumMismatch: return "Checksum mismatch";
            case StorageError::kIoError: return "IO error";
            case StorageError::kValueTooLarge: return "Value too large";
            default: return "Unknown storage error";
        }
    }
};

const std::error_category& storage_error_category() {
    static StorageErrorCategory instance;
    return instance;
}

auto make_error_code(StorageError e) -> std::error_code {
    return {static_cast<int>(e), storage_error_category()};
}

SstableBuilder::SstableBuilder(widedb::io::EventLoop* loop, const std::string& filepath)
    : loop_(loop), filepath_(filepath), filter_(1000, 10) {
    file_ = std::make_unique<widedb::io::AsyncFile>(loop_, filepath_, O_CREAT | O_RDWR | O_TRUNC);
}

void SstableBuilder::flush_data_block() {
    if (current_block_entries_.empty()) return;
    
    // Save first key to index
    index_.push_back({current_block_entries_[0].key, current_offset_});
    
    // Create 4KB block
    auto shared_buf = std::make_shared<widedb::io::IoBuffer>(4096);
    uint8_t* start_ptr = shared_buf->data();
    uint8_t* ptr = start_ptr;
    uint32_t count = current_block_entries_.size();
    std::memcpy(ptr, &count, 4); ptr += 4;
    
    std::vector<uint16_t> restart_points;
    for (const auto& entry : current_block_entries_) {
        restart_points.push_back(static_cast<uint16_t>(ptr - start_ptr));
        uint32_t ksz = entry.key.size();
        std::memcpy(ptr, &ksz, 4); ptr += 4;
        std::memcpy(ptr, entry.key.data(), ksz); ptr += ksz;
        uint32_t vsz = entry.value.size();
        std::memcpy(ptr, &vsz, 4); ptr += 4;
        std::memcpy(ptr, entry.value.data(), vsz); ptr += vsz;
        std::memcpy(ptr, &entry.ts, sizeof(Timestamp)); ptr += sizeof(Timestamp);
    }
    
    // Write restart points at the very end of the block
    uint32_t num_restarts = restart_points.size();
    uint16_t restarts_offset = 4096 - 4 - num_restarts * 2;
    std::memcpy(start_ptr + restarts_offset, restart_points.data(), num_restarts * 2);
    std::memcpy(start_ptr + 4096 - 4, &num_restarts, 4);
    
    pending_writes_.push_back(shared_buf);
    current_offset_ += 4096;
    current_block_entries_.clear();
    current_block_size_ = 0;
}

auto SstableBuilder::add(Slice key, Slice value, Timestamp ts) -> std::expected<void, std::error_code> {
    // Per-entry size: key + value + timestamp + 2 length prefixes,
    // plus 2 bytes for this entry's restart point.
    // It does NOT include the 8 block-level bytes: 4 for the entry count
    // at the block start and 4 for the restart count at the block end.
    size_t entry_size = 4 + key.size() + 4 + value.size() + sizeof(Timestamp) + 2;

    // Block is full: flush it to make room (+8 is the block overhead above).
    if (current_block_size_ + entry_size + 8 > 4096 && !current_block_entries_.empty()) {
        flush_data_block();
    }

    // Still does not fit even into an empty block -> the value is too large.
    if (current_block_size_ + entry_size + 8 > 4096) {
        return std::unexpected(make_error_code(StorageError::kValueTooLarge));
    }

    filter_.add(key);
    current_block_entries_.push_back({
        std::string(reinterpret_cast<const char*>(key.data()), key.size()),
        std::vector<uint8_t>(value.begin(), value.end()),
        ts
    });
    current_block_size_ += entry_size;
    return {};
}

void SstableBuilder::try_finish() {
    if (in_flight_writes_ == 0 && finish_called_) {
        if (finish_cb_) finish_cb_({});
    }
}

void SstableBuilder::finish(EmptyCallback cb) {
    flush_data_block(); // flush last block
    
    finish_cb_ = std::move(cb);
    finish_called_ = true;
    
    // Serialize index block
    uint32_t index_size = 4;
    for (const auto& idx : index_) {
        index_size += 4 + idx.first_key.size() + 8;
    }
    
    auto filter_data = filter_.data();
    uint32_t filter_size = filter_data.size();
    
    uint32_t payload_size = index_size + 4 + filter_size;
    uint32_t total_footer_size = payload_size + 4;
    uint32_t aligned_footer_size = (total_footer_size + 4095) & ~4095;
    
    auto footer_buf = std::make_shared<widedb::io::IoBuffer>(aligned_footer_size);
    uint8_t* ptr = footer_buf->data();
    
    uint32_t padding = (aligned_footer_size - 4) - payload_size;
    std::memset(ptr, 0, padding);
    ptr += padding;
    
    // Write Index
    uint32_t idx_count = index_.size();
    std::memcpy(ptr, &idx_count, 4); ptr += 4;
    for (const auto& idx : index_) {
        uint32_t ksz = idx.first_key.size();
        std::memcpy(ptr, &ksz, 4); ptr += 4;
        std::memcpy(ptr, idx.first_key.data(), ksz); ptr += ksz;
        std::memcpy(ptr, &idx.offset, 8); ptr += 8;
    }
    
    // Write Filter
    std::memcpy(ptr, &filter_size, 4); ptr += 4;
    std::memcpy(ptr, filter_data.data(), filter_size); ptr += filter_size;
    
    std::memcpy(footer_buf->data() + aligned_footer_size - 4, &payload_size, 4);
    
    pending_writes_.push_back(footer_buf);
    
    // Submit up to 64 writes concurrently (Pipelined I/O)
    auto do_next_writes = std::make_shared<std::function<void()>>();
    auto current_idx = std::make_shared<size_t>(0);
    
    *do_next_writes = [weak = weak_from_this(), do_next_writes, current_idx]() {
        auto self = weak.lock();
        if (!self) return;
        
        while (*current_idx < self->pending_writes_.size() && self->in_flight_writes_ < 64) {
            size_t i = (*current_idx)++;
            self->in_flight_writes_++;
            
            uint64_t offset = i < self->pending_writes_.size() - 1 ? i * 4096 : self->current_offset_;
            self->file_->write(offset, self->pending_writes_[i]->span(), [weak, do_next_writes](int res, uint32_t) {
                auto self = weak.lock();
                if (!self) return;
                
                self->in_flight_writes_--;
                if (res < 0) {
                    if (self->finish_cb_) self->finish_cb_(std::unexpected(std::error_code(-res, std::system_category())));
                    return;
                }
                (*do_next_writes)();
            });
        }
        self->try_finish();
    };
    
    (*do_next_writes)();
}

SstableReader::SstableReader(widedb::io::EventLoop* loop, const std::string& filepath)
    : loop_(loop), filepath_(filepath), filter_(10, 10) {}

void SstableReader::init(EmptyCallback cb) {
    file_ = std::make_shared<widedb::io::AsyncFile>(loop_, filepath_, O_RDONLY);
    if (!file_->is_open()) {
        cb(std::unexpected(std::error_code(EBADF, std::system_category())));
        return;
    }
    
    // 1. Stat file to get size
    struct stat st;
    if (fstat(file_->fd(), &st) < 0) {
        cb(std::unexpected(std::error_code(errno, std::system_category())));
        return;
    }
    
    if (st.st_size < 4096) {
        cb(std::unexpected(make_error_code(StorageError::kCorruption)));
        return;
    }
    
    // 2. Read the last 4KB block to get the footer
    uint64_t footer_offset = st.st_size - 4096;
    auto shared_buf = std::make_shared<widedb::io::IoBuffer>(4096);
    uint64_t file_size = st.st_size;
    file_->read(footer_offset, shared_buf->span(), [weak = weak_from_this(), cb = std::move(cb), shared_buf, file_size](int res, uint32_t) {
        auto self = weak.lock();
        if (!self) return;
        
        if (res < 0) {
            cb(std::unexpected(std::error_code(-res, std::system_category())));
            return;
        }
        
        uint32_t payload_size;
        std::memcpy(&payload_size, shared_buf->data() + 4096 - 4, 4);
        
        if (payload_size == 0) {
            cb(std::unexpected(make_error_code(StorageError::kCorruption)));
            return;
        }

        auto parse_footer = [self, cb](uint8_t* buf, uint32_t footer_cap, uint32_t payload_sz) {
            uint8_t* ptr = buf + footer_cap - 4 - payload_sz;
            
            // Parse Index
            uint32_t idx_count;
            std::memcpy(&idx_count, ptr, 4); ptr += 4;
            
            for (uint32_t i = 0; i < idx_count; ++i) {
                uint32_t ksz;
                std::memcpy(&ksz, ptr, 4); ptr += 4;
                std::string key(reinterpret_cast<const char*>(ptr), ksz); ptr += ksz;
                
                uint64_t offset;
                std::memcpy(&offset, ptr, 8); ptr += 8;
                self->index_.push_back({key, offset});
            }
            
            // Parse Filter
            uint32_t filter_size;
            std::memcpy(&filter_size, ptr, 4); ptr += 4;
            std::span<const uint8_t> fspan(ptr, filter_size);
            self->filter_ = BloomFilter(fspan, 10);
            
            cb({});
        };

        if (payload_size <= 4092) {
            parse_footer(shared_buf->data(), 4096, payload_size);
        } else {
            uint32_t total_footer_size = payload_size + 4;
            uint32_t aligned_footer_size = (total_footer_size + 4095) & ~4095;
            
            if (aligned_footer_size > file_size) {
                cb(std::unexpected(make_error_code(StorageError::kCorruption)));
                return;
            }
            
            auto large_buf = std::make_shared<widedb::io::IoBuffer>(aligned_footer_size);
            self->file_->read(file_size - aligned_footer_size, large_buf->span(), 
                [self, cb, large_buf, aligned_footer_size, payload_size, parse_footer](int res, uint32_t) {
                    if (res < 0) {
                        cb(std::unexpected(std::error_code(-res, std::system_category())));
                        return;
                    }
                    parse_footer(large_buf->data(), aligned_footer_size, payload_size);
                });
        }
    });
}

void SstableReader::get(Slice key, SstableGetCallback cb) {
    if (!filter_.may_contain(key)) {
        cb(std::nullopt);
        return;
    }
    
    std::string k_str(reinterpret_cast<const char*>(key.data()), key.size());
    auto it = std::upper_bound(index_.begin(), index_.end(), k_str, [](const std::string& a, const IndexEntry& b) {
        return a < b.first_key;
    });
    
    if (it == index_.begin()) {
        cb(std::nullopt);
        return;
    }
    
    --it; // The block that might contain the key
    uint64_t block_offset = it->offset;
    
    // Read the block directly using AsyncFile instead of BlockCache
    auto shared_buf = std::make_shared<widedb::io::IoBuffer>(4096);
    file_->read(block_offset, shared_buf->span(), [cb = std::move(cb), k_str, shared_buf](int res, uint32_t) {
        if (res < 0) {
            cb(std::nullopt);
            return;
        }
        
        const uint8_t* ptr = shared_buf->data();
        
        // Binary search using restart points
        uint32_t num_restarts;
        std::memcpy(&num_restarts, ptr + 4096 - 4, 4);
        
        int left = 0;
        int right = num_restarts - 1;

        while (left <= right) {
            int mid = left + (right - left) / 2;
            
            uint16_t offset;
            std::memcpy(&offset, ptr + 4096 - 4 - num_restarts * 2 + mid * 2, 2);
            
            const uint8_t* entry_ptr = ptr + offset;
            uint32_t ksz;
            std::memcpy(&ksz, entry_ptr, 4);
            std::string_view current_key(reinterpret_cast<const char*>(entry_ptr + 4), ksz);
            
            if (current_key == k_str) {
                entry_ptr += 4 + ksz;
                uint32_t vsz;
                std::memcpy(&vsz, entry_ptr, 4); entry_ptr += 4;
                
                std::vector<uint8_t> val_vec(entry_ptr, entry_ptr + vsz);
                cb(val_vec);
                return;
            } else if (current_key < k_str) {
                left = mid + 1;
            } else {
                right = mid - 1;
            }
        }
        
        cb(std::nullopt);
    });
}

} // namespace widedb::storage
