#pragma once

#include <Windows.h>

enum class ConsoleMode;

namespace InputCapture
{
    struct InputCaptureState
    {
        bool blockKeyboardWndProc = false;
        bool blockMouseMoveWndProc = false;
        bool blockDirectInputMouseButtons = false; // includes mouse buttons + wheel for the game (DirectInput path)
        bool showMouseCursor = false;              // ImGui cursor rendering
    };

    // Central policy: preserve project constraint:
    // - keyboard + mouse movement via WndProc
    // - mouse buttons/wheel via DirectInput (game path)
    inline InputCaptureState Compute(bool isFocused, bool menuOpen, ConsoleMode consoleMode)
    {
        const bool consoleWantsFocus = (consoleMode == ConsoleMode::ForegroundAndFocusable);
        const bool uiActive = (menuOpen || consoleWantsFocus);
        const bool block = (isFocused && uiActive);

        InputCaptureState s;
        s.blockKeyboardWndProc = block;
        s.blockMouseMoveWndProc = block;
        s.blockDirectInputMouseButtons = block;
        s.showMouseCursor = block;
        return s;
    }
}


