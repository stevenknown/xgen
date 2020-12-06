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
#include "../xgen/xgeninc.h"

//Process binary operation, the function generate OR that consist of two
//operands and one result.
void ARMIR2OR::convertBinaryOp(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERTN(ir->isBinaryOp() && BIN_opnd0(ir) && BIN_opnd1(ir),
            ("missing operand"));
    ASSERTN(!ir->is_vec(), ("TODO"));

    //TO BE DETERMINED:Inequal byte size loading should be handled.
    //ASSERT0((BIN_opnd0(ir)->getTypeSize(m_tm) ==
    //         BIN_opnd1(ir)->getTypeSize(m_tm)) ||
    //        (BIN_opnd0(ir)->is_ptr() || BIN_opnd1(ir)->is_ptr()));

    if (!ir->is_add() && !ir->is_sub()) {
        IR2OR::convertBinaryOp(ir, ors, cont);
        return;
    }

    //Add, sub are more complex opertions.
    if (ir->is_fp()) {
        convertAddSubFp(ir, ors, cont);
        return;
    }

    if (BIN_opnd0(ir)->getTypeSize(m_tm) > DWORD_LENGTH_OF_TARGET_MACHINE ||
        BIN_opnd1(ir)->getTypeSize(m_tm) > DWORD_LENGTH_OF_TARGET_MACHINE) {
        //ADD may be vector-add or simulated-add-call.
        ASSERTN(0, ("TODO"));
    }

    //Load Operand0
    IOC tmp;
    convert(BIN_opnd0(ir), ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());

    //Load Operand1
    tmp.clean();
    convert(BIN_opnd1(ir), ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());
    ASSERT0(opnd0->getByteSize() == opnd1->getByteSize());

    RecycORList tors(this);
    switch (ir->getCode()) {
    case IR_ADD:
        getCG()->buildAdd(opnd0, opnd1, BIN_opnd0(ir)->getTypeSize(m_tm),
                          ir->is_signed(), tors.getList(), cont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        break;
    case IR_SUB:
        getCG()->buildSub(opnd0, opnd1, BIN_opnd0(ir)->getTypeSize(m_tm),
                          ir->is_signed(), tors.getList(), cont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        break;
    default: UNREACHABLE();
    }
}


//Generate operations: reg = &var
void ARMIR2OR::convertLda(Var const* var, HOST_INT lda_ofst, Dbx const* dbx,
                          OUT RecycORList & ors, IN IOC * cont)
{
    getCG()->buildLda(var, lda_ofst, dbx, ors.getList(), cont);
}


void ARMIR2OR::convertStoreVar(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_st());
    UINT resbytesize = ir->getTypeSize(m_tm);
    if (resbytesize <= BYTESIZE_OF_DWORD) {
        IR2OR::convertStoreVar(ir, ors, cont);
        return;
    }

    ASSERT0(ST_rhs(ir)->is_ld());
    ASSERT0(((HOST_INT)ST_rhs(ir)->getTypeSize(m_tm)) == resbytesize);

    //Load the address of target Var into register.
    cont->clean_bottomup();
    convertLda(ST_idinfo(ir), ST_ofst(ir), ::getDbx(ir), ors, cont);
    SR * tgt = cont->get_reg(0);
    ASSERT0(tgt && tgt->is_reg());

    //Load the address of source Var into register.
    cont->clean_bottomup();
    convertLda(LD_idinfo(ST_rhs(ir)), LD_ofst(ST_rhs(ir)),
        ::getDbx(ST_rhs(ir)), ors, cont);
    SR * src = cont->get_reg(0);
    ASSERT0(src && src->is_reg());

    //Generate ::memcpy.
    RecycORList tors(this);
    cont->clean_bottomup();
    getCG()->buildMemcpy(tgt, src, resbytesize, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


//Generate operations: reg = &var
void ARMIR2OR::convertLda(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_lda());
    Var * v = LDA_idinfo(ir);
    ASSERT0(v);
    ASSERTN(!VAR_is_unallocable(v), ("var must be allocable during CG"));
    convertLda(v, LDA_ofst(ir), ::getDbx(ir), ors, cont);
}


void ARMIR2OR::convertReturnValue(IR const* ir, OUT RecycORList & ors,
                                  IN IOC * cont)
{
    if (ir == nullptr) { return; }
    ASSERTN(ir->get_next() == nullptr, ("ARM only support C language."));
    ASSERT0(ir->isCallStmt());
    if (!ir->hasReturnValue()) {
        return;
    }
    IOC tmp_cont;
    SR * retv = nullptr;
    if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        retv = getCG()->gen_r0();
    } else if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        retv = getCG()->getSRVecMgr()->genSRVec(2, getCG()->gen_r0(),
                                                getCG()->gen_r1());
    } else {
        //Get the first formal parameter, it is the return buffer of the value.
        Var const* v = getCG()->get_param_vars().get(0);
        ASSERT0(v);
        DUMMYUSE(v);
        ASSERTN(!g_gen_code_for_big_return_value, ("TODO"));
        return;
    }
    convertCopyPR(ir, retv, ors, cont);
}


void ARMIR2OR::convertCall(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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

    RecycORList tors(this);
    UINT retv_sz = GENERAL_REGISTER_SIZE;

    if (ir->hasReturnValue()) {
        retv_sz = ir->getTypeSize(m_tm);
    }

    if (ir->is_call()) {
        getCG()->buildCall(CALL_idinfo(ir), retv_sz, tors.getList(), cont);
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
        getCG()->buildICall(tgtsr, retv_sz, tors.getList(), cont);
    }
    tors.copyDbx(ir);
    ors.append_tail(tors);

    convertReturnValue(ir, ors, cont);
}


