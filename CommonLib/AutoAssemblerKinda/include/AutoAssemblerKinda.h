#pragma once


/*
This little project is created to assist translating Cheat Engine's AutoAssembler-generated
code for x64 into C++.
THIS IS NOT AN ASSEMBLER.


Example usage:
In Cheat Engine's Auto Assembler:
        define(address,"ACU.exe"+1A4C618)
        define(bytes,49 8B 47 28 4C 8D 45 80)

        [ENABLE]

        assert(address,bytes)

        // `allocated_variable` could come from a different script, which means
        // the distance from it to `maybeSkipParkourScanner` hook could be different every time,
        // which means the generated code that depends on this variable could be different every time.
        alloc(allocated_variable,8,"ACU.exe")
        allocated_variable:
          dq 0

        alloc(maybeSkipParkourScanner,$1000,"ACU.exe"+1A4C618)
        label(return)


        newmem:
          cmp [allocated_variable], 0
          je return
          mov rax,[r15+28]
          lea r8,[rbp-80]
          lea rdx,[rbp-60]
          movaps xmm3,xmm7
          mov rcx,[rax]
          mov rax,[rcx+000001F0]
          mov rcx,[r15+00000208]
          mov [rsp+28],rax
          mov [rsp+20],edi
          call ACU.exe+1858D0
          mov rax,[r15+28]
          lea rcx,[rbp-70]
          lea r9,[rbp-80]
          mov rax,[rax]
          mov [rsp+40],rcx
          mov rcx,[r15+00000208]
          mov rax,[rax+000001F0]
          lea r8,[rbp-30]
          lea rdx,[rbp-60]
          mov [rsp+38],rax
          mov [rsp+30],r12l
          mov [rsp+28],edi
          movss [rsp+20],xmm7
          call ACU.exe+185630
          jmp return

        address:
          jmp maybeSkipParkourScanner
          nop 3
        return:

        [DISABLE]

        address:
          db bytes
        dealloc(maybeSkipParkourScanner)
        dealloc(allocated_variable)

        {
        // ORIGINAL CODE - INJECTION POINT: ACU.exe+1A4C618

        ACU.exe+1A4C60F: 0F 2F FE              - comiss xmm7,xmm6
        ACU.exe+1A4C612: 0F 86 83 00 00 00     - jbe ACU.exe+1A4C69B
        // ---------- INJECTING HERE ----------
        ACU.exe+1A4C618: 49 8B 47 28           - mov rax,[r15+28]
        // ---------- DONE INJECTING  ----------
        ACU.exe+1A4C61C: 4C 8D 45 80           - lea r8,[rbp-80]
        ACU.exe+1A4C620: 48 8D 55 A0           - lea rdx,[rbp-60]
        }
The above, when activated, turns into the following code (copied from Cheat Engine's Disassembler Window):
        ACU.exe+1A4C618 - E9 E33959FE           - jmp 13FFE0000
        ACU.exe+1A4C61D - 0F1F 00               - nop dword ptr [rax]
    and
        13FFE0000 - 83 3D F9FF0000 00     - cmp dword ptr [13FFF0000],00
        13FFE0007 - 0F84 13C6A601         - je ACU.exe+1A4C620
        13FFE000D - 49 8B 47 28           - mov rax,[r15+28]
        13FFE0011 - 4C 8D 45 80           - lea r8,[rbp-80]
        13FFE0015 - 48 8D 55 A0           - lea rdx,[rbp-60]
        13FFE0019 - 0F28 DF               - movaps xmm3,xmm7
        13FFE001C - 48 8B 08              - mov rcx,[rax]
        13FFE001F - 48 8B 81 F0010000     - mov rax,[rcx+000001F0]
        13FFE0026 - 49 8B 8F 08020000     - mov rcx,[r15+00000208]
        13FFE002D - 48 89 44 24 28        - mov [rsp+28],rax
        13FFE0032 - 89 7C 24 20           - mov [rsp+20],edi
        13FFE0036 - E8 95581A00           - call ACU.exe+1858D0
        13FFE003B - 49 8B 47 28           - mov rax,[r15+28]
        13FFE003F - 48 8D 4D 90           - lea rcx,[rbp-70]
        13FFE0043 - 4C 8D 4D 80           - lea r9,[rbp-80]
        13FFE0047 - 48 8B 00              - mov rax,[rax]
        13FFE004A - 48 89 4C 24 40        - mov [rsp+40],rcx
        13FFE004F - 49 8B 8F 08020000     - mov rcx,[r15+00000208]
        13FFE0056 - 48 8B 80 F0010000     - mov rax,[rax+000001F0]
        13FFE005D - 4C 8D 45 D0           - lea r8,[rbp-30]
        13FFE0061 - 48 8D 55 A0           - lea rdx,[rbp-60]
        13FFE0065 - 48 89 44 24 38        - mov [rsp+38],rax
        13FFE006A - 44 88 64 24 30        - mov [rsp+30],r12l
        13FFE006F - 89 7C 24 28           - mov [rsp+28],edi
        13FFE0073 - F3 0F11 7C 24 20      - movss [rsp+20],xmm7
        13FFE0079 - E8 B2551A00           - call ACU.exe+185630
        13FFE007E - E9 9DC5A601           - jmp ACU.exe+1A4C620
This can be turned into the following C++ code:
        struct ParkourScannerHook : AutoAssemblerCodeHolder_Base
        {
            ParkourScannerHook();
        };
        ParkourScannerHook::ParkourScannerHook()
        {
            ALLOC(allocated_variable, 8, 0x140000000);
            allocated_variable = {
                dq(0)
            };
            DEFINE_ADDR(maybeSkipParkourScanner, 0x141A4C618);
            DEFINE_ADDR(__fn1401858D0, 0x1401858D0);
            DEFINE_ADDR(__fn140185630, 0x140185630);
            LABEL(maybeSkipParkourScanner__return);
            ALLOC(maybeSkipParkourScanner__cave, 0x1000, 0x141A4C618);

            maybeSkipParkourScanner = {
                "E9", RIP(maybeSkipParkourScanner__cave),   // - jmp 13FFC0030                                  - needs to be adjusted - relative `jmp` between static and allocated addresses.
                "0F1F 00"                                   // - nop dword ptr [rax]
            };
            maybeSkipParkourScanner__cave = {
                "83 3D", RIP(allocated_variable, 5), "00"       // - cmp dword ptr [allocated_variable],00      - needs to be adjusted - relative `cmp` between two dynamic addresses; second parameter 5 == how many bytes until the end of the opcode.
                "0F84", RIP(maybeSkipParkourScanner__return),   // - je ACU.exe+1A4C620                         - needs to be adjusted - relative `je` between static and allocated addresses.
                "49 8B 47 28         "  // - mov rax,[r15+28]
                "4C 8D 45 80         "  // - lea r8,[rbp-80]
                "48 8D 55 A0         "  // - lea rdx,[rbp-60]
                "0F28 DF             "  // - movaps xmm3,xmm7
                "48 8B 08            "  // - mov rcx,[rax]
                "48 8B 81 F0010000   "  // - mov rax,[rcx+000001F0]
                "49 8B 8F 08020000   "  // - mov rcx,[r15+00000208]
                "48 89 44 24 28      "  // - mov [rsp+28],rax
                "89 7C 24 20         "  // - mov [rsp+20],edi
                "E8", RIP(__fn1401858D0), // - call ACU.exe+1858D0                                              - needs to be adjusted - relative `call` between static and allocated addresses.
                "49 8B 47 28         "  // - mov rax,[r15+28]
                "48 8D 4D 90         "  // - lea rcx,[rbp-70]
                "4C 8D 4D 80         "  // - lea r9,[rbp-80]
                "48 8B 00            "  // - mov rax,[rax]
                "48 89 4C 24 40      "  // - mov [rsp+40],rcx
                "49 8B 8F 08020000   "  // - mov rcx,[r15+00000208]
                "48 8B 80 F0010000   "  // - mov rax,[rax+000001F0]
                "4C 8D 45 D0         "  // - lea r8,[rbp-30]
                "48 8D 55 A0         "  // - lea rdx,[rbp-60]
                "48 89 44 24 38      "  // - mov [rsp+38],rax
                "44 88 64 24 30      "  // - mov [rsp+30],r12l
                "89 7C 24 28         "  // - mov [rsp+28],edi
                "F3 0F11 7C 24 20    "  // - movss [rsp+20],xmm7
                "E8", RIP(__fn140185630), // - call ACU.exe+185630                                              - needs to be adjusted - relative `call` between static and allocated addresses.
                "E9", RIP(maybeSkipParkourScanner__return),  // - jmp ACU.exe+1A4C620                           - needs to be adjusted - relative `jmp` between static and allocated addresses.
            };
        }
        void CreateAndActivatePatch()
        {
            static AutoAssembleWrapper<EnterWindowWhenRisPressed> parkourScannerPatch;
            parkourScannerPatch.Activate();
        }


The workflow I had in mind is the following:
- In Cheat Engine, write AutoAssembler code until you're satisfied with the results.
- Copy the generated bytecode, and without any additional processing paste it into C++ as a string.
- Look at the generated bytes, recognize and manually select which of them depend on specific locations
    where the code is allocated, then automatically adjust them for the real allocations.
    Typically these are:
        - the `jmp newmem`, `jmp return`. The code generated for these opcodes
          depends on the distance in memory between the opcode and the `jmp` destination;
        - the reads and writes to a variable allocated separately from the code using it;
        - the read and writes to absolute addresses of dynamic variables (happens sometimes).
- Activate() the C++ "script" object.


However, the most often used feature of this project for me has been
the much more straightforward "C++ Code in the Middle" script preset.

Cheat Engine now allows you to write C code in the AutoAssembler scripts
using the new {$ccode} blocks.

For example, consider the following Cheat Engine Script snippet:
        ...
        ...
        alloc(newmem,$1000,"ACU.exe"+1A4C618)

        label(code)
        label(return)

        newmem:
        {$ccode rbxProxy=rbx rcxProxy=rcx}

        {$asm}
        code:
          // Execute the stolen bytes
          mov rax,[r15+28]
          lea r8,[rbp-80]
          // Return from `newmem`
          jmp return

        address:
          jmp newmem
        return:
        ...
        ...
This would generate the following bytecode upon activation:
    - call ccode_wrapper_function
       - save all registers to stack
       - call actual_C_function
          - read and write the _stored_ registers
       - restore all registers from stack (including the modifications)
       - return from ccode_wrapper_function
    - execute stolen bytes
    - jmp return from newmem
I carried this idea over to this project in the form of
    `AutoAssemblerCodeHolder_Base::PresetScript_CCodeInTheMiddle()`
The function can be used from the `AutoAssemblerCodeHolder_Base` subclasses,
and results in a patch that effectively executes a chosen C++ function before a chosen opcode.
This covers a lot of use cases, since you can now modify register values from C++,
the hooks are very quick to set up and have very few limitations
regarding where they can be placed (there are some, since I don't use any assembler/disassembler):
- At least 5 bytes need to be stolen to make the jump
- The stolen bytes' opcodes must not depend on where they are located
  (again, because I'm blindly copying the stolen bytes, and I'm not using an assembler/disassembler)
  - This means, the stolen bytes cannot refer to relative addresses, so not one of, for example:
    - jmp <static address>
    - je <static address>
    - call <static address> (unless you choose the option to _not_ execute the stolen bytes)
  - `jmp rax`, `call [r15]` and such would be fine. The idea is that the instruction's bytes
    mustn't depend on its location.


Example:
    Bytes and opcodes were copied from Disassembler Window; comments describe
    whether a `PresetScript_CCodeInTheMiddle()` can be used _before_ (in place of) each opcode:
       141A4C691 - 0FB6 C0               - movzx eax,al                       // YES: 7 bytes, 2 opcodes will be stolen
       141A4C694 - 41 0F47 C6            - cmova eax,r14d                     // YES: 7 bytes, 2 opcodes will be stolen
       141A4C698 - 89 45 E0              - mov [rbp-20],eax                   // YES: 8 bytes, 2 opcodes will be stolen
       141A4C69B - 80 7C 24 70 00        - cmp byte ptr [rsp+70],00           // YES: 5 bytes, 1 opcode will be stolen
       141A4C6A0 - 0F84 C9000000         - je ACU.exe+1A4C76F                 // NO: the bytes of `je` instruction depend on where it is located
       141A4C6A6 - 49 8B 4F 28           - mov rcx,[r15+28]                   // YES: 7 bytes, 2 opcodes will be stolen
       141A4C6AA - 48 8B 01              - mov rax,[rcx]                      // YES: 11 bytes, 2 opcodes will be stolen
       141A4C6AD - 48 83 B8 F0010000 00  - cmp qword ptr [rax+000001F0],00    // YES: 8 bytes, 1 opcode will be stolen
       141A4C6B5 - 0F85 B4000000         - jne ACU.exe+1A4C76F                // NO: the bytes of `jne` instruction depend on where it is located
       141A4C6BB - 48 8B 88 F0010000     - mov rcx,[rax+000001F0]             // YES: 7 bytes, 1 opcode will be stolen
       141A4C6C2 - E8 396078FE           - call ACU.exe+1D2700                // NO: the bytes of `call` instruction depend on where it is located
       141A4C6C7 - 84 C0                 - test al,al                         // ! NO: at least 5 bytes, so 2 opcodes are required here, but the second opcode is `jbe <static address>`, which cannot be used
       141A4C6C9 - 0F86 A0000000         - jbe ACU.exe+1A4C76F                // NO: the bytes of `jbe` instruction depend on where it is located


    For any of the allowed opcodes (e.g. at address 141A4C694) you can now create a hook like so:
        void InspectAllRegistersDuringParkour(AllRegisters* params)
        {
            // Freely read and modify the registers here.
            params->r14_ = 55;
            ParkourData* parkourData = (ParkourData*)params->r15_;
            // And use all of C++.
            parkourData->member_28->p0->p1F0 = nullptr;
        }
        struct ParkourHook : AutoAssemblerCodeHolder_Base
        {
            ParkourHook()
            {
                uintptr_t onParkourUpdate = 0x141A4C694;
                const bool executeStolenBytes = true;
                PresetScript_CCodeInTheMiddle(
                    onParkourUpdate, 7,
                    InspectAllRegistersDuringParkour, RETURN_TO_RIGHT_AFTER_STOLEN_BYTES, executeStolenBytes);
            }
        };
        void CreateAndActivateParkourUpdateHook()
        {
            static AutoAssembleWrapper<ParkourHook> parkourUpdateHook;
            parkourUpdateHook.Activate();
        }


*/


