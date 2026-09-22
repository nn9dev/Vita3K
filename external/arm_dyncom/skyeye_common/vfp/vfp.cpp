/*
    armvfp.c - ARM VFPv3 emulation unit
    Copyright (C) 2003 Skyeye Develop Group
    for help please send mail to <skyeye-developer@lists.gro.clinux.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Note: this file handles interface with arm core and vfp registers */

#include <arm_dyncom/common/common_types.h>
#include <util/log.h>
#include <arm_dyncom/skyeye_common/armstate.h>
#include <arm_dyncom/skyeye_common/vfp/asm_vfp.h>
#include <arm_dyncom/skyeye_common/vfp/vfp.h>

void VFPInit(ARMul_State* state) {
    state->VFP[VFP_FPSID] = VFP_FPSID_IMPLMEN << 24 | VFP_FPSID_SW << 23 | VFP_FPSID_SUBARCH << 16 |
                            VFP_FPSID_PARTNUM << 8 | VFP_FPSID_VARIANT << 4 | VFP_FPSID_REVISION;
    state->VFP[VFP_FPEXC] = 0;
    state->VFP[VFP_FPSCR] = 0;

    // ARM11 MPCore instruction register reset values.
    state->VFP[VFP_FPINST] = 0xEE000A00;
    state->VFP[VFP_FPINST2] = 0;

    // ARM11 MPCore feature register values.
    state->VFP[VFP_MVFR0] = 0x11111111;
    state->VFP[VFP_MVFR1] = 0;
}

void VMOVBRS(ARMul_State* state, u32 to_arm, u32 t, u32 n, u32* value) {
    if (to_arm) {
        *value = state->ExtReg[n];
    } else {
        state->ExtReg[n] = *value;
    }
}

void VMOVBRRD(ARMul_State* state, u32 to_arm, u32 t, u32 t2, u32 n, u32* value1, u32* value2) {
    if (to_arm) {
        *value2 = state->ExtReg[n * 2 + 1];
        *value1 = state->ExtReg[n * 2];
    } else {
        state->ExtReg[n * 2 + 1] = *value2;
        state->ExtReg[n * 2] = *value1;
    }
}
void VMOVBRRSS(ARMul_State* state, u32 to_arm, u32 t, u32 t2, u32 n, u32* value1, u32* value2) {
    if (to_arm) {
        *value1 = state->ExtReg[n + 0];
        *value2 = state->ExtReg[n + 1];
    } else {
        state->ExtReg[n + 0] = *value1;
        state->ExtReg[n + 1] = *value2;
    }
}

void VMOVI(ARMul_State* state, u32 single, u32 d, u32 imm) {
    if (single) {
        state->ExtReg[d] = imm;
    } else {
        /* Check endian please */
        state->ExtReg[d * 2 + 1] = imm;
        state->ExtReg[d * 2] = 0;
    }
}
void VMOVR(ARMul_State* state, u32 single, u32 d, u32 m) {
    if (single) {
        state->ExtReg[d] = state->ExtReg[m];
    } else {
        /* Check endian please */
        state->ExtReg[d * 2 + 1] = state->ExtReg[m * 2 + 1];
        state->ExtReg[d * 2] = state->ExtReg[m * 2];
    }
}

// IEEE-754 half-precision <-> single-precision conversion for VCVTB/VCVTT
// Half -> single is exact or a signalling NaN raises Invalid
// Single -> half honours the FPSCR rounding mode and reports Invalid/Overflow/Underflow/Inexact
// Modeled after the mbitsnbites/softfp w/ halves implementation (in turn based on Fabrice Bellard) made into self-contained functions
static u32 vfp_half_to_single(u16 half, u32* exceptions) {
    static constexpr u32 F32_INF = 0x7F800000u;
    static constexpr u32 F32_QUIET_NAN = 0x7FC00000u;
    static constexpr u32 F16_MAX_EXPONENT = 0x1F;

    const u32 sign = (u32)(half & 0x8000) << 16;
    u32 exponent = (half >> 10) & 0x1F;
    u32 mantissa = half & 0x3FF;

    if (exponent == 0) {
        if (mantissa == 0)
            return sign; // +/- zero
        // subnormal half -> normalised single
        exponent = 127 - 15 + 1;
        do {
            mantissa <<= 1;
            exponent--;
        } while (!(mantissa & 0x400));
        return sign | (exponent << 23) | ((mantissa & 0x3FF) << 13);
    }
    if (exponent == F16_MAX_EXPONENT) {
        if (mantissa == 0)
            return sign | F32_INF; // +/- infinity
        if (!(mantissa & 0x200)) // signalling NaN -> invalid
            *exceptions |= FPSCR_IOC;
        return sign | F32_QUIET_NAN | (mantissa << 13); // quiet NaN w/ payload preserved
    }
    return sign | ((exponent - 15 + 127) << 23) | (mantissa << 13); // normal
}