void ARMIR2OR::convertRem(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_rem(), ("illegal ir"));
    ASSERTN(BIN_opnd0(ir) != nullptr && BIN_opnd1(ir) != nullptr,
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
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    RecycORList tors(this);
    UINT retv_sz = ir->getTypeSize(m_tm);
    Var const* builtin = nullptr;
    if (retv_sz <= BYTESIZE_OF_WORD) {
        if (ir->is_uint()) {
            //builtin = getBuiltinVar(BUILTIN_UIMOD);
            builtin = getBuiltinVar(BUILTIN_UMODSI3);
        } else if (ir->is_sint()) {
            builtin = getBuiltinVar(BUILTIN_MODSI3);
        } else {
            UNREACHABLE();
        }
    } else if (retv_sz <= BYTESIZE_OF_DWORD) {
        if (ir->is_uint()) {
            builtin = getBuiltinVar(BUILTIN_UMODDI3);
        } else if (ir->is_sint()) {
            builtin = getBuiltinVar(BUILTIN_MODDI3);
        } else {
            UNREACHABLE();
        }
    } else {
        UNREACHABLE();
    }
    getCG()->buildCall(builtin, retv_sz, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertAddSubFp(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
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
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());
    ASSERT0(opnd0->getByteSize() == op0->getTypeSize(m_tm));

    //Convert Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());
    ASSERT0(opnd1->getByteSize() == op1->getTypeSize(m_tm));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    UINT retv_sz = ir->getTypeSize(m_tm);
    Var const* builtin = nullptr;
    if (retv_sz <= BYTESIZE_OF_WORD) {
        if (ir->is_add()) {
            builtin = getBuiltinVar(BUILTIN_ADDSF3);
        } else {
            builtin = getBuiltinVar(BUILTIN_SUBSF3);
        }
    } else if (retv_sz <= BYTESIZE_OF_DWORD) {
        if (ir->is_add()) {
            builtin = getBuiltinVar(BUILTIN_ADDDF3);
        } else {
            builtin = getBuiltinVar(BUILTIN_SUBDF3);
        }
    } else { UNREACHABLE(); }

    RecycORList tors(this);
    getCG()->buildCall(builtin, retv_sz, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertDiv(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());

    //Convert Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    UINT retv_sz = ir->getTypeSize(m_tm);
    Var const* builtin = nullptr;
    if (retv_sz <= BYTESIZE_OF_WORD) {
        if (op0->is_uint()) {
            builtin = getBuiltinVar(BUILTIN_UDIVSI3);
        } else if (op0->is_sint()) {
            builtin = getBuiltinVar(BUILTIN_DIVSI3);
        } else if (op0->is_fp()) {
            builtin = getBuiltinVar(BUILTIN_DIVSF3);
        } else { UNREACHABLE(); }
    } else if (retv_sz <= BYTESIZE_OF_DWORD) {
        if (op0->is_uint()) {
            builtin = getBuiltinVar(BUILTIN_UDIVDI3);
        } else if (op0->is_sint()) {
            builtin = getBuiltinVar(BUILTIN_DIVDI3);
        } else if (op0->is_fp()) {
            builtin = getBuiltinVar(BUILTIN_DIVDF3);
        } else { UNREACHABLE(); }
    } else { UNREACHABLE(); }

    RecycORList tors(this);
    getCG()->buildCall(builtin, retv_sz, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMulofInt(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
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
    RecycORList tors(this);
    getCG()->buildMul(sr1, sr2, GENERAL_REGISTER_SIZE, true,
                      tors.getList(), cont);
    if (dbx != nullptr) {
        tors.copyDbx(dbx);
    }
    ors.append_tail(tors);
}


//CALL __muldi3
void ARMIR2OR::convertMulofLongLong(IR const* ir, OUT RecycORList & ors,
                                    IN IOC * cont)
{
    ASSERT0(ir->is_mul());
    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);
    ASSERT0(op0->getTypeSize(m_tm) == BYTESIZE_OF_DWORD ||
            op1->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);

    //Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    ASSERT0(tmp.get_addr() == nullptr);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg() && opnd0->is_vec());

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    ASSERT0(tmp.get_addr() == nullptr);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg() && opnd1->is_vec());

    ASSERT0(opnd0->getByteSize() == opnd1->getByteSize());
    //TO BE DETERMINED:Inequal byte size loading should be handled.
    //ASSERT0(BIN_opnd0(ir)->getTypeSize(m_tm) ==
    //        BIN_opnd1(ir)->getTypeSize(m_tm));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    RecycORList tors(this);
    getCG()->buildCall(getBuiltinVar(BUILTIN_MULDI3), ir->getTypeSize(m_tm),
                       tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMulofFloat(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont)
{
    ASSERT0(ir->is_mul());

    IR * op0 = BIN_opnd0(ir);
    IR * op1 = BIN_opnd1(ir);

    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));
    ASSERT0(op0->getTypeSize(m_tm) == BYTESIZE_OF_WORD ||
            op0->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);

    //Operand0
    IOC tmp;
    convert(op0, ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());

    #ifdef _DEBUG_
    if (op0->getTypeSize(m_tm) > BYTESIZE_OF_WORD &&
        op0->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        ASSERT0(opnd0->is_vec());
    }
    #endif

    //Operand1
    tmp.clean();
    convert(op1, ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());

    #ifdef _DEBUG_
    if (op1->getTypeSize(m_tm) > BYTESIZE_OF_WORD &&
        op1->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        ASSERT0(opnd1->is_vec());
    }
    #endif

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, ors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    //Intrinsic Call.
    Var const* builtin;
    if (op0->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        builtin = getBuiltinVar(BUILTIN_MULSF3);
    } else {
        builtin = getBuiltinVar(BUILTIN_MULDF3);
    }

    RecycORList tors(this);
    getCG()->buildCall(builtin, ir->getTypeSize(m_tm), tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void ARMIR2OR::convertMul(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_mul(), ("illegal ir"));
    IR * op0 = BIN_opnd0(ir);
    IR * or1 = BIN_opnd1(ir);
    CHECK0_DUMMYUSE(or1);
    //TO BE DETERMINED:Ineuqal byte size loading should be handled.
    //ASSERT0(op0->getTypeSize(m_tm) == or1->getTypeSize(m_tm));
    if (op0->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        if (op0->is_int()) {
            convertMulofInt(ir, ors, cont);
        } else if (op0->is_fp()) {
            convertMulofFloat(ir, ors, cont);
        }
    } else if (op0->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        if (op0->is_int()) {
            convertMulofLongLong(ir, ors, cont);
        } else if (op0->is_fp()) {
            convertMulofFloat(ir, ors, cont);
        }
    } else {
        ASSERTN(0, ("unsupport"));
    }
}


void ARMIR2OR::convertNeg(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_neg(), ("illegal ir"));
    IR * opnd = UNA_opnd(ir);
    if (opnd->is_const()) {
        ASSERT0(opnd->is_const() && opnd->is_int());
        if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
            SR * res = getCG()->genIntImm((HOST_INT)-CONST_int_val(opnd), true);
            ASSERT0(cont != nullptr);
            cont->set_reg(RESULT_REGISTER_INDEX, res);
        } else if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
            ASSERT0(sizeof(LONGLONG) == BYTESIZE_OF_DWORD);
            LONGLONG v = -CONST_int_val(opnd);
            SR * res = getCG()->genIntImm((HOST_INT)v, true);
            SR * res2 = getCG()->genIntImm((HOST_INT)(v >> 32), true);
            ASSERT0(cont != nullptr);
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
    if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        SR * res = tc.get_reg(0);
        OR * o = getCG()->buildOR(OR_neg, 1, 2,
                                  res, getCG()->getTruePred(), res);
        copyDbx(o, ir);
        ors.append_tail(o);
        ASSERT0(cont != nullptr);
        cont->set_reg(RESULT_REGISTER_INDEX, res);
    } else if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        SR * res = tc.get_reg(0);
        ASSERT0(res); //result0
        ASSERT0(SR_vec(res)->get(1)); //result1
        SR * src = getCG()->genZero();
        SR * src2 = getCG()->genZero();
        RecycORList tors(this);
        getCG()->getSRVecMgr()->genSRVec(2, src, src2);
        getCG()->buildSub(src, res, BYTESIZE_OF_DWORD, true,
                          tors.getList(), &tc);
        tors.copyDbx(ir);
        ors.append_tail(tors);
        ASSERT0(cont && tc.get_reg(0)->getByteSize() == BYTESIZE_OF_DWORD);
        res = tc.get_reg(0);
        cont->set_reg(RESULT_REGISTER_INDEX, res);

        //SR * res2 = SR_vec(res)->get(1); //get result2
        //cont->set_reg(1, res2);
    } else {
        UNREACHABLE();
    }
}


