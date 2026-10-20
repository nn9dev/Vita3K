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

#include <cpu/impl/sanity_check_cpu.h>

#include <cpu/functions.h>
#include <cpu/impl/arm_dyncom_cpu.h>
#include <cpu/impl/dynarmic_cpu.h>
#include <cpu/state.h>

#include <mem/ptr.h>
#include <util/log.h>

#include <bit>
#include <cstdint>

// CPSR masks, add or remove as necessary if any kick up a lot of irrelevant divergences
static constexpr uint32_t CPSR_FLAGS_MASK = 0xF8000000u; // NZCVQ
static constexpr uint32_t CPSR_THUMB_MASK = 0x00000020u; // T
static constexpr uint32_t CPSR_GE_MASK = 0x000F0000u; // GE[3:0] (SIMD greater-or-equal)
static constexpr uint32_t CPSR_E_MASK = 0x00000200u; // E (load/store endianness)
static constexpr uint32_t CPSR_COMPARE_MASK = CPSR_FLAGS_MASK | CPSR_THUMB_MASK | CPSR_GE_MASK | CPSR_E_MASK;


// Due to Dynarmic doing a value compare before store in STREX, we step dynarmic first
// Otherwise if the subject were to go first it would already see the store had taken place
// and then fail. Everything else (CLREX/barriers included) compares fine subject-first.
static bool is_exclusive(uint32_t pc, bool thumb, MemState &mem) {
    if (thumb) {
        const uint16_t hw1 = *Ptr<uint16_t>(pc & ~1u).get(mem);
        const uint32_t top5 = (hw1 >> 11) & 0x1F;
        const bool is32 = top5 == 0x1D || top5 == 0x1E || top5 == 0x1F;
        if (!is32)
            return false; // no 16-bit exclusives
        const uint32_t grp = hw1 & 0xFFF0;
        if (grp == 0xE840 || grp == 0xE850 || grp == 0xE8C0)
            return true; // LDREX/STREX (word/byte/half/dual)
        if (grp == 0xE8D0) {
            // 0xE8Dn is shared by LDREXB/H/D *and* TBB/TBH. hw2 = 1111 0000 000H mmmm
            // is a table branch. Anything else here is a load-exclusive.
            const uint16_t hw2 = *Ptr<uint16_t>((pc & ~1u) + 2).get(mem);
            return (hw2 & 0xFFE0) != 0xF000;
        }
        return false;
    }
    const uint32_t inst = *Ptr<uint32_t>(pc & ~3u).get(mem);
    return (inst & 0x0F8000F0u) == 0x01800090u; // ARM LDREX/STREX family (loads and stores)
}

// The guest-return sentinel is a thumb WFI (kernel.halt_instruction).
// Both cores halt on WFI identically so the WFI itself is run through the normal compare.
// This only tells exec_one that the halt it just verified marks the end of a guest call.
static bool is_halt_sentinel(uint32_t pc, bool thumb, MemState &mem) {
    if (thumb)
        return *Ptr<uint16_t>(pc & ~1u).get(mem) == 0xBF30;
    return *Ptr<uint32_t>(pc & ~3u).get(mem) == 0xE320F003u;
}

// FPSCR masks, again add or remove as necessary if any kick up a lot of irrelevant divergences.
// Control bits excluded
static constexpr uint32_t FPSCR_FLAGS_MASK = 0xF0000000u; // N,Z,C,V
static constexpr uint32_t FPSCR_QC_MASK = 0x08000000u; // QC (cumulative saturation)
static constexpr uint32_t FPSCR_CUMEX_MASK = 0x0000009Fu; // IDC,IXC,UFC,OFC,DZC,IOC
static constexpr uint32_t FPSCR_COMPARE_MASK = FPSCR_FLAGS_MASK | FPSCR_QC_MASK | FPSCR_CUMEX_MASK;

// Two single-precision lanes are "equal" if their bits match, OR if both are NaN
static bool fp_lane_match(float fa, float fb) {
    const uint32_t a = std::bit_cast<uint32_t>(fa);
    const uint32_t b = std::bit_cast<uint32_t>(fb);
    if (a == b)
        return true;
    const auto is_nan = [](uint32_t x) {
        return (x & 0x7F800000u) == 0x7F800000u && (x & 0x007FFFFFu) != 0;
    };
    return is_nan(a) && is_nan(b);
}

