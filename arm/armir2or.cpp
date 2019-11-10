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

//Process binary operation, the function generate OR that consist of two
//operands and one result.
void ARMIR2OR::convertBinaryOp(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->isBinaryOp() && BIN_opnd0(ir) && BIN_opnd1(ir),
           ("missing operand"));
    ASSERTN(!ir->is_vec(), ("TODO"));

    if (!ir->is_add() && !ir->is_sub()) {
        IR2OR::convertBinaryOp(ir, ors, cont);
        return;
    }

    //Add, sub are more complex opertions.
    if (ir->is_fp()) {
        convertAddSubFp(ir, ors, cont);
        return;
    }

    //Load Operand0
    IOC tmp;
    convert(BIN_opnd0(ir), ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));

    //Load Operand1
    tmp.clean();
    convert(BIN_opnd1(ir), ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));

    ASSERT0((BIN_opnd0(ir)->getTypeSize(m_tm) ==
             BIN_opnd1(ir)->getTypeSize(m_tm)) ||
            (BIN_opnd0(ir)->is_ptr() || BIN_opnd1(ir)->is_ptr()));

    ORList tors;
    switch (ir->getCode()) {
    case IR_ADD:
        getCG()->buildAdd(opnd0, opnd1,
            BIN_opnd0(ir)->getTypeSize(m_tm), ir->is_signed(), tors, cont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        break;
    case IR_SUB:
        getCG()->buildSub(opnd0, opnd1,
            BIN_opnd0(ir)->getTypeSize(m_tm), ir->is_signed(), tors, cont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        break;
    default: UNREACHABLE();
    }
}


//Generate operations: reg = &var
void ARMIR2OR::convertLda(
        VAR const* var,
        HOST_INT lda_ofst,
        Dbx const* dbx,
        OUT ORList & ors,
        IN IOC * cont)
{
    getCG()->buildLda(var, lda_ofst, dbx, ors, cont);
}


void ARMIR2OR::convertStoreVar(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir != NULL && ir->is_st());
    UINT resbytesize = ir->getTypeSize(m_tm);
    if (resbytesize <= 8) {
        IR2OR::convertStoreVar(ir, ors, cont);
        return;
    }

    ASSERT0(ST_rhs(ir)->is_ld());
    ASSERT0(((HOST_INT)ST_rhs(ir)->getTypeSize(m_tm)) == resbytesize);

    //Load the address of target VAR into register.
    cont->clean_bottomup();
    convertLda(ST_idinfo(ir), ST_ofst(ir), ::getDbx(ir), ors, cont);
    SR * tgt = cont->get_reg(0);
    ASSERT0(tgt && SR_is_reg(tgt));

    //Load the address of source VAR into register.
    cont->clean_bottomup();
    convertLda(LD_idinfo(ST_rhs(ir)), LD_ofst(ST_rhs(ir)),
        ::getDbx(ST_rhs(ir)), ors, cont);
    SR * src = cont->get_reg(0);
    ASSERT0(src && SR_is_reg(src));

    //Generate ::memcpy.
    ORList tors;
    cont->clean_bottomup();
    getCG()->buildMemcpy(tgt, src, resbytesize, tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


//Generate operations: reg = &var
void ARMIR2OR::convertLda(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_lda());
    VAR * v = LDA_idinfo(ir);
    ASSERT0(v);
    ASSERTN(!VAR_is_unallocable(v), ("var must be allocable during CG"));
    convertLda(v, LDA_ofst(ir), ::getDbx(ir), ors, cont);
}


void ARMIR2OR::convertReturnValue(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    if (ir == NULL) { return; }
    ASSERTN(ir->get_next() == NULL, ("ARM only support C language."));
    ASSERT0(ir->isCallStmt());
    if (!ir->hasReturnValue()) {
        return;
    }
    IOC tmp_cont;
    SR * retv = NULL;
    if (ir->getTypeSize(m_tm) <= 4) {
        retv = getCG()->gen_r0();
    } else if (ir->getTypeSize(m_tm) <= 8) {
        retv = getCG()->getSRVecMgr()->genSRVec(2,
            getCG()->gen_r0(),
            getCG()->gen_r1());
    } else {
        //Get the first formal parameter, it is the return buffer of the value.
        VAR const* v = getCG()->get_param_vars().get(0);
        ASSERT0(v);
        DUMMYUSE(v);
        ASSERTN(!g_gen_code_for_big_return_value, ("TODO"));
        return;
    }
    convertCopyPR(ir, retv, ors, cont);
}


void ARMIR2OR::convertCall(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->isCallStmt(), ("illegal ir"));
    processRealParams(CALL_param_list(ir), ors, cont);

    //Collect the maximum parameters size during code generation.
    //And revise spadjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(IOC_param_size(cont));

    if (IOC_param_size(cont) > 0) {
        //DO not adjust SP here for parameters, callee will
        //do this job.
    }

    ASSERTN(!CALL_is_intrinsic(ir), ("TODO"));

    ORList tors;
    UINT retv_sz = GENERAL_REGISTER_SIZE;

    if (ir->hasReturnValue()) {
        retv_sz = ir->getTypeSize(m_tm);
    }

    if (ir->is_call()) {
        getCG()->buildCall(CALL_idinfo(ir), retv_sz, tors, cont);
    } else {
        ASSERT0(ir->is_icall());
        //ICALL
        IR * callee = ICALL_callee(ir);
        ASSERT0(callee->is_ld() || callee->is_lda() ||
            (callee->is_cvt() && CVT_exp(callee)->is_const()));
        IOC tc;
        if (callee->is_ld() || callee->is_lda()) {
            convertGeneralLoad(callee, tors, &tc);
        } else {
            convertGeneralLoad(CVT_exp(callee), tors, &tc);
        }
        SR * tgtsr = tc.get_reg(0);
        getCG()->buildICall(tgtsr, retv_sz, tors, cont);
    }
    tors.copyDbx(ir);
    ors.append_tail(tors);

    convertReturnValue(ir, ors, cont);
}


