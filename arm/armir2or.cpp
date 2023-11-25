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

//Process binary operation, the function generate OR that consist of two
//operands and one result.
void ARMIR2OR::convertBinaryOp(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERTN(ir->isBinaryOp() && BIN_opnd0(ir) && BIN_opnd1(ir),
            ("missing operand"));
    //TBD:Inequal byte size loading should be handled.
    //ASSERT0((BIN_opnd0(ir)->getTypeSize(m_tm) ==
    //         BIN_opnd1(ir)->getTypeSize(m_tm)) ||
    //        (BIN_opnd0(ir)->is_ptr() || BIN_opnd1(ir)->is_ptr()));

    if (!ir->is_add() && !ir->is_sub()) {
        IR2OR::convertBinaryOp(ir, ors, cont);
        return;
    }

    //Add, sub are more complex opertions.
    if (ir->is_fp()) {
        convertAddSubFP(ir, ors, cont);
        return;
    }
    ASSERTN(!BIN_opnd0(ir)->is_any() && !BIN_opnd1(ir)->is_any(),
            ("operand of '%s' can not be ANY", IRNAME(ir)));
    if (BIN_opnd0(ir)->getTypeSize(m_tm) > DWORD_LENGTH_OF_TARGET_MACHINE ||
        BIN_opnd1(ir)->getTypeSize(m_tm) > DWORD_LENGTH_OF_TARGET_MACHINE) {
        //ADD may be vector-add or simulated-add-call.
        //ASSERTN(0, ("TODO"));
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
        ors.move_tail(tors);
        break;
    case IR_SUB:
        getCG()->buildSub(opnd0, opnd1, BIN_opnd0(ir)->getTypeSize(m_tm),
                          ir->is_signed(), tors.getList(), cont);
        tors.copyDbx(ir);
        ors.move_tail(tors);
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
    UINT resbytesize = ir->is_any() ? 0 : ir->getTypeSize(m_tm);
    if (resbytesize <= BYTESIZE_OF_DWORD) {
        ASSERT0(ir->getRHS()->is_any() ||
                ir->getRHS()->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD);
        IR2OR::convertStoreVar(ir, ors, cont);
        return;
    }
    SR * src = nullptr; //record source address reg.
    IR const* rhs = ST_rhs(ir);
    ASSERT0(((HOST_INT)rhs->getTypeSize(m_tm)) == resbytesize);
    if (rhs->is_ld()) {
        //CASE: st x:mc<200> = ld y:mc<200>
        //Load the address of source Var into register.
        cont->clean_bottomup();
        convertLda(LD_idinfo(rhs), LD_ofst(rhs), ::getDbx(rhs), ors, cont);
        src = cont->get_reg(0);
    } else if (rhs->is_ild()) {
        //CASE: st x:mc<200> = ild(ld(p)):mc<200>
        //Load the address of source via pointer.
        cont->clean_bottomup();
        convertGeneralLoad(ILD_base(rhs), ors, cont);
        src = cont->get_reg(0);
    } else {
        ASSERTN(0, ("support more kind of MC to MC copy"));
    }
    ASSERT0(src && src->is_reg());

    //Load the address of target Var into register.
    cont->clean_bottomup();
    convertLda(ST_idinfo(ir), ST_ofst(ir), ::getDbx(ir), ors, cont);
    SR * tgt = cont->get_reg(0);
    ASSERT0(tgt && tgt->is_reg());

    //Generate ::memcpy.
    RecycORList tors(this);
    cont->clean_bottomup();
    getCG()->buildMemcpy(tgt, src, resbytesize, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.move_tail(tors);
}


//Generate operations: reg = &var
void ARMIR2OR::convertLda(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_lda());
    xoc::Var * v = LDA_idinfo(ir);
    ASSERT0(v);
    ASSERTN(!v->is_unallocable(),
            ("LDA's operand variable must be allocable"));
    convertLda(v, LDA_ofst(ir), xoc::getDbx(ir), ors, cont);
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
    IOC tc;
    SR * retv = nullptr;
    if (ir->is_any() || ir->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        //Note if the result-type of CALL is ANY type, regard it
        //is as long as register.
        retv = getCG()->genR0();
    } else if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        retv = getCG()->getSRVecMgr()->genSRVec(2,
            getCG()->genRegWithPhyReg(REG_R0, RF_R),
            getCG()->genRegWithPhyReg(REG_R1, RF_R));
    } else {
        //Get the first formal parameter, it is the return buffer of the value.
        Var const* v = getCG()->get_param_vars().get(0);
        ASSERT0_DUMMYUSE(v);
        ASSERTN(!g_gen_code_for_big_return_value, ("TODO"));
        return;
    }
    convertCopyPR(ir, retv, ors, cont);
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
        opnd0, nullptr, opnd0->getByteSize(), xoc::getDbx(op0),
        opnd1, nullptr, opnd1->getByteSize(), xoc::getDbx(op1));

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
    ors.move_tail(tors);
}


void ARMIR2OR::convertAddSubFP(IR const* ir, OUT RecycORList & ors,
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
    ors.move_tail(tors);
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
    ors.move_tail(tors);
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
    ors.move_tail(tors);
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
    ors.move_tail(tors);
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
    ors.move_tail(tors);
}


void ARMIR2OR::convertMul(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_mul(), ("illegal ir"));
    IR * op0 = BIN_opnd0(ir);
    IR * or1 = BIN_opnd1(ir);
    ASSERT0_DUMMYUSE(or1);
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