#define DEFINE_ADDR(symbol, addr) StaticSymbol& symbol = m_ctx->MakeNew_Define(addr, #symbol);
#define DEFINE_ADDR_NAMED(symbol, name, addr) StaticSymbol& symbol = m_ctx->MakeNew_Define(addr, name);
#define ALLOC(symbol, size, preferredAddrOptional) AllocatedWriteableSymbol& symbol = m_ctx->MakeNew_Alloc(size, #symbol, preferredAddrOptional);
#define ALLOC_NAMED(symbol, name, size, preferredAddrOptional) AllocatedWriteableSymbol& symbol = m_ctx->MakeNew_Alloc(size, name, preferredAddrOptional);
#define LABEL(symbol) Label& symbol = m_ctx->MakeNew_Label(#symbol);
#define LABEL_NAMED(symbol, name) Label& symbol = m_ctx->MakeNew_Label(name);





#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <string_view>
#include "PatternScanner.h"

namespace AutoAssemblerKinda {
typedef unsigned char		byte;		// 8 bits
} // namespace AutoAssemblerKinda





class db
{
public:
    explicit db(unsigned __int8 singleValue) : m_Values({ singleValue }) {}
    explicit db(std::vector<unsigned __int8>&& values) : m_Values(std::move(values)) {}
    std::vector<unsigned __int8> m_Values;
};
class dw
{
public:
    explicit dw(unsigned __int16 singleValue) : m_Values({ singleValue }) {}
    std::vector<unsigned __int16> m_Values;
};
class dd
{
public:
    explicit dd(unsigned __int32 singleValue) : m_Values({ singleValue }) {}
    explicit dd(std::vector<unsigned __int32>&& values) : m_Values(std::move(values)) {}
    std::vector<unsigned __int32> m_Values;
};
class dq
{
public:
    explicit dq(unsigned __int64 singleValue) : m_Values({ singleValue }) {}
    explicit dq(std::vector<unsigned __int64>&& values) : m_Values(std::move(values)) {}
    std::vector<unsigned __int64> m_Values;
};
class nop {
public:
    explicit nop(size_t howManyBytes = 1) : m_HowManyBytes(howManyBytes) {}
    size_t m_HowManyBytes;
};
class SymbolWithAnAddress;
class RIP
{
public:
    explicit RIP(SymbolWithAnAddress& symbol, int howManyBytesUntilTheEndOfOpcodeIncludingThese4bytes = 4)
        : m_Symbol(symbol)
        , m_HowManyBytesUntilTheEndOfOpcodeIncludingThese4bytes(howManyBytesUntilTheEndOfOpcodeIncludingThese4bytes)
    {}
    std::reference_wrapper<SymbolWithAnAddress> m_Symbol;
    uint32_t m_HowManyBytesUntilTheEndOfOpcodeIncludingThese4bytes;
};
class ABS
{
public:
    explicit ABS(SymbolWithAnAddress& symbol, int absoluteAddrWidth) : m_Symbol(symbol), m_AbsoluteAddrWidth(absoluteAddrWidth) {}
    std::reference_wrapper<SymbolWithAnAddress> m_Symbol;
    uint8_t m_AbsoluteAddrWidth;
};
class Label;
class LabelPlacement
{
public:
    explicit LabelPlacement(Label& label) : m_Label(label) {}
    std::reference_wrapper<Label> m_Label;
};
inline LabelPlacement PutLabel(Label& label) { return LabelPlacement(label); }
class WriteableSymbol;
using CodeElement = std::variant <
    db
    , dw
    , dd
    , dq
    // A string of hex numbers that can be copied from Cheat Engine's generated bytecode (e.g. "48 C1 E8 30" for `shr rax,30`)
    , std::string_view
    // Address of a symbol (Absolute or RIP-Relative, that depends on which opcode it is a part of), to correct opcodes that refer to it.
    // For example: Cheat Engine might turn "jmp newmem" into "E9 87654321", and that would mean
    // that the real address of `newmem` is equal to (0x21436587 + 5 + <current_opcode_position>).
    //      (+5 is because a near relative `jmp` opcode is 5 bytes long, and the relative addressing is relative to the _next_ instruction)
    // So this opcode can be translated here into {"E9", RIP(newmem)}.
    // Another, kinda specific example: if you have a `some_variable` allocated at 784B0000 (note that it has a 4-byte address),
    // then "mov [some_variable],2D681820" ==
    //      13FFF0004 - C7 04 25 00004B78 2018682D - mov [784B0000],2D681820 { 2018181120,(0) }
    // which translates into {"C7 04 25", ABS(some_variable), "2018682D"}
    // The following example uses relative addressing again, for some_variable at near address 1784B0000
    //      13FFF0004 - C7 05 F2FF4B38 2018682D - mov [1784B0000],2D681820
    //      13FFF000E - ...
    // If disassembled, this sequence of bytes actually corresponds to `mov DWORD PTR [rip+0x384bfff2], 0x2d681820`.
    // (0x384bfff2 == <address of some_variable> - <address_of_next_instruction> == 0x1784B0000 - 0x13FFF000E)
    // Since I don't disassemble anything here, I do not know where the next instruction starts, and that means that I do not know
    // what exactly to subtract from 0x1784B0000, and therefore I do not know what to write at 13FFF0006 (which is where the 4-byte relative address needs to be written).
    // Some additional information needs to be provided to determine the correct offset. Here's some math:
    // The correct offset == 0x384bfff2
    //      == 0x1784B0000 - 0x13FFF000E
    //      == <address of variable> - <address of next instruction>
    //      == <address of variable> - (<address of where the offset needs to be written> + 8)
    // So the `8` here is that additional information needed - the number of bytes until the end of the current opcode.
    // Therefore, the correct translation of this particular opcode would be:
    //      {"C7 05", RIP(some_variable, 8), "2018682D"}
    , RIP, ABS
    // Label's position will be defined by putting it in the code elements array at needed position.
    , LabelPlacement
    // Just some number of nop bytes
    , nop