static u16 vfp_single_to_half(u32 single, u32 fpscr, u32* exceptions) {
    static constexpr u16 F16_INF = 0x7C00;
    static constexpr u16 F16_QUIET_NAN = 0x7E00;
    static constexpr u16 F16_MAX_EXPONENT = 0x1F;
    static constexpr u16 F32_MAX_EXPONENT = 0xFF;

    const u32 s_sign = (single >> 16) & 0x8000;
    const s32 s_exponent = (single >> 23) & 0xFF;
    const u32 s_mantissa = single & 0x7FFFFF;
    const u32 rmode = fpscr & FPSCR_RMODE_MASK; // rounding mode

    if (s_exponent == F32_MAX_EXPONENT) {
        if (s_mantissa == 0)
            return (u16)(s_sign | F16_INF); // infinity
        if (!(s_mantissa & 0x400000)) // signalling NaN -> invalid
            *exceptions |= FPSCR_IOC;
        return (u16)(s_sign | F16_QUIET_NAN | (s_mantissa >> 13)); // quiet NaN, payload truncated
    }
    if (s_exponent == 0 && s_mantissa == 0)
        return (u16)s_sign; // +/- zero

    // Half's biased exponent for a 1.f significand, plus the 24-bit significand
    s32 h_exponent = s_exponent - 127 + 15;
    u32 s_significand = s_mantissa | (s_exponent ? 0x800000u : 0u);
    if (s_exponent == 0)
        h_exponent += 1; // single subnormal

    if (h_exponent >= F16_MAX_EXPONENT) { // magnitude too large -> overflow
        *exceptions |= FPSCR_OFC | FPSCR_IXC;
        const bool to_inf = (rmode == FPSCR_ROUND_NEAREST) ||
                            (rmode == FPSCR_ROUND_PLUSINF && s_sign == 0) ||
                            (rmode == FPSCR_ROUND_MINUSINF && s_sign != 0);
        return (u16)(s_sign | (to_inf ? F16_INF : F16_INF - 0b1)); // infinity or largest finite (inf-1)
    }

    // An f32 has 23 mantissa bits, f16 has 10. Keep the top 10 and drop the bottom 13
    int shift = 13; 
    bool is_subnormal = false;
    if (h_exponent <= 0) { // result is a half subnormal (or underflows to zero)
        shift += 1 - h_exponent;
        h_exponent = 0;
        is_subnormal = true;
    }

    if (shift >= 32)
        return (u16)s_sign; // underflows to zero (all bits dropped)

    const u32 dropped_bits = s_significand & ((1u << shift) - 1);
    u32 h_significand = s_significand >> shift;
    const u32 halfway = 1u << (shift - 1);

    bool round_up;
    if (rmode == FPSCR_ROUND_NEAREST)
        round_up = (dropped_bits > halfway) || ((dropped_bits == halfway) && (h_significand & 1)); // ties to even
    else {
        // Rounds toward +inf
        if (rmode == FPSCR_ROUND_PLUSINF)
            round_up = ((dropped_bits != 0) && (s_sign == 0));
        else if (rmode == FPSCR_ROUND_MINUSINF)
            round_up = ((dropped_bits != 0) && (s_sign != 0));
        else 
            round_up = false; // TOZERO
    }
    if (round_up)
        h_significand += 1;

    if (dropped_bits != 0) {
        *exceptions |= FPSCR_IXC;
        if (is_subnormal)
            *exceptions |= FPSCR_UFC;
    }

    // Rounding can carry the significand into the next bit, creating a subnormal->normal mistake 
    // or a mantissa overflow that bumps the exponent
    if (is_subnormal)
        return (u16)(s_sign | (h_significand & 0x7FF)); // if bit10 is set the half is promoted to exp==1
    if (h_significand & 0x800) {
        h_significand >>= 1;
        h_exponent += 1;
    }
    if (h_exponent >= F16_MAX_EXPONENT) { // rounding pushed us to overflow
        *exceptions |= FPSCR_OFC;
        return (u16)(s_sign | F16_INF);
    }
    return (u16)(s_sign | (h_exponent << 10) | (h_significand & 0x3FF));
}

