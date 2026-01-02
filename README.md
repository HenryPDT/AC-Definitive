# AC Definitive Framework

AC Definitive is a unified modding framework and plugin loader for the classic Assassin's Creed games on PC (AC1, AC2, Brotherhood, and Revelations).

It combines a robust plugin architecture with the essential hardware and graphical fixes of **EaglePatch**, wrapping them in a modern, in-game graphical interface (ImGui).

## Supported Games

| Game | Versions Supported |
| :--- | :--- |
| **Assassin's Creed I** | DX9 & DX10 (GOG/Steam) |
| **Assassin's Creed II** | Uplay & Retail 1.01 |
| **AC: Brotherhood** | Latest (Uplay/Steam) |
| **AC: Revelations** | Latest (Uplay/Steam) |

## Features

### 1. Modern Controller Support (BaseHook)
The framework includes `BaseHook`, a low-level input wrapper that modernizes hardware compatibility.
*   **Universal Device Support:** Native support for **Xbox** (XInput) and **PlayStation** (DualShock 4, DualSense) controllers via USB/Bluetooth.
*   **Virtual XInput:** Creates a virtual XInput device from connected controllers. This allows the game (via the EaglePatch plugin) to support PlayStation controllers natively without tools like DS4Windows or Steam Input.
*   **Hotplugging (Backend):** Detects device connection/disconnection at the hardware level to update the virtual state seamlessly.

### 2. Window Management & Performance
A completely rewritten windowing and graphics backend provides modern conveniences for these older titles.
*   **Windowed Modes:** Force the game into **Borderless Fullscreen**, **Exclusive Fullscreen**, or standard **Windowed** modes.
*   **Smart Resizing:** Choose between "Match Game Resolution" (resizes window to game) or "Scale Content" (scales game to fit window/desktop).
*   **Framerate Limiter:** Integrated high-precision framerate limiter to cap FPS without external tools.
*   **CPU Affinity Fix:** Automatically restricts the game to < 32 cores (or disables Core 0) to prevent crashes and stuttering on modern high-core count CPUs (Ryzen/Threadripper).

### 3. EaglePatch Integration (Plugins)
Ported from **Sergeanur's EaglePatch** and enhanced with dynamic scanning, these plugins apply game-specific logic fixes.

**Input & Engine Fixes:**
*   **XInput Injection:** Replaces the game's legacy input handling with a modern XInput implementation.
*   **Simultaneous Input:** Hybrid input support allows Keyboard/Mouse and Gamepad to be used simultaneously.
*   **Skip Intro:** Bypasses startup videos.

**Game-Specific Features:**
*   **AC1:**
    *   Fixes Multisampling (MSAA) on modern hardware.
    *   Fixes duplicate resolution entries in DX10 mode.
    *   Disables Telemetry/Bloat functions.
*   **AC2:**
    *   **FPS Unlock:** Removes the 60 FPS cap.
    *   **Graphics:** Increases Shadow Map resolution to 4096 and maxes out draw distances.
    *   **Content:** Unlocks Uplay rewards (Auditore Crypt, Altair Robes, Extra Knives).

### 4. In-Game Mod Menu & Console
No more editing `.ini` files manually. The new UI allows for real-time configuration.
*   Press **INSERT** to toggle the centralized **Mod Menu**.
*   **Settings Tab:** Configure Window modes, FPS limits, CPU affinity, and hotkeys.
*   **Plugins Tab:** View loaded plugins and toggle specific game patches (e.g., Graphics, Cheats) on the fly.
*   **Developer Console:** Press **~ (Tilde)** to view logs and debug info.

### 5. For Developers: Modular Architecture
*   **Pattern Scanning:** Plugins use AOB scanning to locate addresses dynamically, improving compatibility across versions.
*   **Shared Libraries:** `AC-RE` libraries provide shared game structures and version detection.
*   **Automatic Hooking:** The loader handles hooking DirectX (DX9/10/11), DirectInput8, and WndProc.
*   **AutoAssemblerKinda:** A C++ library for easy runtime assembly patching (supports JMP injection and code caves).

## Installation and File Structure

This framework requires [**Ultimate ASI Loader**](https://github.com/ThirteenAG/Ultimate-ASI-Loader) to load the plugins into the game.

### Installation Steps

1.  Download the **AC Definitive** zip file for your specific game from the [**Releases**](https://github.com/HenryPDT/AC-Definitive/releases) page.
2.  Download [**Ultimate ASI Loader**](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.4.0/Ultimate-ASI-Loader.zip) (`Ultimate-ASI-Loader.zip`).
3.  Open `Ultimate-ASI-Loader.zip` and extract the **32-bit** `dinput8.dll` into your game's main directory (where the `.exe` is located).
4.  Open the **AC Definitive** zip and drag the `scripts` folder into your game's main directory.

### Final Folder Structure

    Assassin's Creed Directory/
    ├── AssassinsCreed_Dx9.exe      (or similar)
    ├── dinput8.dll                 (Ultimate ASI Loader)
    └── scripts/
        ├── PluginLoader.asi        (Core Loader)
        └── plugins/                (Game Specific Plugins)
            └── AC1-EaglePatch.asi  (or AC2/ACB/ACR)

## Controls

*   **INSERT** - Open/Close the Mod Menu
*   **END** - Unload/Detach the framework entirely.
*   **~** (Tilde) - Open the Debug Console

## Credits

*   [**Sergeanur**](https://github.com/Sergeanur/EaglePatch): Creator of the original **EaglePatch**.
*   [**NameTaken3125**](https://github.com/NameTaken3125/ACUFixes): Creator of **ACUFixes**. Core architecture derived from his work.
*   [**rdbo**](https://github.com/rdbo/ImGui-DirectX-9-Kiero-Hook) / **Rebzzel** (kiero): DirectX hooking libraries.
*   **ocornut**: [Dear ImGui](https://github.com/ocornut/imgui).