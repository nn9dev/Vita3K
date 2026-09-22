#pragma once
#ifdef _MSC_VER
// nonstandard extension used: zero-sized array in struct/union
#pragma warning(disable : 4200)
#endif

#include <cstddef>
#include "common/common_types.h"

struct ARMul_State;
typedef unsigned int (*shtop_fp_t)(ARMul_State* cpu, unsigned int sht_oper);

enum class TransExtData {
    COND = (1 << 0),
    NON_BRANCH = (1 << 1),
    DIRECT_BRANCH = (1 << 2),
    INDIRECT_BRANCH = (1 << 3),
    CALL = (1 << 4),
    RET = (1 << 5),
    END_OF_PAGE = (1 << 6),
    THUMB = (1 << 7),
    SINGLE_STEP = (1 << 8)
};

struct arm_inst {
    unsigned int idx;
    unsigned int cond;
    TransExtData br;
    u32 size;
    char component[0];
};

struct generic_arm_inst {
    u32 Ra;
    u32 Rm;
    u32 Rn;
    u32 Rd;
    u8 op1;
    u8 op2;
};

struct adc_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct add_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct orr_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct and_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct eor_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct bbl_inst {
    unsigned int L;
    int signed_immed_24;
    unsigned int next_addr;
    unsigned int jmp_addr;
};

struct bx_inst {
    unsigned int Rm;
};

struct blx_inst {
    union {
        s32 signed_immed_24;
        u32 Rm;
    } val;
    unsigned int inst;
};

struct clz_inst {
    unsigned int Rm;
    unsigned int Rd;
};

struct cps_inst {
    unsigned int imod0;
    unsigned int imod1;
    unsigned int mmod;
    unsigned int A, I, F;
    unsigned int mode;
};

struct clrex_inst {};

struct cpy_inst {
    unsigned int Rm;
    unsigned int Rd;
};