void ARMIR2OR::convertNegImm(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
{
    ASSERTN(ir->is_neg(), ("illegal ir"));
    IR * opnd = UNA_opnd(ir);
    ASSERT0(opnd->is_const());
    IR * newopnd = m_rg->dupIRTree(opnd);
    if (newopnd->is_int()) {
        CONST_int_val(newopnd) = -CONST_int_val(newopnd);
    } else if (newopnd->is_fp()) {
        //CASE:ARMCG can not figure out OR code for neg(const).
        CONST_fp_val(newopnd) = -CONST_fp_val(newopnd);
    } else { UNREACHABLE(); }
    convertLoadConst(newopnd, ors, cont);
}


void ARMIR2OR::convertNeg(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERTN(ir->is_neg(), ("illegal ir"));
    IR * opnd = UNA_opnd(ir);
    if (opnd->is_const()) {
        convertNegImm(ir, ors, cont);
        return;
    }
    IOC tc(*cont);
    IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
    convertGeneralLoad(opnd, ors, &tc);
    if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_WORD) {
        SR * res = tc.get_reg(0);
        OR * o = getCG()->buildOR(OR_neg, 1, 2,
                                  res, getCG()->getTruePred(), res);
        copyDbx(o, ir);
        ors.append_tail(o);
        ASSERT0(cont != nullptr);
        cont->set_reg(RESULT_REGISTER_INDEX, res);
        return;
    }
    if (ir->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD) {
        SR * res = tc.get_reg(0);
        ASSERT0(res); //result0
        ASSERT0(res->getVec() && res->getVec()->get(1)); //result1
        SR * src = getCG()->genZero();
        SR * src2 = getCG()->genZero();
        RecycORList tors(this);
        getCG()->getSRVecMgr()->genSRVec(2, src, src2);
        getCG()->buildSub(src, res, BYTESIZE_OF_DWORD, true,
                          tors.getList(), &tc);
        tors.copyDbx(ir);
        ors.move_tail(tors);
        ASSERT0(cont && tc.get_reg(0)->getByteSize() == BYTESIZE_OF_DWORD);
        res = tc.get_reg(0);
        cont->set_reg(RESULT_REGISTER_INDEX, res);

        //TBD:Do we need store high-part to context.
        //SR * res2 = res->getVec()->get(1); //get result2
        //cont->set_reg(1, res2);
        return;
    }
    UNREACHABLE();
}


//Inverted the result value, e.g: 1->0 or 0->1.
void ARMIR2OR::invertBoolValue(Dbx * dbx, SR * val, OUT RecycORList & ors)
{
    ASSERT0(val);
    SR * one = getCG()->genIntImm(1, false);
    OR_CODE orty = m_cg->mapIRCode2ORCode(IR_XOR, m_tm->getDTypeByteSize(D_B),
        m_tm->getBool(), val, one, false);
    ASSERTN(orty != OR_UNDEF,
            ("mapIRCode2ORCode() can not find proper operation"));

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


void ARMIR2OR::getResultPredByIRCode(IR_CODE code, SR ** truepd,
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


void ARMIR2OR::convertRelationOpDWORDForEquality(IR const* ir,
                                                 SR * sr0, SR * sr1,
                                                 bool is_signed,
                                                 OUT RecycORList & ors,
                                                 OUT RecycORList & tors,
                                                 IN IOC * cont)
{
    ASSERT0(ir->is_eq() || ir->is_ne());
    //Compare high part equality.
    //truepd, falsepd = cmp sr0_h, sr1_h
    SR * sr0_l = sr0->getVec()->get(0);
    SR * sr0_h = sr0->getVec()->get(1);
    SR * sr1_l = sr1->getVec()->get(0);
    SR * sr1_h = sr1->getVec()->get(1);

    //Compare <sr0_l,sr0_h> is equal to <sr1_l,sr1_h>:
    //  truepd is EQ, falsepd is NE
    //  cmp sr0_h, sr1_h
    //  ifeq cmp sr0_l, sr1_l
    //  return res, truepd
    //Compare <sr0_l,sr0_h> is not equal to <sr1_l,sr1_h>:
    //  truepd is NE, falsepd is EQ
    //  cmp sr0_h, sr1_h
    //  ifeq cmp sr0_l, sr1_l
    //  return res, truepd

    getCG()->buildARMCmp(OR_cmp, getCG()->getTruePred(),
                         sr0_h, sr1_h, tors.getList(), cont);

    //Compare low part if high part is equal.
    //truepd, falsepd = (ifeq) cmp sr0_l, sr1_l
    getCG()->buildARMCmp(OR_cmp, getCG()->genEQPred(),
                         sr0_l, sr1_l, tors.getList(), cont);

    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    getResultPredByIRCode(ir->getCode(), &truepd, &falsepd, is_signed);
    tors.copyDbx(ir);
    ors.move_tail(tors);
    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);
}


void ARMIR2OR::convertRelationOpDWORDForLTGELEGT(IR const* ir,
                                                 SR * sr0_l, SR * sr0_h,
                                                 SR * sr1_l, SR * sr1_h,
                                                 bool is_signed,
                                                 OUT RecycORList & ors,
                                                 OUT RecycORList & tors,
                                                 IN IOC * cont)
{
    ASSERT0(ir->is_lt() || ir->is_ge() || ir->is_le() || ir->is_gt());
    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    IR_CODE code;
    //Note truepd/falsepd of IR_GT is same with LT, and
    //truepd/falsepd of IR_LE is same with GE.
    if (ir->is_le()) {
        code = IR_GE;
    } else if (ir->is_gt()) {
        code = IR_LT;
    } else {
        code = ir->getCode();
    }
    getResultPredByIRCode(code, &truepd, &falsepd, is_signed);

    if (is_signed) {
        //SBCS: If S is specified, the SBC instruction updates the
        //N, Z, C and V flags according to the result.
        //
        //Compare <sr0_l,sr0_h> is less-than <sr1_l,sr1_h>:
        //  truepd is LT, falsepd is GE
        //  cmp sr0_l, sr1_l
        //  sbcs res, sr0_h, sr1_h
        //  return res, truepd
        getCG()->buildARMCmp(OR_cmp, m_cg->getTruePred(),
                             sr0_l, sr1_l, tors.getList(), cont);
        OR * o = m_cg->buildOR(OR_sbcs, 2, 4,
                               m_cg->genReg(), m_cg->getRflag(),
                               m_cg->getTruePred(), sr0_h, sr1_h,
                               m_cg->getRflag());
        tors.append_tail(o);
        tors.copyDbx(ir);
        ors.move_tail(tors);
        recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);
        return;
    }

    //Unsigned less-than.
    getCG()->buildARMCmp(OR_cmp, getCG()->getTruePred(),
                         sr0_h, sr1_h, tors.getList(), cont);

    //Compare low part if high part is equal.
    //truepd, falsepd = (ifeq) cmp sr0_l, sr1_l
    getCG()->buildARMCmp(OR_cmp, getCG()->genEQPred(),
                         sr0_l, sr1_l, tors.getList(), cont);

    tors.copyDbx(ir);
    ors.move_tail(tors);
    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);
}


