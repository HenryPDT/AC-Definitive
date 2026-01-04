#include "EaglePatch.h"
#include "Controller.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include <cstring>
#include "log.h"

using namespace Utils;

// --- Static Member Definitions (RE Types) ---
ACR::Gear::MemHook*** ACR::Gear::MemHook::pRef = nullptr;

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

    struct sAddresses {
        static uintptr_t Pad_UpdateTimeStamps;
        static uintptr_t Pad_ScaleStickValues;
        static uintptr_t PadXenon_ctor;
        static uintptr_t PadProxyPC_AddPad;
        static uintptr_t _addXenonJoy_Patch;
        static uintptr_t _PadProxyPC_Patch;
        static uint32_t* _descriptor_var;
        static void** _delete_class;
    };

    uintptr_t sAddresses::Pad_UpdateTimeStamps = 0;
    uintptr_t sAddresses::Pad_ScaleStickValues = 0;
    uintptr_t sAddresses::PadXenon_ctor = 0;
    uintptr_t sAddresses::PadProxyPC_AddPad = 0;
    uintptr_t sAddresses::_addXenonJoy_Patch = 0;
    uintptr_t sAddresses::_PadProxyPC_Patch = 0;
    uint32_t* sAddresses::_descriptor_var = nullptr;
    void** sAddresses::_delete_class = nullptr;

    // Forward declaration
    void __cdecl AddXenonPad();

    // Hook Wrapper
    DEFINE_HOOK(AddXenonJoy_HookFunction, AddXenonJoy_Return) {
        __asm {
            mov eax, [eax + 0x04]
            mov [pPad], eax
            pushad
            pushfd
            call AddXenonPad
            popfd
            popad
            jmp [AddXenonJoy_Return]
        }
    }

    namespace
    {
        bool ResolveAddresses(uintptr_t baseAddr, GameVersion version)
        {
            // 1. Pad_ScaleStickValues & Pad_UpdateTimeStamps
            // Pattern: mov edx,[esi+5B0h]; lea ecx,[ebp-08]
            auto result1 = PatternScanner::ScanMain("8B 96 B0 05 00 00 8D");
            if (!result1) return false;

            // Offset 0x16: call Pad_ScaleStickValues (Relative Call)
            // Offset 0x1D: call Pad_UpdateTimeStamps (Relative Call)
            sAddresses::Pad_ScaleStickValues = result1.Offset(0x16).ResolveRelative().address;
            sAddresses::Pad_UpdateTimeStamps = result1.Offset(0x1D).ResolveRelative().address;

            // 2. _addXenonJoy_Patch
            // Pattern: mov eax,[esi+04]; mov ecx,[eax]; mov edx,[ecx]; push 1
            auto result2 = PatternScanner::ScanMain("8B 46 04 8B 08 8B 11 6A");
            if (!result2) return false;

            sAddresses::_addXenonJoy_Patch = result2.address;
            // JumpOut is at +0x26
            AddXenonJoy_Return = result2.address + 0x26;

            // 3. _PadProxyPC_Patch
            // Pattern: push esi; mov esi,ecx; mov eax,[esi+5B4h]; cmp eax,05
            auto result3 = PatternScanner::ScanMain("56 8B F1 8B 86 B4 05 00 00 83");
            if (!result3) return false;

            sAddresses::_PadProxyPC_Patch = result3.address;

            // 4. PadXenon_ctor, etc.
            // Pattern: push ecx; push 10h; push 5D0h
            auto result4 = PatternScanner::ScanMain("51 6A 10 68 D0 05 00 00");
            if (!result4) return false;
            
            // Offset -0x06: mov ecx, [_descriptor_var] (Opcode 8B 0D + Address)
            sAddresses::_descriptor_var = (uint32_t*)result4.Dereference(-0x06 + 0x02).address;

            // Offset 0x08: call ac_getNewDescriptor (Relative Call)
            ac_getNewDescriptor = result4.Offset(0x08).ResolveRelative().As<t_ac_getNewDescriptor>();

            // Offset 0x0D: mov edx, [pRef] (Opcode 8B 15 + Address)
            ACR::Gear::MemHook::pRef = (ACR::Gear::MemHook***)result4.Dereference(0x0D + 0x02).address;

            // Offset 0x42: call PadXenon_ctor (Relative Call)
            sAddresses::PadXenon_ctor = result4.Offset(0x42).ResolveRelative().address;

            // Offset 0x7B: call PadProxyPC_AddPad (Relative Call)
            sAddresses::PadProxyPC_AddPad = result4.Offset(0x7B).ResolveRelative().address;

            // Offset 0x88: mov ecx, [_delete_class] (Opcode 8B 0D + Address)
            sAddresses::_delete_class = (void**)result4.Dereference(0x88 + 0x02).address;

            // Offset 0x8F: call ac_getDeleteDescriptor (Relative Call)
            ac_getDeleteDescriptor = result4.Offset(0x8F).ResolveRelative().As<t_ac_getDeleteDescriptor>();

            return true;
        }
    }

    // --- Allocators ---
    void* ac_allocate_wrapper(int a1, uint32_t a2, void* a3, const void* a4, const char* a5, const char* a6, uint32_t a7, const char* a8) {
        return ACR::Gear::MemHook::GetRef()->Alloc(a1, a2, a3, a4, a5, a6, a7, a8);
    }
    void ac_delete_wrapper(void* ptr, void* a2, const char* a3) {
        if (ptr) {
            uint32_t descr = ac_getDeleteDescriptor(*sAddresses::_delete_class, ptr);
            void** vtable = *(void***)ptr;
            auto dtor = (void(__thiscall*)(void*, int))vtable[0];
            dtor(ptr, 0);
            ACR::Gear::MemHook::GetRef()->Free(5, ptr, descr, a2, a3);
        }
    }
}

