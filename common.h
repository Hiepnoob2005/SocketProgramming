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

#ifdef _WIN32
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

// Send packet function
inline bool sendPacket(SOCKET_TYPE socket, const Packet& packet) {
    // Send packet header (command and size)
    int header[2] = {packet.command, packet.dataSize};
    if (send(socket, (char*)header, sizeof(header), 0) == -1) {
        return false;
    }
    
    // Send data if exists
    if (packet.dataSize > 0) {
        if (send(socket, packet.data, packet.dataSize, 0) == -1) {
            return false;
        }
    }
    
    return true;
}

// Receive packet function
inline bool receivePacket(SOCKET_TYPE socket, Packet& packet) {
    // Receive header
    int header[2];
    int bytesReceived = recv(socket, (char*)header, sizeof(header), 0);
    if (bytesReceived <= 0) {
        return false;
    }
    
    packet.command = header[0];
    packet.dataSize = header[1];
    
    // Receive data if exists
    if (packet.dataSize > 0) {
        bytesReceived = recv(socket, packet.data, packet.dataSize, 0);
        if (bytesReceived <= 0) {
            return false;
        }
    }
    
    return true;
}

#endif // COMMON_H