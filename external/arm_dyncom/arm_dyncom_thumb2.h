/**
 * @file arm_dyncom_thumb2.h
 * @brief thumb2 interpreter
 * @author good afternoon
 * @version 1
 * @date 2026-08-14
 */

#pragma once

#include "common/common_types.h"
#include "arm_dyncom_thumb.h" // ThumbDecodeStatus, GetThumbInstruction
#include "arm_dyncom_trans.h" // ARM_INST_PTR, arm_instruction_trans[]

struct ARMul_State;

// decode one 32-bit thumb2 instruction at `addr`. inst = ((hw1 << 16) | hw2)
// Instead of creating an ""arm"" equivalent of a given thumb instruction like
// the thumb1 path does, instead we hijack the IR mechanism and craft our own, new
// instructions, fill them, and feed them to arm_instruction_trans[]
// thumb2 will also now handle all BL/BLX instructions :)
ThumbDecodeStatus TranslateThumb2Instruction(const ARMul_State* cpu, u32 addr, u32 inst,
                                             u32* inst_size, ARM_INST_PTR* ptr_inst_base);