//Info about ARM Conditions Flag that need to know.
//Note LT and GE do not have the completely consistent inverse-behaviors
//compared to LE and GT, namely, it is not correct to replace GT/LE
//combination with GE/LT combination, vice versa.
//e.g: 64bit signed comparation:
//  if (a > b) TrueBody
//  FalseBody:
//
//Correct code ===>:
//  cmp b_lo, a_lo //use subs b_lo, a_lo is also work, because compare
//                 //is substract essentially.
//  sbcs ip, b_hi, a_hi
//  bge FalseBody
//  TrueBody:
//
//Incorrect code, swap a, b ===>:
//  cmp a_lo, b_lo
//  sbcs ip, a_hi, a_hi
//  ble FalseBody
//  TrueBody:
void ARMIR2OR::convertRelationOpDWORDForLTandGE(IR const* ir,
                                                SR * sr0, SR * sr1,
                                                bool is_signed,
                                                OUT RecycORList & ors,
                                                OUT RecycORList & tors,
                                                IN IOC * cont)
{
    ASSERT0(ir->is_lt() || ir->is_ge());
    SR * sr0_l = sr0->getVec()->get(0);
    SR * sr0_h = sr0->getVec()->get(1);
    SR * sr1_l = sr1->getVec()->get(0);
    SR * sr1_h = sr1->getVec()->get(1);

    //SBCS: If S is specified, the SBC instruction updates the
    //N, Z, C and V flags according to the result.
    //
    //Compare <sr0_l,sr0_h> is less-than <sr1_l,sr1_h>:
    //  truepd is LT, falsepd is GE
    //  cmp sr0_l, sr1_l
    //  sbcs res, sr0_h, sr1_h
    //  return res, truepd
    convertRelationOpDWORDForLTGELEGT(ir, sr0_l, sr0_h, sr1_l, sr1_h,
                                      is_signed, ors, tors, cont);
    return;
}


//Info about ARM Conditions Flag that need to know.
//Note LT and GE do not have the completely consistent inverse-behaviors
//compared to LE and GT, namely, it is not correct to replace GT/LE
//combination with GE/LT combination, vice versa.
//e.g: 64bit signed comparation:
//  if (a > b) TrueBody
//  FalseBody:
//
//Correct code ===>:
//  cmp b_lo, a_lo //use subs b_lo, a_lo is also work, because compare
//                 //is substract essentially.
//  sbcs ip, b_hi, a_hi
//  bge FalseBody
//  TrueBody:
//
//Incorrect code, swap a, b ===>:
//  cmp a_lo, b_lo
//  sbcs ip, a_hi, a_hi
//  ble FalseBody
//  TrueBody:
void ARMIR2OR::convertRelationOpDWORDForLEandGT(IR const* ir,
                                                SR * sr0, SR * sr1,
                                                bool is_signed,
                                                OUT RecycORList & ors,
                                                OUT RecycORList & tors,
                                                IN IOC * cont)
{
    ASSERT0(ir->is_le() || ir->is_gt());
    SR * sr0_l = sr0->getVec()->get(0);
    SR * sr0_h = sr0->getVec()->get(1);
    SR * sr1_l = sr1->getVec()->get(0);
    SR * sr1_h = sr1->getVec()->get(1);

    //SBCS: If S is specified, the SBC instruction updates the
    //N, Z, C and V flags according to the result.
    //
    //Compare <sr0_l,sr0_h> is less-than <sr1_l,sr1_h>:
    //  truepd is LT, falsepd is GE
    //  cmp sr1_l, sr0_l
    //  sbcs res, sr1_h, sr0_h
    //  return res, truepd

    //Herein subtle differences to LT/GE is swapping operand of CMP,
    //e.g: cmp sr1, sr0.
    SR * t = sr0_l; sr0_l = sr1_l; sr1_l = t; //SWAP sr0_l, sr1_l
    t = sr0_h; sr0_h = sr1_h; sr1_h = t; //SWAP sr0_h, sr1_h

    convertRelationOpDWORDForLTGELEGT(ir, sr0_l, sr0_h, sr1_l, sr1_h,
                                      is_signed, ors, tors, cont);
    return;
}


