#include "pch.h"
#include "base.h"
#include <xinput.h>

typedef DWORD(WINAPI* XInputGetState_t)(DWORD, XINPUT_STATE*);
static XInputGetState_t oXInputGetState = nullptr;


DWORD WINAPI hkXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    if (!pState)
        return ERROR_BAD_ARGUMENTS;

    XINPUT_STATE virtualState{};
    bool haveVirtual = BaseHook::Hooks::TryGetVirtualXInputState(dwUserIndex, &virtualState);

    DWORD result = ERROR_DEVICE_NOT_CONNECTED;
    if (!haveVirtual || dwUserIndex != 0)
    {
        if (oXInputGetState)
            result = oXInputGetState(dwUserIndex, pState);
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

namespace BaseHook { namespace Hooks {
    void InitXInput()
    {
        // Try to find loaded XInput module first
        const char* names[] = { "xinput1_3.dll", "xinput1_4.dll", "xinput9_1_0.dll" };
        HMODULE hXInput = nullptr;
        
        for (const char* name : names) {
            hXInput = GetModuleHandleA(name);
            if (hXInput) {
                break;
            }
        }

        // If not loaded, try to load 1.3 (common for older games like AC)
        if (!hXInput) {
            hXInput = LoadLibraryA("xinput1_3.dll");
        }

        if (hXInput)
        {
            void* pProc = (void*)GetProcAddress(hXInput, "XInputGetState");
            if (pProc) {
                if (MH_CreateHook(pProc, hkXInputGetState, (void**)&oXInputGetState) == MH_OK &&
                    MH_EnableHook(pProc) == MH_OK) {
                    LOG_INFO("XInput: Hooked XInputGetState.");
                } else {
                    LOG_ERROR("XInput: Failed to hook XInputGetState.");
                }
            } else {
                LOG_ERROR("XInput: XInputGetState export not found.");
            }
        } else {
            LOG_ERROR("XInput: Could not find or load any XInput DLL.");
        }
    }
}}