//Inverted the result value, e.g: 1->0 or 0->1.
void ARMIR2OR::invertBoolValue(Dbx * dbx, SR * val, OUT RecycORList & ors)
{
    ASSERT0(val);
    SR * one = getCG()->genIntImm(1, false);
    OR_TYPE orty = m_cg->mapIRType2ORType(IR_XOR, m_tm->getDTypeByteSize(D_B),
                                          val, one, false);
    ASSERTN(orty != OR_UNDEF,
            ("mapIRType2ORType() can not find proper operation"));

    OR * o;
    if (HAS_PREDICATE_REGISTER) {
        ASSERT0(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 3);
        o = getCG()->buildOR(orty, 1, 3, val,
                             getCG()->getTruePred(), val, one);
    } else {
        ASSERT0(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 2);
        o = getCG()->buildOR(orty, 1, 3, val, val, one);
    }
    if (dbx != nullptr) {
        OR_dbx(o).copy(*dbx);
    }
    ors.append_tail(o);
}


void ARMIR2OR::getResultPredByIRTYPE(IR_TYPE code, SR ** truepd,
                                     SR ** falsepd, bool is_signed)
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
void ARMIR2OR::convertRelationOpDWORD(IR const* ir,
                                      OUT RecycORList & ors,
                                      IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());

    IR const* opnd0 = BIN_opnd0(ir);
    IR const* opnd1 = BIN_opnd1(ir);

    ASSERT0(opnd0->getTypeSize(m_tm) == opnd1->getTypeSize(m_tm));

    //Integer, dould size of GENERAL_REGISTER_SIZE.
    ASSERT0(opnd0->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);

    RecycORList tors(this);
    IOC tmp;
    //Operands 0
    convertGeneralLoad(opnd0, tors, &tmp);
    SR * sr0 = tmp.get_reg(0);
    ASSERT0(sr0->is_vec());

    //Operands 1
    tmp.clean();
    convertGeneralLoad(opnd1, tors, &tmp);
    SR * sr1 = tmp.get_reg(0);
    ASSERT0(sr1->is_vec());

    ASSERT0(sr0->getByteSize() == opnd0->getTypeSize(m_tm));
    ASSERT0(sr0->getByteSize() == sr1->getByteSize());

    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    getResultPredByIRTYPE(ir->getCode(), &truepd, &falsepd,
                          opnd0->is_signed());

    //Comparison algo:
    //  compare is <sr1,sr2>, <sr3,sr4> equal:
    //  cmp sr2, sr4
    //  ifeq cmp sr1, sr3
    //  ifle cmp sr2, sr4

    //Compare high part equality.
    //truepd, falsepd = cmp sr0, sr1
    getCG()->buildARMCmp(OR_cmp, getCG()->getTruePred(),
                         SR_vec(sr0)->get(1), SR_vec(sr1)->get(1),
                         tors.getList(), cont);

    //Compare low part if high part is equal.
    getCG()->buildARMCmp(OR_cmp, getCG()->genEQPred(),
                         SR_vec(sr0)->get(0), SR_vec(sr1)->get(0),
                         tors.getList(), cont);

    //Recompare high part if falsepd is true.
    getCG()->buildARMCmp(OR_cmp, falsepd, SR_vec(sr0)->get(1),
                         SR_vec(sr1)->get(1), tors.getList(), cont);

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
void ARMIR2OR::convertRelationOp(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont)
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
    getCG()->buildARMCmp(OR_cmp, getCG()->getTruePred(), sr0, sr1,
                         ors.getList(), cont);
    cont->set_reg(RESULT_REGISTER_INDEX, nullptr);
    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    getResultPredByIRTYPE(ir->getCode(), &truepd, &falsepd,
                          opnd0->is_signed());

    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);

    //if (!ir->getParent()->isConditionalBr()) {
    //    SR * res = getCG()->genReg();
    //    RecycORList tors(this);
    //    getCG()->buildMove(res, getCG()->gen_one(), tors.getList(), cont);
    //    tors.set_pred(truepd, m_cg));
    //    tors.copyDbx(ir);
    //    ors.append_tail(tors);

    //    tors.clean();
    //    getCG()->buildMove(res, getCG()->genZero(), tors.getList(), cont);
    //    tors.set_pred(falsepd, m_cg);
    //    tors.copyDbx(ir);
    //    ors.append_tail(tors);

    //    //used by non-conditional op
    //    cont->set_reg(RESULT_REGISTER_INDEX, res);
    //    return;
    //}

    ////record result
    ////used by convertSelect
    //cont->set_reg(RESULT_REGISTER_INDEX, getCG()->genPredReg());
    //
    ////record true-result predicator
    ////used by convertSelect
    //cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd);

    ////record false-result predicator
    ////used by convertSelect
    //cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd);

    ////record true-result
    //cont->set_pred(truepd, m_cg);

    //IOC_is_inverted(cont) = false;
}


