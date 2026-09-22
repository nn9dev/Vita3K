/**
 * @file arm_dyncom_thumb2.cpp
 * @brief thumb2 interpreter
 * @author good afternoon
 * @version 1
 * @date 2026-08-14
 * @copyright Copyright 2026 good afternoon
 */

#include <cstddef>
#include <mutex>
#include <unordered_set>

#include <util/log.h>

#include "arm_dyncom_dec.h"
#include "arm_dyncom_thumb.h"
#include "arm_dyncom_thumb2.h"
#include "skyeye_common/armstate.h"

//#define THUMB2_DEBUG 

// A halfword is the first halfword of a 32-bit Thumb-2 instruction (that is, the
// instruction is 32-bit) when bits[15:11] are one of 0b11101, 0b11110, 0b11111
// (0x1D, 0x1E, 0x1F) (thank you ARM Reference Manual). Everything below that is 
// a 16-bit Thumb-1 instruction and is handled by the pre-existing thumb1 decoder

// The encoding of a 32-bit Thumb instruction is:
//  | 15 14 13 | 12 11 | 10 9 8 7 6 5 4 | 3 2 1 0 | 15 | 14 ... 0 |
//  | 1  1  1  |  op1  |       op2      |         | op |          |

// Rather than round-tripping through ARM encodings (which is impossible for some encodings), 
// each recognised instruction fills the interpreter's decoded-operand struct directly and 
// dispatches through the existing arm_instruction_trans[] execution backend.
// In this manner, we are able to ""simulate"" thumb2 instructions by directly producing
// an `arm_inst`, the final structure that the interp/translator operates on

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

// Build a compatible ARM single-data Load/Store operation from the thumb2 version
// returns 0 on out-of-scope cases
static u32 BuildArmSingleLdSt(const u32 hw1, const u32 hw2) {
    const bool load = (hw1 >> 4) & 0x1;
    const bool size = (((hw1 >> 5) & 0x3) == 0); // hw1[6:5]: 0=byte, 2=word
    const u32 Rn = hw1 & 0xF;
    const u32 Rt = (hw2 >> 12) & 0xF;
    

    // Rt==1111 on a load is a PLD/PLI hint, not and LDR(B)
    if (load && Rt == 0xF)
        return 0;

    // ARM single-data-transfer skeleton: cond=AL, bits[27:26]=01, byte (B) and L set here
    u32 arm = 0xE4000000u | ((size ? 1u : 0u) << 22) | ((load ? 1u : 0u) << 20) | (Rt << 12);

    if (Rn == 0xF) {
        // literal version (PC-relative): U=hw1[7], P=1, W=0, imm12
        arm |= (1u << 24) | (((hw1 >> 7) & 1u) << 23) | (15u << 16) | (hw2 & 0xFFF);
    } else if ((hw1 >> 7) & 1) {
        // imm12 positive offset version (T3 word / T2 byte): P=1, U=1, W=0
        arm |= (1u << 24) | (1u << 23) | (Rn << 16) | (hw2 & 0xFFF);
    } else if ((hw2 & 0x0F00) == 0x0E00) {
        // Unprivileged *T (LDRT/STRT/LDRBT/STRBT) get sent to the normal unprivileged versions
        arm |= (1u << 24) | (1u << 23) | (Rn << 16) | (hw2 & 0xFF);
    } else if ((hw2 >> 11) & 1) {
        // imm8 version with index/wback (T4 word / T3 byte): hw2 = 1 P U W imm8
        const u32 P = (hw2 >> 10) & 1;
        const u32 U = (hw2 >> 9) & 1;
        // Thumb post-index is P=0,W=1, ARM post-index writes back with W=0
        // (P=0,W=1 is ARM's unprivileged LDRT so be careful)
        const u32 W = P ? ((hw2 >> 8) & 1) : 0;
        arm |= (P << 24) | (U << 23) | (W << 21) | (Rn << 16) | (hw2 & 0xFF);
    } else if ((hw2 & 0x0FC0) == 0) {
        // register version (T2): hw2 = Rt:000000:imm2:Rm, LSL #imm2, P=1, U=1, W=0 (type=LSL=0)
        arm |= (1u << 25) | (1u << 24) | (1u << 23) | (Rn << 16) |
               (((hw2 >> 4) & 0x3) << 7) | (hw2 & 0xF);
    } else {
        return 0; // malformed register form
    }
    #ifdef THUMB2_DEBUG
        LOG_INFO("Built Single LDST thumb {:X} to ARM {:X}", thumb32, arm);
    #endif
    return arm;
}

