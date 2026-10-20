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

#pragma once

#include "cpu/impl/interface.h"

#include "atomic"
#include "memory"

/*
   SanityCheckCPU is a debug backend that runs both arm_dyncom and Dynarmic at 
   the same time in lock-step with the goal of ensuring parity between the two backends. 
   Dynarmic is the oracle and arm_dyncom is the subject since Dynarmic is usually right.
   For each guest instruction, pre-execute state between the two is recorded, the subject runs one
   instruction using step(), post-execute state is recorded, *oracle* runs one instruction w/ step(),
   and the post-execute state is compared (with some exceptions). More or less, this lets us see 
   if and where arm_dyncom divereged from Dynarmic.

   Getters read the subject and setters write to both so that any external state changes like thread
   setup, SVC's, WFI's, etc. are synchronized between the backends.
   Before each instruction the oracle is re-seeded with the subject's pre-execute state so that every
   instruction is a self contained single-step comparison from a known-good state.
*/
class SanityCheckCPU : public CPUInterface {
    CPUState *parent;

    CPUInterfacePtr subject; // arm_dyncom (the core under test and actually running)
    CPUInterfacePtr oracle; // dynarmic (the reference)

    std::atomic<bool> want_stop{ false };
    bool halted = false;

    // Execute one guest instruction on the primary and compare it against the oracle.
    // Returns false when the run loop should stop (svc/halt/breakpoint/stop requested)
    bool exec_one();

    // Print the register/flag diff for a divergence at pc before crash/trap
    void report_divergence(uint32_t pc, bool thumb, const CPUContext &pre,
        const CPUContext &post_p, const CPUContext &post_o);

public:
    SanityCheckCPU(CPUState *state, std::size_t processor_id, bool cpu_opt);
    ~SanityCheckCPU() override;

    int run() override;
    void stop() override;

    uint32_t get_reg(uint8_t idx) override;
    void set_reg(uint8_t idx, uint32_t val) override;

    uint32_t get_sp() override;
    void set_sp(uint32_t val) override;

    uint32_t get_pc() override;
    void set_pc(uint32_t val) override;

    uint32_t get_lr() override;
    void set_lr(uint32_t val) override;

    uint32_t get_cpsr() override;
    void set_cpsr(uint32_t val) override;

    uint32_t get_tpidruro() override;
    void set_tpidruro(uint32_t val) override;

    uint32_t get_tpidrurw() override;
    void set_tpidrurw(uint32_t val) override;

    float get_float_reg(uint8_t idx) override;
    void set_float_reg(uint8_t idx, float val) override;

    uint32_t get_fpscr() override;
    void set_fpscr(uint32_t val) override;

    CPUContext save_context() override;
    void load_context(const CPUContext &ctx) override;

    bool is_thumb_mode() override;
    int step() override;

    bool hit_breakpoint() override;
    void trigger_breakpoint() override;
    void set_log_code(bool log) override;
    void set_log_mem(bool log) override;
    bool get_log_code() override;
    bool get_log_mem() override;

    void clear_exclusive() override;
    std::size_t processor_id() const override;
    void invalidate_jit_cache(Address start, size_t length) override;
};