// VCVTB/VCVTT, half<->single conversion
// word[16]=0 half->single, else single->half
// word[7]=1 uses the top halfword [31:16] of the F16 side, else the bottom [15:0]
u32 VCVTBHS(ARMul_State* state, u32 inst) {
    const u32 d = ((inst >> 11) & 0x1E) | ((inst >> 22) & 1); // Vd:D
    const u32 m = ((inst << 1) & 0x1E) | ((inst >> 5) & 1);   // Vm:M
    const u32 op = (inst >> 16) & 1;
    const u32 top = (inst >> 7) & 1;
    u32 exceptions = 0;

    if (op == 0) { // half -> single
        const u16 half = (u16)(state->ExtReg[m] >> (top ? 16 : 0));
        state->ExtReg[d] = vfp_half_to_single(half, &exceptions);
    } else { // single -> half, merged into the selected halfword of Sd
        const u16 half = vfp_single_to_half(state->ExtReg[m], state->VFP[VFP_FPSCR], &exceptions);
        const u32 keep = state->ExtReg[d] & (top ? 0x0000FFFFu : 0xFFFF0000u);
        state->ExtReg[d] = keep | ((u32)half << (top ? 16 : 0));
    }
    return exceptions;
}

/* Miscellaneous functions */
s32 vfp_get_float(ARMul_State* state, unsigned int reg) {
    LOG_TRACE("VFP get float: s{}=[{:08x}]", reg, state->ExtReg[reg]);
    return state->ExtReg[reg];
}

void vfp_put_float(ARMul_State* state, s32 val, unsigned int reg) {
    LOG_TRACE("VFP put float: s{} <= [{:08x}]", reg, val);
    state->ExtReg[reg] = val;
}

u64 vfp_get_double(ARMul_State* state, unsigned int reg) {
    u64 result = ((u64)state->ExtReg[reg * 2 + 1]) << 32 | state->ExtReg[reg * 2];
    LOG_TRACE("VFP get double: s[{}-{}]=[{:016x}]", reg * 2 + 1, reg * 2, result);
    return result;
}

void vfp_put_double(ARMul_State* state, u64 val, unsigned int reg) {
    LOG_TRACE("VFP put double: s[{}-{}] <= [{:08x}-{:08x}]", reg * 2 + 1, reg * 2,
              (u32)(val >> 32), (u32)(val & 0xffffffff));
    state->ExtReg[reg * 2] = (u32)(val & 0xffffffff);
    state->ExtReg[reg * 2 + 1] = (u32)(val >> 32);
}

/*
 * Process bitmask of exception conditions. (from vfpmodule.c)
 */
void vfp_raise_exceptions(ARMul_State* state, u32 exceptions, u32 inst, u32 fpscr) {
    LOG_TRACE("VFP: raising exceptions {:08x}", exceptions);

    if (exceptions == VFP_EXCEPTION_ERROR) {
        LOG_CRITICAL("unhandled bounce {:x}", inst);
        Crash();
    }

    /*
     * If any of the status flags are set, update the FPSCR.
     * Comparison instructions always return at least one of
     * these flags set.
     */
    if (exceptions & (FPSCR_NFLAG | FPSCR_ZFLAG | FPSCR_CFLAG | FPSCR_VFLAG))
        fpscr &= ~(FPSCR_NFLAG | FPSCR_ZFLAG | FPSCR_CFLAG | FPSCR_VFLAG);

    fpscr |= exceptions;

    state->VFP[VFP_FPSCR] = fpscr;
}
