# AC Definitive Framework

AC Definitive is a unified modding framework and plugin loader for the classic Assassin's Creed games (AC1, AC2, Brotherhood, and Revelations).

It combines the robust plugin architecture derived from ACUFixes with the essential hardware and graphical fixes of EaglePatch, wrapping them in a modern, in-game graphical interface (ImGui).

## Supported Games

*   Assassin's Creed 1 (DX9 & DX10)
*   Assassin's Creed 2
*   Assassin's Creed: Brotherhood
*   Assassin's Creed: Revelations

## Features

### 1. (Improved) EaglePatch Controller Integration
This project integrates and expands upon Sergeanur's EaglePatch, providing essential fixes without needing external configuration files.
*   **XInput Controller Support:** Native support for Xbox 360/One/Series and generic XInput controllers.
*   **Hotplugging:** Connect or disconnect controllers while the game is running.
*   **Unified Inputs:** Simultaneous use of Keyboard/Mouse and Gamepad.

### 2. In-Game Mod Menu
No more editing .ini files or restarting the game to toggle features.
*   Press **INSERT** to toggle the menu.
*   Toggle fixes, cheats, and modifications in real-time.
*   Integrated Developer Console.

### 3. Modular Plugin System
*   **Automatic Injection:** The loader handles hooking DirectX (DX9/10/11) and the Windows message loop (WndProc).
*   **Plugin API:** A standardized API allowing developers to create independent .asi plugins that share the same ImGui context and game hooks.

## Installation and File Structure

This framework requires the [**Ultimate ASI Loader**](https://github.com/ThirteenAG/Ultimate-ASI-Loader) to function. It uses a specific directory structure to distinguish between the "Loader" and the "Plugins".

### Prerequisites
1.  Download [**Ultimate ASI Loader**](https://github.com/ThirteenAG/Ultimate-ASI-Loader) (dinput8.dll) from its repository.
2.  Download the **AC Definitive** release files.

### Installation Steps
1.  Place `dinput8.dll` in your game's root directory (next to the game executable).
2.  Create a folder named `scripts` in the root directory.
3.  Place `PluginLoader.asi` inside the `scripts` folder.
4.  Create a subfolder named `plugins` inside the `scripts` folder.
5.  Place the game-specific plugin (e.g., `AC1-EaglePatch.asi`) inside `scripts/plugins/`.

### Required Folder Structure

    Assassin's Creed Directory/
    ├── AssassinsCreed_Dx9.exe
    ├── dinput8.dll                 (Ultimate ASI Loader)
    └── scripts/
        ├── PluginLoader.asi        (The Manager)
        └── plugins/                (Specific Mods go here)
            ├── AC1-EaglePatch.asi
            └── [Other Plugins]

## Controls

*   **INSERT** - Open/Close the Mod Menu
*   **END** - Unload/Detach the framework (panic button)
*   **~** (Tilde) - Open the Debug Console

## Credits

*   [**Sergeanur**](https://github.com/Sergeanur/EaglePatch): Creator of the original **EaglePatch**. This project is a port and expansion of his work.
*   [**NameTaken3125**](https://github.com/NameTaken3125/ACUFixes): Creator of **ACUFixes**. The plugin loader architecture and `AutoAssemblerKinda` library are derived from his source code.
*   [**rdbo**](https://github.com/rdbo/ImGui-DirectX-9-Kiero-Hook): For the DirectX hook implementation used in BaseHook.