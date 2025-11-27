#ifndef COMMON_H
#define COMMON_H

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>  // Cho std::min

#ifdef _WIN32
    #ifndef NOMINMAX
    #define NOMINMAX          // Ngăn Windows định nghĩa min/max macro
    #endif 
    #include <winsock2.h>
    #undef UNICODE
    #include <windows.h>
    #include <ws2tcpip.h>
    #include <tlhelp32.h>
    #include <psapi.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "psapi.lib")
    #define SOCKET_TYPE SOCKET
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <signal.h>
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #define SOCKET_TYPE int
    #define INVALID_SOCKET_VALUE -1
    #define closesocket close
#endif

#define PORT 8888
#define BUFFER_SIZE 65536
#define MAX_CLIENTS 10

// Command codes
enum Commands {
    CMD_LIST_APPS = 1,
    CMD_STOP_APP = 2,
    CMD_START_APP = 3,
    CMD_LIST_PROCESS = 4,
    CMD_KILL_PROCESS = 5,
    CMD_SCREENSHOT = 6,
    CMD_KEYLOG_START = 7,
    CMD_KEYLOG_STOP = 8,
    CMD_KEYLOG_GET = 9,
    CMD_WEBCAM_ON = 10,
    CMD_WEBCAM_OFF = 11,
    CMD_SHUTDOWN = 12,
    CMD_RESTART = 13,
    CMD_WEBCAM_CAPTURE = 14,  // NEW: Capture webcam for N seconds
    CMD_EXIT = 99
};

// Packet structure for communication
struct Packet {
    int command;
    int dataSize;
    char data[BUFFER_SIZE];
    
    Packet() : command(0), dataSize(0) {
        memset(data, 0, BUFFER_SIZE);
    }
};

// Process info structure
struct ProcessInfo {
    int pid;
    std::string name;
    long memoryUsage;
};

// Application info structure
struct AppInfo {
    std::string name;
    std::string path;
    bool isRunning;
};

// Utility functions
inline void clearBuffer(char* buffer, int size) {
    memset(buffer, 0, size);
}

inline std::string getCurrentTime() {
    time_t now = time(0);
    char* dt = ctime(&now);
    std::string timeStr(dt);
    if (!timeStr.empty() && timeStr.back() == '\n') {
        timeStr.pop_back();
    }
    return timeStr;
}

#ifdef _WIN32
inline void initWinsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed!" << std::endl;
        exit(1);
    }
}

inline void cleanupWinsock() {
    WSACleanup();
}
#else
inline void initWinsock() {}
inline void cleanupWinsock() {}
#endif

// Send packet function - HỖ TRỢ GỬI DỮ LIỆU LỚN
inline bool sendPacket(SOCKET_TYPE socket, const Packet& packet) {
    // Send packet header (command and size)
    int header[2] = {packet.command, packet.dataSize};
    if (send(socket, (char*)header, sizeof(header), 0) == -1) {
        return false;
    }
    
    // Send data if exists - GỬI TỪNG PHẦN NẾU DỮ LIỆU LỚN
    if (packet.dataSize > 0) {
        int totalSent = 0;
        while (totalSent < packet.dataSize) {
            int chunkSize = std::min(4096, packet.dataSize - totalSent);
            int sent = send(socket, packet.data + totalSent, chunkSize, 0);
            if (sent == -1) {
                return false;
            }
            totalSent += sent;
        }
    }
    
    return true;
}

// Receive packet function - HỖ TRỢ NHẬN DỮ LIỆU LỚN
inline bool receivePacket(SOCKET_TYPE socket, Packet& packet) {
    // Receive header
    int header[2];
    int bytesReceived = recv(socket, (char*)header, sizeof(header), 0);
    if (bytesReceived <= 0) {
        return false;
    }
    
    packet.command = header[0];
    packet.dataSize = header[1];
    
    // Receive data if exists - NHẬN TỪNG PHẦN NẾU DỮ LIỆU LỚN
    if (packet.dataSize > 0) {
        int totalReceived = 0;
        while (totalReceived < packet.dataSize) {
            int chunkSize = std::min(4096, packet.dataSize - totalReceived);
            bytesReceived = recv(socket, packet.data + totalReceived, chunkSize, 0);
            if (bytesReceived <= 0) {
                return false;
            }
            totalReceived += bytesReceived;
        }
    }
    
    return true;
}

// ============ HÀM GỬI FILE LỚN (VIDEO) ============
// Gửi file lớn qua socket (chia thành nhiều packet)
inline bool sendLargeFile(SOCKET_TYPE socket, int command, const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Gửi header: command + total file size
    int header[2] = {command, (int)fileSize};
    if (send(socket, (char*)header, sizeof(header), 0) == -1) {
        file.close();
        return false;
    }
    
    // Gửi data theo chunks
    char buffer[8192];
    int totalSent = 0;
    while (file.read(buffer, sizeof(buffer)) || file.gcount() > 0) {
        int bytesToSend = (int)file.gcount();
        int sent = send(socket, buffer, bytesToSend, 0);
        if (sent == -1) {
            file.close();
            return false;
        }
        totalSent += sent;
    }
    
    file.close();
    std::cout << "[Server] Sent file: " << filePath << " (" << totalSent << " bytes)" << std::endl;
    return true;
}

#endif // COMMON_H