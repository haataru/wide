# 01. IO Framework Architecture

This document describes the lowest layer of the SDK: the completely asynchronous, zero-copy I/O subsystem.

## Core Concepts

The IO framework is built on two strict principles:
1. **Asynchronous Execution via io_uring**: All blocking syscalls are eliminated. Reads, writes, and timers are submitted to the Linux kernel via shared memory ring buffers (Submission Queue).
2. **Direct I/O (O_DIRECT)**: The OS page cache is entirely bypassed. This prevents memory bloat and guarantees that an I/O completion means the data has physically hit the NVMe device or has been read directly into user-space memory.

## Components

### 1. IoUringContext
A thin, RAII wrapper around the raw `liburing` structs. It manages the lifecycle of the ring buffers and provides the raw primitives to `get_sqe()` (get a submission queue entry), `peek_cqe()` (poll for completions), and `advance_cqe()` (acknowledge completions).

### 2. EventLoop
The beating heart of the system. It owns the `IoUringContext` and maintains an active event loop.
- **Callback Slots**: Provides an array of `IoCallback` slots. Every `io_uring` request is tagged with a unique `user_data` integer comprising a slot index and a generation counter.
- **CQE Harvesting**: The `run_once()` method polls the Completion Queue. For every completion, it decodes the `user_data`, retrieves the corresponding lambda callback from the slot array, and invokes it.
- **Timers**: Supports asynchronous timers via `io_uring_prep_timeout`.

### 3. IoBuffer
Because `O_DIRECT` strictly requires aligned memory, `std::vector` and `malloc` are insufficient. `IoBuffer` allocates memory aligned to 4096 bytes using `posix_memalign`. It manages the lifetime of these aligned byte arrays and ensures data is structurally sound before being submitted to the kernel.

### 4. AsyncFile
An RAII wrapper over a Linux file descriptor.
- Opened strictly with `O_DIRECT | O_RDWR`.
- Provides `read()` and `write()` methods that accept an `IoBuffer` or `std::span` (ensuring 4KB alignment).
- Exposes `fsync()` for physical durability guarantees.
- Submits the operation to the `EventLoop` and calls the user's callback upon hardware completion.