void ARMIR2OR::convertRem(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_rem(), ("illegal ir"));
    ASSERTN(BIN_opnd0(ir) != NULL && BIN_opnd1(ir) != NULL,
            ("missing operand"));
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    ASSERT0(BIN_opnd0(ir)->getTypeSize(m_tm) ==
            BIN_opnd1(ir)->getTypeSize(m_tm));

    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);
    //Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    ORList tors;
    UINT retv_sz = ir->getTypeSize(m_tm);
    VAR const* builtin = NULL;
    if (retv_sz <= 4) {
        if (ir->is_uint()) {
            //builtin = getCG()->m_builtin_uimod;
            builtin = getCG()->m_builtin_umodsi3;
        } else if (ir->is_sint()) {
            builtin = getCG()->m_builtin_modsi3;
        } else {
            UNREACHABLE();
        }
    } else if (retv_sz <= 8) {
        if (ir->is_uint()) {
            builtin = getCG()->m_builtin_umoddi3;
        } else if (ir->is_sint()) {
            builtin = getCG()->m_builtin_moddi3;
        } else {
            UNREACHABLE();
        }
    } else {
        UNREACHABLE();
    }
    getCG()->buildCall(builtin, retv_sz, tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertAddSubFp(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN((ir->is_add() || ir->is_sub()) && ir->is_fp(), ("illegal ir"));
    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);

    ASSERTN(op0 && op1, ("missing operand"));
    ASSERT0(!op0->is_vec() && !op1->is_vec());
    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));

    //Convert Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));
    ASSERT0(opnd0->getByteSize() == op0->getTypeSize(m_tm));

    //Convert Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));
    ASSERT0(opnd1->getByteSize() == op1->getTypeSize(m_tm));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    UINT retv_sz = ir->getTypeSize(m_tm);
    VAR const* builtin = NULL;
    if (retv_sz <= 4) {
        if (ir->is_add()) {
            builtin = getCG()->m_builtin_addsf3;
        } else {
            builtin = getCG()->m_builtin_subsf3;
        }
    } else if (retv_sz <= 8) {
        if (ir->is_add()) {
            builtin = getCG()->m_builtin_adddf3;
        } else {
            builtin = getCG()->m_builtin_subdf3;
        }
    } else { UNREACHABLE(); }

    ORList tors;
    getCG()->buildCall(builtin, retv_sz, tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertDiv(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_div(), ("illegal ir"));
    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);

    ASSERTN(op0 && op1, ("missing operand"));
    ASSERT0(!op0->is_vec() && !op1->is_vec());
    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));

    //Convert Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));

    //Convert Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    UINT retv_sz = ir->getTypeSize(m_tm);
    VAR const* builtin = NULL;
    if (retv_sz <= 4) {
        if (op0->is_uint()) {
            builtin = getCG()->m_builtin_udivsi3;
        } else if (op0->is_sint()) {
            builtin = getCG()->m_builtin_divsi3;
        } else if (op0->is_fp()) {
            builtin = getCG()->m_builtin_divsf3;
        } else { UNREACHABLE(); }
    } else if (retv_sz <= 8) {
        if (op0->is_uint()) {
            builtin = getCG()->m_builtin_udivdi3;
        } else if (op0->is_sint()) {
            builtin = getCG()->m_builtin_divdi3;
        } else if (op0->is_fp()) {
            builtin = getCG()->m_builtin_divdf3;
        } else { UNREACHABLE(); }
    } else { UNREACHABLE(); }

    ORList tors;
    getCG()->buildCall(builtin, retv_sz, tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMulofInt(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    //CASE:
    // int b,c;
    // int a;
    // a = b * c;
    IOC tmp;
    convertGeneralLoad(BIN_opnd0(ir), ors, &tmp);
    SR * sr1 = tmp.get_reg(0);
    tmp.clean();
    convertGeneralLoad(BIN_opnd1(ir), ors, &tmp);
    SR * sr2 = tmp.get_reg(0);
    Dbx * dbx = ::getDbx(ir);
    ASSERT0(sr1->getByteSize() == GENERAL_REGISTER_SIZE);
    ASSERT0(sr2->getByteSize() == GENERAL_REGISTER_SIZE);
    ORList tors;
    getCG()->buildMul(sr1, sr2, GENERAL_REGISTER_SIZE, true, tors, cont);
    if (dbx != NULL) {
        tors.copyDbx(dbx);
    }
    ors.append_tail(tors);
}


//CALL __muldi3
void ARMIR2OR::convertMulofLongLong(
        IR const* ir,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(ir->is_mul());
    ASSERT0((BIN_opnd0(ir)->getTypeSize(m_tm) ==
             BIN_opnd1(ir)->getTypeSize(m_tm)) &&
             BIN_opnd0(ir)->getTypeSize(m_tm) == 8);

    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);

    //Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    ASSERT0(tmp.get_addr() == NULL);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0) && SR_is_vec(opnd0));

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    ASSERT0(tmp.get_addr() == NULL);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1) && SR_is_vec(opnd1));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    ORList tors;
    getCG()->buildCall(getCG()->m_builtin_muldi3, ir->getTypeSize(m_tm),
        tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMulofFloat(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_mul());

    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);

    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));
    ASSERT0(op0->getTypeSize(m_tm) == 4 || op0->getTypeSize(m_tm) == 8);

    //Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));

    #ifdef _DEBUG_
    if (op0->getTypeSize(m_tm) > 4 && op0->getTypeSize(m_tm) <= 8) {
        ASSERT0(SR_is_vec(opnd0));
    }
    #endif

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));

    #ifdef _DEBUG_
    if (op1->getTypeSize(m_tm) > 4 && op1->getTypeSize(m_tm) <= 8) {
        ASSERT0(SR_is_vec(opnd1));
    }
    #endif

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    VAR const* builtin;
    if (op0->getTypeSize(m_tm) <= 4) {
        builtin = getCG()->m_builtin_mulsf3;
    } else {
        builtin = getCG()->m_builtin_muldf3;
    }

    ORList tors;
    getCG()->buildCall(builtin, ir->getTypeSize(m_tm), tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMul(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_mul(), ("illegal ir"));
    IR * op0 = BIN_opnd0(ir);
    IR * or1 = BIN_opnd1(ir);
    CHECK_DUMMYUSE(or1);
    ASSERT0(op0->getTypeSize(m_tm) == or1->getTypeSize(m_tm));
    if (op0->getTypeSize(m_tm) <= 2) {
        convertBinaryOp(ir, ors, cont);
    } else if (op0->getTypeSize(m_tm) <= 4) {
        if (op0->is_int()) {
            convertMulofInt(ir, ors, cont);
        } else if (op0->is_fp()) {
            convertMulofFloat(ir, ors, cont);
        }
    } else if (op0->getTypeSize(m_tm) <= 8) {
        if (op0->is_int()) {
            convertMulofLongLong(ir, ors, cont);
        } else if (op0->is_fp()) {
            convertMulofFloat(ir, ors, cont);
        }
    } else {
        ASSERTN(0, ("unsupport"));
    }
}


