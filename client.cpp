// client.cpp
// Multiplayer Tic-Tac-Toe Client over TCP using Winsock2
// Build in Visual Studio: create a Console App project, add this file,
// it will auto-link ws2_32.lib via the #pragma comment below.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600   // Vista or later, needed for inet_pton under MinGW
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define PORT 54000
#define BOARD_SIZE 9

bool sendLine(SOCKET s, const std::string& msg) {
    std::string out = msg + "\n";
    int total = 0;
    int len = (int)out.size();
    while (total < len) {
        int sent = send(s, out.c_str() + total, len - total, 0);
        if (sent == SOCKET_ERROR) return false;
        total += sent;
    }
    return true;
}

bool recvLine(SOCKET s, std::string& outLine) {
    outLine.clear();
    char c;
    while (true) {
        int r = recv(s, &c, 1, 0);
        if (r <= 0) return false;
        if (c == '\n') break;
        if (c != '\r') outLine += c;
    }
    return true;
}

void printBoard(const std::string& b) {
    // b is 9 chars: '_' 'X' 'O'
    auto cell = [&](int i) { return b[i] == '_' ? std::to_string(i) : std::string(1, b[i]); };
    std::cout << "\n";
    std::cout << " " << cell(0) << " | " << cell(1) << " | " << cell(2) << "\n";
    std::cout << "---+---+---\n";
    std::cout << " " << cell(3) << " | " << cell(4) << " | " << cell(5) << "\n";
    std::cout << "---+---+---\n";
    std::cout << " " << cell(6) << " | " << cell(7) << " | " << cell(8) << "\n\n";
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    std::string serverIp;
    std::cout << "Enter server IP (e.g. 127.0.0.1 for same machine): ";
    std::cin >> serverIp;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    serverAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
        std::cerr << "Invalid IP address entered.\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << "\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server. Waiting for game to start...\n";

    char mySymbol = 0;
    std::string line;
    std::string currentBoard(BOARD_SIZE, '_');
    bool playing = true;

    while (playing) {
        if (!recvLine(sock, line)) {
            std::cout << "Disconnected from server.\n";
            break;
        }

        if (line.rfind("SYMBOL:", 0) == 0) {
            mySymbol = line[7];
            std::cout << "You are playing as: " << mySymbol << "\n";
        }
        else if (line.rfind("BOARD:", 0) == 0) {
            currentBoard = line.substr(6);
            printBoard(currentBoard);
        }
        else if (line == "YOURTURN") {
            int pos = -1;
            while (true) {
                std::cout << "Your turn (" << mySymbol << "). Enter cell number (0-8): ";
                std::cin >> pos;
                if (std::cin.fail()) {
                    std::cin.clear();
                    std::cin.ignore(10000, '\n');
                    continue;
                }
                break;
            }
            sendLine(sock, "MOVE:" + std::to_string(pos));
        }
        else if (line == "WAIT") {
            std::cout << "Waiting for opponent's move...\n";
        }
        else if (line == "INVALID") {
            std::cout << "Invalid move, try again.\n";
        }
        else if (line == "RESULT:WIN") {
            std::cout << "*** You WIN! ***\n";
        }
        else if (line == "RESULT:LOSE") {
            std::cout << "*** You LOSE. ***\n";
        }
        else if (line == "RESULT:DRAW") {
            std::cout << "*** It's a DRAW. ***\n";
        }
        else if (line == "RESTART?") {
            char choice;
            std::cout << "Play again? (Y/N): ";
            std::cin >> choice;
            std::string resp(1, (char)toupper(choice));
            sendLine(sock, resp);
            if (resp != "Y") playing = false;
        }
        else if (line == "EXIT") {
            std::cout << "Game ended. Goodbye!\n";
            playing = false;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}