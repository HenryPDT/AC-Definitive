#pragma once
#include <imgui.h>
#include <mutex>
#include <string>
#include <vector>

enum class ConsoleMode
{
    Hidden,
    ForegroundAndFocusable,
};

class ImGuiConsole
{
public:
    ImGuiConsole();
    ~ImGuiConsole();

    void AddLog(const char* s);
    void AddLogF(const char* fmt, ...) IM_FMTARGS(2);
    void Draw(const char* title);
    void ToggleVisibility() { mode = (mode == ConsoleMode::Hidden) ? ConsoleMode::ForegroundAndFocusable : ConsoleMode::Hidden; }

private:
    void ClearLog();
    void ExecCommand(const char* command_line);

    static int TextEditCallbackStub(ImGuiInputTextCallbackData* data);
    int TextEditCallback(ImGuiInputTextCallbackData* data);

    char                  m_InputBuf[256];
    ImVector<char*>       m_Items;
    ImVector<const char*> m_Commands;
    ImVector<char*>       m_History;
    int                   m_HistoryPos;
    ImGuiTextFilter       m_Filter;
    bool                  m_AutoScroll;
    bool                  m_ScrollToBottom;
    std::mutex            m_Mutex;
    std::string           m_SingleLineToCopy;
public:
    ConsoleMode           mode = ConsoleMode::Hidden;
};
