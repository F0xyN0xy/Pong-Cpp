# PONG — C++ Edition

A polished, feature-rich arcade Pong clone built with **Raylib 5.x** and modern C++17. No external assets required — all audio is synthesized procedurally at runtime.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)
![Raylib](https://img.shields.io/badge/Raylib-5.x-green.svg)
![Github release](https://img.shields.io/github/v/release/F0xyN0xy/Pong-Cpp)
![License](https://img.shields.io/github/license/F0xyN0xy/Pong-Cpp)

---

## Table of Contents

- [Features](#features)
- [Getting Started](#getting-started)
  - [Prerequisites](#prerequisites)
  - [Building](#building)
    - [Windows (MinGW)](#windows-mingw)
    - [Windows (MSVC)](#windows-msvc)
    - [Linux](#linux)
    - [macOS](#macos)
- [Controls](#controls)
- [Game Modes](#game-modes)
- [Settings](#settings)
- [Achievements](#achievements)
- [Skins](#skins)
- [Project Structure](#project-structure)
- [Adding Background Music](#adding-background-music)
- [License](#license)

---

## Features

- **Single Player vs AI** with 3 difficulty levels (Easy, Medium, Hard)
- **Local Two-Player** competitive mode
- **4 Game Modes**: Classic, Speed, Endless, Practice
- **Random Round Modifiers**: Big Paddles, Small Paddles, Turbocharged, Wobbly Ball
- **Procedural Audio**: All SFX generated at runtime — no asset files needed
- **Particle Effects** on paddle hits, wall bounces, and scoring
- **Ball Trail System** with fading ghost circles
- **Screen Shake** on intense hits
- **Animated Starfield Background** with parallax depth
- **Score Pop-up Animations**
- **Achievement System** with 7 unlockable achievements
- **Paddle Skin System** with unlockable colors
- **Persistent Save Data** (statistics, settings, unlocks)
- **Neon Theme** toggle with gradient overlays
- **Customizable Settings**: Volume sliders, fullscreen, FPS display
- **Resizable Window** with aspect-ratio-preserving scaling
- **Pause Menu** with resume, restart, settings, and quit options

---

## Getting Started

### Prerequisites

| Dependency | Version | Notes |
|------------|---------|-------|
| [Raylib](https://www.raylib.com/) | 5.x | Graphics/audio/input library |
| C++ Compiler | C++17 | GCC, Clang, or MSVC |
| Git | any | For cloning |

### Building

#### Windows (MinGW)

```bash
# 1. Clone the repository
git clone https://github.com/yourusername/pong-cpp.git
cd pong-cpp

# 2. Compile (adjust raylib path as needed)
g++ -std=c++17 -O2 main.cpp -o pong.exe     -I"C:/raylib/include"     -L"C:/raylib/lib"     -lraylib -lopengl32 -lgdi32 -lwinmm

# 3. Run
./pong.exe
```

#### Windows (MSVC)

```bash
cl /std:c++17 /O2 /EHsc main.cpp /Fe:pong.exe    /I"C:/raylib/include"    /link /LIBPATH:"C:/raylib/lib" raylib.lib opengl32.lib gdi32.lib winmm.lib
```

#### Linux

```bash
# Install raylib
sudo apt-get install libraylib-dev

# Compile
g++ -std=c++17 -O2 main.cpp -o pong -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

# Run
./pong
```

#### macOS

```bash
# Install raylib via Homebrew
brew install raylib

# Compile
g++ -std=c++17 -O2 main.cpp -o pong -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo

# Run
./pong
```

---

## Controls

| Action | Player 1 | Player 2 |
|--------|----------|----------|
| Move Up | `W` | `↑` (Up Arrow) |
| Move Down | `S` | `↓` (Down Arrow) |
| Pause | `ESC` | — |
| Restart | `R` | — |
| Menu Select | `↑` / `↓` or `W` / `S` | — |
| Menu Confirm | `ENTER` or `SPACE` | — |
| Fullscreen | `F11` or `F` | — |

---

## Game Modes

| Mode | Description |
|------|-------------|
| **Classic** | First to 7 points wins |
| **Speed** | Ball accelerates faster after each hit |
| **Endless** | No win condition — survive as long as possible |
| **Practice** | No scoring, free play for training |

---

## Settings

Accessible from the main menu or pause menu:

| Setting | Description |
|---------|-------------|
| **Master Volume** | Global volume multiplier |
| **SFX Volume** | Sound effects volume |
| **Music Volume** | Background music volume |
| **Fullscreen** | Toggle fullscreen mode |
| **Show FPS** | Display frames-per-second counter |
| **Neon Theme** | Enable gradient overlay effects |

All settings are saved automatically to `pong_save.dat`.

---

## Achievements

| Achievement | Unlock Condition |
|-------------|------------------|
| 🏆 First Victory | Win your first match |
| 🔥 Rally x10 | Achieve a 10-hit rally |
| ⚡ Rally x25 | Achieve a 25-hit rally |
| 🎯 Perfect Game | Win 7-0 without conceding |
| 💨 Speed Demon | Ball speed exceeds 700 |
| 🎨 Collector | Unlock all paddle skins |
| 🎖️ Veteran | Play 50 total games |

---

## Skins

Unlock new paddle colors by playing games:

| Skin | Unlock Requirement |
|------|-------------------|
| Classic White | Default |
| Neon Cyan | Play 5 games |
| Neon Magenta | Play 15 games |
| Gold | Play 30 games |

Skins can be equipped per-player in the Skins screen (`ENTER` for P1, `SPACE` for P2).

---

## Project Structure

```
pong-cpp/
├── main.cpp              # Entire game source (single-file)
├── README.md             # This file
├── pong_save.dat         # Auto-generated save file
└── music.mp3             # Optional: background music file
```

The game is intentionally contained in a **single source file** for simplicity. All audio (except optional background music) is synthesized at runtime.

---

## Adding Background Music

The game supports optional background music. To add your own:

1. Place an audio file named **`music.mp3`** in the same folder as the executable
2. Supported formats: `.mp3`, `.ogg`, `.wav`, `.flac`, `.xm`, `.mod`
3. The **Music Volume** slider in Settings controls its volume

> **Note:** If no music file is found, the game runs silently with only SFX.

---

## License

This project is licensed under the **MIT License**.

---

## Credits

- Built with [Raylib](https://www.raylib.com/) — a simple and easy-to-use game development library
- Procedural audio synthesis inspired by classic chiptune techniques

---