// Build an Arm block-transfer Load/Store (LDM/STM) from a thumb2 inst
static u32 BuildArmBlockLdSt(const u32 hw1, const u32 hw2) {
    const bool db = ((hw1 & 0xFFC0) == 0xE900);
    const bool load = (hw1 >> 4) & 1;
    const u32 W = (hw1 >> 5) & 1;
    const u32 Rn = hw1 & 0xF;
    #ifdef THUMB2_DEBUG
        u32 arm = 0xE8000000u | ((db ? 1u : 0u) << 24) | ((db ? 0u : 1u) << 23) |
            (W << 21) | ((load ? 1u : 0u) << 20) | (Rn << 16) | (hw2 & 0xFFFF);
        LOG_INFO("Built Block LDST thumb {:X} to ARM {:X}", thumb32, arm);
        return arm;
    #else
    return 0xE8000000u | ((db ? 1u : 0u) << 24) | ((db ? 0u : 1u) << 23) |
           (W << 21) | ((load ? 1u : 0u) << 20) | (Rn << 16) | (hw2 & 0xFFFF);
    #endif
}

// Build an ARM Multiply instruction from a thumb2 word
static u32 BuildArmMultiply(const u32 hw1, const u32 hw2) {
    const u32 Rn = hw1 & 0xF;
    const u32 Rm = hw2 & 0xF;
    const u32 Rd = (hw2 >> 8) & 0xF;    // Rd (32-bit result) / RdHi (long)
    const u32 Ra = (hw2 >> 12) & 0xF;   // Ra (accumulate) / RdLo (long); 1111 => no accumulate
    const u32 N  = (hw2 >> 5) & 1;      // operand-half select (16x16 forms)
    const u32 M  = (hw2 >> 4) & 1;      // operand-half / dual-swap / rounding select, per family
    const bool accumulate = (Ra != 0xF);

    /*
    if(accumulate) {
        switch ((hw1 >> 4) & 0xF) {   // hw1[7:4]: sub-class within the multiply region
        case 0b0:
            return 0xE0200090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn
        } 
    } else {
        switch ((hw1 >> 4) & 0xF) {   // hw1[7:4]: sub-class within the multiply region
        case 0b0:
            0xE0000090u | (Rd << 16) | (Rm << 8) | Rn; 
        } 
    }
    */
   
    switch ((hw1 >> 4) & 0xF) {   // hw1[7:4]: sub-class within the multiply region
    case 0x0: // MUL / MLA
        return accumulate ? 0xE0200090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn
                          : 0xE0000090u | (Rd << 16) |              (Rm << 8) | Rn;
    case 0x1: // SMLA<x><y> / SMUL<x><y>   (ARM bit6=M, bit5=N)
        return accumulate ? 0xE1000080u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 6) | (N << 5) | Rn
                          : 0xE1600080u | (Rd << 16) |              (Rm << 8) | (M << 6) | (N << 5) | Rn;
    case 0x2: // SMLAD / SMUAD   (ARM bit5=M)
        return accumulate ? 0xE7000010u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn
                          : 0xE700F010u | (Rd << 16) |              (Rm << 8) | (M << 5) | Rn;
    case 0x3: // SMLAW<y> / SMULW<y>   (ARM bit6=M)
        return accumulate ? 0xE1200080u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 6) | Rn
                          : 0xE12000A0u | (Rd << 16) |              (Rm << 8) | (M << 6) | Rn;
    case 0x4: // SMLSD / SMUSD   (ARM bit5=M)
        return accumulate ? 0xE7000050u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn
                          : 0xE700F050u | (Rd << 16) |              (Rm << 8) | (M << 5) | Rn;
    case 0x5: // SMMLA / SMMUL   (ARM bit5=R, R sits in hw2[4])
        return accumulate ? 0xE7500010u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn
                          : 0xE750F010u | (Rd << 16) |              (Rm << 8) | (M << 5) | Rn;
    case 0x6: // SMMLS   (ARM bit5=R)
        return 0xE75000D0u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn;
    case 0x7: // USAD8 / USADA8
        return accumulate ? 0xE7800010u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn
                          : 0xE780F010u | (Rd << 16) |              (Rm << 8) | Rn;
    // long multiply & Multiply Accumulate: RdLo == Ra field, RdHi == Rd field
    case 0x8: // SMULL
        return 0xE0C00090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;
    case 0x9:
        __builtin_trap();
    case 0xA: // UMULL
        return 0xE0800090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;
    case 0xC: // SMLAL / SMLAL<x><y> / SMLALD, split on hw2[7:4]
        switch ((hw2 >> 4) & 0xF) {
        case 0x0: // SMLAL
            return 0xE0E00090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;
        case 0x8: 
        case 0x9: 
        case 0xA: 
        case 0xB: // SMLAL<x><y>  (ARM bit6=M, bit5=N)
            return 0xE1400080u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 6) | (N << 5) | Rn;
        case 0xC: 
        case 0xD: // SMLALD  (ARM bit5=M)
            return 0xE7400010u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn;
        }
        return 0;
    case 0xD: // SMLSLD  (ARM bit5=M)
        return 0xE7400050u | (Rd << 16) | (Ra << 12) | (Rm << 8) | (M << 5) | Rn;
    case 0xE: // UMLAL / UMAAL   (hw2[7:4]: 0110 == UMAAL)
        return (((hw2 >> 4) & 0xF) == 0x6)
             ? 0xE0400090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn    // UMAAL
             : 0xE0A00090u | (Rd << 16) | (Ra << 12) | (Rm << 8) | Rn;   // UMLAL
    // 0x9 SDIV and 0xB UDIV have dedicated handlers, 0xF is unallocated
    }
    return 0;
}

