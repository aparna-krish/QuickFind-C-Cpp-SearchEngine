# QuickFind: A Systems-Level Search Engine

QuickFind is a full-stack, high-performance file search engine built entirely in C and C++. This project was developed across four progressive stages, starting from low-level data structures and culminating in a multithreaded web-based search application.

### Features

- 🔗 Custom-built doubly-linked list and chained hash table in C
- 🧠 In-memory inverted index for efficient word-based search
- 🗃️ Architecture-neutral binary index format with endianness conversion
- 💬 Console-based and web-based search interfaces
- 🌐 Multithreaded web server with static file serving and query processing

### Technology Stack

- C (Memory-safe data structures, indexing logic)
- C++ (Disk-based storage, query processing, multithreading)
- POSIX Sockets, Valgrind, gtest, cpplint
- HTML (UI templates), TCP/IP networking

### Key Components

| Component | Description |
|----------|-------------|
| `hw1/` | Generic doubly-linked list and chained hash table (C) |
| `hw2/` | File parser, crawler, and in-memory inverted index |
| `hw3/` | Binary on-disk index writer/reader with query processor (C++) |
| `hw4/` | Multithreaded HTTP server supporting file serving and search |

### Example Usage

```bash
# Build and run the web server
cd hw4/
make
./http333d 5555 ../projdocs unit_test_indices/*
