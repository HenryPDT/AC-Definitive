#pragma once
#include <cstdint>
#include <Xinput.h>
#include "../EaglePatch.h"

namespace AC2EaglePatch
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

            void** vtable;              // 0x00
            int m_Flags;                // 0x04
            ButtonStates m_LastFrame;   // 0x08 -> 0x18
            ButtonStates m_ThisFrame;   // 0x18 -> 0x28
            uint64_t m_LastFrameTimeStamp; // 0x28 -> 0x30
            uint64_t m_ThisFrameTimeStamp; // 0x30 -> 0x38
            uint64_t m_ButtonPressTimeStamp[NbButtons]; // 0x38 -> 0xB8
            uint64_t m_LastFrameEngineTimeStamp; // 0xB8 -> 0xC0
            uint64_t m_ThisFrameEngineTimeStamp; // 0xC0 -> 0xC8
            uint64_t m_ButtonPressEngineTimeStamp[NbButtons]; // 0xC8 -> 0x148
            AnalogButtonStates m_ButtonValues; // 0x148 -> 0x188
            
            char _pad1[8];          // 0x188 -> 0x190 (Align LeftStick)
            StickState LeftStick;   // 0x190 -> 0x198
            char _pad2[8];          // 0x198 -> 0x1A0 (Align RightStick)
            StickState RightStick;  // 0x1A0 -> 0x1A8
            char _pad3[8];          // 0x1A8 -> 0x1B0 (Gap to field_1B0)
            
            int field_1B0[17];     // 0x1B0 -> 0x1F4
            float* vibrationData;  // 0x1F4 -> 0x1F8
            char field_1F8[0x390]; // 0x1F8 -> 0x588
            char _pad_align[8];    // 0x588 -> 0x590 (Sizeof Pad)

            void UpdatePad(InputBindings* a);
            void UpdateTimeStamps();
            void ScaleStickValues();

            bool IsEmpty() const {
                return m_ThisFrame.IsEmpty() && LeftStick.IsEmpty() && RightStick.IsEmpty();
            }
        };
        static_assert(sizeof(Pad) == 0x590, "Pad size mismatch");

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
        // PadXenon adds 0x20 bytes (0x590 -> 0x5B0)
        static_assert(sizeof(PadXenon) == 0x5B0, "PadXenon size mismatch");

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
            // Pad is 0x590
            int field_590;        // 0x590 -> 0x594
            uint32_t selectedPad; // 0x594 -> 0x598
            int field_598;        // 0x598 -> 0x59C
            
            scimitar::PadData pads[NbPadSets]; // 0x59C -> 0x165C
            char _tail_padding[4]; // 0x165C -> 0x1660 (Align to 16 bytes)

            bool AddPad(scimitar::Pad* a, PadType b, const wchar_t* c, uint16_t d, uint16_t e);
            void Update(); // We hook this
        };
        static_assert(sizeof(PadProxyPC) == 0x1660, "PadProxyPC size mismatch");
    }

    // --- Engine Allocator Interfaces ---
    namespace Gear
    {
        class MemHook
        {
        public:
            static MemHook*** pRef;
            static MemHook* GetRef() { return **pRef; }
            virtual void _0() = 0;
            virtual void* Alloc(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*) = 0;
            virtual void _8() = 0;
            virtual void _12() = 0;
            virtual void _16() = 0;
            virtual void Free(int, void*, uint32_t, void*, const char*) = 0;
        };
    }

    // Function pointer typedefs
    using t_ac_getNewDescriptor = void*(__cdecl*)(uint32_t, uint32_t, uint32_t);
    using t_ac_getDeleteDescriptor = uint32_t(__thiscall*)(void*, void*);
    using t_ac_allocate = void*(__cdecl*)(int, uint32_t, void*, const void*, const char*, const char*, uint32_t, const char*);
    using t_ac_delete = void(__cdecl*)(void*, void*, const char*);
}