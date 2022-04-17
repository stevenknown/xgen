/*@
Copyright (c) 2013-2021, Su Zhenyu steven.known@gmail.com

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
#include "../xgen/xgeninc.h"

void ARMCG::initDedicatedSR()
{
    genSP();
    genFP();
    genTruePred();
    genRflag();
    genParamPointer();
}


DataDepGraph * ARMCG::allocDDG()
{
    return new ARMDDG(this);
}


LIS * ARMCG::allocLIS(ORBB * bb, DataDepGraph * ddg, BBSimulator * sim,
                      UINT sch_mode)
{
    return new ARMLIS(bb, *ddg, sim, sch_mode);
}


//'is_log': false value means that Caller will delete
//    the object allocated utilmately.
BBSimulator * ARMCG::allocBBSimulator(ORBB * bb)
{
    return new ARMSim(bb);
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
    SR * sr = genDedicatedReg(REG_FP);
    SR_is_fp(sr) = 1;
    return sr;
}


SR * ARMCG::genRflag()
{
    SR * sr = genDedicatedReg(REG_RFLAG_REGISTER);
    SR_is_rflag(sr) = 1;
    return sr;
}


//Get stack pointer.
SR * ARMCG::getSP() const
{
    return getDedicatedReg(REG_SP);
}


//Get global register pointer.
SR * ARMCG::getGP() const
{
    ASSERT0(0); //TODO
    return getDedicatedReg(0);
}


//Get frame pointer.
SR * ARMCG::getFP() const
{
    return getDedicatedReg(REG_FP);
}


SR * ARMCG::getRflag() const
{
    return getDedicatedReg(REG_RFLAG_REGISTER);
}


SR * ARMCG::genR0()
{
    SR * sr = genDedicatedReg(REG_R0);
    SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
    return sr;
}


SR * ARMCG::genR1()
{
    SR * sr = genDedicatedReg(REG_R1);
    SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
    return sr;
}


SR * ARMCG::genR2()
{
    SR * sr = genDedicatedReg(REG_R2);
    SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
    return sr;
}


SR * ARMCG::genR3()
{
    SR * sr = genDedicatedReg(REG_R3);
    SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
    return sr;
}


//Scratch Register, the synonym is IP register.
SR * ARMCG::genR12()
{
    SR * sr = genDedicatedReg(REG_R12);
    SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
    return sr;
}


SR * ARMCG::genTruePred()
{
    SR * sr = genDedicatedReg(REG_TRUE_PRED);
    SR_is_pred(sr) = true;
    return sr;
}


SR * ARMCG::getTruePred() const
{
    return getDedicatedReg(REG_TRUE_PRED);
}


SR * ARMCG::getEQPred() const
{
    return getDedicatedReg(REG_EQ_PRED);
}


SR * ARMCG::getNEPred() const
{
    return getDedicatedReg(REG_NE_PRED);
}


SR * ARMCG::getCSPred() const
{
    return getDedicatedReg(REG_CS_PRED);
}


SR * ARMCG::getHSPred() const
{
    return getDedicatedReg(REG_HS_PRED);
}


SR * ARMCG::getCCPred() const
{
    return getDedicatedReg(REG_CC_PRED);
}


SR * ARMCG::getLOPred() const
{
    return getDedicatedReg(REG_LO_PRED);
}


//Unsigned GT
SR * ARMCG::getHIPred() const
{
    return getDedicatedReg(REG_HI_PRED);
}


//Unsigned LE
SR * ARMCG::getLSPred() const
{
    return getDedicatedReg(REG_LS_PRED);
}


SR * ARMCG::getMIPred() const
{
    return getDedicatedReg(REG_MI_PRED);
}


SR * ARMCG::getPLPred() const
{
    return getDedicatedReg(REG_PL_PRED);
}


SR * ARMCG::getGEPred() const
{
    return getDedicatedReg(REG_GE_PRED);
}


SR * ARMCG::getLTPred() const
{
    return getDedicatedReg(REG_LT_PRED);
}


SR * ARMCG::getGTPred() const
{
    return getDedicatedReg(REG_GT_PRED);
}


SR * ARMCG::getLEPred() const
{
    return getDedicatedReg(REG_LE_PRED);
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


//Get function return address register.
SR * ARMCG::getReturnAddr() const
{
    return getDedicatedReg(REG_RETURN_ADDRESS_REGISTER);
}


void ARMCG::buildLoad(IN SR * load_val, IN SR * base, IN SR * ofst,
                      bool is_signed, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(load_val && base && ofst && cont);
    ASSERT0(ofst->is_int_imm());
    Var const* v = nullptr;
    SR * sr_ofst = nullptr;
    if (base->is_var()) {
        SR * sr_base;
        v = SR_var(base);
        computeVarBaseAndOffset(SR_var(base), ofst->getInt(),
                                &sr_base, &sr_ofst);
        if (v->is_global() && !sr_base->is_reg()) {
            //ARM does not support load value from memory label directly.
            SR_var_ofst(base) += (UINT)ofst->getInt();

            if (sr_ofst->is_int_imm()) {
                SR_int_imm(sr_ofst) = SR_var_ofst(base);
            } else {
                ASSERT0(isComputeStackOffset());
                ASSERT0(sr_ofst->is_var());
            }

            //Write address of memory symbol into base register, then
            //build an indirect load from the base register.
            SR * res = genReg();
            buildMove(res, base, ors, cont);
            base = res;
        } else {
            ASSERT0(sr_base->is_reg());
            base = sr_base;
        }
    } else {
        sr_ofst = ofst;
    }

    ASSERT0(sr_ofst);
    ASSERT0(cont);

    if (IOC_mem_byte_size(cont) == 3) {
        //Generate: ldr %rx = value;
        OR * ld = genOR(OR_ldr);
        ld->set_first_load_val(load_val, this);
        ld->set_load_base(base, this);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        //Mapping from LD OR to correspond variable. Used by OR::dump()
        setMapOR2Mem(ld, v);
        ld->set_load_ofst(sr_ofst, this);
        ors.append_tail(ld);

        //Generate: mov %ry, 0xFFffFF00;
        ASSERT0(!isValidImmOpnd(OR_and_i,
                                getMaskByByte(IOC_mem_byte_size(cont))));
        SR * maskbit = genIntImm(getMaskByByte(IOC_mem_byte_size(cont)),
                                 false);
        SR * reg = genReg();
        buildMove(reg, maskbit, ors, cont);

        //Generate: and_r %rx, %ry;
        OR * mask = buildOR(OR_and, 1, 3, load_val, getTruePred(), load_val,
                            reg);
        ors.append_tail(mask);
        return;
    }

    if (IOC_mem_byte_size(cont) <= 4) {
        OR_TYPE ort;
        switch (IOC_mem_byte_size(cont)) {
        case 1: ort = is_signed ? OR_ldrsb : OR_ldrb; break;
        case 2: ort = is_signed ? OR_ldrsh : OR_ldrh; break;
        case 4: ort = OR_ldr; break;
            break;
        default: UNREACHABLE();
        }
        OR * ld = genOR(ort);
        ld->set_first_load_val(load_val, this);
        ld->set_load_base(base, this);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        ld->set_load_ofst(sr_ofst, this);

        //Mapping from LD OR to correspond variable. Used by OR::dump()
        setMapOR2Mem(ld, v);
        ors.append_tail(ld);
        return;
    }

    if (IOC_mem_byte_size(cont) <= 8) {
        OR * o = genOR(OR_ldrd);
        ASSERT0(load_val->getByteSize() == 8);
        ASSERT0(load_val->is_vec());
        ASSERT0(load_val->getVec()->get(0) && load_val->getVec()->get(1));
        o->set_load_val(load_val->getVec()->get(0), this, 0);
        o->set_load_val(load_val->getVec()->get(1), this, 1);
        o->set_load_base(base, this);

        //If the bitsize of sr_ofst exceeded the capacity of operation,
        //use R12 the scatch register to record the offset.
        o->set_load_ofst(sr_ofst, this);
        setMapOR2Mem(o, v);
        ors.append_tail(o);
        return;
    }

    UNREACHABLE();
}


void ARMCG::buildStoreCase13Bytes(IN SR * store_val, IN SR * base,
                                  IN SR * sr_ofst, Var const* v,
                                  OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(IOC_mem_byte_size(cont) == 3);

    //Generate: mov %ry, 0xFFffFF00;
    ASSERT0(!isValidImmOpnd(OR_and_i,
                            getMaskByByte(IOC_mem_byte_size(cont))));
    SR * maskbit = genIntImm(getMaskByByte(IOC_mem_byte_size(cont)),
                             false);
    SR * reg = genReg();
    buildMove(reg, maskbit, ors, cont);

    //Generate: and_r %rx, %ry;
    OR * mask = buildOR(OR_and, 1, 3, store_val, getTruePred(),
                        store_val, reg);
    ors.append_tail(mask);

    //Generate: sdr %rx = value;
    OR * st = genOR(OR_str);
    st->set_first_store_val(store_val, this);
    st->set_store_base(base, this);

    //If the bitsize of sr_ofst exceeded the capacity of operation,
    //use R12 the scatch register to record the offset.
    //Mapping from LD OR to correspond variable. Used by OR::dump()
    setMapOR2Mem(st, v);
    st->set_store_ofst(sr_ofst, this);
    ors.append_tail(st);
}


void ARMCG::buildStoreCase1(IN SR * store_val, IN SR * base, IN SR * sr_ofst,
                            Var const* v, bool is_signed,
                            OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(IOC_mem_byte_size(cont) <= 4);
    if (IOC_mem_byte_size(cont) == 3) {
        buildStoreCase13Bytes(store_val, base, sr_ofst, v, ors, cont);
        return;
    }

    OR_TYPE code = OR_UNDEF;
    switch (IOC_mem_byte_size(cont)) {
    case 1:
        if (sr_ofst->is_int_imm()) {
            if (isValidImmOpnd(OR_strb_i12, sr_ofst->getInt())) {
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
                ASSERT0(base && base->is_reg());
                code = OR_strb;
                sr_ofst = genIntImm(0, false);
            }
        } else if (sr_ofst->is_var()) {
            code = OR_strb;
        } else {
            UNREACHABLE();
        }
        break;
    case 2:
        if (sr_ofst->is_int_imm()) {
            if (isValidImmOpnd(OR_strh_i8, sr_ofst->getInt())) {
                code = OR_strh_i8;
            } else {
                IOC tc;
                buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                         false, ors, &tc);
                base = tc.get_reg(0);
                ASSERT0(base && base->is_reg());
                code = OR_str;
                sr_ofst = genIntImm(0, false);
            }
        } else if (sr_ofst->is_var()) {
            code = OR_strh;
        } else {
            UNREACHABLE();
        }
        break;
    case 4:
        if (sr_ofst->is_int_imm()) {
            if (isValidImmOpnd(OR_str_i12, sr_ofst->getInt())) {
                code = OR_str_i12;
            } else {
                IOC tc;
                buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                         false, ors, &tc);
                base = tc.get_reg(0);
                ASSERT0(base && base->is_reg());
                code = OR_str;
                sr_ofst = genIntImm(0, false);
            }
        } else if (sr_ofst->is_var()) {
            code = OR_str;
        } else {
            UNREACHABLE();
        }
        break;
    default: UNREACHABLE();
    }

    OR * o = genOR(code);
    o->set_first_store_val(store_val, this);
    o->set_store_base(base, this);

    //If the bitsize of sr_ofst exceeded the capacity of operation,
    //use R12 the scatch register to record the offset.
    o->set_store_ofst(sr_ofst, this);

    //Mapping from LD OR to corresponnd variable. Used by OR::dump()
    setMapOR2Mem(o, v);
    ors.append_tail(o);
}


void ARMCG::buildStoreCase2(IN SR * store_val,
                            IN SR * base,
                            IN SR * sr_ofst,
                            Var const* v,
                            bool is_signed,
                            OUT ORList & ors,
                            MOD IOC * cont)
{
    ASSERT0(IOC_mem_byte_size(cont) > 4 && IOC_mem_byte_size(cont) <= 8);
    OR_TYPE code = OR_UNDEF;
    if (sr_ofst->is_int_imm()) {
        if (isValidImmOpnd(OR_strd_i8, sr_ofst->getInt())) {
            code = OR_strd_i8;
        } else {
            IOC tc;
            buildAdd(base, sr_ofst, IOC_mem_byte_size(cont),
                     false, ors, &tc);
            base = tc.get_reg(0);
            ASSERT0(base && base->is_reg());
            code = OR_str;
            sr_ofst = genIntImm(0, false);
        }
    } else if (sr_ofst->is_var()) {
        code = OR_strd;
    } else {
        UNREACHABLE();
    }
    OR * o = genOR(code);
    ASSERT0(store_val->getByteSize() == 8);
    ASSERT0(store_val->is_vec());
    ASSERT0(store_val->getVec()->get(0) &&
            store_val->getVec()->get(1));
    o->set_store_val(store_val->getVec()->get(0), this, 0);
    o->set_store_val(store_val->getVec()->get(1), this, 1);
    o->set_store_base(base, this);

    //If the bitsize of sr_ofst exceeded the capacity of operation,
    //use R12 the scatch register to record the offset.
    o->set_store_ofst(sr_ofst, this);

    setMapOR2Mem(o, v);
    ors.append_tail(o);
}


//Some target machine applys comparing instruction to calculate boolean.
//e.g:
//    r1 = 0x2,
//    r2 = 0x2,
//    r0 = eq, r1, r2 ;then r0 is 1.
OR_TYPE ARMCG::mapIRType2ORType(IR_TYPE ir_type,
                                UINT ir_opnd_size,
                                IN SR * opnd0,
                                IN SR * opnd1,
                                bool is_signed)
{
    DUMMYUSE(opnd0);
    OR_TYPE orty = OR_UNDEF;
    switch (ir_type) {
    case IR_ADD:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            if (is_signed) {
                orty = OR_add;
            } else {
                orty = OR_add;
            }
        } else if (opnd1->is_int_imm()) {
            if (isValidImmOpnd(OR_add_i, opnd1->is_int_imm())) {
                orty = OR_add_i;
            }
        }
        break;
    case IR_SUB:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            if (is_signed) {
                orty = OR_sub;
            } else {
                orty = OR_sub;
            }
        } else if (opnd1->is_int_imm()) {
            if (isValidImmOpnd(OR_sub_i, opnd1->is_int_imm())) {
                orty = OR_sub_i;
            }
        }
        break;
    case IR_MUL:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            if (ir_opnd_size > BYTE_PER_SHORT) {
                orty = OR_UNDEF;
                break;
            }
            if (is_signed) {
                orty = OR_mul;
            } else {
                orty = OR_mul;
            }
        }
        break;
    case IR_DIV:
    case IR_REM:
    case IR_MOD:
        break;
    case IR_BAND:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            orty = OR_and;
        } else if (opnd1->is_int_imm()) {
            if (isValidImmOpnd(OR_and_i, opnd1->is_int_imm())) {
                orty = OR_and_i;
            }
        }
        break;
    case IR_BOR:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            orty = OR_orr;
        } else if (opnd1->is_int_imm()) {
            if (isValidImmOpnd(OR_orr_i, opnd1->is_int_imm())) {
                orty = OR_orr_i;
            }
        }
        break;
    case IR_XOR:
        ASSERT0(opnd0->is_reg() && opnd1);
        if (opnd1->is_reg()) {
            orty = OR_eor;
        } else if (opnd1->is_int_imm()) {
            orty = OR_eor_i;
        }
        break;
    case IR_BNOT:
        break;
    case IR_NEG:
        if (opnd0->is_reg()) {
            orty = OR_neg;
        }
        break;
    case IR_LT:
        ASSERT0(opnd0->is_reg() && opnd1);
        break;
    case IR_LE:
        break;
    case IR_GT:
        break;
    case IR_GE:
        break;
    case IR_EQ:
        break;
    case IR_NE:
        break;
    case IR_IGOTO:
        orty = OR_bx;
        break;
    case IR_GOTO:
        orty = OR_b;
        break;
    default: ASSERTN(0, ("unsupport"));
    }
    return orty;
}


//[base + ofst] = store_val
void ARMCG::buildStore(IN SR * store_val, IN SR * base, IN SR * ofst,
                       bool is_signed, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(store_val && base && ofst);
    ASSERT0(ofst->is_int_imm() && (base->is_reg() || base->is_var()));
    ASSERTN(store_val->is_reg(), ("store_val can only be register on ARM"));
    ASSERT0(ofst->is_int_imm());
    Var const* v = nullptr;
    SR * sr_ofst = nullptr;
    if (base->is_var()) {
        SR * sr_base;
        v = SR_var(base);
        computeVarBaseAndOffset(SR_var(base), ofst->getInt(),
                                &sr_base, &sr_ofst);
        if (v->is_global() && !sr_base->is_reg()) {
            //ARM does not support load value from memory label directly.
            SR_var_ofst(base) += (UINT)ofst->getInt();
            SR * res = genReg();
            buildMove(res, base, ors, cont);

            if (sr_ofst->is_int_imm()) {
                SR_int_imm(sr_ofst) = SR_var_ofst(base);
            } else {
                ASSERT0(isComputeStackOffset());
                ASSERT0(sr_ofst->is_var());
            }

            base = res;
        } else {
            ASSERT0(sr_base->is_reg());
            base = sr_base;
        }
    } else {
        sr_ofst = ofst;
    }

    ASSERT0(sr_ofst);
    ASSERT0(cont != nullptr);
    if (IOC_mem_byte_size(cont) <= 4) {
        buildStoreCase1(store_val, base, sr_ofst, v, is_signed, ors, cont);
        return;
    }

    if (IOC_mem_byte_size(cont) <= 8) {
        buildStoreCase2(store_val, base, sr_ofst, v, is_signed, ors, cont);
        return;
    }

    UNREACHABLE();
}


//Build copy-operation with predicate register.
void ARMCG::buildCopyPred(CLUST clust,
                          UNIT unit,
                          SR * to,
                          SR * from,
                          SR * pd,
                          ORList & ors)
{
    ASSERT0(to && to->is_reg() && from && from->is_reg());
    buildCopy(clust, unit, to, from, ors);
    if (pd != nullptr) {
        OR * o = ors.get_head();
        ASSERT0(ors.get_elem_count() == 1 && isCopyOR(o) && pd->is_pred());
        o->set_pred(pd, this);
    }
}


//This is an util function.
//Build [tgt] <- [src] operation.
//tgt: target memory address register.
//src: source memory address register.
//bytesize: assigned bytesize that customized by caller.
void ARMCG::buildMemAssignBySize(SR * tgt,
                                 SR * src,
                                 UINT bytesize,
                                 OUT ORList & ors,
                                 MOD IOC * cont)
{
    ASSERT0(bytesize <= src->getByteSize());
    SR * loadval = nullptr;
    if (bytesize == 3) {
        IOC tc;
        //Bytesize of 1,2,4, has been handled, just leave 3 unsupport.
        //Current support for 3 byte loading is that load 4 bytes data,
        //then reserve the lower 3 bytes.
        ASSERT0(3 == GENERAL_REGISTER_SIZE - 1);
        IOC_mem_byte_size(&tc) = GENERAL_REGISTER_SIZE;
        //Regard memory assignment as unsigned operation.
        buildGeneralLoad(src, 0, false, ors, &tc);
        loadval = tc.getResult();
        ASSERT0(loadval && loadval->is_reg());

        tc.clean_bottomup();
        buildBinaryOR(IR_BAND, loadval,
                      genIntImm(getMaskByByte(bytesize), false),
                      false, ors, &tc);
        loadval = tc.getResult();
        ASSERT0(loadval && loadval->is_reg());
        bytesize = GENERAL_REGISTER_SIZE;
    } else {
        IOC tc;
        IOC_mem_byte_size(&tc) = bytesize;
        //Regard memory assignment as unsigned operation.
        buildGeneralLoad(src, 0, false, ors, &tc);
        loadval = tc.getResult();
    }
    ASSERT0(loadval && loadval->is_reg());
    IOC tc;
    IOC_mem_byte_size(&tc) = bytesize;
    buildStore(loadval, tgt, genZero(), false, ors, &tc);
}


//This is an util function.
//Build [tgt] <- [src] operation.
//tgt: target memory address register.
//src: source memory address register.
void ARMCG::buildMemAssign(SR * tgt,
                           SR * src,
                           OUT ORList & ors,
                           MOD IOC * cont)
{
    ASSERT0(tgt->getByteSize() == src->getByteSize());
    buildMemAssignBySize(tgt, src, src->getByteSize(), ors, cont);
}


//This is an util function.
//Build several [tgt] <- [src] operations accroding unrolling factor.
//e.g: given unrolling factor is 2, two memory assignments will be generated:
//     [tgt] <- [src];
//     src <- src + GENERAL_REGISTER_SIZE;
//     tgt <- tgt + GENERAL_REGISTER_SIZE;
//     [tgt] <- [src];
//tgt: target memory address register.
//src: source memory address register.
void ARMCG::buildMemAssignUnroll(SR * tgt,
                                 SR * src,
                                 UINT unroll_factor,
                                 OUT ORList & ors,
                                 MOD IOC * cont)
{
    SR * sz = genIntImm((HOST_INT)GENERAL_REGISTER_SIZE, true);
    IOC tc;
    for (UINT i = 0; i < unroll_factor; i++) {
        buildMemAssign(tgt, src, ors, cont);

        if (i == unroll_factor - 1) { continue; }

        //Increase the address of src pointer and tgt pointer.
        //src = src + size.
        tc.clean_bottomup();
        buildAdd(src, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
        ASSERT0(OR_code(ors.get_tail()) == OR_add ||
                OR_code(ors.get_tail()) == OR_add_i);
        ASSERT0(ors.get_tail()->get_result(0)->is_reg());
        ors.get_tail()->set_result(0, src, this);

        //tgt = tgt + size.
        tc.clean_bottomup();
        buildAdd(tgt, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
        ASSERT0(OR_code(ors.get_tail()) == OR_add ||
                OR_code(ors.get_tail()) == OR_add_i);
        ASSERT0(ors.get_tail()->get_result(0)->is_reg());
        ors.get_tail()->set_result(0, tgt, this);
    }
}


//This is an util function.
//Build several [tgt] <- [src] operations accroding unrolling factor.
//Generate loop to copy from src to tgt.
//e.g: given tgt address register, src address register, bytesize:
//  1. iv = bytesize
//  2. LOOP_START:
//  3. x = [src]
//  4. [tgt] = x
//  5. src = src + GENERAL_REGISTER_SIZE
//  6. tgt = tgt + GENERAL_REGISTER_SIZE
//  7. iv = iv - GENERAL_REGISTER_SIZE
//  8. teq_i, cpsr = iv, 0
//  9. b.ne, LOOP_START
//tgt: target memory address register.
//src: source memory address register.
void ARMCG::buildMemAssignLoop(SR * tgt,
                               SR * src,
                               UINT bytesize,
                               OUT ORList & ors,
                               MOD IOC * cont)
{
    IOC tc;
    tc.clean_bottomup();
    //1. iv = bytesize
    //Regard memory assignment as unsigned operation.
    buildGeneralLoad(genIntImm((HOST_INT)bytesize, true), 0, false, ors, &tc);
    SR * iv = tc.get_reg(0);
    ASSERT0(iv && iv->is_reg());

    //2. LOOP_START
    SR * loop_start_lab = genLabel(m_rg->genILabel());
    buildLabel(SR_label(loop_start_lab), ors, &tc);

    //3. x = [src]
    IOC_mem_byte_size(&tc) = src->getByteSize();
    tc.clean_bottomup();
    //Regard memory assignment as unsigned operation.
    buildGeneralLoad(src, 0, false, ors, &tc);

    SR * srt1 = tc.get_reg(0);
    ASSERT0(srt1 && srt1->is_reg());

    //4. [tgt] = x
    IOC_mem_byte_size(&tc) = src->getByteSize();
    buildStore(srt1, tgt, genZero(), false, ors, &tc);

    ASSERT0(GENERAL_REGISTER_SIZE == src->getByteSize());
    ASSERT0(GENERAL_REGISTER_SIZE == tgt->getByteSize());

    //5. src = src + GENERAL_REGISTER_SIZE
    tc.clean_bottomup();
    SR * sz = genIntImm((HOST_INT)GENERAL_REGISTER_SIZE, true);
    buildAdd(src, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(ors.get_tail()->get_result(0)->is_reg());
    ors.get_tail()->set_result(0, src, this);

    //6. tgt = tgt + GENERAL_REGISTER_SIZE
    tc.clean_bottomup();
    buildAdd(tgt, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(ors.get_tail()->get_result(0)->is_reg());
    ors.get_tail()->set_result(0, tgt, this);

    //Update loop_counter
    //7. iv = iv - GENERAL_REGISTER_SIZE
    //total_size = total_size - GENERAL_REGISTER_SIZE.
    tc.clean_bottomup();
    buildSub(iv, sz, GENERAL_REGISTER_SIZE, false, ors, &tc);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
    ASSERT0(ors.get_tail()->get_result(0)->is_reg());
    ors.get_tail()->set_result(0, iv, this);
    //buildMove(iv, tc.get_reg(0), ors, &tc);

    //8. teq_i, cpsr = iv, 0
    tc.clean_bottomup();
    ORList tors;
    buildCompare(OR_teq_i, true, iv, genZero(), tors, &tc);
    ASSERT0(tors.get_elem_count() == 1);
    ors.move_tail(tors);

    //9. b.ne, LOOP_START
    cont->set_pred(genNEPred());
    buildCondBr(loop_start_lab, ors, cont);
}


//Implement memory block copy.
//Note tgt and src should not overlap.
void ARMCG::buildMemcpyInternal(SR * tgt,
                                SR * src,
                                UINT bytesize,
                                OUT ORList & ors,
                                MOD IOC * cont)
{
    ASSERT0(src && src->is_reg());
    ASSERT0(tgt && tgt->is_reg());
    IOC tc;
    if (bytesize % GENERAL_REGISTER_SIZE != 0) {
        //Generate memory assignment for remainder data.
        UINT rest = bytesize % GENERAL_REGISTER_SIZE;
        buildMemAssignBySize(tgt, src, rest, ors, cont);

        //Increase source address pointer.
        //src = src + rest
        tc.clean_bottomup();
        buildIncReg(src, rest, ors, &tc);

        //Increase target address pointer.
        //tgt = tgt + rest
        tc.clean_bottomup();
        buildIncReg(tgt, rest, ors, &tc);

        bytesize -= rest;
    }

    if (bytesize == 0) { return; }

    if (bytesize <= GENERAL_REGISTER_SIZE * 2) {
        //Use SR move instead of block copy.
        ASSERT0(bytesize % GENERAL_REGISTER_SIZE == 0);
        buildMemAssignUnroll(tgt, src, bytesize / GENERAL_REGISTER_SIZE,
                             ors, cont);
    } else {
        buildMemAssignLoop(tgt, src, bytesize, ors, cont);
    }
}


//Increase 'reg' by 'val'.
void ARMCG::buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont)
{
    if (val == 0) { return; }
    CG::buildIncReg(reg, val, ors, cont);
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
}


//Decrease 'reg' by 'val'.
void ARMCG::buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont)
{
    if (val == 0) { return; }
    CG::buildDecReg(reg, val, ors, cont);
    //On ARM, decrement is also use ADD.
    ASSERT0(OR_code(ors.get_tail()) == OR_add ||
            OR_code(ors.get_tail()) == OR_add_i);
}

void ARMCG::buildCopy(CLUST clust,
                      UNIT unit,
                      SR * to,
                      SR * from,
                      ORList & ors)
{
    ASSERT0(to->is_reg() && from->is_reg());
    OR_TYPE ot = computeEquivalentORType(OR_mov, unit, clust);
    OR * o = genOR(ot);
    o->set_copy_to(to, this);
    o->set_copy_from(from, this);
    OR_clust(o) = clust;
    ors.append_tail(o);
}


#ifdef _DEBUG_
//Return true if val is 32bit integer more than 16bit.
//e.g: 0xFFFF is not more than 16bit.
//     0x1FFFF is more than 32bit.
static bool isInteger32bit(UINT64 val)
{
    DUMMYUSE(isInteger32bit);
    ASSERT0(sizeof(UINT64) >= sizeof(UINT32));
    return (((UINT32)val) & (UINT32)0x0000FFFF) != val;
}


//Return true if val is 64bit integer more than 32bit.
//e.g: 0xffffFFFF is not more than 32bit.
//     0x1ffffFFFF is more than 32bit.
static bool isInteger64bit(UINT64 val)
{
    DUMMYUSE(isInteger64bit);
    return (val & (UINT64)0xFFFFFFFF) != val;
}
#endif


void ARMCG::buildMove(SR * to, SR * from, OUT ORList & ors, MOD IOC *)
{
    ASSERT0(to->is_reg());
    ASSERT0(from->is_reg() || from->is_var() || from->is_int_imm() ||
            from->is_fp_imm());
    switch (SR_type(from)) {
    case SR_REG:
        buildCopy(CLUST_FIRST, UNIT_A, to, from, ors);
        return;
    case SR_INT_IMM: {
        OR * o = genOR(OR_mov32_i);
        o->set_mov_to(to, this);
        o->set_mov_from(genIntImm(from->getInt(), true), this);
        ors.append_tail(o);

        //Decompose OR_mov32_i into OR_movw_i and OR_movt_i after RA,
        //otherwise it might confuse ORBB localization pass.
        //OR * o = genOR(OR_movw_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genIntImm(
        //    (HOST_INT)(from->getInt() & 0xFFFF), true), this);
        //ors.append_tail(o);
        //if (isInteger32bit(from->getInt())) {
        //    o = genOR(OR_movt_i);
        //    o->set_mov_to(to);
        //    o->set_mov_from(genIntImm(
        //        (HOST_INT)((from->getInt() >> 16) & 0xFFFF), true), this);
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
        o->set_mov_to(to, this);
        o->set_mov_from(genVAR(SR_var(from)), this);
        ors.append_tail(o);

        //OR * o = genOR(OR_movw_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genVAR(SR_var(from)), this);
        //ors.append_tail(o);
        //o = genOR(OR_movt_i);
        //o->set_mov_to(to);
        //o->set_mov_from(genVAR(SR_var(from)), this);
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
    ASSERT0(spadj && spadj->isSpadjustImm());
    spadj->set_opnd(HAS_PREDICATE_REGISTER + SPADJUST_OFFSET_INDX,
                    genIntImm((HOST_INT)size, true), this);
}


//The function builds stack-pointer adjustment operation.
//Note XGEN supposed that the direction of stack-pointer is always
//decrement.
//bytesize: bytesize that needed to adjust, it can be immediate or register.
void ARMCG::buildAlloca(OUT ORList & ors, SR * bytesize, MOD IOC * cont)
{
    OR * o;
    if (bytesize->is_imm()) {
        o = genOR(OR_spadjust_i);
    } else {
        ASSERT0(bytesize->is_reg());
        o = genOR(OR_spadjust_r);
    }
    ASSERT0(o->is_fake());
    o->set_result(0, getSP(), this);
    o->set_result(1, genR12(), this);
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, getSP(), this);
    ASSERT0(cont);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1, bytesize, this);
    ors.append_tail(o);
    cont->set_reg(0, getSP());
}


//Generate sp adjust operation.
void ARMCG::buildSpadjust(OUT ORList & ors, MOD IOC * cont)
{
    OR * o = genOR(OR_spadjust_i);
    ASSERT0(o->is_fake());
    o->set_result(0, getSP(), this);
    o->set_result(1, genR12(), this);
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, getSP(), this);
    ASSERT0(cont);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1,
                genIntImm((HOST_INT)IOC_int_imm(cont), true), this);
    ors.append_tail(o);
    cont->set_reg(0, getSP());
}


void ARMCG::buildICall(SR * callee, UINT ret_val_size,
                       OUT ORList & ors, IOC * cont)
{
    ASSERT0(callee && callee->is_reg());
    //Function-Call will violate SP,FP,GP, RFLAG register,
    //return-value register, return address register.
    OR * o = buildOR(OR_blx, 1, 2, genReturnAddr(), getTruePred(), callee);
    ors.append_tail(o);

    ASSERT0(cont != nullptr);
    if (ret_val_size <= GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, genR0());
        return;
    }
    if (ret_val_size <= GENERAL_REGISTER_SIZE * 2) {
        SR * sr = getSRVecMgr()->genSRVec(2,
            genRegWithPhyReg(REG_R0, RF_R),
            genRegWithPhyReg(REG_R1, RF_R));
        cont->set_reg(RESULT_REGISTER_INDEX, sr);
        return;
    }
    ASSERTN(0, ("TODO"));    
}


void ARMCG::buildCall(Var const* callee, UINT ret_val_size, OUT ORList & ors,
                      IOC * cont)
{
    //Function-Call will violate SP,FP,GP, RFLAG register, return-value
    //register, return address register.
    OR * o = buildOR(OR_bl, 1, 2, genReturnAddr(), getTruePred(),
                     genVAR(callee));
    ors.append_tail(o);

    ASSERT0(cont != nullptr);
    if (ret_val_size <= GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, genR0());
        return;
    }
    if (ret_val_size <= GENERAL_REGISTER_SIZE * 2) {
        SR * sr = getSRVecMgr()->genSRVec(2,
            genRegWithPhyReg(REG_R0, RF_R),
            genRegWithPhyReg(REG_R1, RF_R));
        cont->set_reg(RESULT_REGISTER_INDEX, sr);
        return;
    }
    //Return value will be stored in 'retval_buf_of_XXX'.
    cont->set_reg(RESULT_REGISTER_INDEX, nullptr);    
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
        CLUST clust, OUT List<REGFILE> & regfiles) const
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
    xgen::UnitSet const& us,
    OUT List<REGFILE> & regfiles) const
{
    regfiles.append_tail(RF_R);
    return regfiles;
}


//Mapping from single unit to its corresponed cluster.
SLOT ARMCG::mapUnit2Slot(UNIT unit, CLUST clst) const
{
    switch (unit) {
    case UNIT_A: return SLOT_G;
    default: UNREACHABLE();
    }
    return SLOT_NUM;
}


//Mapping from single issue slot(for multi-issue architecture) to
//its corresponed function unit.
UNIT ARMCG::mapSlot2Unit(SLOT slot) const
{
    switch (slot) {
    case SLOT_G: return UNIT_A;
    default: UNREACHABLE();
    }
    return UNIT_UNDEF;
}


//Return the cluster which owns 'regfile'.
CLUST ARMCG::mapRegFile2Cluster(REGFILE regfile, SR const*) const
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
CLUST ARMCG::mapReg2Cluster(REG reg) const
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
UnitSet & ARMCG::mapRegFile2UnitSet(REGFILE regfile,
                                    SR const* sr,
                                    UnitSet & us) const
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
CLUST ARMCG::mapSR2Cluster(OR const* o, SR const* sr) const
{
    if (!sr->is_reg() || sr->getRegFile() == RF_UNDEF) {
        return CLUST_UNDEF;
    }
    if (sr->getPhyReg() != REG_UNDEF) {
        return mapReg2Cluster(sr->getPhyReg());
    }
    if (sr->getRegFile() != RF_UNDEF) {
        return mapRegFile2Cluster(sr->getRegFile(), sr);
    }

    CLUST clust = CLUST_UNDEF;
    switch (o->getCode()) {
    case OR_spadjust_i:
    case OR_spadjust_r:
        //Disable ASSERT0(sr == getSP() || sr == getGP() || sr == getFP());
        //Because alloca generate spadjust operation, such as:
        //    foo (alloca ((int)&main));
        //and generated OR are:
        //    t233 :- mov_i t97(p0) (sym:main+0)
        //    t234 :- add_i t97(p0) t233 (0x3)
        //    t235[A1] :- and_i t97(p0) TN234 (0xfffffffc)
        //    t49(sp) :- spadjust t97(p0) t235[A1] ; spadjust_minus
        //Here t235 is not a getSP().
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
    ASSERT0(unit == UNIT_A);
    OR * nop = buildOR(OR_nop, 0, 1, getTruePred());
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
bool ARMCG::changeORType(MOD OR * o, OR_TYPE ortype, CLUST src,
                         CLUST tgt, RegFileSet const* regfile_unique)
{
    DUMMYUSE(src);
    ASSERTN(tgt != CLUST_UNDEF, ("need cluster info"));
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (!sr->is_reg() ||
            sr->is_pred() ||
            SR_is_dedicated(sr)) {
            continue;
        }

        //Handle general sr.
        if (sr->getPhyReg() != REG_UNDEF) {
            ASSERTN(sr->getRegFile() != RF_UNDEF, ("Loss regfile info"));
            ASSERTN(tgt == mapReg2Cluster(sr->getPhyReg()), ("Unmatch info"));
        } else {
            //ASSERTN(sr->getPhyReg() == REG_UNDEF &&
            //        sr->getRegFile() == RF_UNDEF, ("sr has allocated"));
            if (regfile_unique == nullptr ||
                !regfile_unique->is_contain(SR_sregid(sr))) {
                //Reassign regfile.
                SR_phy_reg(sr) = REG_UNDEF;
                SR_regfile(sr) = RF_UNDEF;
            }
        }
    }

    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (!sr->is_reg() ||
            sr->is_pred() ||
            SR_is_dedicated(sr)) {
            continue;
        }

        if (sr->getPhyReg() != REG_UNDEF) {
            ASSERTN(sr->getRegFile() != RF_UNDEF, ("Loss regfile info"));
            //When 'sr' has been assigned register, the 'tgt' cluster
            //must be as same as the cluster which 'sr' correlated to.
            //ASSERTN(tgt == mapReg2Cluster(
            //        TN_register_class(sr), sr->getPhyReg()),
            //        ("Unmatch info"));
            if (tgt != mapReg2Cluster(sr->getPhyReg())) {
                return false;
            }
        } else {
            if (regfile_unique == nullptr ||
                !regfile_unique->is_contain(SR_sregid(sr))) {
                //ASSERTN(sr->getPhyReg() == REG_UNDEF &&
                //        sr->getRegFile() == RF_UNDEF, ("sr has allocated"));
                SR_phy_reg(sr) = REG_UNDEF;
                SR_regfile(sr) = RF_UNDEF;
            }
        }
    }
    OR_code(o) = ortype;
    return true;
}


void ARMCG::buildShiftLeftReg(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                              OUT ORList & ors, MOD IOC * cont)
{
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
    SR * hi = src->getVec()->get(1);
    SR * lo = src->getVec()->get(0);
    ASSERT0(hi && lo);
    SR * res_lo = genReg();
    SR * res_hi = genReg();
    getSRVecMgr()->genSRVec(2, res_lo, res_hi);

    //    rsb r6, j, #32
    SR * r6 = genReg();
    OR * o = buildOR(OR_rsb_i, 1, 3, r6,
                     getTruePred(), shift_ofst, genIntImm(32, false));
    ors.append_tail(o);

    //    sub r1, j, #32
    SR * r1 = genReg();
    o = buildOR(OR_sub_i, 1, 3, r1,
                getTruePred(), shift_ofst, genIntImm(32, false));
    ors.append_tail(o);

    //    lsl r5, a_hi, j
    SR * r5 = genReg();
    o = buildOR(OR_lsl, 1, 3, r5,
                getTruePred(), hi, shift_ofst);
    ors.append_tail(o);

    //    lsr r6, a_lo, r6
    o = buildOR(OR_lsr, 1, 3, r6,
                getTruePred(), lo, r6);
    ors.append_tail(o);

    //    lsl r4, a_lo, r1
    SR * r4 = genReg();
    o = buildOR(OR_lsl, 1, 3, r4,
                getTruePred(), lo, r1);
    ors.append_tail(o);

    //    orrs r5, r5, r6
    o = buildOR(OR_orrs, 2, 3, r5, getRflag(),
                getTruePred(), r5, r6);
    ors.append_tail(o);

    //    lsl res_lo, a_lo, j
    o = buildOR(OR_lsl, 1, 3, res_lo,
                getTruePred(), lo, shift_ofst);
    ors.append_tail(o);

    //    ands res_hi, r5, r1, asr #32
    o = buildOR(OR_ands_asr_i, 2, 4, res_hi, getRflag(),
                getTruePred(), r5, r1, genIntImm(32, false));
    ors.append_tail(o);

    //    movcc res_hi, r4
    o = buildOR(OR_mov, 1, 2, res_hi, genCCPred(), r4);
    ors.append_tail(o);

    //    strd res_lo, res_hi ->[b_lo, b_hi]
    ASSERT0(cont);
    cont->set_reg(0, res_lo);

}


void ARMCG::buildShiftLeftImm(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                              OUT ORList & ors, MOD IOC * cont)
{
    if (shift_ofst->getInt() <= 31) {
        //hi <- hi << ofst
        //t <- lo << (32 - ofst)
        //hi <- hi | t
        //lo <- lo << ofst
        SR * hi = src->getVec()->get(1);
        SR * lo = src->getVec()->get(0);
        ASSERT0(hi && lo);
        OR * o = buildOR(OR_lsl_i, 1, 3, hi, getTruePred(),
                         hi, shift_ofst);
        ors.append_tail(o);
        o = buildOR(OR_orr_lsr_i, 1, 4, hi, getTruePred(),
                    hi, lo, genIntImm(32 - shift_ofst->getInt(), false));
        ors.append_tail(o);
        o = buildOR(OR_lsl_i, 1, 3, lo, getTruePred(),
                    lo, shift_ofst);
        ors.append_tail(o);
        ASSERT0(cont);
        cont->set_reg(0, lo);
        //cont->set_reg(0, hi);
        return;
    }

    if (shift_ofst->getInt() <= 63) {
        //hi <- lo << (ofst - 32)
        //lo <- 0
        SR * hi = src->getVec()->get(1);
        SR * lo = src->getVec()->get(0);
        ASSERT0(hi && lo);
        OR * o = buildOR(OR_lsl_i, 1, 3, hi, getTruePred(),
                         lo, genIntImm(shift_ofst->getInt() - 32, false));
        ors.append_tail(o);
        o = buildOR(OR_mov_i, 1, 2, lo, getTruePred(), genZero(), true);
        ors.append_tail(o);
        ASSERT0(cont);
        cont->set_reg(0, lo);
        return;
    }

    SR * hi = src->getVec()->get(1);
    SR * lo = src->getVec()->get(0);
    ASSERT0(hi && lo);
    // hi <- 0
    OR * set_high = buildOR(OR_mov_i, 1, 2, hi, getTruePred(),
                            genIntImm(0, true));
    // lo <- 0
    OR * set_low = buildOR(OR_mov_i, 1, 2, lo, getTruePred(),
                           genIntImm(0, true));
    ASSERT0(set_high && set_low);
    ors.append_tail(set_high);
    ors.append_tail(set_low);
    ASSERT0(cont);
    cont->set_reg(0, hi);
    //cont->set_reg(1, hi);
}


void ARMCG::buildShiftLeft(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                           OUT ORList & ors, MOD IOC * cont)
{
    if (sr_size <= 4) {
        //There is no different between signed and unsigned left shift.
        SR * res = genReg();
        OR_TYPE ort = OR_UNDEF;
        if (shift_ofst->is_reg()) {
            ort = OR_lsl;
        } else if (shift_ofst->is_imm()) {
            ort = OR_lsl_i32;
        } else {
            UNREACHABLE();
        }
        OR * o = buildOR(ort, 1, 3, res, getTruePred(), src, shift_ofst);
        ors.append_tail(o);
        ASSERT0(cont);
        cont->set_reg(0, res);
        return;
    }

    ASSERTN(sr_size <= 8, ("TODO"));
    if (shift_ofst->is_reg()) {
        buildShiftLeftReg(src, sr_size, shift_ofst, ors, cont);
        return;
    }

    if (shift_ofst->is_imm()) {
        buildShiftLeftImm(src, sr_size, shift_ofst, ors, cont);
        return;
    }

    UNREACHABLE();
}


void ARMCG::buildShiftRightCase1(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                                 bool is_signed, OUT ORList & ors,
                                 MOD IOC * cont)
{
    SR * res = genReg();
    OR_TYPE ort = OR_UNDEF;
    if (is_signed) {
        if (shift_ofst->is_reg()) { ort = OR_asr; }
        else if (shift_ofst->is_imm()) { ort = OR_asr_i32; }
        else { UNREACHABLE(); }
    } else {
        if (shift_ofst->is_reg()) { ort = OR_lsr; }
        else if (shift_ofst->is_imm()) { ort = OR_lsr_i32; }
        else { UNREACHABLE(); }
    }
    OR * o = buildOR(ort, 1, 3, res, getTruePred(), src, shift_ofst);
    ors.append_tail(o);
    ASSERT0(cont);
    cont->set_reg(0, res);
}


void ARMCG::buildShiftRightCase2(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                                 bool is_signed, OUT ORList & ors,
                                 MOD IOC * cont)
{
    //e.g:long long res;
    //    long long src;
    //    int shift_ofst;
    //    void foo() {
    //        res = src >> shift_ofst;
    //    }
    //
    //Algo:
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

    SR * src_hi = src->getVec()->get(1);
    SR * src_lo = src->getVec()->get(0);
    ASSERT0(src_hi && src_lo);
    SR * res_lo = genReg();
    SR * res_hi = genReg();
    getSRVecMgr()->genSRVec(2, res_lo, res_hi);

    //    sub r5, shift_ofst, #32
    SR * r5 = genReg();
    OR * o = buildOR(OR_sub_i, 1, 3, r5,
                     getTruePred(), shift_ofst, genIntImm(32, false));
    ors.append_tail(o);

    //    asr/lsr r6, src_hi, r5
    SR * r6 = genReg();
    o = buildOR(ort, 1, 3, r6, getTruePred(), src_hi, r5);
    ors.append_tail(o);

    //    rsb ip, shift_ofst, #32
    SR * ip = genReg();
    o = buildOR(OR_rsb_i, 1, 3, ip,
                getTruePred(), shift_ofst, genIntImm(32, false));
    ors.append_tail(o);

    //    lsl ip, src_hi, ip
    o = buildOR(OR_lsl, 1, 3, ip, getTruePred(), src_hi, ip);
    ors.append_tail(o);

    //    lsr r2, src_lo, shift_ofst
    SR * r2 = genReg();
    o = buildOR(OR_lsr, 1, 3, r2, getTruePred(), src_lo, shift_ofst);
    ors.append_tail(o);

    //    orr res_lo, ip, r2
    o = buildOR(OR_orr, 1, 3, res_lo, getTruePred(), ip, r2);
    ors.append_tail(o);

    //    cmp r5, #0
    o = buildOR(OR_cmp_i, 1, 3, getRflag(),
                getTruePred(), r5, genIntImm(0, false));
    ors.append_tail(o);

    //    mov_ge  res_lo, r6
    o = buildOR(OR_mov, 1, 2, res_lo, genGEPred(), r6);
    ors.append_tail(o);

    //    asr/lsr res_hi, src_hi, shift_ofst
    o = buildOR(ort, 1, 3, res_hi, getTruePred(), src_hi, shift_ofst);
    ors.append_tail(o);

    //    strd res_lo, res_hi ->[b_lo, b_hi]
    ASSERT0(cont);
    cont->set_reg(0, res_lo);
    return;
}


void ARMCG::buildShiftRightCase3_1(IN SR * src,
                                   ULONG sr_size,
                                   IN SR * shift_ofst,
                                   bool is_signed,
                                   OUT ORList & ors,
                                   MOD IOC * cont)
{
    //Algo:
    //lo = lo >>(lsr) shift_ofst
    //lo = lo | (hi <<(lsl) (32 - shift_ofst))
    //hi = hi >> shift_ofst

    SR * hi = src->getVec()->get(1);
    SR * lo = src->getVec()->get(0);
    ASSERT0(hi && lo);

    //NOTE: Need LOGICAL shift to reserve space to
    //hold residual part of high-part.
    //DO NOT USE ARITH-SHIFT.
    //lo = lo >>(lsr) shift_ofst
    OR * o = buildOR(OR_lsr_i, 1, 3, lo, getTruePred(), lo, shift_ofst);
    ors.append_tail(o);

    //lo = lo | (hi <<(lsl) (32 - shift_ofst))
    o = buildOR(OR_orr_lsl_i, 1, 4, lo,
                getTruePred(),
                lo, hi, genIntImm(32 - shift_ofst->getInt(), false));
    ors.append_tail(o);

    //hi = hi >> shift_ofst
    OR_TYPE ort = OR_lsr_i;
    if (is_signed) {
        ort = OR_asr_i;
    } else {
        ort = OR_lsr_i;
    }
    o = buildOR(ort, 1, 3, hi, getTruePred(), hi, shift_ofst);
    ors.append_tail(o);

    ASSERT0(cont);
    cont->set_reg(0, lo);
    //cont->set_reg(1, hi);
}


void ARMCG::buildShiftRightCase3_2(IN SR * src,
                                   ULONG sr_size,
                                   IN SR * shift_ofst,
                                   bool is_signed,
                                   OUT ORList & ors,
                                   MOD IOC * cont)
{
    //Algo:
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

    SR * hi = src->getVec()->get(1);
    SR * lo = src->getVec()->get(0);
    ASSERT0(hi && lo);
    //Do we need asr_i here?
    //lo <- hi asr (ofst - 32)
    OR * set_low = buildOR(ort, 1, 3, lo,
                           getTruePred(), hi,
                           genIntImm(shift_ofst->getInt() - 32, true));
    ors.append_tail(set_low);

    OR * set_high = nullptr;
    if (is_signed) {
        //DO WE NEED ASR_I HERE ??
        //    hi <- hi asr shift_ofst
        set_high = buildOR(OR_asr_i, 1, 3, hi,
                           getTruePred(), hi, genIntImm(31, true));
    } else {
        //    hi <- 0
        set_high = buildOR(OR_mov_i, 1, 2, hi,
                           getTruePred(), genIntImm(0, true));
    }
    ors.append_tail(set_high);
    ASSERT0(cont);
    cont->set_reg(0, hi);
    //cont->set_reg(1, hi);
}


void ARMCG::buildShiftRightCase3(IN SR * src,
                                 ULONG sr_size,
                                 IN SR * shift_ofst,
                                 bool is_signed,
                                 OUT ORList & ors,
                                 MOD IOC * cont)
{
    if (shift_ofst->getInt() <= 31) {
        buildShiftRightCase3_1(src, sr_size, shift_ofst, is_signed, ors, cont);
        return;
    }

    if (shift_ofst->getInt() <= 63) {
        buildShiftRightCase3_2(src, sr_size, shift_ofst, is_signed, ors, cont);
        return;
    }

    //Algo:
    //if (is_signed) {
    //    lo <- hi asr 31
    //    hi <- hi asr 31
    //} else {
    //    lo <- 0
    //    hi <- 0
    //}

    SR * hi = src->getVec()->get(1);
    SR * lo = src->getVec()->get(0);
    ASSERT0(hi && lo);
    OR * set_high = nullptr;
    OR * set_low = nullptr;
    if (is_signed) {
        //Do we need asrs_i here?
        //    lo <- hi asr 31
        set_high = buildOR(OR_asr_i, 1, 3, lo,
                           getTruePred(), hi, genIntImm(31, true));
        //    hi <- hi asr 31
        set_low = buildOR(OR_asr_i, 1, 3, hi,
                          getTruePred(), hi, genIntImm(31, true));
    } else {
        //    hi <- 0
        set_high = buildOR(OR_mov_i, 1, 2, hi,
                           getTruePred(), genIntImm(0, true));
        //    lo <- 0
        set_low = buildOR(OR_mov_i, 1, 2, lo,
                          getTruePred(), genIntImm(0, true));
    }
    ASSERT0(set_high && set_low);
    ors.append_tail(set_high);
    ors.append_tail(set_low);
    ASSERT0(cont);
    cont->set_reg(0, hi);
    //cont->set_reg(1, hi);
}


void ARMCG::buildShiftRight(IN SR * src,
                            ULONG sr_size,
                            IN SR * shift_ofst,
                            bool is_signed,
                            OUT ORList & ors,
                            MOD IOC * cont)
{
    if (sr_size <= 4) {
        buildShiftRightCase1(src, sr_size, shift_ofst, is_signed, ors, cont);
        return;
    }

    ASSERTN(sr_size <= 8, ("TODO"));
    if (shift_ofst->is_reg()) {
        buildShiftRightCase2(src, sr_size, shift_ofst, is_signed, ors, cont);
        return;
    }
    if (shift_ofst->is_imm()) {
        ASSERTN(shift_ofst->getInt() != 0, ("a >> 0, redundant operation"));
        buildShiftRightCase3(src, sr_size, shift_ofst, is_signed, ors, cont);
        return;
    }
    UNREACHABLE();
}


//Generate inter-cluster copy operation.
//Copy from 'src' in 'src_clust' to 'tgt' of in 'tgt_clust'.
OR * ARMCG::buildBusCopy(IN SR * src,
                         IN SR * tgt,
                         IN SR *, //predicate register.
                         CLUST src_clust,
                         CLUST tgt_clust)
{
    ASSERTN(0, ("Unsupport"));
    return nullptr;
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildAddRegImm(SR * src, SR * imm,  UINT sr_size,
                           bool is_sign, OUT ORList & ors, MOD IOC * cont)
{
    DUMMYUSE(is_sign);
    ASSERT0(src->is_reg());
    ASSERT0(imm->is_int_imm() ||
            (!isComputeStackOffset() || imm->is_var()));
    if (sr_size <= 4) { //< 4bytes
        if (isValidImmOpnd(OR_add_i, imm->is_int_imm())) {
            SR * res = genReg();
            OR * o = buildOR(OR_add_i, 1, 3, res,
                             getTruePred(), src, imm);
            cont->set_reg(0, res);
            ors.append_tail(o);
            return;
        }

        SR * t = genReg();
        buildMove(t, genIntImm((HOST_INT)(imm->getInt()), true), ors, nullptr);

        SR * res = genReg();
        OR * o = buildOR(OR_add, 1, 3, res,
                         getTruePred(), src, t);
        cont->set_reg(0, res);
        ors.append_tail(o);
        return;
    }

    if (sr_size <= 8) {
        SRVec * sv = src->getVec();
        ASSERT0(sv != nullptr && SR_vec_idx(src) == 0);

        //load low 32bit imm
        SR * t = genReg();
        buildMove(t, genIntImm((HOST_INT)(imm->getInt() & 0xFFFFFFFF), true),
                  ors, nullptr);
        SR * t2 = genReg();
        buildMove(t2, genIntImm((HOST_INT)
                                ((imm->getInt() >> 32) & 0xFFFFFFFF), true),
                  ors, nullptr);

        SR * res = genReg();
        SR * res2 = genReg();
        getSRVecMgr()->genSRVec(2, res, res2);
        ASSERT0(res == res->getVec()->get(0) && res2 == res->getVec()->get(1));

        OR * o = buildOR(OR_adds, 2, 3, res, getRflag(),
                         getTruePred(), src, t);
        ors.append_tail(o);

        SR * src2 = sv->get(1);
        ASSERT0(src2);
        o = buildOR(OR_adc, 1, 4, res2,
                    getTruePred(), src2, t2, getRflag());
        ors.append_tail(o);

        ASSERT0(cont);
        cont->set_reg(0, res);
        //cont->set_reg(1, res2);
        return;
    }

    ASSERTN(0, ("TODO"));
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildMul(SR * src1,
                     SR * src2,
                     UINT sr_size,
                     bool is_sign,
                     OUT ORList & ors,
                     MOD IOC * cont)
{
    if (src1->is_int_imm() && src2->is_int_imm()) {
        SR * result = genIntImm((HOST_INT)
                                (src1->getInt() * src2->getInt()), true);
        ASSERT0(cont);
        cont->set_reg(0, result);
        return;
    }

    if (src2->is_reg() && src2->is_reg()) {
        buildMulRegReg(src1, src2, sr_size, is_sign, ors, cont);
        return;
    }

    UNREACHABLE();
}


//'sr_size': The number of byte-size of SR.
void ARMCG::buildMulRegReg(SR * src1,
                           SR * src2,
                           UINT sr_size,
                           bool is_sign,
                           OUT ORList & ors,
                           MOD IOC * cont)
{
    ASSERT0(src1->is_reg() && src2->is_reg());
    if (sr_size <= 4) { //< 4bytes
        DUMMYUSE(is_sign);
        SR * res = genReg();
        //Both signed and unsigned uses the opcode.
        OR * o = buildOR(OR_mul, 1, 3, res, getTruePred(), src1, src2);
        cont->set_reg(0, res);
        ors.append_tail(o);
        return;
    }

    UNREACHABLE();
}


void ARMCG::buildAddRegReg(bool is_add, SR * src1, SR * src2, UINT sr_size,
                           bool is_sign, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(src1->is_reg() && src2->is_reg());
    if (sr_size <= 4) { //< 4bytes
        OR_TYPE orty = OR_UNDEF;
        if (is_add) {
            orty = OR_add;
        } else {
            orty = OR_sub;
        }
        SR * res = genReg();
        OR * o = buildOR(orty, 1, 3, res, getTruePred(), src1, src2);
        cont->set_reg(0, res);
        ors.append_tail(o);
        return;
    }

    if (sr_size <= 8) {
        SRVec * sv1 = src1->getVec();
        ASSERT0(sv1 && SR_vec_idx(src1) == 0);
        SR * src1_2 = sv1->get(1);
        ASSERT0(src1_2 != nullptr && SR_vec_idx(src1_2) == 1);

        SRVec * sv2 = src2->getVec();
        ASSERT0(sv2 && SR_vec_idx(src2) == 0);
        SR * src2_2 = sv2->get(1);
        ASSERT0(src2_2 && SR_vec_idx(src2_2) == 1);

        SR * res = getSRVecMgr()->genSRVec(2, genReg(), genReg());
        SR * res_2 = res->getVec()->get(1);

        OR_TYPE orty = OR_UNDEF;
        OR_TYPE orty2 = OR_UNDEF;
        List<SR*> reslst;
        List<SR*> opndlst;
        if (is_add) {
            orty = OR_adds;
            orty2 = OR_adc;
            reslst.append_tail(res_2);
            opndlst.append_tail(getTruePred());
            opndlst.append_tail(src1_2);
            opndlst.append_tail(src2_2);
            opndlst.append_tail(getRflag());
            ASSERT0(::tmGetResultNum(orty) == 2);
            ASSERT0(::tmGetOpndNum(orty) == 3);
            ASSERT0(::tmGetResultNum(orty2) == 1);
            ASSERT0(::tmGetOpndNum(orty2) == 4);
        } else {
            orty = OR_subs;
            orty2 = OR_sbc;
            reslst.append_tail(res_2);
            opndlst.append_tail(getTruePred());
            opndlst.append_tail(src1_2);
            opndlst.append_tail(src2_2);
            opndlst.append_tail(getRflag());
            ASSERT0(::tmGetResultNum(orty) == 2);
            ASSERT0(::tmGetOpndNum(orty) == 3);
            ASSERT0(::tmGetResultNum(orty2) == 1);
            ASSERT0(::tmGetOpndNum(orty2) == 4);
        }

        OR * o1 = buildOR(orty, 2, 3, res, getRflag(),
            getTruePred(), src1, src2);
        ors.append_tail(o1);

        OR * o2 = buildOR(orty2, reslst, opndlst);
        ors.append_tail(o2);

        if (!is_add) {
            //TBD: What is the purpose of following code?
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
            //ASSERT0(src1->is_reg() && src2->is_reg());
            ////Build: al < bl
            //buildARMCmp(OR_cmp, getTruePred(),
            //    src1, src2, ors, cont);
            //
            ////Build: ch = ch - 1 to compute high-part 32bit value.
            //ORList tors;
            //IOC tmp;
            //buildSub(res_2, genIntImm((HOST_INT)1, false),
            //         GENERAL_REGISTER_SIZE, is_sign, tors, &tmp);
            //ASSERT0(tors.get_elem_count() == 1);
            //res_2 = tmp.get_reg(0); //update the high-part SR.
            //tors.get_head()->set_pred(genLTPred());
            //ors.append_tail(tors);
        }

        ASSERT0(cont);
        //Record the result, also the first element of SRVector.
        cont->set_reg(0, res);
        //cont->set_reg(1, res_2); //no need to record second part.
        assembleSRVec(res->getVec(), res, res_2);
        return;
    }

    ASSERTN(0, ("TODO"));
}


void ARMCG::buildARMCmp(OR_TYPE cmp_ot,
                        IN SR * pred,
                        IN SR * opnd0,
                        IN SR * opnd1,
                        OUT ORList & ors,
                        MOD IOC *)
{
    ors.append_tail(buildOR(cmp_ot, 1, 3, getRflag(),
                            pred, opnd0, opnd1));
}


void ARMCG::buildUncondBr(IN SR * tgt_lab, OUT ORList & ors, MOD IOC *)
{
    ASSERT0(tgt_lab && tgt_lab->is_label());
    //OR_b with TruePred is unconditional branch.
    //ASSERT0(OTD_is_uncond_br(tmGetORTypeDesc(OR_b)));
    OR * o = genOR(OR_b);
    o->setLabel(tgt_lab, this);
    o->set_pred(getTruePred(), this);
    ASSERT0(isValidOpndRegfile(OR_b, HAS_PREDICATE_REGISTER + 1,
                               getRflagRegfile()));
    o->set_opnd(HAS_PREDICATE_REGISTER + 1, getRflag(), this);
    ors.append_tail(o);
}


void ARMCG::buildCompare(OR_TYPE br_cond,
                         bool is_truebr,
                         IN SR * opnd0,
                         IN SR * opnd1,
                         OUT ORList & ors,
                         MOD IOC * cont)
{
    ASSERT0(opnd0 && opnd1);
    buildARMCmp(br_cond, getTruePred(), opnd0, opnd1, ors, cont);
    ASSERT0(cont);

    //ARM cmp operation does not produce result in register.
    cont->set_reg(RESULT_REGISTER_INDEX, nullptr); //record compare result
}


//Build conditional branch operation.
//cont: record the predicated register to
//      condition the execution of Branch operation.
void ARMCG::buildCondBr(IN SR * tgt_lab, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(tgt_lab && tgt_lab->is_label());
    OR * o = genOR(OR_b); //conditional relative offset branch.
    SR * pred = cont->get_pred();
    ASSERT0(pred && pred->is_pred());
    o->set_pred(pred, this);
    o->setLabel(tgt_lab, this);
    ASSERT0(isValidOpndRegfile(OR_b, HAS_PREDICATE_REGISTER + 1,
                               getRflagRegfile()));
    o->set_opnd(HAS_PREDICATE_REGISTER + 1, getRflag(), this);
    ors.append_tail(o);
}


//Build memory store operation that store 'reg' into stack.
//NOTE: user have to assign physical register manually if there is
//new OR generated and need register allocation.
void ARMCG::buildStoreAndAssignRegister(SR * reg, UINT offset,
                                        ORList & ors, IOC * cont)
{
    SR * sr_offset = genIntImm((HOST_INT)offset, false);
    ORList tors;
    buildStore(reg, getSP(), sr_offset, false, tors, cont);
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
        ASSERT0(res && SR_phy_reg(res) == REG_UNDEF);

        //Use scratch physical register.
        renameResult(add, res, genR12(), false);
        renameOpnd(st, res, genR12(), false);
        break;
    }
    default: UNREACHABLE();
    }
    ors.move_tail(tors);
}


bool ARMCG::isValidResultRegfile(OR_TYPE ortype,
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
bool ARMCG::isValidOpndRegfile(OR_TYPE ortype,
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
bool ARMCG::isValidRegInSRVec(OR const*, SR const* sr,
                              UINT idx, bool is_result) const
{
    DUMMYUSE(is_result);
    CHECK0_DUMMYUSE(sr);
    ASSERT0(sr->getVec());
    if (idx == 0) {
        ASSERTN(isEvenReg(sr->getPhyReg()),
                ("Must be even number register."));
    } else if (idx == 1) {
        ASSERTN(!isEvenReg(sr->getPhyReg()),
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
bool ARMCG::isSPUnit(UNIT unit) const
{
    return UNIT_A == unit;
}


//Return true if 'sr' can be allocated with integer register.
//'idx': idx of 'sr' in operand list of 'o'.
//'is_result': true if 'sr' is result of 'o', otherwise be false.
bool ARMCG::isIntRegSR(OR const* o, SR const* sr,
                       UINT idx, bool is_result) const
{
    if (!sr->is_reg()) { return false; }
    if (sr->getRegFile() != RF_UNDEF) {
        return tmIsIntRegFile(sr->getRegFile());
    }
    ASSERT0(o);
    RegFileSet const* rfset = getValidRegfileSet(o->getCode(), idx, is_result);
    if (rfset->is_contain(RF_R)) {
        return true;
    }
    return false;
}


bool ARMCG::isBusCluster(CLUST clust) const
{
    return false;
}


//SR that can used on each clusters.
//    predicate,
//    rflags,
//    system,
//    cpointer,
bool ARMCG::isBusSR(SR const* sr) const
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
bool ARMCG::isRecalcOR(OR const* o) const
{
    switch (o->getCode()) {
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
            if (opnd->is_reg() && !SR_is_dedicated(opnd)) {
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
            if (opnd->is_reg() && !isDedicatedSR(opnd)) {
                return false;
            }
        }
        return true;
    }
    case OR_spadjust_i:
    case OR_spadjust_r:
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
bool ARMCG::isSameLikeCluster(SLOT slot1, SLOT slot2) const
{
    return slot1 == SLOT_G && slot2 == SLOT_G;
}


//Return true if the data BUS operation processed that was
//using by other general operations.
bool ARMCG::isSameLikeCluster(OR const* or1, OR const* or2) const
{
    return true;
}


//Return true if or-type 'ortype' has the number of 'res_num' results.
bool ARMCG::isMultiResultOR(OR_TYPE ortype, UINT res_num) const
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
bool ARMCG::isMultiResultOR(OR_TYPE ortype) const
{
    return isMultiResultOR(ortype, 2);
}


bool ARMCG::isMultiStore(OR_TYPE ortype, INT opnd_num) const
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


bool ARMCG::isMultiLoad(OR_TYPE ortype, INT res_num) const
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


bool ARMCG::isCopyOR(OR const* o) const
{
    switch (o->getCode()) {
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


bool ARMCG::isStackPointerValueEqu(SR const* base1, SR const* base2) const
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
bool ARMCG::isReduction(OR const* o) const
{
    if (isCondExecOR(o)) { return false; }

    INT reduct_opnd = 0;
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (!opnd->is_reg() ||
            opnd == getTruePred() ||
            !opnd->is_global()) {
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
    switch (o->getCode()) {
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
    OR_TYPE opr = o->getCode();
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
    ASSERTN(o->is_mem(), ("Need memory operation"));
    switch (o->getCode()) {
    case OR_ldm: {
        UINT sz = 0;
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (!sr->is_reg() ||
                sr->is_pred() ||
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
            if (!sr->is_reg() ||
                sr->is_pred() ||
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
    if (o->is_bus() || o->is_asm() || o->isSpadjust()) {
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
    switch (o->getCode()) {
    case OR_str: real_code = OR_str_i12; break;
    case OR_strd: real_code = OR_strd_i8; break;
    case OR_strb: real_code = OR_strb_i12; break;
    case OR_strh: real_code = OR_strh_i8; break;
    default: UNREACHABLE();
    }

    SR * ofst = o->get_store_ofst();
    ASSERT0(ofst->is_int_imm());
    if (isValidImmOpnd(real_code, ofst->getInt())) {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, o, this);
        ipl->append_tail(ip, getIssuePackageMgr());
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
    last->set_result(0, genR12(), this); //replace result-register with r12.
    for (OR * o2 = ors.get_head(); o2 != nullptr; o2 = ors.get_next()) {
        o2->set_pred(o->get_pred(), this);
    }
    renameOpnd(o, base, genR12(), false);
    renameOpnd(o, ofst, genIntImm(0, true), false);

    if (OR_is_fake(last)) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, last, this);
        ipl->append_tail(ip, getIssuePackageMgr());
    }

    OR_code(o) = real_code;
    IssuePackage * ip2 = m_ip_mgr.allocIssuePackage();
    ip2->set(SLOT_G, o, this);
    ipl->append_tail(ip2, getIssuePackageMgr());
    return;
}


void ARMCG::expandFakeSpadjust(IN OR * o, OUT IssuePackageList * ipl)
{
    SR * ofst = o->get_opnd(HAS_PREDICATE_REGISTER + 1);
    ORList ors;
    IOC cont;
    if (o->isSpadjustImm()) {
        ASSERT0(ofst->is_int_imm());
        // spadjust, SIZEOFSTACK
        //=>
        // sp = sp + ((+/-)SIZEOFSTACK)
        buildAdd(getSP(), ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    } else if (o->isSpadjustReg()) {
        ASSERT0(ofst->is_reg());
        //Note XGEN supposed that the direction of stack-pointer is always
        //decrement.
        // spadjust, ADDEND
        //=>
        // sp = sp - ADDEND
        buildSub(getSP(), ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    } else {
        UNREACHABLE();
    }
    ors.copyDbx(&OR_dbx(o));

    if (ors.get_elem_count() == 1) {
        // add sr66 <--tp, sp, #Imm10
        //=>
        // add sp <--PRED, sp, #Imm10
        OR * adj = ors.get_head();
        ASSERT0(adj && adj->get_result(0)->is_reg());

        adj->set_pred(o->get_pred(), this);
        adj->set_result(0, getSP(), this);

        if (adj->is_fake()) {
            expandFakeOR(adj, ipl);
            return;
        }

        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, adj, this);
        ipl->append_tail(ip, getIssuePackageMgr());
        return;
    }

    if (ors.get_elem_count() == 2) {
        // movw_i sr65 <--tp, #108
        // add sr66 <--tp, sp, sr65
        //=>
        // movw_i t <--tp, #108
        // add sp <--tp, sp, t
        ASSERT0(ors.get_head_nth(0)->is_movi());
        ASSERT0(ors.get_head_nth(1)->getCode() == OR_add ||
                ors.get_head_nth(1)->getCode() == OR_sub);

        OR * movi = ors.get_head_nth(0);
        SR * res = movi->get_result(0);
        movi->set_mov_to(o->get_result(1), this);
        ASSERT0(!movi->is_fake());

        OR * add = ors.get_head_nth(1);
        renameOpnd(add, res, o->get_result(1), false);
        add->set_result(0, getSP(), this);

        for (OR * o2 = ors.get_head(); o2 != nullptr; o2 = ors.get_next()) {
            IssuePackage * ip = m_ip_mgr.allocIssuePackage();
            o2->set_pred(o->get_pred(), this);
            ip->set(SLOT_G, o2, this);
            ipl->append_tail(ip, getIssuePackageMgr());
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
        ASSERT0(ors.get_head_nth(0)->is_movi());
        ASSERT0(ors.get_head_nth(1)->is_movi());
        ASSERT0(ors.get_head_nth(2)->getCode() == OR_add ||
                ors.get_head_nth(2)->getCode() == OR_sub);

        OR * movi = ors.get_head_nth(0);
        ASSERT0(!movi->is_fake());

        SR * res = movi->get_result(0);
        movi->set_mov_to(o->get_result(1), this);
        ors.get_head_nth(1)->set_mov_to(o->get_result(1), this);

        OR * add = ors.get_head_nth(2);
        renameOpnd(add, res, o->get_result(1), false);
        add->set_result(0, getSP(), this);

        for (OR * o2 = ors.get_head(); o2 != nullptr; o2 = ors.get_next()) {
            IssuePackage * ip = m_ip_mgr.allocIssuePackage();
            o2->set_pred(o->get_pred(), this);
            ip->set(SLOT_G, o2, this);
            ipl->append_tail(ip, getIssuePackageMgr());
        }
        return;
    }
    UNREACHABLE();
}


void ARMCG::expandFakeLoad(IN OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o && ipl);
    OR_TYPE real_code = OR_UNDEF;
    switch (o->getCode()) {
    case OR_ldr: real_code = OR_ldr_i12; break;
    case OR_ldrb: real_code = OR_ldrb_i12; break;
    case OR_ldrh: real_code = OR_ldrh_i8; break;
    case OR_ldrsb: real_code = OR_ldrsb_i8; break;
    case OR_ldrsh: real_code = OR_ldrsh_i8; break;
    case OR_ldrd: real_code = OR_ldrd_i8; break;
    default: UNREACHABLE();
    }

    SR * ofst = o->get_load_ofst();
    ASSERT0(ofst->is_int_imm());
    if (isValidImmOpnd(real_code, ofst->getInt())) {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, o, this);
        ipl->append_tail(ip, getIssuePackageMgr());
        return;
    }

    //  sr1 = [sr3, ofst]
    //=>
    //  sr1 = sr3 + ofst
    //  sr1 = [sr1]
    SR * sr1 = o->get_result(0);
    SR * sr3 = o->get_load_base();
    ASSERT0(sr1 && sr3 && sr1->is_reg() && sr3->is_reg());
    ASSERT0(ofst);

    ORList ors;
    IOC cont;
    buildAdd(sr3, ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));

    ASSERT0(ors.get_elem_count() == 1);
    OR * last = ors.remove_tail();
    ASSERT0(last && (last->getCode() == OR_add ||
                     last->getCode() == OR_add_i));
    last->set_result(0, sr1, this); //replace result-register with sr1.
    if (last->is_fake()) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        last->set_pred(o->get_pred(), this);
        ip->set(SLOT_G, last, this);
        ipl->append_tail(ip, getIssuePackageMgr());
    }

    OR_code(o) = real_code;
    o->set_load_base(sr1, this);
    o->set_load_ofst(genIntImm(0, false), this);
    IssuePackage * ip = m_ip_mgr.allocIssuePackage();
    ip->set(SLOT_G, o, this);
    ipl->append_tail(ip, getIssuePackageMgr());
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
    ASSERT0(sr1 && sr3 && sr1->is_reg() && sr3->is_reg());

    SR * ofst = o->get_load_ofst();
    ASSERT0(ofst);
    ASSERT0(ofst->is_int_imm());
    if (isValidImmOpnd(OR_ldrd_i8, ofst->getInt())) {
        OR_code(o) = OR_ldrd_i8;
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, o, this);
        ipl->append_tail(ip, getIssuePackageMgr());
        return;
    }

    ORList ors;
    IOC cont;
    buildAdd(sr3, ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ors.copyDbx(&OR_dbx(o));
    ASSERT0(ors.get_elem_count() == 1);

    OR * last = ors.remove_tail();
    ASSERT0(last && (OR_code(last) == OR_add || OR_code(last) == OR_add_i));
    last->set_result(0, sr1, this); //replace result-register with sr1.

    if (OR_is_fake(last)) {
        expandFakeOR(last, ipl);
    } else {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        last->set_pred(o->get_pred(), this);
        ip->set(SLOT_G, last, this);
        ipl->append_tail(ip, getIssuePackageMgr());
    }

    OR * ldrd = buildOR(OR_ldrd_i8, 2, 3, sr1, sr2,
                        o->get_pred(), sr1, genIntImm(0, false));
    ldrd->set_pred(o->get_pred(), this);
    OR_dbx(ldrd).copy(OR_dbx(o));

    IssuePackage * ip = m_ip_mgr.allocIssuePackage();
    ip->set(SLOT_G, ldrd, this);
    ipl->append_tail(ip, getIssuePackageMgr());
}


void ARMCG::expandFakeMov32(IN OR * o, OUT IssuePackageList * ipl)
{
    SR * to = o->get_mov_to();
    SR * from = o->get_mov_from();
    ASSERT0(to && from);
    OR * low = nullptr;
    OR * high = nullptr;

    //Decompose OR_mov32_i into OR_movw_i and OR_movt_i.
    if (SR_type(from) == SR_INT_IMM) {
        // mov32 rd = imm
        //=>
        // movw rd, first_half_part(imm)
        // movt rd, second_half_part(imm)
        low = genOR(OR_movw_i);
        low->set_mov_to(to, this);
        low->set_mov_from(genIntImm((HOST_INT)(from->getInt() & 0xFFFF),
                                    true), this);
        low->set_pred(o->get_pred(), this);

        high = genOR(OR_movt_i);
        high->set_mov_to(to, this);
        high->set_mov_from(genIntImm((HOST_INT)
                                     ((from->getInt() >> 16) & 0xFFFF),
                                     true), this);
        high->set_pred(o->get_pred(), this);
    } else if (SR_type(from) == SR_VAR) {
        // mov32 rd = Sym
        //=>
        // movw rd, Sym
        // movt rd, Sym
        low = genOR(OR_movw_i);
        low->set_mov_to(to, this);
        low->set_mov_from(genVAR(SR_var(from)), this);
        low->set_pred(o->get_pred(), this);

        high = genOR(OR_movt_i);
        high->set_mov_to(to, this);
        high->set_mov_from(genVAR(SR_var(from)), this);
        high->set_pred(o->get_pred(), this);
    } else {
        UNREACHABLE();
    }

    ASSERT0(low && high);
    OR_dbx(low).copy(OR_dbx(o));
    OR_dbx(high).copy(OR_dbx(o));
    IssuePackage * ip = m_ip_mgr.allocIssuePackage();
    ip->set(SLOT_G, low, this);
    ipl->append_tail(ip, getIssuePackageMgr());
    ip = m_ip_mgr.allocIssuePackage();
    ip->set(SLOT_G, high, this);
    ipl->append_tail(ip, getIssuePackageMgr());
}


void ARMCG::expandFakeShift(IN OR * o, OUT IssuePackageList * ipl)
{
    OR_TYPE ckort = OR_UNDEF;
    OR_TYPE newort = OR_UNDEF;
    switch (o->getCode()) {
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
    ASSERT0(imm->is_imm());
    if (!isValidImmOpnd(ckort, 2, imm->getInt())) {
        // rd = rs + imm;
        //=>
        // r12 = imm;
        // rd = rs + r12;
        ORList ors;
        IOC cont;
        buildMove(genR12(), imm, ors, &cont);
        OR * mv = ors.get_tail();
        ASSERT0(mv && ors.get_elem_count() == 1);
        mv->set_pred(o->get_pred(), this);
        OR_dbx(mv).copy(OR_dbx(o));

        if (OR_is_fake(mv)) {
            expandFakeOR(mv, ipl);
        } else {
            IssuePackage * ip = m_ip_mgr.allocIssuePackage();
            ip->set(SLOT_G, mv, this);
            ipl->append_tail(ip, getIssuePackageMgr());
        }

        renameOpnd(o, imm, genR12(), false);

        //Change OR code from OR_xxx_i to OR_xxx.
        OR_code(o) = newort;

        IssuePackage * ip2 = m_ip_mgr.allocIssuePackage();
        ip2->set(SLOT_G, o, this);
        ipl->append_tail(ip2, getIssuePackageMgr());
    } else {
        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, o, this);
        ipl->append_tail(ip, getIssuePackageMgr());
    }
}


void ARMCG::expandFakeOR(IN OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o->is_fake() && ipl);
    switch (o->getCode()) {
    case OR_str:
    case OR_strd:
    case OR_strb:
    case OR_strsb:
    case OR_strh:
    case OR_strsh:
       expandFakeStore(o, ipl);
       return;
    case OR_spadjust_i:
    case OR_spadjust_r:
       expandFakeSpadjust(o, ipl);
       return;
    case OR_ldr:
    case OR_ldrb:
    case OR_ldrsb:
    case OR_ldrh:
    case OR_ldrsh:
       expandFakeLoad(o, ipl);
       return;
    case OR_ldrd:
       expandFakeMultiLoad(o, ipl);
       return;
    case OR_mov32_i:
       expandFakeMov32(o, ipl);
       return;
    case OR_lsr_i32:
    case OR_lsl_i32:
    case OR_asr_i32:
       expandFakeShift(o, ipl);
       return;
    case OR_add_i:
    case OR_sub_i:
    case OR_lsr_i: {
        SR * imm = o->get_opnd(2);
        ASSERT0(imm->is_imm());
        if (!isValidImmOpnd(o->getCode(), 2, imm->getInt())) {
            // rd = rs + imm;
            //=>
            // r12 = imm;
            // rd = rs + r12;
            ORList ors;
            IOC cont;
            buildMove(genR12(), imm, ors, &cont);
            OR * mv = ors.get_tail();
            ASSERT0(mv && ors.get_elem_count() == 1);
            mv->set_pred(o->get_pred(), this);
            OR_dbx(mv).copy(OR_dbx(o));

            if (OR_is_fake(mv)) {
                expandFakeOR(mv, ipl);
            } else {
                IssuePackage * ip = m_ip_mgr.allocIssuePackage();
                ip->set(SLOT_G, mv, this);
                ipl->append_tail(ip, getIssuePackageMgr());
            }

            renameOpnd(o, imm, genR12(), false);

            //Change OR code from OR_xxx_i to OR_xxx.
            OR_TYPE newort = OR_UNDEF;
            switch (o->getCode()) {
            case OR_add_i: newort = OR_add; break;
            case OR_sub_i: newort = OR_sub; break;
            case OR_lsr_i: newort = OR_lsr; break;
            default: UNREACHABLE();
            }
            OR_code(o) = newort;

            IssuePackage * ip2 = m_ip_mgr.allocIssuePackage();
            ip2->set(SLOT_G, o, this);
            ipl->append_tail(ip2, getIssuePackageMgr());
            return;
        }

        IssuePackage * ip = m_ip_mgr.allocIssuePackage();
        ip->set(SLOT_G, o, this);
        ipl->append_tail(ip, getIssuePackageMgr());
        return;
    }
    default: UNREACHABLE();
    }
}


bool ARMCG::skipArgRegister(Var const* param,
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
            regset->get_next(reg) == BS_UNDEF) {
            //Passed the odd number register to facilitate the use
            //of value in paired-register.
            ASSERTN(isEvenReg((REG)regset->get_next(reg)),
                ("not continuous"));
            return true;
        }
    }
    #endif
    return false;
}


//Interface to generate ORs to store physical register on top of stack.
void ARMCG::storeRegToStack(SR * reg, OUT ORList & ors)
{
    ASSERT0(reg->is_reg());
    OR * push = buildOR(OR_push, 1, 3, getSP(), getTruePred(), reg, getSP());
    ors.append_tail(push);
}


//Interface to generate ORs to reload physical register from top of stack.
void ARMCG::reloadRegFromStack(SR * reg, OUT ORList & ors)
{
    ASSERT0(reg->is_reg());
    OR * pop = buildOR(OR_pop, 2, 2, reg, getSP(), getTruePred(), getSP());
    ors.append_tail(pop);
}                            
