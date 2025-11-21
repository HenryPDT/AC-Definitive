#include "pch.h"
#include "base.h"
#include <xinput.h>

typedef DWORD(WINAPI* XInputGetState_t)(DWORD, XINPUT_STATE*);
static XInputGetState_t oXInputGetState = nullptr;


DWORD WINAPI hkXInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    DWORD result = oXInputGetState(dwUserIndex, pState);
    
    if (result == ERROR_SUCCESS && BaseHook::Data::bIsInitialized)
    {
        if (BaseHook::Data::bBlockInput)
        {
            // Zero out the state so the game sees no input
            memset(pState, 0, sizeof(XINPUT_STATE));
        }
    }
    return result;
}

namespace BaseHook { namespace Hooks {
    void InitXInput()
    {
        LOG_INFO("XInput: Initializing hooks...");

        // Try to find loaded XInput module first
        const char* names[] = { "xinput1_4.dll", "xinput9_1_0.dll", "xinput1_3.dll" };
        HMODULE hXInput = nullptr;
        
        for (const char* name : names) {
            hXInput = GetModuleHandleA(name);
            if (hXInput) {
                LOG_INFO("XInput: Found loaded %s", name);
                break;
            }
        }

        // If not loaded, try to load 1.3 (common for older games like AC)
        if (!hXInput) {
            hXInput = LoadLibraryA("xinput1_3.dll");
            if (hXInput) LOG_INFO("XInput: Loaded xinput1_3.dll");
        }

        if (hXInput)
        {
            void* pProc = (void*)GetProcAddress(hXInput, "XInputGetState");
            if (pProc) {
                if (MH_CreateHook(pProc, hkXInputGetState, (void**)&oXInputGetState) == MH_OK &&
                    MH_EnableHook(pProc) == MH_OK) {
                    LOG_INFO("XInput: Hooked XInputGetState at %p", pProc);
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