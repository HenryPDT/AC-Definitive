#include "../EaglePatch.h"
#include "Controller.h"
#include <AutoAssemblerKinda.h>
#include <cstring>

namespace ACREaglePatch
{
    scimitar::PadProxyPC* pPad = nullptr;
    scimitar::PadXenon* padXenon = nullptr;
    
    // Configurable Keyboard Layout (Default to 0 = KeyboardMouse1)
    int NEEDED_KEYBOARD_SET = scimitar::PadSets::Keyboard1;

    namespace
    {
        // Helper to find the first connected XInput controller for hotplugging
        int GetActiveXInputIndex()
        {
            XINPUT_STATE state;
            for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
            {
                if (XInputGetState(i, &state) == ERROR_SUCCESS)
                    return (int)i;
            }
            return -1;
        }
    }

    // --- Globals & Addresses ---
    t_ac_getNewDescriptor ac_getNewDescriptor = nullptr;
    t_ac_getDeleteDescriptor ac_getDeleteDescriptor = nullptr;
    Gear::MemHook*** Gear::MemHook::pRef = nullptr;

    struct sAddresses {
        static uintptr_t Pad_UpdateTimeStamps;
        static uintptr_t Pad_ScaleStickValues;
        static uintptr_t PadXenon_ctor;
        static uintptr_t PadProxyPC_AddPad;
        static uintptr_t _addXenonJoy_Patch;
        static uintptr_t _addXenonJoy_JumpOut;
        static uintptr_t _PadProxyPC_Patch;
        static uint32_t* _descriptor_var;
        static void** _delete_class;
    };