> ;









struct SymbolMentionTypeRelative
{
    uint32_t m_HowManyBytesUntilTheEndOfOpcodeIncludingThese4bytes;
};
struct SymbolMentionTypeAbsolute
{
    uint32_t m_AbsoluteAddressWidth;
};
using SymbolReferenceDetails = std::variant<SymbolMentionTypeRelative, SymbolMentionTypeAbsolute>;
class SymbolMention
{
public:
    SymbolMention(WriteableSymbol& symbol, int offsetFromSymbolStart, SymbolReferenceDetails referenceOption)
        : m_Symbol(symbol)
        , m_OffsetFromSymbolStart(offsetFromSymbolStart)
        , m_ReferenceOption(referenceOption)
    {}
    WriteableSymbol& m_Symbol;
    /*
    E.g:
    If m_Symbol is at address 0x70000000,
    and contains `jmp 70123456`, the instruction needs to be resolved to "E9 51341200" == `jmp <0x70123456 - (0x70000000+5)>`,
    so the resolved relative address would be 0x123451 and `m_OffsetFromSymbolStart` == 1 (first byte after E9).
    */
    int m_OffsetFromSymbolStart;
    SymbolReferenceDetails m_ReferenceOption;
};
struct ByteVector
{
    std::vector<AutoAssemblerKinda::byte> m_bytes;
    ByteVector() {}
    ByteVector(std::vector<AutoAssemblerKinda::byte>&& bytes) : m_bytes(std::move(bytes)) {}
};
class SymbolWithAnAddress
{
public:
    virtual ~SymbolWithAnAddress() {}
    SymbolWithAnAddress(std::optional<uintptr_t> m_ConstantAddress) : m_ResolvedAddr(m_ConstantAddress) {}
    SymbolWithAnAddress(const SymbolWithAnAddress& other) = delete;
    std::string m_SymbolName;
    std::optional<uintptr_t> m_ResolvedAddr;
    std::vector<SymbolMention> m_ReferencesToResolve;
    ByteVector CopyCurrentBytes(size_t numBytes);
};
class AssemblerContext;
/*
Just std::vector<CodeElements> but the only constructor is for initializer_list
to encourage the intended syntax i.e.

    DEFINE_ADDR(whenSomethingHappensThatNeedsToBePatched, 0x1419158E0);
    ALLOC(whenSomethingHappensThatNeedsToBePatched_cave, 0x80, 0x1419158E0);

    whenSomethingHappensThatNeedsToBePatched = {
        db(0xE9), RIP(whenSomethingHappensThatNeedsToBePatched_cave)
    };

The braces in the assignment above should convert into std::initializer_list
and that one into std::vector, but in C++20 one can accidentally use
        0xE9, RIP(whenSomethingHappensThatNeedsToBePatched_cave)
instead of
        db(0xE9), RIP(whenSomethingHappensThatNeedsToBePatched_cave)
thinking that 0xE9 will be interpreted as a single byte (which used to be the case).
While the latter (and probably intended) line would mean a 5-byte relative jmp to the code cave,
the former places 4-byte relative pointer, 233 times in a row, without even a warning.
*/
class StdVectorOfCodeElements : public std::vector<CodeElement>
{
public:
    StdVectorOfCodeElements(std::initializer_list<CodeElement> ilist)
        : std::vector<CodeElement>(ilist)
    {}
};
class WriteableSymbol : public SymbolWithAnAddress
{
    friend AssemblerContext;
    AssemblerContext* m_ctx;
public:
    WriteableSymbol(std::optional<uintptr_t> m_ConstantAddress, AssemblerContext* ctx) : SymbolWithAnAddress(m_ConstantAddress), m_ctx(ctx) {}
    WriteableSymbol& operator=(const StdVectorOfCodeElements& codeElements) { SetCodeElements(codeElements); return *this; }
    WriteableSymbol& operator+=(const StdVectorOfCodeElements& codeElements) { AppendCodeElements(codeElements); return *this; }
    void SetCodeElements(const std::vector<CodeElement>& codeElements);
    void AppendCodeElements(const std::vector<CodeElement>& codeElements);
    std::string GetResultBytesString();
protected:
    void ProcessNewCodeElements(size_t idxToStartProcessingFrom);
    std::vector<CodeElement> m_codeElements;
    std::vector<AutoAssemblerKinda::byte> m_resultantCode;
    void Write();
};
class StaticSymbol : public WriteableSymbol
{
    friend AssemblerContext;
public:
    using WriteableSymbol::WriteableSymbol;
    using WriteableSymbol::operator=;
private:
    std::vector<AutoAssemblerKinda::byte> m_OriginalBytes;
    void Write();
    void Unwrite();
};
class AllocatedWriteableSymbol : public WriteableSymbol
{
    friend AssemblerContext;
public:
    using WriteableSymbol::WriteableSymbol;
    using WriteableSymbol::operator=;
    virtual ~AllocatedWriteableSymbol();
private:
    uint32_t m_SizeToAllocate;
    uintptr_t m_PreferredAddr;
    std::optional<uintptr_t> m_SuccessfulAllocBase;
private:
    using WriteableSymbol::Write;
};
class Label : public SymbolWithAnAddress
{
public:
    using SymbolWithAnAddress::SymbolWithAnAddress;
    WriteableSymbol* m_AssignedInWhichSymbol = nullptr;
    uint32_t m_Offset;
};
class AssemblerContext
{
private:
    std::vector<std::unique_ptr<SymbolWithAnAddress>> m_Symbols;
    std::vector<Label*> m_LabelsQuickAccess;
    std::vector<AllocatedWriteableSymbol*> m_AllocsQuickAccess;
    std::vector<StaticSymbol*> m_DefinesQuickAccess;
public:
    Label&
        MakeNew_Label(const std::string_view& symbolName);
    StaticSymbol&
        MakeNew_Define(uintptr_t addr, const std::string_view& symbolName);
    AllocatedWriteableSymbol&
        MakeNew_Alloc(uint32_t size, const std::string_view& symbolName, uintptr_t preferredAddr = 0);
    void AddSymbolRef(SymbolWithAnAddress& symbol, const SymbolMention& newRef);
    void AssignLabel(Label& label, WriteableSymbol& symbol, uint32_t offset);
    void AllocateVariables();
    void ResolveSymbolAddresses();
    void ResolveSymbolReferences();

