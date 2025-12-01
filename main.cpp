#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <thread>
#include <map>
#include <sstream>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <mutex>
#include <functional> 

namespace fs = std::filesystem;

// Configuration class
class Config {
private:
    std::string document_root = ".";
    int port = 8080;
    int max_threads = 10;
    bool verbose = false;

public:
    static Config parseArgs(int argc, char* argv[]) {
        Config config;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-p" || arg == "--port") {
                if (i + 1 < argc) config.port = std::stoi(argv[++i]);
            }
            else if (arg == "-d" || arg == "--directory") {
                if (i + 1 < argc) config.document_root = argv[++i];
            }
            else if (arg == "-v" || arg == "--verbose") {
                config.verbose = true;
            }
            else if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  -p, --port PORT      Server port (default: 8080)\n"
                          << "  -d, --directory DIR  Document root (default: .)\n"
                          << "  -v, --verbose        Enable verbose logging\n"
                          << "  -h, --help          Show this help\n";
                exit(0);
            }
        }
        
        return config;
    }
    
    // Getters
    int getPort() const { return port; }
    std::string getDocumentRoot() const { return document_root; }
    bool isVerbose() const { return verbose; }
};

// Thread-safe logger
class Logger {
private:
    std::mutex log_mutex;
    bool verbose = false;

public:
    void setVerbose(bool v) { verbose = v; }

    void log(const std::string& message) {
        std::lock_guard<std::mutex> lock(log_mutex);
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] " 
                  << message << std::endl;
    }

    void debug(const std::string& message) {
        if (verbose) {
            log("[DEBUG] " + message);
        }
    }
};

Logger logger;

// Global statistics
struct ServerStats {
    std::atomic<int> total_requests{0};
    std::atomic<int> active_connections{0};
    std::atomic<int> success_responses{0};
    std::atomic<int> error_responses{0};
    std::chrono::steady_clock::time_point start_time = std::chrono::steady_clock::now();
};

ServerStats stats;

// HTTP Request with advanced parsing
struct HTTPRequest {
    std::string method;
    std::string path;
    std::string version;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> query_params;
    std::string body;
    
    static HTTPRequest parse(const char* buffer) {
        HTTPRequest req;
        std::string request_str(buffer);
        std::istringstream stream(request_str);
        std::string line;
        
        // Parse request line
        if (std::getline(stream, line)) {
            std::istringstream line_stream(line);
            line_stream >> req.method >> req.path >> req.version;
            
            // Parse query parameters
            size_t query_pos = req.path.find('?');
            if (query_pos != std::string::npos) {
                std::string query_str = req.path.substr(query_pos + 1);
                req.path = req.path.substr(0, query_pos);
                
                std::istringstream query_stream(query_str);
                std::string pair;
                while (std::getline(query_stream, pair, '&')) {
                    size_t eq_pos = pair.find('=');
                    if (eq_pos != std::string::npos) {
                        std::string key = pair.substr(0, eq_pos);
                        std::string value = pair.substr(eq_pos + 1);
                        req.query_params[key] = value;
                    }
                }
            }
        }
        
        // Parse headers
        while (std::getline(stream, line) && line != "\r") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 2);
                if (!value.empty() && value.back() == '\r') value.pop_back();
                req.headers[key] = value;
            }
        }
        
        // Parse body
        if (req.headers.count("Content-Length")) {
            int content_length = std::stoi(req.headers["Content-Length"]);
            std::string body;
            body.resize(content_length);
            stream.read(&body[0], content_length);
            req.body = body;
        }
        
        return req;
    }
};

// Advanced HTTP Response with file serving
class HTTPResponse {
private:
    std::string status;
    std::map<std::string, std::string> headers;
    std::string body;
    std::string document_root;

public:
    HTTPResponse(const std::string& root = ".") : document_root(root) {
        headers["Server"] = "C++ HTTP Server 2.0";
        headers["Connection"] = "close";
    }
    
    void setStatus(int code, const std::string& message) { 
        status = std::to_string(code) + " " + message; 
    }
    
    void setHeader(const std::string& key, const std::string& value) { 
        headers[key] = value; 
    }
    
