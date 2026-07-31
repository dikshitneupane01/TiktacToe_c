//   g++ server_mt.cpp -o server_mt.exe -lws2_32

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define PORT 54000
#define BOARD_SIZE 9

CRITICAL_SECTION g_consoleCS;

void logMsg(const std::string& msg) {
    EnterCriticalSection(&g_consoleCS);
    std::cout << msg << std::endl;
    LeaveCriticalSection(&g_consoleCS);
}

bool sendLine(SOCKET s, const std::string& msg) {
    std::string out = msg + "\n";
    int total = 0, len = (int)out.size();
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

std::string boardToString(const char board[BOARD_SIZE]) {
    return std::string(board, BOARD_SIZE);
}

void resetBoard(char board[BOARD_SIZE]) {
    for (int i = 0; i < BOARD_SIZE; i++) board[i] = '_';
}

char checkWinner(const char board[BOARD_SIZE]) {
    static const int lines[8][3] = {
        {0,1,2}, {3,4,5}, {6,7,8},
        {0,3,6}, {1,4,7}, {2,5,8},
        {0,4,8}, {2,4,6}
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


struct GameThreadParams {
    SOCKET clientX;
    SOCKET clientO;
    int gameId;
};

DWORD WINAPI gameThreadProc(LPVOID param) {
    GameThreadParams* p = (GameThreadParams*)param;
    SOCKET clientX = p->clientX;
    SOCKET clientO = p->clientO;
    int gameId = p->gameId;
    delete p;

    logMsg("[Game " + std::to_string(gameId) + "] Started. Player X and Player O connected.");

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

            int pos = -1;
            if (line.rfind("MOVE:", 0) == 0) {
                try { pos = std::stoi(line.substr(5)); }
                catch (...) { pos = -1; }
            }

            if (pos < 0 || pos >= BOARD_SIZE || board[pos] != '_') {
                sendLine(current, "INVALID");
                continue;
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
                logMsg("[Game " + std::to_string(gameId) + "] Player " + std::string(1, currentSymbol) + " won.");
            } else if (isBoardFull(board)) {
                sendLine(clientX, "RESULT:DRAW");
                sendLine(clientO, "RESULT:DRAW");
                gameOver = true;
                logMsg("[Game " + std::to_string(gameId) + "] Draw.");
            } else {
                std::swap(current, other);
                currentSymbol = (currentSymbol == 'X') ? 'O' : 'X';
            }
        }

        if (!keepPlaying) break;

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
    }

    logMsg("[Game " + std::to_string(gameId) + "] Ended. Closing connections.");
    closesocket(clientX);
    closesocket(clientO);
    return 0;
}

int main() {
    InitializeCriticalSection(&g_consoleCS);

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

    if (listen(listenSocket, 8) == SOCKET_ERROR) {
        std::cerr << "listen() failed: " << WSAGetLastError() << "\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Multithreaded server started on port " << PORT << ".\n";
    std::cout << "Waiting for players (2 per match, unlimited concurrent matches)...\n";

    int gameCounter = 0;

    while (true) {
        SOCKET clientX = accept(listenSocket, nullptr, nullptr);
        if (clientX == INVALID_SOCKET) {
            std::cerr << "accept() failed: " << WSAGetLastError() << "\n";
            continue;
        }
        logMsg("Player connected, waiting for an opponent to pair with...");

        SOCKET clientO = accept(listenSocket, nullptr, nullptr);
        if (clientO == INVALID_SOCKET) {
            std::cerr << "accept() failed: " << WSAGetLastError() << "\n";
            closesocket(clientX);
            continue;
        }

        gameCounter++;
        GameThreadParams* params = new GameThreadParams{ clientX, clientO, gameCounter };

        HANDLE hThread = CreateThread(NULL, 0, gameThreadProc, params, 0, NULL);
        if (hThread == NULL) {
            std::cerr << "CreateThread failed for game " << gameCounter << "\n";
            delete params;
            closesocket(clientX);
            closesocket(clientO);
            continue;
        }
        CloseHandle(hThread); 
    }

    closesocket(listenSocket);
    DeleteCriticalSection(&g_consoleCS);
    WSACleanup();
    return 0;
}