//Convert Bitwise NOT into OR list. bnot is unary operation.
//e.g BNOT(0x0001) = 0xFFFE
//Note ONLY thumb supports ORN logical OR NOT operation.
//This implementation is just used to verify the function of BNOT,
//in the sake of excessive redundant operations has been generated.
void ARMIR2OR::convertBitNotLowPerformance(IR const* ir,
                                           OUT RecycORList & ors,
                                           IN IOC * cont)
{
    ASSERTN(ir->is_bnot(), ("illegal ir"));
    //Algo:
    //  bnot res, src
    // =>
    //  res = movi res, -1
    //  res = xor src, res
    RecycORList tors(this);
    IR * movi_ir = m_rg->buildStorePR(ir->getType(),
                                      m_rg->buildImmInt(-1, ir->getType()));
    convert(movi_ir, tors, cont);

    cont->clean_bottomup();
    IR * mov_ir = m_rg->buildStorePR(ir->getType(),
        m_rg->buildBinaryOpSimp(IR_XOR, UNA_opnd(ir)->getType(),
            m_rg->dupIRTree(UNA_opnd(ir)), m_rg->buildPRdedicated(
                movi_ir->getPrno(), movi_ir->getType())));

    convert(mov_ir, tors, cont);

    ASSERT0(cont != nullptr);
    ASSERTN(cont->get_reg(0) && cont->get_reg(0)->is_reg(), ("check tgt reg"));

    tors.copyDbx(ir);
    ors.append_tail(tors);
    m_rg->freeIRTree(mov_ir);
}


