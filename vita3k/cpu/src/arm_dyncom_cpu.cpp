// Vita3K emulator project
// Copyright (C) 2026 Vita3K team
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#include <cpu/impl/arm_dyncom_cpu.h>
#include <cpu/state.h>

#include <arm_dyncom/arm_dyncom_interpreter.h>
#include <arm_dyncom/arm_dyncom_trans.h>
#include <arm_dyncom/skyeye_common/armstate.h>

#include <bit>
#include <cstring>
#include <limits>

// The DynCom interpreter keeps the NZCV/T flags in dedicated fields for speed and
// only syncs them with Cpsr at run-loop boundaries (LOAD_NZCVT on entry,
// SAVE_NZCVT on exit). Consequently Cpsr is authoritative outside InterpreterMainLoop,
// so all the accessors below read/write Cpsr directly, like how DynarmicCPU
// treats jit->Cpsr(). 

// Only user mode (USER32MODE) is emulated. Vita3K processes SVCs as HLE 
// rather than through ARM mode switches, so the banked
// register sets are never exercised, mirroring dynarmic's usermode model

ArmDynComCPU::ArmDynComCPU(CPUState *state, std::size_t processor_id, bool cpu_opt)
    : parent(state)
    , core_id(processor_id)
    , cpu_opt(cpu_opt) {
    this->state = std::make_unique<ARMul_State>(state->mem, USER32MODE);
    // Fast path is available only when optimizations are on,
    // it also gets turned off  whenever memory logging is enabled (set_log_mem)
    this->state->fastmem = cpu_opt;
    this->state->log_mem = log_mem;
}

ArmDynComCPU::~ArmDynComCPU() = default;

int ArmDynComCPU::run() {
    halted = false;
    break_ = false;
    parent->svc_called = false;
    state->svc_called = false;

    // Run until the interpreter stops itself (SVC, breakpoint)
    // or propmpted by stop() zeroing NumInstrsToExecute
    // dynarmic_cpu also does `1ull << 60;` so :)
    state->NumInstrsToExecute = 1ull << 60;
    InterpreterMainLoop(state.get());

    if (state->svc_called) {
        parent->svc_called = true;
        parent->svc = state->svc;
    }

    return halted;
}

int ArmDynComCPU::step() {
    parent->svc_called = false;
    state->svc_called = false;

    state->NumInstrsToExecute = 1;
    InterpreterMainLoop(state.get());

    if (state->svc_called) {
        parent->svc_called = true;
        parent->svc = state->svc;
    }

    return 0;
}

void ArmDynComCPU::stop() {
    // Ask the interpreter to bail out at the next dispatch boundary
    state->NumInstrsToExecute = 0;
}

uint32_t ArmDynComCPU::get_reg(uint8_t idx) {
    return state->Reg[idx];
}

void ArmDynComCPU::set_reg(uint8_t idx, uint32_t val) {
    state->Reg[idx] = val;
}

uint32_t ArmDynComCPU::get_sp() {
    return state->Reg[13];
}

void ArmDynComCPU::set_sp(uint32_t val) {
    state->Reg[13] = val;
}

uint32_t ArmDynComCPU::get_pc() {
    return state->Reg[15];
}

void ArmDynComCPU::set_pc(uint32_t val) {
    if (val & 1) {
        set_cpsr(get_cpsr() | 0x20);
        val = val & 0xFFFFFFFE;
    } else {
        set_cpsr(get_cpsr() & 0xFFFFFFDF);
        val = val & 0xFFFFFFFC;
    }
    state->Reg[15] = val;
}

uint32_t ArmDynComCPU::get_lr() {
    return state->Reg[14];
}

void ArmDynComCPU::set_lr(uint32_t val) {
    state->Reg[14] = val;
}

uint32_t ArmDynComCPU::get_cpsr() {
    return state->Cpsr;
}

void ArmDynComCPU::set_cpsr(uint32_t val) {
    state->Cpsr = val;
}

uint32_t ArmDynComCPU::get_tpidruro() {
    return state->CP15[CP15_THREAD_URO];
}

void ArmDynComCPU::set_tpidruro(uint32_t val) {
    state->CP15[CP15_THREAD_URO] = val;
}

float ArmDynComCPU::get_float_reg(uint8_t idx) {
    return std::bit_cast<float>(state->ExtReg[idx]);
}

void ArmDynComCPU::set_float_reg(uint8_t idx, float val) {
    state->ExtReg[idx] = std::bit_cast<uint32_t>(val);
}

uint32_t ArmDynComCPU::get_fpscr() {
    return state->VFP[VFP_FPSCR];
}

void ArmDynComCPU::set_fpscr(uint32_t val) {
    state->VFP[VFP_FPSCR] = val;
}

CPUContext ArmDynComCPU::save_context() {
    CPUContext ctx;
    ctx.cpu_registers = state->Reg;
    static_assert(sizeof(ctx.fpu_registers) == sizeof(state->ExtReg));
    std::memcpy(ctx.fpu_registers.data(), state->ExtReg.data(), sizeof(ctx.fpu_registers));
    ctx.cpsr = state->Cpsr;
    ctx.fpscr = state->VFP[VFP_FPSCR];

    return ctx;
}

void ArmDynComCPU::load_context(const CPUContext &ctx) {
    state->Reg = ctx.cpu_registers;
    static_assert(sizeof(ctx.fpu_registers) == sizeof(state->ExtReg));
    std::memcpy(state->ExtReg.data(), ctx.fpu_registers.data(), sizeof(ctx.fpu_registers));
    state->Cpsr = ctx.cpsr;
    state->VFP[VFP_FPSCR] = ctx.fpscr;
}

bool ArmDynComCPU::is_thumb_mode() {
    return state->Cpsr & 0x20;
}

bool ArmDynComCPU::hit_breakpoint() {
    return break_;
}

void ArmDynComCPU::trigger_breakpoint() {
    break_ = true;
    stop();
}

void ArmDynComCPU::set_log_code(bool log) {
    // arm_dyncom does not yet have a per-instruction translation hook like
    // dynarmic's PreCodeTranslationHook so code tracing is not wired up yet
    // Track the flag so get_log_code() stays consistent for callers
    log_code = log;
}

void ArmDynComCPU::set_log_mem(bool log) {
    log_mem = log;
    state->log_mem = log;
    // Mem logging forces the validating path,
    // so keep the direct path enabled only
    // when requested (cpu_opt) and logging is off
    state->fastmem = cpu_opt && !log;
}

bool ArmDynComCPU::get_log_code() {
    return log_code;
}

bool ArmDynComCPU::get_log_mem() {
    return log_mem;
}

void ArmDynComCPU::clear_exclusive() {
    state->UnsetExclusiveMemoryAddress();
}

std::size_t ArmDynComCPU::processor_id() const {
    return core_id;
}

void ArmDynComCPU::invalidate_jit_cache(Address start, size_t length) {
    // The interpreter caches decoded instructions keyed by guest PC and appends
    // their translations to a shared buffer. There is no range-based
    // invalidation so, as Citra's ARM_DynCom did, drop the whole decoded cache
    // and rewind the translation buffer
    state->instruction_cache.clear();
    trans_cache_buf_top = 0;
}
