# 02. Storage Engine Architecture

This document details the immutable block storage layer, built entirely over the `io_uring` and `O_DIRECT` primitives from the IO Framework.

## Core Concepts

The engine stores data in Sorted String Tables (SSTables). Unlike traditional systems that map SSTables into the page cache (e.g., via `mmap`), this engine explicitly manages memory and issues direct I/O requests.

An SSTable is strictly immutable. Once built and closed, it can only be read.

## Components

### 1. SstableBuilder
Responsible for constructing a new SSTable file on disk.
- **Pipelined I/O**: Instead of waiting for one block to be written to disk before processing the next, the builder accepts key-value pairs synchronously, buffers them into 4KB `IoBuffer`s, and flushes blocks asynchronously. It allows up to 64 concurrent block writes in-flight.
- **Data Blocks**: Each block stores serialized keys and values. At the end of the 4KB block, it stores an array of "restart points" (offsets to the start of strings) to enable fast binary search within the block.
- **Footer**: When `finish()` is called, the builder writes the Index and the Bloom Filter to the end of the file. The size of the footer is dynamically aligned to 4096 bytes.

### 2. SstableReader
Responsible for querying an existing SSTable.
- **Initialization**: Upon `init()`, the reader executes an asynchronous `read()` of the final blocks of the file to retrieve the Footer. It parses the Index (a map of the first key in a block to its physical offset) and the Bloom Filter. It handles footers of any size seamlessly.
- **Get Operation**: When `get(key)` is called:
  1. It checks the in-memory Bloom Filter. If the key is absent, it returns immediately.
  2. It performs a binary search over the in-memory Index to find the single 4KB block that could contain the key.
  3. It allocates an `IoBuffer` and dispatches an asynchronous `read()` via `AsyncFile` for that precise 4KB block on disk.
  4. Once the read completes, it performs a binary search inside the block utilizing the block's restart points.
  5. The raw byte value is returned to the caller.

### 3. Bloom Filter
A probabilistic data structure used to drastically reduce unnecessary disk I/O.
- Serialized alongside the SSTable index.
- Uses `xxHash` for ultra-fast, hardware-accelerated hashing of keys.
- Ensures that read requests for non-existent keys almost never result in physical disk reads.
