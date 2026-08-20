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

#include <cpu/functions.h>
#include <cpu/impl/interface.h>

#include <memory>

// The ArmDynCom state defined by armstate.h is forward declared here so
// the skyeye headers (which pull in redeclared int types) stay out
// The Arm_DynCom interpreter is/was pulled from Citra (now Azahar) and refactored
// to work under the Vita3K codebase
// (also had/have to move the whole ISA up from armv6k to armv7-a but whatever)
struct ARMul_State;

class ArmDynComCPU : public CPUInterface {
    CPUState *parent;

    std::unique_ptr<ARMul_State> state;

    std::size_t core_id = 0;

    bool halted = false;
    bool break_ = false;

    bool log_mem = false;
    bool log_code = false;
    bool cpu_opt;

public:
    ArmDynComCPU(CPUState *state, std::size_t processor_id, bool cpu_opt);
    ~ArmDynComCPU() override;

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