// Sign extend or Zero extend with optional add
static u32 BuildArmExtend(const u32 hw1, const u32 hw2) {
    static const u32 base[6] = {
        0xE6B00070u, // SXTAH / SXTH
        0xE6F00070u, // UXTAH / UXTH
        0xE6800070u, // SXTAB16 / SXTB16
        0xE6C00070u, // UXTAB16 / UXTB16
        0xE6A00070u, // SXTAB / SXTB
        0xE6E00070u, // UXTAB / UXTB
    };
    const u32 op = (hw1 >> 4) & 0xF;
    if (op > 5)
        return 0;
    const u32 Rn  = hw1 & 0xF;
    const u32 Rd  = (hw2 >> 8) & 0xF;
    const u32 rotate = (hw2 >> 4) & 0x3;    // hw2[5:4]
    const u32 Rm  = hw2 & 0xF;
    return base[op] | (Rn << 16) | (Rd << 12) | (rotate << 10) | Rm;
}

// Parallel addition/subtraction (Thumb-2 0xFA8x-0xFAFx) to ARM word
// hw1[7:4] picks the operation (ADD/SUB/ASX/SAX, byte or halfword) and hw2[6:4] picks the prefix (S/Q/SH/U/UQ/UH)
// Both land in non-contiguous ARM fields, so we look them up.
// A 0 entry marks an unused row/flavor (return 0 falls through to UNDEF)
static u32 BuildArmParallel(const u32 hw1, const u32 hw2) {
    static const u32 op_arm[16] = {   // thumb2 hw1[7:4] = ARM op field bits[7:4]
        0, 0, 0, 0, 0, 0, 0, 0,
        0x9 /*ADD8*/, 0x1 /*ADD16*/, 0x3 /*ASX*/, 0,
        0xF /*SUB8*/, 0x7 /*SUB16*/, 0x5 /*SAX*/, 0,
    };
    static const u32 pre_arm[8] = {   // thumb2 hw2[6:4] = ARM prefix field bits[22:20]
        0x1 /*S*/, 0x2 /*Q*/, 0x3 /*SH*/, 0,
        0x5 /*U*/, 0x6 /*UQ*/, 0x7 /*UH*/, 0,
    };
    const u32 op  = op_arm[(hw1 >> 4) & 0xF];
    const u32 pre = pre_arm[(hw2 >> 4) & 0x7];
    if (op == 0 || pre == 0)
        return 0;
    const u32 Rn = hw1 & 0xF;
    const u32 Rd = (hw2 >> 8) & 0xF;
    const u32 Rm = hw2 & 0xF;
    return 0xE6000F00u | (pre << 20) | (Rn << 16) | (Rd << 12) | (op << 4) | Rm;
}