//Convert Bitwise NOT into OR list. bnot is unary operation.
//e.g BNOT(0x0001) = 0xFFFE
//Note ONLY thumb supports ORN logical OR NOT operation.
void ARMIR2OR::convertBitNot(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
{
    ASSERTN(ir->is_bnot(), ("illegal ir"));
    //Algo:
    //  bnot res, opnd
    // =>
    // 1. res = movi res, -1
    // 2. res = xor opnd, res
    RecycORList tors(this);

    // 1. res = movi res, -1
    // 1.1 Generate result SR group.
    UINT res_size = ir->getTypeSize(m_tm);
    SR * res_sr0 = nullptr;
    SR * res_sr1 = nullptr;
    if (res_size <= BYTESIZE_OF_WORD) {
        res_sr0 = m_cg->genReg();

        // 1.2 Generate move operation.
        IOC tmp;
        m_cg->buildMove(res_sr0, getCG()->genIntImm((HOST_INT)-1, false),
                        tors.getList(), &tmp);
    } else if (res_size <= BYTESIZE_OF_DWORD) {
        res_sr0 = m_cg->genReg();
        res_sr1 = m_cg->genReg();
        m_cg->getSRVecMgr()->genSRVec(2, res_sr0, res_sr1);
        // 1.2 Generate move operation.
        IOC tmp;
        m_cg->buildMove(res_sr0, getCG()->genIntImm((HOST_INT)-1, false),
                        tors.getList(), &tmp);

        //The second mov-imm seems redundant. It is not necessary.
        //m_cg->buildMove(res_sr1, getCG()->genIntImm((HOST_INT)-1, false),
        //                tors.getList(), &tmp);
    } else {
        UNREACHABLE();
    }

    // 2. res = xor opnd, res
    //Load operand
    IOC tmp2;
    convert(UNA_opnd(ir), tors, &tmp2);
    SR * opnd = tmp2.get_reg(0);
    ASSERT0(opnd && opnd->is_reg());

    // 2.1 Make sure we have an anticipated or-type.
    OR_TYPE orty = m_cg->mapIRType2ORType(IR_XOR,
                                          UNA_opnd(ir)->getTypeSize(m_tm),
                                          res_sr0, opnd, ir->is_signed());
    ASSERTN(orty != OR_UNDEF,
            ("mapIRType2ORType() can not find properly target"
             "instruction, you should handle this situation."));

    // 2.2 Build XOR operation.
    ASSERTN(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 3,
            ("not binary operation"));
    if (res_size <= BYTESIZE_OF_WORD) {
        OR * o = m_cg->buildOR(orty, 1, 3, res_sr0,
                               m_cg->getTruePred(), opnd, res_sr0);
        tors.append_tail(o);
    } else if (res_size <= BYTESIZE_OF_DWORD) {
        OR * o = m_cg->buildOR(orty, 1, 3, res_sr0,
                               m_cg->getTruePred(), opnd, res_sr0);
        tors.append_tail(o);

        ASSERT0(res_sr1);
        ASSERT0(opnd->is_group() && opnd->getVecIdx() == 0);
        SR * opnd_2 = opnd->getVec()->get(1);

        ASSERT0(opnd_2->getVecIdx() == 1);
        o = m_cg->buildOR(orty, 1, 3, res_sr1,
                          m_cg->getTruePred(), opnd_2,
                          res_sr0); //use res_sr0 to save one operation.
        tors.append_tail(o);
    } else {
        UNREACHABLE();
    }

    // 3.3 Set predicate register if any.
    tors.set_pred(m_cg->getTruePred(), m_cg);
    tors.copyDbx(ir);

    ASSERT0(cont != nullptr);
    cont->set_reg(0, res_sr0);
    if (res_sr1 != nullptr) {
        cont->set_reg(1, res_sr1);
    }
    ors.append_tail(tors);
}


void ARMIR2OR::recordRelationOpResult(IR const* ir, SR * truepd,
                                      SR * falsepd, SR * result_pred,
                                      OUT RecycORList & ors,
                                      IN OUT IOC * cont)
{
    if (!ir->getParent()->isConditionalBr()) {
        SR * res = getCG()->genReg();
        RecycORList tors(this);
        getCG()->buildMove(res, getCG()->genOne(), tors.getList(), cont);
        tors.set_pred(truepd, m_cg);
        ors.append_tail(tors);

        tors.clean();
        getCG()->buildMove(res, getCG()->genZero(), tors.getList(), cont);
        tors.set_pred(falsepd, m_cg);
        ors.append_tail(tors);

        cont->set_reg(RESULT_REGISTER_INDEX, res); //used by non-conditional op

        //truepd, falsepd and result_pred are useless.
        return;
    }

    //Record result.
    //used by Non-Conditional-Op, such as convertSelect.
    cont->set_reg(RESULT_REGISTER_INDEX, nullptr);

    //Record true-result predicator
    cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd);

    //Record false-result predicator    
    cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd);

    //Record true-result.
    cont->set_pred(result_pred);

    IOC_is_inverted(cont) = false;
}


void ARMIR2OR::convertIgoto(IR const* ir, OUT RecycORList & ors, IN IOC *)
{
    ASSERT0(ir->is_igoto());
    IOC tmp;
    convertLoadVar(IGOTO_vexp(ir), ors, &tmp);
    SR * tgt_addr = tmp.get_reg(0);
    ASSERT0(tgt_addr);

    OR * o = getCG()->buildOR(OR_bx, 0, 2, getCG()->getTruePred(), tgt_addr);
    OR_dbx(o).copy(*getDbx(ir));
    ors.append_tail(o);
}