void ARMIR2OR::convertNeg(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_neg(), ("illegal ir"));
    IR * opnd = UNA_opnd(ir);
    if (opnd->is_const()) {
        ASSERT0(opnd->is_const() && opnd->is_int());
        if (ir->getTypeSize(m_tm) <= 4) {
            SR * res = getCG()->genIntImm((HOST_INT)-CONST_int_val(opnd), true);
            ASSERT0(cont != NULL);
            cont->set_reg(RESULT_REGISTER_INDEX, res);
        } else if (ir->getTypeSize(m_tm) <= 8) {
            ASSERT0_DUMMYUSE(sizeof(LONGLONG) == 8);
            LONGLONG v = -CONST_int_val(opnd);
            SR * res = getCG()->genIntImm((HOST_INT)v, true);
            SR * res2 = getCG()->genIntImm((HOST_INT)(v >> 32), true);
            ASSERT0(cont != NULL);
            getCG()->getSRVecMgr()->genSRVec(2, res, res2);
            cont->set_reg(RESULT_REGISTER_INDEX, res);
            //cont->set_reg(1, res2);
        } else {
            UNREACHABLE();
        }

        return;
    }

    IOC tc;
    convertGeneralLoad(opnd, ors, &tc);
    if (ir->getTypeSize(m_tm) <= 4) {
        SR * res = tc.get_reg(0);
        OR * o = getCG()->buildOR(OR_neg, 1, 2,
            res, getCG()->genTruePred(), res);
        copyDbx(o, ir);
        ors.append_tail(o);
        ASSERT0(cont != NULL);
        cont->set_reg(RESULT_REGISTER_INDEX, res);
    } else if (ir->getTypeSize(m_tm) <= 8) {
        SR * res = tc.get_reg(0);
        SR * res2 = SR_vec(res)->get(1);;
        ASSERT0(res && res2);
        SR * src = getCG()->genIntImm((HOST_INT)0, false);
        SR * src2 = getCG()->genIntImm((HOST_INT)0, false);
        ORList tors;
        getCG()->getSRVecMgr()->genSRVec(2, src, src2);
        getCG()->buildSub(src, res, 8, true, tors, &tc);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        ASSERT0(cont && tc.get_reg(0)->getByteSize() == 8);
        res = tc.get_reg(0);
        res2 = SR_vec(res)->get(1);
        cont->set_reg(RESULT_REGISTER_INDEX, res);
        //cont->set_reg(1, res2);
    } else {
        UNREACHABLE();
    }
}


//Inverted the result value, e.g: 1->0 or 0->1.
void ARMIR2OR::invertBoolValue(Dbx * dbx, SR * val, OUT ORList & ors)
{
    ASSERT0(val);
    SR * one = getCG()->genIntImm(1, false);
    OR_TYPE orty = mapIRType2ORType(IR_XOR,
        m_tm->getDTypeByteSize(D_B), val, one, false);
    ASSERTN(orty != OR_UNDEF,
        ("mapIRType2ORType() can not find proper operation"));

    OR * o;
    if (HAS_PREDICATE_REGISTER) {
        ASSERT0(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 3);
        o = getCG()->buildOR(orty, 1, 3, val, getCG()->genTruePred(), val, one);
    } else {
        ASSERT0(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 2);
        o = getCG()->buildOR(orty, 1, 3, val, val, one);
    }
    if (dbx != NULL) {
        OR_dbx(o).copy(*dbx);
    }
    ors.append_tail(o);
}


