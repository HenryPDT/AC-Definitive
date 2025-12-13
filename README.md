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

### 2. EaglePatch Integration (Plugins)
Ported from **Sergeanur's EaglePatch**, these plugins hook into the game engine to apply logic fixes and utilize the modern input provided by BaseHook.

**Input & Engine Fixes:**
*   **XInput Injection:** Replaces the game's legacy input handling with a modern XInput implementation (fed by BaseHook), fixing incorrect trigger mappings and axis issues.
*   **Simultaneous Input:** Allows Keyboard/Mouse and Gamepad to be used at the same time without the UI flickering or inputs getting locked.
*   **Game Hotplugging:** Ensures the game logic recovers correctly if a controller is disconnected and reconnected.
*   **Skip Intro Videos:** Bypasses startup splash screens.

**Game-Specific Graphics & Content Fixes:**
*   **AC1:**
    *   Fixes Multisampling (MSAA) on modern hardware.
    *   Fixes duplicate resolution entries in DX10 mode.
    *   Disables Telemetry/Bloat functions.
*   **AC2:**
    *   **Graphics:** Increases Shadow Map resolution to 4096 and maxes out draw distances for buildings and NPCs.
    *   **Content:** Unlocks Uplay rewards (Auditore Crypt, Altair Robes, Extra Knives).

### 3. In-Game Mod Menu & Console
No more editing `.ini` files or restarting the game to toggle features.
*   Press **INSERT** to toggle the menu.
*   Toggle fixes, cheats, and modifications in real-time.
*   Integrated Developer Console (Press **~ Tilde**).

### 4. For Developers: Modular Plugin System
*   **Automatic Hooking:** The loader handles hooking DirectX (DX9/10/11), DirectInput8, and WndProc.
*   **Shared API:** Plugins share the same ImGui context and hooks, preventing conflicts.
*   **AutoAssemblerKinda:** A C++ library for easy runtime assembly patching (similar to Cheat Engine's AutoAssembler).

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

*   [**Sergeanur**](https://github.com/Sergeanur/EaglePatch): Creator of the original **EaglePatch**. This project is a port and expansion of his work.
*   [**NameTaken3125**](https://github.com/NameTaken3125/ACUFixes): Creator of **ACUFixes**. The plugin loader architecture, `AutoAssemblerKinda` library, and serialization utils are derived from his source code.
*   [**rdbo**](https://github.com/rdbo/ImGui-DirectX-9-Kiero-Hook) / **Rebzzel** (kiero): For the DirectX hooking method and libraries used in BaseHook.
*   **ocornut**: For [Dear ImGui](https://github.com/ocornut/imgui).