// Miscellaneous data-processing (Thumb-2 0xFA8x-0xFABx, hw2[7:6]==10) -> ARM word
// op1 = hw1[5:4], op2 = hw2[5:4]
// Covers the saturating add/sub, byte-reverse, SEL and CLZ.
// RBIT (op1==01, op2==10) keeps its dedicated handler and returns 0 here
static u32 BuildArmMisc(u32 hw1, u32 hw2) {
    const u32 Rn  = hw1 & 0xF;
    const u32 Rd  = (hw2 >> 8) & 0xF;
    const u32 Rm  = hw2 & 0xF;
    const u32 op2 = (hw2 >> 4) & 0x3;
    switch ((hw1 >> 4) & 0x3) {   // op1
    case 0x0: {   // saturating add/sub: QADD/QDADD/QSUB/QDSUB, selected by op2
        static const u32 sat[4] = { 0xE1000050u, 0xE1400050u, 0xE1200050u, 0xE1600050u };
        return sat[op2] | (Rn << 16) | (Rd << 12) | Rm;
    }
    case 0x1:   // byte-reverse: REV/REV16/REVSH (op2==10 is RBIT, handled elsewhere)
        switch (op2) {
        case 0x0: return 0xE6BF0F30u | (Rd << 12) | Rm;   // REV
        case 0x1: return 0xE6BF0FB0u | (Rd << 12) | Rm;   // REV16
        case 0x3: return 0xE6FF0FB0u | (Rd << 12) | Rm;   // REVSH
        }
        return 0;
    case 0x2:   // SEL
        return 0xE6800FB0u | (Rn << 16) | (Rd << 12) | Rm;
    case 0x3:   // CLZ
        return 0xE16F0F10u | (Rd << 12) | Rm;
    }
    return 0;
}

// Signed/unsigned saturate (Thumb-2 plain-binary-immediate 0xF3xx) to ARM word
// SSAT/USAT carry a shift (sh + imm3:imm2 -> imm5), but the xyz16 forms saturate two halfwords and have no shift
// SSAT with sh==1 and a zero shift amount is the slot the xyz16 form occupies, so (sh && shift==0) selects the halfword variant
// The caller should gate hw1 to the four saturate encodings, so here hw1[7]==1 means "unsigned"
static u32 BuildArmSat(u32 hw1, u32 hw2) {
    const u32 Rn = hw1 & 0xF;
    const u32 Rd = (hw2 >> 8) & 0xF;
    const u32 satimm = hw2 & 0x1F;
    const u32 imm5 = (((hw2 >> 12) & 0x7) << 2) | ((hw2 >> 6) & 0x3);
    const u32 sh = (hw1 >> 5) & 1;
    const bool halfword = (sh && imm5 == 0);
    const bool uns = (hw1 >> 7) & 1;
    if (halfword)   // SSAT16 / USAT16 (4-bit sat_imm, no shift)
        return (uns ? 0xE6E00F30u : 0xE6A00F30u) | ((satimm & 0xF) << 16) | (Rd << 12) | Rn;
    return (uns ? 0xE6E00010u : 0xE6A00010u)     // SSAT / USAT
         | (satimm << 16) | (Rd << 12) | (imm5 << 7) | (sh << 6) | Rn;
}

// static abuse
// because these are static dynamic definitions they always run the evaluation at program load
// this way they're not hardcoded and also always accurate :)
// I changed this to just decodearminstruction down at the bottom of translatethumb2instr so idk we'll see
// still think hardcoding might be the move
// TODO: Deliberate on this...
static const int stm_idx = [] { int i = -1; DecodeARMInstruction(0xE8800000u, &i); return i; }(); // idx 168
static const int ldm_idx = [] { int i = -1; DecodeARMInstruction(0xE8900000u, &i); return i; }(); // idx 169