    void WriteChanges();
    void Unwrite();

    SymbolWithAnAddress* GetSymbol(const std::string_view& symbolName);
};

typedef struct {
    union {
        struct {
            float f0;
            float f1;
            float f2;
            float f3;
        };
        struct {
            double d0;
            double d1;
        };
        float fa[4];
        double da[2];
    };
} xmmreg, * pxmmreg;
// From https://en.wikipedia.org/wiki/FLAGS_register.
struct RFLAGS
{
    bool CF : 1;
    bool unused_bit_1 : 1;
    bool PF : 1;
    bool unused_bit_3 : 1;
    bool AF : 1;
    bool unused_bit_5 : 1;
    bool ZF : 1;
    bool SF : 1;
    bool TF : 1;
    bool IF : 1;
    bool DF : 1;
    bool OF : 1;
    unsigned char unused_bits_12_15 : 16 - 12;
    unsigned short unused_bits_16_31;
    unsigned int unused_bits_32_63;
};
static_assert(sizeof(RFLAGS) == 8);

#ifdef _WIN64
struct AllRegisters
{
    char pad_0[0xA0];
    xmmreg XMM0;
    xmmreg XMM1;
    xmmreg XMM2;
    xmmreg XMM3;
    xmmreg XMM4;
    xmmreg XMM5;
    xmmreg XMM6;
    xmmreg XMM7;
    xmmreg XMM8;
    xmmreg XMM9;
    xmmreg XMM10;
    xmmreg XMM11;
    xmmreg XMM12;
    xmmreg XMM13;
    xmmreg XMM14;
    xmmreg XMM15;
    char pad_1A0[0x200 - 0x1A0];
    unsigned long long rbx_;
    unsigned long long rcx_;
    unsigned long long rdx_;
    unsigned long long rsi_;
    unsigned long long rdi_;
    unsigned long long* rax_;   // Note that RAX is accessed differently from other registers: the _value_ of RAX register is `*this->rax_`, not `this->rax_`.
    unsigned long long rbp_;
    unsigned long long r8_;
    unsigned long long r9_;
    unsigned long long r10_;
    unsigned long long r11_;
    unsigned long long r12_;
    unsigned long long r13_;
    unsigned long long r14_;
    unsigned long long r15_;
    unsigned long long GetRSP() { return (unsigned long long)rax_ + 0x18; }
    unsigned long long& GetRAX() { return *rax_; }
    RFLAGS& GetFLAGS() { return *(RFLAGS*)((unsigned long long)rax_ + 8); }
};
static_assert(offsetof(AllRegisters, XMM0) == 0xA0);
static_assert(offsetof(AllRegisters, XMM15) == 0x190);
static_assert(offsetof(AllRegisters, rbx_) == 0x200);
#else // _WIN32
struct AllRegisters // Matches the order of PUSHAD
{
    // This struct is passed by pointer to your C++ function.
    // You can read and write these values. They will be restored on return.
    unsigned long edi;
    unsigned long esi;
    unsigned long ebp;
    unsigned long esp_original; // ESP before the function call
    unsigned long ebx;
    unsigned long edx;
    unsigned long ecx;
    unsigned long eax;
    unsigned long eflags;
    
