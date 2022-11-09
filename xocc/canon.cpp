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
@*/
#include "../opt/cominc.h"
#include "xoccinc.h"

namespace xocc {

IR * Canon::handle_det_list(IN IR * ir_list, OUT bool & change, CanonCtx * cc)
{
    IR * x = only_left_last(ir_list);
    return handle_exp(x, change, cc);
}


IR * Canon::only_left_last(IR * head)
{
    IR * last = removetail(&head);
    while (head != nullptr) {
        IR * t = xcom::removehead(&head);
        m_rg->freeIRTree(t);
    }
    return last;
}


IR * Canon::handle_select(IN IR * ir, OUT bool & change, CanonCtx * cc)
{
    ASSERT0(ir->is_select());
    //e.g:
    //     a>0,x<0 ? b/3,c*2,d:0,1,2
    // normalize to:
    //     x>0 ? d:2
    bool lchange = false;
    SELECT_det(ir) = only_left_last(SELECT_det(ir));
    SELECT_det(ir) = handle_exp(SELECT_det(ir), lchange, cc);

    if (!SELECT_det(ir)->is_judge()) {
        SELECT_det(ir) = m_rg->getIRMgr()->buildJudge(SELECT_det(ir));
        ir->setParent(SELECT_det(ir));
    }

    SELECT_trueexp(ir) = only_left_last(SELECT_trueexp(ir));
    SELECT_trueexp(ir) = handle_exp(SELECT_trueexp(ir), lchange, cc);

    SELECT_falseexp(ir) = only_left_last(SELECT_falseexp(ir));
    SELECT_falseexp(ir) = handle_exp(SELECT_falseexp(ir), lchange, cc);

    if (lchange) {
        ir->setParentPointer(false);
    }
    change |= lchange;
    return ir;
}


//Handle the case that get address of array element.
//e.g: =&a[i]
IR * Canon::handle_lda(IR * ir, bool & change, CanonCtx * cc)
{
    DUMMYUSE(ir);
    DUMMYUSE(cc);
    DUMMYUSE(change);
    //if (LDA_base(ir)->is_array()) {
    //    SimpCtx tc;
    //    SIMP_array(&tc) = true;
    //    ir = m_rg->simplifyArrayAddrExp(LDA_base(ir), &tc);
    //    ASSERTN(SIMP_stmtlist(&tc) == nullptr, ("TODO: handle this case"));
    //    if (cc != nullptr) {
    //        xcom::add_next(&cc->new_stmts, SIMP_stmtlist(&tc));
    //    }
    //    change = true;
    //}

    //if (LDA_base(ir)->is_ild()) {
    //    //Convert
    //    //    LDA
    //    //     ILD
    //    //      LD,ofst
    //    //=>
    //    //    LD,ofst
    //    //e.g1: &(*(a.p)) => a.p
    //    //
    //    //
    //    //Convert
    //    //    LDA
    //    //     ILD,ofst2
    //    //      LD,ofst1
    //    //=>
    //    //    ADD((LD,ofst1), ofst2)
    //    //e.g2:&(s.a->b) => LD(s.a) + ofst(b)
    //    IR * newir = ILD_base(LDA_base(ir));
    //    ILD_base(LDA_base(ir)) = nullptr;
    //    if (ILD_ofst(LDA_base(ir)) != 0) {
    //        Type const* t = getTypeMgr()->getSimplexTypeEx(D_U32);
    //        newir = buildBinaryOpSimp(IR_ADD, ir->getType(), newir,
    //                             buildImmInt(ILD_ofst(LDA_base(ir)), t));
    //        copyDbx(newir, ir, this);
    //    }
    //    //Do not need to set parent pointer.
    //    freeIRTree(ir);
    //    change = true;
    //    return newir; //No need to update DU chain and MDS.
    //}

    //Convert LDA(ILD(x)) => x
    //if (LDA_base(ir)->is_ild()) {
    //    IR * newir = ILD_base(LDA_base(rhs));
    //    ILD_base(LDA_base(rhs)) = nullptr;
    //    freeIRTree(ir);
    //    change = true;
    //    ir = newir;
    //}

    //if (ILD_ofst(base) != 0) {
    //    //Reform IR tree.
    //    //e.g:struct S { int a; int b; } * p;
    //    //    ... = &(p->b);
    //    //For now base will be:
    //    //    LDA (ptr)
    //    //      ILD (ofst 4)
    //    //        LD ('p', ptr)
    //    //Normalize to
    //    //    ADD (ptr)
    //    //      LD ('p', ptr)
    //    //      IMM (4)
    //    ir = m_rg->buildBinaryOpSimp(IR_ADD, ILD_base(base)->getType(),
    //        ILD_base(base), m_rg->buildImmInt(
    //            ILD_ofst(base), m_tm->getI32()));
    //} else if (kid->getCode() == TR_INDMEM) {
    //    //Reform IR tree.
    //    //e.g:struct S { int a; int b; } * p;
    //    //    ... = &(p->a);
    //    //For now base will be:
    //    //    LDA
    //    //      ILD <int>
    //    //        LD ('p', ptr)
    //    //Normalize to
    //    //    LD ('p', ptr)
    //    ir = ILD_base(base);
    //}
    //ILD_base(base) = nullptr;
    //m_rg->freeIRTree(base);
    //setLineNum(ir, lineno, m_rg);
    //return ir;

    return ir;
}


IR * Canon::handle_exp(IN IR * ir, OUT bool & change, CanonCtx * cc)
{
    ASSERT0(ir->is_exp());
    switch (ir->getCode()) {
    SWITCH_CASE_DIRECT_MEM_EXP:
    SWITCH_CASE_INDIRECT_MEM_EXP:
    case IR_CONST:
    case IR_ID:
        return ir;
    case IR_LDA: // &a get address of 'a'
        return handle_lda(ir, change, cc);
    SWITCH_CASE_BIN:
        BIN_opnd0(ir) = only_left_last(BIN_opnd0(ir));
        BIN_opnd1(ir) = only_left_last(BIN_opnd1(ir));
        BIN_opnd0(ir) = handle_exp(BIN_opnd0(ir), change, cc);
        BIN_opnd1(ir) = handle_exp(BIN_opnd1(ir), change, cc);
        return ir;
    SWITCH_CASE_UNA:
        UNA_opnd(ir) = only_left_last(UNA_opnd(ir));
        UNA_opnd(ir) = handle_exp(UNA_opnd(ir), change, cc);
        return ir;
    case IR_LABEL:
        return ir;
    SWITCH_CASE_READ_ARRAY:
        for (IR * k = ARR_sub_list(ir); k != NULL; k = k->get_next()) {
            IR * tmp = handle_exp(k, change, cc);
            ASSERTN(tmp == k, ("need to be replaced from sublist"));
        }
        ARR_base(ir) = handle_exp(ARR_base(ir), change, cc);
        return ir;
    SWITCH_CASE_READ_PR:
        return ir;
    case IR_SELECT:
        return handle_select(ir, change, cc);
    default: UNREACHABLE();
    }
    return ir;
}


void Canon::handle_call(IN IR * ir, OUT bool & change, CanonCtx * cc)
{
    ASSERT0(ir->isCallStmt());

    if (CALL_param_list(ir) != nullptr) {
        IR * param = xcom::removehead(&CALL_param_list(ir));
        IR * newparamlst = nullptr;
        IR * last = nullptr;
        while (param != nullptr) {
            IR * newp = handle_exp(param, change, cc);
            xcom::add_next(&newparamlst, &last, newp);
            last = newp;
            param = xcom::removehead(&CALL_param_list(ir));
        }
        CALL_param_list(ir) = newparamlst;
    }
}


IR * Canon::handle_stmt(IN IR * ir, OUT bool & change, CanonCtx * cc)
{
    bool tmpc = false;
    switch (ir->getCode()) {
    SWITCH_CASE_DIRECT_MEM_STMT:
    SWITCH_CASE_INDIRECT_MEM_STMT:
    SWITCH_CASE_WRITE_ARRAY:
    case IR_STPR:
        ir->setRHS(handle_exp(ir->getRHS(), tmpc, cc));
        break;
    SWITCH_CASE_CALL:
        handle_call(ir, tmpc, cc);
        break;
    case IR_DO_WHILE:
    case IR_WHILE_DO:
        LOOP_det(ir) = handle_det_list(LOOP_det(ir), tmpc, cc);
        LOOP_body(ir) = handle_stmt_list(LOOP_body(ir), tmpc);
        break;
    case IR_DO_LOOP:
        LOOP_det(ir) = handle_det_list(LOOP_det(ir), tmpc, cc);
        LOOP_init(ir) = handle_stmt_list(LOOP_init(ir), tmpc);
        LOOP_step(ir) = handle_stmt_list(LOOP_step(ir), tmpc);
        LOOP_body(ir) = handle_stmt_list(LOOP_body(ir), tmpc);
        break;
    case IR_IF:
        IF_det(ir) = handle_det_list(IF_det(ir), tmpc, cc);
        IF_truebody(ir) = handle_stmt_list(IF_truebody(ir), tmpc);
        IF_falsebody(ir) = handle_stmt_list(IF_falsebody(ir), tmpc);
        break;
    case IR_SWITCH:
        SWITCH_vexp(ir) = handle_det_list(SWITCH_vexp(ir), tmpc, cc);
        SWITCH_body(ir) = handle_stmt_list(SWITCH_body(ir), tmpc);
        break;
    SWITCH_CASE_CONDITIONAL_BRANCH_OP:
        BR_det(ir) = handle_det_list(BR_det(ir), tmpc, cc);
        break;
    SWITCH_CASE_LOOP_ITER_CFS_OP:
    SWITCH_CASE_UNCONDITIONAL_BRANCH_OP:
    case IR_RETURN:
    case IR_LABEL:
        break;
    default: UNREACHABLE(); //TODO
    }
    if (tmpc && ir->is_stmt()) {
        ir->setParentPointer(false);
    }
    change |= tmpc;
    return ir;
}


IR * Canon::handle_stmt_list(IR * ir_list, bool & change)
{
    bool lchange = true; //local flag
    while (lchange) {
        lchange = false;
        IR * new_list = nullptr;
        IR * last = nullptr;
        while (ir_list != nullptr) {
            IR * ir = xcom::removehead(&ir_list);
            if (!ir->is_stmt()) {
                //Delete exp node from statement list.
                //Just free ir and its kids tree,
                //but without sibling nodes!
                m_rg->freeIRTree(ir);
                continue;
            }

            CanonCtx cc;
            IR * allocir = handle_stmt(ir, lchange, &cc);
            xcom::add_next(&new_list, &last, cc.new_stmts);
            xcom::add_next(&new_list, &last, allocir);
        }
        change |= lchange;
        ir_list = new_list;
    }
    return ir_list;
}

} //namespace xocc