ThumbDecodeStatus TranslateThumb2Instruction(const ARMul_State* cpu, u32 addr, u32 inst,
                                             u32* inst_size, ARM_INST_PTR* ptr_inst_base) {
    const u32 hw1 = GetThumbInstruction(inst, addr);
    // Second halfword sits in the high 16 bits of the fetched word when the
    // instruction is word-aligned, otherwise it is the first halfword of the
    // following word and must be read separately
    // TODO: Potential pinch-point here having to fetch a second instruction?
    const u32 hw2 = (addr & 2) ? cpu->ReadMemory16(addr + 2) : (inst >> 16);
    const u32 thumb32 = (hw1 << 16) | hw2;
    u32 dispatch_inst = thumb32;    // final instruction
    u32 arm_word = 0;
    #ifdef THUMB2_DEBUG
        LOG_INFO("Decoding Thumb32 {:X}", thumb32);
    #endif

    *inst_size = 4;

    const int base = thumb2_table_base();

    // Selected arm_instruction_trans[] slot for this encoding, -1 means "not decoded"
    int idx = -1;

    // switch statement to mirror the manual decoding i do based on the manual
    // always list statements from least value to greatest value for consistency!
    switch ((hw1 & 0x1800) >> 11) { // isolate op1 (hw1[12:11])
    case 0b01: {
        switch ((hw1 & 0xF00 ) >> 8) {   // hw1[11:8]: 0xE8..0xEF selector
        case 0x8:   // 0xE8xx: load/store multiple (IA), dual, exclusive, table branch
            switch ((hw1 & 0xF0) >> 4) {   // hw1[7:4]
            case 0x4:   // STREX (word)
                idx = base + THUMB2_STREX;
                break;
            case 0x5:   // LDREX (word)
                idx = base + (((hw2 & 0x0F00) == 0x0F00) ? THUMB2_LDREX : THUMB2_LDRD);
                break;
            case 0x8: case 0x9: case 0xA: case 0xB:   // LDM/STM (IA) T2 get sent to ARM
                arm_word = BuildArmBlockLdSt(hw1, hw2);
                break;
            case 0xC:   // store exclusive byte/half/dual (by hw2[7:4])
                switch ((hw2 & 0x0F) >> 4) {
                case 0x4: idx = base + THUMB2_STREXB; break;
                case 0x5: idx = base + THUMB2_STREXH; break;
                case 0x7: idx = base + THUMB2_STREXD; break;
                }
                break;
            case 0xD:   // load exclusive byte/half/dual (by hw2[7:0]), else TBB/TBH
                switch (hw2 & 0x00FF) {
                case 0x4F: idx = base + THUMB2_LDREXB; break;
                case 0x5F: idx = base + THUMB2_LDREXH; break;
                case 0x7F: idx = base + THUMB2_LDREXD; break;
                default:
                    if ((hw2 & 0xFFE0) == 0xF000)   // TBB/TBH: hw2 = 1111 0000 000 H Rm
                        idx = base + THUMB2_TB;
                    break;
                }
                break;
            case 0x6: case 0x7: case 0xE: case 0xF:   // LDRD/STRD (immediate, T1)
                idx = base + THUMB2_LDRD;
                break;
            // 0x0–0x3 are reserved (bit6==0 not a multiple) -> UNDEF
            default:
                Crash();    // might should be undef?
            }
            break;
        case 0x9:   // 0xE9xx: load/store multiple (DB) and dual
            switch ((hw1 >> 4) & 0xF) {   // hw1[7:4]
            case 0x0: case 0x1: case 0x2: case 0x3:   // LDMDB/STMDB T1 sent to ARM handler
                arm_word = BuildArmBlockLdSt(hw1, hw2);
                break;
            case 0x4: case 0x5: case 0x6: case 0x7:
            case 0xC: case 0xD: case 0xE: case 0xF:   // LDRD/STRD (immediate, T1)
                idx = base + THUMB2_LDRD;
                break;
            // 0x8–0xB are reserved, UNDEF, or crash
            default:
                Crash();
            }
            break;
        case 0xA:   // 0xEAxx: data-processing (shifted register) MOV/LSL/.../ADD/SUB/etc.
        case 0xB:   // 0xEBxx
            if (((hw1 >> 5) & 0xF) != 0x6) {   // op==0b0110 is PKH, handled separately
                idx = base + THUMB2_DATA_REG;
            } else {   // PKHBT/PKHTB (0xEACx) get sent to ARM PKH handler. imm5 = imm3:imm2, tb = hw2[5]
                const u32 Rn = hw1 & 0xF;
                const u32 Rd = (hw2 >> 8) & 0xF;
                const u32 Rm = hw2 & 0xF;
                const u32 imm5 = (((hw2 >> 12) & 0x7) << 2) | ((hw2 >> 6) & 0x3);
                const u32 tb = (hw2 >> 5) & 1;
                arm_word = 0xE6800010u | (Rn << 16) | (Rd << 12) | (imm5 << 7) | (tb << 6) | Rm;
            }
            break;
        case 0xC:   // 0xECxx-0xEFxx: coprocessor space (VFP/SIMD, CDP/MCR/MRC, MCRR/MRRC, LDC/STC)
        case 0xD:
        case 0xE:
        case 0xF:
        // TODO: Fix this
            // VFP / Advanced SIMD (cp10/cp11): the T1 encoding IS the ARM A2 word, so convert to ARM
            if (((hw2 >> 8) & 0xE) == 0xA) {
                arm_word = (thumb32 & 0x0FFFFFFFu) | 0xE0000000u;
                break;
            }
            // Other coprocessors, dispatched by class = hw1[11:8] (bits[27:24])
            switch ((hw1 >> 8) & 0xF) {
            case 0xE:   // CDP/MCR/MRC -> ARM handler (CP15 incl. c13 thread-ID/TLS)
                arm_word = (thumb32 & 0x0FFFFFFFu) | 0xE0000000u;
                break;
            case 0xC:   // MCRR/MRRC (hw1[7:5]==010) access nothing here -> NOP; LDC/STC deferred
                if ((hw1 & 0xE0) == 0x40)
                    idx = base + THUMB2_NOP;
                break;
            // 0xD (LDC/STC), 0xF (unallocated) -> unimplemented -> UNDEF
            }
            break;
        }
        break;
    }
    case 0b10:
        // this is the only one of the 3 op1 cases where op has bearing
        if ((hw2 & 0x8000) == 0x8000) {  // check 15th bit of second half-word (op)
            // Branches and miscellaneous control on page A6-233. hw2[14] selects
            // BL/BLX, hw2[12] (with hw2[14]==0) selects the unconditional B (T4);
            // everything else routes on hw1[10:4] (the op field).
            switch (hw2 & 0x5000) {   // isolate hw2[14] and hw2[12]
            case 0x4000:              // hw2[15:14]==11: BL (T1) / BLX (T2)
            case 0x5000:
                idx = base + THUMB2_BL;
                break;
            case 0x1000:              // hw2[15:14]==10, hw2[12]==1: B (T4, unconditional)
                idx = base + THUMB2_B_W;
                break;
            case 0x0000:              // hw2[15:14]==10, hw2[12]==0: misc control / B (T3)
                switch ((hw1 >> 4) & 0x7F) {   // op = hw1[10:4]
                case 0b0111000:   // MSR (register, T1): write Rn to CPSR/SPSR, send to ARM
                case 0b0111001:
                    if ((hw2 & 0xF000) == 0x8000) {
                        const u32 R = (hw1 >> 4) & 1;
                        arm_word = 0xE120F000u | (R << 22) | (((hw2 >> 8) & 0xF) << 16) | (hw1 & 0xF);
                    }
                    break;
                case 0b0111010:   // hints (CSDB/DBG) resolve to NOP
                    if (hw1 == 0xF3AF && (hw2 == 0x8014 || (hw2 & 0xFFF0) == 0x80F0))
                        idx = base + THUMB2_NOP;
                    break;
                case 0b0111011:   // misc control: CLREX, or DSB/DMB/ISB barriers -> NOP
                    if (hw1 == 0xF3BF) {
                        if (hw2 == 0x8F2F)
                            idx = base + THUMB2_CLREX;
                        else if ((hw2 & 0xFFF0) == 0x8F40 ||    // DSB
                                 (hw2 & 0xFFF0) == 0x8F50 ||    // DMB
                                 (hw2 & 0xFFF0) == 0x8F60)      // ISB
                            idx = base + THUMB2_NOP;
                    }
                    break;
                case 0b0111100:   // BXJ (T1): "No Jazelle" behaves as BX Rm, send to ARM
                    if (hw2 == 0x8F00)
                        arm_word = 0xE12FFF20u | (hw1 & 0xF);
                    break;
                case 0b0111110:   // MRS (T1): read CPSR/SPSR into Rd, send to ARM
                case 0b0111111:
                    if ((hw1 & 0xFFEF) == 0xF3EF && (hw2 & 0xF000) == 0x8000) {
                        const u32 R = (hw1 >> 4) & 1;
                        arm_word = 0xE10F0000u | (R << 22) | (((hw2 >> 8) & 0xF) << 12);
                    }
                    break;
                case 0b1111111:   // UDF (permanently undefined) when hw2[15:12]==1010
                    if ((hw2 & 0xF000) == 0xA000)
                        return ThumbDecodeStatus::UNDEFINED;
                    break;
                default: {
                    // B (T3, conditional); cond==111x is reserved and stays undefined
                    const u32 cond = (hw1 >> 6) & 0xF;
                    if (cond != 0xE && cond != 0xF)
                        idx = base + THUMB2_B_COND_W;
                    break;
                }
                }
                break;
            }
            break;
        }
        // isolate bits[9] of hw1 to differentiate between modified vs plain-binary-immediate
        switch ((hw1 & 0x200)) {
        case 0x000:   // Data-processing (modified immediate) on page A6-229
            switch ((hw1 & 0x1E0) >> 5) { // op (hw1[8:5])
                case 0b0000: // AND, if Rd==1111 w/ SFlag then decodes to TST
                case 0b0001: // BIC
                case 0b0010: // ORR, or MOV{S} (immediate) when Rn==1111 (A8-485/A8-517)
                case 0b0011: // ORN, if Rn==1111, decodes to MVN
                case 0b0100: // EOR, if Rd==1111 w/ SFlag then decodes to TEQ
                case 0b1000: // ADD, if Rd==1111 w/ SFlag then decodes to CMN
                case 0b1010: // ADC
                case 0b1011: // SBC
                case 0b1101: // SUB, if Rd==1111 w/ SFlag then decodes to CMP
                case 0b1110: // RSB
                    idx = base + THUMB2_DATA_IMM;
                    break;
                default:
                    return ThumbDecodeStatus::UNDEFINED;
            }
            break;
        case 0x200: // Data-processing (plain binary immediate) on page A6-232, hw1[9]==0b1 here
            switch ((hw1 >> 4) & 0x1F) {   // op = hw1[8:4]; hw1[10] is the 'i' imm bit
            case 0x00:   // ADDW / ADR (add)
            case 0x0A:   // SUBW / ADR (sub)  -- ADDW, SUBW, and ADR share a handler
                idx = base + THUMB2_ADDW;
                break;
            case 0x04:   // MOVW (16-bit immediate)
                idx = base + THUMB2_MOVW;
                break;
            case 0x0C:   // MOVT
                idx = base + THUMB2_MOVT;
                break;
            case 0x14:   // SBFX
                idx = base + THUMB2_SBFX;
                break;
            case 0x16:   // BFI, or BFC when Rn==1111
                idx = ((hw1 & 0xF) == 0xF) ? (base + THUMB2_BFC) : (base + THUMB2_BFI);
                break;
            case 0x1C:   // UBFX
                idx = base + THUMB2_UBFX;
                break;
            // SSAT/SSAT16 (op 0x10/0x12) and USAT/USAT16 (op 0x18/0x1A) both go to ARM
            case 0x10: case 0x12:
            case 0x18: case 0x1A:
                arm_word = BuildArmSat(hw1, hw2);
                break;
            }
            break;
        }
        break;
    case 0b11:
        // TODO: update this comment (I don't like it)
        // Data-processing (register): LSL/LSR/ASR/ROR (register) T2
        // hw1 == 0xFA0x/0x2x/0x4x/0x6x (type in hw1[6:5]), hw2[15:12]==1111 and hw2[7:4]==0
        // The extend/parallel siblings in this class have hw2[7:4]!=0, so they fall through
        if ((hw1 & 0xFF80) == 0xFA00 && (hw2 & 0xF0F0) == 0xF000) {
            idx = base + THUMB2_SHIFT_REG;
            break;
        }
        // Sign/zero extend (A6-243): 0xFA0x-0xFA5x, hw2 = 1111 Rd 10 rr Rm.
        if ((hw1 & 0xFF80) == 0xFA00 && (hw2 & 0xF0C0) == 0xF080) {
            arm_word = BuildArmExtend(hw1, hw2);
            if (arm_word)
                break;
        }
        // Parallel addition/subtraction (A6-247): 0xFA8x-0xFAFx, hw2 = 1111 Rd 0ppp Rm
        if ((hw1 & 0xFF80) == 0xFA80 && (hw2 & 0xF080) == 0xF000) {
            arm_word = BuildArmParallel(hw1, hw2);
            if (arm_word)
                break;
        }
        // Miscellaneous operations (A6-248): 0xFA8x-0xFABx, hw2 = 1111 Rd 10 oo Rm.
        // QADD/QSUB/QDADD/QDSUB, REV/REV16/REVSH, SEL, CLZ
        // if returned 0, fall through (probably to RBIT)
        if ((hw1 & 0xFFC0) == 0xFA80 && (hw2 & 0xF0C0) == 0xF080) {
            arm_word = BuildArmMisc(hw1, hw2);
            if (arm_word)
                break;
        }
        // RBIT (Reverse Bits)
        if ((hw1 & 0xFFF0) == 0xFA90 && (hw2 & 0xF0F0) == 0xF0A0) {
            idx = base + THUMB2_RBIT;
            break;
        }
        // MLS (multiply-subtract)
        if ((hw1 & 0xFFF0) == 0xFB00 && (hw2 & 0x00F0) == 0x0010) {
            idx = base + THUMB2_MLS;
            break;
        }
        // SDIV / UDIV
        if ((hw2 & 0xF0F0) == 0xF0F0) {
            if ((hw1 & 0xFFF0) == 0xFB90) {
                idx = base + THUMB2_SDIV;
                break;
            }
            if ((hw1 & 0xFFF0) == 0xFBB0) {
                idx = base + THUMB2_UDIV;
                break;
            }
        }
        // Multiply, multiply-accumulate, and long multiply/accumulate (A6-244/245) get sent to ARM
        // hw1 = 0xFB0x-0xFBFx
        if ((hw1 & 0xFF00) == 0xFB00) {
            arm_word = BuildArmMultiply(hw1, hw2);
            if (arm_word)
                break;
        }
        // Load/store single data item (A6-236)
        if ((hw2 & 0x0F00) == 0x0E00 && (hw1 & 0xF) != 0xF) {
            switch (hw1 & 0xFFF0) {
            case 0xF820: idx = base + THUMB2_STRHT;  break; // STRHT
            case 0xF830: idx = base + THUMB2_LDRHT;  break; // LDRHT
            case 0xF910: idx = base + THUMB2_LDRSBT; break; // LDRSBT
            case 0xF930: idx = base + THUMB2_LDRSHT; break; // LDRSHT
            }
            if (idx >= 0)
                break;
        }

        // Load/store single data item (A6-236) aka word/byte LDR/STR/LDRB/STRB
        if ((hw1 & 0xFE00) == 0xF800) {
            const u32 size = (hw1 >> 5) & 0x3;
            const bool sign = (hw1 >> 8) & 0x1;
            if (!sign && (size == 0 || size == 2)) {
                arm_word = BuildArmSingleLdSt(hw1, hw2);
            } else if (size == 1 || (size == 0 && sign)) {
                // Halfword / signed load & store
                // LDRH/STRH (size==1), LDRSB (size==0, signed), LDRSH (size==1, signed)
                const bool load = (hw1 >> 4) & 0x1;
                const u32 Rn = hw1 & 0xF;
                const u32 Rt = (hw2 >> 12) & 0xF;
                // Rt==1111 on a load is a PLD/PLI hint
                if (!(load && Rt == 0xF)) {
                    bool valid;
                    if (Rn == 0xF || ((hw1 >> 7) & 1))   // literal or imm12 positive offset
                        valid = true;
                    else if ((hw2 >> 11) & 1)            // imm8 index/wback (*T handled above)
                        valid = ((hw2 & 0x0F00) != 0x0E00);
                    else                                 // register offset
                        valid = ((hw2 & 0x0FC0) == 0);
                    if (valid) {
                        idx = base + THUMB2_LDST_HS;
                        break;
                    }
                }
            }
        }
        // PLD/PLI hints resolve to NOP
        if ((hw1 & 0x0650) == 0x0010) {
            idx = base + THUMB2_NOP;
            break;
        }
        // Coprocessor, Advanced SIMD, and Floating-point (Page A6-249)
        // Generic coprocessor T2 forms (0xFC–0xFF) of CDP2/MCR2/MRC2 get sent to arm
        // MCRR2/MRRC2 get stubbed; VFP/NEON and LDC/STC are left for the VFP stage.
        switch ((hw2 >> 8) & 0xF) {   // coprocessor number
        case 0xA: case 0xB:           // cp10/cp11 (VFP/Adv-SIMD) are left for the VFP stage
            break;
        default:
            switch ((hw1 >> 8) & 0xF) {   // instruction class = bits[27:24]
            case 0xE: {                   // CDP2/MCR2/MRC2 sent to ARM
                arm_word = (thumb32 & 0x0FFFFFFFu) | 0xE0000000u; // cond AL
                break;
            }
            case 0xC:                     // MCRR2 / MRRC2 (bits[7:5]==010)
                if ((hw1 & 0xE0) == 0x40)
                    idx = base + THUMB2_NOP;
                break;
            }
            break;
        }
        break;
    }

    // handle any instructions that will get sent to arm
    if (arm_word) {
        int aidx = -1;
        if (DecodeARMInstruction(arm_word, &aidx) == ARMDecodeStatus::SUCCESS) {
            idx = aidx;
            dispatch_inst = arm_word;
        }
    }

    if (idx < 0) {
        LogUnimplementedThumb2(addr, hw1, hw2);
        idx = base + THUMB2_UNDEF;
    }

    #ifdef THUMB2_DEBUG
        LOG_INFO("Thumb2 IDX of {}", idx);
    #endif
    *ptr_inst_base = arm_instruction_trans[idx](dispatch_inst, idx);
    return ThumbDecodeStatus::DECODED;
}
