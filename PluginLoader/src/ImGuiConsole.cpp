#include "ImGuiConsole.h"
#include <vector>
#include <string>
#include <algorithm>
#include <cstdarg>

// Portable helpers
static int Stricmp(const char* s1, const char* s2) { int d; while ((d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; } return d; }
static int Strnicmp(const char* s1, const char* s2, int n) { int d = 0; while (n > 0 && (d = toupper(*s2) - toupper(*s1)) == 0 && *s1) { s1++; s2++; n--; } return d; }
static char* Strdup(const char* s) { size_t len = strlen(s) + 1; void* buf = malloc(len); IM_ASSERT(buf); return (char*)memcpy(buf, (const void*)s, len); }
static void Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

ImGuiConsole::ImGuiConsole()
{
    ClearLog();
    memset(m_InputBuf, 0, sizeof(m_InputBuf));
    m_HistoryPos = -1;
    m_Commands.push_back("HELP");
    m_Commands.push_back("HISTORY");
    m_Commands.push_back("CLEAR");
    m_AutoScroll = true;
    m_ScrollToBottom = false;
    m_SingleLineToCopy.reserve(1024);
}

ImGuiConsole::~ImGuiConsole()
{
    ClearLog();
    for (int i = 0; i < m_History.Size; i++)
        free(m_History[i]);
}

void ImGuiConsole::ClearLog()
{
    std::lock_guard lock(m_Mutex);
    for (int i = 0; i < m_Items.Size; i++)
        free(m_Items[i]);
    m_Items.clear();
}

void ImGuiConsole::AddLog(const char* s)
{
    std::lock_guard lock(m_Mutex);
    m_Items.push_back(Strdup(s));
}

void ImGuiConsole::AddLogF(const char* fmt, ...)
{
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf) - 1] = 0;
    va_end(args);
    AddLog(buf);
}

void ImGuiConsole::ExecCommand(const char* command_line)
{
    AddLogF("> %s", command_line);
    m_HistoryPos = -1;
    for (int i = m_History.Size - 1; i >= 0; i--)
        if (Stricmp(m_History[i], command_line) == 0)
        {
            free(m_History[i]);
            m_History.erase(m_History.begin() + i);
            break;
        }
    m_History.push_back(Strdup(command_line));

    if (Stricmp(command_line, "CLEAR") == 0)
    {
        ClearLog();
    }
    else if (Stricmp(command_line, "HELP") == 0)
    {
        AddLogF("Available commands: %s, %s, %s", "HELP", "HISTORY", "CLEAR");
    }
    else if (Stricmp(command_line, "HISTORY") == 0)
    {
        int first = m_History.Size - 10;
        for (int i = first > 0 ? first : 0; i < m_History.Size; i++)
            AddLog(m_History[i]);
    }
    else
    {
        AddLogF("Unknown command: '%s'", command_line);
    }
    m_ScrollToBottom = true;
}

int ImGuiConsole::TextEditCallbackStub(ImGuiInputTextCallbackData* data)
{
    return static_cast<ImGuiConsole*>(data->UserData)->TextEditCallback(data);
}

int ImGuiConsole::TextEditCallback(ImGuiInputTextCallbackData* data)
{
    switch (data->EventFlag)
    {
    case ImGuiInputTextFlags_CallbackCompletion:
    {
        const char* word_end = data->Buf + data->CursorPos;
        const char* word_start = word_end;
        while (word_start > data->Buf)
        {
            const char c = word_start[-1];
            if (c == ' ' || c == '\t' || c == ',' || c == ';')
                break;
            word_start--;
        }
        ImVector<const char*> candidates;
        for (int i = 0; i < m_Commands.Size; i++)
            if (Strnicmp(m_Commands[i], word_start, (int)(word_end - word_start)) == 0)
                candidates.push_back(m_Commands[i]);

        if (candidates.Size == 1)
        {
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0]);
            data->InsertChars(data->CursorPos, " ");
        }
        break;
    }
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int prev_history_pos = m_HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (m_HistoryPos == -1) m_HistoryPos = m_History.Size - 1;
            else if (m_HistoryPos > 0) m_HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (m_HistoryPos != -1)
                if (++m_HistoryPos >= m_History.Size)
                    m_HistoryPos = -1;
        }
        if (prev_history_pos != m_HistoryPos)
        {
            const char* history_str = (m_HistoryPos >= 0) ? m_History[m_HistoryPos] : "";
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, history_str);
        }
    }
    }
    return 0;
}

void ImGuiConsole::Draw(const char* title)
{
    if (mode == ConsoleMode::Hidden) return;

    ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, nullptr, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("Options"))
        {
            ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (ImGui::BeginPopupContextWindow())
        {
            if (ImGui::Selectable("Copy line")) ImGui::SetClipboardText(m_SingleLineToCopy.c_str());
            if (ImGui::Selectable("Clear")) ClearLog();
            ImGui::EndPopup();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
        std::lock_guard lock(m_Mutex);
        for (int i = 0; i < m_Items.Size; i++)
        {
            const char* item = m_Items[i];
            ImVec4 color;
            bool has_color = false;
            if (strstr(item, "[ERROR]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
            else if (strstr(item, "[WARN]")) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            if (has_color) ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) m_SingleLineToCopy = item;
            if (has_color) ImGui::PopStyleColor();
        }

        if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
            ImGui::SetScrollHereY(1.0f);
        m_ScrollToBottom = false;

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::Separator();

    bool reclaim_focus = false;
    ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
    if (ImGui::InputText("Input", m_InputBuf, IM_ARRAYSIZE(m_InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
    {
        char* s = m_InputBuf;
        Strtrim(s);
        if (s[0]) ExecCommand(s);
        strcpy_s(s, 256, "");
        reclaim_focus = true;
    }

    ImGui::SetItemDefaultFocus();
    if (reclaim_focus)
        ImGui::SetKeyboardFocusHere(-1);

    ImGui::End();
}
