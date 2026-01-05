#include "EaglePatch.h"
#include "Controller.h"
#include <AutoAssemblerKinda.h>
#include <PatternScanner.h>
#include <cstring>
#include "log.h"

using namespace AutoAssemblerKinda;

namespace AC1EaglePatch
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
    t_ac_allocate ac_allocate = nullptr;
    t_ac_delete ac_delete = nullptr;

    struct sAddresses {
        static uintptr_t Pad_UpdateTimeStamps;
        static uintptr_t Pad_ScaleStickValues;
        static uintptr_t PadXenon_ctor;
        static uintptr_t PadProxyPC_AddPad;
        static uint32_t* _descriptor_var;
    };

    uintptr_t sAddresses::Pad_UpdateTimeStamps = 0;
    uintptr_t sAddresses::Pad_ScaleStickValues = 0;
    uintptr_t sAddresses::PadXenon_ctor = 0;
    uintptr_t sAddresses::PadProxyPC_AddPad = 0;
    uint32_t* sAddresses::_descriptor_var = nullptr;

    // --- Address Definitions ---

    // 1. Pad_ScaleStickValues & Pad_UpdateTimeStamps
    // Pattern: mov ecx,[esi+500h]; lea eax,[esp+0Ch]
    DEFINE_ADDRESS(Pad_BasePattern1, "8B 8E 00 05 00 00 8D", 0, RAW, nullptr);
    DEFINE_ADDRESS(Pad_ScaleStickValues, "@Pad_BasePattern1", 0x23, CALL, &sAddresses::Pad_ScaleStickValues);
    DEFINE_ADDRESS(Pad_UpdateTimeStamps, "@Pad_BasePattern1", 0x2A, CALL, &sAddresses::Pad_UpdateTimeStamps);

    // 2. AddXenonJoy Hook Location
    // Pattern: mov eax,[esi+04]; mov ecx,[eax]; mov edx,[ecx]; push 1
    // We use DEFINE_AOB_HOOK directly with the signature below.

    // 3. PadProxyPC Hook Location
    // Pattern: push esi; mov esi,ecx; mov eax,[esi+504h]; cmp eax,04
    // We use DEFINE_AOB_HOOK directly with the signature below.

    // 4. PadXenon_ctor, Allocators, Descriptors
    // Pattern: push ecx; push 10h; push 520h
    DEFINE_ADDRESS(Pad_BasePattern4, "51 6A 10 68 20 05 00 00", 0, RAW, nullptr);

    // Offset -0x06: mov ecx, [_descriptor_var] (Opcode 8B 0D + Address) -> Disp32 at -0x04
    DEFINE_ADDRESS(DescVar, "@Pad_BasePattern4", -0x04, DEREF, (uintptr_t*)&sAddresses::_descriptor_var);

    // Offset 0x08: call ac_getNewDescriptor
    DEFINE_ADDRESS(GetNewDesc, "@Pad_BasePattern4", 0x08, CALL, (uintptr_t*)&ac_getNewDescriptor);

    // Offset 0x1A: call ac_allocate
    DEFINE_ADDRESS(AcAlloc, "@Pad_BasePattern4", 0x1A, CALL, (uintptr_t*)&ac_allocate);

    // Offset 0x38: call PadXenon_ctor
    DEFINE_ADDRESS(XenonCtor, "@Pad_BasePattern4", 0x38, CALL, &sAddresses::PadXenon_ctor);

    // Offset 0x5B: call PadProxyPC_AddPad
    DEFINE_ADDRESS(ProxyAddPad, "@Pad_BasePattern4", 0x5B, CALL, &sAddresses::PadProxyPC_AddPad);

    // Offset 0x69: call ac_delete
    DEFINE_ADDRESS(AcDel, "@Pad_BasePattern4", 0x69, CALL, (uintptr_t*)&ac_delete);


    // Forward declaration
    void __cdecl AddXenonPad();

    // --- Hooks ---

    // 2. AddXenonJoy Hook
    // Pattern: mov eax,[esi+04]; mov ecx,[eax]; mov edx,[ecx]; push 1
    // "8B 46 04 8B 08 8B 11 6A"
    DEFINE_AOB_HOOK(AddXenonJoy, "8B 46 04 8B 08 8B 11 6A", 0, 5);
    DEFINE_EXITS(AddXenonJoy, Exit, 0x17);

    HOOK_IMPL(AddXenonJoy) {
        __asm {
            mov eax, [eax + 0x04]
            mov [pPad], eax
            pushad
            pushfd
            call AddXenonPad
            popfd
            popad
            jmp [AddXenonJoy_Exit]
        }
    }



    // --- Allocators (Wrappers to match AC2/ACB/ACR structure) ---
    void* ac_allocate_wrapper(int a1, uint32_t a2, void* a3, const void* a4, const char* a5, const char* a6, uint32_t a7, const char* a8) {
        if (ac_allocate) return ac_allocate(a1, a2, a3, a4, a5, a6, a7, a8);
        return nullptr;
    }
    void ac_delete_wrapper(void* ptr, void* a2, const char* a3) {
        if (ac_delete) ac_delete(ptr, a2, a3);
    }
}