void ARMIR2OR::convertSelect(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
{
    ASSERT0(ir->is_select());
    IOC tmp;
    RecycORList tors(this);

    //Generate Compare operation
    convertRelationOp(SELECT_pred(ir), tors, &tmp);
    tors.copyDbx(SELECT_pred(ir));
    ors.append_tail(tors);
    SR * cres = tmp.get_reg(0);
    CHECK0_DUMMYUSE(cres);

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
        if (res_size <= BYTESIZE_OF_WORD) {
            SR * sr0 = tmp.get_reg(0);
            getCG()->buildMove(res, sr0, tors.getList(), &tmp);
        } else if (res_size <= BYTESIZE_OF_DWORD) {
            SR * sr0 = tmp.get_reg(0);
            SR * sr1 = nullptr;
            if (SR_vec(sr0) != nullptr && SR_vec(sr0)->get(1) != nullptr) {
                sr1 = SR_vec(sr0)->get(1);
            } else {
                sr1 = getCG()->genReg();
                getCG()->buildMove(sr1, getCG()->genZero(),
                                   tors.getList(), &tmp);
            }
            SR * res2 = getCG()->genReg();
            getCG()->getSRVecMgr()->genSRVec(2, res, res2);
            getCG()->buildMove(res, sr0, tors.getList(), &tmp);
            getCG()->buildMove(res2, sr1, tors.getList(), &tmp);
        } else {
            UNREACHABLE();
        }
        tors.set_pred(true_pr, m_cg);
        tors.copyDbx(SELECT_trueexp(ir));
        ors.append_tail(tors);
    }

    //False exp value
    {
        tmp.clean();
        tors.clean();
        convertGeneralLoad(SELECT_falseexp(ir), tors, &tmp);

        if (res_size <= BYTESIZE_OF_WORD) {
            SR * sr0 = tmp.get_reg(0);
            getCG()->buildMove(res, sr0, tors.getList(), &tmp);
        } else if (res_size <= BYTESIZE_OF_DWORD) {
            SR * sr0 = tmp.get_reg(0);
            SR * sr1 = nullptr;
            if (SR_vec(sr0) != nullptr && SR_vec(sr0)->get(1) != nullptr) {
                sr1 = SR_vec(sr0)->get(1);
            } else {
                sr1 = getCG()->genReg();
                getCG()->buildMove(sr1, getCG()->genZero(),
                                   tors.getList(), &tmp);
            }

            SR * res2 = getCG()->genReg();
            getCG()->getSRVecMgr()->genSRVec(2, res, res2);
            getCG()->buildMove(res, sr0, tors.getList(), &tmp);
            getCG()->buildMove(res2, sr1, tors.getList(), &tmp);
        } else {
            UNREACHABLE();
        }

        tors.set_pred(false_pr, m_cg);
        tors.copyDbx(SELECT_falseexp(ir));
        ors.append_tail(tors);
    }
    cont->set_reg(RESULT_REGISTER_INDEX, res);
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR,
//TruePredicatedSR, FalsePredicatedSR.
//The ResultSR record the boolean value of comparison of relation operation.
void ARMIR2OR::convertRelationOpFp(IR const* ir, OUT RecycORList & ors,
                                   IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);

    ASSERT0(op0->getTypeSize(m_tm) == op1->getTypeSize(m_tm));

    RecycORList tors(this);

    //Convert Operand0.
    IOC tmp;
    convert(op0, tors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());
    ASSERT0(opnd0->getByteSize() == op0->getTypeSize(m_tm));

    //Convert Operand1.
    tmp.clean();
    convert(op1, tors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());
    ASSERT0(opnd1->getByteSize() == op1->getTypeSize(m_tm));

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, tors.getList(), 2,
                         opnd0, nullptr, opnd0->getByteSize(), ::getDbx(op0),
                         opnd1, nullptr, opnd1->getByteSize(), ::getDbx(op1));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    Var const* builtin = nullptr;
    if (opnd0->getByteSize() == GENERAL_REGISTER_SIZE) {
        switch (ir->getCode()) {
        case IR_LT: builtin = getBuiltinVar(BUILTIN_LTSF2); break;
        case IR_GT: builtin = getBuiltinVar(BUILTIN_GTSF2); break;
        case IR_GE: builtin = getBuiltinVar(BUILTIN_GESF2); break;
        case IR_EQ: builtin = getBuiltinVar(BUILTIN_EQSF2); break;
        case IR_NE: builtin = getBuiltinVar(BUILTIN_NESF2); break;
        case IR_LE: builtin = getBuiltinVar(BUILTIN_LESF2); break;
        default: UNREACHABLE();
        }
    } else {
        ASSERT0(opnd0->getByteSize() == GENERAL_REGISTER_SIZE * 2);
        switch (ir->getCode()) {
        case IR_LT: builtin = getBuiltinVar(BUILTIN_LTDF2); break;
        case IR_GT: builtin = getBuiltinVar(BUILTIN_GTDF2); break;
        case IR_GE: builtin = getBuiltinVar(BUILTIN_GEDF2); break;
        case IR_EQ: builtin = getBuiltinVar(BUILTIN_EQDF2); break;
        case IR_NE: builtin = getBuiltinVar(BUILTIN_NEDF2); break;
        case IR_LE: builtin = getBuiltinVar(BUILTIN_LEDF2); break;
        default: UNREACHABLE();
        }
    }

    getCG()->buildCall(builtin, ir->getTypeSize(m_tm), tors.getList(), cont);

    //Get return value of call.
    //SR * retv = getCG()->genReg();
    //getCG()->buildMove(retv, getCG()->gen_r0(), tors.getList(), cont);

    tors.copyDbx(ir);
    ors.append_tail(tors);
    tors.clean();

    SR * r0 = getCG()->gen_r0();
    SR * one = getCG()->genOne();
    SR * zero = getCG()->genZero();
    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    bool needresval = !ir->getStmt()->isConditionalBr();
    if (needresval) {
        getCG()->buildCompare(OR_cmp_i, true, getCG()->gen_r0(),
                              getCG()->genZero(), tors.getList(), cont);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(lt)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(le)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(gt)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(ge)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(eq)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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
            getCG()->buildMove(r0, zero, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(falsepd, m_cg);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            tors.clean();

            //(ne)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
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


void ARMIR2OR::convertTruebrFp(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERT0(cont->get_pred() == nullptr);
    convertRelationOpFp(BR_det(ir), ors, cont);
    ASSERT0(cont->get_pred());

    SR * retv = cont->get_reg(0);
    ASSERT0(retv && retv->is_reg());

    //Build truebr.
    RecycORList tors(this);
    SR * tgt_lab = getCG()->genLabel(BR_lab(ir));
    getCG()->buildCompare(OR_cmp_i, true, retv,
                          getCG()->genZero(), tors.getList(), cont);
    //cmp does not produce result in register.
    ASSERT0(cont->get_reg(0) == nullptr);
    getCG()->buildCondBr(tgt_lab, tors.getList(), cont);
    tors.copyDbx(ir);
    cont->set_pred(nullptr); //clean status
    ors.append_tail(tors);
}


void ARMIR2OR::convertTruebr(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
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

    RecycORList tors(this);
    getCG()->buildCondBr(getCG()->genLabel(BR_lab(ir)), tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
    return;
}


Var const* ARMIR2OR::fp2int(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_int());
    if (tgt->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        if (tgt->is_sint()) {
            if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
                //float -> sign int.
                return getBuiltinVar(BUILTIN_FIXSFSI);
            }

            ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
            //double -> sign int.
            return getBuiltinVar(BUILTIN_FIXDFSI);
        }

        ASSERT0(tgt->is_uint() || tgt->is_bool());
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> unsign int.
            //float -> bool.
            return getBuiltinVar(BUILTIN_FIXUNSSFSI);
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //double -> unsign int.
        return getBuiltinVar(BUILTIN_FIXUNSDFSI);
    }

    ASSERT0(tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    if (tgt->is_sint()) {
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> sign long long.
            return getBuiltinVar(BUILTIN_FIXSFDI);
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //double -> sign long long.
        return getBuiltinVar(BUILTIN_FIXDFDI);
    }

    ASSERT0(tgt->is_uint());
    if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
         //float -> unsign long long.
         return getBuiltinVar(BUILTIN_FIXUNSSFDI);
    }

    ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    //double -> unsign long long.
    return getBuiltinVar(BUILTIN_FIXUNSDFDI);
}


Var const* ARMIR2OR::int2fp(IR const* tgt, IR const* src)
{
    ASSERT0(src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE * 2
            && tgt->is_fp());
    if (tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
        if (src->is_sint()) {
            if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
                //sign int -> float.
                return getBuiltinVar(BUILTIN_FLOATSISF);
            }

            ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
            //sign long long -> float.
            return getBuiltinVar(BUILTIN_FLOATDISF);
        }

        ASSERT0(src->is_uint() || src->is_bool() ||
            src->is_ptr() || src->is_mc());
        if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
            //unsign int -> float.
            //bool -> float.
            return getBuiltinVar(BUILTIN_FLOATUNSISF);
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);

        //unsign long long -> float.
        return getBuiltinVar(BUILTIN_FLOATUNDISF);
    }

    ASSERT0(tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    if (src->is_sint()) {
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //sign int -> double.
            return getBuiltinVar(BUILTIN_FLOATSIDF);
        }

        ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //sign long long -> double.
        return getBuiltinVar(BUILTIN_FLOATDIDF);
    }

    ASSERT0(src->is_uint() || src->is_bool() || src->is_ptr() || src->is_mc());
    if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        //unsign int -> double.
        //bool -> double.
        return getBuiltinVar(BUILTIN_FLOATUNSIDF);
    }

    ASSERT0(src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
    //unsign long long -> double.
    return getBuiltinVar(BUILTIN_FLOATUNDIDF);
}