static bool contexts_match(const CPUContext &a, const CPUContext &b) {
    if (a.cpu_registers != b.cpu_registers)
        return false;
    if (((a.cpsr ^ b.cpsr) & CPSR_COMPARE_MASK) != 0)
        return false;
    if (((a.fpscr ^ b.fpscr) & FPSCR_COMPARE_MASK) != 0)
        return false;
    for (std::size_t i = 0; i < a.fpu_registers.size(); ++i) {
        if (!fp_lane_match(a.fpu_registers[i], b.fpu_registers[i]))
            return false;
    }
    return true;
}

SanityCheckCPU::SanityCheckCPU(CPUState *state, std::size_t processor_id, bool cpu_opt)
    : parent(state) {
    subject = std::make_unique<ArmDynComCPU>(state, processor_id, cpu_opt);
    oracle = std::make_unique<DynarmicCPU>(state, processor_id, cpu_opt);
}

SanityCheckCPU::~SanityCheckCPU() = default;

void SanityCheckCPU::report_divergence(uint32_t pc, bool thumb, const CPUContext &pre,
    const CPUContext &post_p, const CPUContext &post_o) {
    uint16_t size = 0;
    const std::string text = disassemble(*parent, pc, thumb, &size);

    LOG_ERROR("==== SanityCheckCPU divergence ====");
    LOG_ERROR("  @ pc=0x{:08X} ({}): {}", pc, thumb ? "thumb" : "arm", text);
    for (int i = 0; i < 16; ++i) {
        if (post_p.cpu_registers[i] != post_o.cpu_registers[i])
            LOG_ERROR("  r{:<2} pre=0x{:08X}  dyncom=0x{:08X}  dynarmic=0x{:08X}  <<<",
                i, pre.cpu_registers[i], post_p.cpu_registers[i], post_o.cpu_registers[i]);
    }
    if (((post_p.cpsr ^ post_o.cpsr) & CPSR_COMPARE_MASK) != 0)
        LOG_ERROR("  cpsr pre=0x{:08X}  dyncom=0x{:08X}  dynarmic=0x{:08X}  (masked 0x{:08X})  <<<",
            pre.cpsr, post_p.cpsr, post_o.cpsr, CPSR_COMPARE_MASK);
    if (((post_p.fpscr ^ post_o.fpscr) & FPSCR_COMPARE_MASK) != 0)
        LOG_ERROR("  fpscr pre=0x{:08X}  dyncom=0x{:08X}  dynarmic=0x{:08X}  (masked 0x{:08X})  <<<",
            pre.fpscr, post_p.fpscr, post_o.fpscr, FPSCR_COMPARE_MASK);
    for (int i = 0; i < 64; ++i) {
        if (!fp_lane_match(post_p.fpu_registers[i], post_o.fpu_registers[i]))
            LOG_ERROR("  s{:<2} pre=0x{:08X}  dyncom=0x{:08X}  dynarmic=0x{:08X}  <<<",
                i, std::bit_cast<uint32_t>(pre.fpu_registers[i]),
                std::bit_cast<uint32_t>(post_p.fpu_registers[i]),
                std::bit_cast<uint32_t>(post_o.fpu_registers[i]));
    }
    LOG_ERROR("  (dyncom = arm_dyncom, the core under test; dynarmic = reference)");
    spdlog::default_logger()->flush();
}

bool SanityCheckCPU::exec_one() {
    MemState &mem = *parent->mem;
    const bool thumb = subject->is_thumb_mode();
    const uint32_t pc = subject->get_pc();

    const bool halt = is_halt_sentinel(pc, thumb, mem);
    const bool oracle_first = is_exclusive(pc, thumb, mem);
    const CPUContext pre = subject->save_context();

    // LDREX/STREX
    if (oracle_first) {
        oracle->load_context(pre);
        oracle->step();
    }

    subject->step();
    const bool p_svc = parent->svc_called;
    const uint32_t p_svcnum = parent->svc;

    if (!p_svc) {
        if (!oracle_first) {
            oracle->load_context(pre);
            oracle->step();
            // Restore the subject's SVC bookkeeping of CPUState::svc_called / svc
            // in case a diverging oracle->step() executed an SVC the subject didn't
            parent->svc_called = p_svc;
            parent->svc = p_svcnum;
        }

        const CPUContext post_p = subject->save_context();
        const CPUContext post_o = oracle->save_context();
        if (!contexts_match(post_p, post_o)) {
            report_divergence(pc, thumb, pre, post_p, post_o);
            spdlog::default_logger()->flush();
            __builtin_debugtrap();
        }
    }

    if (halt) {
        halted = true;
        return false; // WFI verified above; guest returned to the halt block
    }
    if (p_svc)
        return false;
    if (subject->hit_breakpoint())
        return false;
    return true;
}

