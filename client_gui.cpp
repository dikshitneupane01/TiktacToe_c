
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>

#pragma comment(lib, "ws2_32.lib")

#define PORT 54000
#define BOARD_SIZE 9
#define WM_APP_SYMBOL     (WM_APP + 1)
#define WM_APP_BOARD      (WM_APP + 2)
#define WM_APP_YOURTURN   (WM_APP + 3)
#define WM_APP_WAIT       (WM_APP + 4)
#define WM_APP_INVALID    (WM_APP + 5)
#define WM_APP_RESULT     (WM_APP + 6)
#define WM_APP_RESTART    (WM_APP + 7)
#define WM_APP_EXIT       (WM_APP + 8)
#define WM_APP_DISCONNECT (WM_APP + 9)

#define ID_CELL_BASE 100 
#define ID_CONNECT   200
#define ID_IP_EDIT   201

HWND g_hCells[BOARD_SIZE];
HWND g_hStatus;
HWND g_hIpEdit;
HWND g_hConnectBtn;
HWND g_hMainWnd;

SOCKET g_sock = INVALID_SOCKET;
char g_mySymbol = 0;
std::string g_board(BOARD_SIZE, '_');
bool g_myTurn = false;


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

DWORD WINAPI networkThreadProc(LPVOID param) {
    HWND hwnd = (HWND)param;
    std::string line;
    while (true) {
        if (!recvLine(g_sock, line)) {
            PostMessage(hwnd, WM_APP_DISCONNECT, 0, 0);
            return 0;
        }

        if (line.rfind("SYMBOL:", 0) == 0) {
            std::string* data = new std::string(line.substr(7));
            PostMessage(hwnd, WM_APP_SYMBOL, 0, (LPARAM)data);
        }
        else if (line.rfind("BOARD:", 0) == 0) {
            std::string* data = new std::string(line.substr(6));
            PostMessage(hwnd, WM_APP_BOARD, 0, (LPARAM)data);
        }
        else if (line == "YOURTURN") {
            PostMessage(hwnd, WM_APP_YOURTURN, 0, 0);
        }
        else if (line == "WAIT") {
            PostMessage(hwnd, WM_APP_WAIT, 0, 0);
        }
        else if (line == "INVALID") {
            PostMessage(hwnd, WM_APP_INVALID, 0, 0);
        }
        else if (line.rfind("RESULT:", 0) == 0) {
            std::string* data = new std::string(line.substr(7));
            PostMessage(hwnd, WM_APP_RESULT, 0, (LPARAM)data);
        }
        else if (line == "RESTART?") {
            PostMessage(hwnd, WM_APP_RESTART, 0, 0);
        }
        else if (line == "EXIT") {
            PostMessage(hwnd, WM_APP_EXIT, 0, 0);
            return 0;
        }
    }
    return 0;
}

void refreshCells() {
    for (int i = 0; i < BOARD_SIZE; i++) {
        char c = g_board[i];
        const char* txt = (c == 'X') ? "X" : (c == 'O') ? "O" : "";
        SetWindowTextA(g_hCells[i], txt);
        EnableWindow(g_hCells[i], g_myTurn && c == '_');
    }
}

