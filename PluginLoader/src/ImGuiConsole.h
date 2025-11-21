#pragma once
#include <imgui.h>
#include <mutex>
#include <string>
#include <vector>
#include <deque>

enum class ConsoleMode
{
    Hidden,
    ForegroundAndFocusable,
};

struct LogItem {
    std::string text;
    int level; // 0: Info, 1: Warn, 2: Error
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
    std::deque<LogItem>   m_Items;
    ImVector<const char*> m_Commands;
    ImVector<std::string> m_History;
    int                   m_HistoryPos;
    ImGuiTextFilter       m_Filter;
    bool                  m_AutoScroll;
    bool                  m_ScrollToBottom;
    std::recursive_mutex  m_Mutex;
    std::string           m_SingleLineToCopy;
public:
    ConsoleMode           mode = ConsoleMode::Hidden;
};