int SanityCheckCPU::run() {
    halted = false;
    want_stop = false;
    parent->svc_called = false;
    while (!want_stop) {
        if (!exec_one())
            break;
    }
    return halted;
}

int SanityCheckCPU::step() {
    parent->svc_called = false;
    exec_one();
    return 0;
}

void SanityCheckCPU::stop() {
    want_stop = true;
    subject->stop();
    oracle->stop();
}

// Getters read the subject (the core we're debugging)
uint32_t SanityCheckCPU::get_reg(uint8_t idx) { return subject->get_reg(idx); }
uint32_t SanityCheckCPU::get_sp() { return subject->get_sp(); }
uint32_t SanityCheckCPU::get_pc() { return subject->get_pc(); }
uint32_t SanityCheckCPU::get_lr() { return subject->get_lr(); }
uint32_t SanityCheckCPU::get_cpsr() { return subject->get_cpsr(); }
uint32_t SanityCheckCPU::get_tpidruro() { return subject->get_tpidruro(); }
uint32_t SanityCheckCPU::get_tpidrurw() { return subject->get_tpidrurw(); }
float SanityCheckCPU::get_float_reg(uint8_t idx) { return subject->get_float_reg(idx); }
uint32_t SanityCheckCPU::get_fpscr() { return subject->get_fpscr(); }
CPUContext SanityCheckCPU::save_context() { return subject->save_context(); }
bool SanityCheckCPU::is_thumb_mode() { return subject->is_thumb_mode(); }
bool SanityCheckCPU::hit_breakpoint() { return subject->hit_breakpoint(); }
bool SanityCheckCPU::get_log_code() { return subject->get_log_code(); }
bool SanityCheckCPU::get_log_mem() { return subject->get_log_mem(); }
std::size_t SanityCheckCPU::processor_id() const { return subject->processor_id(); }

// Setters mirror to both
void SanityCheckCPU::set_reg(uint8_t idx, uint32_t val) {
    subject->set_reg(idx, val);
    oracle->set_reg(idx, val);
}
void SanityCheckCPU::set_sp(uint32_t val) {
    subject->set_sp(val);
    oracle->set_sp(val);
}
void SanityCheckCPU::set_pc(uint32_t val) {
    subject->set_pc(val);
    oracle->set_pc(val);
}
void SanityCheckCPU::set_lr(uint32_t val) {
    subject->set_lr(val);
    oracle->set_lr(val);
}
void SanityCheckCPU::set_cpsr(uint32_t val) {
    subject->set_cpsr(val);
    oracle->set_cpsr(val);
}
void SanityCheckCPU::set_tpidruro(uint32_t val) {
    subject->set_tpidruro(val);
    oracle->set_tpidruro(val);
}
void SanityCheckCPU::set_tpidrurw(uint32_t val) {
    subject->set_tpidrurw(val);
    oracle->set_tpidrurw(val);
}
void SanityCheckCPU::set_float_reg(uint8_t idx, float val) {
    subject->set_float_reg(idx, val);
    oracle->set_float_reg(idx, val);
}
void SanityCheckCPU::set_fpscr(uint32_t val) {
    subject->set_fpscr(val);
    oracle->set_fpscr(val);
}
void SanityCheckCPU::load_context(const CPUContext &ctx) {
    subject->load_context(ctx);
    oracle->load_context(ctx);
}
void SanityCheckCPU::set_log_code(bool log) {
    subject->set_log_code(log);
    oracle->set_log_code(log);
}
void SanityCheckCPU::set_log_mem(bool log) {
    subject->set_log_mem(log);
    oracle->set_log_mem(log);
}
void SanityCheckCPU::trigger_breakpoint() {
    subject->trigger_breakpoint();
    oracle->trigger_breakpoint();
}
void SanityCheckCPU::clear_exclusive() {
    subject->clear_exclusive();
    oracle->clear_exclusive();
}
void SanityCheckCPU::invalidate_jit_cache(Address start, size_t length) {
    subject->invalidate_jit_cache(start, length);
    oracle->invalidate_jit_cache(start, length);
}