struct bic_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct sub_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct tst_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct cmn_inst {
    unsigned int I;
    unsigned int Rn;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct teq_inst {
    unsigned int I;
    unsigned int Rn;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct stm_inst {
    unsigned int inst;
};

struct bkpt_inst {
    u32 imm;
};

struct stc_inst {};

struct ldc_inst {};

struct swi_inst {
    unsigned int num;
};

struct cmp_inst {
    unsigned int I;
    unsigned int Rn;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct mov_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct mvn_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct rev_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int op1;
    unsigned int op2;
};

struct rsb_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct rsc_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct sbc_inst {
    unsigned int I;
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int shifter_operand;
    shtop_fp_t shtop_func;
};

struct mul_inst {
    unsigned int S;
    unsigned int Rd;
    unsigned int Rs;
    unsigned int Rm;
};

struct smul_inst {
    unsigned int Rd;
    unsigned int Rs;
    unsigned int Rm;
    unsigned int x;
    unsigned int y;
};

struct umull_inst {
    unsigned int S;
    unsigned int RdHi;
    unsigned int RdLo;
    unsigned int Rs;
    unsigned int Rm;
};

struct smlad_inst {
    unsigned int m;
    unsigned int Rm;
    unsigned int Rd;
    unsigned int Ra;
    unsigned int Rn;
    unsigned int op1;
    unsigned int op2;
};

struct smla_inst {
    unsigned int x;
    unsigned int y;
    unsigned int Rm;
    unsigned int Rd;
    unsigned int Rs;
    unsigned int Rn;
};

struct smlalxy_inst {
    unsigned int x;
    unsigned int y;
    unsigned int RdLo;
    unsigned int RdHi;
    unsigned int Rm;
    unsigned int Rn;
};

struct ssat_inst {
    unsigned int Rn;
    unsigned int Rd;
    unsigned int imm5;
    unsigned int sat_imm;
    unsigned int shift_type;
};

// BFC / BFI, BFC is BFI with Rn == 15
struct bfi_inst {
    unsigned int Rd;
    unsigned int Rn;
    unsigned int lsb;
    unsigned int msb;
};

// SBFX / UBFX
struct bfx_inst {
    unsigned int Rd;
    unsigned int Rn;
    unsigned int lsb;
    unsigned int widthm1;
};

struct umaal_inst {
    unsigned int Rn;
    unsigned int Rm;
    unsigned int RdHi;
    unsigned int RdLo;
};

struct umlal_inst {
    unsigned int S;
    unsigned int Rm;
    unsigned int Rs;
    unsigned int RdHi;
    unsigned int RdLo;
};

struct smlal_inst {
    unsigned int S;
    unsigned int Rm;
    unsigned int Rs;
    unsigned int RdHi;
    unsigned int RdLo;
};

struct smlald_inst {
    unsigned int RdLo;
    unsigned int RdHi;
    unsigned int Rm;
    unsigned int Rn;
    unsigned int swap;
    unsigned int op1;
    unsigned int op2;
};

struct mla_inst {
    unsigned int S;
    unsigned int Rn;
    unsigned int Rd;
    unsigned int Rs;
    unsigned int Rm;
};

struct mrc_inst {
    unsigned int opcode_1;
    unsigned int opcode_2;
    unsigned int cp_num;
    unsigned int crn;
    unsigned int crm;
    unsigned int Rd;
    unsigned int inst;
};

struct mcr_inst {
    unsigned int opcode_1;
    unsigned int opcode_2;
    unsigned int cp_num;
    unsigned int crn;
    unsigned int crm;
    unsigned int Rd;
    unsigned int inst;
};

struct mcrr_inst {
    unsigned int opcode_1;
    unsigned int cp_num;
    unsigned int crm;
    unsigned int rt;
    unsigned int rt2;
};

struct mrs_inst {
    unsigned int R;
    unsigned int Rd;
};

struct msr_inst {
    unsigned int field_mask;
    unsigned int R;
    unsigned int inst;
};

struct pld_inst {};

struct sxtb_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int rotate;
};

struct sxtab_inst {
    unsigned int Rd;
    unsigned int Rn;
    unsigned int Rm;
    unsigned rotate;
};

struct sxtah_inst {
    unsigned int Rd;
    unsigned int Rn;
    unsigned int Rm;
    unsigned int rotate;
};

struct sxth_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int rotate;
};

struct uxtab_inst {
    unsigned int Rn;
    unsigned int Rd;
    unsigned int rotate;
    unsigned int Rm;
};

struct uxtah_inst {
    unsigned int Rn;
    unsigned int Rd;
    unsigned int rotate;
    unsigned int Rm;
};

struct uxth_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int rotate;
};

struct cdp_inst {
    unsigned int opcode_1;
    unsigned int CRn;
    unsigned int CRd;
    unsigned int cp_num;
    unsigned int opcode_2;
    unsigned int CRm;
    unsigned int inst;
};

struct uxtb_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int rotate;
};

struct swp_inst {
    unsigned int Rn;
    unsigned int Rd;
    unsigned int Rm;
};

struct setend_inst {
    unsigned int set_bigend;
};

struct b_2_thumb {
    unsigned int imm;
};
struct b_cond_thumb {
    unsigned int imm;
    unsigned int cond;
};

struct bl_1_thumb {
    unsigned int imm;
};
struct bl_2_thumb {
    unsigned int imm;
};
struct blx_1_thumb {
    unsigned int imm;
    unsigned int instr;
};

struct thumb_cbz {
    unsigned int Rn;
    unsigned int imm;
    unsigned int nonzero;  // 0 = CBZ, 1 = CBNZ
};

struct thumb_it {
    unsigned int imm8;
};

struct pkh_inst {
    unsigned int Rm;
    unsigned int Rn;
    unsigned int Rd;
    unsigned char imm;
};

// Floating point VFPv3 structures
#define VFP_INTERPRETER_STRUCT
#include "skyeye_common/vfp/vfpinstr.cpp"
#undef VFP_INTERPRETER_STRUCT

typedef void (*get_addr_fp_t)(ARMul_State* cpu, unsigned int inst, unsigned int& virt_addr);