    // Convenience accessors for x86
    unsigned long& GetEAX() { return eax; }
    unsigned long& GetEBX() { return ebx; }
    unsigned long& GetECX() { return ecx; }
    unsigned long& GetEDX() { return edx; }
    unsigned long& GetESI() { return esi; }
    unsigned long& GetEDI() { return edi; }
    unsigned long& GetEBP() { return ebp; }
    unsigned long GetESP() { return esp_original; }
    unsigned long& GetEFLAGS() { return eflags; }
};

#endif
class AutoAssemblerCodeHolder_Base
{
public:
    std::unique_ptr<AssemblerContext> m_ctx;
public:
    AutoAssemblerCodeHolder_Base();
    // Can optionally be overridden.
    virtual void OnBeforeActivate() {}
    virtual void OnBeforeDeactivate() {}

    using CCodeInTheMiddleFunctionPtr_t = void (*)(AllRegisters* parameters);
    static std::optional<uintptr_t> RETURN_TO_RIGHT_AFTER_STOLEN_BYTES;
    /*
    The freaking god-combo of AutoAssembler.
    jmp newmem
    -> store registers
    -> call [externalFuncAddr] (with ptr to registers as parameter)
    -> restore registers
    -> execute stolen bytes
    -> jmp return
    */
    void PresetScript_CCodeInTheMiddle(
        uintptr_t whereToInject, size_t howManyBytesStolen
        , CCodeInTheMiddleFunctionPtr_t receiverFunc
        , std::optional<uintptr_t> whereToReturn = RETURN_TO_RIGHT_AFTER_STOLEN_BYTES, bool isNeedToExecuteStolenBytesAfterwards = true);
    void PresetScript_NOP(
        uintptr_t whereToInject, size_t howManyBytesToNOP);
    void PresetScript_ReplaceFunctionAtItsStart(
        uintptr_t whereToInject, void* Func);
    void PresetScript_InjectJump(
        uintptr_t whereToInject, uintptr_t targetAddr, size_t howManyBytesStolen = 5, uintptr_t* pReturnAddr = nullptr);
};
template<class HasAutoAssemblerCodeInConstructor>
class AutoAssembleWrapper
{
    static_assert(std::is_base_of_v<AutoAssemblerCodeHolder_Base, HasAutoAssemblerCodeInConstructor >,
        "AutoAssembleWrapper template parameter needs to be derived from `AutoAssemblerCodeHolder_Base`");
private:
    HasAutoAssemblerCodeInConstructor m_CodeHolderInstantiation;
public:
    template <typename ... Args>
    AutoAssembleWrapper(Args&& ... args)
        : m_CodeHolderInstantiation(std::forward<Args>(args) ...)
    {
        m_CodeHolderInstantiation.m_ctx->AllocateVariables();
        m_CodeHolderInstantiation.m_ctx->ResolveSymbolAddresses();
        m_CodeHolderInstantiation.m_ctx->ResolveSymbolReferences();
    }
    void Activate()
    {
        if (m_IsActive) { return; }
        m_CodeHolderInstantiation.OnBeforeActivate();
        m_CodeHolderInstantiation.m_ctx->WriteChanges();
        m_IsActive = true;
    }
    void Deactivate()
    {
        if (!m_IsActive) { return; }
        m_CodeHolderInstantiation.OnBeforeDeactivate();
        m_CodeHolderInstantiation.m_ctx->Unwrite();
        m_IsActive = false;
    }
    void Toggle(bool activate)
    {
        if (activate)
            Activate();
        else
            Deactivate();
    }
    void Toggle()
    {
        Toggle(!m_IsActive);
    }
    bool IsActive() { return m_IsActive; }
    ~AutoAssembleWrapper()
    {
        if (m_IsActive) { Deactivate(); }
    }

    HasAutoAssemblerCodeInConstructor& GetCodeHolder() { return m_CodeHolderInstantiation; }
    AssemblerContext& debug_GetAssemblerContext() { return *m_CodeHolderInstantiation.m_ctx; }
private:
    bool m_IsActive = false;
};

// ============================================
// Hook Utilities
// ============================================

// Legacy macro to define a static naked hook function and its return address variable.
// Usage:
// DEFINE_HOOK(MyHookName, ReturnAddressVar) {
//     __asm {
//         // your asm code
//         jmp [ReturnAddressVar]
//     }
// }
//
// In your Init function:
// PresetScript_InjectJump(hookAddress, (uintptr_t)MyHookName, stolenBytes, &ReturnAddressVar);

#define DEFINE_HOOK(HookName, ReturnVarName) \
    static uintptr_t ReturnVarName = 0; \
    static void __declspec(naked) HookName()

// ============================================
// Unified Hook System
// ============================================

// Forward declarations
class HookManager;

// Named exit point for hooks with multiple jump targets
struct HookExit {
    const char* name;
    intptr_t offset;       // Offset from resolved address
    uintptr_t* targetPtr;  // Pointer to variable that will receive resolved address
};

// Extracted dependency from pattern (e.g., function pointer via relative call)
// (HookDependency struct removed) -> Replaced by AOBAddress logic
// Base Interface for all Hook Types
class IHook {
public:
    virtual ~IHook() = default;

    // Metadata
    virtual const char* GetName() const = 0;
    
    // Core Operations
    virtual bool Resolve(bool requireUnique = true) = 0;
    virtual bool Install() = 0;
    virtual bool Uninstall() = 0;
    
    // State Query
    virtual bool IsResolved() const = 0;
    virtual bool IsInstalled() const = 0;
    virtual uintptr_t GetAddress() const = 0;
    
