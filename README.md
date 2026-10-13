# Wide

[![C++](https://img.shields.io/badge/C%2B%2B-26-blue.svg?logo=cplusplus)](https://isocpp.org)
[![License](https://img.shields.io/badge/license-GPLv3-green.svg?logo=gnu)](https://www.gnu.org/licenses/gpl-3.0)
[![Status](https://img.shields.io/badge/status-early%20access-orange.svg)](https://github.com/your-org/wide)
[![Build](https://img.shields.io/badge/build-passing-brightgreen.svg?logo=github)](https://github.com/haataru/wide/actions)

## Overview

A hyper-optimized, zero-copy, C++26 immutable block storage SDK designed as a
foundational layer for building high-performance database engines.

This SDK provides a bare-metal interface for writing and reading Sorted String
Tables (SSTables). It completely bypasses the OS page cache using Direct I/O
(`O_DIRECT`) and executes all operations asynchronously via Linux `io_uring`.

There are no MemTables, no WALs, and no high-level query planners. It is
strictly a low-level engine that provides extremely fast, pipelined disk I/O
with ~0 ms overhead.

## Key Features

- **C++26 & Zero-Cost Abstractions**: Written in modern C++ without exceptions.
  Errors are handled via `std::expected`.
- **Pure `io_uring`**: Fully asynchronous I/O framework (`EventLoop`,
  `AsyncFile`) leveraging `io_uring` for submission and completion queue polling.
- **Direct I/O (`O_DIRECT`)**: Reads and writes bypass the kernel page cache.
  Memory is strictly 4KB-aligned via `posix_memalign`.
- **Immutable SSTables**: Pipelined concurrent block writes (up to 64 in-flight).
  Footer includes a binary-searchable Index and a Bloom Filter for fast `get()`
  lookups.
- **Zero-Copy Architecture**: Uses `std::span` and raw byte vectors to avoid
  unnecessary memory allocations.

> [!IMPORTANT]
> Requires Linux. `io_uring` and `O_DIRECT` are not portable — macOS and Windows
> are not supported.

## Building

Requires Linux (for `io_uring`), GCC 15+ or Clang 20+, and CMake 3.25+.

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./tests/unit/widedb_unit_tests
```

> [!TIP]
> Build with `-DCMAKE_BUILD_TYPE=Release` for production numbers. Debug builds
> disable key inlining and can be 5–10× slower.

## Learn more

- Read the [IO Framework docs](docs/framework.md).
- Read the [Storage Engine docs](docs/storage_engine.md).
- Open an issue or start a discussion on GitHub.