//Generate compare operations and return the comparation result registers.
// e.g:
//     a - 1 > b + 2
// =>
//     sr0 = a - 1
//     sr1 = b + 2
//     res, truepd, falsepd <- cmp sr0, sr1
//     return res, truepd, falsepd
void ARMIR2OR::convertRelationOpDWORD(IR const* ir, OUT RecycORList & ors,
                                      IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());
    IR const* opnd0 = BIN_opnd0(ir);
    IR const* opnd1 = BIN_opnd1(ir);
    ASSERTN(!opnd0->is_any() && !opnd1->is_any(),
            ("operand of '%s' can not be ANY", IRNAME(ir)));
    UINT maxopndsize = MAX(opnd0->getTypeSize(m_tm), opnd1->getTypeSize(m_tm));
    xoc::Type const* maxty =
        opnd1->getTypeSize(m_tm) < opnd1->getTypeSize(m_tm) ?
            opnd1->getType() : opnd1->getType();
    //Integer, dould size of GENERAL_REGISTER_SIZE.
    ASSERTN(maxopndsize == BYTESIZE_OF_DWORD, ("only support DWORD"));

    RecycORList tors(this);
    IOC tmp;
    //Operands 0
    IR const* loc0 = opnd0;
    if (loc0->getTypeSize(m_tm) < maxopndsize) {
        loc0 = m_irmgr->buildCvt(m_rg->dupIRTree(opnd0), maxty);
    }
    convertGeneralLoad(loc0, tors, &tmp);
    SR * sr0 = tmp.get_reg(0);
    ASSERT0(sr0->is_vec());

    //Operands 1
    tmp.clean();
    IR const* loc1 = opnd1;
    if (loc1->getTypeSize(m_tm) < maxopndsize) {
        loc1 = m_irmgr->buildCvt(m_rg->dupIRTree(opnd1), maxty);
    }
    convertGeneralLoad(loc1, tors, &tmp);
    SR * sr1 = tmp.get_reg(0);
    ASSERT0(sr1->is_vec());
    ASSERT0(sr0->getByteSize() == maxopndsize);
    ASSERT0(sr0->getByteSize() == sr1->getByteSize());

    //ARM Conditions Flag.
    // Name Condition                    State of Flags
    // EQ   Equal / equals zero          Z set
    // NE   Not equal                    Z clear
    // CS   Carry set                    C set
    // CC   Carry clear                  C clear
    // MI   Minus / negative             N set
    // PL   Plus / positive or zero      N clear
    // VS   Overflow                     V set
    // VC   No overflow                  V clear
    // HS   Unsigned higher or same      C set
    // LO   Unsigned lower               C clear
    // HI   Unsigned higher              C set and Z clear
    // LS   Unsigned lower or same       C clear or Z set
    // GE   Signed greater than or equal N equals V
    // LT   Signed less than             N is not equal to V
    // GT   Signed greater than          Z clear and N equals V
    // LE   Signed less than or equal    Z set or N is not equal to V
    bool is_signed = opnd0->is_signed();
    switch (ir->getCode()) {
    case IR_EQ:
    case IR_NE:
        convertRelationOpDWORDForEquality(ir, sr0, sr1, is_signed,
                                          ors, tors, cont);
        break;
    case IR_LE:
    case IR_GT:
        convertRelationOpDWORDForLEandGT(ir, sr0, sr1, is_signed,
                                         ors, tors, cont);
        break;
    case IR_LT:
    case IR_GE:
        convertRelationOpDWORDForLTandGE(ir, sr0, sr1, is_signed,
                                         ors, tors, cont);
        break;
    default:
        UNREACHABLE();
    }
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR, TruePredicatedSR, and
//FalsePredicatedSR.
//The ResultSR records the boolean value of comparison of relation operation.
// e.g:
//     a - 1 > b + 2
// =>
//     sr0 = a - 1
//     sr1 = b + 2
//     res, truepd, falsepd <- cmp sr0, sr1
//     return res, truepd, falsepd
void ARMIR2OR::convertRelationOp(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont)
{
    ASSERT0(ir->is_relation());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);
    ASSERT0(opnd0 && opnd1);
    if (opnd0->getType()->is_pointer()) {
        ASSERT0(opnd0->getTypeSize(m_tm) >= opnd1->getTypeSize(m_tm));
    }
    if (opnd1->getType()->is_pointer()) {
        ASSERT0(opnd1->getTypeSize(m_tm) >= opnd0->getTypeSize(m_tm));
    }

    if (opnd0->is_fp()) {
        convertRelationOpFP(ir, ors, cont);
        return;
    }

    ASSERTN(!opnd0->is_any() && !opnd1->is_any(),
            ("operand of '%s' can not be ANY", IRNAME(ir)));
    UINT maxopndsize = MAX(opnd0->getTypeSize(m_tm), opnd1->getTypeSize(m_tm));
    if (maxopndsize == BYTESIZE_OF_DWORD) {
        convertRelationOpDWORD(ir, ors, cont);
        return;
    }

    ASSERT0(opnd0->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE);
    ASSERT0(opnd1->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE);

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
    getResultPredByIRCode(ir->getCode(), &truepd, &falsepd,
                          opnd0->is_signed());

    recordRelationOpResult(ir, truepd, falsepd, truepd, ors, cont);

    //if (!ir->getParent()->isConditionalBr()) {
    //    SR * res = getCG()->genReg();
    //    RecycORList tors(this);
    //    getCG()->buildMove(res, getCG()->gen_one(), tors.getList(), cont);
    //    tors.set_pred(truepd, m_cg));
    //    tors.copyDbx(ir);
    //    ors.move_tail(tors);

    //    tors.clean();
    //    getCG()->buildMove(res, getCG()->genZero(), tors.getList(), cont);
    //    tors.set_pred(falsepd, m_cg);
    //    tors.copyDbx(ir);
    //    ors.move_tail(tors);

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
    IR * movi_ir = m_irmgr->buildStorePR(
        ir->getType(), m_irmgr->buildImmInt(-1, ir->getType()));
    convert(movi_ir, tors, cont);

    cont->clean_bottomup();
    IR * mov_ir = m_irmgr->buildStorePR(ir->getType(),
        m_irmgr->buildBinaryOpSimp(IR_XOR, UNA_opnd(ir)->getType(),
            m_rg->dupIRTree(UNA_opnd(ir)), m_irmgr->buildPRdedicated(
                movi_ir->getPrno(), movi_ir->getType())));

    convert(mov_ir, tors, cont);

    ASSERT0(cont != nullptr);
    ASSERTN(cont->get_reg(0) && cont->get_reg(0)->is_reg(), ("check tgt reg"));

    tors.copyDbx(ir);
    ors.move_tail(tors);
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
    OR_CODE orty = m_cg->mapIRCode2ORCode(IR_XOR,
        UNA_opnd(ir)->getTypeSize(m_tm), UNA_opnd(ir)->getType(),
        res_sr0, opnd, ir->is_signed());
    ASSERTN(orty != OR_UNDEF,
            ("mapIRCode2ORCode() can not find properly target"
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
    ors.move_tail(tors);
}


void ARMIR2OR::recordRelationOpResult(IR const* ir, SR * truepd,
                                      SR * falsepd, SR * result_pred,
                                      OUT RecycORList & ors,
                                      MOD IOC * cont)
{
    if (!ir->getParent()->isConditionalBr()) {
        SR * res = getCG()->genReg();
        RecycORList tors(this);
        getCG()->buildMove(res, getCG()->genOne(), tors.getList(), cont);
        tors.set_pred(truepd, m_cg);
        tors.copyDbx(ir);
        ors.move_tail(tors);

        tors.clean();
        getCG()->buildMove(res, getCG()->genZero(), tors.getList(), cont);
        tors.set_pred(falsepd, m_cg);
        tors.copyDbx(ir);
        ors.move_tail(tors);

        cont->set_reg(RESULT_REGISTER_INDEX, res); //used by non-conditional op

        //truepd, falsepd and result_pred are useless from now on.
        return;
    }

    //Record result-reg.
    //The reg used by Non-Conditional-Op, such as convertSelect.
    cont->set_reg(RESULT_REGISTER_INDEX, nullptr);

    //Record true-result predicated register.
    cont->set_reg(TRUE_PREDICATE_REGISTER_INDEX, truepd);

    //Record false-result predicated register
    cont->set_reg(FALSE_PREDICATE_REGISTER_INDEX, falsepd);

    //Record true-result.
    cont->set_pred(result_pred);

    IOC_is_inverted(cont) = false;
}


void ARMIR2OR::convertIgoto(IR const* ir, OUT RecycORList & ors, IN IOC *)
{
    ASSERT0(ir->is_igoto());
    IOC tmp;
    convertGeneralLoad(IGOTO_vexp(ir), ors, &tmp);
    SR * tgt_addr = tmp.get_reg(0);
    ASSERT0(tgt_addr);
    SR * ll = m_cg->genLabelList(m_cg->buildLabelInfoList(
                                     IGOTO_case_list(ir)));
    ASSERT0(xgen::tmGetOpndSRDesc(OR_bx, 1)->is_label_list());
    OR * o = getCG()->buildOR(OR_bx, 0, 3, getCG()->getTruePred(),
                              ll, tgt_addr);
    OR_dbx(o).copy(*getDbx(ir));
    ors.append_tail(o);
}


void ARMIR2OR::convertSelect(IR const* ir, OUT RecycORList & ors, IOC * cont)
{
    ASSERT0(ir->is_select());
    IOC tmp;
    RecycORList tors(this);

    //Generate Compare operation
    convertRelationOp(SELECT_det(ir), tors, &tmp);
    tors.copyDbx(SELECT_det(ir));
    ors.move_tail(tors);
    SR * cres = tmp.get_reg(0);
    ASSERT0_DUMMYUSE(cres);

    SR * true_pr = tmp.get_reg(1);
    SR * false_pr = tmp.get_reg(2);
    ASSERT0(true_pr && false_pr);

    SR * res = getCG()->genReg();
    ASSERT0(IR_dt(BIN_opnd0(SELECT_det(ir))) ==
            IR_dt(BIN_opnd1(SELECT_det(ir))));
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
            if (sr0->getVec() != nullptr && sr0->getVec()->get(1) != nullptr) {
                sr1 = sr0->getVec()->get(1);
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
        ors.move_tail(tors);
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
            if (sr0->getVec() != nullptr && sr0->getVec()->get(1) != nullptr) {
                sr1 = sr0->getVec()->get(1);
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
        ors.move_tail(tors);
    }
    cont->set_reg(RESULT_REGISTER_INDEX, res);
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR,
//TruePredicatedSR, FalsePredicatedSR.
//The ResultSR record the boolean value of comparison of relation operation.
void ARMIR2OR::convertRelationOpFP(IR const* ir, OUT RecycORList & ors,
                                   IN IOC * cont)
{
    ASSERT0(ir && ir->is_relation());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);
    ASSERTN(!op0->is_any() && !op1->is_any(),
            ("operand of '%s' can not be ANY", IRNAME(ir)));
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
        ASSERT0(opnd0->getByteSize() == BYTESIZE_OF_DWORD);
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

    tors.copyDbx(ir);
    ors.move_tail(tors);
    tors.clean();

    SR * r0 = getCG()->genR0();
    SR * one = getCG()->genOne();
    SR * zero = getCG()->genZero();
    SR * truepd = nullptr;
    SR * falsepd = nullptr;
    bool needresval = !ir->getStmt()->isConditionalBr();
    if (needresval) {
        getCG()->buildCompare(OR_cmp_i, true, r0, zero, tors.getList(), cont);
        tors.copyDbx(ir);
        ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(lt)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(le)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(gt)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(ge)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(eq)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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
            ors.move_tail(tors);
            tors.clean();

            //(ne)r0 = 1;
            getCG()->buildMove(r0, one, tors.getList(), cont);
            ASSERT0(tors.get_elem_count() == 1);
            tors.set_pred(truepd, m_cg);
            tors.copyDbx(ir);
            ors.move_tail(tors);
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


void ARMIR2OR::convertTruebrFP(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERT0(cont->get_pred() == nullptr);
    convertRelationOpFP(BR_det(ir), ors, cont);
    ASSERT0(cont->get_pred());

    SR * retv = cont->get_reg(0);
    ASSERT0(retv && retv->is_reg());

    //Build truebr.
    RecycORList tors(this);
    SR * tgt_lab = getCG()->genLabel(BR_lab(ir));
    getCG()->buildCompare(OR_cmp_i, true, retv,
                          getCG()->genZero(), tors.getList(), cont);
    //cmp does not produce result register.
    ASSERT0(cont->get_reg(0) == nullptr);
    getCG()->buildCondBr(tgt_lab, tors.getList(), cont);
    tors.copyDbx(ir);
    cont->set_pred(nullptr); //clean status
    ors.move_tail(tors);
}


void ARMIR2OR::convertTruebr(IR const* ir, OUT RecycORList & ors, IOC * cont)
{
    ASSERT0(ir && ir->is_truebr());
    IR * opnd0 = BIN_opnd0(BR_det(ir));
    if (opnd0->is_fp()) {
        convertTruebrFP(ir, ors, cont);
        return;
    }
    IR * opnd1 = BIN_opnd1(BR_det(ir));
    ASSERTN_DUMMYUSE(!opnd0->is_any() && !opnd1->is_any(),
                     ("operand of '%s' can not be ANY", IRNAME(ir)));

    if (opnd0->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        IR2OR::convertTruebr(ir, ors, cont);
        return;
    }

    ASSERT0(opnd0->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    convertRelationOpDWORD(BR_det(ir), ors, cont);

    RecycORList tors(this);
    getCG()->buildCondBr(getCG()->genLabel(BR_lab(ir)), tors.getList(), cont);
    tors.copyDbx(ir);
    ors.move_tail(tors);
    return;
}


Var const* ARMIR2OR::genBuiltinVarFP2Int(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_int());
    if (tgt->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        if (tgt->is_sint()) {
            if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
                //float -> sign int.
                return getBuiltinVar(BUILTIN_FIXSFSI);
            }

            ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
            //double -> sign int.
            return getBuiltinVar(BUILTIN_FIXDFSI);
        }

        ASSERT0(tgt->is_uint() || tgt->is_bool());
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> unsign int.
            //float -> bool.
            return getBuiltinVar(BUILTIN_FIXUNSSFSI);
        }

        ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
        //double -> unsign int.
        return getBuiltinVar(BUILTIN_FIXUNSDFSI);
    }

    ASSERT0(tgt->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    if (tgt->is_sint()) {
        if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
            //float -> sign long long.
            return getBuiltinVar(BUILTIN_FIXSFDI);
        }

        ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
        //double -> sign long long.
        return getBuiltinVar(BUILTIN_FIXDFDI);
    }

    ASSERT0(tgt->is_uint());
    if (src->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
         //float -> unsign long long.
         return getBuiltinVar(BUILTIN_FIXUNSSFDI);
    }

    ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    //double -> unsign long long.
    return getBuiltinVar(BUILTIN_FIXUNSDFDI);
}


Var const* ARMIR2OR::genBuiltinVarInt2FP(IR const* tgt, IR const* src)
{
    ASSERT0(src->getTypeSize(m_tm) <= BYTESIZE_OF_DWORD
            && tgt->is_fp());
    if (tgt->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
        if (src->is_sint()) {
            if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
                //sign int -> float.
                return getBuiltinVar(BUILTIN_FLOATSISF);
            }

            ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
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

        ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);

        //unsign long long -> float.
        return getBuiltinVar(BUILTIN_FLOATUNDISF);
    }

    ASSERT0(tgt->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    if (src->is_sint()) {
        if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
            //sign int -> double.
            return getBuiltinVar(BUILTIN_FLOATSIDF);
        }
        ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
        //sign long long -> double.
        return getBuiltinVar(BUILTIN_FLOATDIDF);
    }

    ASSERT0(src->is_uint() || src->is_bool() || src->is_ptr() || src->is_mc());
    if (src->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        //unsign int -> double.
        //bool -> double.
        return getBuiltinVar(BUILTIN_FLOATUNSIDF);
    }

    ASSERT0(src->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    //unsign long long -> double.
    return getBuiltinVar(BUILTIN_FLOATUNDIDF);
}


Var const* ARMIR2OR::genBuiltinVarFP2FP(IR const* tgt, IR const* src)
{
    ASSERT0(src->is_fp() && tgt->is_fp());
    UINT tgtsz = tgt->getTypeSize(m_tm);
    UINT srcsz = src->getTypeSize(m_tm);
    DUMMYUSE(srcsz);
    ASSERTN(tgtsz != srcsz, ("CVT is redundant"));
    if (tgtsz == GENERAL_REGISTER_SIZE) {
        ASSERT0(srcsz == BYTESIZE_OF_DWORD);
        return getBuiltinVar(BUILTIN_TRUNCDFSF2);
    }
    ASSERT0(tgtsz == srcsz * 2);
    return getBuiltinVar(BUILTIN_EXTENDSFDF2);
}


//CASE: integer and pointer convertion.
void ARMIR2OR::convertCvtBetweenIntType(IR const* ir, OUT RecycORList & ors,
                                        MOD IOC * cont)
{
    ASSERT0((ir->is_int() || ir->is_ptr()) &&
            (CVT_exp(ir)->is_int() || CVT_exp(ir)->is_ptr()));
    IR2OR::convertCvt(ir, ors, cont);
}


//CASE: Load constant-string address into register.
void ARMIR2OR::convertCvtIntAndStr(IR const* ir, SR * opnd,
                                   OUT RecycORList & ors,
                                   MOD IOC * cont)
{
    ASSERT0((ir->is_int() || ir->is_ptr()) && CVT_exp(ir)->is_str());
    ASSERTN(ir->getTypeSize(m_tm) >= GENERAL_REGISTER_SIZE,
            ("Unsupported CVT string to small integer"));
    ASSERT0(opnd->is_reg());
    if (ir->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, opnd);
        return;
    }

    ASSERT0(ir->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
    if (opnd->is_vec()) {
        SR * opnd2 = opnd->getVec()->get(1);
        ASSERT0(opnd2);
        getCG()->buildMove(opnd2, getCG()->genIntImm((HOST_INT)0, false),
                           ors.getList(), cont);
        cont->clean_regvec();
        cont->set_reg(RESULT_REGISTER_INDEX, opnd);
        return;
    }

    SR * opnd2 = getCG()->genReg();
    getCG()->getSRVecMgr()->genSRVec(2, opnd, opnd2);
    getCG()->buildMove(opnd2, getCG()->genIntImm((HOST_INT)0, false),
                       ors.getList(), cont);
    cont->clean_regvec();
    cont->set_reg(RESULT_REGISTER_INDEX, opnd);
}


void ARMIR2OR::convertCvtByBuiltIn(IR * newir, IR const* orgir,
                                   SR * opnd_of_cvt_exp,
                                   OUT RecycORList & ors, IN IOC * cont)
{
    Var const* builtin = nullptr;
    if (m_cg->regardAsIntType(newir->getType())) {
        builtin = genBuiltinVarFP2Int(newir, CVT_exp(newir));
    } else if (newir->is_fp()) {
        if (m_cg->regardAsIntType(CVT_exp(newir)->getType())) {
            builtin = genBuiltinVarInt2FP(newir, CVT_exp(newir));
        } else {
            ASSERT0(CVT_exp(newir)->is_fp());
            UINT tgtsz = newir->getTypeSize(m_tm);
            UINT srcsz = CVT_exp(newir)->getTypeSize(m_tm);
            if (tgtsz == srcsz) {
                //CVT is dispensable.
                ASSERT0(opnd_of_cvt_exp->is_reg());
                cont->clean_regvec();
                cont->set_reg(RESULT_REGISTER_INDEX, opnd_of_cvt_exp);
                return;
            }
            builtin = genBuiltinVarFP2FP(newir, CVT_exp(newir));
        }
    } else { UNREACHABLE(); }
    ASSERT0(builtin);
    RecycORList tors(this);
    //Prepare argdesc.
    ArgDescMgr argdescmgr;
    m_cg->passArgVariant(&argdescmgr, tors.getList(), 1,
                         opnd_of_cvt_exp, nullptr,
                         opnd_of_cvt_exp->getByteSize(),
                         ::getDbx(CVT_exp(newir)));

    //Collect the maximum parameters size during code generation.
    //And revise SP-adjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());
    getCG()->buildCall(builtin, newir->getTypeSize(m_tm),
                       tors.getList(), cont);

    //Result register.
    ASSERT0(cont);
    cont->clean_regvec();
    if (newir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        cont->set_reg(RESULT_REGISTER_INDEX, getCG()->genR0());
    } else {
        ASSERT0(newir->getTypeSize(m_tm) == BYTESIZE_OF_DWORD);
        SR * res0 = getCG()->getSRVecMgr()->genSRVec(2,
            getCG()->genRegWithPhyReg(REG_R0, RF_R),
            getCG()->genRegWithPhyReg(REG_R1, RF_R));
        cont->set_reg(RESULT_REGISTER_INDEX, res0);
        //TBD:Is it indispensable?
        //cont->set_reg(RESULT_REGISTER_INDEX + 1, res1);
    }
    tors.copyDbx(orgir); //copy dbx from origin ir.
    ors.move_tail(tors);
    if (newir != orgir) {
        m_rg->freeIRTree(newir);
    }
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
    if (m_cg->regardAsIntType(newir->getType()) &&
        m_cg->regardAsIntType(CVT_exp(newir)->getType())) {
        convertCvtBetweenIntType(newir, ors, cont);
        if (newir != ir) {
            m_rg->freeIRTree(newir);
        }
        return;
    }
    ASSERTN(!newir->is_any(), ("unsupport CVT to ANY"));
    RecycORList tors(this);
    IOC tmp;
    convertGeneralLoad(CVT_exp(newir), tors, &tmp);
    tors.copyDbx(CVT_exp(ir)); //copy dbx from origin ir.
    ors.move_tail(tors);
    SR * opnd = tmp.get_reg(0);
    ASSERT0(CVT_exp(newir)->getTypeSize(m_tm) <= opnd->getByteSize());
    if (m_cg->regardAsIntType(newir->getType()) && CVT_exp(newir)->is_str()) {
        convertCvtIntAndStr(newir, opnd, ors, cont);
        if (newir != ir) {
            m_rg->freeIRTree(newir);
        }
        return;
    }
    convertCvtByBuiltIn(newir, ir, opnd, ors, cont);
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
    ASSERT0(exp->get_next() == nullptr);
    IOC tmp;
    convert(exp, tors, &tmp);
    SR * r0 = getCG()->genR0();
    ASSERTN(!exp->is_any(), ("data type of '%s' can not be ANY", IRNAME(exp)));
    if (exp->getTypeSize(m_tm) >
        m_rg->getTargInfo()->getNumOfReturnValueRegister() *
            GENERAL_REGISTER_SIZE) {
        SR * srcaddr = tmp.get_addr();
        ASSERT0(srcaddr && srcaddr->is_reg());

        //Copy return-value to implicit return-value-buffer.
        //Note the buffer should have been passed as the first parameter.
        Var const* retvalbuf = m_rg->findFormalParam(0);
        ASSERT0(retvalbuf);
        ASSERTN(retvalbuf->getByteSize(m_tm) >= exp->getTypeSize(m_tm),
                ("return-value-buffer is not big enough"));
        tmp.clean_bottomup();
        getCG()->buildLda(retvalbuf, 0, nullptr, tors.getList(), &tmp);
        SR * retbufaddr = tmp.get_reg(0);
        ASSERT0(retbufaddr && retbufaddr->is_reg());
        getCG()->buildMemcpyInternal(retbufaddr, srcaddr,
                                     exp->getTypeSize(m_tm),
                                     tors.getList(), &tmp);

        //Return operation references return-address register.
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
            ASSERTN(retv->getVec() && SR_vec_idx(retv) == 0,
                    ("it should be the first SR in vector"));
            SR * retv_2 = retv->getVec()->get(1);
            SR * r1 = getCG()->genR1();
            getCG()->buildMove(r1, retv_2, tors.getList(), nullptr);
            o = getCG()->buildOR(OR_ret2, 0, 4,
                                 getCG()->getTruePred(),
                                 getCG()->genReturnAddr(), r0, r1);
        }
        tors.append_tail(o);
    }
    tors.copyDbx(ir);
    ors.move_tail(tors);
}


IR * ARMIR2OR::insertCvt(IR const* ir)
{
    switch (ir->getCode()) {
    SWITCH_CASE_DIRECT_MEM_STMT:
    SWITCH_CASE_INDIRECT_MEM_STMT:
    SWITCH_CASE_WRITE_ARRAY:
    case IR_STPR: {
        if (ir->is_any() || ir->getRHS()->is_any()) { break; }
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
            IR * cvt = m_irmgr->buildCvt(newir->getRHS(), newir->getType());
            newir->setRHS(cvt);
            return newir;
        }
        break;
    }
    default: break;
    }
    return const_cast<IR*>(ir);
}


void ARMIR2OR::convertSetElem(IR const* ir, OUT RecycORList & ors,
                              MOD IOC * cont)
{
    ASSERT0(0); //TODO
}


void ARMIR2OR::convertGetElem(IR const* ir, OUT RecycORList & ors,
                              MOD IOC * cont)
{
    ASSERT0(0); //TODO
}


void ARMIR2OR::convert(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    IR * newir = insertCvt(ir);
    IR2OR::convert(newir, ors, cont);
    if (newir != ir) {
        m_rg->freeIRTree(newir);
    }
}