    // Config
    virtual void SetExits(HookExit* exits, size_t count) = 0;

    
    // For automatic registration
    bool RegisterSelf();
};

// ============================================
// Concrete Hook Types
// ============================================

// 1. Naked Hook (x86 Only, classic assembly jmp wrapper)
// Uses a raw assembly function with a trampoline.
// Very efficient, but x86 only.
class NakedHook : public IHook {
public:
    using NakedFuncPtr = void(*)();
    
    struct Descriptor {
        const char* name;
        const char* moduleName;
        const char* aobSignature;
        intptr_t aobOffset;
        size_t stolenBytes;
        NakedFuncPtr hookFunction;
        uintptr_t* returnAddress;
    };

    NakedHook(const Descriptor& desc);
    ~NakedHook();

    const char* GetName() const override { return m_Desc.name; }
    bool Resolve(bool requireUnique = true) override;
    bool Install() override;
    bool Uninstall() override;
    
    bool IsResolved() const override { return m_Resolved; }
    bool IsInstalled() const override { return m_Installed; }
    uintptr_t GetAddress() const override { return m_ResolvedAddress; }

    void SetExits(HookExit* exits, size_t count) override;


private:
    Descriptor m_Desc;
    uintptr_t m_ResolvedAddress = 0;
    bool m_Resolved = false;
    bool m_Installed = false;
    void* m_WrapperInstance = nullptr; // AOBHookWrapper*
    
    HookExit* m_Exits = nullptr;
    size_t m_ExitCount = 0;

};

// 2. CCode Hook (Cross-Platform / Easier Logic)
// Uses AutoAssembler engine to inject a C++ function call.
// Works on x86 and x64.
class CCodeHook : public IHook {
public:
    using CCodeFuncPtr = void(*)(AllRegisters*);

    struct Descriptor {
        const char* name;
        const char* moduleName;
        const char* aobSignature;
        intptr_t aobOffset;
        size_t stolenBytes;
        CCodeFuncPtr paramFunction;
        bool executeStolenBytes;
    };

    CCodeHook(const Descriptor& desc);
    ~CCodeHook();

    // IHook Interface
    const char* GetName() const override { return m_Desc.name; }
    bool Resolve(bool requireUnique = true) override;
    bool Install() override;
    bool Uninstall() override;

    bool IsResolved() const override { return m_Resolved; }
    bool IsInstalled() const override;
    uintptr_t GetAddress() const override { return m_ResolvedAddress; }
    
    void SetExits(HookExit* exits, size_t count) override;


private:
    Descriptor m_Desc;
    uintptr_t m_ResolvedAddress = 0;
    bool m_Resolved = false;
    
    // Internal Logic Holder
    struct HookLogic;
    AutoAssembleWrapper<HookLogic>* m_Wrapper = nullptr; 

    HookExit* m_Exits = nullptr;
    size_t m_ExitCount = 0;

};

// 3. Data Patch (Write bytes)
class DataPatch : public IHook {
public:
    struct Descriptor {
        const char* name;
        const char* moduleName;
        const char* aobSignature;
        intptr_t aobOffset;
        const uint8_t* data;
        size_t size;
        bool allSections;  // Set to true to scan data sections (for strings)
    };

    DataPatch(const Descriptor& desc);

    const char* GetName() const override { return m_Desc.name; }
    bool Resolve(bool requireUnique = true) override;
    bool Install() override;
    bool Uninstall() override;

    bool IsResolved() const override { return m_Resolved; }
    bool IsInstalled() const override { return m_Installed; }
    uintptr_t GetAddress() const override { return m_ResolvedAddress; }

    void SetExits(HookExit* exits, size_t count) override {}


private:
    Descriptor m_Desc;
    uintptr_t m_ResolvedAddress = 0;
    bool m_Resolved = false;
    bool m_Installed = false;
    std::vector<uint8_t> m_OriginalBytes;
};

// 4. AOBAddress (Unified Address Resolver)
// Replaces HookDependency. Can scan signature OR resolve relative to another hook.
class AOBAddress : public IHook {
public:
    struct Descriptor {
        const char* name;
        const char* moduleName;
        const char* sectionName;
        const char* signature;       // IF SET: Scan this.
        const char* parentHookName;  // IF SET: Get address from this Hook.
        intptr_t offset;             // Applied to result of Scan or Parent.
        bool resolveRelative;        // If true, resolve as relative call/jmp/mov.
        uint8_t instructionSize;     // e.g. 5
        uint8_t relativeOffset;      // e.g. 1
        int dereferenceCount;        // How many times to dereference the result
        uintptr_t* targetPtr;        // Pointer to variable that will receive resolved address
    };

    AOBAddress(const Descriptor& desc);

    const char* GetName() const override { return m_Desc.name; }
    bool Resolve(bool requireUnique = true) override;
    bool Install() override { return true; } // Nothing to install
    bool Uninstall() override { return true; } // Nothing to uninstall

    bool IsResolved() const override { return m_Resolved; }
    bool IsInstalled() const override { return true; }
    uintptr_t GetAddress() const override { return m_ResolvedAddress; }

    void SetExits(HookExit* exits, size_t count) override {}

private:
    Descriptor m_Desc;
    uintptr_t m_ResolvedAddress = 0;
    bool m_Resolved = false;
};

// ============================================
// Hook Manager
// ============================================
class HookManager {
public:
    static void Register(IHook* hook);
    static IHook* Get(const char* name);
    
    static size_t ResolveAll(bool requireUnique = true);
    static size_t InstallAll();
    static void UninstallAll();
    
    // Convenience
    static bool Resolve(IHook* hook, bool requireUnique = true);
    static void Toggle(IHook* hook, bool enable);
    
    static const std::vector<IHook*>& GetAll();

    // Convenience: ResolveAll + InstallAll
    static size_t ResolveAndInstallAll(bool requireUnique = true);
    
    // Compatibility / Single Hook Control
    static bool Install(IHook* hook) { return hook ? hook->Install() : false; }
    static bool Uninstall(IHook* hook) { return hook ? hook->Uninstall() : false; }

private:
    static std::vector<IHook*>& GetHooks();
};

// ============================================
// Macros - The User-Facing API
// ============================================

// -------------------------------------------------------------------------
// DEFINE_AOB_HOOK (Naked x86)
// -------------------------------------------------------------------------
// Kept for backward compat / pure asm usage.
// Only valid on x86. On x64 this should ideally error or fallback (but naked isnt supported on x64).
#ifndef _WIN64
#define DEFINE_AOB_HOOK(HookName, Signature, Offset, StolenBytes) \
    static uintptr_t HookName##_Return = 0; \
    static void HookName##_Func(); \
    static NakedHook::Descriptor HookName##_Config = { \
        #HookName, nullptr, Signature, Offset, StolenBytes, &HookName##_Func, &HookName##_Return \
    }; \
    static NakedHook HookName##_Descriptor(HookName##_Config); \
    static bool HookName##_Reg = HookName##_Descriptor.RegisterSelf(); \
    static void HookName##_Func()