Var const* ARMIR2OR::fp2fp(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_fp());
    UINT tgtsz = tgt->getTypeSize(m_tm);
    UINT srcsz = src->getTypeSize(m_tm);
    DUMMYUSE(srcsz);
    ASSERTN(tgtsz != srcsz, ("CVT is redundant"));
    if (tgtsz == GENERAL_REGISTER_SIZE) {
        ASSERT0(srcsz == GENERAL_REGISTER_SIZE * 2);
        return getBuiltinVar(BUILTIN_TRUNCDFSF2);
    }

    ASSERT0(tgtsz == srcsz * 2);
    return getBuiltinVar(BUILTIN_EXTENDSFDF2);
}


void ARMIR2OR::convertCvt(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_cvt() && CVT_exp(ir));

    IR * newir = const_cast<IR*>(ir);
    if (CVT_exp(ir)->is_any()) {
        //Use result of type of CVT as source type because CG must know
        //how many byte of data have to load.
        newir = m_rg->dupIRTree(ir);
        IR_dt(CVT_exp(newir)) = newir->getType();
    }

    if ((newir->is_int() || newir->is_ptr()) &&
        (CVT_exp(newir)->is_int() || CVT_exp(newir)->is_ptr())) {
        IR2OR::convertCvt(newir, ors, cont);
        if (newir != ir) {
            m_rg->freeIRTree(newir);
        }
        return;
    }
    ASSERTN(!newir->is_any(), ("Unsupported CVT to ANY"));

    RecycORList tors(this);
    IOC tmp;
    convertGeneralLoad(CVT_exp(newir), tors, &tmp);
    SR * opnd = tmp.get_reg(0);
    ASSERT0(CVT_exp(newir)->getTypeSize(m_tm) <= opnd->getByteSize());

    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, tors.getList(), 1,
                         opnd, nullptr, opnd->getByteSize(),
                         ::getDbx(CVT_exp(newir)));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    Var const* builtin = nullptr;
    if (newir->is_int()) {
        builtin = fp2int(newir, CVT_exp(newir));
    } else if (newir->is_fp()) {
        if (CVT_exp(newir)->is_int() || CVT_exp(newir)->is_ptr()) {
            builtin = int2fp(newir, CVT_exp(newir));
        } else {
            ASSERT0(CVT_exp(newir)->is_fp());
            builtin = fp2fp(newir, CVT_exp(newir));
        }
    } else { UNREACHABLE(); }

    ASSERT0(builtin);
    getCG()->buildCall(builtin, newir->getTypeSize(m_tm),
                       tors.getList(), cont);

    //Result register.
    ASSERT0(cont);
    if (newir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, getCG()->gen_r0());
    } else {
        ASSERT0(newir->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE * 2);
        //SR * res0 = getCG()->genReg();
        //SR * res1 = getCG()->genReg();
        //getCG()->buildMove(res0, getCG()->gen_r0(), tors.getList(), nullptr);
        //getCG()->buildMove(res1, getCG()->gen_r1(), tors.getList(), nullptr);

        SR * res0 = getCG()->gen_r0();
        SR * res1 = getCG()->gen_r1();
        getCG()->getSRVecMgr()->genSRVec(2, res0, res1);
        cont->clean_regvec();
        cont->set_reg(RESULT_REGISTER_INDEX, res0);
        //cont->set_reg(1, res1); //Is it indispensable?
    }

    tors.copyDbx(ir); //copy dbx from origin ir.
    ors.append_tail(tors);
    if (newir != ir) {
        m_rg->freeIRTree(newir);
    }
}


