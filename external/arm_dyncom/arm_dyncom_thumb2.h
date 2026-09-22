/**
 * @file arm_dyncom_thumb2.h
 * @brief thumb2 interpreter
 * @author good afternoon
 * @version 1
 * @date 2026-08-14
 * @copyright Copyright 2026 good afternoon
 */

#pragma once

#include "common/common_types.h"
#include "arm_dyncom_thumb.h" // ThumbDecodeStatus, GetThumbInstruction
#include "arm_dyncom_trans.h" // ARM_INST_PTR, arm_instruction_trans[]

struct ARMul_State;

// decode one 32-bit thumb2 instruction at `addr`. inst = ((hw1 << 16) | hw2)
// Instead of creating an ""arm"" equivalent of a given thumb instruction like
// the thumb1 path does, instead we hijack the IR and craft our own new
// instructions, fill them, and feed them to arm_instruction_trans[]
// thumb2 will also now handle all BL/BLX instructions :)
// might honestly want to adapt some of these into arm translations though just to reduce code repitition
ThumbDecodeStatus TranslateThumb2Instruction(const ARMul_State* cpu, u32 addr, u32 inst,
                                             u32* inst_size, ARM_INST_PTR* ptr_inst_base);

// ordered by decode precedence
// this is basically a mini, thumb2-exclusive version of the table in trans.cpp (arm_instruction_trans[])
enum Thumb2Table : int {
    THUMB2_LDREX = 0,
    THUMB2_STREX,
    THUMB2_LDREXB,
    THUMB2_LDREXH,
    THUMB2_LDREXD,
    THUMB2_STREXB,
    THUMB2_STREXH,
    THUMB2_STREXD,
    THUMB2_DATA_REG, // data-processing (shifted register), AND/ORR/MOV/ADD/etc.
    THUMB2_SHIFT_REG, // data-processing (register), LSL/LSR/ASR/ROR by register
    THUMB2_BL,       // BL / BLX (32-bit Thumb-2)
    THUMB2_CLREX,
    THUMB2_NOP,      // DMB / DSB / ISB / PLI / CSDB / DBG / PLD / PLI resolve to NOP
    THUMB2_DATA_IMM, // data-processing (modified immediate), AND/ORR/MOV/ADD/etc.
    THUMB2_MOVW,
    THUMB2_MOVT,
    THUMB2_ADDW,     // plain-binary 12-bit immediate ADD/SUB (ADDW/SUBW, ADR, ADD/SUB SP)
    THUMB2_BFC,
    THUMB2_BFI,
    THUMB2_SBFX,
    THUMB2_UBFX,
    THUMB2_RBIT,
    THUMB2_MLS,
    THUMB2_SDIV,
    THUMB2_UDIV,
    THUMB2_STRHT,
    THUMB2_LDRHT,
    THUMB2_LDRSBT,
    THUMB2_LDRSHT,
    THUMB2_LDST_HS,  // halfword/signed load & store (LDRH/STRH/LDRSB/LDRSH)
    THUMB2_LDRD,     // dual load & store (LDRD/STRD) w/ explicit Rt2, imm8<<2 offset
    THUMB2_TB,       // table branch (TBB/TBH)
    THUMB2_UNDEF,    // recognized-but-unimplemented fall-through skip
    // 16-bit Thumb 2
    THUMB2_CBZ,      // CBZ / CBNZ
    THUMB2_IT,       // IT
    THUMB2_B_COND,   // b_cond_thumb (conditional B)
    THUMB2_B_2,      // b_2_thumb    (unconditional B)
    THUMB2_BLX_1,    // blx_1_thumb
    THUMB2_BL_1,     // bl_1_thumb
    THUMB2_BL_2,     // bl_2_thumb
    THUMB2_B_W,      // B (T4 unconditional), reuses the b_2_thumb handler
    THUMB2_B_COND_W, // B (T3 conditional), reuses the b_cond_thumb handler
    THUMB2_TABLE_COUNT    // number of table entries
};

inline int thumb2_table_base() {
    return static_cast<int>(arm_instruction_trans_len) - THUMB2_TABLE_COUNT;
}

// sub-operations that that THUMB2_DATA_REG and THUMB2_DATA_IMM handle
// The idea is that since these are all so similar encoding-wise and can be grouped,
// turn them into one type of op and choose the op afterwards
// (mostly in decode order)
enum Thumb2DataOp : int {
    THUMB2_AND, THUMB2_TST, THUMB2_BIC, THUMB2_ORR, 
    THUMB2_ORN, THUMB2_MOV, THUMB2_MVN, THUMB2_EOR, 
    THUMB2_TEQ, THUMB2_ADD, THUMB2_CMN, THUMB2_ADC, 
    THUMB2_SBC, THUMB2_SUB, THUMB2_CMP, THUMB2_RSB
};


// Thumb2 helper functions that mostly consist of ARM Pseudocode functions turned into real functions