void setStatus(const std::string& s) {
    SetWindowTextA(g_hStatus, s.c_str());
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateWindowA("STATIC", "Server IP:", WS_CHILD | WS_VISIBLE,
            10, 10, 60, 20, hwnd, NULL, NULL, NULL);
        g_hIpEdit = CreateWindowA("EDIT", "127.0.0.1", WS_CHILD | WS_VISIBLE | WS_BORDER,
            75, 8, 120, 20, hwnd, (HMENU)ID_IP_EDIT, NULL, NULL);
        g_hConnectBtn = CreateWindowA("BUTTON", "Connect", WS_CHILD | WS_VISIBLE,
            200, 8, 80, 22, hwnd, (HMENU)ID_CONNECT, NULL, NULL);

        g_hStatus = CreateWindowA("STATIC", "Not connected.", WS_CHILD | WS_VISIBLE,
            10, 40, 270, 20, hwnd, NULL, NULL, NULL);

        int startX = 10, startY = 70, size = 80, gap = 5;
        for (int i = 0; i < BOARD_SIZE; i++) {
            int row = i / 3, col = i % 3;
            g_hCells[i] = CreateWindowA("BUTTON", "", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                startX + col * (size + gap), startY + row * (size + gap), size, size,
                hwnd, (HMENU)(ID_CELL_BASE + i), NULL, NULL);
            EnableWindow(g_hCells[i], FALSE);
        }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);

        if (id == ID_CONNECT) {
            char ipBuf[64];
            GetWindowTextA(g_hIpEdit, ipBuf, sizeof(ipBuf));

            g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (g_sock == INVALID_SOCKET) {
                setStatus("Failed to create socket.");
                return 0;
            }

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_port = htons(PORT);
            serverAddr.sin_addr.s_addr = inet_addr(ipBuf);
            if (serverAddr.sin_addr.s_addr == INADDR_NONE) {
                setStatus("Invalid IP address.");
                return 0;
            }

            if (connect(g_sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                setStatus("Connection failed.");
                closesocket(g_sock);
                g_sock = INVALID_SOCKET;
                return 0;
            }

            setStatus("Connected. Waiting for opponent...");
            EnableWindow(g_hConnectBtn, FALSE);
            EnableWindow(g_hIpEdit, FALSE);

            CreateThread(NULL, 0, networkThreadProc, hwnd, 0, NULL);
            return 0;
        }

        if (id >= ID_CELL_BASE && id < ID_CELL_BASE + BOARD_SIZE) {
            int idx = id - ID_CELL_BASE;
            if (g_myTurn && g_board[idx] == '_') {
                sendLine(g_sock, "MOVE:" + std::to_string(idx));
                g_myTurn = false;
                refreshCells();
            }
            return 0;
        }
        break;
    }

    case WM_APP_SYMBOL: {
        std::string* data = (std::string*)lParam;
        g_mySymbol = (*data)[0];
        setStatus(std::string("You are playing as: ") + g_mySymbol);
        delete data;
        return 0;
    }

    case WM_APP_BOARD: {
        std::string* data = (std::string*)lParam;
        g_board = *data;
        delete data;
        g_myTurn = false;
        refreshCells();
        return 0;
    }

    case WM_APP_YOURTURN: {
        g_myTurn = true;
        setStatus(std::string("Your turn (") + g_mySymbol + ")");
        refreshCells();
        return 0;
    }

    case WM_APP_WAIT: {
        g_myTurn = false;
        setStatus("Waiting for opponent's move...");
        refreshCells();
        return 0;
    }

    case WM_APP_INVALID: {
        MessageBoxA(hwnd, "That move is invalid. Try a different cell.", "Invalid Move", MB_OK | MB_ICONWARNING);
        return 0;
    }

    case WM_APP_RESULT: {
        std::string* data = (std::string*)lParam;
        std::string result = *data;
        delete data;
        g_myTurn = false;
        refreshCells();

        std::string text = (result == "WIN") ? "You WIN!" :
                            (result == "LOSE") ? "You LOSE." : "It's a DRAW.";
        MessageBoxA(hwnd, text.c_str(), "Game Over", MB_OK | MB_ICONINFORMATION);
        return 0;
    }

    case WM_APP_RESTART: {
        int choice = MessageBoxA(hwnd, "Play again?", "Restart", MB_YESNO | MB_ICONQUESTION);
        sendLine(g_sock, (choice == IDYES) ? "Y" : "N");
        if (choice != IDYES) setStatus("Game ended.");
        return 0;
    }

    case WM_APP_EXIT: {
        setStatus("Game ended. You can close the window.");
        for (int i = 0; i < BOARD_SIZE; i++) EnableWindow(g_hCells[i], FALSE);
        return 0;
    }

    case WM_APP_DISCONNECT: {
        setStatus("Disconnected from server.");
        for (int i = 0; i < BOARD_SIZE; i++) EnableWindow(g_hCells[i], FALSE);
        return 0;
    }

    case WM_DESTROY:
        if (g_sock != INVALID_SOCKET) closesocket(g_sock);
        WSACleanup();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    const char* className = "TicTacToeGuiClient";
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassA(&wc);

    g_hMainWnd = CreateWindowA(className, "Tic-Tac-Toe", WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 305, 340, NULL, NULL, hInstance, NULL);

    ShowWindow(g_hMainWnd, nCmdShow);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}