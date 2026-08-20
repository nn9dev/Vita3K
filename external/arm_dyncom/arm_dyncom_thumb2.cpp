/**
 * @file arm_dyncom_thumb2.cpp
 * @brief thumb2 interpreter
 * @author good afternoon
 * @version 1
 * @date 2026-08-14
 */

#include <cstddef>
#include <mutex>
#include <unordered_set>

#include <util/log.h>

#include "arm_dyncom_thumb.h"
#include "arm_dyncom_thumb2.h"
#include "skyeye_common/armstate.h"

// A halfword is the first halfword of a 32-bit Thumb-2 instruction (i.e. the
// instruction is 32-bit) when bits[15:11] are one of 0b11101, 0b11110, 0b11111
// (0x1D, 0x1E, 0x1F) (thank you ARM Reference Manual). Everything below that is 
// a 16-bit Thumb-1 instruction and is handled by the pre-existing thumb1 decoder

// The encoding of a 32-bit Thumb instruction is:
//  | 15 14 13 | 12 11 | 10 9 8 7 6 5 4 | 3 2 1 0 | 15 | 14 ... 0 |
//  | 1  1  1  |  op1  |       op2      |         | op |          |

// Rather than round-tripping through ARM encodings (which is impossible for
// some encodings), each recognised instruction fills the interpreter's
// decoded-operand struct directly and dispatches through the
// existing arm_instruction_trans[] execution back-end.
// In this manner, we are able to ""simulate"" thumb2 instructions by directly producing
// an `arm_inst`, the final structure that the interp/translator operates on

enum : int {
    THUMB2_IDX_MOV_IMM = 12, // arm_instruction_trans_len - 12 (MOV{S}.W #imm)
    THUMB2_IDX_MOVW = 11,    // arm_instruction_trans_len - 11
    THUMB2_IDX_MOVT = 10,    // arm_instruction_trans_len - 10
    THUMB2_IDX_BL = 9,       // arm_instruction_trans_len - 9  (BL and BLX)
    THUMB2_IDX_UNDEF = 8,    // arm_instruction_trans_len - 8  (unimplemented skip)
};

// Report each distinct top-level 32-bit encoding class once, so the log reads as
// a clean "what to implement next" worklist rather than one line per execution.
// op1/op2/op are the ARM ARM top-level Thumb-2 lookup key (A6.3).
static void LogUnimplementedThumb2(u32 addr, u32 hw1, u32 hw2) {
    const u32 op1 = (hw1 >> 11) & 0x3;  // bits[12:11]
    const u32 op2 = (hw1 >> 4) & 0x7F;  // bits[10:4]
    const u32 op = (hw2 >> 15) & 0x1;   // hw2[15]
    const u32 key = (op1 << 8) | (op2 << 1) | op;

    static std::mutex mtx;
    static std::unordered_set<u32> seen;
    {
        std::lock_guard<std::mutex> lock(mtx);
        if (!seen.insert(key).second)
            return;
    }

    LOG_WARN("[thumb2] unimplemented 32-bit encoding @ 0x{:08X}: {:04X} {:04X}  "
             "(op1={:02b} op2={:07b} op={:b})",
             addr, hw1, hw2, op1, op2, op);
}

ThumbDecodeStatus TranslateThumb2Instruction(const ARMul_State* cpu, u32 addr, u32 inst,
                                             u32* inst_size, ARM_INST_PTR* ptr_inst_base) {
    const u32 hw1 = GetThumbInstruction(inst, addr);
    // Second halfword sits in the high 16 bits of the fetched word when the
    // instruction is word-aligned, otherwise it is the first halfword of the
    // following word and must be read separately
    const u32 hw2 = (addr & 2) ? cpu->ReadMemory16(addr + 2) : (inst >> 16);
    const u32 thumb32 = (hw1 << 16) | hw2;

    *inst_size = 4;

    const int len = static_cast<int>(arm_instruction_trans_len);

    // Selected arm_instruction_trans[] slot for this encoding. -1 means "not
    // decoded", and should fall through to the log + 4-byte skip
    int idx = -1;

    // switch statement to mirror the manual decoding i do based on the manual
    // always list statements from least value to greatest value for consistency!
    switch ((hw1 & 0x1800) >> 11) { // isolate op1 (hw1[12:11])
    case 0b01:
        // Load/store (multiple/dual/exclusive), table branch, data-processing
        // (shifted register), multiply, divide (A6-231 ) — not implemented yet.
        break;
    case 0b10:
        // this is the only one of the 3 op1 cases where op has bearing
        if ((hw2 & 0x8000) == 0x8000) {  // check 15th bit of second half-word (op)
            // Branches and miscellaneous control on page A6-233
            // BL (T1) / BLX (T2): hw2[15:14] == 11 (hw2[12] selects BL vs BLX)
            if ((hw2 & 0xC000) == 0xC000)
                idx = len - THUMB2_IDX_BL;
            break;
        }
        // only need to check one bit (bits[15]) inside op2
        // op2[5] (hw1[9]): modified vs plain-binary-immediate
        switch ((hw1 & 0x200)) {
        case 0x000:   // Data-processing (modified immediate) on page A6-229
            switch ((hw1 & 0x1E0) >> 5) { // op (hw1[8:5])
                case 0b0000:
                    break;
                case 0b0001:
                    break;
                case 0b0010:
                    if ((hw1 & 0xF) == 0xF)
                        idx = len - THUMB2_IDX_MOV_IMM; // MOV{S} (immediate) (A8-485)
                    // else: Bitwise OR, ORR (immediate) on page A8-517
                    break;
                case 0b0011:
                    break;
                case 0b0100:
                    break;
                case 0b1001:
                    break;
                case 0b1010:
                    break;
                case 0b1100:
                    break;
                case 0b1101:
                    break;
                case 0b1110:
                    break;
                default:
                    return ThumbDecodeStatus::UNDEFINED;
            }
            break;
        case 0x200: // Data-processing (plain binary immediate) on page A6-232
            // MOVW (T3): hw1[9:4] == 100100 ; MOVT (T1): hw1[9:4] == 101100
            if ((hw1 & 0xFBF0) == 0xF240)
                idx = len - THUMB2_IDX_MOVW;
            else if ((hw1 & 0xFBF0) == 0xF2C0)
                idx = len - THUMB2_IDX_MOVT;
            break;
        }
        break;
    case 0b11:
        // Coprocessor, Advanced SIMD, and Floating-point (Page A6-249) — not
        // implemented yet.
        break;
    }

    if (idx < 0) {
        // Unrecognised 32-bit encoding: log once per class, then emit a 4-byte
        // skip so the stream stays aligned and the run keeps collecting gaps
        LogUnimplementedThumb2(addr, hw1, hw2);
        idx = len - THUMB2_IDX_UNDEF;
    }

    *ptr_inst_base = arm_instruction_trans[idx](thumb32, idx);
    return ThumbDecodeStatus::DECODED;
}
