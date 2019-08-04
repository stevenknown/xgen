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
#include "../opt/cominc.h"
#include "../opt/comopt.h"
#include "../opt/cfs_opt.h"
#include "../opt/liveness_mgr.h"
#include "../xgen/xgeninc.h"
#include "../cfe/cfexport.h"
#include "../opt/util.h"

void ARMCG::initBuiltin()
{
    CG::initBuiltin();

    m_builtin_uimod = addBuiltinVar("__aeabi_uidivmod");
    m_builtin_imod = addBuiltinVar("__aeabi_idivmod");
    m_builtin_uidiv = addBuiltinVar("__aeabi_uidiv");
    m_builtin_ashldi3 = addBuiltinVar("__ashldi3");
    m_builtin_lshrdi3 = addBuiltinVar("__lshrdi3");
    m_builtin_ashrdi3 = addBuiltinVar("__ashrdi3");
    m_builtin_modsi3 = addBuiltinVar("__modsi3");
    m_builtin_umodsi3 = addBuiltinVar("__umodsi3");
    m_builtin_moddi3 = addBuiltinVar("__moddi3");
    m_builtin_umoddi3 = addBuiltinVar("__umoddi3");
    m_builtin_addsf3 = addBuiltinVar("__addsf3");
    m_builtin_adddf3 = addBuiltinVar("__adddf3");
    m_builtin_subsf3 = addBuiltinVar("__subsf3");
    m_builtin_subdf3 = addBuiltinVar("__subdf3");
    m_builtin_divsi3 = addBuiltinVar("__divsi3");
    m_builtin_udivsi3 = addBuiltinVar("__udivsi3");
    m_builtin_divsf3 = addBuiltinVar("__divsf3");
    m_builtin_divdi3 = addBuiltinVar("__divdi3");
    m_builtin_udivdi3 = addBuiltinVar("__udivdi3");
    m_builtin_divdf3 = addBuiltinVar("__divdf3");
    m_builtin_muldi3 = addBuiltinVar("__muldi3");
    m_builtin_mulsf3 = addBuiltinVar("__mulsf3");
    m_builtin_muldf3 = addBuiltinVar("__muldf3");
    m_builtin_ltsf2 = addBuiltinVar("__ltsf2");
    m_builtin_gtsf2 = addBuiltinVar("__gtsf2");
    m_builtin_gesf2 = addBuiltinVar("__gesf2");
    m_builtin_eqsf2 = addBuiltinVar("__eqsf2");
    m_builtin_nesf2 = addBuiltinVar("__nesf2");
    m_builtin_lesf2 = addBuiltinVar("__lesf2");
    m_builtin_ltdf2 = addBuiltinVar("__ltdf2");
    m_builtin_gtdf2 = addBuiltinVar("__gtdf2");
    m_builtin_gedf2 = addBuiltinVar("__gedf2");
    m_builtin_eqdf2 = addBuiltinVar("__eqdf2");
    m_builtin_nedf2 = addBuiltinVar("__nedf2");
    m_builtin_ledf2 = addBuiltinVar("__ledf2");
    m_builtin_fixsfsi = addBuiltinVar("__fixsfsi");
    m_builtin_fixdfsi = addBuiltinVar("__fixdfsi");
    m_builtin_fixunssfsi = addBuiltinVar("__fixunssfsi");
    m_builtin_fixunsdfsi = addBuiltinVar("__fixunsdfsi");
    m_builtin_fixunssfdi = addBuiltinVar("__fixunssfdi");
    m_builtin_fixunsdfdi = addBuiltinVar("__fixunsdfdi");
    m_builtin_truncdfsf2 = addBuiltinVar("__truncdfsf2");
    m_builtin_floatsisf = addBuiltinVar("__floatsisf");
    m_builtin_floatdisf = addBuiltinVar("__floatdisf");
    m_builtin_floatsidf = addBuiltinVar("__floatsidf");
    m_builtin_floatdidf = addBuiltinVar("__floatdidf");
    m_builtin_fixsfdi = addBuiltinVar("__fixsfdi");
    m_builtin_fixdfdi = addBuiltinVar("__fixdfdi");
    m_builtin_floatunsisf = addBuiltinVar("__floatunsisf");
    m_builtin_floatundisf = addBuiltinVar("__floatundisf");
    m_builtin_floatunsidf = addBuiltinVar("__floatunsidf");
    m_builtin_floatundidf = addBuiltinVar("__floatundidf");
    m_builtin_extendsfdf2 = addBuiltinVar("__extendsfdf2");
}


LIS * ARMCG::allocLIS(
        ORBB * bb,
        DataDepGraph * ddg,
        BBSimulator * sim,
        UINT sch_mode,
        bool change_slot,
        bool change_cluster,
        bool is_log)
{
    ARMLIS * p = new ARMLIS(bb, *ddg, sim, sch_mode,
        change_slot, change_cluster);
    if (is_log) {
        m_lis_list.append_tail(p);
    }
    return (LIS*)p;
}


RaMgr * ARMCG::allocRaMgr(List<ORBB*> * bblist, bool is_func)
{
    return (RaMgr*)new ARMRaMgr(bblist, is_func, this);
}


IR2OR * ARMCG::allocIR2OR()
{
    return new ARMIR2OR(this);
}
//END ARMCG


//Get stack pointer.
SR * ARMCG::genSP()
{
    SR * sr = genDedicatedReg(REG_SP);
    SR_is_sp(sr) = 1;
    return sr;
}


//Get global register pointer.
SR * ARMCG::genGP()
{
    ASSERTN(0, ("TODO"));
    SR * sr = genDedicatedReg(0);
    SR_is_gp(sr) = 1;
    return sr;
}


//Get frame pointer.
SR * ARMCG::genFP()
{
    ASSERTN(0, ("TODO"));
    SR * sr = genDedicatedReg(0);
    SR_is_fp(sr) = 1;
    return sr;
}


SR * ARMCG::genRflag()
{
    SR * sr = genDedicatedReg(REG_RFLAG_REGISTER);
    SR_is_rflag(sr) = 1;
    return sr;
}

SR * ARMCG::gen_one() {
    return genIntImm((HOST_INT)1, false);
}

SR * ARMCG::gen_zero() {
    return genIntImm((HOST_INT)0, false);
}

SR * ARMCG::gen_r0()
{
    SR * sr = genDedicatedReg(REG_R0);
    SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
    return sr;
}


SR * ARMCG::gen_r1()
{
    SR * sr = genDedicatedReg(REG_R1);
    SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
    return sr;
}


SR * ARMCG::gen_r2()
{
    SR * sr = genDedicatedReg(REG_R2);
    SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
    return sr;
}


SR * ARMCG::gen_r3()
{
    SR * sr = genDedicatedReg(REG_R3);
    SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
    return sr;
}


SR * ARMCG::gen_r12()
{
    SR * sr = genDedicatedReg(REG_R12);
    SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
    return sr;
}