    uintptr_t sAddresses::Pad_UpdateTimeStamps = 0;
    uintptr_t sAddresses::Pad_ScaleStickValues = 0;
    uintptr_t sAddresses::PadXenon_ctor = 0;
    uintptr_t sAddresses::PadProxyPC_AddPad = 0;
    uintptr_t sAddresses::_addXenonJoy_Patch = 0;
    uintptr_t sAddresses::_addXenonJoy_JumpOut = 0;
    uintptr_t sAddresses::_PadProxyPC_Patch = 0;
    uint32_t* sAddresses::_descriptor_var = nullptr;
    void** sAddresses::_delete_class = nullptr;

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            switch (version)
            {
            case GameVersion::Version1:
                sAddresses::Pad_UpdateTimeStamps = baseAddr + 0x017BA0F0;
                sAddresses::Pad_ScaleStickValues = baseAddr + 0x017BAB20;
                sAddresses::PadXenon_ctor = baseAddr + 0x01793AA0;
                sAddresses::PadProxyPC_AddPad = baseAddr + 0x01829C10;
                sAddresses::_addXenonJoy_Patch = baseAddr + 0x01793875;
                sAddresses::_addXenonJoy_JumpOut = baseAddr + 0x0179389B;
                sAddresses::_PadProxyPC_Patch = baseAddr + 0x01829230;
                sAddresses::_descriptor_var = (uint32_t*)(baseAddr + 0x025DF110);
                sAddresses::_delete_class = (void**)(baseAddr + 0x025DB20C);
                ac_getNewDescriptor = (t_ac_getNewDescriptor)(baseAddr + 0x01797C10);
                ac_getDeleteDescriptor = (t_ac_getDeleteDescriptor)(baseAddr + 0x0176FD60);
                Gear::MemHook::pRef = (Gear::MemHook***)(baseAddr + 0x025DB208);
                break;

            case GameVersion::Version2:
                sAddresses::Pad_UpdateTimeStamps = baseAddr + 0x017ED6B0;
                sAddresses::Pad_ScaleStickValues = baseAddr + 0x017EE0E0;
                sAddresses::PadXenon_ctor = baseAddr + 0x017C7240;
                sAddresses::PadProxyPC_AddPad = baseAddr + 0x0185D1E0;
                sAddresses::_addXenonJoy_Patch = baseAddr + 0x017C7015;
                sAddresses::_addXenonJoy_JumpOut = baseAddr + 0x017C703B;
                sAddresses::_PadProxyPC_Patch = baseAddr + 0x0185C800;
                sAddresses::_descriptor_var = (uint32_t*)(baseAddr + 0x028D1C40);
                sAddresses::_delete_class = (void**)(baseAddr + 0x028CDD3C);
                ac_getNewDescriptor = (t_ac_getNewDescriptor)(baseAddr + 0x017CB3B0);
                ac_getDeleteDescriptor = (t_ac_getDeleteDescriptor)(baseAddr + 0x017A34F0);
                Gear::MemHook::pRef = (Gear::MemHook***)(baseAddr + 0x028CDD38);
                break;

            default:
                return false;
            }
            return true;
        }
    }

    // --- Allocators ---
    void* ac_allocate_wrapper(int a1, uint32_t a2, void* a3, const void* a4, const char* a5, const char* a6, uint32_t a7, const char* a8) {
        return Gear::MemHook::GetRef()->Alloc(a1, a2, a3, a4, a5, a6, a7, a8);
    }
    void ac_delete_wrapper(void* ptr, void* a2, const char* a3) {
        if (ptr) {
            uint32_t descr = ac_getDeleteDescriptor(*sAddresses::_delete_class, ptr);
            void** vtable = *(void***)ptr;
            auto dtor = (void(__thiscall*)(void*, int))vtable[0];
            dtor(ptr, 0);
            Gear::MemHook::GetRef()->Free(5, ptr, descr, a2, a3);
        }
    }

    // --- Wrapper Calls ---
    void scimitar::Pad::UpdatePad(InputBindings* a) {
        ((void(__thiscall*)(Pad*, InputBindings*))vtable[12])(this, a);
    }
    void scimitar::Pad::UpdateTimeStamps() { ((void(__thiscall*)(Pad*))sAddresses::Pad_UpdateTimeStamps)(this); }
    void scimitar::Pad::ScaleStickValues() { ((void(__thiscall*)(Pad*))sAddresses::Pad_ScaleStickValues)(this); }
    scimitar::PadXenon* scimitar::PadXenon::_ctor(uint32_t padId) {
        return ((PadXenon * (__thiscall*)(PadXenon*, uint32_t))sAddresses::PadXenon_ctor)(this, padId);
    }
    bool scimitar::PadProxyPC::AddPad(scimitar::Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e, int64_t f, int64_t g) {
        return ((bool(__thiscall*)(PadProxyPC*, scimitar::Pad*, PadType, const wchar_t*, uint16_t, uint16_t, int64_t, int64_t))sAddresses::PadProxyPC_AddPad)(this, a, b, c, d, e, f, g);
    }
    void scimitar::PadXenon::operator_new(size_t size, void** out) {
        *out = ac_allocate_wrapper(2, sizeof(PadXenon), ac_getNewDescriptor(sizeof(PadXenon), 16, *sAddresses::_descriptor_var), nullptr, nullptr, nullptr, 0, nullptr);
    }

    // --- Injection Function ---
    void __cdecl AddXenonPad()
    {
        // Prevent double initialization
        if (padXenon || !pPad) return;

        void* memory = nullptr;
        scimitar::PadXenon::operator_new(sizeof(scimitar::PadXenon), &memory);
        if (memory) {
            padXenon = (scimitar::PadXenon*)memory;

            // Initialize with the first active controller found, defaulting to 0
            int activeIndex = GetActiveXInputIndex();
            padXenon->_ctor(activeIndex != -1 ? activeIndex : 0);

            // We add it, but we will manually manage its updates in the proxy hook to control the mapping
            if (pPad->AddPad(padXenon, scimitar::Pad::PadType::XenonPad, L"XInput Controller 1", 5, 5, 0, 0)) {
                if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] XInput Controller successfully injected.");
            } else {
                // Failed to add pad, cleanup memory to prevent leak
                ac_delete_wrapper(padXenon, nullptr, nullptr);
                padXenon = nullptr;
            }
        }
    }

    // --- Core Logic Hook: PadProxyPC::Update ---
    void __fastcall Hook_PadProxyPC_Update(scimitar::PadProxyPC* thisPtr, void* /*edx*/)
    {
        // Initialization check
        if (!pPad) pPad = thisPtr;

        // --- 1. Enforce Single Controller / Remove DInput ---
        // If original controllers were added (e.g. plugin loaded late), remove them.
        // We strictly allow only our injected padXenon in the Joy slots.
        for (int i = scimitar::PadSets::Joy1; i <= scimitar::PadSets::Joy3; ++i) {
            if (thisPtr->pads[i].pad && thisPtr->pads[i].pad != padXenon) {
                thisPtr->pads[i].pad = nullptr; // Detach unwanted controller
            }
        }

        if (!padXenon) AddXenonPad();

        // Ensure padXenon is in Joy1 slot (and only Joy1)
        if (padXenon && thisPtr->pads[scimitar::PadSets::Joy1].pad != padXenon) {
            thisPtr->pads[scimitar::PadSets::Joy1].pad = padXenon;
        }

        // --- 2. Hotplugging Support ---
        if (padXenon) {
            static DWORD lastScanTime = 0;
            DWORD currentTime = GetTickCount();

            // Check connectivity every 1000ms
            if (currentTime - lastScanTime > 1000) {
                lastScanTime = currentTime;

                // Direct XInput check is more reliable than game internal flags
                XINPUT_STATE dummy;
                bool isConnected = (XInputGetState(padXenon->m_PadIndex, &dummy) == ERROR_SUCCESS);

                if (!isConnected) {
                    int activeIndex = GetActiveXInputIndex();
                    if (activeIndex != -1) padXenon->m_PadIndex = activeIndex;
                }
            }
        }

        // --- 3. Update Input State ---
        // Force the game to look at Keyboard or Joy1.
        if (thisPtr->selectedPad != NEEDED_KEYBOARD_SET && thisPtr->selectedPad != scimitar::PadSets::Joy1)
            thisPtr->selectedPad = NEEDED_KEYBOARD_SET;

        scimitar::Pad* kbPad = thisPtr->pads[NEEDED_KEYBOARD_SET].pad;
        scimitar::Pad* joyPad = thisPtr->pads[scimitar::PadSets::Joy1].pad; // Should be padXenon

        if (kbPad) kbPad->UpdatePad(thisPtr->pads[NEEDED_KEYBOARD_SET].pInputBindings);
        if (joyPad) joyPad->UpdatePad(thisPtr->pads[scimitar::PadSets::Joy1].pInputBindings);

        // --- 4. Simultaneous Input Merge ---

        // Save previous state for edge detection logic in game
        thisPtr->m_LastFrame = thisPtr->m_ThisFrame;

        // Reset current frame state
        memset(&thisPtr->m_ThisFrame, 0, sizeof(thisPtr->m_ThisFrame));
        memset(&thisPtr->m_ButtonValues, 0, sizeof(thisPtr->m_ButtonValues));
        memset(&thisPtr->LeftStick, 0, sizeof(thisPtr->LeftStick));
        memset(&thisPtr->RightStick, 0, sizeof(thisPtr->RightStick));

        bool isJoyActive = false;
        bool isKbActive = false;

        // Lambda to accumulate inputs from a source pad
        auto AccumulatePad = [&](scimitar::Pad* src, bool isJoy)
        {
            if (!src) return false;
            bool active = false;

            // Merge Buttons
            for (int i = 0; i < scimitar::Pad::NbButtons; ++i)
            {
                bool down = src->m_ThisFrame.state[i];
                float val = src->m_ButtonValues.state[i];

                // KBM usually has 0.0f for button values, simulate 1.0f if pressed
                if (!isJoy && down && val <= 0.0f) val = 1.0f;

                if (down) {
                    thisPtr->m_ThisFrame.state[i] = true;
                    active = true;
                }
                // Take max analog value (e.g. triggers)
                if (val > thisPtr->m_ButtonValues.state[i]) {
                    thisPtr->m_ButtonValues.state[i] = val;
                }
            }

            // Merge Sticks (Add together for simultaneous movement/look)
            if (src->LeftStick.x != 0.0f || src->LeftStick.y != 0.0f) {
                thisPtr->LeftStick.x += src->LeftStick.x;
                thisPtr->LeftStick.y += src->LeftStick.y;
                active = true;
            }
            if (src->RightStick.x != 0.0f || src->RightStick.y != 0.0f) {
                thisPtr->RightStick.x += src->RightStick.x;
                thisPtr->RightStick.y += src->RightStick.y;
                active = true;
            }
            return active;
        };

        if (joyPad) isJoyActive = AccumulatePad(joyPad, true);
        if (kbPad) isKbActive = AccumulatePad(kbPad, false);

        // Clamp ONLY Movement Stick (Left) to valid range [-1.0, 1.0]
        // Do NOT clamp Camera Stick (Right) because Mouse uses unbounded deltas.
        auto ClampStick = [](float& v) { if (v > 1.0f) v = 1.0f; else if (v < -1.0f) v = -1.0f; };
        ClampStick(thisPtr->LeftStick.x);
        ClampStick(thisPtr->LeftStick.y);

        // Priority Logic:
        // If Mouse/Keyboard is being used, set Pad to Keyboard to ensure Mouse Deltas are read correctly.
        // Otherwise, default to Controller (if active) to get Analog Camera behavior and Xbox Prompts.
        if (isKbActive) thisPtr->selectedPad = NEEDED_KEYBOARD_SET;
        else if (isJoyActive) thisPtr->selectedPad = scimitar::PadSets::Joy1;

        thisPtr->UpdateTimeStamps();
    }

    // --- ASM Hooks ---
    struct AddXenonJoyHook : AutoAssemblerCodeHolder_Base
    {
        AddXenonJoyHook() {
            DEFINE_ADDR(hook_addr, sAddresses::_addXenonJoy_Patch);
            DEFINE_ADDR(ret_addr, sAddresses::_addXenonJoy_JumpOut);
            DEFINE_ADDR(fnAddXenonPad, (uintptr_t)&AddXenonPad);
            DEFINE_ADDR(var_pPad, (uintptr_t)&pPad);
            
            ALLOC(newmem, 128, sAddresses::_addXenonJoy_Patch);

            hook_addr = { db(0xE9), RIP(newmem) };
            newmem = {
                "8B 40 04",                 // mov eax, [eax+4]
                "A3", ABS(var_pPad, 4),     // mov [pPad], eax
                "60",                       // pushad
                "9C",                       // pushfd
                "E8", RIP(fnAddXenonPad),   // call AddXenonPad
                "9D",                       // popfd
                "61",                       // popad
                "E9", RIP(ret_addr)
            };
        }
    };
    
    struct PadProxyUpdateHook : AutoAssemblerCodeHolder_Base
    {
        PadProxyUpdateHook() {
            DEFINE_ADDR(hook_addr, sAddresses::_PadProxyPC_Patch);
            DEFINE_ADDR(fnUpdate, (uintptr_t)&Hook_PadProxyPC_Update);
            hook_addr = { "E9", RIP(fnUpdate) };
        }
    };

    void InitController(uintptr_t baseAddr, GameVersion version)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        static AutoAssembleWrapper<AddXenonJoyHook> hook1;
        hook1.Activate();

        static AutoAssembleWrapper<PadProxyUpdateHook> hook2;
        hook2.Activate();
        
        if (g_loader_ref) g_loader_ref->LogToConsole("[EaglePatch] Controller patches applied.");
    }
}