    void setBody(const std::string& b) { 
        body = b; 
        headers["Content-Length"] = std::to_string(body.length());
    }
    
    // Serve static files
    bool serveFile(const std::string& filepath) {
        std::string full_path = document_root + filepath;
        
        if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
            setStatus(404, "Not Found");
            setBody("<h1>404 - File Not Found</h1><p>File: " + filepath + "</p>");
            return false;
        }
        
        try {
            std::ifstream file(full_path, std::ios::binary);
            if (!file) return false;
            
            // Determine content type
            std::string extension = fs::path(full_path).extension().string();
            std::map<std::string, std::string> mime_types = {
                {".html", "text/html"},
                {".css", "text/css"},
                {".js", "application/javascript"},
                {".json", "application/json"},
                {".png", "image/png"},
                {".jpg", "image/jpeg"},
                {".txt", "text/plain"}
            };
            
            std::string content_type = mime_types.count(extension) ? 
                mime_types[extension] : "application/octet-stream";
            
            // Read file
            std::stringstream buffer;
            buffer << file.rdbuf();
            setBody(buffer.str());
            setStatus(200, "OK");
            setHeader("Content-Type", content_type);
            return true;
            
        } catch (...) {
            setStatus(500, "Internal Server Error");
            setBody("<h1>500 - File Read Error</h1>");
            return false;
        }
    }
    
    std::string build() {
        std::string response = "HTTP/1.1 " + status + "\r\n";
        
        for (const auto& header : headers) {
            response += header.first + ": " + header.second + "\r\n";
        }
        
        response += "\r\n" + body;
        return response;
    }
};

// Route handler system
class Router {
private:
    std::map<std::string, std::function<void(HTTPRequest&, HTTPResponse&)>> routes;
    std::string document_root;

public:
    Router(const std::string& root = ".") : document_root(root) {}

    void get(const std::string& path, std::function<void(HTTPRequest&, HTTPResponse&)> handler) {
        routes["GET " + path] = handler;
    }
    
    void post(const std::string& path, std::function<void(HTTPRequest&, HTTPResponse&)> handler) {
        routes["POST " + path] = handler;
    }
    
    bool handle(HTTPRequest& req, HTTPResponse& res) {
        std::string key = req.method + " " + req.path;
        if (routes.count(key)) {
            routes[key](req, res);
            return true;
        }
        return false;
    }

    std::string getDocumentRoot() const { return document_root; }
};

