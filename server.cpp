// server.cpp
// Multiplayer Tic-Tac-Toe Server over TCP using Winsock2
// Build in Visual Studio: create a Console App project, add this file,
// it will auto-link ws2_32.lib via the #pragma comment below.

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600   // Vista or later
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

#define PORT 54000
#define BOARD_SIZE 9

// ---------- Networking helpers ----------

// Sends a string terminated with '\n'
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

// Reads a single '\n'-terminated line (blocking). Returns false on disconnect/error.
bool recvLine(SOCKET s, std::string& outLine) {
    outLine.clear();
    char c;
    while (true) {
        int r = recv(s, &c, 1, 0);
        if (r <= 0) return false;       // connection closed or error
        if (c == '\n') break;
        if (c != '\r') outLine += c;
    }
    return true;
}

// ---------- Game logic ----------

// Board cells are '_' (empty), 'X' or 'O'
std::string boardToString(const char board[BOARD_SIZE]) {
    return std::string(board, BOARD_SIZE);
}

void resetBoard(char board[BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) board[i] = '_';
}

// Returns 'X', 'O' if that symbol has won, or 0 if no winner yet
char checkWinner(const char board[BOARD_SIZE]) {
    static const int lines[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},   // rows
        {0,3,6}, {1,4,7}, {2,5,8},   // columns
        {0,4,8}, {2,4,6}             // diagonals
    };
    for (auto& line : lines) {
        char a = board[line[0]], b = board[line[1]], c = board[line[2]];
        if (a != '_' && a == b && b == c) return a;
    }
    return 0;
}

bool isBoardFull(const char board[BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++)
        if (board[i] == '_') return false;
    return true;
}

// ---------- Main server ----------

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "bind() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, 2) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server started. Waiting for 2 players on port " << PORT << "...\n";

    SOCKET clientX = accept(listenSocket, nullptr, nullptr);
    if (clientX == INVALID_SOCKET) { std::cerr << "accept() failed.\n"; return 1; }
    std::cout << "Player X connected.\n";

    SOCKET clientO = accept(listenSocket, nullptr, nullptr);
    if (clientO == INVALID_SOCKET) { std::cerr << "accept() failed.\n"; return 1; }
    std::cout << "Player O connected.\n";

    sendLine(clientX, "SYMBOL:X");
    sendLine(clientO, "SYMBOL:O");

    char board[BOARD_SIZE];
    bool keepPlaying = true;

    while (keepPlaying) {
        resetBoard(board);
        SOCKET current = clientX, other = clientO;
        char currentSymbol = 'X';

        sendLine(clientX, "BOARD:" + boardToString(board));
        sendLine(clientO, "BOARD:" + boardToString(board));

        bool gameOver = false;
        while (!gameOver) {
            sendLine(current, "YOURTURN");
            sendLine(other, "WAIT");

            std::string line;
            if (!recvLine(current, line)) { gameOver = true; keepPlaying = false; break; }

            // Expect "MOVE:<0-8>"
            int pos = -1;
            if (line.rfind("MOVE:", 0) == 0) {
                try { pos = std::stoi(line.substr(5)); }
                catch (...) { pos = -1; }
            }

            if (pos < 0 || pos >= BOARD_SIZE || board[pos] != '_') {
                sendLine(current, "INVALID");
                continue; // ask the same player again, don't switch turn
            }

            board[pos] = currentSymbol;
            std::string boardMsg = "BOARD:" + boardToString(board);
            sendLine(clientX, boardMsg);
            sendLine(clientO, boardMsg);

            char winner = checkWinner(board);
            if (winner) {
                sendLine(current, "RESULT:WIN");
                sendLine(other, "RESULT:LOSE");
                gameOver = true;
            } else if (isBoardFull(board)) {
                sendLine(clientX, "RESULT:DRAW");
                sendLine(clientO, "RESULT:DRAW");
                gameOver = true;
            } else {
                // switch turn
                std::swap(current, other);
                currentSymbol = (currentSymbol == 'X') ? 'O' : 'X';
            }
        }

        if (!keepPlaying) break;

        // Ask both players if they want to restart
        sendLine(clientX, "RESTART?");
        sendLine(clientO, "RESTART?");

        std::string respX, respO;
        bool okX = recvLine(clientX, respX);
        bool okO = recvLine(clientO, respO);

        if (!okX || !okO || respX != "Y" || respO != "Y") {
            sendLine(clientX, "EXIT");
            sendLine(clientO, "EXIT");
            keepPlaying = false;
        }
        // else loop again with a fresh board
    }

    std::cout << "Game session ended. Closing connections.\n";
    closesocket(clientX);
    closesocket(clientO);
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}