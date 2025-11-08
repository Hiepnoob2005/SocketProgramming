#include "common.h"
#include <sstream>
#include <fstream>

class WebClient {
public:
    WebClient() : httpSocket(INVALID_SOCKET_VALUE), 
                  remoteSocket(INVALID_SOCKET_VALUE),
                  remoteConnected(false) {
        initWinsock();
    }
    
    ~WebClient() {
        stop();
        cleanupWinsock();
    }
    
    bool startWebServer() {
        // Create HTTP server socket
        httpSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (httpSocket == INVALID_SOCKET_VALUE) {
            std::cerr << "Failed to create HTTP socket" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(httpSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(8080);
        
        if (bind(httpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "HTTP bind failed" << std::endl;
            return false;
        }
        
        if (listen(httpSocket, 5) < 0) {
            std::cerr << "HTTP listen failed" << std::endl;
            return false;
        }
        
        std::cout << "Web interface started at http://localhost:8080" << std::endl;
        
        // Create HTML file
        createHTMLFile();
        
        return true;
    }
    
    bool connectToRemote(const std::string& ip) {
        remoteIP = ip;
        
        remoteSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (remoteSocket == INVALID_SOCKET_VALUE) {
            return false;
        }
        
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        
#ifdef _WIN32
        serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
#else
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
#endif
        
        if (::connect(remoteSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            return false;
        }
        
        remoteConnected = true;
        std::cout << "Connected to remote server: " << ip << std::endl;
        return true;
    }
    
    void run() {
        std::cout << "Enter remote server IP: ";
        std::cin >> remoteIP;
        
        if (!connectToRemote(remoteIP)) {
            std::cerr << "Failed to connect to remote server!" << std::endl;
        }
        
        std::cout << "Web server running..." << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        while (true) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            SOCKET_TYPE clientSocket = accept(httpSocket, 
                                             (struct sockaddr*)&clientAddr, 
                                             &clientLen);
            
            if (clientSocket != INVALID_SOCKET_VALUE) {
                std::thread clientThread(&WebClient::handleHTTPClient, this, clientSocket);
                clientThread.detach();
            }
        }
    }
    
private:
    SOCKET_TYPE httpSocket;
    SOCKET_TYPE remoteSocket;
    std::string remoteIP;
    bool remoteConnected;
    
    void createHTMLFile() {
        std::ofstream file("index.html");
        file << "<!DOCTYPE html>\n";
        file << "<html>\n";
        file << "<head>\n";
        file << "<meta charset=\"UTF-8\">\n";
        file << "<title>Remote Control Panel</title>\n";
        file << "<style>\n";
        file << "body { font-family: Arial; padding: 20px; background: #f0f0f0; }\n";
        file << ".container { max-width: 800px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 20px rgba(0,0,0,0.1); }\n";
        file << "h1 { color: #333; text-align: center; }\n";
        file << ".btn-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(150px, 1fr)); gap: 10px; margin: 20px 0; }\n";
        file << "button { padding: 15px; background: #4CAF50; color: white; border: none; border-radius: 5px; cursor: pointer; font-size: 14px; }\n";
        file << "button:hover { background: #45a049; }\n";
        file << "button.danger { background: #f44336; }\n";
        file << "button.danger:hover { background: #da190b; }\n";
        file << ".input-group { margin: 20px 0; }\n";
        file << "input { padding: 10px; margin: 5px; width: 200px; }\n";
        file << "#output { background: #000; color: #0f0; padding: 15px; height: 300px; overflow-y: auto; font-family: monospace; margin-top: 20px; }\n";
        file << "</style>\n";
        file << "</head>\n";
        file << "<body>\n";
        file << "<div class=\"container\">\n";
        file << "<h1>Remote Control Panel</h1>\n";
        file << "<div id=\"status\">Status: <span id=\"statusText\">Connecting...</span></div>\n";
        file << "<div class=\"btn-grid\">\n";
        file << "<button onclick=\"sendCmd(1)\">List Apps</button>\n";
        file << "<button onclick=\"sendCmd(4)\">List Process</button>\n";
        file << "<button onclick=\"sendCmd(6)\">Screenshot</button>\n";
        file << "<button onclick=\"sendCmd(7)\">Start Keylogger</button>\n";
        file << "<button onclick=\"sendCmd(8)\">Stop Keylogger</button>\n";
        file << "<button onclick=\"sendCmd(9)\">Get Keylog</button>\n";
        file << "<button onclick=\"sendCmd(10)\">Webcam On</button>\n";
        file << "<button onclick=\"sendCmd(11)\">Webcam Off</button>\n";
        file << "<button class=\"danger\" onclick=\"if(confirm('Shutdown?')) sendCmd(12)\">Shutdown</button>\n";
        file << "<button class=\"danger\" onclick=\"if(confirm('Restart?')) sendCmd(13)\">Restart</button>\n";
        file << "</div>\n";
        file << "<div class=\"input-group\">\n";
        file << "<input type=\"text\" id=\"appName\" placeholder=\"App name\">\n";
        file << "<button onclick=\"stopApp()\">Stop App</button>\n";
        file << "<button onclick=\"startApp()\">Start App</button>\n";
        file << "</div>\n";
        file << "<div class=\"input-group\">\n";
        file << "<input type=\"text\" id=\"pid\" placeholder=\"Process ID\">\n";
        file << "<button onclick=\"killProc()\">Kill Process</button>\n";
        file << "</div>\n";
        file << "<div id=\"output\">Output will appear here...</div>\n";
        file << "</div>\n";
        file << "<script>\n";
file << "function addOutput(text, isHtml = false) {\n"; // Sửa hàm này
file << "  var output = document.getElementById('output');\n";
file << "  var time = new Date().toLocaleTimeString();\n";
file << "  if (isHtml) {\n";
file << "    output.innerHTML += '[' + time + '] ' + text + '<br>';\n";
file << "  } else {\n";
// Sửa lỗi bảo mật (tránh chèn text không an toàn vào HTML)
file << "    var p = document.createElement('p');\n";
file << "    p.style.margin = 0;\n"; // Thêm style cho gọn
file << "    p.textContent = '[' + time + '] ' + text;\n";
file << "    output.appendChild(p);\n";
file << "  }\n";
file << "  output.scrollTop = output.scrollHeight;\n";
file << "}\n";

file << "function sendCmd(cmd, data) {\n"; // Thay thế toàn bộ hàm này
file << "  data = data || '';\n";
file << "  addOutput('Sending command ' + cmd + '...', false);\n";
file << "  fetch('/cmd', {\n";
file << "    method: 'POST',\n";
file << "    headers: {'Content-Type': 'application/json'},\n";
file << "    body: JSON.stringify({cmd: cmd, data: data})\n";
file << "  })\n";
file << "  .then(response => {\n";
file << "    if (!response.ok) {\n";
file << "      throw new Error('Network response was not ok');\n";
file << "    }\n";
// Kiểm tra xem command có phải là SCREENSHOT (số 6) không
file << "    if (cmd === 6) { // CMD_SCREENSHOT = 6\n"; 
file << "      addOutput('Received screenshot data (BMP).', false);\n";
file << "      return response.blob(); // Xử lý như file nhị phân (ảnh)\n";
file << "    } else {\n";
file << "      return response.text(); // XửLý như text (cho các lệnh khác)\n";
file << "    }\n";
file << "  })\n";
file << "  .then(result => {\n";
file << "    if (cmd === 6) {\n";
// Nếu là ảnh, tạo URL và hiển thị thẻ <img>
file << "      var imgUrl = URL.createObjectURL(result);\n";
file << "      var imgTag = '<img src=\"' + imgUrl + '\" style=\"max-width: 100%;\" alt=\"Screenshot\">';\n";
file << "      addOutput('Screenshot:', true);\n";
file << "      addOutput(imgTag, true);\n";
file << "    } else {\n";
// Nếu là text, hiển thị text
file << "      addOutput(result, false);\n";
file << "    }\n";
file << "  })\n";
file << "  .catch(err => {\n";
file << "    addOutput('Error: ' + err.message, false);\n";
file << "  });\n";
file << "}\n";
        file << "function stopApp() {\n";
        file << "  var name = document.getElementById('appName').value;\n";
        file << "  if(name) sendCmd(2, name);\n";
        file << "}\n";
        file << "function startApp() {\n";
        file << "  var name = document.getElementById('appName').value;\n";
        file << "  if(name) sendCmd(3, name);\n";
        file << "}\n";
        file << "function killProc() {\n";
        file << "  var pid = document.getElementById('pid').value;\n";
        file << "  if(pid) sendCmd(5, pid);\n";
        file << "}\n";
        file << "setInterval(function() {\n";
        file << "  fetch('/status').then(r => r.json()).then(r => {\n";
        file << "    document.getElementById('statusText').textContent = r.connected ? 'Connected' : 'Disconnected';\n";
        file << "  });\n";
        file << "}, 2000);\n";
        file << "</script>\n";
        file << "</body>\n";
        file << "</html>\n";
        file.close();
        
        std::cout << "HTML file created: index.html" << std::endl;
    }
    
    void handleHTTPClient(SOCKET_TYPE clientSocket) {
        char buffer[4096];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            std::string request(buffer);
            
            if (request.find("GET / ") == 0 || request.find("GET /index.html") == 0) {
                // Serve HTML file
                std::ifstream file("index.html");
                std::stringstream htmlContent;
                htmlContent << file.rdbuf();
                file.close();
                
                std::string html = htmlContent.str();
                std::string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: text/html\r\n";
                response += "Content-Length: " + std::to_string(html.length()) + "\r\n";
                response += "\r\n";
                response += html;
                
                send(clientSocket, response.c_str(), response.length(), 0);
                
            } else if (request.find("GET /status") == 0) {
                // Return connection status
                std::string json = remoteConnected ? 
                    "{\"connected\": true}" : "{\"connected\": false}";
                
                std::string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
                response += "\r\n";
                response += json;
                
                send(clientSocket, response.c_str(), response.length(), 0);
                
            } else if (request.find("POST /cmd") == 0) {
                // Handle command
                size_t bodyStart = request.find("\r\n\r\n");
                if (bodyStart != std::string::npos) {
                    std::string body = request.substr(bodyStart + 4);
                    
                    // Simple JSON parsing
                    int command = 0;
                    std::string data = "";
                    
                    size_t cmdPos = body.find("\"cmd\":");
                    if (cmdPos != std::string::npos) {
                        cmdPos += 6;
                        // Skip whitespace
                        while (body[cmdPos] == ' ') cmdPos++;
                        command = std::stoi(body.substr(cmdPos));
                    }
                    
                    size_t dataPos = body.find("\"data\":\"");
                    if (dataPos != std::string::npos) {
                        dataPos += 8;
                        size_t dataEnd = body.find("\"", dataPos);
                        if (dataEnd != std::string::npos) {
                            data = body.substr(dataPos, dataEnd - dataPos);
                        }
                    }
                    
                    // Send command to remote server
                    std::string result = executeRemoteCommand(command, data);
                    
                    std::string response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: text/plain\r\n";
                    response += "Access-Control-Allow-Origin: *\r\n";
                    response += "Content-Length: " + std::to_string(result.length()) + "\r\n";
                    response += "\r\n";
                    response += result;
                    
                    send(clientSocket, response.c_str(), response.length(), 0);
                }
            } else if (request.find("OPTIONS") == 0) {
                // Handle CORS preflight
                std::string response = "HTTP/1.1 200 OK\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
                response += "Access-Control-Allow-Headers: Content-Type\r\n";
                response += "\r\n";
                
                send(clientSocket, response.c_str(), response.length(), 0);
            }
        }
        
        closesocket(clientSocket);
    }
    
    std::string executeRemoteCommand(int command, const std::string& data) {
        if (!remoteConnected) {
            // Try to reconnect
            if (!connectToRemote(remoteIP)) {
                return "Not connected to remote server";
            }
        }
        
        Packet packet;
        packet.command = command;
        strcpy(packet.data, data.c_str());
        packet.dataSize = data.length();
        
        if (!sendPacket(remoteSocket, packet)) {
            remoteConnected = false;
            return "Failed to send command";
        }
        
        Packet response;
        if (!receivePacket(remoteSocket, response)) {
            remoteConnected = false;
            return "Failed to receive response";
        }
        
        return std::string(response.data, response.dataSize);
    }
    
    void stop() {
        if (remoteConnected) {
            Packet packet;
            packet.command = CMD_EXIT;
            packet.dataSize = 0;
            sendPacket(remoteSocket, packet);
            closesocket(remoteSocket);
        }
        
        if (httpSocket != INVALID_SOCKET_VALUE) {
            closesocket(httpSocket);
        }
    }
};

int main() {
    std::cout << "===================================" << std::endl;
    std::cout << "   Web-based Remote Control v1.0  " << std::endl;
    std::cout << "===================================" << std::endl;
    
    WebClient client;
    
    if (!client.startWebServer()) {
        std::cerr << "Failed to start web server!" << std::endl;
        return 1;
    }
    
    std::cout << "Open your browser and navigate to http://localhost:8080" << std::endl;
    std::cout << "Or open the file: index.html" << std::endl;
    
    try {
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    
    return 0;
}