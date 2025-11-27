# 📦 HTTP Server in C++

A minimal, educational HTTP/1.1 server implementation written from scratch in C++ for learning network programming concepts.

## 🎯 Educational Purpose

**This is a TEST SERVER for learning purposes only** - designed to understand:
- Low-level socket programming with BSD sockets
- HTTP protocol implementation
- Multithreading and concurrent connections
- Network I/O operations
- Request/response lifecycle
- **NOT suitable for production use**

## ✨ Features

- **Basic HTTP/1.1** protocol support
- **Multithreaded** client handling
- **Static file serving** with MIME types
- **JSON API** endpoints
- **Query parameter** parsing
- **GET/POST** request handling
- **Live statistics** dashboard
- **File browser** interface
- **Custom routing** system

## 🛠️ Tech Stack

- C++17
- BSD Sockets API
- POSIX Threads
- STL Containers & Algorithms
- CMake build system

## 🚀 Getting started

```bash
# Clone and build
git clone https://github.com/Pupler/HTTP-Server-CPP.git
cd HTTP-Server-CPP
g++ main.cpp -o server

# Run server
./server
```
