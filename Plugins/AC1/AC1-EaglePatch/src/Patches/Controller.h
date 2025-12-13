#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"

namespace AC1EaglePatch
{
    void InitController(uintptr_t baseAddr, GameVersion version, int keyboardLayout);

    // --- Game Structures (scimitar) ---
    namespace scimitar
    {
        class InputBindings;

        struct Pad
        {
            enum PadType { MouseKeyboardPad = 0, PCPad, XenonPad, PS2Pad };
            enum PadButton {
                Button1, Button2, Button3, Button4,
                PadDown, PadLeft, PadUp, PadRight,
                Select, Start,
                ShoulderLeft1, ShoulderLeft2, ShoulderRight1, ShoulderRight2,
                StickLeft, StickRight,
                NbButtons
            };

            struct ButtonStates {
                bool state[NbButtons];
                bool IsEmpty() const {
                    for (int i = 0; i < NbButtons; i++) if (state[i]) return false;
                    return true;
                }
            };
            struct AnalogButtonStates { float state[NbButtons]; };
            
            struct StickState {
                float x, y;
                bool IsEmpty() const { return x == 0.0f && y == 0.0f; }
            };

            void** vtable;          // 0x00
            int m_Flags;            // 0x04
            ButtonStates m_LastFrame; // 0x08 -> 0x18
            ButtonStates m_ThisFrame; // 0x18 -> 0x28
            uint64_t m_LastFrameTimeStamp; // 0x28 -> 0x30
            uint64_t m_ThisFrameTimeStamp; // 0x30 -> 0x38
            uint64_t m_ButtonPressTimeStamp[NbButtons]; // 0x38 -> 0xB8
            AnalogButtonStates m_ButtonValues; // 0xB8 -> 0xF8

            char _pad1[8];          // 0xF8 -> 0x100 (Align LeftStick)
            StickState LeftStick;   // 0x100 -> 0x108
            char _pad2[8];          // 0x108 -> 0x110 (Align RightStick)
            StickState RightStick;  // 0x110 -> 0x118

            char _gap_118[0x98];    // 0x118 -> 0x1B0
            int field_1B0[17];      // 0x1B0 -> 0x1F4
            float* vibrationData;   // 0x1F4 -> 0x1F8
            char field_1F8[0x308];  // 0x1F8 -> 0x500

            void UpdatePad(InputBindings* a);
            void UpdateTimeStamps();
            void ScaleStickValues();

            bool IsEmpty() const {
                return m_ThisFrame.IsEmpty() && LeftStick.IsEmpty() && RightStick.IsEmpty();
            }
        };
        static_assert(sizeof(Pad) == 0x500, "Pad size mismatch");

        struct PadXenon : Pad
        {
            struct PadState
            {
                XINPUT_CAPABILITIES Caps;
                bool Connected;
                bool Inserted;
                bool Removed;
            };

            uint32_t m_PadIndex;
            PadState m_PadState;

            PadXenon* _ctor(uint32_t padId);
            static void operator_new(size_t size, void** out);
        };
        // PadXenon adds 0x20 bytes (0x500 -> 0x520)
        static_assert(sizeof(PadXenon) == 0x520, "PadXenon size mismatch");

        struct PadData
        {
            scimitar::Pad* pad;
            char field_4[528];
            InputBindings* pInputBindings;
        };
        // 4 + 528 + 4 = 536 (0x218)

        enum PadSets {
            Keyboard1 = 0, Keyboard2, Keyboard3, Keyboard4,
            Joy1, Joy2, Joy3, Joy4,
            NbPadSets
        };

        struct PadProxyPC : Pad
        {
            // Pad is 0x500
            int field_500;        // 0x500 -> 0x504
            uint32_t selectedPad; // 0x504 -> 0x508
            int field_508;        // 0x508 -> 0x50C

            scimitar::PadData pads[NbPadSets]; // 0x50C -> 0x15CC
            char _tail_padding[4]; // 0x15CC -> 0x15D0 (Align to 16 bytes)

            bool AddPad(scimitar::Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e);
            void Update(); // We hook this
        };
        static_assert(sizeof(PadProxyPC) == 0x15D0, "PadProxyPC size mismatch");
    }

    // Function pointer typedefs
    using t_ac_getNewDescriptor = void* (__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_allocate = void* (__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}