// Initialize routes
Router setupRoutes(const std::string& document_root) {
    Router router(document_root);
    
    // Homepage
    router.get("/", [&router](HTTPRequest& req, HTTPResponse& res) {
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody(
            "<html><head><title>C++ HTTP Server</title><style>"
            "body { font-family: Arial, sans-serif; max-width: 800px; margin: 0 auto; padding: 20px; }"
            ".card { border: 1px solid #ddd; padding: 20px; margin: 10px 0; border-radius: 5px; }"
            "</style></head><body>"
            "<h1>C++ HTTP Server 2.0</h1>"
            "<div class='card'><h3>Available Routes:</h3>"
            "<ul>"
            "<li><a href='/'>Home</a></li>"
            "<li><a href='/api'>API</a></li>"
            "<li><a href='/stats'>Statistics</a></li>"
            "<li><a href='/files'>File Browser</a></li>"
            "<li><a href='/hello?name=Visitor'>Hello with params</a></li>"
            "</ul></div>"
            "<p><strong>Document Root:</strong> " + router.getDocumentRoot() + "</p>"
            "<p><strong>Thread ID:</strong> " + 
            std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "</p>"
            "</body></html>"
        );
    });
    
    // JSON API
    router.get("/api", [&router](HTTPRequest& req, HTTPResponse& res) {
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(
            "{\n"
            "  \"server\": \"C++ HTTP Server\",\n"
            "  \"version\": \"2.0\",\n"
            "  \"document_root\": \"" + router.getDocumentRoot() + "\",\n"
            "  \"thread\": \"" + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "\",\n"
            "  \"timestamp\": " + std::to_string(time(nullptr)) + ",\n"
            "  \"status\": \"running\"\n"
            "}"
        );
    });
    
    // Statistics
    router.get("/stats", [&router](HTTPRequest& req, HTTPResponse& res) {
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - stats.start_time);
            
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody(
            "<html><head><title>Server Statistics</title></head><body>"
            "<h1>📊 Server Statistics</h1>"
            "<div style='border: 1px solid #ccc; padding: 20px; border-radius: 5px;'>"
            "<p><strong>Total Requests:</strong> " + std::to_string(stats.total_requests) + "</p>"
            "<p><strong>Active Connections:</strong> " + std::to_string(stats.active_connections) + "</p>"
            "<p><strong>Success Responses:</strong> " + std::to_string(stats.success_responses) + "</p>"
            "<p><strong>Error Responses:</strong> " + std::to_string(stats.error_responses) + "</p>"
            "<p><strong>Uptime:</strong> " + std::to_string(uptime.count()) + " seconds</p>"
            "<p><strong>Document Root:</strong> " + router.getDocumentRoot() + "</p>"
            "<p><strong>Total Requests:</strong> " + std::to_string(stats.total_requests) + "</p>"
            "<p><strong>Active Connections:</strong> " + std::to_string(stats.active_connections) + "</p>"
            "</div></body></html>"
        );
    });
    
    // File browser
    router.get("/files", [&router](HTTPRequest& req, HTTPResponse& res) {
        std::string html = "<html><head><title>File Browser</title></head><body>"
                        "<h1>📁 File Browser - " + router.getDocumentRoot() + "</h1><ul>";
        
        try {
            for (const auto& entry : fs::directory_iterator(router.getDocumentRoot())) {
                std::string filename = entry.path().filename().string();
                std::string type = entry.is_directory() ? "📁" : "📄";
                html += "<li>" + type + " <a href='/static/" + filename + "'>" + filename + "</a></li>";
            }
        } catch (...) {
            html += "<li>Error reading directory: " + router.getDocumentRoot() + "</li>";
        }
        
        html += "</ul></body></html>";
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody(html);
    });
    
    // Static file serving
    router.get("/static", [&router](HTTPRequest& req, HTTPResponse& res) {
        std::string filepath = req.path.substr(7); // Remove "/static"
        HTTPResponse file_response(router.getDocumentRoot());
        if (!file_response.serveFile(filepath)) {
            res.setStatus(404, "Not Found");
            res.setBody("File not found: " + filepath);
        } else {
            res = file_response;
        }
    });
    
    // Parameter example
    router.get("/hello", [](HTTPRequest& req, HTTPResponse& res) {
        std::string name = "World";
        if (req.query_params.count("name")) {
            name = req.query_params["name"];
        }
        
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "text/html");
        res.setBody(
            "<html><body>"
            "<h1>👋 Hello, " + name + "!</h1>"
            "<p>Try adding <code>?name=YourName</code> to the URL</p>"
            "</body></html>"
        );
    });

    router.get("/check", [&router](HTTPRequest& req, HTTPResponse& res) {
        if (req.query_params.count("file")) {
            std::string filename = req.query_params["file"];
            std::string full_path = router.getDocumentRoot() + "/" + filename;
            
            bool exists = fs::exists(full_path) && fs::is_regular_file(full_path);
            
            res.setStatus(200, "OK");
            res.setHeader("Content-Type", "application/json");
            res.setBody(
                "{\n"
                "  \"file\": \"" + filename + "\",\n"
                "  \"exists\": " + (exists ? "true" : "false") + ",\n"
                "  \"path\": \"" + full_path + "\",\n"
                "  \"timestamp\": " + std::to_string(time(nullptr)) + "\n"
                "}"
            );
        } else {
            res.setStatus(400, "Bad Request");
            res.setHeader("Content-Type", "application/json");
            res.setBody(
                "{\n"
                "  \"error\": \"Missing 'file' parameter\",\n"
                "  \"usage\": \"/check?file=filename.txt\"\n"
                "}"
            );
        }
    });

    router.get("/health", [](HTTPRequest& req, HTTPResponse& res) {
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(
            "{\n"
            "  \"status\": \"healthy\",\n"
            "  \"uptime\": " + std::to_string(stats.total_requests) + ",\n"
            "  \"timestamp\": " + std::to_string(time(nullptr)) + "\n"
            "}"
        );
    });
    
    // POST example
    router.post("/echo", [](HTTPRequest& req, HTTPResponse& res) {
        res.setStatus(200, "OK");
        res.setHeader("Content-Type", "application/json");
        res.setBody(
            "{\n"
            "  \"received_body\": \"" + req.body + "\",\n"
            "  \"content_length\": " + std::to_string(req.body.length()) + "\n"
            "}"
        );
    });
    
    return router;
}