#define DEFINE_AOB_HOOK_MOD(HookName, ModuleName, Signature, Offset, StolenBytes) \
    static uintptr_t HookName##_Return = 0; \
    static void HookName##_Func(); \
    static NakedHook::Descriptor HookName##_Config = { \
        #HookName, ModuleName, Signature, Offset, StolenBytes, &HookName##_Func, &HookName##_Return \
    }; \
    static NakedHook HookName##_Descriptor(HookName##_Config); \
    static bool HookName##_Reg = HookName##_Descriptor.RegisterSelf(); \
    static void HookName##_Func()

#define HOOK_IMPL(HookName) static void __declspec(naked) HookName##_Func()
#else
// On x64, if user tries to use this macro, we can't implement it purely using naked.
// They should use DEFINE_CPP_HOOK.
#define DEFINE_AOB_HOOK(HookName, Signature, Offset, StolenBytes) \
    static_assert(false, "DEFINE_AOB_HOOK (Naked) is not supported on x64. Use DEFINE_CPP_HOOK instead.");
#define DEFINE_AOB_HOOK_MOD(HookName, ModuleName, Signature, Offset, StolenBytes) \
    static_assert(false, "DEFINE_AOB_HOOK_MOD (Naked) is not supported on x64. Use DEFINE_CPP_HOOK_MOD instead.");
#endif

// -------------------------------------------------------------------------
// DEFINE_CPP_HOOK (C++ Logic - Works on x86/x64)
// -------------------------------------------------------------------------
// Usage:
//   DEFINE_CPP_HOOK(MyHook, "AOB...", 0, 5)        // Execute stolen bytes (Default)
//   DEFINE_CPP_HOOK(MyHook, "AOB...", 0, 5, false) // Don't execute stolen bytes (Replace)
//
// The optional last argument controls 'executeStolenBytes'.
// (true, ##__VA_ARGS__) uses the comma operator:
//   - If empty: (true) -> true
//   - If false: (true, false) -> false
#define DEFINE_CPP_HOOK(HookName, Signature, Offset, StolenBytes, ...) \
    static void HookName##_Func(AllRegisters* params); \
    static CCodeHook::Descriptor HookName##_Config = { \
        #HookName, nullptr, Signature, Offset, StolenBytes, &HookName##_Func, \
        (true, ##__VA_ARGS__) \
    }; \
    static CCodeHook HookName##_Descriptor(HookName##_Config); \
    static bool HookName##_Reg = HookName##_Descriptor.RegisterSelf(); \
    static void HookName##_Func(AllRegisters* params)

#define DEFINE_CPP_HOOK_MOD(HookName, ModuleName, Signature, Offset, StolenBytes, ...) \
    static void HookName##_Func(AllRegisters* params); \
    static CCodeHook::Descriptor HookName##_Config = { \
        #HookName, ModuleName, Signature, Offset, StolenBytes, &HookName##_Func, \
        (true, ##__VA_ARGS__) \
    }; \
    static CCodeHook HookName##_Descriptor(HookName##_Config); \
    static bool HookName##_Reg = HookName##_Descriptor.RegisterSelf(); \
    static void HookName##_Func(AllRegisters* params)

// -------------------------------------------------------------------------
// DEFINE_DATA_PATCH - Unified data patching macro
// -------------------------------------------------------------------------
// Uses auto to deduce type from the value. Cast to specify type:
//   DEFINE_DATA_PATCH(ShadowPatch, "C7 41 20 ?? ?? ??", 3, (int32_t)4096);
//   DEFINE_DATA_PATCH(FloatPatch, "F3 0F 10 05 ?? ?? ??", 4, 1.5f);
//   DEFINE_DATA_PATCH(BytePatch, "89 45 E8", 0, (uint8_t)0x90);
//   DEFINE_DATA_PATCH(StringPatch, "my string", 0, (uint8_t)0x00, true);  // allSections=true
#define DEFINE_DATA_PATCH(PatchName, Signature, Offset, Value, ...) \
    static const auto PatchName##_Value = Value; \
    static DataPatch::Descriptor PatchName##_Config = { \
        #PatchName, nullptr, Signature, Offset, (const uint8_t*)&PatchName##_Value, sizeof(PatchName##_Value), \
        (false, ##__VA_ARGS__)  /* Default false, or use provided value */ \
    }; \
    static DataPatch PatchName##_Descriptor(PatchName##_Config); \
    static bool PatchName##_Reg = PatchName##_Descriptor.RegisterSelf()

#define DEFINE_DATA_PATCH_MOD(PatchName, ModuleName, Signature, Offset, Value, ...) \
    static const auto PatchName##_Value = Value; \
    static DataPatch::Descriptor PatchName##_Config = { \
        #PatchName, ModuleName, Signature, Offset, (const uint8_t*)&PatchName##_Value, sizeof(PatchName##_Value), \
        (false, ##__VA_ARGS__)  /* Default false, or use provided value */ \
    }; \
    static DataPatch PatchName##_Descriptor(PatchName##_Config); \
    static bool PatchName##_Reg = PatchName##_Descriptor.RegisterSelf()


// =========================================================================
// UNIFIED ADDRESS RESOLUTION SYSTEM
// =========================================================================
// Single macro for all address resolution needs.
//
// Usage:
//   DEFINE_ADDRESS(Name, Source, Offset, Mode, Target)
//
// Parameters:
//   Name   - Unique identifier for this address
//   Source - Pattern string "8B 46..." OR parent reference "@ParentName"
//   Offset - Byte offset from resolved address
//   Mode   - RAW (just offset), CALL (resolve call/jmp), DEREF (dereference)
//   Target - Pointer to store result (or nullptr)
//
// Examples:
//   // Scan pattern, store raw address
//   DEFINE_ADDRESS(AllocBase, "52 6A 10 68 B0 05 00 00", 0, RAW, nullptr);
//
//   // Derive from parent, resolve relative CALL target
//   DEFINE_ADDRESS(GetNewDescr, "@AllocBase", 0x08, CALL, &fn);
//
//   // Derive from parent, dereference pointer (MOV [addr])
//   DEFINE_ADDRESS(DescriptorVar, "@AllocBase", -0x04, DEREF, &ptr);
// =========================================================================

// Address resolution modes
enum AddrMode : uint8_t {
    ADDR_RAW      = 0,  // Just apply offset, no processing
    ADDR_RELATIVE = 1,  // Resolve relative call/jmp: target = base + rel32 + instructionSize
    ADDR_POINTER  = 2   // Dereference: target = *(uintptr_t*)(base + offset)
};

// Shorthand (safe names that don't conflict with Windows macros)
#define RAW       ADDR_RAW
#define CALL      ADDR_RELATIVE
#define DEREF     ADDR_POINTER

// Compile-time string helpers
namespace _AddrHelper {
    constexpr bool IsParent(const char* s) { return s && s[0] == '@'; }
    constexpr const char* GetSignature(const char* s) { return IsParent(s) ? nullptr : s; }
    constexpr const char* GetParent(const char* s) { return IsParent(s) ? s + 1 : nullptr; }
    