namespace AC1::Scimitar {
    // --- Wrapper Calls ---
    void Pad::UpdatePad(InputBindings* a) {
        ((void(__thiscall*)(Pad*, InputBindings*))vtable[10])(this, a);
    }
    void Pad::UpdateTimeStamps() { ((void(__thiscall*)(Pad*))AC1EaglePatch::sAddresses::Pad_UpdateTimeStamps)(this); }
    void Pad::ScaleStickValues() { ((void(__thiscall*)(Pad*))AC1EaglePatch::sAddresses::Pad_ScaleStickValues)(this); }
    PadXenon* PadXenon::_ctor(uint32_t padId) {
        return ((PadXenon * (__thiscall*)(PadXenon*, uint32_t))AC1EaglePatch::sAddresses::PadXenon_ctor)(this, padId);
    }
    bool PadProxyPC::AddPad(Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e) {
        return ((bool(__thiscall*)(PadProxyPC*, Pad*, PadType, const wchar_t*, uint16_t, uint16_t))AC1EaglePatch::sAddresses::PadProxyPC_AddPad)(this, a, b, c, d, e);
    }
    void PadXenon::operator_new(size_t size, void** out) {
        *out = AC1EaglePatch::ac_allocate_wrapper(2, sizeof(PadXenon), AC1EaglePatch::ac_getNewDescriptor(sizeof(PadXenon), 16, *AC1EaglePatch::sAddresses::_descriptor_var), nullptr, nullptr, nullptr, 0, nullptr);
    }
}

namespace AC1EaglePatch
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
            if (pPad->AddPad(padXenon, scimitar::Pad::PadType::XenonPad, L"XInput Controller 1", 5, 5)) {
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
        for (int i = scimitar::PadSets::Joy1; i <= scimitar::PadSets::Joy4; ++i) {
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
    
    // --- 3. Hook_PadProxyPC_Update Hook Definition ---
    // Pattern: push esi; mov esi,ecx; mov eax,[esi+504h]; cmp eax,04
    // "56 8B F1 8B 86 04 05 00 00 83"
    DEFINE_AOB_HOOK(PadProxyPC, "56 8B F1 8B 86 04 05 00 00 83", 0, 5);

    HOOK_IMPL(PadProxyPC)
    {
        __asm {
            jmp Hook_PadProxyPC_Update
        }
    }

    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout)
    {
        // Resolve all addresses and hooks
        if (!HookManager::ResolveAndInstallAll())
        {
             LOG_ERROR("[EaglePatch] Failed to resolve Controller patches!");
             return;
        }

        // Set the global keyboard layout variable used in Hook_PadProxyPC_Update
        NEEDED_KEYBOARD_SET = keyboardLayout;

        LOG_INFO("[EaglePatch] Controller patches applied.");
    }

    void UpdateKeyboardLayout(int keyboardLayout)
    {
        NEEDED_KEYBOARD_SET = keyboardLayout;
    }
}