// Client handler
void handleClient(int client_fd, const Config& config) {
    stats.active_connections++;
    stats.total_requests++;
    
    char buffer[8192] = {0};
    int bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
    
    if (bytes_read > 0) {
        try {
            HTTPRequest request = HTTPRequest::parse(buffer);
            HTTPResponse response(config.getDocumentRoot());
            Router router = setupRoutes(config.getDocumentRoot());
            
            logger.log(request.method + " " + request.path + " - Thread: " + 
                      std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
            
            if (router.handle(request, response)) {
                stats.success_responses++;
            } else {
                response.setStatus(404, "Not Found");
                response.setHeader("Content-Type", "text/html");
                response.setBody("<h1>404 - Route Not Found</h1><p>Path: " + request.path + "</p>");
                stats.error_responses++;
            }
            
            std::string response_str = response.build();
            send(client_fd, response_str.c_str(), response_str.length(), 0);
            
        } catch (const std::exception& e) {
            logger.log("ERROR: " + std::string(e.what()));
            HTTPResponse error_response(config.getDocumentRoot());
            error_response.setStatus(500, "Internal Server Error");
            error_response.setBody("<h1>500 - Server Error</h1>");
            std::string error_str = error_response.build();
            send(client_fd, error_str.c_str(), error_str.length(), 0);
            stats.error_responses++;
        }
    }
    
    close(client_fd);
    stats.active_connections--;
}

int main(int argc, char* argv[]) {
    Config config = Config::parseArgs(argc, argv);
    logger.setVerbose(config.isVerbose());
    
    logger.log("🚀 Starting C++ HTTP Server 2.0...");
    logger.log("📋 Configuration: port=" + std::to_string(config.getPort()) + 
               ", document_root=" + config.getDocumentRoot() +
               ", verbose=" + (config.isVerbose() ? "true" : "false"));
    
    // Create socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        logger.log("❌ Socket creation failed");
        return 1;
    }
    
    // Set socket options to reuse address
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        logger.log("❌ Socket options failed");
        return 1;
    }
    
    // Configure address
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config.getPort());
    
    // Bind socket
    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
        logger.log("❌ Bind failed - try changing port or killing existing process");
        close(server_fd);
        return 1;
    }
    
    // Start listening
    if (listen(server_fd, 20) < 0) {
        logger.log("❌ Listen failed");
        close(server_fd);
        return 1;
    }
    
    logger.log("✅ Server listening on http://localhost:" + std::to_string(config.getPort()));
    logger.log("📊 Available routes: /, /api, /stats, /files, /hello, /static/*, /echo (POST)");
    
    // Create some test files in document root
    std::string root = config.getDocumentRoot();
    std::ofstream test_file(root + "/test.html");
    test_file << "<html><body><h1>Test File</h1><p>Served by C++ HTTP Server from " << root << "!</p></body></html>";
    test_file.close();
    
    std::ofstream api_file(root + "/api.json");
    api_file << "{\"message\": \"Static JSON file from " << root << "\"}";
    api_file.close();

    logger.debug("Created test files in " + root);
    
    // Main accept loop
    while(true) {
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd < 0) {
            logger.log("❌ Accept failed");
            continue;
        }
        
        std::thread clientThread(handleClient, client_fd, std::ref(config));
        clientThread.detach();
    }
    
    close(server_fd);
    return 0;
}