// Expands a 12-bit Thumb-2 modified immediate (imm12 = i:imm3:imm8) to
// 32 bits and reports whether the carry flag was affected and, if so, its value
static u32 ThumbExpandImm(u32 imm12, bool* update_c, u32* carry) {
    if ((imm12 & 0xC00) == 0) { // imm12[11:10] == 00
        const u32 imm8 = imm12 & 0xFF;
        *update_c = false;
        *carry = 0;
        switch ((imm12 >> 8) & 0x3) {
        case 0: return imm8;
        case 1: return (imm8 << 16) | imm8;
        case 2: return (imm8 << 24) | (imm8 << 8);
        default: return (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | imm8;
        }
    }
    // unrotated = '1':imm12[6:0], then rotated right by imm12[11:7] (>= 8)
    const u32 unrot = 0x80 | (imm12 & 0x7F);
    const u32 rot = (imm12 >> 7) & 0x1F;
    const u32 result = (unrot >> rot) | (unrot << (32 - rot));
    *update_c = true;
    *carry = result >> 31;
    return result;
}

// determines the thumb2 data operation type
static unsigned int thumb2_data_op(u32 op4, u32 Rn, u32 Rd) {
    switch (op4) {
    case 0x0: return (Rd == 15) ? THUMB2_TST : THUMB2_AND;
    case 0x1: return THUMB2_BIC;
    case 0x2: return (Rn == 15) ? THUMB2_MOV : THUMB2_ORR;
    case 0x3: return (Rn == 15) ? THUMB2_MVN : THUMB2_ORN;
    case 0x4: return (Rd == 15) ? THUMB2_TEQ : THUMB2_EOR;
    case 0x8: return (Rd == 15) ? THUMB2_CMN : THUMB2_ADD;
    case 0xA: return THUMB2_ADC;
    case 0xB: return THUMB2_SBC;
    case 0xD: return (Rd == 15) ? THUMB2_CMP : THUMB2_SUB;
    case 0xE: return THUMB2_RSB;
    default:  Crash();  // !!!
    }
}

// Thumb-2 immediate shift func that combines DecodeImmShift & Shift_C functions
// for LSR/ASR/RRX, imm5 == 0 means shift by 32 & shift by 1 for ROR
static u32 ThumbShiftImm(u32 value, u32 type, u32 imm5, bool carry_in, bool* carry_out) {
    assert(imm5 < 32);  // internet says C/Cpp doesn't like it when you bit-shift by the number of bits, so just making sure
    switch (type) {
    case 0: // LSL (Logical Shift Left)
        if (imm5 == 0) { 
            *carry_out = carry_in; 
            return value; 
        }
        *carry_out = (value >> (32 - imm5)) & 1;
        return value << imm5;
    case 1: { // LSR (Logical Shift Right)
        const u32 shift = (imm5 ? imm5 : 32);
        *carry_out = ((shift == 32) ? ((value >> 31) & 1) : ((value >> (shift - 1)) & 1));
        return ((shift == 32) ? 0 : (value >> shift));
    }
    case 2: { // ASR (Arithmetic Shift Right, Arithemtic shift preserves positive/negative sign)
        const u32 shift = (imm5 ? imm5 : 32);
        if (shift == 32) {
            *carry_out = (value >> 31) & 1; 
            return ((value & 0x80000000) ? 0xFFFFFFFF : 0); 
        }
        *carry_out = (value >> (shift - 1)) & 1;
        return static_cast<u32>(static_cast<s32>(value) >> shift);
    }
    case 3: {
        if (imm5 == 0) { // RRX (Rotate Right (by one bit) and eXtend)
            // bits[31:1] are shifted right one bit with the old Carry flag being moved to bits[31]
            // Bit[0] is written to carry_out 
            *carry_out = value & 1;
            return (static_cast<u32>(carry_in) << 31) | (value >> 1);
        }
        // otherwise, just rotate by imm5
        *carry_out = ((value >> (imm5 - 1)) & 1); // ROR (ROtate Right)
        return ((value >> imm5) | (value << (32 - imm5)));
    }
    default:
        __builtin_trap();
    }
}

// Register-controlled shift (ARM Shift_C with amount = Rs[7:0])
static u32 ThumbShiftReg(u32 value, u32 type, u32 amount, bool carry_in, bool* carry_out) {
    amount &= 0xFF;
    if (amount == 0) { 
        *carry_out = carry_in; 
        return value; 
    }

    switch (type) {
    case 0: // LSL (Logical Shift Left)
        if (amount < 32) {
            *carry_out = (value >> (32 - amount)) & 1; 
            return value << amount;
        }
        *carry_out = ((amount == 32) ? (value & 1) : 0);
        return 0;
    case 1: // LSR (Logical Shift Right)
        if (amount < 32) {
            *carry_out = (value >> (amount - 1)) & 1; 
            return value >> amount;
        }
        *carry_out = ((amount == 32) ? ((value >> 31) & 1) : 0);
        return 0;
    case 2: // ASR (Arithmetic Shift Right, Arithemtic shift preserves positive/negative sign)
        if (amount < 32) {
            *carry_out = (value >> (amount - 1)) & 1;
            return static_cast<u32>(static_cast<s32>(value) >> amount);
        }
        *carry_out = (value >> 31) & 1;
        return ((value & 0x80000000) ? 0xFFFFFFFF : 0);
    case 3: { // ROR
        const u32 shift = amount & 0x1F;
        if (shift == 0) {
            // minor "C/Cpp doesn't like it when you bit-shift by the size" warning here?
            *carry_out = (value >> 31) & 1; // rotate by 32
            return value; 
        }
        *carry_out = (value >> (shift - 1)) & 1;
        return ((value >> shift) | (value << (32 - shift)));
    }
    default: 
        __builtin_trap();
    }
}