SR * ARMCG::genTruePred()
{
    SR * sr = genDedicatedReg(REG_TRUE_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genEQPred()
{
    SR * sr = genDedicatedReg(REG_EQ_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genNEPred()
{
    SR * sr = genDedicatedReg(REG_NE_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genCSPred()
{
    SR * sr = genDedicatedReg(REG_CS_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genHSPred()
{
    SR * sr = genDedicatedReg(REG_HS_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genCCPred()
{
    SR * sr = genDedicatedReg(REG_CC_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genLOPred()
{
    SR * sr = genDedicatedReg(REG_LO_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


//Unsigned GT
SR * ARMCG::genHIPred()
{
    SR * sr = genDedicatedReg(REG_HI_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


//Unsigned LE
SR * ARMCG::genLSPred()
{
    SR * sr = genDedicatedReg(REG_LS_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genMIPred()
{
    SR * sr = genDedicatedReg(REG_MI_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genPLPred()
{
    SR * sr = genDedicatedReg(REG_PL_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genGEPred()
{
    SR * sr = genDedicatedReg(REG_GE_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genLTPred()
{
    SR * sr = genDedicatedReg(REG_LT_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genGTPred()
{
    SR * sr = genDedicatedReg(REG_GT_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::genLEPred()
{
    SR * sr = genDedicatedReg(REG_LE_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


//Generate function return address register.
SR * ARMCG::genReturnAddr()
{
    SR * sr = genDedicatedReg(REG_RETURN_ADDRESS_REGISTER);
    SR_is_ra(sr) = true;
    return sr;
}


void ARMCG::buildLoad(
        IN SR * load_val,
        IN SR * base,
        IN SR * ofst,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(load_val && base && ofst && cont);
    ASSERT0(SR_is_int_imm(ofst));
    VAR const* v = NULL;
    SR * sr_ofst = NULL;
    if (SR_is_var(base)) {
        SR * sr_base;
        v = SR_var(base);
        computeVarBaseOffset(SR_var(base),
            SR_int_imm(ofst), &sr_base, &sr_ofst);
        if (VAR_is_global(v) && !SR_is_reg(sr_base)) {
            //ARM does not support load value from memory label directly.
            SR_var_ofst(base) += (UINT)SR_int_imm(ofst);
            SR * res = genReg();

            if (SR_is_int_imm(sr_ofst)) {
                SR_int_imm(sr_ofst) = SR_var_ofst(base);
            } else {
                ASSERT0(m_is_compute_sect_offset);
                ASSERT0(SR_is_var(sr_ofst));
            }

            //Load address of memory symbol into base register and
            //indirect load value from the base register.
            buildMove(res, base, ors, cont);
            base = res;
        } else {
            ASSERT0(SR_is_reg(sr_base));
            base = sr_base;
        }
    } else {
        sr_ofst = ofst;
    }

    ASSERT0(sr_ofst);
    ASSERT0(cont);
    if (IOC_mem_byte_size(cont) <= 4) {
        OR * o = NULL;
        switch (IOC_mem_byte_size(cont)) {
        case 1:
            o = genOR(OR_ldrb);
            o->set_load_val(load_val);
            break;
        case 2:
            o = genOR(OR_ldrh);
            o->set_load_val(load_val);
            break;
        case 4:
            o = genOR(OR_ldr);
            o->set_load_val(load_val);
            break;
        default: UNREACHABLE();
        }
        o->set_load_base(base);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        o->set_load_ofst(sr_ofst);

        //Mapping from LD OR to correspond variable. Used by OR::dump()
        setMapOR2Mem(o, v);
        ors.append_tail(o);
    } else if (IOC_mem_byte_size(cont) <= 8) {
        OR * o = genOR(OR_ldrd);
        ASSERT0(load_val->getByteSize() == 8);
        ASSERT0(SR_is_vec(load_val));
        ASSERT0(SR_vec(load_val)->get(0) && SR_vec(load_val)->get(1));
        o->set_load_val(SR_vec(load_val)->get(0), 0);
        o->set_load_val(SR_vec(load_val)->get(1), 1);
        o->set_load_base(base);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        o->set_load_ofst(sr_ofst);
        setMapOR2Mem(o, v);
        ors.append_tail(o);
    } else {
        UNREACHABLE();
    }
}


//[base + ofst] = store_val
void ARMCG::buildStore(
        IN SR * store_val,
        IN SR * base,
        IN SR * ofst,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(store_val && base && ofst);
    ASSERT0(SR_is_int_imm(ofst) && (SR_is_reg(base) || SR_is_var(base)));
    ASSERTN(SR_is_reg(store_val), ("store_val can only be register on ARM"));
    ASSERT0(SR_is_int_imm(ofst));
    VAR const* v = NULL;
    SR * sr_ofst = NULL;
    if (SR_is_var(base)) {
        SR * sr_base;
        v = SR_var(base);
        computeVarBaseOffset(SR_var(base), SR_int_imm(ofst),
            &sr_base, &sr_ofst);
        if (VAR_is_global(v) && !SR_is_reg(sr_base)) {
            //ARM does not support load value from memory label directly.
            SR_var_ofst(base) += (UINT)SR_int_imm(ofst);
            SR * res = genReg();
            buildMove(res, base, ors, cont);

            if (SR_is_int_imm(sr_ofst)) {
                SR_int_imm(sr_ofst) = SR_var_ofst(base);
            } else {
                ASSERT0(m_is_compute_sect_offset);
                ASSERT0(SR_is_var(sr_ofst));
            }

            base = res;
        } else {
            ASSERT0(SR_is_reg(sr_base));
            base = sr_base;
        }
    } else {
        sr_ofst = ofst;
    }

    ASSERT0(sr_ofst);
    OR * o = NULL;
    ASSERT0(cont != NULL);
    if (IOC_mem_byte_size(cont) <= 4) {
        OR_TYPE code = OR_UNDEF;
        switch (IOC_mem_byte_size(cont)) {
        case 1:
            if (SR_is_int_imm(sr_ofst)) {
                if (isValidImmOpnd(OR_strb_i12, SR_int_imm(sr_ofst))) {
                    code = OR_strb_i12;
                } else {
                    //base + ofst
                    //=>
                    //t = base + ofst
                    //base = t
                    IOC tc;
                    buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                        false, ors, &tc);
                    base = tc.get_reg(0);
                    ASSERT0(base && SR_is_reg(base));
                    code = OR_strb;
                    sr_ofst = genIntImm(0, false);
                }
            } else if (SR_is_var(sr_ofst)) {
                code = OR_strb;
            } else {
                UNREACHABLE();
            }
            break;
        case 2:
            if (SR_is_int_imm(sr_ofst)) {
                if (isValidImmOpnd(OR_strh_i8, SR_int_imm(sr_ofst))) {
                    code = OR_strh_i8;
                } else {
                    IOC tc;
                    buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                        false, ors, &tc);
                    base = tc.get_reg(0);
                    ASSERT0(base && SR_is_reg(base));
                    code = OR_str;
                    sr_ofst = genIntImm(0, false);
                }
            } else if (SR_is_var(sr_ofst)) {
                code = OR_strh;
            } else {
                UNREACHABLE();
            }
            break;
        case 4:
            if (SR_is_int_imm(sr_ofst)) {
                if (isValidImmOpnd(OR_str_i12, SR_int_imm(sr_ofst))) {
                    code = OR_str_i12;
                } else {
                    IOC tc;
                    buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                        false, ors, &tc);
                    base = tc.get_reg(0);
                    ASSERT0(base && SR_is_reg(base));
                    code = OR_str;
                    sr_ofst = genIntImm(0, false);
                }
            } else if (SR_is_var(sr_ofst)) {
                code = OR_str;
            } else {
                UNREACHABLE();
            }
            break;
        default: UNREACHABLE();
        }
        o = genOR(code);
        o->set_store_val(store_val);
        o->set_store_base(base);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        o->set_store_ofst(sr_ofst);

        //Mapping from LD OR to corresponnd variable. Used by OR::dump()
        setMapOR2Mem(o, v);
        ors.append_tail(o);
    } else if (IOC_mem_byte_size(cont) <= 8) {
        OR_TYPE code = OR_UNDEF;
        if (SR_is_int_imm(sr_ofst)) {
            if (isValidImmOpnd(OR_strd_i8, SR_int_imm(sr_ofst))) {
                code = OR_strd_i8;
            } else {
                IOC tc;
                buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                    false, ors, &tc);
                base = tc.get_reg(0);
                ASSERT0(base && SR_is_reg(base));
                code = OR_str;
                sr_ofst = genIntImm(0, false);
            }
        } else if (SR_is_var(sr_ofst)) {
            code = OR_strd;
        } else {
            UNREACHABLE();
        }
        o = genOR(code);
        ASSERT0(store_val->getByteSize() == 8);
        ASSERT0(SR_is_vec(store_val));
        ASSERT0(SR_vec(store_val)->get(0) &&
                SR_vec(store_val)->get(1));
        o->set_store_val(SR_vec(store_val)->get(0), 0);
        o->set_store_val(SR_vec(store_val)->get(1), 1);
        o->set_store_base(base);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        o->set_store_ofst(sr_ofst);

        setMapOR2Mem(o, v);
        ors.append_tail(o);
    } else {
        UNREACHABLE();
    }
}


//Build copy-operation with predicate register.
void ARMCG::buildCopyPred(
        CLUST clust,
        UNIT unit,
        SR * to,
        SR * from,
        SR * pd,
        ORList & ors)
{
    ASSERT0(to && SR_is_reg(to) && from && SR_is_reg(from));
    buildCopy(clust, unit, to, from, ors);
    if (pd != NULL) {
        OR * o = ors.get_head();
        ASSERT0(ors.get_elem_count() == 1 && isCopyOR(o) && SR_is_pred(pd));
        o->set_pred(pd);
    }
}


//Implement memory block copy.
//Note tgt and src should not overlap.
void ARMCG::buildMemcpyInternal(
            SR * tgt,
            SR * src,
            UINT bytesize,
            OUT ORList & ors,
            IN IOC * cont)
{
    ASSERT0(src && SR_is_reg(src));
    ASSERT0(tgt && SR_is_reg(tgt));
    ASSERTN(bytesize > GENERAL_REGISTER_SIZE,
        ("use SR move instead of block copy"));
    //Generate loop to copy from src to tgt.
    //e.g:
    // srt2 = bytesize
    // LOOP_START:
    // x = [SRsrc]
    // [SRtgt] = x
    // SRsrc = SRsrc + 4byte
    // SRtgt = SRtgt + 4byte
    // srt2 = srt2 - 4byte
    // teq_i, cpsr = srt2, 0
    // b.ne, LOOP_START
    IOC tc;
    SR * zero = genIntImm((HOST_INT)0, true);
    SR * sz = genIntImm((HOST_INT)GENERAL_REGISTER_SIZE, true);
    if (bytesize % GENERAL_REGISTER_SIZE != 0) {
        UINT rest = bytesize % GENERAL_REGISTER_SIZE;
        buildGeneralLoad(
            genIntImm((HOST_INT)rest, true),
            0, ors, cont);
        SR * srt2 = cont->get_reg(0);
        ASSERT0(srt2 && SR_is_reg(srt2));

        IOC_mem_byte_size(&tc) = rest;
        buildGeneralLoad(src, 0, ors, &tc);

        SR * srt1 = tc.get_reg(0);
        ASSERT0(srt1 && SR_is_reg(srt1));

        IOC_mem_byte_size(&tc) = rest;
        buildStore(srt1, tgt, zero, ors, &tc);

        SR * sz2 = genIntImm((HOST_INT)rest, true);
        tc.clean_bottomup();
        buildAdd(src, sz2, GENERAL_REGISTER_SIZE, false, ors, &tc);
        ASSERT0(OR_code(ors.get_tail()) == OR_add ||
                OR_code(ors.get_tail()) == OR_add_i);
        ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
        ors.get_tail()->set_result(0, src);
        //buildMove(src, tc.get_reg(0), ors, &tc);

        tc.clean_bottomup();
        buildAdd(tgt, sz2, GENERAL_REGISTER_SIZE, false, ors, &tc);
        ASSERT0(OR_code(ors.get_tail()) == OR_add ||
                OR_code(ors.get_tail()) == OR_add_i);
        ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
        ors.get_tail()->set_result(0, tgt);
        //buildMove(tgt, tc.get_reg(0), ors, &tc);

        tc.clean_bottomup();
        buildSub(srt2, sz2, GENERAL_REGISTER_SIZE, false, ors, &tc);
        ASSERT0(OR_code(ors.get_tail()) == OR_add ||
                OR_code(ors.get_tail()) == OR_add_i);
        ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
        ors.get_tail()->set_result(0, srt2);
        //buildMove(srt2, tc.get_reg(0), ors, &tc);

        bytesize -= rest;
    }

    tc.clean_bottomup();
    buildGeneralLoad(genIntImm((HOST_INT)bytesize, true), 0, ors, &tc);
    SR * srt2 = tc.get_reg(0);
    ASSERT0(srt2 && SR_is_reg(srt2));

    SR * loop_start_lab = genLabel(m_ru->buildIlabel()->getLabel());
    buildLabel(SR_label(loop_start_lab), ors, &tc);

    IOC_mem_byte_size(&tc) = src->getByteSize();
    tc.clean_bottomup();
    buildGeneralLoad(src, 0, ors, &tc);

    SR * srt1 = tc.get_reg(0);
    ASSERT0(srt1 && SR_is_reg(srt1));

    IOC_mem_byte_size(&tc) = src->getByteSize();
    buildStore(srt1, tgt, zero, ors, &tc);

    ASSERT0(GENERAL_REGISTER_SIZE == src->getByteSize());
    ASSERT0(GENERAL_REGISTER_SIZE == tgt->getByteSize());

    //src = src + size.
    tc.clean_bottomup();
    buildAdd(src, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
    ors.get_tail()->set_result(0, src);
    //buildMove(src, tc.get_reg(0), ors, &tc);

    //tgt = tgt + size.
    tc.clean_bottomup();
    buildAdd(tgt, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
    ors.get_tail()->set_result(0, tgt);
    //buildMove(tgt, tc.get_reg(0), ors, &tc);

    //Update loop_counter
    //total_size = total_size - GENERAL_REGISTER_SIZE.
    tc.clean_bottomup();
    SR * sz3 = genIntImm((HOST_INT)GENERAL_REGISTER_SIZE, true);
    buildSub(srt2, sz3, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(SR_is_reg(ors.get_tail()->get_result(0)));
    ors.get_tail()->set_result(0, srt2);
    //buildMove(srt2, tc.get_reg(0), ors, &tc);

    tc.clean_bottomup();
    ORList tors;
    buildCompare(OR_teq_i, true, srt2, zero, tors, &tc);
    ASSERT0(tors.get_elem_count() == 1);
    ors.append_tail(tors);

    cont->set_pred(genNEPred());
    buildCondBr(loop_start_lab, ors, cont);
}


void ARMCG::buildCopy(
        CLUST clust,
        UNIT unit,
        SR * to,
        SR * from,
        ORList & ors)
{
    ASSERT0(SR_is_reg(to) && SR_is_reg(from));
    OR_TYPE ot = computeEquivalentORType(OR_mov, unit, clust);
    OR * o = genOR(ot);
    o->set_copy_to(to);
    o->set_copy_from(from);
    OR_clust(o) = clust;
    ors.append_tail(o);
}


void ARMCG::buildMove(SR * to, SR * from, OUT ORList & ors, IN IOC *)
{
    ASSERT0(SR_is_reg(to));
    ASSERT0(SR_is_reg(from) ||
            SR_is_var(from) ||
            SR_is_int_imm(from) ||
            SR_is_fp_imm(from));
    switch (SR_type(from)) {
    case SR_REG:
        buildCopy(CLUST_FIRST, UNIT_A, to, from, ors);
        return;
    case SR_INT_IMM: {
        OR * o = genOR(OR_mov32_i);
        o->set_mov_to(to);
        o->set_mov_from(genIntImm(SR_int_imm(from), true));
        ors.append_tail(o);

        //Decompose OR_mov32_i into OR_movw_i and OR_movt_i after RA,
        //otherwise it might confuse ORBB localization pass.
        //OR * o = genOR(OR_movw_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genIntImm(
        //    (HOST_INT)(SR_int_imm(from) & 0xFFFF), true));
        //ors.append_tail(o);
        //if (isInteger32bit(SR_int_imm(from))) {
        //    o = genOR(OR_movt_i);
        //    o->set_mov_to(to);
        //    o->set_mov_from(genIntImm(
        //        (HOST_INT)((SR_int_imm(from) >> 16) & 0xFFFF), true));
        //    ors.append_tail(o);
        //}
        return;
    }
    case SR_FP_IMM:
        ASSERTN(0, ("TODO"));
        break;
    case SR_VAR: {
        //mov32_i might reduce the number of candidate instructions in
        //instruction scheduling.
        //Decompose OR_mov32_i into OR_movw_i and OR_movt_i after RA,
        //otherwise it might confuse ORBB localization pass.

        OR * o = genOR(OR_mov32_i);
        o->set_mov_to(to);
        o->set_mov_from(genVAR(SR_var(from)));
        ors.append_tail(o);

        //OR * o = genOR(OR_movw_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genVAR(SR_var(from)));
        //ors.append_tail(o);
        //o = genOR(OR_movt_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genVAR(SR_var(from)));
        //ors.append_tail(o);
        return;
    }
    default: UNREACHABLE();
    }
}


void ARMCG::setSpadjustOffset(OR * spadj, INT size)
{
    DUMMYUSE(spadj);
    DUMMYUSE(size);
    ASSERT0(spadj && OR_code(spadj) == OR_spadjust);
    spadj->set_opnd(HAS_PREDICATE_REGISTER + SPADJUST_OFFSET_INDX,
        genIntImm((HOST_INT)size, true));
}


//Generate sp adjust operation.
void ARMCG::buildSpadjust(OUT ORList & ors, IN IOC * cont)
{
    OR * o = genOR(OR_spadjust);
    ASSERT0(OR_is_fake(o));
    o->set_result(0, genSP());
    o->set_result(1, gen_r12());
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, genSP());
    ASSERT0(cont);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1,
        genIntImm((HOST_INT)IOC_int_imm(cont), true));
    ors.append_tail(o);
}


void ARMCG::buildICall(
        SR * callee,
        UINT ret_val_size,
        OUT ORList & ors,
        IOC * cont)
{
    ASSERT0(callee && SR_is_reg(callee));
    //Function-Call will violate SP,FP,GP, RFLAG register,
    //return-value register, return address register.
    OR * o = buildOR(OR_blx, 1, 2,
                     genReturnAddr(),
                     genTruePred(),
                     callee);
    ors.append_tail(o);

    ASSERT0(cont != NULL);
    if (ret_val_size <= 4) {
        cont->set_reg(0, gen_r0());
    } else if (ret_val_size <= 8) {
        SR * sr = getSRVecMgr()->genSRVec(2, gen_r0(), gen_r1());
        cont->set_reg(0, sr);
    } else {
        ASSERTN(0, ("TODO"));
    }
}


void ARMCG::buildCall(
        VAR const* callee,
        UINT ret_val_size,
        OUT ORList & ors,
        IOC * cont)
{
    //Function-Call will violate SP,FP,GP,
    //RFLAG register, return-value register,
    //return address register.
    OR * o = buildOR(OR_bl, 1, 2,
                     genReturnAddr(),
                     genTruePred(),
                     genVAR(callee));
    ors.append_tail(o);

    ASSERT0(cont != NULL);
    if (ret_val_size <= 4) {
        cont->set_reg(0, gen_r0());
    } else if (ret_val_size <= 8) {
        SR * sr = getSRVecMgr()->genSRVec(2, gen_r0(), gen_r1());
        cont->set_reg(0, sr);
    } else {
        //Return value will be stored in 'retval_buf_of_XXX'.
        cont->set_reg(0, NULL);
    }
}

CLUST ARMCG::mapSlot2Cluster(SLOT slot)
{
    switch (slot) {
    case SLOT_G: return CLUST_FIRST;
    default: UNREACHABLE();
    }
    return CLUST_UNDEF;
}


//Return the regisiter-file set which 'clust' corresponded with.
List<REGFILE> & ARMCG::mapCluster2RegFileList(
        CLUST clust,
        OUT List<REGFILE> & regfiles)
{
    switch (clust) {
    case CLUST_FIRST:
        regfiles.append_tail(RF_R);
        break;
    default: UNREACHABLE();
    }
    return regfiles;
}


//Return register-file set which the unit set corresponded with.
//'units': function unit set
List<REGFILE> & ARMCG::mapUnitSet2RegFileList(
        IN xgen::UnitSet & us,
        OUT List<REGFILE> & regfiles)
{
    regfiles.append_tail(RF_R);
    return regfiles;
}


//Mapping from single unit to its corresponed cluster.
SLOT ARMCG::mapUnit2Slot(UNIT unit, CLUST clst)
{
    switch (unit) {
    case UNIT_A: return SLOT_G;
    default: UNREACHABLE();
    }
    return SLOT_NUM;
}


//Mapping from single issue slot(for multi-issue architecture) to
//its corresponed function unit.
UNIT ARMCG::mapSlot2Unit(SLOT slot)
{
    switch (slot) {
    case SLOT_G: return UNIT_A;
    default: UNREACHABLE();
    }
    return UNIT_UNDEF;
}


//Return the cluster which owns 'regfile'.
CLUST ARMCG::mapRegFile2Cluster(REGFILE regfile, SR const*)
{
    switch (regfile) {
    case RF_UNDEF:
        return CLUST_UNDEF;
    case RF_R:
    case RF_D:
    case RF_Q:
    case RF_S:
        return CLUST_FIRST;
    default: UNREACHABLE();
    }
    return CLUST_UNDEF;
}


//Return the cluster which owns 'reg'
CLUST ARMCG::mapReg2Cluster(REG reg)
{
    for (INT c = CLUST_UNDEF + 1; c < CLUST_NUM; c++) {
        if (tmMapCluster2RegSet((CLUST)c)->is_contain(reg)) {
            return (CLUST)c;
        }
    }
    UNREACHABLE();
    return CLUST_FIRST;
}


//Return the function unit which can operate on 'regfile'
UnitSet & ARMCG::mapRegFile2UnitSet(REGFILE regfile, SR const* sr, UnitSet & us)
{
    switch (regfile) {
    case RF_R:
    case RF_P:
        us.bunion(UNIT_A);
        return us;
    default: UNREACHABLE();
    }
    return us;
}


//Return the cluster that is the correct function unit of 'sr'.
CLUST ARMCG::mapSR2Cluster(OR * o, SR const* sr)
{
    if (!SR_is_reg(sr) || SR_regfile(sr) == RF_UNDEF) {
        return CLUST_UNDEF;
    }
    if (SR_phy_regid(sr) != REG_UNDEF) {
        return mapReg2Cluster(SR_phy_regid(sr));
    }
    if (SR_regfile(sr) != RF_UNDEF) {
        return mapRegFile2Cluster(SR_regfile(sr), sr);
    }

    CLUST clust = CLUST_UNDEF;
    switch (OR_code(o)) {
    case OR_spadjust:
        //Disable ASSERT0(sr == genSP() || sr == genGP() || sr == genFP());
        //Because alloca generate spadjust operation, such as:
        //    foo (alloca ((int)&main));
        //and generated OR are:
        //    t233 :- mov_i t97(p0) (sym:main+0)
        //    t234 :- add_i t97(p0) t233 (0x3)
        //    t235[A1] :- and_i t97(p0) TN234 (0xfffffffc)
        //    t49(sp) :- spadjust t97(p0) t235[A1] ; spadjust_minus
        //Here t235 is not a genSP().
        clust = CLUST_FIRST;
        break;
    default:
        clust = OR_clust(o);
        if (clust == CLUST_UNDEF) {
            //'sr' have no REGFILE, no REG, and not any information can
            //be used to determine which cluster it belong to.
            //ARM does not have any SR used at BUS beside rflag,
            //predicate register.
            //So 'sr' should belong to first cluster.
            clust = CLUST_FIRST;
        }
    }
    return clust;
}


//Generate nop operation
OR * ARMCG::buildNop(UNIT unit, CLUST clust)
{
    OR * nop = NULL;
    switch (unit) {
    case UNIT_A:
        nop = buildOR(OR_nop, 0, 1, genTruePred());
        break;
    default: UNREACHABLE();
    }
    OR_clust(nop) = clust;
    return nop;
}


//Compute the default-cluster 'o' should be.
CLUST ARMCG::computeORCluster(OR const*) const
{
    return CLUST_FIRST;
}


//Change 'or' to 'ortype', modifing all operands and results.
//Performing verification and substitution certainly.
bool ARMCG::changeORType(
        IN OUT OR * o,
        OR_TYPE ortype,
        CLUST src,
        CLUST tgt,
        Vector<bool> const& regfile_unique)
{
    DUMMYUSE(src);
    ASSERTN(tgt != CLUST_UNDEF, ("need cluster info"));
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (!SR_is_reg(sr) ||
            SR_is_pred(sr) ||
            SR_is_dedicated(sr)) {
            continue;
        }

        //Handle general sr.
        if (SR_phy_regid(sr) != REG_UNDEF) {
            ASSERTN(SR_regfile(sr) != RF_UNDEF, ("Loss regfile info"));
            ASSERTN(tgt == mapReg2Cluster(SR_phy_regid(sr)), ("Unmatch info"));
        } else {
            //ASSERTN(SR_phy_regid(sr) == REG_UNDEF &&
            //       SR_regfile(sr) == RF_UNDEF, ("sr has allocated"));
            if (!regfile_unique.get(SR_sregid(sr))) {
                SR_phy_regid(sr) = REG_UNDEF;
                SR_regfile(sr) = RF_UNDEF;
            }
        }
    }

    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (!SR_is_reg(sr) ||
            SR_is_pred(sr) ||
            SR_is_dedicated(sr)) {
            continue;
        }

        if (SR_phy_regid(sr) != REG_UNDEF) {
            ASSERTN(SR_regfile(sr) != RF_UNDEF, ("Loss regfile info"));
            //When 'sr' has been assigned register, the 'tgt' cluster
            //must be as same as the cluster which 'sr' correlated to.
            //ASSERTN(tgt == mapReg2Cluster(
            //    TN_register_class(sr), SR_phy_regid(sr)), ("Unmatch info"));

            if (tgt != mapReg2Cluster(SR_phy_regid(sr))) {
                return false;
            }
        } else {
            if (!regfile_unique.get(SR_sregid(sr))) {
                //ASSERTN(SR_phy_regid(sr) == REG_UNDEF &&
                //    SR_regfile(sr) == RF_UNDEF, ("sr has allocated"));

                SR_phy_regid(sr) = REG_UNDEF;
                SR_regfile(sr) = RF_UNDEF;
            }
        }
    }
    OR_code(o) = ortype;
    return true;
}


void ARMCG::buildShiftLeft(
        IN SR * src,
        ULONG sr_size,
        IN SR * shift_ofst,
        OUT ORList & ors,
        IN OUT IOC * cont)
{
    if (sr_size <= 4) {
        //There is no different between signed and unsigned left shift.
        SR * res = genReg();
        OR_TYPE ort = OR_UNDEF;
        if (SR_is_reg(shift_ofst)) {
            ort = OR_lsl;
        } else if (SR_is_imm(shift_ofst)) {
            ort = OR_lsl_i32;
        } else {
            UNREACHABLE();
        }
        OR * o = buildOR(ort, 1, 3, res, genTruePred(), src, shift_ofst);
        ors.append_tail(o);
        ASSERT0(cont);
        cont->set_reg(0, res);
        return;
    }

    ASSERTN(sr_size <= 8, ("TODO"));
    if (SR_is_reg(shift_ofst)) {
        //e.g:long long a,b; int j;
        //    a = b << j;
        //  generated code:
        //    rsb r6, j, #32
        //    sub r1, j, #32
        //    lsl r5, a_hi, j             
        //    lsr r6, a_lo, r6             
        //    lsl r4, a_lo, r1             
        //    orrs r5, r5, r6         
        //    lsl res_lo, r0, j             
        //    ands res_hi, r5, r1, asr #32
        //    it  cc
        //    movcc res_hi, r4
        //    strd res_lo, res_hi ->[b_lo, b_hi]
        SR * hi = SR_vec(src)->get(1);
        SR * lo = SR_vec(src)->get(0);
        ASSERT0(hi && lo);
        SR * res_lo = genReg();
        SR * res_hi = genReg();
        getSRVecMgr()->genSRVec(2, res_lo, res_hi);

        //    rsb r6, j, #32
        SR * r6 = genReg();
        OR * o = buildOR(OR_rsb_i, 1, 3, r6,
            genTruePred(), shift_ofst, genIntImm(32, false));
        ors.append_tail(o);

        //    sub r1, j, #32
        SR * r1 = genReg();
        o = buildOR(OR_sub_i, 1, 3, r1,
            genTruePred(), shift_ofst, genIntImm(32, false));
        ors.append_tail(o);

        //    lsl r5, a_hi, j
        SR * r5 = genReg();
        o = buildOR(OR_lsl, 1, 3, r5,
            genTruePred(), hi, shift_ofst);
        ors.append_tail(o);

        //    lsr r6, a_lo, r6
        o = buildOR(OR_lsr, 1, 3, r6,
            genTruePred(), lo, r6);
        ors.append_tail(o);

        //    lsl r4, a_lo, r1
        SR * r4 = genReg();
        o = buildOR(OR_lsl, 1, 3, r4,
            genTruePred(), lo, r1);
        ors.append_tail(o);

        //    orrs r5, r5, r6
        o = buildOR(OR_orrs, 2, 3, r5, genRflag(),
            genTruePred(), r5, r6);
        ors.append_tail(o);

        //    lsl res_lo, a_lo, j
        o = buildOR(OR_lsl, 1, 3, res_lo,
            genTruePred(), lo, shift_ofst);
        ors.append_tail(o);

        //    ands res_hi, r5, r1, asr #32
        o = buildOR(OR_ands_asr_i, 2, 4, res_hi, genRflag(),
            genTruePred(), r5, r1, genIntImm(32, false));
        ors.append_tail(o);

        //    movcc res_hi, r4
        o = buildOR(OR_mov, 1, 2, res_hi, genCCPred(), r4);
        ors.append_tail(o);

        //    strd res_lo, res_hi ->[b_lo, b_hi]
        ASSERT0(cont);
        cont->set_reg(0, res_lo);
        return;
    } else if (SR_is_imm(shift_ofst)) {
        if (SR_int_imm(shift_ofst) <= 31) {
            //hi <- hi << ofst
            //t <- lo << (32 - ofst)
            //hi <- hi | t
            //lo <- lo << ofst
            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);
            OR * o = buildOR(OR_lsl_i, 1, 3, hi, genTruePred(),
                hi, shift_ofst);
            ors.append_tail(o);
            o = buildOR(OR_orr_lsr_i, 1, 4, hi, genTruePred(),
                hi, lo, genIntImm(32 - SR_int_imm(shift_ofst), false));
            ors.append_tail(o);
            o = buildOR(OR_lsl_i, 1, 3, lo, genTruePred(),
                lo, shift_ofst);
            ors.append_tail(o);
            ASSERT0(cont);
            cont->set_reg(0, lo);
            //cont->set_reg(0, hi);
            return;
        } else if (SR_int_imm(shift_ofst) <= 63) {
            //hi <- lo << (ofst - 32)
            //lo <- 0
            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);
            OR * o = buildOR(OR_lsl_i, 1, 3, hi, genTruePred(),
                lo, genIntImm(SR_int_imm(shift_ofst) - 32, false));
            ors.append_tail(o);
            o = buildOR(OR_mov_i, 1, 2, lo, genTruePred(),
                genIntImm((HOST_INT)0, true));
            ors.append_tail(o);
            ASSERT0(cont);
            cont->set_reg(0, lo);
            return;
        } else {
            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);
            // hi <- 0
            OR * set_high = buildOR(OR_mov_i, 1, 2, hi,
                genTruePred(), genIntImm(0, true));
            // lo <- 0
            OR * set_low = buildOR(OR_mov_i, 1, 2, lo,
                genTruePred(), genIntImm(0, true));                
            ASSERT0(set_high && set_low);
            ors.append_tail(set_high);
            ors.append_tail(set_low);
            ASSERT0(cont);
            cont->set_reg(0, hi);
            //cont->set_reg(1, hi);
            return;
        }
    }
    UNREACHABLE();
}


void ARMCG::buildShiftRight(
        IN SR * src,
        ULONG sr_size,
        IN SR * shift_ofst,
        bool is_signed,
        OUT ORList & ors,
        IN OUT IOC * cont)
{
    if (sr_size <= 4) {
        SR * res = genReg();
        OR_TYPE ort = OR_UNDEF;
        if (is_signed) {
            if (SR_is_reg(shift_ofst)) { ort = OR_asr; }
            else if (SR_is_imm(shift_ofst)) { ort = OR_asr_i32; }
            else { UNREACHABLE(); }
        } else {
            if (SR_is_reg(shift_ofst)) { ort = OR_lsr; }
            else if (SR_is_imm(shift_ofst)) { ort = OR_lsr_i32; }
            else { UNREACHABLE(); }
        }
        OR * o = buildOR(ort, 1, 3, res, genTruePred(), src, shift_ofst);
        ors.append_tail(o);
        ASSERT0(cont);
        cont->set_reg(0, res);
        return;
    }

    ASSERTN(sr_size <= 8, ("TODO"));
    if (SR_is_reg(shift_ofst)) {
        //e.g:long long res;
        //    long long src;
        //    int shift_ofst;
        //    void foo() {
        //        res = src >> shift_ofst;
        //    }
        //
        //    sub r5 <- shift_ofst, #32
        //    asr r6 <- src_hi, r5
        //    rsb ip <- shift_ofst, #32
        //    lsl ip <- src_hi, ip
        //    lsr r2 <- src_lo, shift_ofst
        //    orr res_lo <- ip, r2
        //    cmp r5, #0
        //    it ge      #must exist if ISA is thumb.
        //    movge res_lo <- r6
        //    asr res_hi <- src_hi, r4
        //    strd {res_lo,res_hi} -> result

        OR_TYPE ort = OR_UNDEF;
        if (is_signed) {
            ort = OR_asr;
        } else {
            ort = OR_lsr;
        }

        SR * src_hi = SR_vec(src)->get(1);
        SR * src_lo = SR_vec(src)->get(0);
        ASSERT0(src_hi && src_lo);
        SR * res_lo = genReg();
        SR * res_hi = genReg();
        getSRVecMgr()->genSRVec(2, res_lo, res_hi);

        //    sub r5, shift_ofst, #32
        SR * r5 = genReg();
        OR * o = buildOR(OR_sub_i, 1, 3, r5,
            genTruePred(), shift_ofst, genIntImm(32, false));
        ors.append_tail(o);

        //    asr/lsr r6, src_hi, r5
        SR * r6 = genReg();
        o = buildOR(ort, 1, 3, r6, genTruePred(), src_hi, r5);
        ors.append_tail(o);

        //    rsb ip, shift_ofst, #32
        SR * ip = genReg();
        o = buildOR(OR_rsb_i, 1, 3, ip,
            genTruePred(), shift_ofst, genIntImm(32, false));
        ors.append_tail(o);

        //    lsl ip, src_hi, ip
        o = buildOR(OR_lsl, 1, 3, ip, genTruePred(), src_hi, ip);
        ors.append_tail(o);

        //    lsr r2, src_lo, shift_ofst
        SR * r2 = genReg();
        o = buildOR(OR_lsr, 1, 3, r2, genTruePred(), src_lo, shift_ofst);
        ors.append_tail(o);

        //    orr res_lo, ip, r2
        o = buildOR(OR_orr, 1, 3, res_lo, genTruePred(), ip, r2);
        ors.append_tail(o);

        //    cmp r5, #0
        o = buildOR(OR_cmp_i, 1, 3, genRflag(),
            genTruePred(), r5, genIntImm(0, false));
        ors.append_tail(o);

        //    mov_ge  res_lo, r6
        o = buildOR(OR_mov, 1, 2, res_lo, genGEPred(), r6);
        ors.append_tail(o);

        //    asr/lsr res_hi, src_hi, shift_ofst
        o = buildOR(ort, 1, 3, res_hi, genTruePred(), src_hi, shift_ofst);
        ors.append_tail(o);

        //    strd res_lo, res_hi ->[b_lo, b_hi]
        ASSERT0(cont);
        cont->set_reg(0, res_lo);
        return;
    } else if (SR_is_imm(shift_ofst)) {
        if (SR_int_imm(shift_ofst) <= 31) {
            //lo = lo >>(lsr) shift_ofst
            //lo = lo | (hi <<(lsl) (32 - shift_ofst))
            //hi = hi >> shift_ofst

            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);

            //NOTE: Need LOGICAL shift to reserve space to
            //hold residual part of high-part.
            //DO NOT USE ARITH-SHIFT.
            //lo = lo >>(lsr) shift_ofst
            OR * o = buildOR(OR_lsr_i, 1, 3, lo, genTruePred(), lo, shift_ofst);
            ors.append_tail(o);

            //lo = lo | (hi <<(lsl) (32 - shift_ofst))
            o = buildOR(OR_orr_lsl_i, 1, 4, lo,
                genTruePred(),
                lo, hi, genIntImm(32 - SR_int_imm(shift_ofst), false));
            ors.append_tail(o);

            //hi = hi >> shift_ofst
            OR_TYPE ort = OR_lsr_i;
            if (is_signed) {
                ort = OR_asr_i;
            } else {
                ort = OR_lsr_i;
            }
            o = buildOR(ort, 1, 3, hi, genTruePred(), hi, shift_ofst);
            ors.append_tail(o);

            ASSERT0(cont);
            cont->set_reg(0, lo);
            //cont->set_reg(1, hi);
            return;
        } else if (SR_int_imm(shift_ofst) <= 63) {
            //lo <- hi asr (ofst - 32)
            //if (is_signed) {
            //    hi <- hi asr shift_ofst
            //} else {
            //    hi <- 0
            //}

            OR_TYPE ort = OR_UNDEF;
            if (is_signed) {
                ort = OR_asr_i;
            }
            else {
                ort = OR_lsr_i;
            }

            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);
            //Do we need asr_i here?
            //lo <- hi asr (ofst - 32)
            OR * set_low = buildOR(ort, 1, 3, lo,
                genTruePred(), hi,
                genIntImm(SR_int_imm(shift_ofst) - 32, true));
            ors.append_tail(set_low);

            OR * set_high = NULL;
            if (is_signed) {
                //DO WE NEED ASR_I HERE ??
                //    hi <- hi asr shift_ofst
                set_high = buildOR(OR_asr_i, 1, 3, hi,
                    genTruePred(), hi, genIntImm(31, true));
            } else {
                //    hi <- 0
                set_high = buildOR(OR_mov_i, 1, 2, hi,
                    genTruePred(), genIntImm(0, true));
            }
            ors.append_tail(set_high);
            ASSERT0(cont);
            cont->set_reg(0, hi);
            //cont->set_reg(1, hi);
            return;
        } else {
            //if (is_signed) {
            //    lo <- hi asr 31
            //    hi <- hi asr 31
            //} else {
            //    lo <- 0
            //    hi <- 0
            //}

            SR * hi = SR_vec(src)->get(1);
            SR * lo = SR_vec(src)->get(0);
            ASSERT0(hi && lo);
            OR * set_high = NULL;
            OR * set_low = NULL;
            if (is_signed) {
                //Do we need asrs_i here?
                //    lo <- hi asr 31
                set_high = buildOR(OR_asr_i, 1, 3, lo,
                    genTruePred(), hi, genIntImm(31, true));
                //    hi <- hi asr 31
                set_low = buildOR(OR_asr_i, 1, 3, hi,
                    genTruePred(), hi, genIntImm(31, true));
            } else {
                //    hi <- 0
                set_high = buildOR(OR_mov_i, 1, 2, hi,
                    genTruePred(), genIntImm(0, true));
                //    lo <- 0
                set_low = buildOR(OR_mov_i, 1, 2, lo,
                    genTruePred(), genIntImm(0, true));
            }
            ASSERT0(set_high && set_low);
            ors.append_tail(set_high);
            ors.append_tail(set_low);
            ASSERT0(cont);
            cont->set_reg(0, hi);
            //cont->set_reg(1, hi);
            return;
        }
    }
    UNREACHABLE();
}


//Generate inter-cluster copy operation.
//Copy from 'src' in 'src_clust' to 'tgt' of in 'tgt_clust'.
OR * ARMCG::buildBusCopy(
        IN SR * src,
        IN SR * tgt,
        IN SR *, //predicate register.
        CLUST src_clust,
        CLUST tgt_clust)
{
    ASSERTN(0, ("Unsupport"));
    return NULL;
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildAddRegImm(
        SR * src,
        SR * imm,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    DUMMYUSE(is_sign);
    ASSERT0(SR_is_reg(src));
    ASSERT0(SR_is_int_imm(imm) ||
            (!m_is_compute_sect_offset || SR_is_var(imm)));
    if (sr_size <= 4) { //< 4bytes
        if (isValidImmOpnd(OR_add_i, SR_is_int_imm(imm))) {
            SR * res = genReg();
            OR * o = buildOR(OR_add_i, 1, 3, res,
                genTruePred(), src, imm);
            cont->set_reg(0, res);
            ors.append_tail(o);
            return;
        }

        SR * t = genReg();
        buildMove(t, genIntImm((HOST_INT)(SR_int_imm(imm)), true), ors, NULL);

        SR * res = genReg();
        OR * o = buildOR(OR_add, 1, 3, res,
            genTruePred(), src, t);
        cont->set_reg(0, res);
        ors.append_tail(o);
    } else if (sr_size <= 8) {
        SRVec * sv = SR_vec(src);
        ASSERT0(sv != NULL && SR_vec_idx(src) == 0);

        //load low 32bit imm
        SR * t = genReg();
        buildMove(t, genIntImm(
            (HOST_INT)(SR_int_imm(imm) & 0xFFFFFFFF), true), ors, NULL);
        SR * t2 = genReg();
        buildMove(t2,
            genIntImm((HOST_INT)((SR_int_imm(imm) >> 32) & 0xFFFFFFFF), true),
            ors, NULL);

        SR * res = genReg();
        SR * res2 = genReg();
        getSRVecMgr()->genSRVec(2, res, res2);
        ASSERT0(res == SR_vec(res)->get(0) && res2 == SR_vec(res)->get(1));

        OR * o = buildOR(OR_adds, 2, 3, res, genRflag(),
            genTruePred(), src, t);
        ors.append_tail(o);

        SR * src2 = sv->get(1);
        ASSERT0(src2);
        o = buildOR(OR_adc, 1, 4, res2,
            genTruePred(), src2, t2, genRflag());
        ors.append_tail(o);

        ASSERT0(cont);
        cont->set_reg(0, res);
        //cont->set_reg(1, res2);
    } else {
        ASSERTN(0, ("TODO"));
    }
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildMul(
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    if (SR_is_int_imm(src1) && SR_is_int_imm(src2)) {
        SR * result = genIntImm((HOST_INT)(SR_int_imm(src1) *
            SR_int_imm(src2)), true);
        ASSERT0(cont);
        cont->set_reg(0, result);
        return;
    } else if (SR_is_reg(src2) && SR_is_reg(src2)) {
        buildMulRegReg(src1, src2, sr_size, is_sign, ors, cont);
        return;
    }
    UNREACHABLE();
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildMulRegReg(
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(SR_is_reg(src1) && SR_is_reg(src2));
    if (sr_size <= 4) { //< 4bytes
        DUMMYUSE(is_sign);
        SR * res = genReg();
        //Both signed and unsigned uses the opcode.
        OR * o = buildOR(OR_mul, 1, 3, res, genTruePred(), src1, src2);
        cont->set_reg(0, res);
        ors.append_tail(o);
        return;
    }
    UNREACHABLE();
}


void ARMCG::buildAddRegReg(
        bool is_add,
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(SR_is_reg(src1) && SR_is_reg(src2));
    if (sr_size <= 4) { //< 4bytes
        OR_TYPE orty = OR_UNDEF;
        if (is_add) {
            orty = OR_add;
        } else {
            orty = OR_sub;
        }
        SR * res = genReg();
        OR * o = buildOR(orty, 1, 3, res, genTruePred(), src1, src2);
        cont->set_reg(0, res);
        ors.append_tail(o);
    } else if (sr_size <= 8) {
        SRVec * sv1 = SR_vec(src1);
        ASSERT0(sv1 && SR_vec_idx(src1) == 0);
        SR * src1_2 = sv1->get(1);
        ASSERT0(src1_2 != NULL && SR_vec_idx(src1_2) == 1);

        SRVec * sv2 = SR_vec(src2);
        ASSERT0(sv2 && SR_vec_idx(src2) == 0);
        SR * src2_2 = sv2->get(1);
        ASSERT0(src2_2 && SR_vec_idx(src2_2) == 1);

        SR * res = getSRVecMgr()->genSRVec(2, genReg(), genReg());
        SR * res_2 = SR_vec(res)->get(1);

        OR_TYPE orty = OR_UNDEF;
        OR_TYPE orty2 = OR_UNDEF;
        List<SR*> reslst;
        List<SR*> opndlst;
        if (is_add) {
            orty = OR_adds;
            orty2 = OR_adc;
            reslst.append_tail(res_2);
            opndlst.append_tail(genTruePred());
            opndlst.append_tail(src1_2);
            opndlst.append_tail(src2_2);
            opndlst.append_tail(genRflag());
            ASSERT0(::tmGetResultNum(orty) == 2);
            ASSERT0(::tmGetOpndNum(orty) == 3);
            ASSERT0(::tmGetResultNum(orty2) == 1);
            ASSERT0(::tmGetOpndNum(orty2) == 4);
        } else {
            orty = OR_subs;
            orty2 = OR_sbc;
            reslst.append_tail(res_2);
            opndlst.append_tail(genTruePred());
            opndlst.append_tail(src1_2);
            opndlst.append_tail(src2_2);
            opndlst.append_tail(genRflag());
            ASSERT0(::tmGetResultNum(orty) == 2);
            ASSERT0(::tmGetOpndNum(orty) == 3);
            ASSERT0(::tmGetResultNum(orty2) == 1);
            ASSERT0(::tmGetOpndNum(orty2) == 4);
        }

        OR * o1 = buildOR(orty, 2, 3, res, genRflag(),
            genTruePred(), src1, src2);
        ors.append_tail(o1);

        OR * o2 = buildOR(orty2, reslst, opndlst);
        ors.append_tail(o2);

        if (!is_add) {
            //TO BE CONFIRMED: What is the purpose of following code?
            ////64BIT c = a - b;
            ////  a:ah, al
            ////  b:bh, bl
            ////  c:ch, cl
            ////
            ////  cl = al - bl
            ////  ch = ah - bh
            ////  if (al < bl) {
            ////      ch = ch - 1
            ////  }
            //ASSERT0(SR_is_reg(src1) && SR_is_reg(src2));
            ////Build: al < bl
            //buildARMCmp(OR_cmp, genTruePred(),
            //    src1, src2, ors, cont);
            //
            ////Build: ch = ch - 1 to compute high-part 32bit value.
            //ORList tors;
            //IOC tmp;
            //buildSub(res_2, genIntImm((HOST_INT)1, false),
            //    GENERAL_REGISTER_SIZE, is_sign, tors, &tmp);
            //ASSERT0(tors.get_elem_count() == 1);
            //res_2 = tmp.get_reg(0); //update the high-part SR.
            //tors.get_head()->set_pred(genLTPred());
            //ors.append_tail(tors);
        }

        ASSERT0(cont);
        //Record the result, also the first element of SRVector.
        cont->set_reg(0, res);
        //cont->set_reg(1, res_2); //no need to record second part.
        assembleSRVec(SR_vec(res), res, res_2);
    } else {
        ASSERTN(0, ("TODO"));
    }
}


void ARMCG::buildARMCmp(
        OR_TYPE cmp_ot,
        IN SR * pred,
        IN SR * opnd0,
        IN SR * opnd1,
        OUT ORList & ors,
        IN IOC *)
{
    ors.append_tail(buildOR(cmp_ot, 1, 3, genRflag(),
                            pred, opnd0, opnd1));
}


void ARMCG::buildUncondBr(IN SR * tgt_lab, OUT ORList & ors, IN IOC *)
{
    ASSERT0(tgt_lab && SR_is_label(tgt_lab));
    //OR_b with TruePred is unconditional branch.
    //ASSERT0(OTD_is_uncond_br(tmGetORTypeDesc(OR_b)));
    OR * o = genOR(OR_b);
    o->setLabel(tgt_lab);
    o->set_pred(genTruePred());
    ors.append_tail(o);
}


void ARMCG::buildCompare(
        OR_TYPE br_cond,
        bool is_truebr,
        IN SR * opnd0,
        IN SR * opnd1,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(opnd0 && opnd1);
    buildARMCmp(br_cond, genTruePred(), opnd0, opnd1, ors, cont);
    ASSERT0(cont);

    //ARM cmp operation does not produce result in register.
    cont->set_reg(RESULT_REGISTER_INDEX, NULL); //record compare result
}


//Build conditional branch operation.
//cont: record the predicated register to
//  condition the execution of Branch operation.
void ARMCG::buildCondBr(IN SR * tgt_lab, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(tgt_lab && SR_is_label(tgt_lab));
    OR * o = genOR(OR_b); //conditional relative offset branch.
    SR * pred = cont->get_pred();
    ASSERT0(pred && SR_is_pred(pred));
    o->set_pred(pred);
    o->setLabel(tgt_lab);
    ors.append_tail(o);
}


//Build memory store operation that store 'reg' into stack.
//NOTE: user have to assign physical register manually if there is
//new OR generated and need register allocation.
void ARMCG::buildStoreAndAssignRegister(
        SR * reg,
        UINT offset,
        ORList & ors,
        IOC * cont)
{
    SR * sr_offset = genIntImm((HOST_INT)offset, false);
    ORList tors;
    buildStore(reg, genSP(), sr_offset, tors, cont);
    switch (tors.get_elem_count()) {
    case 1: break;
    case 2: {
        //add_i sr, SP, offset;
        //store reg -> [sr];
        OR * add = tors.get_head_nth(0);
        ASSERT0(OR_code(add) == OR_add_i);
        OR * st = tors.get_head_nth(1);
        ASSERT0(OR_code(st) == OR_str);
        SR * res = add->get_result(0);
        ASSERT0(res && SR_phy_regid(res) == REG_UNDEF);

        //Use scratch physical register.
        renameResult(add, res, gen_r12(), false);
        renameOpnd(st, res, gen_r12(), false);
        break;
    }
    default: UNREACHABLE();
    }
    ors.append_tail(tors);
}


bool ARMCG::isValidResultRegfile(
        OR_TYPE ortype,
        INT resnum,
        REGFILE regfile) const
{
    if (!CG::isValidResultRegfile(ortype, resnum, regfile)) {
        RegFileSet const* rfs = getValidRegfileSet(ortype, resnum, true);
        if (regfile == RF_SP && rfs->is_contain(RF_R)) {
            return true;
        }
        return false;
    }
    return true;
}


//Check 'regfile' to determine whether it is correct relatived to the 'opndnum'
//operand of 'opcode'.
bool ARMCG::isValidOpndRegfile(
        OR_TYPE ortype,
        INT opndnum,
        REGFILE regfile) const
{
    if (!CG::isValidOpndRegfile(ortype, opndnum, regfile)) {
        RegFileSet const* rfs = getValidRegfileSet(ortype, opndnum, false);
        if (regfile == RF_SP && rfs->is_contain(RF_R)) {
            return true;
        }
        return false;
    }
    return true;
}


//Check if 'sr' has allocated valid physical register corresponding to
//SR-VEC of 'o'.
//'o':
//'sr':
//'idx': opnd/result index of 'sr'.
//'is_result': it is true if 'sr' being the result of 'o'.
bool ARMCG::isValidRegInSRVec(OR *, SR * sr, UINT idx, bool is_result)
{
    DUMMYUSE(is_result);
    CHECK_DUMMYUSE(sr);
    ASSERT0(SR_vec(sr));
    if (idx == 0) {
        ASSERTN(isEvenReg(SR_phy_regid(sr)),
            ("Must be even number register."));
    } else if (idx == 1) {
        ASSERTN(!isEvenReg(SR_phy_regid(sr)),
            ("Must NOT be even number register."));
    } else {
        ASSERTN(0, ("unsupported"));
    }
    return true;
}


bool ARMCG::isEvenReg(REG reg) const
{
    //register start from '1'. And '0' denotes memory.
    return (reg % 2) == 1;
}


//If stack-base-pointer register could use 'unit', return true.
bool ARMCG::isSPUnit(UNIT unit)
{
    return UNIT_A == unit;
}


//Return true if 'sr' can be allocated with integer register.
//'idx': idx of 'sr' in operand list of 'o'.
//'is_result': true if 'sr' is result of 'o', otherwise be false.
bool ARMCG::isIntRegSR(OR * o, SR const* sr, UINT idx, bool is_result) const
{
    if (!SR_is_reg(sr)) { return false; }
    if (SR_regfile(sr) != RF_UNDEF) {
        return tmIsIntRegFile(SR_regfile(sr));
    }
    ASSERT0(o);
    RegFileSet const* rfset = getValidRegfileSet(OR_code(o), idx, is_result);
    if (rfset->is_contain(RF_R)) {
        return true;
    }
    return false;
}


bool ARMCG::isBusCluster(CLUST clust)
{
    return false;
}


//SR that can used on each clusters.
//    predicate,
//    rflags,
//    system,
//    cpointer,
bool ARMCG::isBusSR(SR const* sr)
{
    return false;
}


//Return true if the results of 'o' are independent with other ops, and
//all results can be recalculated any where.
//We can sum up some typical simply expressoin to rematerialize as followed:
//  1. load constant
//  2. frame/stack pointer +/- constant (only use frame/stack
//     register as operand)
//  3. load constant parameter
//  4. load from local data memory for simple(Always on ARM!!)
//  5. load address of variable
bool ARMCG::isRecalcOR(OR * o)
{
    switch (OR_code(o)) {
    //Move constant/symbol operations.
    case OR_mov_i:
        //Instruction must execute unconditionally
        return !isCondExecOR(o);
    case OR_mov: {
        if (isCondExecOR(o)) {
            return false;
        }

        //The only referenced register must be Frame-Pointer.
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * opnd = o->get_opnd(i);
            if (SR_is_reg(opnd) && !SR_is_dedicated(opnd)) {
                return false;
            }
        }
        return true;
    }
    case OR_add_i:
    case OR_sub_i: {
        //ALU with constant/dedicated-reg operations.
        //Get value via calculation of sp/fp/gp,
        //such as : Rd = sp + N or Rd = fp + N
        if (isCondExecOR(o)) {
            return false;
        }

        //The only referenced register must be Frame-Pointer.
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * opnd = o->get_opnd(i);
            if (SR_is_reg(opnd) && !isDedicatedSR(opnd)) {
                return false;
            }
        }
        return true;
    }
    case OR_spadjust:
        //TODO: increase sp as : sp = sp + N
        return false;
    default: break;
    }
    return false;
}


bool ARMCG::isSameCluster(SLOT slot1, SLOT slot2) const
{
    return slot1 == SLOT_G && slot2 == SLOT_G;
}


//Return true if slot1 and slot2 use similar func-unit.
bool ARMCG::isSameLikeCluster(SLOT slot1, SLOT slot2)
{
    return slot1 == SLOT_G && slot2 == SLOT_G;
}


//Return true if the data BUS operation processed that was
//using by other general operations.
bool ARMCG::isSameLikeCluster(OR const* or1, OR const* or2)
{
    return true;
}


//Return true if or-type 'ortype' has the number of 'res_num' results.
bool ARMCG::isMultiResultOR(OR_TYPE ortype, UINT res_num)
{
    DUMMYUSE(res_num);
    if (isMultiLoad(ortype, 2)) {
        return true;
    }
    switch (ortype) {
    case OR_ldm:
    case OR_stm:
    case OR_smull:
    case OR_smlal:
    case OR_umull:
    case OR_umlal:
    case OR_swpb:
    case OR_ldrd:
       return true;
    default:;
    }
    return false;
}


//Return true if or-type 'ortype' has the number of 'res_num' results.
bool ARMCG::isMultiResultOR(OR_TYPE ortype)
{
    return isMultiResultOR(ortype, 2);
}


bool ARMCG::isMultiStore(OR_TYPE ortype, INT opnd_num)
{
    if (opnd_num == -1) {
        switch (ortype) {
        case OR_stm:
        case OR_smull:
        case OR_smlal:
        case OR_strd:
        case OR_strd_i8:
            return true;
        default: break;
        }
    } else if (opnd_num == 2) {
        switch (ortype) {
        case OR_smull:
        case OR_smlal:
        case OR_strd:
        case OR_strd_i8:
            return true;
        default: break;
        }
    }
    return false;
}


bool ARMCG::isMultiLoad(OR_TYPE ortype, INT res_num)
{
    if (res_num == -1) {
        switch (ortype) {
        case OR_ldrd:
        case OR_ldm:
            return true;
        default:;
        }
    } else if (res_num == 2) {
        switch (ortype) {
        case OR_ldrd:
            return true;
        default:;
        }
    }
    return false;
}


bool ARMCG::isCopyOR(OR * o)
{
    switch (OR_code(o)) {
    case OR_add_i:
    case OR_orr_i:
    case OR_eor_i:
        if (SR_is_int_imm(o->get_opnd(2)) && SR_int_imm(o->get_opnd(2)) == 0) {
            return true; //check the index of source operand
        }
        break;
    case OR_and_i:
        if (SR_is_int_imm(o->get_opnd(2)) &&
            SR_int_imm(o->get_opnd(2)) == -1) {
            return true; //check the index of source operand
        }
        break;
    case OR_lsl_i:
    case OR_lsr_i:
    case OR_lsl_i32:
    case OR_lsr_i32:        
    case OR_asr_i32:        
    case OR_asl_i:
    case OR_asr_i:
        if (SR_is_int_imm(o->get_opnd(2)) && SR_int_imm(o->get_opnd(2)) == 0) {
            return true; //check the index of source operand
        }
        break;
    case OR_mov:
    case OR_mov_i:
        return true;
    default:;
    }
    return false;
}


bool ARMCG::isStackPointerValueEqu(SR const* base1, SR const* base2)
{
    return base1 == base2;
}


//Return true if sr is stack base pointer.
bool ARMCG::isSP(SR const* sr) const
{
    return sr == m_sp || sr == m_fp || sr == m_gp;
}


//How to determine whether if an operation is reducible?
//1.Find the DEF of induction variable.
//2.Induction variable should be global register.
//3.Matching the reduction opcode.
bool ARMCG::isReduction(OR * o)
{
    if (isCondExecOR(o)) { return false; }

    INT reduct_opnd = 0;
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (!SR_is_reg(opnd) ||
            opnd == genTruePred() ||
            !SR_is_global(opnd)) {
            continue;
        }
        if (mustDef(o, opnd)) {
            reduct_opnd++;
        }
    }

    if (reduct_opnd == 0) {
        return false;
    }

    //OR should be formated as: sr = sr + 1
    switch (OR_code(o)) {
    case OR_add_i:
    case OR_sub_i:
        return true;
    default:;
    }
    return false;
}


//Return the index of copied source operand.
INT ARMCG::computeCopyOpndIdx(OR * o)
{
    OR_TYPE opr = OR_code(o);
    switch (opr) {
    case OR_add_i:
    case OR_orr_i:
    case OR_eor_i:
        if (SR_is_int_imm(o->get_opnd(2)) && SR_int_imm(o->get_opnd(2)) == 0) {
            return HAS_PREDICATE_REGISTER;
        }
        break;
    case OR_and_i:
         if (SR_is_int_imm(o->get_opnd(2)) &&
             SR_int_imm(o->get_opnd(2)) == -1) {
            return HAS_PREDICATE_REGISTER;
        }
        break;
    case OR_lsl_i:
    case OR_lsr_i:
    case OR_lsl_i32:
    case OR_lsr_i32:
    case OR_asr_i32:
    case OR_asr_i:
        if (SR_is_int_imm(o->get_opnd(2)) && SR_int_imm(o->get_opnd(2)) == 0) {
            return HAS_PREDICATE_REGISTER;
        }
        break;
    case OR_mov_i:
    case OR_mov:
        return HAS_PREDICATE_REGISTER;
    default:;
    }
    return -1;
}


//To compute memory size of load/store.
INT ARMCG::computeMemByteSize(OR * o)
{
    ASSERTN(OR_is_mem(o), ("Need memory operation"));
    switch (OR_code(o)) {
    case OR_ldm: {
        UINT sz = 0;
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (!SR_is_reg(sr) ||
                SR_is_pred(sr) ||
                SR_is_dedicated(sr)) {
                continue;
            }
            sz += 4;
        }
        return sz;
    }
    case OR_stm: {
        UINT sz = 0;
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (!SR_is_reg(sr) ||
                SR_is_pred(sr) ||
                SR_is_dedicated(sr)) {
                continue;
            }
            sz += 4;
        }
        return sz;
    }
    case OR_ldrb:
    case OR_ldrb_i12:
    case OR_ldrsb_i8:
    case OR_strb:
    case OR_strb_i12:
        return 1;
    case OR_ldrh:
    case OR_ldrsh:
    case OR_ldrh_i8:
    case OR_ldrsh_i8:
    case OR_strh:
    case OR_strsh:
    case OR_strh_i8:
        return 2;
    case OR_ldr:
    case OR_ldr_i12:
    case OR_str:
    case OR_str_i12:
        return 4;
    case OR_ldrd:
    case OR_strd:
    case OR_strd_i8:
        return 8;
    default: ASSERTN(0, ("Not memory opcode"));
    }
    return -1;
}


//Return cluster of bus operation.
CLUST ARMCG::computeClusterOfBusOR(OR *)
{
    return CLUST_FIRST;
}


SLOT ARMCG::computeORSlot(OR const* o)
{
    SLOT slot = FIRST_SLOT;
    if (OR_is_bus(o) || OR_is_asm(o) || OR_code(o) == OR_spadjust) {
        //BUS OR might occupy multiple func unit.
        slot = SLOT_G;
    } else if (OR_clust(o) == CLUST_FIRST) {
        UNIT unit = computeORUnit(o)->checkAndGet();
        if (UNIT_A == unit) {
            slot = SLOT_G;
        } else {
            ASSERTN(0, ("illegal func unit assignment!!!!"));
        }
    } else {
        ASSERTN(0,("Not any cluster assigned to above o."));
    }
    return slot;
}


void ARMCG::expandFakeStore(IN OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o && ipl);
    OR_TYPE real_code = OR_UNDEF;
    switch (OR_code(o)) {
    case OR_str: real_code = OR_str_i12; break;
    case OR_strd: real_code = OR_strd_i8; break;
    case OR_strb: real_code = OR_strb_i12; break;
    case OR_strh: real_code = OR_strh_i8; break;
    default: UNREACHABLE();
    }

    SR * ofst = o->get_store_ofst();
    ASSERT0(SR_is_int_imm(ofst));
    if (isValidImmOpnd(real_code, SR_int_imm(ofst))) {
        IssuePackage * ip = allocIssuePackage();
        ip->set(SLOT_G, o);
        ipl->append_tail(ip);
        return;
    }

    // [base+ofst] = v;
    //=>
    // r12 = base+ofst;
    // [r12+0] = v;
    SR * base = o->get_store_base();
    ORList ors;
    IOC cont;
    buildAdd(base, ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));

    ASSERT0(ors.get_elem_count() == 1);
    OR * last = ors.get_tail();
    ASSERT0(last && (OR_code(last) == OR_add ||
                     OR_code(last) == OR_add_i));
    last->set_result(0, gen_r12()); //replace result-register with r12.
    for (OR * o2 = ors.get_head(); o2 != NULL; o2 = ors.get_next()) {
        o2->set_pred(o->get_pred());
    }
    renameOpnd(o, base, gen_r12(), false);
    renameOpnd(o, ofst, genIntImm(0, true), false);

    if (OR_is_fake(last)) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = allocIssuePackage();
        ip->set(SLOT_G, last);
        ipl->append_tail(ip);
    }

    OR_code(o) = real_code;
    IssuePackage * ip2 = allocIssuePackage();
    ip2->set(SLOT_G, o);
    ipl->append_tail(ip2);
    return;
}


void ARMCG::expandFakeSpadjust(IN OR * o, OUT IssuePackageList * ipl)
{
    SR * ofst = o->get_opnd(HAS_PREDICATE_REGISTER + 1);
    ASSERT0(SR_is_int_imm(ofst));
    ORList ors;
    IOC cont;

    // spadjust, SIZEOFSTACK
    //=>
    // sp = sp - SIZEOFSTACK
    buildAdd(genSP(), ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));

    if (ors.get_elem_count() == 1) {
        // add sr66 <--tp, sp, #Imm10
        //=>
        // add sp <--PRED, sp, #Imm10
        OR * add = ors.get_head();
        ASSERT0(add && SR_is_reg(add->get_result(0)));

        add->set_pred(o->get_pred());
        add->set_result(0, genSP());

        if (OR_is_fake(add)) {
            expandFakeOR(add, ipl);
        }
        return;
    }
    if (ors.get_elem_count() == 2) {
        // movw_i sr65 <--tp, #108
        // add sr66 <--tp, sp, sr65
        //=>
        // movw_i t <--tp, #108
        // add sp <--tp, sp, t
        ASSERT0(OR_is_movi(ors.get_head_nth(0)));
        ASSERT0(OR_code(ors.get_head_nth(1)) == OR_add);

        OR * movi = ors.get_head_nth(0);
        SR * res = movi->get_result(0);
        movi->set_mov_to(o->get_result(1));
        ASSERT0(!OR_is_fake(movi));

        OR * add = ors.get_head_nth(1);
        renameOpnd(add, res, o->get_result(1), false);
        add->set_result(0, genSP());

        for (OR * o2 = ors.get_head(); o2 != NULL; o2 = ors.get_next()) {
            IssuePackage * ip = allocIssuePackage();
            o2->set_pred(o->get_pred());
            ip->set(SLOT_G, o2);
            ipl->append_tail(ip);
        }
        return;
    }
    if (ors.get_elem_count() == 3) {
        // movw_i sr57 <--tp, #65428
        // movt_i sr57 <--tp, #65535
        // add sr58 <--tp, sp, sr57
        //=>
        // movw_i t <--tp, #65428
        // movt_i t <--tp, #65535
        // add sp <--tp, sp, t
        ASSERT0(OR_is_movi(ors.get_head_nth(0)));
        ASSERT0(OR_is_movi(ors.get_head_nth(1)));
        ASSERT0(OR_code(ors.get_head_nth(2)) == OR_add);

        OR * movi = ors.get_head_nth(0);
        ASSERT0(!OR_is_fake(movi));

        SR * res = movi->get_result(0);
        movi->set_mov_to(o->get_result(1));
        ors.get_head_nth(1)->set_mov_to(o->get_result(1));

        OR * add = ors.get_head_nth(2);
        renameOpnd(add, res, o->get_result(1), false);
        add->set_result(0, genSP());

        for (OR * o2 = ors.get_head(); o2 != NULL; o2 = ors.get_next()) {
            IssuePackage * ip = allocIssuePackage();
            o2->set_pred(o->get_pred());
            ip->set(SLOT_G, o2);
            ipl->append_tail(ip);
        }
        return;
    }
    UNREACHABLE();
}


void ARMCG::expandFakeLoad(IN OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o && ipl);
    OR_TYPE real_code = OR_UNDEF;
    switch (OR_code(o)) {
    case OR_ldr: real_code = OR_ldr_i12; break;
    case OR_ldrb: real_code = OR_ldrb_i12; break;
    case OR_ldrh: real_code = OR_ldrh_i8; break;
    case OR_ldrsb: real_code = OR_ldrsb_i8; break;
    case OR_ldrsh: real_code = OR_ldrsh_i8; break;
    case OR_ldrd: real_code = OR_ldrd_i8; break;
    default: UNREACHABLE();
    }

    SR * ofst = o->get_load_ofst();
    ASSERT0(SR_is_int_imm(ofst));
    if (isValidImmOpnd(real_code, SR_int_imm(ofst))) {
        IssuePackage * ip = allocIssuePackage();
        ip->set(SLOT_G, o);
        ipl->append_tail(ip);
        return;
    }

    //  sr1 = [sr3, ofst]
    //=>
    //  sr1 = sr3 + ofst
    //  sr1 = [sr1]
    SR * sr1 = o->get_result(0);
    SR * sr3 = o->get_load_base();
    ASSERT0(sr1 && sr3 && SR_is_reg(sr1) && SR_is_reg(sr3));
    ASSERT0(ofst);

    ORList ors;
    IOC cont;
    buildAdd(sr3, ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));

    ASSERT0(ors.get_elem_count() == 1);
    OR * last = ors.remove_tail();
    ASSERT0(last && (OR_code(last) == OR_add ||
                     OR_code(last) == OR_add_i));
    last->set_result(0, sr1); //replace result-register with sr1.
    if (OR_is_fake(last)) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = allocIssuePackage();
        last->set_pred(o->get_pred());
        ip->set(SLOT_G, last);
        ipl->append_tail(ip);
    }

    OR_code(o) = real_code;
    o->set_load_base(sr1);
    o->set_load_ofst(genIntImm(0, false));
    IssuePackage * ip = allocIssuePackage();
    ip->set(SLOT_G, o);
    ipl->append_tail(ip);
}


void ARMCG::expandFakeMultiLoad(IN OR * o, OUT IssuePackageList * ipl)
{
    //  sr1, sr2 = [sr3, ofst]
    //=>
    //  sr1 = sr3 + ofst
    //  sr1, sr2 = [sr1]
    SR * sr1 = o->get_result(0);
    SR * sr2 = o->get_result(1);
    SR * sr3 = o->get_load_base();
    ASSERT0(sr1 && sr3 && SR_is_reg(sr1) && SR_is_reg(sr3));

    SR * ofst = o->get_load_ofst();
    ASSERT0(ofst);
    ASSERT0(SR_is_int_imm(ofst));
    if (isValidImmOpnd(OR_ldrd_i8, SR_int_imm(ofst))) {
        OR_code(o) = OR_ldrd_i8;
        IssuePackage * ip = allocIssuePackage();
        ip->set(SLOT_G, o);
        ipl->append_tail(ip);
        return;
    }

    ORList ors;
    IOC cont;
    buildAdd(sr3, ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));
    ASSERT0(ors.get_elem_count() == 1);

    OR * last = ors.remove_tail();
    ASSERT0(last && (OR_code(last) == OR_add || OR_code(last) == OR_add_i));
    last->set_result(0, sr1); //replace result-register with sr1.

    if (OR_is_fake(last)) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = allocIssuePackage();
        last->set_pred(o->get_pred());
        ip->set(SLOT_G, last);
        ipl->append_tail(ip);
    }

    OR * ldrd = buildOR(OR_ldrd_i8, 2, 3, sr1, sr2,
        o->get_pred(), sr1, genIntImm(0, false));
    ldrd->set_pred(o->get_pred());
    OR_dbx(ldrd).copy(OR_dbx(o));

    IssuePackage * ip = allocIssuePackage();
    ip->set(SLOT_G, ldrd);
    ipl->append_tail(ip);
}


void ARMCG::expandFakeMov32(IN OR * o, OUT IssuePackageList * ipl)
{
    SR * to = o->get_mov_to();
    SR * from = o->get_mov_from();
    ASSERT0(to && from);
    OR * low = NULL;
    OR * high = NULL;

    //Decompose OR_mov32_i into OR_movw_i and OR_movt_i.
    if (SR_type(from) == SR_INT_IMM) {
        // mov32 rd = imm
        //=>
        // movw rd, first_half_part(imm)
        // movt rd, second_half_part(imm)
        low = genOR(OR_movw_i);
        low->set_mov_to(to);
        low->set_mov_from(genIntImm(
            (HOST_INT)(SR_int_imm(from) & 0xFFFF), true));
        low->set_pred(o->get_pred());

        high = genOR(OR_movt_i);
        high->set_mov_to(to);
        high->set_mov_from(genIntImm(
            (HOST_INT)((SR_int_imm(from) >> 16) & 0xFFFF), true));
        high->set_pred(o->get_pred());
    } else if (SR_type(from) == SR_VAR) {
        // mov32 rd = SYM
        //=>
        // movw rd, SYM
        // movt rd, SYM
        low = genOR(OR_movw_i);
        low->set_mov_to(to);
        low->set_mov_from(genVAR(SR_var(from)));
        low->set_pred(o->get_pred());

        high = genOR(OR_movt_i);
        high->set_mov_to(to);
        high->set_mov_from(genVAR(SR_var(from)));
        high->set_pred(o->get_pred());
    } else {
        UNREACHABLE();
    }

    ASSERT0(low && high);
    OR_dbx(low).copy(OR_dbx(o));
    OR_dbx(high).copy(OR_dbx(o));
    IssuePackage * ip = allocIssuePackage();
    ip->set(SLOT_G, low);
    ipl->append_tail(ip);
    ip = allocIssuePackage();
    ip->set(SLOT_G, high);
    ipl->append_tail(ip);
}


void ARMCG::expandFakeShift(IN OR * o, OUT IssuePackageList * ipl)
{
    OR_TYPE ckort = OR_UNDEF;
    OR_TYPE newort = OR_UNDEF;
    switch (OR_code(o)) {
    case OR_lsr_i32:
        ckort = OR_lsr_i;
        newort = OR_lsr;
        break;
    case OR_lsl_i32:
        ckort = OR_lsl_i;
        newort = OR_lsl;
        break;
    case OR_asr_i32:
        ckort = OR_asr_i;
        newort = OR_asr;
        break;
    default: UNREACHABLE();
    }
    SR * imm = o->get_opnd(2);
    ASSERT0(SR_is_imm(imm));
    if (!isValidImmOpnd(ckort, 2, SR_int_imm(imm))) {
        // rd = rs + imm;
        //=>
        // r12 = imm;
        // rd = rs + r12;
        ORList ors;
        IOC cont;
        buildMove(gen_r12(), imm, ors, &cont);
        OR * mv = ors.get_tail();
        ASSERT0(mv && ors.get_elem_count() == 1);
        mv->set_pred(o->get_pred());
        OR_dbx(mv).copy(OR_dbx(o));

        if (OR_is_fake(mv)) {
            expandFakeOR(mv, ipl);
        } else {
            IssuePackage * ip = allocIssuePackage();
            ip->set(SLOT_G, mv);
            ipl->append_tail(ip);
        }

        renameOpnd(o, imm, gen_r12(), false);

        //Change OR code from OR_xxx_i to OR_xxx.
        OR_code(o) = newort;

        IssuePackage * ip2 = allocIssuePackage();
        ip2->set(SLOT_G, o);
        ipl->append_tail(ip2);
    } else {
        IssuePackage * ip = allocIssuePackage();
        ip->set(SLOT_G, o);
        ipl->append_tail(ip);
    }
}


void ARMCG::expandFakeOR(IN OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(OR_is_fake(o) && ipl);
    switch (OR_code(o)) {
    case OR_str:
    case OR_strd:
    case OR_strb:
    case OR_strsb:
    case OR_strh:
    case OR_strsh:
       expandFakeStore(o, ipl);
       break;
    case OR_spadjust:
       expandFakeSpadjust(o, ipl);
       break;
    case OR_ldr:
    case OR_ldrb:
    case OR_ldrsb:
    case OR_ldrh:
    case OR_ldrsh:
       expandFakeLoad(o, ipl);
       break;
    case OR_ldrd:
       expandFakeMultiLoad(o, ipl);
       break;
    case OR_mov32_i:
       expandFakeMov32(o, ipl);
       break;
    case OR_lsr_i32:
    case OR_lsl_i32:
    case OR_asr_i32:
       expandFakeShift(o, ipl);
       break;
    case OR_add_i:
    case OR_sub_i:
    case OR_lsr_i: {
        SR * imm = o->get_opnd(2);
        ASSERT0(SR_is_imm(imm));
        if (!isValidImmOpnd(OR_code(o), 2, SR_int_imm(imm))) {
            // rd = rs + imm;
            //=>
            // r12 = imm;
            // rd = rs + r12;
            ORList ors;
            IOC cont;
            buildMove(gen_r12(), imm, ors, &cont);
            OR * mv = ors.get_tail();
            ASSERT0(mv && ors.get_elem_count() == 1);
            mv->set_pred(o->get_pred());
            OR_dbx(mv).copy(OR_dbx(o));

            if (OR_is_fake(mv)) {
                expandFakeOR(mv, ipl);
            } else {
                IssuePackage * ip = allocIssuePackage();
                ip->set(SLOT_G, mv);
                ipl->append_tail(ip);
            }

            renameOpnd(o, imm, gen_r12(), false);

            //Change OR code from OR_xxx_i to OR_xxx.
            OR_TYPE newort = OR_UNDEF;
            switch (OR_code(o)) {
            case OR_add_i: newort = OR_add; break;
            case OR_sub_i: newort = OR_sub; break;
            case OR_lsr_i: newort = OR_lsr; break;
            default: UNREACHABLE();
            }
            OR_code(o) = newort;

            IssuePackage * ip2 = allocIssuePackage();
            ip2->set(SLOT_G, o);
            ipl->append_tail(ip2);
        } else {
            IssuePackage * ip = allocIssuePackage();
            ip->set(SLOT_G, o);
            ipl->append_tail(ip);
        }
        break;
    }
    default: UNREACHABLE();
    }
}


bool ARMCG::skipArgRegister(
        VAR const* param,
        RegSet const* regset,
        REG reg) const
{
    #ifdef TO_BE_COMPATIBLE_WITH_ARM_LINUX_GNUEABI
    if (//only check value that is in paired-register
        param->getByteSize(m_tm) == CONTINUOUS_REG_NUM * GENERAL_REGISTER_SIZE

        //only check non-memory-chunk
        && !param->is_mc()) {
        ASSERT0(param->getDType() == xoc::D_U64 ||
                param->getDType() == xoc::D_I64 ||
                param->getDType() == xoc::D_F64);
        if (!isEvenReg(reg) || //paired register start at even.

            //at least two registers available
            regset->get_next(reg) == -1) {
            //Passed the odd number register to facilitate the use
            //of value in paired-register.
            ASSERTN(isEvenReg(regset->get_next(reg)),
                ("not continuous"));
            return true;
        }
    }
    #endif
    return false;
}
