#pragma once
#include <imgui.h>
#include <mutex>

// Adapted from ImGui Demo's "Example App: Debug Console" & ACUFixes

enum class ConsoleMode
{
    Hidden,
    ForegroundAndFocusable,
    BackgroundSemitransparentAndUnfocusable,
};

class ImGuiConsole
{
public:
    ImGuiConsole();
    ~ImGuiConsole();

    void    ClearLog();
    void AddLog(const char* s);
    void AddLogF(const char* fmt, ...) IM_FMTARGS(2);
    void Draw(const char* title);
    void    DrawEmbedded(const char* title);
    void    DrawIfVisible(const char* title);
    void ToggleVisibility();

private:
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
    std::recursive_mutex  m_Mutex;
    std::string           m_SingleLineToCopy;
    ImVec2                m_LastWindowPos = ImVec2(100, 100);
    ImVec2                m_LastWindowSize = ImVec2(520, 600);
    bool                  m_IsTabbed = false;
public:
    ConsoleMode           mode = ConsoleMode::Hidden;
};
