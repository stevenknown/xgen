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
@*/
#include "../opt/cominc.h"
#include "feinc.h"

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


IR * Canon::handle_select(IN IR * ir, OUT bool & change, CanonCtx *)
{
    DUMMYUSE(change);

    ASSERT0(ir->is_select());
    //e.g:
    //     a>0,x<0 ? b/3,c*2,d:0,1,2
    // normalize to:
    //     x>0 ? d:2
    if (SELECT_pred(ir)->get_next() != nullptr) {
        SELECT_pred(ir) = only_left_last(SELECT_pred(ir));
        if (!SELECT_pred(ir)->is_judge()) {
            SELECT_pred(ir) = m_rg->buildJudge(SELECT_pred(ir));
            ir->setParent(SELECT_pred(ir));
        }
    }
    if (IR_next(SELECT_trueexp(ir)) != nullptr) {
        SELECT_trueexp(ir) = only_left_last(SELECT_trueexp(ir));
    }
    if (IR_next(SELECT_falseexp(ir)) != nullptr) {
        SELECT_falseexp(ir) = only_left_last(SELECT_falseexp(ir));
    }
    ir->setParentPointer(false);
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
    return ir;
}


IR * Canon::handle_exp(IN IR * ir, OUT bool & change, CanonCtx * cc)
{
    ASSERT0(ir->is_exp());
    switch (ir->getCode()) {
    case IR_CONST:
    case IR_ID:
    case IR_LD:
    case IR_ILD:
        return ir;
    case IR_LDA: // &a get address of 'a'
        return handle_lda(ir, change, cc);
    SWITCH_CASE_BIN:
        BIN_opnd0(ir) = handle_exp(BIN_opnd0(ir), change, cc);
        BIN_opnd1(ir) = handle_exp(BIN_opnd1(ir), change, cc);
        return ir;
    SWITCH_CASE_UNA:
        UNA_opnd(ir) = handle_exp(UNA_opnd(ir), change, cc);
        return ir;
    case IR_LABEL:
    case IR_ARRAY:
        return ir;
    case IR_PR:
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
    case IR_ST:
    case IR_STPR:
    case IR_IST:
    case IR_STARRAY:
        ir->setRHS(handle_exp(ir->getRHS(), tmpc, cc));
        break;
    case IR_CALL:
    case IR_ICALL:
        handle_call(ir, tmpc, cc);
        break;
    case IR_GOTO:
    case IR_IGOTO:
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

    case IR_TRUEBR:
    case IR_FALSEBR:
        BR_det(ir) = handle_det_list(BR_det(ir), tmpc, cc);
        break;
    case IR_RETURN:
    case IR_BREAK:
    case IR_CONTINUE:
    case IR_LABEL:
        break;
    case IR_PHI:
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