struct ldst_inst {
    unsigned int inst;
    get_addr_fp_t get_addr;
};

struct mov16_inst {
    unsigned int Rd;
    unsigned int imm16;
};

struct thumb2_bl_inst {
    unsigned int imm;
    unsigned int blx;
};

// enc keeps the full instruction
struct thumb2_undef_inst {
    unsigned int enc;
};

struct thumb2_ldstrex_inst {
    unsigned int Rn;
    unsigned int Rt;
    unsigned int Rd;
    unsigned int imm;
};

// For thumb2 data operations with an immediate value
struct thumb2_data_imm_inst {
    unsigned int op;       // Thumb2DataOp
    unsigned int Rd;
    unsigned int Rn;
    unsigned int imm;
    unsigned int S;
    unsigned int update_c; // whether the expanded immediate sets C
    unsigned int carry;    // the C value to apply when update_c
};

struct thumb2_data_reg_inst {
    unsigned int op;         // Thumb2DataOp
    unsigned int Rd;
    unsigned int Rn;
    unsigned int Rm;
    unsigned int shift_type; // raw type field (0=LSL,1=LSR,2=ASR,3=ROR/RRX)
    unsigned int shift_amount; // imm5 shifter
    unsigned int S;
};

// Data-processing, register-controlled shift, LSL/LSR/ASR/ROR by Rs[7:0]
struct thumb2_shift_reg_inst {
    unsigned int Rd;
    unsigned int Rm;
    unsigned int Rs;
    unsigned int shift_type; // 0=LSL,1=LSR,2=ASR,3=ROR
    unsigned int S;
};

// Plain-binary 12-bit immediate ADD/SUB (ADDW/SUBW, ADR, ADD/SUB SP)
struct thumb2_addw_inst {
    unsigned int Rd;
    unsigned int Rn;
    unsigned int imm;
    unsigned int sub; // 0 = ADD, 1 = SUB
};

// Halfword Signed Load & Store (Thumb2 LDRH/STRH/LDRSB/LDRSH)
// separated from the word/byte forms due to the thumb imm12 (need their own handler)
struct thumb2_ldst_hs_inst {
    unsigned int Rt;
    unsigned int Rn;
    unsigned int Rm;       // register-offset form
    unsigned int imm;      // immediate-offset magnitude
    unsigned int shift;    // LSL amount (imm2) for the register form
    unsigned int U;        // 1 = add offset, 0 = subtract
    unsigned int P;        // 1 = offset/pre-indexed (apply before access)
    unsigned int W;        // writeback
    unsigned int is_reg;   // register vs immediate offset
    unsigned int is_half;  // 1 = halfword (16-bit), 0 = byte (8-bit, signed load only)
    unsigned int is_signed;// sign-extend the loaded value
    unsigned int is_load;  // 1 = load, 0 = store (halfword only)
};

// Dual Load & Store with Rt2 and imm8<<2 added to the thumb version
struct thumb2_ldrd_inst {
    unsigned int Rt;
    unsigned int Rt2;
    unsigned int Rn;
    unsigned int imm;      // already scaled (imm8 << 2)
    unsigned int U;        // 1 = add offset
    unsigned int P;        // 1 = index (apply offset before access)
    unsigned int W;        // writeback
    unsigned int is_load;  // 1 = LDRD, 0 = STRD
};

// Thumb2 Table branch (TBB/TBH)
struct thumb2_tb_inst {
    unsigned int Rn;
    unsigned int Rm;
    unsigned int is_half; // 1 = TBH (halfword table), 0 = TBB (byte table)
};

typedef arm_inst* ARM_INST_PTR;
typedef ARM_INST_PTR (*transop_fp_t)(unsigned int, int);

extern const transop_fp_t arm_instruction_trans[];
extern const std::size_t arm_instruction_trans_len;

#define TRANS_CACHE_SIZE (64 * 1024 * 2000)
extern char trans_cache_buf[TRANS_CACHE_SIZE];
extern std::size_t trans_cache_buf_top;