void ARMIR2OR::getResultPredByIRTYPE(
        IR_TYPE code,
        SR ** truepd,
        SR ** falsepd,
        bool is_signed)
{
    ASSERT0(truepd && falsepd);
    switch (code) {
    case IR_EQ:
        *truepd = getCG()->genEQPred();
        *falsepd = getCG()->genNEPred();
        return;
    case IR_NE:
        *truepd = getCG()->genNEPred();
        *falsepd = getCG()->genEQPred();
        return;
    case IR_LT:
        if (is_signed) {
            *truepd = getCG()->genLTPred();
            *falsepd = getCG()->genGEPred();
        } else {
            *truepd = getCG()->genLOPred();
            *falsepd = getCG()->genHSPred();
        }
        return;
    case IR_GT:
        if (is_signed) {
            *truepd = getCG()->genGTPred();
            *falsepd = getCG()->genLEPred();
        } else {
            *truepd = getCG()->genHIPred();
            *falsepd = getCG()->genLSPred();
        }
        return;
    case IR_LE:
        if (is_signed) {
            *truepd = getCG()->genLEPred();
            *falsepd = getCG()->genGTPred();
        } else {
            *truepd = getCG()->genLSPred();
            *falsepd = getCG()->genHIPred();
        }
        return;
    case IR_GE:
        if (is_signed) {
            *truepd = getCG()->genGEPred();
            *falsepd = getCG()->genLTPred();
        } else {
            *truepd = getCG()->genHSPred();
            *falsepd = getCG()->genLOPred();
        }
        return;
    default: break;
    }
    UNREACHABLE();
    return;
}


//Generate compare operations and return the comparation result registers.
// e.g:
//     a - 1 > b + 2
// =>
//     sr0 = a - 1
//     sr1 = b + 2
//     sr3, p1, p2 <- cmp sr0, sr1
//     return sr3, p1, p2
void ARMIR2OR::convertRelationOpDWORD(
        IR const* ir,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());

    IR const* opnd0 = BIN_opnd0(ir);
    IR const* opnd1 = BIN_opnd1(ir);

    ASSERT0(opnd0->getTypeSize(m_tm) == opnd1->getTypeSize(m_tm));

    //Integer, dould size of GENERAL_REGISTER_SIZE.
    ASSERT0(opnd0->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);

    ORList tors;
    IOC tmp;
    //Operands 0
    convertGeneralLoad(opnd0, tors, &tmp);
    SR * sr0 = tmp.get_reg(0);
    ASSERT0(SR_is_vec(sr0));

    //Operands 1
    tmp.clean();
    convertGeneralLoad(opnd1, tors, &tmp);
    SR * sr1 = tmp.get_reg(0);
    ASSERT0(SR_is_vec(sr1));

    ASSERT0(sr0->getByteSize() == opnd0->getTypeSize(m_tm));
    ASSERT0(sr0->getByteSize() == sr1->getByteSize());

    SR * truepd = NULL;
    SR * falsepd = NULL;
    getResultPredByIRTYPE(ir->getCode(), &truepd, &falsepd, opnd0->is_signed());

    //Comparison algo:
    //  compare is <sr1,sr2>, <sr3,sr4> equal:
    //  cmp sr2, sr4
    //  ifeq cmp sr1, sr3
    //  ifle cmp sr2, sr4

    //Compare high part equality.
    //truepd, falsepd = cmp sr0, sr1
    getCG()->buildARMCmp(OR_cmp, getCG()->genTruePred(),
        SR_vec(sr0)->get(1), SR_vec(sr1)->get(1), tors, cont);

    //Compare low part if high part is equal.
    getCG()->buildARMCmp(OR_cmp, getCG()->genEQPred(),
        SR_vec(sr0)->get(0), SR_vec(sr1)->get(0), tors, cont);

    //Recompare high part if falsepd is true.
    getCG()->buildARMCmp(OR_cmp,
        falsepd, SR_vec(sr0)->get(1), SR_vec(sr1)->get(1), tors, cont);

    tors.copyDbx(ir);
    ors.append_tail(tors);
    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR,
//TruePredicatedSR, FalsePredicatedSR.
//The ResultSR record the boolean value of comparison of relation operation.
// e.g:
//     a - 1 > b + 2
// =>
//     sr0 = a - 1
//     sr1 = b + 2
//     sr3, p1, p2 <- cmp sr0, sr1
//     return sr3, p1, p2
void ARMIR2OR::convertRelationOp(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_relation());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);
    ASSERT0(opnd0->getTypeSize(m_tm) == opnd1->getTypeSize(m_tm));
    ASSERT0(opnd0 && opnd1);

    if (opnd0->is_fp()) {
        convertRelationOpFp(ir, ors, cont);
        return;
    }

    if (opnd0->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2) {
        convertRelationOpDWORD(ir, ors, cont);
        return;
    }

    ASSERT0(opnd0->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE);

    IOC tmp;
    //Operands 0
    convertGeneralLoad(opnd0, ors, &tmp);
    SR * sr0 = tmp.get_reg(0);

    //Operands 1
    tmp.clean();
    convertGeneralLoad(opnd1, ors, &tmp);
    SR * sr1 = tmp.get_reg(0);

    //Generate compare operations.
    tmp.clean();
    getCG()->buildARMCmp(OR_cmp, getCG()->genTruePred(), sr0, sr1, ors, cont);
    cont->set_reg(RESULT_REGISTER_INDEX, NULL);
    SR * truepd = NULL;
    SR * falsepd = NULL;
    getResultPredByIRTYPE(ir->getCode(),
        &truepd, &falsepd, opnd0->is_signed());

    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);
    //if (!ir->getParent()->isConditionalBr()) {
    //    SR * res = getCG()->genReg();
    //    ORList tors;
    //    getCG()->buildMove(res, getCG()->gen_one(), tors, cont);
    //    tors.set_pred(truepd);
    //    tors.copyDbx(ir);
    //    ors.append_tail(tors);

    //    tors.clean();
    //    getCG()->buildMove(res, getCG()->gen_zero(), tors, cont);
    //    tors.set_pred(falsepd);
    //    tors.copyDbx(ir);
    //    ors.append_tail(tors);

    //    cont->set_reg(RESULT_REGISTER_INDEX, res); //used by non-conditional op
    //    return;
    //}

    ////record result
    ////used by convertSelect
    //cont->set_reg(RESULT_REGISTER_INDEX, getCG()->genPredReg());
    //
    ////record true-result predicator
    //cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd); //used by convertSelect

    ////record false-result predicator
    //cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd); //used by convertSelect

    ////record true-result
    //cont->set_pred(truepd);

    //IOC_is_inverted(cont) = false;
}