// --- Wrapper Calls (Implementation for RE Types) ---
namespace ACR::Scimitar {
    void Pad::UpdatePad(InputBindings* a) {
        ((void(__thiscall*)(Pad*, InputBindings*))vtable[12])(this, a);
    }
    void Pad::UpdateTimeStamps() { ((void(__thiscall*)(Pad*))ACREaglePatch::sAddresses::Pad_UpdateTimeStamps)(this); }
    void Pad::ScaleStickValues() { ((void(__thiscall*)(Pad*))ACREaglePatch::sAddresses::Pad_ScaleStickValues)(this); }
    PadXenon* PadXenon::_ctor(uint32_t padId) {
        return ((PadXenon * (__thiscall*)(PadXenon*, uint32_t))ACREaglePatch::sAddresses::PadXenon_ctor)(this, padId);
    }
    bool PadProxyPC::AddPad(Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e, int64_t f, int64_t g) {
        return ((bool(__thiscall*)(PadProxyPC*, Pad*, PadType, const wchar_t*, uint16_t, uint16_t, int64_t, int64_t))ACREaglePatch::sAddresses::PadProxyPC_AddPad)(this, a, b, c, d, e, f, g);
    }
    void PadXenon::operator_new(size_t size, void** out) {
        *out = ACREaglePatch::ac_allocate_wrapper(2, sizeof(PadXenon), ACREaglePatch::ac_getNewDescriptor(sizeof(PadXenon), 16, *ACREaglePatch::sAddresses::_descriptor_var), nullptr, nullptr, nullptr, 0, nullptr);
    }
}

namespace ACREaglePatch
{
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
                LOG_INFO("[EaglePatch] XInput Controller successfully injected.");
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
            PresetScript_InjectJump(sAddresses::_addXenonJoy_Patch, (uintptr_t)&AddXenonJoy_HookFunction);
        }
    };
    
    struct PadProxyUpdateHook : AutoAssemblerCodeHolder_Base
    {
        PadProxyUpdateHook() {
            PresetScript_InjectJump(sAddresses::_PadProxyPC_Patch, (uintptr_t)&Hook_PadProxyPC_Update);
        }
    };

    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout)
    {
        if (!ResolveAddresses(baseAddr, version))
            return;

        NEEDED_KEYBOARD_SET = keyboardLayout;

        static AutoAssembleWrapper<AddXenonJoyHook> hook1;
        hook1.Activate();

        static AutoAssembleWrapper<PadProxyUpdateHook> hook2;
        hook2.Activate();
        
        LOG_INFO("[EaglePatch] Controller patches applied.");
    }

    void UpdateKeyboardLayout(int keyboardLayout)
    {
        NEEDED_KEYBOARD_SET = keyboardLayout;
    }
}