// main.cpp
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <thread>
#include <map>
#include <sstream>

// HTTP Request structure
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    
    // Parse raw HTTP request
    static HTTPRequest parse(const char* buffer) {
        HTTPRequest req;
        std::string request_str(buffer);
        std::istringstream stream(request_str);
        std::string line;
        
        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream line_stream(line);
            line_stream >> req.method >> req.path >> req.version;
        }
        
        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 2); // +2 for ": "
                // Remove \r
                if (!value.empty() && value.back() == '\r') {
                    value.pop_back();
                }
                req.headers[key] = value;
            }
        }
        
        // Parse body (if present)
        std::string body;
        while (std::getline(stream, line)) {
            body += line + "\n";
        }
        if (!body.empty()) {
            req.body = body;
        }
        
        return req;
    }
};

// HTTP Response generator
class HTTPResponse {
private:
    std::string status;
    std::map<std::string, std::string> headers;
    std::string body;

public:
    HTTPResponse() {
        headers["Content-Type"] = "text/plain";
        headers["Server"] = "C++ HTTP Server";
    }
    
    void setStatus(const std::string& s) { status = s; }
    void setHeader(const std::string& key, const std::string& value) { headers[key] = value; }
    void setBody(const std::string& b) { body = b; }
    
    std::string build() {
        std::string response = "HTTP/1.1 " + status + "\r\n";
        
        // Add Content-Length header
        headers["Content-Length"] = std::to_string(body.length());
        
        // Add all headers
        for (const auto& header : headers) {
            response += header.first + ": " + header.second + "\r\n";
        }
        
        response += "\r\n" + body;
        return response;
    }
};

// Client handler function (runs in separate thread)
void handleClient(int client_fd) {
    char buffer[4096] = {0};
    int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    
    if (bytes_read > 0) {
        // Parse HTTP request
        HTTPRequest request = HTTPRequest::parse(buffer);
        std::cout << "[" << std::this_thread::get_id() << "] " 
                  << request.method << " " << request.path << std::endl;
        
        // Create response
        HTTPResponse response;
        
        // Routing
        if (request.path == "/") {
            response.setStatus("200 OK");
            response.setHeader("Content-Type", "text/html");
            response.setBody(
                "<html><body>"
                "<h1>C++ HTTP Server</h1>"
                "<p>Thread ID: " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "</p>"
                "<a href='/api'>API</a> | "
                "<a href='/hello'>Hello</a> | "
                "<a href='/stats'>Stats</a>"
                "</body></html>"
            );
        }
        else if (request.path == "/api") {
            response.setStatus("200 OK");
            response.setHeader("Content-Type", "application/json");
            response.setBody(
                "{\"status\": \"success\", \"thread\": \"" + 
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + 
                "\", \"timestamp\": " + std::to_string(time(nullptr)) + "}"
            );
        }
        else if (request.path == "/hello") {
            response.setStatus("200 OK");
            response.setBody("Hello from thread: " + 
                std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
        }
        else if (request.path == "/stats") {
            response.setStatus("200 OK");
            response.setHeader("Content-Type", "text/html");
            response.setBody(
                "<html><body>"
                "<h1>Server Stats</h1>"
                "<p><strong>Thread ID:</strong> " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "</p>"
                "<p><strong>Timestamp:</strong> " + std::to_string(time(nullptr)) + "</p>"
                "</body></html>"
            );
        }
        else {
            response.setStatus("404 Not Found");
            response.setHeader("Content-Type", "text/html");
            response.setBody(
                "<html><body>"
                "<h1>404 - Not Found</h1>"
                "<p>The requested URL " + request.path + " was not found</p>"
                "</body></html>"
            );
        }
        
        // Send response
        std::string response_str = response.build();
        send(client_fd, response_str.c_str(), response_str.length(), 0);
    }
    
    close(client_fd);
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
    if (listen(server_fd, 10) < 0) {  // Increased backlog
        std::cerr << "Listen failed" << std::endl;
        return 1;
    }
    
    std::cout << "Multi-threaded HTTP Server listening on port 8080...\n";
    std::cout << "Available routes: /, /api, /hello, /stats\n";
    
    // Main accept loop
    while(true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            std::cerr << "Accept failed" << std::endl;
            continue;
        }
        
        // Create thread for each client
        std::thread clientThread(handleClient, client_fd);
        clientThread.detach(); // Detach thread (for production use thread pool)
    }
    
    close(server_fd);
    return 0;
}