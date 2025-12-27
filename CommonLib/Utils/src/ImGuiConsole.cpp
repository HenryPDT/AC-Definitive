#include "ImGuiConsole.h"
#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <optional>
#include <string>

static void Strtrim(char* s) { char* str_end = s + strlen(s); while (str_end > s && str_end[-1] == ' ') str_end--; *str_end = 0; }

ImGuiConsole::ImGuiConsole()
{
    ClearLog();
    memset(m_InputBuf, 0, sizeof(m_InputBuf));
    m_HistoryPos = -1;

    // "CLASSIFY" is here to provide the test case where "C"+[tab] completes to "CL" and display multiple matches.
    m_Commands.push_back("HELP");
    m_Commands.push_back("HISTORY");
    m_Commands.push_back("CLEAR");
    m_Commands.push_back("CLASSIFY");
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
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    for (int i = 0; i < m_Items.Size; i++)
        free(m_Items[i]);
    m_Items.clear();
}

void ImGuiConsole::AddLog(const char* s)
{
    std::lock_guard<std::recursive_mutex> lock(m_Mutex);
    m_Items.push_back(_strdup(s));
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
    AddLogF("# %s\n", command_line);

    // Insert into history. First find match and delete it so it can be pushed to the back.
    m_HistoryPos = -1;
    for (int i = m_History.Size - 1; i >= 0; i--)
        if (_stricmp(m_History[i], command_line) == 0)
        {
            free(m_History[i]);
            m_History.erase(m_History.begin() + i);
            break;
        }
    m_History.push_back(_strdup(command_line));

    if (_stricmp(command_line, "CLEAR") == 0)
    {
        ClearLog();
    }
    else if (_stricmp(command_line, "HELP") == 0)
    {
        AddLogF("Commands:");
        for (int i = 0; i < m_Commands.Size; i++)
            AddLogF("- %s", m_Commands[i]);
    }
    else if (_stricmp(command_line, "HISTORY") == 0)
    {
        int first = m_History.Size - 10;
        for (int i = first > 0 ? first : 0; i < m_History.Size; i++)
            AddLogF("%3d: %s\n", i, m_History[i]);
    }
    else
    {
        AddLogF("Unknown command: '%s'\n", command_line);
    }

    // On command input, we scroll to bottom even if AutoScroll==false
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
            if (_strnicmp(m_Commands[i], word_start, (int)(word_end - word_start)) == 0)
                candidates.push_back(m_Commands[i]);

        if (candidates.Size == 0)
        {
            AddLogF("No match for \"%.*s\"!\n", (int)(word_end - word_start), word_start);
        }
        else if (candidates.Size == 1)
        {
            // Single match. Delete the beginning of the word and replace it entirely so we've got nice casing.
            data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
            data->InsertChars(data->CursorPos, candidates[0]);
            data->InsertChars(data->CursorPos, " ");
        }
        else
        {
            // Multiple matches. Complete as much as we can..
            // So inputing "C"+Tab will complete to "CL" then display "CLEAR" and "CLASSIFY" as matches.
            int match_len = (int)(word_end - word_start);
            for (;;)
            {
                int c = 0;
                bool all_candidates_matches = true;
                for (int i = 0; i < candidates.Size && all_candidates_matches; i++)
                    if (i == 0)
                        c = toupper(candidates[i][match_len]);
                    else if (c == 0 || c != toupper(candidates[i][match_len]))
                        all_candidates_matches = false;
                if (!all_candidates_matches)
                    break;
                match_len++;
            }

            if (match_len > 0)
            {
                data->DeleteChars((int)(word_start - data->Buf), (int)(word_end - word_start));
                data->InsertChars(data->CursorPos, candidates[0], candidates[0] + match_len);
            }

            // List matches
            AddLog("Possible matches:\n");
            for (int i = 0; i < candidates.Size; i++)
                AddLogF("- %s\n", candidates[i]);
        }

        break;
    }
    case ImGuiInputTextFlags_CallbackHistory:
    {
        const int prev_history_pos = m_HistoryPos;
        if (data->EventKey == ImGuiKey_UpArrow)
        {
            if (m_HistoryPos == -1)
                m_HistoryPos = m_History.Size - 1;
            else if (m_HistoryPos > 0)
                m_HistoryPos--;
        }
        else if (data->EventKey == ImGuiKey_DownArrow)
        {
            if (m_HistoryPos != -1)
                if (++m_HistoryPos >= m_History.Size)
                    m_HistoryPos = -1;
        }

        // A better implementation would preserve the data on the current input line along with cursor position.
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

void ImGuiConsole::ToggleVisibility()
{
    switch (mode)
    {
    case ConsoleMode::Hidden:
        mode = ConsoleMode::ForegroundAndFocusable;
        break;
    case ConsoleMode::ForegroundAndFocusable:
        if (m_IsTabbed)
            mode = ConsoleMode::Hidden;
        else
            mode = ConsoleMode::BackgroundSemitransparentAndUnfocusable;
        break;
    case ConsoleMode::BackgroundSemitransparentAndUnfocusable:
        mode = ConsoleMode::Hidden;
        break;
    default:
        mode = ConsoleMode::ForegroundAndFocusable;
        break;
    }
}

void ImGuiConsole::Draw(const char* title)
{
    DrawIfVisible(title);
}

void ImGuiConsole::DrawIfVisible(const char* title)
{
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
    ImGuiWindowFlags scrolling_region_flags = ImGuiWindowFlags_None;

    static float window_bgAlphaWhenSemi = 0.35f;
    float window_bgAlpha = 1.0f;
    bool showFooter = false;
    bool showMenuBar = true;
    switch (mode)
    {
    case ConsoleMode::Hidden:
        return;
    case ConsoleMode::BackgroundSemitransparentAndUnfocusable:
    {
        window_bgAlpha = window_bgAlphaWhenSemi;
        window_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoNav;
        scrolling_region_flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
        showFooter = false;
        showMenuBar = false;
        break;
    }
    case ConsoleMode::ForegroundAndFocusable:
    {
        window_bgAlpha = 1.0f;
        scrolling_region_flags |= ImGuiWindowFlags_HorizontalScrollbar;
        showFooter = true;
        showMenuBar = true;
        break;
    }
    default:
        return;
    }

    static std::optional<float> forcedAlphaForThisFrame = {};
    if (forcedAlphaForThisFrame)
    {
        window_bgAlpha = *forcedAlphaForThisFrame;
        forcedAlphaForThisFrame.reset();
    }
    
    // Only force position and size if we are in the background mode, otherwise let the user control it.
    if (mode == ConsoleMode::BackgroundSemitransparentAndUnfocusable)
    {
        ImGui::SetNextWindowPos(m_LastWindowPos);
        ImGui::SetNextWindowSize(m_LastWindowSize);
    }
    else
    {
        ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
    }

    ImGui::SetNextWindowBgAlpha(window_bgAlpha);

    // Ensure unique ID for the window so it doesn't conflict if we have multiple instances (though this is a singleton)
    // We also use a different ID for the background mode so that it doesn't mess up the docking state of the main window.
    std::string windowTitle = title;
    if (mode == ConsoleMode::BackgroundSemitransparentAndUnfocusable)
        windowTitle += "##Overlay";

    if (!ImGui::Begin(windowTitle.c_str(), nullptr, window_flags))
    {
        ImGui::End();
        return;
    }

    if (mode == ConsoleMode::ForegroundAndFocusable)
    {
        // Detect if we are tabbed with other windows.
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        m_IsTabbed = (window->DockNode && window->DockNode->Windows.Size > 1);

        m_LastWindowPos = ImGui::GetWindowPos();
        m_LastWindowSize = ImGui::GetWindowSize();
    }

    ImGuiStyle& style = ImGui::GetStyle();
    const bool showFooterCommandInput = true;
    float scrollingRegionHeight = 0.0f;
    if (showFooter)
    {
        float footer_height_to_reserve = (0
            + style.ItemSpacing.y                               // Separator
            + ImGui::GetFontSize() + style.ItemSpacing.y        // Row of SmallButtons
            + style.ItemSpacing.y                               // Separator
            + ImGui::GetFrameHeightWithSpacing()                // Filter box
            + style.ItemSpacing.y                               // Separator
            + ImGui::GetFrameHeightWithSpacing()                // Text input frame (for the console input)
            );
        scrollingRegionHeight = -footer_height_to_reserve;
    }
    if (ImGui::BeginMenuBar())
    {
        if (showMenuBar)
        {
            if (ImGui::BeginMenu("Options"))
            {
                ImGui::SliderFloat("Transparency when semitransparent", &window_bgAlphaWhenSemi, 0.1f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
                if (ImGui::IsItemActive()) forcedAlphaForThisFrame = window_bgAlphaWhenSemi;
                ImGui::Checkbox("Autoscroll", &m_AutoScroll);
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenuBar();
    }

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, scrollingRegionHeight), false, scrolling_region_flags);
    bool copy_to_clipboard = false;
    if (ImGui::BeginPopupContextWindow())
    {
        if (ImGui::Selectable("Copy line"))
        {
            ImGui::SetClipboardText(m_SingleLineToCopy.c_str());
        }
        if (ImGui::Selectable("Copy all")) copy_to_clipboard = true;
        if (ImGui::Selectable("Clear")) ClearLog();
        ImGui::EndPopup();
    }

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
    if (copy_to_clipboard)
        ImGui::LogToClipboard();
    {
        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (int i = 0; i < m_Items.Size; i++)
        {
            const char* item = m_Items[i];
            if (!m_Filter.PassFilter(item))
                continue;

            ImVec4 color;
            bool has_color = false;
            if (strstr(item, "[ERROR]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
            else if (strstr(item, "[WARN]")) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            if (has_color)
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            {
                m_SingleLineToCopy = item;
            }
            if (has_color)
                ImGui::PopStyleColor();
        }
    }
    if (copy_to_clipboard)
        ImGui::LogFinish();

    if (m_ScrollToBottom || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
        ImGui::SetScrollHereY(1.0f);
    m_ScrollToBottom = false;

    ImGui::PopStyleVar();
    ImGui::EndChild();

    if (showFooter)
    {
        ImGui::Separator();

        if (ImGui::SmallButton("Clear")) { ClearLog(); }
        ImGui::Separator();

        // Options, Filter
        if (ImGui::Button("Options"))
            ImGui::OpenPopup("Options");
        ImGui::SameLine();
        m_Filter.Draw("Filter (\"incl,-excl\") (\"error\")");

        if (showFooterCommandInput)
        {
            ImGui::Separator();
            // Command-line
            bool reclaim_focus = false;
            ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
            if (ImGui::InputText("Input", m_InputBuf, IM_ARRAYSIZE(m_InputBuf), input_text_flags, &TextEditCallbackStub, (void*)this))
            {
                char* s = m_InputBuf;
                Strtrim(s);
                if (s[0])
                    ExecCommand(s);
                strcpy_s(s, 256, "");
                reclaim_focus = true;
            }

            // Auto-focus on window apparition
            ImGui::SetItemDefaultFocus();
            if (reclaim_focus)
                ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget
        }
    }

    ImGui::End();
}

void ImGuiConsole::DrawEmbedded(const char* title)
{
    // Reuse the previous simple implementation for embedded logic if needed, 
    // or just reimplement simpler version.
    // For now, I'll keep the previous embedded logic which is useful for the tab.
    
    // Options menu
    if (ImGui::BeginPopup("Options"))
    {
        ImGui::Checkbox("Auto-scroll", &m_AutoScroll);
        ImGui::EndPopup();
    }

    // Options, Filter
    if (ImGui::Button("Options"))
        ImGui::OpenPopup("Options");
    ImGui::SameLine();
    m_Filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
    ImGui::Separator();

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

        std::lock_guard<std::recursive_mutex> lock(m_Mutex);
        for (int i = 0; i < m_Items.Size; i++)
        {
            const char* item = m_Items[i];
            if (!m_Filter.PassFilter(item))
                continue;

            ImVec4 color;
            bool has_color = false;
            if (strstr(item, "[ERROR]")) { color = ImVec4(1.0f, 0.4f, 0.4f, 1.0f); has_color = true; }
            else if (strstr(item, "[WARN]")) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            else if (strncmp(item, "# ", 2) == 0) { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
            if (has_color)
                ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextUnformatted(item);
            if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
                m_SingleLineToCopy = item;
            if (has_color)
                ImGui::PopStyleColor();
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
}
