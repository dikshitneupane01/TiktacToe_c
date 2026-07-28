# Multiplayer Tic-Tac-Toe over TCP (Winsock2, C++)

Matches your proposal: client/server architecture, TCP sockets, Winsock API, C++, tested on a local network.

## Files
- `server.cpp` — accepts 2 players, manages board state, turns, win/draw detection, restart flow.
- `client.cpp` — console client: connects to the server, shows the board, sends moves on your turn.
- `client_gui.cpp` — Win32 GUI client: same protocol, but with a clickable 3x3 button grid instead of console prompts.

## How to build

### Option A: VS Code (simplest for this project)
1. Install [MinGW-w64](https://www.mingw-w64.org/) (via [MSYS2](https://www.msys2.org/) is easiest) so you have `g++` on your PATH. Confirm with `g++ --version` in a terminal.
2. Install the **C/C++** extension in VS Code (from Microsoft).
3. Open the folder containing `server.cpp` and `client.cpp` in VS Code.
4. Open a terminal in VS Code (`` Ctrl+` ``) and compile each file directly — no project files needed:
   ```
   g++ server.cpp -o server.exe -lws2_32
   g++ client.cpp -o client.exe -lws2_32
   ```
   The `-lws2_32` flag links the Winsock library (the `#pragma comment` in the code is a Visual-C++-only trick, so with g++ you link it manually via this flag instead).
5. Run them from the terminal:
   ```
   ./server.exe
   ./client.exe
   ```
   (Run `client.exe` in two separate terminals to simulate two players.)

Optional: add a `tasks.json` in `.vscode/` if you want to build with Ctrl+Shift+B instead of typing the g++ commands each time — happy to generate that for you if you want it.

### Building the GUI client
`client_gui.cpp` is a self-contained Win32 GUI (no extra libraries needed beyond Winsock). Compile it with:
```
g++ client_gui.cpp -o client_gui.exe -lws2_32 -mwindows
```
The `-mwindows` flag stops a console window from popping up behind the GUI. Run `server.exe` first, then launch `client_gui.exe` twice (or once alongside `client.exe` — they speak the same protocol, so a GUI player and a console player can play each other).

In the GUI: type the server IP in the box at the top, click **Connect**, then click cells on the grid on your turn. Results and restart prompts appear as message boxes. A persistent label at the top always shows your identity (**Player 1 (X)** or **Player 2 (O)**), and the end-of-game popup names the actual winner (e.g. "Player 1 (X) wins!").

### Playing across two devices (LAN)
1. Pick one PC to be the **server** (run `server.exe` there). Both PCs must be on the same network (same Wi-Fi/router).
2. On the server PC, find its local IP: open Command Prompt and run `ipconfig`, then look for **IPv4 Address** (e.g. `192.168.1.23`).
3. On the server PC, allow the app through the firewall if prompted (Windows Defender Firewall → **Allow an app through firewall** → enable `server.exe` for Private networks). If you skip this, the second device usually can't connect.
4. On the **server PC**, run `client_gui.exe`, enter `127.0.0.1` as the IP, click Connect → this becomes Player 1.
5. On the **other PC**, run `client_gui.exe`, enter the server PC's IP from step 2 (e.g. `192.168.1.23`), click Connect → this becomes Player 2.
6. Play — moves and results sync over the network in real time.

### Option B: Visual Studio
1. Open Visual Studio → **Create a new project** → **Console App (C++)**. Name it `TicTacToeServer`.
2. Replace the auto-generated `.cpp` file's contents with `server.cpp`.
3. Build (Ctrl+Shift+B). It links `ws2_32.lib` automatically via the `#pragma comment` in the code — no manual linker config needed.
4. Repeat steps 1–3 in a **second** project called `TicTacToeClient`, using `client.cpp` instead.

(You can also put both in one solution as two separate project files if you prefer a single `.sln`.)

## How to run
1. Run `TicTacToeServer.exe` first. It prints `Waiting for 2 players on port 54000...`
2. Run `TicTacToeClient.exe` twice (two terminals, or two machines on the same LAN).
   - Each time it asks for the server IP:
     - Same machine testing: enter `127.0.0.1`
     - Different machines on the same LAN: enter the server machine's local IP (find it with `ipconfig` on the server PC, e.g. `192.168.1.23`)
3. First client to connect is `X`, second is `O`. Server tells each client its symbol.
4. Board cells are numbered 0–8:
```
 0 | 1 | 2
---+---+---
 3 | 4 | 5
---+---+---
 6 | 7 | 8
```
5. Players alternate turns; the server validates every move and broadcasts the updated board to both clients.
6. On win/draw, both players are asked `Play again? (Y/N)`. If both say Y, the board resets and play continues; otherwise the connection closes.

## Notes for your report / demo
- Protocol is a simple newline-terminated text protocol (`SYMBOL:X`, `BOARD:_________`, `YOURTURN`, `WAIT`, `MOVE:4`, `INVALID`, `RESULT:WIN`, `RESTART?`, `EXIT`) — easy to describe and diagram in your methodology section.
- The server is authoritative: it owns the only copy of the board and enforces turn order and move legality, which satisfies your "validity of move" and "real-time synchronization" objectives.
- If Windows Firewall blocks connections during LAN testing, allow the server app through **Windows Defender Firewall → Allow an app**, or temporarily test on the same machine with `127.0.0.1`.
- To change the port, edit `#define PORT 54000` in both files (must match).

## Suggested next steps to extend the project
- Add a simple GUI (Win32/MFC or a lightweight framework) instead of console I/O.
- Add a lobby/matchmaking system to support more than one game session at once (currently the server handles a single game per run — you'd fork a thread per pair of clients).
- Add basic authentication or player names for a more polished demo.