    // Mode to descriptor fields (use int to accept macro expansions)
    constexpr bool IsRelative(int m) { return m == ADDR_RELATIVE; }
    constexpr uint8_t InstrSize(int m) { return m == ADDR_RELATIVE ? 5 : 0; }
    constexpr uint8_t RelOffset(int m) { return m == ADDR_RELATIVE ? 1 : 0; }
    constexpr int DerefCount(int m) { return m == ADDR_POINTER ? 1 : 0; }
}

// The unified macro
#define DEFINE_ADDRESS(Name, Source, Offset, Mode, Target) \
    static AOBAddress::Descriptor Name##_Cfg = { \
        #Name, nullptr, nullptr, \
        _AddrHelper::GetSignature(Source), _AddrHelper::GetParent(Source), \
        Offset, \
        _AddrHelper::IsRelative(Mode), _AddrHelper::InstrSize(Mode), _AddrHelper::RelOffset(Mode), \
        _AddrHelper::DerefCount(Mode), \
        Target \
    }; \
    static AOBAddress Name##_Desc(Name##_Cfg); \
    static bool Name##_Reg = Name##_Desc.RegisterSelf()

// =========================================================================
// LEGACY COMPATIBILITY MACROS (Deprecated - use DEFINE_ADDRESS instead)
// =========================================================================
// These are kept for backward compatibility with existing code.
// New code should use DEFINE_ADDRESS.

#define DEFINE_AOB_ADDRESS(Name, Sig, Off, Ptr)         DEFINE_ADDRESS(Name, Sig, Off, RAW, Ptr)
#define DEFINE_AOB_RELATIVE(Name, Sig, Off, Ptr)        DEFINE_ADDRESS(Name, Sig, Off, RELATIVE, Ptr)
#define DEFINE_AOB_POINTER(Name, Sig, Off, Ptr)         DEFINE_ADDRESS(Name, Sig, Off, POINTER, Ptr)
#define DEFINE_HOOK_RELATIVE(Name, Parent, Off, Ptr)    DEFINE_ADDRESS(Name, "@" #Parent, Off, RELATIVE, Ptr)
#define DEFINE_PARENT_OFFSET(Name, Parent, Off, Ptr)    DEFINE_ADDRESS(Name, "@" #Parent, Off, RAW, Ptr)
#define DEFINE_PARENT_RELATIVE(Name, Parent, Off, Ptr)  DEFINE_ADDRESS(Name, "@" #Parent, Off, RELATIVE, Ptr)
#define DEFINE_PARENT_POINTER(Name, Parent, Off, Ptr)   DEFINE_ADDRESS(Name, "@" #Parent, Off, POINTER, Ptr)

// Module scan variant (not commonly used, kept for completeness)
#define DEFINE_AOB_ADDRESS_MODULE(Name, ModName, Sig, Off, Ptr) \
    static AOBAddress::Descriptor Name##_Cfg = { #Name, ModName, nullptr, Sig, nullptr, Off, false, 0, 0, 0, Ptr }; \
    static AOBAddress Name##_Desc(Name##_Cfg); \
    static bool Name##_Reg = Name##_Desc.RegisterSelf()


// -------------------------------------------------------------------------
// EXITS / DEPENDENCIES Helper Macros
// -------------------------------------------------------------------------
// These macros need to access the _Instance static variable created above.
// They assume 'HookName' matches what was passed to DEFINE_...

// Explicit macro definitions for up to 5 pairs (10 arguments)
#define DEFINE_EXITS_1(HookName, N1, O1) \
    static uintptr_t HookName##_##N1 = 0; \
    static HookExit HookName##_Exits[] = { { #N1, O1, &HookName##_##N1 } }; \
    static bool HookName##_ExitsLinked = (HookName##_Descriptor.SetExits(HookName##_Exits, 1), true)

#define DEFINE_EXITS_2(HookName, N1, O1, N2, O2) \
    static uintptr_t HookName##_##N1 = 0; \
    static uintptr_t HookName##_##N2 = 0; \
    static HookExit HookName##_Exits[] = { \
        { #N1, O1, &HookName##_##N1 }, \
        { #N2, O2, &HookName##_##N2 } \
    }; \
    static bool HookName##_ExitsLinked = (HookName##_Descriptor.SetExits(HookName##_Exits, 2), true)

#define DEFINE_EXITS_3(HookName, N1, O1, N2, O2, N3, O3) \
    static uintptr_t HookName##_##N1 = 0; \
    static uintptr_t HookName##_##N2 = 0; \
    static uintptr_t HookName##_##N3 = 0; \
    static HookExit HookName##_Exits[] = { \
        { #N1, O1, &HookName##_##N1 }, \
        { #N2, O2, &HookName##_##N2 }, \
        { #N3, O3, &HookName##_##N3 } \
    }; \
    static bool HookName##_ExitsLinked = (HookName##_Descriptor.SetExits(HookName##_Exits, 3), true)

#define DEFINE_EXITS_4(HookName, N1, O1, N2, O2, N3, O3, N4, O4) \
    static uintptr_t HookName##_##N1 = 0; \
    static uintptr_t HookName##_##N2 = 0; \
    static uintptr_t HookName##_##N3 = 0; \
    static uintptr_t HookName##_##N4 = 0; \
    static HookExit HookName##_Exits[] = { \
        { #N1, O1, &HookName##_##N1 }, \
        { #N2, O2, &HookName##_##N2 }, \
        { #N3, O3, &HookName##_##N3 }, \
        { #N4, O4, &HookName##_##N4 } \
    }; \
    static bool HookName##_ExitsLinked = (HookName##_Descriptor.SetExits(HookName##_Exits, 4), true)

#define DEFINE_EXITS_5(HookName, N1, O1, N2, O2, N3, O3, N4, O4, N5, O5) \
    static uintptr_t HookName##_##N1 = 0; \
    static uintptr_t HookName##_##N2 = 0; \
    static uintptr_t HookName##_##N3 = 0; \
    static uintptr_t HookName##_##N4 = 0; \
    static uintptr_t HookName##_##N5 = 0; \
    static HookExit HookName##_Exits[] = { \
        { #N1, O1, &HookName##_##N1 }, \
        { #N2, O2, &HookName##_##N2 }, \
        { #N3, O3, &HookName##_##N3 }, \
        { #N4, O4, &HookName##_##N4 }, \
        { #N5, O5, &HookName##_##N5 } \
    }; \
    static bool HookName##_ExitsLinked = (HookName##_Descriptor.SetExits(HookName##_Exits, 5), true)

// Dispatcher logic
#define _AAK_EXPAND_EXITS(...) __VA_ARGS__
#define _AAK_GET_MACRO_EXITS(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,NAME,...) NAME

// Main Macro
#define DEFINE_EXITS(HookName, ...) \
    _AAK_EXPAND_EXITS(_AAK_GET_MACRO_EXITS(__VA_ARGS__, \
        DEFINE_EXITS_5, _u9, \
        DEFINE_EXITS_4, _u7, \
        DEFINE_EXITS_3, _u5, \
        DEFINE_EXITS_2, _u3, \
        DEFINE_EXITS_1, _u1 \
    )(HookName, __VA_ARGS__))

// Helper Accessor
#define HOOK_RETURN(HookName) HookName##_Return