void ARMIR2OR::convertReturn(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
{
    ASSERT0(ir->is_return());
    IR * exp = RET_exp(ir);
    if (exp == nullptr) {
        OR * o = getCG()->buildOR(OR_ret, 0, 2, getCG()->getTruePred(),
                                  getCG()->genReturnAddr());
        copyDbx(o, ir);
        ors.append_tail(o);
        return;
    }

    RecycORList tors(this);
    ASSERT0(IR_next(exp) == nullptr);
    IOC tmp;
    convert(exp, tors, &tmp);
    SR * r0 = getCG()->gen_r0();

    if (exp->getTypeSize(m_tm) >
        NUM_OF_RETURN_VAL_REGISTERS * GENERAL_REGISTER_SIZE) {
        SR * srcaddr = tmp.get_addr();
        ASSERT0(srcaddr && srcaddr->is_reg());

        //Copy return-value to buffer.
        Var const* retbuf = m_rg->findFormalParam(0);
        ASSERT0(retbuf &&
            retbuf->getByteSize(m_tm) == exp->getTypeSize(m_tm));
        tmp.clean_bottomup();
        getCG()->buildLda(retbuf, 0, nullptr, tors.getList(), &tmp);
        SR * retbufaddr = tmp.get_reg(0);
        ASSERT0(retbufaddr && retbufaddr->is_reg());
        getCG()->buildMemcpyInternal(retbufaddr, srcaddr,
                                     exp->getTypeSize(m_tm),
                                     tors.getList(), &tmp);
        OR * o = getCG()->buildOR(OR_ret, 0, 2,
                                  getCG()->getTruePred(),
                                  getCG()->genReturnAddr());
        tors.append_tail(o);
    } else {
        SR * retv = tmp.get_reg(0);
        ASSERT0(retv && retv->is_reg());
        getCG()->buildMove(r0, retv, tors.getList(), nullptr);
        ASSERT0(cont);
        cont->set_reg(RESULT_REGISTER_INDEX, r0);
        OR * o;
        if (exp->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
            o = getCG()->buildOR(OR_ret1, 0, 3,
                                 getCG()->getTruePred(),
                                 getCG()->genReturnAddr(), r0);
        } else {
            ASSERTN(SR_vec(retv) && SR_vec_idx(retv) == 0,
                    ("it should be the first SR in vector"));
            SR * retv_2 = SR_vec(retv)->get(1);
            SR * r1 = getCG()->gen_r1();
            getCG()->buildMove(r1, retv_2, tors.getList(), nullptr);
            o = getCG()->buildOR(OR_ret2, 0, 4,
                                 getCG()->getTruePred(),
                                 getCG()->genReturnAddr(), r0, r1);
        }
        tors.append_tail(o);
    }
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


IR * ARMIR2OR::insertCvt(IR const* ir)
{
    switch (ir->getCode()) {
    case IR_ST:
    case IR_IST:
    case IR_STPR:
    case IR_STARRAY: {
        UINT tgtsz = ir->getTypeSize(m_tm);
        UINT srcsz = ir->getRHS()->getTypeSize(m_tm);
        if (tgtsz > srcsz && srcsz >= BYTESIZE_OF_WORD) {
            //CASE: Insert IR_CVT to generate zero-extened OR.
            //e.g:
            //  ist:f64 id:80 attachinfo:Dbx
            //    lda:*<800> 'gd' id:12
            //    ld:i32 'i' id:15
            //=> after insertion
            //  ist:f64 id:80 attachinfo:Dbx       
            //    lda:*<800> 'gd' id:12
            //    cvt:f64    
            //      ld:i32 'i' id:15
            IR * newir = m_rg->dupIRTree(ir);
            IR * cvt = m_rg->buildCvt(newir->getRHS(), newir->getType());
            newir->setRHS(cvt);
            return newir;
        }
        break;
    }
    default: break;
    }
    return const_cast<IR*>(ir);
}


void ARMIR2OR::convert(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    IR * newir = insertCvt(ir);
    IR2OR::convert(newir, ors, cont);
    if (newir != ir) {
        m_rg->freeIRTree(newir);
    }
}