void ARMIR2OR::recordRelationOpResult(
        IR const* ir,
        SR * truepd,
        SR * falsepd,
        SR * result_pred,
        OUT ORList & ors,
        IN OUT IOC * cont)
{
    if (!ir->getParent()->isConditionalBr()) {
        SR * res = getCG()->genReg();
        ORList tors;
        getCG()->buildMove(res, getCG()->gen_one(), tors, cont);
        tors.set_pred(truepd);
        ors.append_tail(tors);

        tors.clean();
        getCG()->buildMove(res, getCG()->gen_zero(), tors, cont);
        tors.set_pred(falsepd);
        ors.append_tail(tors);

        cont->set_reg(RESULT_REGISTER_INDEX, res); //used by non-conditional op

        //truepd, falsepd and result_pred are useless.
        return;
    }

    //Record result.
    //used by Non-Conditional-Op, such as convertSelect.
    cont->set_reg(RESULT_REGISTER_INDEX, NULL);

    //Record true-result predicator
    cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd); //record true result

    //Record false-result predicator
    cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd); //record false result

    //Record true-result.
    cont->set_pred(result_pred);

    IOC_is_inverted(cont) = false;
}


void ARMIR2OR::convertIgoto(IR const* ir, OUT ORList & ors, IN IOC *)
{
    ASSERT0(ir->is_igoto());
    IOC tmp;
    convertLoadVar(IGOTO_vexp(ir), ors, &tmp);
    SR * tgt_addr = tmp.get_reg(0);
    ASSERT0(tgt_addr);

    OR * o = getCG()->buildOR(OR_bx, 0, 2, getCG()->genTruePred(), tgt_addr);
    OR_dbx(o).copy(*getDbx(ir));
    ors.append_tail(o);
}


