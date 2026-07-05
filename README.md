# Wide

A hyper-optimized, zero-copy, C++26 immutable block storage SDK designed as a foundational layer for building high-performance database engines.

## Overview

This SDK provides a bare-metal interface for writing and reading Sorted String Tables (SSTables). It completely bypasses the OS page cache using Direct I/O (`O_DIRECT`) and executes all operations asynchronously via Linux `io_uring`. 

There are no MemTables, no WALs, and no high-level query planners. It is strictly a low-level engine that provides extremely fast, pipelined disk I/O with ~0 ms overhead.

## Key Features

- **C++26 & Zero-Cost Abstractions**: Written in modern C++ without exceptions. Errors are handled via `std::expected`.
- **Pure `io_uring`**: Fully asynchronous I/O framework (`EventLoop`, `AsyncFile`) leveraging `io_uring` for submission and completion queue polling.
- **Direct I/O (`O_DIRECT`)**: Reads and writes bypass the kernel page cache. Memory is strictly 4KB-aligned via `posix_memalign`.
- **Immutable SSTables**: Pipelined concurrent block writes (up to 64 in-flight). Footer includes a binary-searchable Index and a Bloom Filter for fast `get()` lookups.
- **Zero-Copy Architecture**: Uses `std::span` and raw byte vectors to avoid unnecessary memory allocations.

## Building

Requires Linux (for `io_uring`), GCC 15+ or Clang 20+, and CMake 3.25+.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./tests/unit/widedb_unit_tests
```

## Documentation
For deep technical details, see the `docs/` directory:
- [IO Framework](docs/framework.md)
- [Storage Engine](docs/storage_engine.md)
