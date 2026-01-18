#include "Cheats/GameFlowCheats.h"
#include "Game/Singletons.h"
#include "Game/Bink.h"
#include "Trainer.h"
#include "Hooks.h"
#include "imgui.h"
#include <PatternScanner.h>
#include <AutoAssemblerKinda.h>

extern Trainer::Configuration g_config;

const char* GameFlowCheats::GetBinkFileName()
{
    void* pBinkFile = Hooks::GetBinkFilePointer();
    if (!pBinkFile) return nullptr;

    using namespace AutoAssemblerKinda;
    uintptr_t pEsi = reinterpret_cast<uintptr_t>(pBinkFile);

    // CE Def: Address: pBinkFile (ESI), Offsets: 0, C
    // Try Pointer to String at [ESI + 0xC]
    if (IsSafeRead((void*)(pEsi + 0xC), sizeof(uintptr_t))) {
        uintptr_t pString = *reinterpret_cast<uintptr_t*>(pEsi + 0xC);
        if (pString > 0x10000 && IsSafeRead((void*)pString, 1)) {
            char c = *reinterpret_cast<char*>(pString);
            if (c != 0) return reinterpret_cast<char*>(pString);
        }
    }

    // Try Embedded String at ESI + 0xC
    if (IsSafeRead((void*)(pEsi + 0xC), 1)) {
        char* pEmbedded = reinterpret_cast<char*>(pEsi + 0xC);
        if (pEmbedded[0] != 0 && pEmbedded[0] > 0x1F) {
            return pEmbedded;
        }
    }

    return nullptr;
}

void GameFlowCheats::TriggerSkip()
{
    auto* bink = AC2::GetBink();
    if (bink)
    {
        // Setting m_FrameCount to 1 makes the video finish immediately on the next frame update.
        bink->m_FrameCount = 1;
    }
}

void GameFlowCheats::Update()
{
    bool binkDown = g_config.Key_SkipBink.get().IsPressed();
    if (binkDown && !m_BinkKeyWasDown) {
        TriggerSkip();
    }
    m_BinkKeyWasDown = binkDown;
}

void GameFlowCheats::DrawUI()
{
    // Skip Current Video Button
    std::string btnLabel = "Skip Current Video [" + g_config.Key_SkipBink.get().ToString() + "]";
    if (ImGui::Button(btnLabel.c_str(), ImVec2(-FLT_MIN, 0))) {
        TriggerSkip();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Skips Bink videos (Logos/Movies). Auto-disables after skip.");

    // Bink Info
    auto* bink = AC2::GetBink();
    if (bink && AutoAssemblerKinda::IsSafeRead(bink, sizeof(AC2::Bink)))
    {
        // Check if Bink is currently active (width > 0 usually)
        if (bink->m_Width > 0)
        {
            ImGui::Separator();
            const char* filename = GetBinkFileName();
            ImGui::Text("Video: %s", filename ? filename : "Unknown");
            ImGui::Text("Resolution: %dx%d", bink->m_Width, bink->m_Height);
            ImGui::Text("Frame: %d / %d", bink->m_CurrentFrame, bink->m_FrameCount);

            float fps = 0.0f;
            if (bink->m_FpsDivisor > 0)
                fps = (float)bink->m_FpsMultiplier / (float)bink->m_FpsDivisor;
            ImGui::Text("FPS: %.2f", fps);

            ImGui::ProgressBar((float)bink->m_CurrentFrame / (float)bink->m_FrameCount);
        }
    }

    ImGui::Separator();

    ImGui::Checkbox("Skip End Credits", &g_config.SkipCredits.get());
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Fast-forwards the credits sequence.");
}