void ARMIR2OR::convertSelect(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_select());
    IOC tmp;
    ORList tors;

    //Generate Compare operation
    convertRelationOp(SELECT_pred(ir), tors, &tmp);
    tors.copyDbx(SELECT_pred(ir));
    ors.append_tail(tors);
    SR * cres = tmp.get_reg(0);
    CHECK_DUMMYUSE(cres);

    SR * true_pr = tmp.get_reg(1);
    SR * false_pr = tmp.get_reg(2);
    ASSERT0(true_pr && false_pr);

    SR * res = getCG()->genReg();
    ASSERT0(IR_dt(BIN_opnd0(SELECT_pred(ir))) ==
            IR_dt(BIN_opnd1(SELECT_pred(ir))));
    UINT res_size = ir->getTypeSize(m_tm);

    //True exp value
    {
        tmp.clean();
        tors.clean();
        convertGeneralLoad(SELECT_trueexp(ir), tors, &tmp);
        if (res_size <= 4) {
            SR * sr0 = tmp.get_reg(0);
            getCG()->buildMove(res, sr0, tors, &tmp);
        } else if (res_size <= 8) {
            SR * sr0 = tmp.get_reg(0);
            SR * sr1 = NULL;
            if (SR_vec(sr0) != NULL && SR_vec(sr0)->get(1) != NULL) {
                sr1 = SR_vec(sr0)->get(1);
            } else {
                sr1 = getCG()->genReg();
                getCG()->buildMove(sr1, getCG()->genIntImm((HOST_INT)0, false),
                    tors, &tmp);
            }
            SR * res2 = getCG()->genReg();
            getCG()->getSRVecMgr()->genSRVec(2, res, res2);
            getCG()->buildMove(res, sr0, tors, &tmp);
            getCG()->buildMove(res2, sr1, tors, &tmp);
        } else {
            UNREACHABLE();
        }
        tors.set_pred(true_pr);
        tors.copyDbx(SELECT_trueexp(ir));
        ors.append_tail(tors);
    }

    //False exp value
    {
        tmp.clean();
        tors.clean();
        convertGeneralLoad(SELECT_falseexp(ir), tors, &tmp);

        if (res_size <= 4) {
            SR * sr0 = tmp.get_reg(0);
            getCG()->buildMove(res, sr0, tors, &tmp);
        } else if (res_size <= 8) {
            SR * sr0 = tmp.get_reg(0);
            SR * sr1 = NULL;
            if (SR_vec(sr0) != NULL && SR_vec(sr0)->get(1) != NULL) {
                sr1 = SR_vec(sr0)->get(1);
            } else {
                sr1 = getCG()->genReg();
                getCG()->buildMove(sr1, getCG()->genIntImm((HOST_INT)0, false),
                    tors, &tmp);
            }

            SR * res2 = getCG()->genReg();
            getCG()->getSRVecMgr()->genSRVec(2, res, res2);
            getCG()->buildMove(res, sr0, tors, &tmp);
            getCG()->buildMove(res2, sr1, tors, &tmp);
        } else {
            UNREACHABLE();
        }

        tors.set_pred(false_pr);
        tors.copyDbx(SELECT_falseexp(ir));
        ors.append_tail(tors);
    }
    cont->set_reg(RESULT_REGISTER_INDEX, res);
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR,
//TruePredicatedSR, FalsePredicatedSR.
//The ResultSR record the boolean value of comparison of relation operation.
void ARMIR2OR::convertRelationOpFp(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);

    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));

    ORList tors;

    //Convert Operand0.
    IOC tmp;
    convert(op0, tors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != NULL && SR_is_reg(opnd0));
    ASSERT0(opnd0->getByteSize() == op0->getTypeSize(m_tm));

    //Convert Operand1.
    tmp.clean();
    convert(op1, tors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != NULL && SR_is_reg(opnd1));
    ASSERT0(opnd1->getByteSize() == op1->getTypeSize(m_tm));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, tors, 2,
        opnd0, NULL, opnd0->getByteSize(), ::getDbx(op0),
        opnd1, NULL, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    VAR const* builtin = NULL;
    if (opnd0->getByteSize() == GENERAL_REGISTER_SIZE) {
        switch (ir->getCode()) {
        case IR_LT: builtin = getCG()->m_builtin_ltsf2; break;
        case IR_GT: builtin = getCG()->m_builtin_gtsf2; break;
        case IR_GE: builtin = getCG()->m_builtin_gesf2; break;
        case IR_EQ: builtin = getCG()->m_builtin_eqsf2; break;
        case IR_NE: builtin = getCG()->m_builtin_nesf2; break;
        case IR_LE: builtin = getCG()->m_builtin_lesf2; break;
        default: UNREACHABLE();
        }
    } else {
        ASSERT0(opnd0->getByteSize() == GENERAL_REGISTER_SIZE * 2);
        switch (ir->getCode()) {
        case IR_LT: builtin = getCG()->m_builtin_ltdf2; break;
        case IR_GT: builtin = getCG()->m_builtin_gtdf2; break;
        case IR_GE: builtin = getCG()->m_builtin_gedf2; break;
        case IR_EQ: builtin = getCG()->m_builtin_eqdf2; break;
        case IR_NE: builtin = getCG()->m_builtin_nedf2; break;
        case IR_LE: builtin = getCG()->m_builtin_ledf2; break;
        default: UNREACHABLE();
        }
    }

    getCG()->buildCall(builtin, ir->getTypeSize(m_tm), tors, cont);

    //Get return value of call.
    //SR * retv = getCG()->genReg();
    //getCG()->buildMove(retv, getCG()->gen_r0(), tors, cont);

    tors.copyDbx(ir);
    ors.append_tail(tors);
    tors.clean();

    SR * r0 = getCG()->gen_r0();
    SR * one = getCG()->gen_one();
    SR * zero = getCG()->gen_zero();
    SR * truepd = NULL;
    SR * falsepd = NULL;
    bool needresval = !ir->getStmt()->isConditionalBr();
    if (needresval) {
        getCG()->buildCompare(OR_cmp_i, true, getCG()->gen_r0(),
            getCG()->genIntImm((HOST_INT)0, false), tors, cont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        tors.clean();
    }
    switch (ir->getCode()) {
    case IR_LT:
        //Buildin function return a value less than zero
        //if neither argument is NaN, and a is less than or equal to b.
        falsepd = getCG()->genGEPred();
        truepd = getCG()->genLTPred();
        if (needresval) {
            //(ge)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(lt)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    case IR_LE:
        //Buildin function return a value less than or equal to zero
        //if neither argument is NaN, and a is less than or equal to b.
        falsepd = getCG()->genGTPred();
        truepd = getCG()->genLEPred();
        if (needresval) {
            //(gt)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(le)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    case IR_GT:
        //Buildin function return a value greater than zero
        //if neither argument is NaN, and a is less than or equal to b.
        falsepd = getCG()->genLEPred();
        truepd = getCG()->genGTPred();
        if (needresval) {
            //(le)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(gt)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    case IR_GE:
        //Buildin function return a value greater than or equal to zero
        //if neither argument is NaN, and a is less than or equal to b.
        falsepd = getCG()->genLTPred();
        truepd = getCG()->genGEPred();
        if (needresval) {
            //(lt)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(ge)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    case IR_EQ:
        //These functions return zero if
        //either argument is NaN, or if a and b are equal.
        falsepd = getCG()->genNEPred();
        truepd = getCG()->genEQPred();
        if (needresval) {
            //(ne)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(eq)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    case IR_NE:
        //These functions return a nonzero value if
        //either argument is NaN, or if a and b are unequal.
        falsepd = getCG()->genEQPred();
        truepd = getCG()->genNEPred();
        if (needresval) {
            //(eq)r0 = 0;
            getCG()->buildMove(r0, zero, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(ne)r0 = 1;
            getCG()->buildMove(r0, one, tors, cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();
        }
        break;
    default:
        UNREACHABLE();
    }

    //Set result SR.
    ASSERT0(cont);
    cont->set_reg(RESULT_REGISTER_INDEX, r0);

    //Record predicate register that will be used by convertSelect.
    cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd);
    cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd);

    //Record predicate register that will be
    //used by convertTruebr/convertTruebrFP.
    cont->set_pred(truepd);
}


void ARMIR2OR::convertTruebrFp(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(cont->get_pred() == NULL);
    convertRelationOpFp(BR_det(ir), ors, cont);
    ASSERT0(cont->get_pred());

    SR * retv = cont->get_reg(0);
    ASSERT0(retv && SR_is_reg(retv));

    //Build truebr.
    ORList tors;
    SR * tgt_lab = getCG()->genLabel(BR_lab(ir));
    getCG()->buildCompare(OR_cmp_i, true, retv,
        getCG()->genIntImm((HOST_INT)0, false), tors, cont);
    //cmp does not produce result in register.
    ASSERT0(cont->get_reg(0) == NULL);
    getCG()->buildCondBr(tgt_lab, tors, cont);
    tors.copyDbx(ir);
    cont->set_pred(NULL); //clean status
    ors.append_tail(tors);
}


void ARMIR2OR::convertTruebr(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->is_truebr());
    IR * opnd0 = BIN_opnd0(BR_det(ir));
    if (opnd0->is_fp()) {
        convertTruebrFp(ir, ors, cont);
        return;
    }

    if (opnd0->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        IR2OR::convertTruebr(ir, ors, cont);
        return;
    }

    ASSERT0(opnd0->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);

    convertRelationOpDWORD(BR_det(ir), ors, cont);

    ORList tors;
    getCG()->buildCondBr(getCG()->genLabel(BR_lab(ir)), tors, cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
    return;
}


VAR const* ARMIR2OR::fp2int(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_int());
    if (tgt->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        if (tgt->is_sint()) {
            if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
                //float -> sign int.
                return getCG()->m_builtin_fixsfsi;
            }

            ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
            //double -> sign int.
            return getCG()->m_builtin_fixdfsi;
        }

        ASSERT0(tgt->is_uint() || tgt->is_bool());
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> unsign int.
            //float -> bool.
            return getCG()->m_builtin_fixunssfsi;
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //double -> unsign int.
        return getCG()->m_builtin_fixunsdfsi;
    }

    ASSERT0(tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    if (tgt->is_sint()) {
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> sign long long.
            return getCG()->m_builtin_fixsfdi;
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //double -> sign long long.
        return getCG()->m_builtin_fixdfdi;
    }

    ASSERT0(tgt->is_uint());
    if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
         //float -> unsign long long.
         return getCG()->m_builtin_fixunssfdi;
    }

    ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    //double -> unsign long long.
    return getCG()->m_builtin_fixunsdfdi;
}


VAR const* ARMIR2OR::int2fp(IR const* tgt, IR const* src)
{
    ASSERT0(src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE * 2
        && tgt->is_fp());
    if (tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
        if (src->is_sint()) {
            if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
                //sign int -> float.
                return getCG()->m_builtin_floatsisf;
            }

            ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
            //sign long long -> float.
            return getCG()->m_builtin_floatdisf;
        }

        ASSERT0(src->is_uint() || src->is_bool() ||
            src->is_ptr() || src->is_mc());
        if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
            //unsign int -> float.
            //bool -> float.
            return getCG()->m_builtin_floatunsisf;
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);

        //unsign long long -> float.
        return getCG()->m_builtin_floatundisf;
    }

    ASSERT0(tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    if (src->is_sint()) {
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //sign int -> double.
            return getCG()->m_builtin_floatsidf;
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //sign long long -> double.
        return getCG()->m_builtin_floatdidf;
    }

    ASSERT0(src->is_uint() || src->is_bool() ||
        src->is_ptr() || src->is_mc());
    if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        //unsign int -> double.
        //bool -> double.
        return getCG()->m_builtin_floatunsidf;
    }

    ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    //unsign long long -> double.
    return getCG()->m_builtin_floatundidf;
}


VAR const* ARMIR2OR::fp2fp(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_fp());
    UINT tgtsz = tgt->getTypeSize(m_tm);
    UINT srcsz = src->getTypeSize(m_tm);
    DUMMYUSE(srcsz);
    ASSERTN(tgtsz != srcsz, ("CVT is redundant"));

    if (tgtsz == GENERAL_REGISTER_SIZE) {
        ASSERT0(srcsz == GENERAL_REGISTER_SIZE * 2);
        return getCG()->m_builtin_truncdfsf2;
    }

    ASSERT0(tgtsz == srcsz * 2);
    return getCG()->m_builtin_extendsfdf2;
}


