#include "pch.h"
#include "base.h"
#include <xinput.h>

typedef DWORD(WINAPI* XInputGetState_t)(DWORD, XINPUT_STATE*);

// Separate trampolines for each version to ensure correct pass-through
static XInputGetState_t oXInputGetState_1_3 = nullptr;
static XInputGetState_t oXInputGetState_1_4 = nullptr;
static XInputGetState_t oXInputGetState_9_1_0 = nullptr;

// Common processing logic
DWORD ProcessXInputInternal(DWORD dwUserIndex, XINPUT_STATE* pState, XInputGetState_t oFunc)
{
    if (!pState)
        return ERROR_BAD_ARGUMENTS;

    XINPUT_STATE virtualState{};
    bool haveVirtual = BaseHook::Hooks::TryGetVirtualXInputState(dwUserIndex, &virtualState);

    DWORD result = ERROR_DEVICE_NOT_CONNECTED;
    
    // Call original if we don't have virtual input or it's not the primary slot
    // We must call the specific original trampoline corresponding to the hooked function
    if (!haveVirtual || dwUserIndex != 0)
    {
        if (oFunc)
            result = oFunc(dwUserIndex, pState);
        else
            return ERROR_DEVICE_NOT_CONNECTED; 
    }

    if (haveVirtual)
    {
        *pState = virtualState;
        result = ERROR_SUCCESS;
    }

    if (result == ERROR_SUCCESS)
    {
        BaseHook::Data::lastXInputTime.store(GetTickCount64(), std::memory_order_relaxed);

        if (BaseHook::Data::bCallingImGui)
        {
            return result;
        }

        if (BaseHook::Data::bIsInitialized && BaseHook::Data::bBlockInput)
        {
            memset(pState, 0, sizeof(XINPUT_STATE));
        }
    }

    return result;
}

// Specific detours
DWORD WINAPI hkXInputGetState_1_3(DWORD dwUserIndex, XINPUT_STATE* pState) {
    return ProcessXInputInternal(dwUserIndex, pState, oXInputGetState_1_3);
}
DWORD WINAPI hkXInputGetState_1_4(DWORD dwUserIndex, XINPUT_STATE* pState) {
    return ProcessXInputInternal(dwUserIndex, pState, oXInputGetState_1_4);
}
DWORD WINAPI hkXInputGetState_9_1_0(DWORD dwUserIndex, XINPUT_STATE* pState) {
    return ProcessXInputInternal(dwUserIndex, pState, oXInputGetState_9_1_0);
}

namespace BaseHook { namespace Hooks {
    
    void HookXInputModule(const char* moduleName, XInputGetState_t* pTrampoline, void* pDetour)
    {
        HMODULE hMod = GetModuleHandleA(moduleName);
        if (!hMod) return;

        void* pProc = (void*)GetProcAddress(hMod, "XInputGetState");
        if (pProc) {
            // Check if already hooked (simple check if trampoline is set)
            if (*pTrampoline != nullptr) return;

            if (MH_CreateHook(pProc, pDetour, (void**)pTrampoline) == MH_OK &&
                MH_EnableHook(pProc) == MH_OK) {
                LOG_INFO("XInput: Hooked XInputGetState in %s.", moduleName);
            } else {
                LOG_ERROR("XInput: Failed to hook XInputGetState in %s.", moduleName);
            }
        }
    }

    void InitXInput()
    {
        // 1. Ensure xinput1_3.dll is loaded (Game Dependency)
        if (!GetModuleHandleA("xinput1_3.dll")) {
            LoadLibraryA("xinput1_3.dll");
        }

        // 2. Attempt to hook ALL present versions
        // This covers the game (1.3) and any plugins that might have pulled in newer versions (1.4, 9.1.0)
        HookXInputModule("xinput1_3.dll", &oXInputGetState_1_3, hkXInputGetState_1_3);
        HookXInputModule("xinput1_4.dll", &oXInputGetState_1_4, hkXInputGetState_1_4);
        HookXInputModule("xinput9_1_0.dll", &oXInputGetState_9_1_0, hkXInputGetState_9_1_0);
        
        if (!oXInputGetState_1_3 && !oXInputGetState_1_4 && !oXInputGetState_9_1_0) {
             LOG_ERROR("XInput: No XInput modules were successfully hooked!");
        }
    }
}}