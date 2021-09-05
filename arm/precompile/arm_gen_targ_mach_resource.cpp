/*@
Copyright (c) 2013-2014, Su Zhenyu steven.known@gmail.com

All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
#include "../../opt/targ_const_info.h"
#include "../../com/xcominc.h"
#include "../../xgen/xgeninc.h"

static FILE * g_output = nullptr;
static List<EquORTypes*> g_or_types_list;
static List<RegFileSet*> g_regfile_set_list;
static List<RegSet*> g_reg_set_list;
static List<SRDescGroup<>*> g_sr_desc_group_list;
static List<SRDesc*> g_sr_desc_list;
static List<UINT*> g_uint_buffer_list;
static CHAR const* g_reg_result_cyc_buf_name = "reg_result_cyc_buf";
static CHAR const* g_mem_result_cyc_buf_name = "mem_result_cyc_buf";
static CHAR const* g_robs_empty_name = "robs_empty";

static CHAR const* g_cluster2regset_allocable_name =
    "g_cluster2regset_allocable";
static CHAR const* g_reg2regfile_name = "g_reg2regfile";
static CHAR const* g_regfile2regset_allocable_name =
    "g_regfile2regset_allocable";
static CHAR const* g_regfile2regset_name = "g_regfile2regset";
static CHAR const* g_or_type_desc_name = "g_or_type_desc";
static CHAR const* g_cluster2regset_name = "g_cluster2regset";

#define CYCLE_OF_EX1 1
#define CYCLE_OF_EX2 2
#define CYCLE_OF_EX3 3
#define CYCLE_OF_WB 4 //write back stage

//Each of group initializing element must be placed conforming
//to the order of OR_TYPE corelated.
static ORTypeDesc g_or_type_desc [] = {
    {OR_UNDEF, "UNDEF", },

    //Scalar Unit - Branch/Jump/Repeat Instruction
    {OR_b,         "b",          }, // (px)b label
    {OR_bl,        "bl",         }, // (px)bl Rd, label
    {OR_blx,       "blx",        }, // (px)blx Rd, Rm
    {OR_bx,        "bx",         }, // (px)bx Rs1

    {OR_mov,       "mov",        }, // (px)mov Rd, Rs
    {OR_mov_i,     "mov_i",      }, // (px)mov Rd, Imm16
    {OR_movw_i,    "movw_i",     }, // (px)movw Rd, Imm16
    {OR_movt_i,    "movt_i",     }, // (px)movt Rd, Imm16
    {OR_mov32_i,   "mov32_i",    }, // (px)mov32 Rd, Imm32
    {OR_mvn,       "mvn",        }, // (px)mvn Rd, Rs
    {OR_mvn_i,     "mvn_i",      }, // (px)mvn Rd, Imm16

    {OR_cmp,       "cmp",        }, // (px)cmp Rd, Rs1, Rs2
    {OR_cmp_i,     "cmp_i",      }, // (px)cmp Rd, Rs1, Imm32
    {OR_cmn,       "cmn",        }, // (px)cmn Rd, Rs1, Imm32
    {OR_cmn_i,     "cmn_i",      }, // (px)cmn Rd, Rs1, Imm32

    {OR_tst,       "tst",        }, // (px)tst Cpsr, Rs1, Rs2
    {OR_tst_i,     "tst_i",      }, // (px)tst Cpsr, Rs1, Imm32
    {OR_teq,       "teq",        }, // (px)teq Cpsr, Rs1, Rs22
    {OR_teq_i,     "teq_i",      }, // (px)teq Cpsr, Rs1, Imm32

    {OR_add,       "add",        }, // (px)add Rd, Rs1, Rs2
    {OR_adds,      "adds",       }, // (px)add Rd, cpsr, Rs1, Rs2
    {OR_adc,       "adc",        }, // (px)adc Rd, Rs1, Rs2, Cpsr
    {OR_adcs,      "adcs",       }, // (px)adc Rd, Cpsr, Rs1, Rs2, Cpsr
    {OR_sub,       "sub",        }, // (px)sub Rd, Rs1, Rs2
    {OR_subs,      "subs",       }, // (px)sub Rd, Cpsr, Rs1, Rs2
    {OR_sbc,       "sbc",        }, // (px)sbc Rd, Rs1, Rs2, Cpsr
    {OR_sbcs,      "sbcs",       }, // (px)sbc Rd, Cpsr, Rs1, Rs2, Cpsr
    {OR_add_i,     "add_i",      }, // (px)add Rd, Rs1, Imm12
    {OR_adds_i,    "adds_i",     }, // (px)add Rd, cpsr, Rs1, Imm8
    {OR_adc_i,     "adc_i",      }, // (px)adc Rd, Rs1, Imm32, Cpsr
    {OR_adcs_i,    "adcs_i",     }, // (px)adc Rd, Cpsr, Rs1, Imm32, Cpsr
    {OR_sub_i,     "sub_i",      }, // (px)sub Rd, Rs1, Imm32
    {OR_subs_i,    "subs_i",     }, // (px)sub Rd, Cpsr, Rs1, Imm32
    {OR_sbc_i,     "sbc_i",      }, // (px)sbc Rd, Rs1, Imm32, Cpsr
    {OR_sbcs_i,    "sbcs_i",     }, // (px)sbc Rd, Cpsr, Rs1, Imm32, Cpsr

    {OR_rsb,       "rsb",        }, // (px)rsb Rd, Rs1, Rs2
    {OR_rsbs,      "rsbs",       }, // (px)rsb Rd, cpsr, Rs1, Rs2
    {OR_rsc,       "rsc",        }, // (px)rsc Rd, Rs1, Rs2, Cpsr
    {OR_rscs,      "rscs",       }, // (px)rsc Rd, Cpsr, Rs1, Rs2, Cpsr
    {OR_rsb_i,     "rsb_i",      }, // (px)rsb Rd, Rs1, Imm32
    {OR_rsbs_i,    "rsbs_i",     }, // (px)rsb Rd, cpsr, Rs1, Imm32
    {OR_rsc_i,     "rsc_i",      }, // (px)rsc Rd, Rs1, Imm32, Cpsr
    {OR_rscs_i,    "rscs_i",     }, // (px)rsc Rd, Cpsr, Rs1, Imm32, Cpsr

    {OR_and,       "and",        }, // (px)and Rd, Rs1, Rs2
    {OR_ands_asr_i,"ands_asr_i", }, // (px)ands Rd, Rs1, Rs2, asr, Imm6
    {OR_and_i,     "and_i",      }, // (px)and Rd, Rs1, Imm32
    {OR_orr,       "orr",        }, // (px)orr Rd, Rs1, Rs2
    {OR_orrs,      "orrs",       }, // (px)orrs Rd, Cpsr, Rs1, Rs2
    {OR_orr_i,     "orr_i",      }, // (px)orr Rd, Rs1, Imm32
    {OR_orr_lsr_i, "orr_lsr_i",  }, // (px)orr Rd, Rs1, Rs2, lsr, Imm5
    {OR_orr_lsl_i, "orr_lsl_i",  }, // (px)orr Rd, Rs1, Rs2, lsr, Imm5
    {OR_eor,       "eor",        }, // (px)eor Rd, Rs1, Rs2
    {OR_eor_i,     "eor_i",      }, // (px)eor Rd, Rs1, Imm32
    {OR_bic,       "bic",        }, // (px)bic Rd, Rt2, Rn

    {OR_mul,       "mul",        }, // (px)mul Rd, Rs1, Rs2
    {OR_mla,       "mla",        }, // (px)mla Rd, Rs1, Rs2, Ra
    {OR_smull,     "smull",      }, // (px)smull RdLo, RdHi, Rs1, Rs2
    {OR_smlal,     "smlal",      }, // (px)smlal RdLo, RdHi, Rs1, Rs2
    {OR_umull,     "umull",      }, // (px)umull RdLo, RdHi, Rs1, Rs2
    {OR_umlal,     "umlal",      }, // (px)umlal RdLo, RdHi, Rs1, Rs2
    {OR_mrs,       "mrs",        }, // (px)mrs Rd, Cpsr
    {OR_msr,       "msr",        }, // (px)mrs Cpsr, Rs

    //swp, swpb:
    // Rn: contains the address in memory. Rn must be a different
    // register from both Rt and Rt2. Rn must not be PC.
    // Rt must not be PC.
    // Rt2 can be the same register as Rt. Rt2 must not be PC.
    {OR_swp,       "swp",        }, // (px)swp Rt, Rt2, [Rn]
    {OR_swpb,      "swpb",       }, // (px)swpb Rt, Rt2, [Rn]

    {OR_lsl,       "lsl",        }, // (px)lsl Rd, Rs1, Rs2
    {OR_lsl_i,     "lsl_i",      }, // (px)lsl Rd, Rs1, Imm5
    {OR_lsl_i32,   "lsl_i32",    }, // (px)lsl Rd, Rs1, Imm32
    {OR_lsr,       "lsr",        }, // (px)lsr Rd, Rs1, Rs2
    {OR_lsr_i,     "lsr_i",      }, // (px)lsr Rd, Rs1, Imm5
    {OR_lsr_i32,   "lsr_i32",    }, // (px)lsr Rd, Rs1, Imm32
    {OR_asl,       "asl",        }, // (px)asl Rd, Rs1, Rs2
    {OR_asl_i,     "asl_i",      }, // (px)asl Rd, Rs1, Imm5
    {OR_asr,       "asr",        }, // (px)asr Rd, Rs1, Rs2
    {OR_asr_i,     "asr_i",      }, // (px)asr Rd, Rs1, Imm5
    {OR_asr_i32,   "asr_i32",    }, // (px)asr Rd, Rs1, Imm32
    {OR_ror,       "ror",        }, // (px)ror Rd, Rs1, Rs2
    {OR_ror_i,     "ror_i",      }, // (px)ror Rd, Rs1, Imm5
    {OR_rrx,       "rrx",        }, // (px)rrx Rd, Rs1
    {OR_neg,       "neg",        }, // (px)neg Rd, Rs1

    //Symbol load
    {OR_ldm,       "ldm",        }, // (px)ldm Rn, {Rx,Ry,...}
    {OR_ldr,       "ldr",        }, // (px)ldr Rd, label
    {OR_ldrb,      "ldrb",       }, // (px)ldr Rd, label
    {OR_ldrsb,     "ldrsb",      }, // (px)ldr Rd, label
    {OR_ldrh,      "ldrh",       }, // (px)ldr Rd, label
    {OR_ldrsh,     "ldrsh",      }, // (px)ldr Rd, label
    {OR_ldrd,      "ldrd",       }, // (px)ldrd Rd, Rd2, [Rn, Imm32]

    //Indirect load via base-register + immdediate-offset.
    {OR_ldr_i12,   "ldr_i12",    }, // (px)ldr Rd, [Rn, Imm12]
    {OR_ldrb_i12,  "ldrb_i12",   }, // (px)ldr Rd, [Rn, Imm12]
    {OR_ldrsb_i8,  "ldrsb_i8",   }, // (px)ldr Rd, [Rn, Imm8]
    {OR_ldrh_i8,   "ldrh_i8",    }, // (px)ldr Rd, [Rn, Imm8]
    {OR_ldrsh_i8,  "ldrsh_i8",   }, // (px)ldr Rd, [Rn, Imm8]
    {OR_ldrd_i8,   "ldrd_i8",    }, // (px)ldr Rd, Rd2, [Rn, Imm8]

    //Symbol store
    {OR_stm,       "stm",        }, // (px)stm Rn, {Rx,Ry,...}
    {OR_str,       "str",        }, // (px)str Rt, label
    {OR_strb,      "strb",       }, // (px)str Rt, label
    {OR_strsb,     "strsb",      }, // (px)str Rt, label
    {OR_strh,      "strh",       }, // (px)str Rt, label
    {OR_strsh,     "strsh",      }, // (px)str Rt, label
    {OR_strd,      "strd",       }, // (px)strd Rt, Rt2, label

    //Indirect store via base-register + immdediate-offset.
    {OR_str_i12,   "str_i12",    }, // (px)str Rt, [Rn, Imm12]
    {OR_strb_i12,  "strb_i12",   }, // (px)str Rt, [Rn, Imm12]
    {OR_strh_i8,   "strh_i8",    }, // (px)str Rt, [Rn, Imm8]
    {OR_strd_i8,   "strd_i8",    }, // (px)str Rt, Rt2, [Rn, Imm8]

    //simulated operation.
    {OR_ret,       "ret",        }, // (px)ret Lr
    {OR_ret1,      "ret1",       }, // (px)ret Lr, R0
    {OR_ret2,      "ret2",       }, // (px)ret Lr, R0, R1
    {OR_ret3,      "ret3",       }, // (px)ret Lr, R0, R1, R2
    {OR_ret4,      "ret4",       }, // (px)ret Lr, R0, R1, R2, R3
    {OR_nop,       "nop",        },
    {OR_asm,       "asm",        },
    {OR_spadjust_i,"spadjust_i", }, // spadjust imm
    {OR_spadjust_r,"spadjust_r", }, // spadjust Rx
    {OR_label,     "label",      },
    {OR_built_in,  "built_in",   },
    {OR_push,      "push",       }, //push {Rx}
    {OR_pop,       "pop",        }, //pop {Rx}
};


static xgen::RegFileProp g_regfile_prop [] = {
    {"RF_UNDEF", 0, 0, 0,},
    {"RF_R",     1, 0, 0,},
    {"RF_D",     0, 1, 0,},
    {"RF_Q",     0, 1, 0,},
    {"RF_S",     0, 1, 0,},
    {"RF_SP",    0, 1, 0,},
    {"RF_LR",    1, 0, 0,},
    {"RF_PC",    1, 0, 0,},
    {"RF_CPSR",  1, 0, 0,},
    {"RF_SPSR",  1, 0, 0,},
    {"RF_P",     0, 0, 1,},
};


//NOTE: It does not exist undefined SLOT.
CHAR const* g_slot_name[SLOT_NUM] = {
    "SLOT_G",
};


CHAR const* g_unit_name[UNIT_NUM] = {
    "UNIT_UNDEF",
    "UNIT_A",
};


CHAR const* g_clust_name[CLUST_NUM] = {
    "CLUST_UNDEF",
    "CLUST_SCALAR",
};


static RegFileSet * newRegFileSet()
{
    RegFileSet * rfs = new RegFileSet();
    g_regfile_set_list.append_tail(rfs);
    return rfs;
}


static RegSet * newRegSet()
{
    RegSet * rs = new RegSet();
    g_reg_set_list.append_tail(rs);
    return rs;
}


static EquORTypes * newEquORType()
{
    EquORTypes * ot = new EquORTypes();
    ot->init();
    g_or_types_list.append_tail(ot);
    return ot;
}


static SRDescGroup<> * newSRDescGroup(UINT resnum, UINT opndnum)
{
    UINT x = computeSRDescGroupSize(resnum, opndnum);
    SRDescGroup<> * sda = (SRDescGroup<>*)malloc(x);
    sda->init(resnum, opndnum);
    g_sr_desc_group_list.append_tail(sda);
    return sda;
}


static SRDesc * newSRDesc()
{
    SRDesc * sd = new SRDesc();
    sd->init();
    g_sr_desc_list.append_tail(sd);
    return sd;
}


inline static void setSRDescGroup(OR_TYPE ort, SRDescGroup<> * srdg)
{
    ASSERTN(OTD_srd_group(&g_or_type_desc[ort]) == nullptr, ("has been set"));
    OTD_srd_group(&g_or_type_desc[ort]) = srdg;
}


static void prtROBitSet(CHAR const* byte_buffer_name)
{
    fprintf(g_output,
            "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            byte_buffer_name,
            byte_buffer_name,
            byte_buffer_name,
            byte_buffer_name);
}


static void prtBitSet(xcom::BitSet const& bs, CHAR const* var_name,
                      bool is_static)
{
    ASSERT0(g_output && var_name);

    if (is_static) {
        fprintf(g_output, "static ");
    }

    fprintf(g_output, "BYTE %s [] = {", var_name);
    BYTE const* bytevec = bs.get_byte_vec();
    if (bytevec == nullptr) { goto FIN; }

    for (UINT i = 0; i < bs.get_byte_size(); i++) {
        fprintf(g_output, "0x%.2x,", bytevec[i]);
    }
FIN:
    fprintf(g_output, "};\n");
    fflush(g_output);
}


static void initAndPrtRegister(OUT xcom::BitSet & allocable)
{
    fprintf(g_output, "\n//Map regfile to allocable register set.\n");
    xcom::BitSet argument;
    xcom::BitSet return_value;
    xcom::BitSet caller_saved;
    xcom::BitSet callee_saved;

    //The standard 32-bit ARM calling convention allocates the 15
    //general-purpose registers as:
    //  r16 is CPSR(CurrentProgram Status Register).
    //  r15 is the PC.
    //  r14 is the link register LR. (The BL instruction, used in a subroutine
    //  call, stores the return address in this register).
    //  r13 is the stack pointer SP. (The Push/Pop instructions in "Thumb"
    //  operating mode use this register only).
    //  r12 is the Intra-Procedure-call scratch register.
    //  r4 to r11: used to hold local variables.
    //  r0 to r3: used to hold argument values passed to a subroutine,
    //  and also hold results returned from a subroutine.
    ////////////////////////////////////////
    //Initialize dedicated regset
    //Initializing argument registers.
    ////////////////////////////////////////
    argument.bunion(1); //r0
    argument.bunion(2); //r1
    argument.bunion(3); //r2
    argument.bunion(4); //r3

    ////////////////////////////////////////
    //Initializing function unit return value registers.
    ////////////////////////////////////////
    return_value.bunion(1); //r0
    return_value.bunion(2); //r1
    return_value.bunion(3); //r2
    return_value.bunion(4); //r3

    ////////////////////////////////////////
    //Initialize regset caller-saved.
    ////////////////////////////////////////
    //R0~R3
    for (REG reg = 1; reg <= 4; reg++) {
        caller_saved.bunion(reg);
    }

    ////////////////////////////////////////
    //Initialize regset callee-saved.
    ////////////////////////////////////////
    //R4~R11
    for (REG reg = 5; reg <= 12; reg++) {
        callee_saved.bunion(reg);
    }
    //R14(LR)
    callee_saved.bunion(REG_RETURN_ADDRESS_REGISTER);

    ////////////////////////////////////////
    //Initialize regset allocable.
    ////////////////////////////////////////
    //R4~R11
    for (REG reg = 5; reg <= 12; reg++) {
    //for (REG reg = 5; reg <= 7; reg++) {
        allocable.bunion(reg);
    }
    //R14(LR), note R14 should be saved at prolog of current function.
    allocable.bunion(REG_RETURN_ADDRESS_REGISTER);

    CHAR const* set1 = "return_value_regset";
    prtBitSet(return_value, set1, true);
    fprintf(g_output, "static ROBitSet g_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            set1, set1, set1, set1);

    CHAR const* set2 = "caller_saved_regset";
    prtBitSet(caller_saved, set2, true);
    fprintf(g_output, "static ROBitSet g_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            set2, set2, set2, set2);

    CHAR const* set3 = "callee_saved_regset";
    prtBitSet(callee_saved, set3, true);
    fprintf(g_output, "static ROBitSet g_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            set3, set3, set3, set3);

    CHAR const* set4 = "allocable_regset";
    prtBitSet(allocable, set4, true);
    fprintf(g_output, "static ROBitSet g_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            set4, set4, set4, set4);

    CHAR const* set5 = "argument_regset";
    prtBitSet(argument, set5, true);
    fprintf(g_output, "static ROBitSet g_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            set5, set5, set5, set5);
}


static void prtEmptyROBitSet()
{
    xcom::BitSet s;
    prtBitSet(s, "empty_set", true);
    fprintf(g_output,
            "static ROBitSet %s(empty_set, "
            "sizeof(empty_set) / sizeof(empty_set[0]));\n",
            g_robs_empty_name);
}


static void prtRegSetArrayDeclaration(CHAR const* array_var_name,
                                      CHAR const* byte_vec_name,
                                      UINT bound_start,
                                      UINT bound_end)
{
    //Print the export variable.
    fprintf(g_output, "static RegSet const* %s [] = {",
            array_var_name);

    //It is necessary to print empty regset.
    for (UINT i = 0; i < bound_start; i++) {
        fprintf(g_output, "(RegSet const*)&%s,", g_robs_empty_name);
    }

    for (UINT i = bound_start; i < bound_end; i++) {
        fprintf(g_output, "(RegSet const*)&robs_%s_%d,", byte_vec_name, i);
    }

    fprintf(g_output, "};\n");
}


static void initAndPrtRegFile2Allocable(xcom::BitSet const& allocable,
                                        xcom::BitSet const regfile2regset[])
{
    xcom::BitSet s;
    CHAR const* byte_vec_name = "regfile2regset_allocable";
    xcom::StrBuf buf(64);
    for (UINT i = RF_UNDEF + 1; i < RF_NUM; i++) {
        s.copy(allocable);
        s.intersect(regfile2regset[i]);

        buf.sprint("%s_%d", byte_vec_name, i);
        prtBitSet(s, buf.buf, true);
        fprintf(g_output,
                "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
                buf.buf, buf.buf, buf.buf, buf.buf);
    }

    //Print the export variable.
    prtRegSetArrayDeclaration(g_regfile2regset_allocable_name,
                              byte_vec_name, RF_UNDEF + 1, RF_NUM);
}


static void initAndPrtRegFile2RegSet(OUT xcom::BitSet regfile2regset[])
{
    //R0~R15
    for (UINT reg = 1; reg <= 16; reg++) {
        regfile2regset[RF_R].bunion(reg);
    }

    //D0~D31
    for (UINT reg = 17; reg <= 48; reg++) {
        regfile2regset[RF_D].bunion(reg);
    }

    //Q0~D15
    for (UINT reg = 49; reg <= 64; reg++) {
        regfile2regset[RF_Q].bunion(reg);
    }

    //S0~S31
    for (UINT reg = 65; reg <= 96; reg++) {
        regfile2regset[RF_S].bunion(reg);
    }

    //CPSR(rflag)
    regfile2regset[RF_CPSR].bunion(97); //

    regfile2regset[RF_P].bunion(98);  //EQ Equal
    regfile2regset[RF_P].bunion(99);  //NE Not equal
    regfile2regset[RF_P].bunion(100); //CS Carry set (identical to HS)
    regfile2regset[RF_P].bunion(101); //HS Unsigned higher or same (identical to CS)
    regfile2regset[RF_P].bunion(102); //CC Carry clear (identical to LO)
    regfile2regset[RF_P].bunion(103); //LO Unsigned lower (identical to CC)
    regfile2regset[RF_P].bunion(104); //MI Minus or negative result
    regfile2regset[RF_P].bunion(105); //PL Positive or zero result
    regfile2regset[RF_P].bunion(106); //VS Overflow
    regfile2regset[RF_P].bunion(107); //VC No overflow
    regfile2regset[RF_P].bunion(108); //HI Unsigned higher
    regfile2regset[RF_P].bunion(109); //LS Unsigned lower or same
    regfile2regset[RF_P].bunion(110); //GE Signed greater than or equal
    regfile2regset[RF_P].bunion(111); //LT Signed less than
    regfile2regset[RF_P].bunion(112); //GT Signed greater than
    regfile2regset[RF_P].bunion(113); //LE Signed less than or equal
    regfile2regset[RF_P].bunion(114); //AL Always (this is the default)
    ASSERT0(REG_RFLAG_REGISTER == 97);
    ASSERT0(REG_TRUE_PRED == 114);
    ASSERT0(REG_RETURN_ADDRESS_REGISTER == 15);
    ASSERT0(REG_R0 == 1);
    ASSERT0(REG_R1 == 2);
    ASSERT0(REG_R2 == 3);
    ASSERT0(REG_R3 == 4);
    ASSERT0(REG_SP == 14);
    ASSERT0(REG_EQ_PRED == 98 &&
            REG_NE_PRED == 99 &&
            REG_CS_PRED == 100 &&
            REG_HS_PRED == 101 &&
            REG_CC_PRED == 102 &&
            REG_LO_PRED == 103 &&
            REG_MI_PRED == 104 &&
            REG_PL_PRED == 105 &&
            REG_VS_PRED == 106 &&
            REG_VC_PRED == 107 &&
            REG_HI_PRED == 108 &&
            REG_LS_PRED == 109 &&
            REG_GE_PRED == 110 &&
            REG_LT_PRED == 111 &&
            REG_GT_PRED == 112 &&
            REG_LE_PRED == 113);
    ASSERT0(REG_LAST == 114);

    fprintf(g_output, "\n//RegFile to RegisterSet.\n");

    CHAR const* byte_vec_name = "rf2regset";
    xcom::StrBuf buf(64);
    for (UINT i = RF_UNDEF + 1; i < RF_NUM; i++) {
        buf.sprint("%s_%d", byte_vec_name, i);
        prtBitSet(regfile2regset[i], buf.buf, true);
        fprintf(g_output,
                "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
                buf.buf, buf.buf, buf.buf, buf.buf);
    }

    //Print the export variable.
    prtRegSetArrayDeclaration(g_regfile2regset_name, byte_vec_name,
                              RF_UNDEF + 1, RF_NUM);
}


static void initCluster2RegSet(xcom::BitSet const regfile2regset[],
                               OUT xcom::BitSet cluster2regset[CLUST_NUM])
{
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_P]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_R]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_D]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_S]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_Q]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_SPSR]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_CPSR]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_PC]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_LR]);
    cluster2regset[CLUST_SCALAR].bunion(regfile2regset[RF_SP]);

    fprintf(g_output, "\n//Cluster to RegisterSet.\n");

    CHAR const* byte_vec_name = "clust2regset";
    xcom::StrBuf buf(64);
    for (UINT i = CLUST_UNDEF + 1; i < CLUST_NUM; i++) {
        buf.sprint("%s_%d", byte_vec_name, i);
        prtBitSet(cluster2regset[i], buf.buf, true);
        fprintf(g_output,
                "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
                buf.buf, buf.buf, buf.buf, buf.buf);
    }

    //Print the export variable.
    prtRegSetArrayDeclaration(g_cluster2regset_name, byte_vec_name,
                              CLUST_UNDEF + 1, CLUST_NUM);
}


static void initAndPrtReg2RegFile(xcom::BitSet const regfile2regset[])
{
    fprintf(g_output, "static REGFILE %s [] = {", g_reg2regfile_name);

    for (UINT reg = REG_UNDEF; reg < REG_UNDEF + 1; reg++) {
        fprintf(g_output, "%s,", g_regfile_prop[RF_UNDEF].name);
    }

    for (UINT reg = REG_UNDEF + 1; reg < REG_NUM; reg++) {
        bool find = false;
        for (UINT rf = RF_UNDEF + 1; rf < RF_NUM; rf++) {
            if (regfile2regset[rf].is_contain(reg)) {
                find = true;
                fprintf(g_output, "%s,", g_regfile_prop[rf].name);
                break;
            }
        }
        ASSERTN(find, ("register %d does not belong to any regfile.", reg));
        DUMMYUSE(find);
    }

    fprintf(g_output, "};");
}


//Initialize the register files that can be used by each operands.
static void initSRDesc(xcom::BitSet const regfile2regset[])
{
    /////////////////////////////////////////////////////
    //Create RegFileSet which SRDesc will use.
    //r0-r15
    RegFileSet * r = newRegFileSet();
    r->bunion(RF_R);

    //pEQ-pAL
    RegFileSet * p = newRegFileSet();
    p->bunion(RF_P);

    //cpsr
    RegFileSet * cpsr = newRegFileSet();
    cpsr->bunion(RF_CPSR);

    /////////////////////////////////////////////////////
    //Create SR Descriptor of literal.
    //Indicates 4GB of address space.
    SRDesc * sr_32b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_32b_unsig_imm) = true;
    SRD_bitsize(sr_32b_unsig_imm) = 32;
    SRD_is_signed(sr_32b_unsig_imm) = false;

    //Indicates 4GB of address space.
    SRDesc * sr_32b_sig_imm = newSRDesc();
    SRD_is_imm(sr_32b_sig_imm) = true;
    SRD_bitsize(sr_32b_sig_imm) = 32;
    SRD_is_signed(sr_32b_sig_imm) = true;

    //Indicates 32MB of address space.
    SRDesc * sr_25b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_25b_unsig_imm) = true;
    SRD_bitsize(sr_25b_unsig_imm) = 25;
    SRD_is_signed(sr_25b_unsig_imm) = false;

    //Indicates 16MB byte of address space.
    SRDesc * sr_24b_sig_imm = newSRDesc();
    SRD_is_imm(sr_24b_sig_imm) = true;
    SRD_bitsize(sr_24b_sig_imm) = 24;
    SRD_is_signed(sr_24b_sig_imm) = true;

    //Indicates 16MB of address space.
    SRDesc * sr_24b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_24b_unsig_imm) = true;
    SRD_bitsize(sr_24b_unsig_imm) = 24;
    SRD_is_signed(sr_24b_unsig_imm) = false;

    //Indicates 65536 byte(64KB) of address space.
    SRDesc * sr_16b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_16b_unsig_imm) = true;
    SRD_bitsize(sr_16b_unsig_imm) = 16;
    SRD_is_signed(sr_16b_unsig_imm) = false;

    //Indicates 65536 byte of address space.
    SRDesc * sr_16b_sig_imm = newSRDesc();
    SRD_is_imm(sr_16b_sig_imm) = true;
    SRD_bitsize(sr_16b_sig_imm) = 16;
    SRD_is_signed(sr_16b_sig_imm) = true;

    //Indicates 8192 byte of address space.
    SRDesc * sr_13b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_13b_unsig_imm) = true;
    SRD_bitsize(sr_13b_unsig_imm) = 13;
    SRD_is_signed(sr_13b_unsig_imm) = false;

    //Indicates 4096 byte of address space.
    SRDesc * sr_12b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_12b_unsig_imm) = true;
    SRD_bitsize(sr_12b_unsig_imm) = 12;
    SRD_is_signed(sr_12b_unsig_imm) = false;

    //Indicates 1024 byte of address space.
    SRDesc * sr_10b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_10b_unsig_imm) = true;
    SRD_bitsize(sr_10b_unsig_imm) = 10;
    SRD_is_signed(sr_10b_unsig_imm) = false;

    //Indicates 512 byte of address space.
    SRDesc * sr_9b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_9b_unsig_imm) = true;
    SRD_bitsize(sr_9b_unsig_imm) = 9;
    SRD_is_signed(sr_9b_unsig_imm) = false;

    //Indicates 255 byte of address space.
    SRDesc * sr_8b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_8b_unsig_imm) = true;
    SRD_bitsize(sr_8b_unsig_imm) = 8;
    SRD_is_signed(sr_8b_unsig_imm) = false;

    //Indicates 64 byte of address space.
    SRDesc * sr_6b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_6b_unsig_imm) = true;
    SRD_bitsize(sr_6b_unsig_imm) = 6;
    SRD_is_signed(sr_6b_unsig_imm) = false;

    //Indicates 32 byte of address space.
    SRDesc * sr_5b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_5b_unsig_imm) = true;
    SRD_bitsize(sr_5b_unsig_imm) = 5;
    SRD_is_signed(sr_5b_unsig_imm) = false;

    //Indicates 16 byte of address space.
    SRDesc * sr_4b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_4b_unsig_imm) = true;
    SRD_bitsize(sr_4b_unsig_imm) = 4;
    SRD_is_signed(sr_4b_unsig_imm) = false;

    //Indicates 4 byte of address space.
    SRDesc * sr_2b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_2b_unsig_imm) = true;
    SRD_bitsize(sr_2b_unsig_imm) = 2;
    SRD_is_signed(sr_2b_unsig_imm) = false;

    //Indicates 0 byte of address space.
    SRDesc * sr_0b_unsig_imm = newSRDesc();
    SRD_is_imm(sr_0b_unsig_imm) = true;
    SRD_bitsize(sr_0b_unsig_imm) = 1;
    SRD_is_signed(sr_0b_unsig_imm) = false;

    //Indicates label which address 4GB memory.
    SRDesc * sr_label_32bits = newSRDesc();
    SRD_is_imm(sr_label_32bits) = true;
    SRD_bitsize(sr_label_32bits) = 32;
    SRD_is_signed(sr_label_32bits) = false;
    SRD_is_label(sr_label_32bits) = true;

    //Indicates label which address 32MB memory.
    SRDesc * sr_label_25bits = newSRDesc();
    SRD_is_imm(sr_label_25bits) = true;
    SRD_bitsize(sr_label_25bits) = 25;
    SRD_is_signed(sr_label_25bits) = false;
    SRD_is_label(sr_label_25bits) = true;

    //Indicates label list.
    SRDesc * sr_label_list = newSRDesc();
    SRD_is_label_list(sr_label_list) = true;

    /////////////////////////////////////////////////////
    //Create SR Descriptor of related RegFileSet, RegSet.
    RegSet * t = nullptr;

    //init regfile-sets for r
    SRDesc * sr_r = newSRDesc();
    SRD_valid_regfile_set(sr_r) = r;
    t = newRegSet();
    for (INT rf = r->get_first(); rf != -1; rf = r->get_next(rf)) {
        t->bunion(regfile2regset[rf]);
    }
    SRD_valid_reg_set(sr_r) = t;

    //init regfile-sets for p
    SRDesc * sr_p = newSRDesc();
    SRD_valid_regfile_set(sr_p) = p;
    t = newRegSet();
    for (INT rf = p->get_first(); rf != -1; rf = p->get_next(rf)) {
        t->bunion(regfile2regset[rf]);
    }
    SRD_valid_reg_set(sr_p) = t;

    //init regfile-sets for cpsr
    SRDesc * sr_cpsr = newSRDesc();
    SRD_valid_regfile_set(sr_cpsr) = cpsr;
    t = newRegSet();
    for (INT rf = cpsr->get_first(); rf != -1; rf = cpsr->get_next(rf)) {
        t->bunion(regfile2regset[rf]);
    }
    SRD_valid_reg_set(sr_cpsr) = t;

    //////////////////////////////////////////////////////////
    //Init the recording placeholder for each operand/result
    //of each instructions.
    //The first operand always be predicate register.
    SRDescGroup<> * sda = nullptr;

    //0 res, 2 opnd:-- <- p, 32b_unsig_imm
    sda = newSRDescGroup(0, 2);
    //opnd
    sda->set_opnd(0, sr_r);
    sda->set_opnd(1, sr_label_32bits); //label
    setSRDescGroup(OR_label, sda);

    //0 res, 2 opnd:-- <- p, r
    sda = newSRDescGroup(0, 2);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //LR
    setSRDescGroup(OR_ret, sda);

    //0 res, 3 opnd:-- <- p, label_list, r
    sda = newSRDescGroup(0, 3);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_label_list); //label list
    sda->set_opnd(2, sr_r); //branch target register
    setSRDescGroup(OR_bx, sda);

    //0 res, 3 opnd:-- <- p, +/-32MB
    sda = newSRDescGroup(0, 3);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_label_25bits); //label
    sda->set_opnd(2, sr_cpsr); //Rflag
    setSRDescGroup(OR_b, sda);

    //0 res, 3 opnd:-- <- p, r, r
    sda = newSRDescGroup(0, 3);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //LR
    sda->set_opnd(2, sr_r);
    setSRDescGroup(OR_ret1, sda);

    //0 res, 4 opnd:-- <- p, r, r, r
    sda = newSRDescGroup(0, 4);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //LR
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_r);
    setSRDescGroup(OR_ret2, sda);

    //0 res, 4 opnd:-- <- p(pred), r(base), 13b_unsig_imm(offset), r(store_val)
    sda = newSRDescGroup(0, 4);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_12b_unsig_imm); //offset
    sda->set_opnd(3, sr_r);
    setSRDescGroup(OR_str_i12, sda);
    setSRDescGroup(OR_strb_i12, sda);

    //0 res, 4 opnd:-- <- p(pred), r(base), 8b_unsig_imm(offset), r(store_val)
    sda = newSRDescGroup(0, 4);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_8b_unsig_imm); //offset
    sda->set_opnd(3, sr_r);
    setSRDescGroup(OR_strh_i8, sda);

    //0 res, 4 opnd:-- <- p(pred), r(base), 1b_unsig_imm(offset), r(store_val)
    sda = newSRDescGroup(0, 4);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_32b_unsig_imm); //offset
    sda->set_opnd(3, sr_r); //store_val
    setSRDescGroup(OR_str, sda);
    setSRDescGroup(OR_strb, sda);
    setSRDescGroup(OR_strh, sda);

    //0 res, 5 opnd: <- p, r, r, r, r
    sda = newSRDescGroup(0, 5);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //LR
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_r);
    sda->set_opnd(4, sr_r);
    setSRDescGroup(OR_ret3, sda);

    //double store
    //0 res, 5 opnd: <- p, r(base), 9b_unsig_imm(offset),
    //                  r(store_val1), r(store_val2)
    sda = newSRDescGroup(0, 5);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_8b_unsig_imm); //offset
    sda->set_opnd(3, sr_r); // Rs1 store to [base]+offset
    sda->set_opnd(4, sr_r); // Rs2 store to [base]+offset+4
    setSRDescGroup(OR_strd_i8, sda);

    //double store
    //0 res, 6 opnd: <- p, r(base), 32b_unsig_imm, r(store_val1), r(store_val2)
    sda = newSRDescGroup(0, 5);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_32b_unsig_imm); //offset
    sda->set_opnd(3, sr_r); // Rs1 store to [base]+
    sda->set_opnd(4, sr_r); // Rs2 store to [base]+4
    setSRDescGroup(OR_strd, sda);

    //0 res, 6 opnd: <- p, r, r, r, r, r
    sda = newSRDescGroup(0, 6);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //LR
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_r);
    sda->set_opnd(4, sr_r);
    sda->set_opnd(5, sr_r);
    setSRDescGroup(OR_ret4, sda);

    //1 res, 2 opnd: r <- p, label
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_label_32bits); //label
    setSRDescGroup(OR_bl, sda);

    //1 res, 2 opnd: r <- p, r
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    setSRDescGroup(OR_mov, sda); //mov Rd, Rs
    setSRDescGroup(OR_blx, sda);
    setSRDescGroup(OR_mvn, sda); //mvn Rd, Rs
    setSRDescGroup(OR_neg, sda); //neg Rd, Rs

    //1 res, 2 opnd: r <- p, 16b_unsig_imm
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_16b_unsig_imm);
    setSRDescGroup(OR_mov_i, sda);
    setSRDescGroup(OR_movw_i, sda);
    setSRDescGroup(OR_movt_i, sda);
    setSRDescGroup(OR_mvn_i, sda);

    //1 res, 2 opnd: r <- p, 32b_unsig_imm
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_32b_unsig_imm);
    setSRDescGroup(OR_mov32_i, sda);

    //1 res, 2 opnd: r <- p, cpsr
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_cpsr);
    setSRDescGroup(OR_mrs, sda);

    //1 res, 2 opnd: cpsr <- p, r
    sda = newSRDescGroup(1, 2);
    //res
    sda->set_res(0, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    setSRDescGroup(OR_msr, sda);

    //1 res, 3 opnd: sp <- p, r, sp
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r); //sp
    //opnd
    sda->set_opnd(0, sr_p); //p
    sda->set_opnd(1, sr_r); //r
    sda->set_opnd(2, sr_r); //sp
    setSRDescGroup(OR_push, sda);

    //1 res, 3 opnd: cpsr <- p, r, r
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    setSRDescGroup(OR_tst, sda);
    setSRDescGroup(OR_cmp, sda);
    setSRDescGroup(OR_cmn, sda);
    setSRDescGroup(OR_teq, sda);

    //1 res, 3 opnd: cpsr <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_32b_unsig_imm);
    setSRDescGroup(OR_tst_i, sda);
    setSRDescGroup(OR_teq_i, sda);
    setSRDescGroup(OR_cmp_i, sda);
    setSRDescGroup(OR_cmn_i, sda);

    //1 res, 3 opnd: r <- p, r, r
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    setSRDescGroup(OR_add, sda);
    setSRDescGroup(OR_mul, sda);
    setSRDescGroup(OR_sub, sda);
    setSRDescGroup(OR_rsb, sda);
    setSRDescGroup(OR_and, sda);
    setSRDescGroup(OR_orr, sda);
    setSRDescGroup(OR_eor, sda);
    setSRDescGroup(OR_bic, sda);
    setSRDescGroup(OR_lsl, sda);
    setSRDescGroup(OR_lsr, sda);
    setSRDescGroup(OR_asl, sda);
    setSRDescGroup(OR_asr, sda);
    setSRDescGroup(OR_ror, sda);

    //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_8b_unsig_imm);
    setSRDescGroup(OR_add_i, sda);
    setSRDescGroup(OR_sub_i, sda);
    setSRDescGroup(OR_rsb_i, sda);
    setSRDescGroup(OR_and_i, sda);
    setSRDescGroup(OR_orr_i, sda);
    setSRDescGroup(OR_eor_i, sda);

    //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_32b_unsig_imm); //offset
    setSRDescGroup(OR_ldr, sda);
    setSRDescGroup(OR_ldrb, sda);
    setSRDescGroup(OR_ldrsb, sda);
    setSRDescGroup(OR_ldrh, sda);
    setSRDescGroup(OR_ldrsh, sda);

    //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_12b_unsig_imm); //offset
    setSRDescGroup(OR_ldr_i12, sda);
    setSRDescGroup(OR_ldrb_i12, sda);

   //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r); //base
    sda->set_opnd(2, sr_8b_unsig_imm); //offset
    setSRDescGroup(OR_ldrsb_i8, sda);
    setSRDescGroup(OR_ldrh_i8, sda);
    setSRDescGroup(OR_ldrsh_i8, sda);

    //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_5b_unsig_imm);
    setSRDescGroup(OR_asl_i, sda);
    setSRDescGroup(OR_asr_i, sda);
    setSRDescGroup(OR_lsl_i, sda);
    setSRDescGroup(OR_lsr_i, sda);

    //1 res, 3 opnd: r <- p, r, Imm
    sda = newSRDescGroup(1, 3);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_32b_unsig_imm);
    setSRDescGroup(OR_lsl_i32, sda);
    setSRDescGroup(OR_lsr_i32, sda);
    setSRDescGroup(OR_asr_i32, sda);

    //1 res, 4 opnd: r <- p, r, r, cpsr
    sda = newSRDescGroup(1, 4);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_cpsr);
    setSRDescGroup(OR_adc, sda);
    setSRDescGroup(OR_sbc, sda);
    setSRDescGroup(OR_rsc, sda);

    //1 res, 4 opnd: r <- p, r, r, Imm
    sda = newSRDescGroup(1, 4);
    //res
    sda->set_res(0, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_5b_unsig_imm);
    setSRDescGroup(OR_orr_lsr_i, sda);
    setSRDescGroup(OR_orr_lsl_i, sda);

    //2 res, 2 opnd: r, sp <- p, sp
    sda = newSRDescGroup(2, 2);
    //res
    sda->set_res(0, sr_r); //r
    sda->set_res(1, sr_r); //sp
    //opnd
    sda->set_opnd(0, sr_p); //p
    sda->set_opnd(1, sr_r); //sp
    setSRDescGroup(OR_pop, sda);

    //2 res, 4 opnd: r, r <- p, r, r, Imm
    sda = newSRDescGroup(2, 4);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_6b_unsig_imm);
    setSRDescGroup(OR_ands_asr_i, sda);

    //2 res, 3 opnd: r, cpsr <- p, r, r
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    setSRDescGroup(OR_adds, sda);
    setSRDescGroup(OR_subs, sda);
    setSRDescGroup(OR_rsbs, sda);
    setSRDescGroup(OR_orrs, sda);

    //2 res, 3 opnd: r, cpsr <- p, r, Imm
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_8b_unsig_imm);
    setSRDescGroup(OR_adds_i, sda);
    setSRDescGroup(OR_subs_i, sda);
    setSRDescGroup(OR_rsbs_i, sda);

    //2 res, 3 opnd: r, r <- p, r, Imm
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_8b_unsig_imm);
    setSRDescGroup(OR_ldrd_i8, sda);

    //2 res, 3 opnd: r, r <- p, r, imm32
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_r);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_32b_unsig_imm);
    setSRDescGroup(OR_ldrd, sda);

    //2 res, 4 opnd: r, cpsr <- p, r, r, cpsr
    sda = newSRDescGroup(2, 4);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_r);
    sda->set_opnd(3, sr_cpsr);
    setSRDescGroup(OR_adcs, sda);
    setSRDescGroup(OR_sbcs, sda);
    setSRDescGroup(OR_rscs, sda);

    //2 res, 4 opnd: r, cpsr <- p, r, Imm, cpsr
    sda = newSRDescGroup(2, 4);
    //res
    sda->set_res(0, sr_r);
    sda->set_res(1, sr_cpsr);
    //opnd
    sda->set_opnd(0, sr_p);
    sda->set_opnd(1, sr_r);
    sda->set_opnd(2, sr_32b_unsig_imm);
    sda->set_opnd(3, sr_cpsr);
    setSRDescGroup(OR_adcs_i, sda);
    setSRDescGroup(OR_sbcs_i, sda);
    setSRDescGroup(OR_rscs_i, sda);

    //spadjust
    //2 res, 3 opnd: sp, r <- p, sp, 32bit-ofst
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r); //sp
    sda->set_res(1, sr_r); //tmp
    //opnd
    sda->set_opnd(0, sr_p); //true pred
    sda->set_opnd(1, sr_r); //sp
    sda->set_opnd(2, sr_32b_unsig_imm);
    setSRDescGroup(OR_spadjust_i, sda); //spadjust

    //spadjust
    //2 res, 3 opnd: sp, r <- p, sp, r
    sda = newSRDescGroup(2, 3);
    //res
    sda->set_res(0, sr_r); //sp
    sda->set_res(1, sr_r); //tmp
    //opnd
    sda->set_opnd(0, sr_p); //true pred
    sda->set_opnd(1, sr_r); //sp
    sda->set_opnd(2, sr_r); //addend
    setSRDescGroup(OR_spadjust_r, sda); //spadjust
}


static void initFuncUnit()
{
    for (UINT i = OR_UNDEF + 1; i < OR_LAST; i++) {
        OTD_unit(&g_or_type_desc[i]) = UNIT_A;
    }
}


//Return name of register.
static void initAndPrtRegisterName()
{
    fprintf(g_output, "\n//Register Name.\n");
    fprintf(g_output, "CHAR const* g_register_name [] = {");

    xcom::StrBuf buf(32);
    for (UINT i = REG_UNDEF; i < REG_NUM; i++) {
        REG reg = i;

        //Index of physical register starting at 1,
        //whereas 0 indicates REG_UNDEF.
        if (reg == 0) {
            buf.clean();
        } else if (reg >= 1 && reg <= 16) {
            //R0~R15
            buf.sprint("r%d", reg-1);
        } else if (reg >= 17 && reg <= 48) {
            //D0~D31
            buf.sprint("d%d", reg-17);
        } else if (reg >= 49 && reg <= 64) {
            //Q0~Q15
            buf.sprint("q%d", reg-49);
        } else if (reg >= 65 && reg <= 96) {
            //S0~S31
            buf.sprint("s%d", reg-65);
        } else if (reg == REG_RFLAG_REGISTER) {
            buf.sprint("cpsr");
        } else {
            //predicate register
            switch (reg) {
            case REG_EQ_PRED: buf.sprint("eq"); break;
            case REG_NE_PRED: buf.sprint("ne"); break;
            case REG_CS_PRED: buf.sprint("cs"); break;
            case REG_HS_PRED: buf.sprint("hs"); break;
            case REG_CC_PRED: buf.sprint("cc"); break;
            case REG_LO_PRED: buf.sprint("lo"); break;
            case REG_MI_PRED: buf.sprint("mi"); break;
            case REG_PL_PRED: buf.sprint("pl"); break;
            case REG_VS_PRED: buf.sprint("vs"); break;
            case REG_VC_PRED: buf.sprint("vc"); break;
            case REG_HI_PRED: buf.sprint("hi"); break;
            case REG_LS_PRED: buf.sprint("ls"); break;
            case REG_GE_PRED: buf.sprint("ge"); break;
            case REG_LT_PRED: buf.sprint("lt"); break;
            case REG_GT_PRED: buf.sprint("gt"); break;
            case REG_LE_PRED: buf.sprint("le"); break;
            case REG_TRUE_PRED: buf.sprint("al"); break;
            default: ASSERTN(0, ("unknown physic register"));
            }
        }
        fprintf(g_output, "\"%s\",", buf.buf);
    }

    fprintf(g_output, "};\n");
    fflush(g_output);
}


static void initEquORType()
{
    for (UINT i = OR_UNDEF + 1; i < OR_LAST; i++) {
        EquORTypes * g = newEquORType();
        EQUORTY_unit2ortype(g, UNIT_A) = (OR_TYPE)i;
        OTD_equ_or_types(&g_or_type_desc[i]) = g;
    }
}


//Initializing or-type description.
static void initORProperty()
{
    ORTypeDesc * od;

    for (INT i = OR_UNDEF + 1; i < OR_NUM; i++) {
        od = &g_or_type_desc[i];
        OTD_is_predicated(od) = true;
    }

    od = &g_or_type_desc[OR_spadjust_i];
    OTD_is_fake(od) = true; //expand OR if offset is out of range.
    OTD_is_bus(od) = true;

    od = &g_or_type_desc[OR_spadjust_r];
    OTD_is_fake(od) = true;
    OTD_is_bus(od) = true;

    od = &g_or_type_desc[OR_label];
    OTD_is_fake(od) = true;

    od = &g_or_type_desc[OR_asm];
    OTD_is_volatile(od) = true;
    OTD_is_side_effect(od) = true;
    OTD_is_asm(od) = true;

    od = &g_or_type_desc[OR_nop];
    OTD_is_nop(od) = true;

    //load
    OR_TYPE load[] = {
        OR_ldm,
        OR_ldr,
        OR_ldrb,
        OR_ldrsb,
        OR_ldrh,
        OR_ldrsh,
        OR_ldrd,
        OR_ldr_i12,
        OR_ldrb_i12,
        OR_ldrsb_i8,
        OR_ldrh_i8,
        OR_ldrsh_i8,
        OR_ldrd_i8,
    };
    for (UINT i = 0; i < sizeof(load) / sizeof(load[0]); i++) {
        od = &g_or_type_desc[load[i]];
        OTD_is_load(od) = true;
        switch (load[i]) {
        case OR_ldr:
        case OR_ldrb:
        case OR_ldrsb:
        case OR_ldrh:
        case OR_ldrsh:
        case OR_ldrd:
            OTD_is_fake(od) = true; //expand OR if offset is out of range.
            break;
        default: {}
        }
    }

    od = &g_or_type_desc[OR_lsr_i];
    OTD_is_fake(od) = true; //expand OR if shift-size is out of range.

    od = &g_or_type_desc[OR_lsr_i32];
    OTD_is_fake(od) = true; //expand OR if shift-size is out of range.

    od = &g_or_type_desc[OR_lsl_i32];
    OTD_is_fake(od) = true; //expand OR if shift-size is out of range.

    od = &g_or_type_desc[OR_asr_i32];
    OTD_is_fake(od) = true; //expand OR if shift-size is out of range.

    OR_TYPE store[] = {
        OR_stm,
        OR_str,
        OR_strb,
        OR_strsb,
        OR_strh,
        OR_strsh,
        OR_strd,
        OR_str_i12,
        OR_strb_i12,
        OR_strh_i8,
        OR_strd_i8,
    };
    for (UINT i = 0; i < sizeof(store) / sizeof(store[0]); i++) {
        od = &g_or_type_desc[store[i]];
        OTD_is_store(od) = true;
        switch (store[i]) {
        case OR_str:
        case OR_strd:
        case OR_strb:
        case OR_strsb:
        case OR_strh:
        case OR_strsh:
            OTD_is_fake(od) = true; //expand OR if offset is out of range.
            break;
        default: {}
        }
    }

    od = &g_or_type_desc[OR_bl];
    OTD_is_call(od) = true;

    od = &g_or_type_desc[OR_ret];
    OTD_is_return(od) = true;

    od = &g_or_type_desc[OR_ret1];
    OTD_is_return(od) = true;

    od = &g_or_type_desc[OR_ret2];
    OTD_is_return(od) = true;

    od = &g_or_type_desc[OR_ret3];
    OTD_is_return(od) = true;

    od = &g_or_type_desc[OR_ret4];
    OTD_is_return(od) = true;

    od = &g_or_type_desc[OR_b];
    OTD_is_cond_br(od) = true;

    od = &g_or_type_desc[OR_bx];
    OTD_is_uncond_br(od) = true;
    OTD_is_indirect_br(od) = true;

    od = &g_or_type_desc[OR_nop];
    OTD_is_nop(od) = true;

    od = &g_or_type_desc[OR_add_i];
    OTD_is_addi(od) = true;
    OTD_is_fake(od) = true; //expand OR if immediate-addend is out of range.

    od = &g_or_type_desc[OR_sub_i];
    OTD_is_subi(od) = true;
    OTD_is_fake(od) = true; //expand OR if immediate-addend is out of range.

    od = &g_or_type_desc[OR_mov_i];
    OTD_is_movi(od) = true;

    od = &g_or_type_desc[OR_mov32_i];
    OTD_is_movi(od) = true;
    OTD_is_fake(od) = true; //expand OR if immediate-addend is out of range.

    od = &g_or_type_desc[OR_movw_i];
    OTD_is_movi(od) = true;

    od = &g_or_type_desc[OR_movt_i];
    OTD_is_movi(od) = true;
}


static void fini()
{
    for (EquORTypes * ot = g_or_types_list.get_head();
         ot != nullptr; ot = g_or_types_list.get_next()) {
        delete ot;
    }

    for (RegFileSet * rfs = g_regfile_set_list.get_head();
         rfs != nullptr; rfs = g_regfile_set_list.get_next()) {
        delete rfs;
    }

    for (RegSet * rs = g_reg_set_list.get_head();
         rs != nullptr; rs = g_reg_set_list.get_next()) {
        delete rs;
    }

    for (SRDescGroup<> * sda = g_sr_desc_group_list.get_head();
         sda != nullptr; sda = g_sr_desc_group_list.get_next()) {
        free(sda);
    }

    for (SRDesc * sd = g_sr_desc_list.get_head();
         sd != nullptr; sd = g_sr_desc_list.get_next()) {
        delete sd;
    }

    for (UINT * buf = g_uint_buffer_list.get_head();
         buf != nullptr; buf = g_uint_buffer_list.get_next()) {
        free(buf);
    }
}


inline static void prtUnit(UINT u)
{
    fprintf(g_output, "%s,", g_unit_name[u]);
}


inline static void prtEquORTypeAddress(EquORTypes const* equort)
{
    if (equort == nullptr) {
        fprintf(g_output, "nullptr,");
        return;
    }

    fprintf(g_output, "&g_equort_%p,", equort);
}


inline static void prtSRDescGroupAddress(SRDescGroup<> const* srdgroup)
{
    if (srdgroup == nullptr) {
        fprintf(g_output, "nullptr,");
        return;
    }

    fprintf(g_output, "(SRDescGroup<>*)&g_sr_desc_group_%p,", srdgroup);
}


static CHAR const* getORTypeName(OR_TYPE ot, OUT xcom::StrBuf & buf)
{
    //Print ORType and name.
    CHAR const* name = OTD_name(&g_or_type_desc[ot]);
    buf.sprint("OR_%s", name);

    //Replace . with _
    for (CHAR * p = buf.buf; *p != 0; p++) {
        if (*p == '.') { *p = '_'; }
    }
    return buf.buf;
}


static void prtEquORTypesList()
{
    xcom::StrBuf buf(64);
    //Print a list of EquORTypes.
    fprintf(g_output, "\n//Initialize EquORTypes.\n");
    for (EquORTypes * e = g_or_types_list.get_head();
         e != nullptr; e = g_or_types_list.get_next()) {
        fprintf(g_output, "static EquORTypes g_equort_%p = {", e);

        //Initialize first class member.
        UINT num = 0;
        for (UINT u = UNIT_UNDEF + 1; u < UNIT_NUM; u++) {
            if (EQUORTY_unit2ortype(e, u) != OR_UNDEF) {
                num++;
            }
        }
        EQUORTY_num_equortype(e) = num;
        fprintf(g_output, "%d,", num);
        //

        //Initialize second class member.
        fprintf(g_output, "{");
        for (INT i = UNIT_UNDEF; i < UNIT_NUM; i++) {
            fprintf(g_output, "%s,",
                getORTypeName(EQUORTY_unit2ortype(e, i), buf));
        }
        fprintf(g_output, "},");
        //

        fprintf(g_output, "};\n");
    }
}


static void prtSRDesc(SRDesc const* srd, CHAR const* byte_buffer_name,
                      CHAR const* byte_buffer_name2)
{
    StrBuf buf(64);
    ASSERT0(srd);
    if (srd->getValidRegFileSet() != nullptr) {
        //Print ROBitSet of valid register file set to current SRDesc.
        buf.sprint("%s_%p", byte_buffer_name, srd);
        prtBitSet(*srd->getValidRegFileSet(), buf.buf, true);

        fprintf(g_output,
        "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
        buf.buf, buf.buf, buf.buf, buf.buf);
    }

    if (srd->getValidRegSet() != nullptr) {
        //Print ROBitSet of valid register set to current SRDesc.
        buf.sprint("%s_%p", byte_buffer_name2, srd);
        prtBitSet(*srd->getValidRegSet(), buf.buf, true);

        fprintf(g_output,
            "static ROBitSet robs_%s(%s, sizeof(%s) / sizeof(%s[0]));\n",
            buf.buf, buf.buf, buf.buf, buf.buf);
    }

    //Print the initialization of SRDesc to current SRDesc.
    fprintf(g_output, "static SRDesc g_sr_desc_%p(", srd);

    //NOTE THE PRINT ORDER OF FOLLOWING FIELD MUST BE CONFORM TO THE
    //RESPECTIVE DECLARATION ORDER.
    fprintf(g_output, "%d,", srd->is_signed()); //1th field
    fprintf(g_output, "%d,", srd->is_imm()); //2nd field
    fprintf(g_output, "%d,", srd->is_label()); //3rd field
    fprintf(g_output, "%d,", srd->is_label_list()); //4th field
    fprintf(g_output, "%u,", srd->getBitSize()); //5th field

     //6th field
    if (srd->getValidRegFileSet() != nullptr) {
        buf.sprint("robs_%s_%p", byte_buffer_name, srd);
        fprintf(g_output, "(RegFileSet*)&%s,", buf.buf);
    } else {
        fprintf(g_output, "nullptr,");
    }

    //7th field
    if (srd->getValidRegSet() != nullptr) {
        buf.sprint("robs_%s_%p", byte_buffer_name2, srd);
        fprintf(g_output, "(RegSet*)&%s", buf.buf);
    } else {
        fprintf(g_output, "nullptr");
    }
    fprintf(g_output, ");\n");
}


static void prtSRDescList()
{
    //Print a list of SRDesc.
    fprintf(g_output, "\n//List of SRDesc.\n");
    xcom::StrBuf buf(64); //name buffer, can not overflow the length.
    CHAR const* byte_buffer_name = "byte_valid_rfset";
    CHAR const* byte_buffer_name2 = "byte_valid_regset";
    for (SRDesc const* srd = g_sr_desc_list.get_head();
         srd != nullptr; srd = g_sr_desc_list.get_next()) {
        prtSRDesc(srd, byte_buffer_name, byte_buffer_name2);
    }
}


static void prtSRDescGroupList()
{
    //Print a list of SRDescGroup.
    fprintf(g_output, "\n//List of SRDescGroup.\n");
    for (SRDescGroup<> const* sdg = g_sr_desc_group_list.get_head();
         sdg != nullptr; sdg = g_sr_desc_group_list.get_next()) {
        fprintf(g_output, "static SRDescGroup<%d> g_sr_desc_group_%p(%d, %d",
                sdg->get_opnd_num() + sdg->get_res_num(),
                sdg, sdg->get_res_num(), sdg->get_opnd_num());

        if (sdg->get_opnd_num() + sdg->get_res_num() == 0) {
            fprintf(g_output, ");\n");
            continue;
        }

        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            SRDesc const* sd = sdg->get_res(i);
            ASSERT0(sd);
            fprintf(g_output, ",&g_sr_desc_%p", sd);
        }

        for (UINT i = 0; i < sdg->get_opnd_num(); i++) {
            SRDesc const* sd = sdg->get_opnd(i);
            ASSERT0(sd);
            fprintf(g_output, ",&g_sr_desc_%p", sd);
        }

        fprintf(g_output, ");\n");
    }
}


static void prtORScheInfoContent(ORScheInfo const& si)
{
    fprintf(g_output, "{");

    //NOTE THE PRINT ORDER OF FOLLOWING FIELD MUST BE CONFORM TO THE
    //RESPECTIVE DECLARATION ORDER.
    //1th field.
    if (ORSI_reg_result_cyc_buf(&si) != nullptr) {
        fprintf(g_output, "%s_%p,", g_reg_result_cyc_buf_name,
                ORSI_reg_result_cyc_buf(&si));
    } else {
        fprintf(g_output, "nullptr,");
    }

    //2nd field.
    if (ORSI_mem_result_cyc_buf(&si) != nullptr) {
        fprintf(g_output, "%s_%p,", g_mem_result_cyc_buf_name,
                ORSI_mem_result_cyc_buf(&si));
    } else {
        fprintf(g_output, "nullptr,");
    }

    //3rd field.
    fprintf(g_output, "%d,", ORSI_last_result_avail_cyc(&si));

    //4th field.
    fprintf(g_output, "%d,", ORSI_first_result_avail_cyc(&si));

    fprintf(g_output, "},");
}


static void prtORTypeDesc(ORTypeDesc const* otd)
{
    StrBuf buf(64);
    fprintf(g_output, "  {");

    //Print ORType and name.
    fprintf(g_output, "%s,", getORTypeName(OTD_code(otd), buf));
    fprintf(g_output, "\"%s\",", OTD_name(otd));

    //Print function unit.
    prtUnit(OTD_unit(otd));

    //Print EquORTypes.
    prtEquORTypeAddress(OTD_equ_or_types(otd));

    //Print SRDescGroup.
    prtSRDescGroupAddress(OTD_srd_group(otd));

    //Print ORScheInfo.
    prtORScheInfoContent(OTD_sche_info(otd));

    fprintf(g_output, "%s,", OTD_is_predicated(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_fake(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_bus(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_nop(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_volatile(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_side_effect(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_asm(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_call(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_cond_br(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_uncond_br(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_indirect_br(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_return(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_unaligned(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_store(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_load(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_eq(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_lt(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_gt(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_movi(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_addi(otd) ? "1":"0");
    fprintf(g_output, "%s,", OTD_is_subi(otd) ? "1":"0");

    fprintf(g_output, "},");

    fprintf(g_output, "\n");

}


static void prtORTypeDescTab()
{
    xcom::StrBuf buf(64);

    //Print ORTypeDesc.
    fprintf(g_output, "\n//Initialize ORTypeDesc.\n");
    fprintf(g_output, "ORTypeDesc const %s [] = {\n", g_or_type_desc_name);
    UINT num = sizeof(g_or_type_desc) / sizeof(g_or_type_desc[0]);
    for (UINT i = 0; i < num; i++) {
        ASSERT0(i < OR_NUM);
        prtORTypeDesc(&g_or_type_desc[i]);
    }
    fprintf(g_output, "};\n");
    fflush(g_output);
}


static void prtHeaderFile()
{
    fprintf(g_output, "#include \"../../opt/targ_const_info.h\"\n");
    fprintf(g_output, "#include \"../../com/xcominc.h\"\n");
    fprintf(g_output, "#include \"../../xgen/xgeninc.h\"\n");
}


static void prtRegFileProp()
{
    UINT n = sizeof(g_regfile_prop) / sizeof(g_regfile_prop[0]);
    ASSERT0(RF_NUM == n);
    fprintf(g_output, "static RegFileProp const g_regfile_prop [] = {\n");
    for (UINT i = 0; i < n; i++) {
        fprintf(g_output, "    {");
        fprintf(g_output, "\"%s\",", g_regfile_prop[i].name);
        fprintf(g_output, "%d,", g_regfile_prop[i].is_integer);
        fprintf(g_output, "%d,", g_regfile_prop[i].is_float);
        fprintf(g_output, "%d,", g_regfile_prop[i].is_predicate);
        fprintf(g_output, "},\n");
    }
    fprintf(g_output, "};\n");
}


static void prtClusterName()
{
    UINT n = sizeof(g_clust_name) / sizeof(g_clust_name[0]);
    fprintf(g_output, "static CHAR const* g_clust_name [] = {");
    for (UINT i = 0; i < n; i++) {
        fprintf(g_output, "\"%s\",", g_clust_name[i]);
    }
    fprintf(g_output, "};\n");
}


static void prtUnitName()
{
    UINT n = sizeof(g_unit_name) / sizeof(g_unit_name[0]);
    fprintf(g_output, "static CHAR const* g_unit_name [] = {");
    for (UINT i = 0; i < n; i++) {
        fprintf(g_output, "\"%s\",", g_unit_name[i]);
    }
    fprintf(g_output, "};\n");
}


static void prtSlotName()
{
    UINT n = sizeof(g_slot_name) / sizeof(g_slot_name[0]);
    fprintf(g_output, "static CHAR const* g_slot_name [] = {");
    for (UINT i = 0; i < n; i++) {
        fprintf(g_output, "\"%s\",", g_slot_name[i]);
    }
    fprintf(g_output, "};\n");
}


//Return true if specified operand or result is cpsr register.
//NOTE SRDescGroup INFORMATION MUST BE AVAILABLE BEFORE INVOKE
//THIS FUNCTION.
static bool isRflagRegister(OR_TYPE ot, UINT idx, bool is_result)
{
    ORTypeDesc const* otd = &g_or_type_desc[ot];    
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    RegFileSet cpsr;
    cpsr.bunion(RF_CPSR);
    if (is_result) {
        SRDesc const* sr_desc = sdg->get_res(idx);
        if (sr_desc->getValidRegFileSet()->is_contain(cpsr)) {
            return true;
        }
        return false;
    }

    SRDesc const* sr_desc = sdg->get_opnd(idx);
    if (sr_desc->getValidRegFileSet()->is_contain(cpsr)) {
        return true;
    }

    return false;
}


//Return the info of result available cycle.
//Use 'get' to compute sche-info as later as possible instead of 'create'.
static void initAndPrtScheInfoImpl(OR_TYPE ot)
{
    ORTypeDesc * otd = &g_or_type_desc[ot];
    UINT mem_result_num = 0;
    if (OTD_is_store(otd)) {
        mem_result_num = 1; //store needs one cycle.
    }

    ORScheInfo * si = &OTD_sche_info(otd);
    si->init();
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    if (sdg == nullptr) {
        //The OR does NOT require scheduling info.
        return;
    }

    UINT reg_result_num = sdg->get_res_num();
    if (reg_result_num > 0) {
        UINT * buf = (UINT*)malloc(sizeof(UINT) * reg_result_num);
        ORSI_reg_result_cyc_buf(si) = buf;
        g_uint_buffer_list.append_head(buf);
    }

    if (mem_result_num > 0) {
        UINT * buf = (UINT*)malloc(sizeof(UINT) * mem_result_num);
        ORSI_mem_result_cyc_buf(si) = buf;
        g_uint_buffer_list.append_head(buf);
    }

    //Get the number of result.
    switch (ot) {
    case OR_b:
    case OR_bl:
    case OR_ret:
    case OR_ret1:
    case OR_ret2:
    case OR_ret3:
    case OR_ret4:
    case OR_blx:
    case OR_bx:
    case OR_mov:
    case OR_mov_i:
    case OR_movw_i:
    case OR_movt_i:
    case OR_mov32_i:
    case OR_mvn:
    case OR_mvn_i:
    case OR_cmp:
    case OR_cmp_i:
    case OR_cmn:
    case OR_cmn_i:
    case OR_tst:
    case OR_tst_i:
    case OR_teq:
    case OR_teq_i:
    case OR_rsb:
    case OR_rsb_i:
    case OR_rsc:
    case OR_rsc_i:
    case OR_and:
    case OR_and_i:
    case OR_orr:
    case OR_orrs:
    case OR_orr_i:
    case OR_orr_lsr_i:
    case OR_ands_asr_i:
    case OR_orr_lsl_i:
    case OR_eor:
    case OR_eor_i:
    case OR_bic:
    case OR_mul:
    case OR_mla:
    case OR_smull:
    case OR_smlal:
    case OR_umull:
    case OR_umlal:
    case OR_mrs:
    case OR_msr:
    case OR_swp:
    case OR_swpb:
    case OR_lsl:
    case OR_lsl_i:
    case OR_lsl_i32:
    case OR_lsr:
    case OR_lsr_i:
    case OR_lsr_i32:
    case OR_asl:
    case OR_asl_i:
    case OR_asr:
    case OR_asr_i:
    case OR_asr_i32:
    case OR_ror:
    case OR_ror_i:
    case OR_rrx:
    case OR_add:
    case OR_add_i:
    case OR_adc:
    case OR_adc_i:
    case OR_sub:
    case OR_sub_i:
    case OR_sbc:
    case OR_sbc_i:
    case OR_neg: {
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            ORSI_reg_result_avail_cyc(si, i) = 1;
        }
        ORSI_last_result_avail_cyc(si) = 1;
        ORSI_first_result_avail_cyc(si) = 1;
        break;
    }
    case OR_adds:
    case OR_adds_i:
    case OR_adcs:
    case OR_adcs_i:
    case OR_subs:
    case OR_subs_i:
    case OR_rsbs:
    case OR_rsbs_i:
    case OR_rscs:
    case OR_rscs_i:
    case OR_sbcs:
    case OR_sbcs_i: {
        //CASE: The result of cpsr is available at cycle of EX3 stage.
        //e.g: add, cpsr, a1 = a2, a3
        //     nop
        //     nop
        //     addc, cpsr, a5 = a6, a7, cpsr
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            if (isRflagRegister(ot, i, true)) {
                ORSI_reg_result_avail_cyc(si, i) = CYCLE_OF_EX3;
            }
            else {
                ORSI_reg_result_avail_cyc(si, i) = 1;
            }
        }
        ORSI_last_result_avail_cyc(si) = CYCLE_OF_EX3;
        ORSI_first_result_avail_cyc(si) = 1;
        break;
    }
    case OR_ldm:
    case OR_ldr:
    case OR_ldrb:
    case OR_ldrsb:
    case OR_ldrh:
    case OR_ldrsh:
    case OR_ldrd:
    case OR_ldr_i12:
    case OR_ldrb_i12:
    case OR_ldrsb_i8:
    case OR_ldrh_i8:
    case OR_ldrsh_i8:
    case OR_ldrd_i8: {
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            //Optimistic load latency.
            ORSI_reg_result_avail_cyc(si, i) = 3;
        }
        ORSI_last_result_avail_cyc(si) = 3;
        ORSI_first_result_avail_cyc(si) = 3;
        break;
    }
    case OR_stm:
    case OR_str:
    case OR_strb:
    case OR_strsb:
    case OR_strh:
    case OR_strsh:
    case OR_strd:
    case OR_str_i12:
    case OR_strb_i12:
    case OR_strh_i8:
    case OR_strd_i8: {
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            //Optimistic write latency.
            ORSI_reg_result_avail_cyc(si, i) = 1;
        }
        ASSERT0(ORSI_mem_result_cyc_buf(si));
        ORSI_mem_result_avail_cyc(si, 0) = 1;
        ORSI_last_result_avail_cyc(si) = 1;
        ORSI_first_result_avail_cyc(si) = 1;
        break;
    }
    case OR_spadjust_i:
    case OR_spadjust_r: {
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            ORSI_reg_result_avail_cyc(si, i) = 2;
        }
        ORSI_last_result_avail_cyc(si) = 2;
        ORSI_first_result_avail_cyc(si) = 2;
        break;
    }
    case OR_asm:
    case OR_push:
    case OR_pop:
    case OR_built_in: {
        for (UINT i = 0; i < sdg->get_res_num(); i++) {
            ORSI_reg_result_avail_cyc(si, i) = 1;
        }
        ORSI_last_result_avail_cyc(si) = 1;
        ORSI_first_result_avail_cyc(si) = 1;
        break;
    }
    case OR_label: break;
    default:
        ASSERTN(0, ("unknown tor code, should add case in"
                    " initAndPrtScheInfoImpl"));
    }

    //Print cycle buffer.
    if (reg_result_num > 0) {
        ASSERT0(si);
        UINT * buf = ORSI_reg_result_cyc_buf(si);
        ASSERT0(buf);
        fprintf(g_output, "static UINT %s_%p [] = {",
                g_reg_result_cyc_buf_name, buf);
        for (UINT i = 0; i < reg_result_num; i++) {
            fprintf(g_output, "%d,", buf[i]);
        }
        fprintf(g_output, "};\n");
    }

    if (mem_result_num > 0) {
        ASSERT0(si);
        UINT * buf = ORSI_mem_result_cyc_buf(si);
        ASSERT0(buf);
        fprintf(g_output, "static UINT %s_%p [] = {",
               g_mem_result_cyc_buf_name, buf);
        for (UINT i = 0; i < mem_result_num; i++) {
            fprintf(g_output, "%d,", buf[i]);
        }
        fprintf(g_output, "};\n");
    }
    fflush(g_output);
}


//Return the info of result available cycle.
//Use 'get' to compute sche-info as later as possible instead of 'create'.
static void initScheInfo()
{
    for (UINT i = OR_UNDEF; i < OR_NUM; i++) {
        initAndPrtScheInfoImpl((OR_TYPE)i);
    }
}


void initAndPrtCluster2Allocable(
        xcom::BitSet const& allocable,
        xcom::BitSet const cluster2regset[])
{
    DUMMYUSE(cluster2regset);
    CHAR const* byte_vec_name = "cluster2regset_allocable";
    xcom::StrBuf buf(64);
    xcom::BitSet s;
    for (UINT i = CLUST_UNDEF + 1; i < CLUST_NUM; i++) {
        switch ((CLUST)i) {
        case CLUST_SCALAR:
            s.copy(allocable);
            break;
        default: UNREACHABLE();
        }

        buf.sprint("%s_%d", byte_vec_name, i);
        prtBitSet(s, buf.buf, true);
        prtROBitSet(buf.buf);
    }

    //Print the export variable.
    prtRegSetArrayDeclaration(g_cluster2regset_allocable_name,
        byte_vec_name, CLUST_UNDEF + 1, CLUST_NUM);
}


#ifdef _DEBUG_
static bool verifyORTypeDesc()
{
    for (UINT i = OR_UNDEF; i < sizeof(g_or_type_desc) /
         sizeof(g_or_type_desc[0]); i++) {
        ASSERT0((UINT)OTD_code(&g_or_type_desc[i]) == i);
    }
    return true;
}
#endif


int main()
{
    CHAR const* name = "arm_targ_mach_resource.cpp";
    UNLINK(name);
    g_output = fopen(name, "a+");

    ASSERT0(verifyORTypeDesc());
    prtHeaderFile();
    prtEmptyROBitSet();
    prtRegFileProp();
    prtSlotName();
    prtClusterName();
    prtUnitName();

    //Initialize readonly target machine info.
    initAndPrtRegisterName();

    xcom::BitSet allocable;
    initAndPrtRegister(allocable);

    xcom::BitSet regfile2regset[RF_NUM];
    initAndPrtRegFile2RegSet(regfile2regset);
    initAndPrtRegFile2Allocable(allocable, regfile2regset);
    initAndPrtReg2RegFile(regfile2regset);

    xcom::BitSet cluster2regset[CLUST_NUM];
    initCluster2RegSet(regfile2regset, cluster2regset);
    initAndPrtCluster2Allocable(allocable, cluster2regset);
    initSRDesc(regfile2regset);
    initORProperty();
    initEquORType();
    initFuncUnit();
    initScheInfo();

    prtEquORTypesList();
    prtSRDescList();
    prtSRDescGroupList();
    prtORTypeDescTab();

    fclose(g_output);
    fini();
    return 0;
}
