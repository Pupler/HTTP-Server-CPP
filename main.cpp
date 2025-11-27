#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>

int main() {
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Configure address
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);
    
    // Bind socket to port
    bind(server_fd, (sockaddr*)&address, sizeof(address));
    
    // Start listening
    listen(server_fd, 5);
    std::cout << "Server listening on port 8080...\n";
    
    // Accept single connection
    int client_fd = accept(server_fd, nullptr, nullptr);
    
    // Send basic HTTP response
    const char* response = 
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "\r\n"
        "Hello from C++ server!";
    send(client_fd, response, strlen(response), 0);
    
    // Cleanup
    close(client_fd);
    close(server_fd);
    return 0;
}