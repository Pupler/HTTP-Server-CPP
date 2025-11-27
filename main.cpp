#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <string>

// Parse HTTP request path
std::string getRequestPath(const char* buffer) {
    std::string request(buffer);
    size_t start = request.find(' ') + 1;
    size_t end = request.find(' ', start);
    if (start == std::string::npos || end == std::string::npos) {
        return "/";
    }
    return request.substr(start, end - start);
}

// Generate HTTP response based on path
std::string generateResponse(const std::string& path) {
    if (path == "/") {
        return 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body><h1>Homepage</h1><p>Welcome to C++ server!</p></body></html>";
    }
    else if (path == "/api") {
        return 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "\r\n"
            "{\"status\": \"success\", \"message\": \"API response\"}";
    }
    else if (path == "/hello") {
        return 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "\r\n"
            "Hello World from C++ server!";
    }
    else {
        return 
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/html\r\n"
            "\r\n"
            "<html><body><h1>404</h1><p>Page not found</p></body></html>";
    }
}

int main() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        std::cerr << "Socket failed" << std::endl;
        return 1;
    }
    
    // Configure address
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // Bind socket
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        return 1;
    }
    
    // Start listening
    if (listen(server_fd, 5) < 0) {
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }
    
    std::cout << "Server listening on port 8080...\n";
    
    // Main loop
    while(true) {
        // Accept connection
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // Read HTTP request
        char buffer[1024] = {0};
        int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
        if (bytes_read > 0) {
            // Parse request path
            std::string path = getRequestPath(buffer);
            std::cout << "Requested path: " << path << std::endl;
            
            // Generate and send response
            std::string response = generateResponse(path);
            send(client_fd, response.c_str(), response.length(), 0);
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}