# ntchess

A self-contained chess application built with **Qt 6 / QML** and a native C++ chess engine backend. Supports human-vs-human, human-vs-engine, and engine-vs-engine games, board/piece theme selection, game import via PGN, and an analysis view powered by Stockfish.

## Features

- Full legal-move generation and game logic in C++
- UCI engine integration (Stockfish bundled for Linux and Windows)
- Human vs Human, Human vs Engine, and Computer vs Computer modes
- Clocked games with configurable time controls
- PGN import and game analysis view with eval bar
- 30+ board themes and 40+ piece sets
- Persistent settings (themes, engine path, etc.) stored in `settings.ini` beside the executable

## Requirements

| Dependency | Version |
|---|---|
| Qt | 6.10 or newer |
| CMake | 3.23 or newer |
| C++ compiler | C++17 (GCC / Clang on Linux, MinGW-w64 on Windows) |
| Ninja | any recent version |

Qt modules required: `Qt6::Quick`, `Qt6::Multimedia`, `Qt6::Concurrent`.

---

## Building on Linux

### 1. Install dependencies

**Ubuntu / Debian:**
```bash
sudo apt install cmake ninja-build
# Install Qt 6.10+ via the Qt Online Installer: https://www.qt.io/download
```

### 2. Configure and build (Debug)

```bash
cmake --preset linux-debug
cmake --build --preset linux-debug
```

For a release build:
```bash
cmake --preset linux-release
cmake --build --preset linux-release
```

### 3. Run

```bash
./build/linux-debug/appntchess      # debug
./build/linux-release/appntchess    # release
```

If Qt is not on your system path, set `CMAKE_PREFIX_PATH` to your Qt kit root before configuring:
```bash
export CMAKE_PREFIX_PATH=/opt/Qt/6.10.0/gcc_64
cmake --preset linux-debug
```

---

## Building on Windows

### Prerequisites

- [Qt Online Installer](https://www.qt.io/download) – install the **MinGW 64-bit** kit for Qt 6.10+
- CMake available on `PATH` (the Qt Creator bundle includes one)

### Using the helper script

```bat
:: Debug build (default)
build-windows.bat

:: Release build
build-windows.bat release
```

The script auto-detects Qt under `C:\Qt\`. If Qt is installed elsewhere, set the environment variable first:
```bat
set QT_ROOT_DIR=C:\Qt\6.10.0\mingw_64
build-windows.bat release
```

The executable and all required Qt DLLs are placed in:
```
build\windows-mingw-debug\appntchess.exe
build\windows-mingw-release\appntchess.exe
```

### Manual CMake build on Windows

```bat
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
```

---

## Installing

```bash
cmake --install build/linux-release --prefix /path/to/install
```

This copies the executable and the `assets/` folder (board themes, piece sets, sounds, Stockfish binary) to the install prefix. The application is fully self-contained — no system-wide data directories are required.

---

## Project Structure

```
ntchess/
├── src/
│   ├── main.cpp               # Application entry point
│   ├── backend/               # Chess engine: board, move generation, UCI
│   └── bridge/                # Qt/QML bridge (GameBridge)
├── qml/                       # QML UI (Main, ChessBoard, dialogs)
├── assets/
│   ├── board/                 # Board themes (SVG)
│   ├── piece/                 # Piece sets (PNG, multiple sizes)
│   ├── sound/                 # Move/capture sound effects
│   └── stockfish/             # Stockfish binary (Linux)
├── CMakeLists.txt
├── CMakePresets.json
└── build-windows.bat          # One-shot Windows build helper
```

---

## License

MIT License — © 2026 Vũ Nhật Thanh. See [LICENSE](LICENSE) for details.