void ARMIR2OR::convertCvt(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_cvt() && CVT_exp(ir));
    if ((ir->is_int() || ir->is_ptr()) &&
        (CVT_exp(ir)->is_int() || CVT_exp(ir)->is_ptr())) {
        IR2OR::convertCvt(ir, ors, cont);
        return;
    }
    ASSERTN(!ir->is_any() && !CVT_exp(ir)->is_any(),
            ("Unsupported CVT from void"));
    ORList tors;
    IOC tmp;
    convertGeneralLoad(CVT_exp(ir), tors, &tmp);
    SR * opnd = tmp.get_reg(0);
    ASSERT0(CVT_exp(ir)->getTypeSize(m_tm) <= opnd->getByteSize());

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, tors, 1,
        opnd, NULL, opnd->getByteSize(), ::getDbx(CVT_exp(ir)));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    VAR const* builtin = NULL;
    if (ir->is_int()) {
        builtin = fp2int(ir, CVT_exp(ir));
    } else if (ir->is_fp()) {
        if (CVT_exp(ir)->is_int() || CVT_exp(ir)->is_ptr()) {
            builtin = int2fp(ir, CVT_exp(ir));
        } else {
            ASSERT0(CVT_exp(ir)->is_fp());
            builtin = fp2fp(ir, CVT_exp(ir));
        }
    } else { UNREACHABLE(); }

    ASSERT0(builtin);
    getCG()->buildCall(builtin, ir->getTypeSize(m_tm), tors, cont);

    //Result register.
    ASSERT0(cont);
    if (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, getCG()->gen_r0());
    } else {
        ASSERT0(ir->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //SR * res0 = getCG()->genReg();
        //SR * res1 = getCG()->genReg();
        //getCG()->buildMove(res0, getCG()->gen_r0(), tors, NULL);
        //getCG()->buildMove(res1, getCG()->gen_r1(), tors, NULL);

        SR * res0 = getCG()->gen_r0();
        SR * res1 = getCG()->gen_r1();
        getCG()->getSRVecMgr()->genSRVec(2, res0, res1);
        cont->clean_regvec();
        cont->set_reg(RESULT_REGISTER_INDEX, res0);
        //cont->set_reg(1, res1); //Is it indispensable?
    }

    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertReturn(IR const* ir, OUT ORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_return());
    IR * exp = RET_exp(ir);
    if (exp == NULL) {
        OR * o = getCG()->buildOR(OR_ret, 0, 2, getCG()->genTruePred(),
            getCG()->genReturnAddr());
        copyDbx(o, ir);
        ors.append_tail(o);
        return;
    }

    ORList tors;
    ASSERT0(IR_next(exp) == NULL);
    IOC tmp;
    convert(exp, tors, &tmp);
    SR * r0 = getCG()->gen_r0();

    if (exp->getTypeSize(m_tm) >
        NUM_OF_RETURN_VAL_REGISTERS * GENERAL_REGISTER_SIZE) {
        SR * srcaddr = tmp.get_addr();
        ASSERT0(srcaddr && SR_is_reg(srcaddr));

        //Copy return-value to buffer.
        VAR const* retbuf = m_ru->findFormalParam(0);
        ASSERT0(retbuf &&
            retbuf->getByteSize(m_tm) == exp->getTypeSize(m_tm));
        tmp.clean_bottomup();
        getCG()->buildLda(retbuf, 0, NULL, tors, &tmp);
        SR * retbufaddr = tmp.get_reg(0);
        ASSERT0(retbufaddr && SR_is_reg(retbufaddr));
        getCG()->buildMemcpyInternal(
            retbufaddr, srcaddr, exp->getTypeSize(m_tm), tors, &tmp);
        OR * o = getCG()->buildOR(OR_ret, 0, 2,
            getCG()->genTruePred(), getCG()->genReturnAddr());
        tors.append_tail(o);
    } else {
        SR * retv = tmp.get_reg(0);
        ASSERT0(retv && SR_is_reg(retv));
        getCG()->buildMove(r0, retv, tors, NULL);
        ASSERT0(cont);
        cont->set_reg(RESULT_REGISTER_INDEX, r0);
        OR * o;
        if (exp->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
            o = getCG()->buildOR(OR_ret1, 0, 3,
                getCG()->genTruePred(), getCG()->genReturnAddr(), r0);
        } else {
            ASSERTN(SR_vec(retv) && SR_vec_idx(retv) == 0,
                ("it should be the first SR in vector"));
            SR * retv_2 = SR_vec(retv)->get(1);
            SR * r1 = getCG()->gen_r1();
            getCG()->buildMove(r1, retv_2, tors, NULL);
            o = getCG()->buildOR(OR_ret2, 0, 4,
                getCG()->genTruePred(), getCG()->genReturnAddr(), r0, r1);
        }
        tors.append_tail(o);
    }
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


//Some target machine applys comparing instruction to calculate boolean.
//e.g:
//    r1 = 0x2,
//    r2 = 0x2,
//    r0 = eq, r1, r2 ;then r0 is 1.
OR_TYPE ARMIR2OR::mapIRType2ORType(
        IR_TYPE ir_type,
        UINT ir_opnd_size,
        IN SR * opnd0,
        IN SR * opnd1,
        bool is_signed)
{
    DUMMYUSE(opnd0);
    OR_TYPE orty = OR_UNDEF;
    switch (ir_type) {
    case IR_ADD:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
            if (is_signed) {
                orty = OR_add;
            } else {
                orty = OR_add;
            }
        } else if (SR_is_int_imm(opnd1)) {
            if (getCG()->isValidImmOpnd(OR_add_i, SR_is_int_imm(opnd1))) {
                orty = OR_add_i;
            }
        }
        break;
    case IR_SUB:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
            if (is_signed) {
                orty = OR_sub;
            } else {
                orty = OR_sub;
            }
        } else if (SR_is_int_imm(opnd1)) {
            if (getCG()->isValidImmOpnd(OR_sub_i, SR_is_int_imm(opnd1))) {
                orty = OR_sub_i;
            }
        }
        break;
    case IR_MUL:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
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
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
            orty = OR_and;
        } else if (SR_is_int_imm(opnd1)) {
            if (getCG()->isValidImmOpnd(OR_and_i, SR_is_int_imm(opnd1))) {
                orty = OR_and_i;
            }
        }
        break;
    case IR_BOR:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
            orty = OR_orr;
        } else if (SR_is_int_imm(opnd1)) {
            if (getCG()->isValidImmOpnd(OR_orr_i, SR_is_int_imm(opnd1))) {
                orty = OR_orr_i;
            }
        }
        break;
    case IR_XOR:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
        if (SR_is_reg(opnd1)) {
            orty = OR_eor;
        } else if (SR_is_int_imm(opnd1)) {
            orty = OR_eor_i;
        }
        break;
    case IR_BNOT:
        break;
    case IR_NEG:
        if (SR_is_reg(opnd0)) {
            orty = OR_neg;
        }
        break;
    case IR_LT:
        ASSERT0(SR_is_reg(opnd0) && opnd1);
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
