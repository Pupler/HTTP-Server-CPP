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

- **HTTP/1.1** protocol support
- **Multithreaded** client handling
- **Static file serving** with MIME types
- **JSON API** endpoints
- **Query parameter** parsing
- **GET/POST** request handling
- **Live statistics** dashboard
- **File browser** interface
- **Custom routing** system
- **Configurable** port and document root
- **Verbose logging** mode

## 🛠️ Tech Stack

- C++17
- BSD Sockets API
- POSIX Threads
- STL Containers & Algorithms

## 🚀 Getting started

```bash
# Clone and build
git clone https://github.com/Pupler/HTTP-Server-CPP.git
cd HTTP-Server-CPP

# Compile
g++ -std=c++17 -pthread main.cpp -o server -lstdc++fs

# Run with defaults (port 8080, current directory)
./server

# Run with custom configuration
./server -p 9000 -d ./public -v
```

## ⚙️ Configuration

```bash
./server -p 8080              # Custom port
./server -d ./www             # Custom document root  
./server -v                   # Verbose logging
./server --help               # Show help
```

## 🌐 Available Routes

| Method | Route | Description |
|--------|-------|-------------|
| `GET` | `/` | Homepage with navigation |
| `GET` | `/api` | JSON API endpoint |
| `GET` | `/stats` | Live server statistics |
| `GET` | `/files` | File browser |
| `GET` | `/hello?name=User` | Parameter example |
| `GET` | `/static/{filename}` | Static file serving |
| `POST` | `/echo` | Echo POST requests |

## 📄 License

This project is licensed under the MIT License - see the LICENSE file for details.
