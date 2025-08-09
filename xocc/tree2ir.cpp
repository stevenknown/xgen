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
#include "xoccinc.h"

namespace xocc {

#define RETVAL_BUFFER_NAME "retval_buf_of_non_name_func_pointer"

//Revise result type of 'ir' according to field_dt.
static void reviseResultTypeViaFieldType(
    IR * ir, xoc::DATA_TYPE dt, UINT sz, Decl const* field_decl,
    xoc::TypeMgr * tm)
{
    if (dt == D_PTR) {
        ir->setPointerType(field_decl->getPointerBaseSize(), tm);
    } else if (dt == D_MC) {
        //DMEM is accessing a memory block. It may be lead a memopy-copy or
        //vector-operation.
        IR_dt(ir) = tm->getMCType(sz);
    } else {
        ASSERT0(IS_SIMPLEX(dt));
        IR_dt(ir) = tm->getSimplexTypeEx(dt);
    }
}


static bool isReturnValueNeedBuffer(xoc::Type const* rettype, xoc::TypeMgr * tm)
{
    ASSERT0(rettype && tm);
    UINT retvalsize = rettype->is_any() ? 0 : tm->getByteSize(rettype);
    return retvalsize > tm->getRegionMgr()->getTargInfo()->
        getNumOfReturnValueRegister() * GENERAL_REGISTER_SIZE;
}


static bool isAncestorsIncludeLDA(xfe::Tree const* t)
{
    while (t != nullptr && t->getCode() != TR_LDA) {
        t = TREE_parent(t);
    }
    return t != nullptr && t->getCode() == TR_LDA;
}


static xoc::Type const* determineIRCode(xfe::Tree const* t, MOD TypeMgr * tm)
{
    if (t->getResultType()->regardAsPointer()) {
        return tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    }

    UINT s;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(t->getResultType(), &s, tm);
    if (dt == D_MC) {
        return tm->getMCType(s);
    }

    ASSERT0(IS_SIMPLEX(dt));
    return tm->getSimplexTypeEx(dt);
}


static IR * convertDerefPointToArray(
    IR * deref_addr, xfe::Tree * t, xfe::Tree * base, MOD TypeMgr * tm,
    Region * rg, UINT lineno)
{
    if (base->getCode() == TR_LDA) {
        //CASE: char s[10]; ...=*s;
        //  where *s will be translated into AST likes DEREF(LDA(ID))),
        //  whereas LDA(ID) will acted as a Pointer which pointed to an array.
        //  The ILD is needed for this case.
        IR * ir = rg->getIRMgr()->buildILoad(deref_addr,
            determineIRCode(t, tm));
        xoc::setLineNum(ir, lineno, rg);
        return ir;
    }

    //Here we are computing the base address of ARRAY accessing.
    //In C spec, base of array should be address expression,
    //e.g:
    //  1. a[i] indicates a is symbol, the base address is LDA(a),
    //  2. (*p)[i] indicates p is variable, the base address is LD(p), it
    //     is different to a[i].
    //For now, it is the second scenario. Because the convertion of DEREF's
    //lchild is 'deref_addr' who has already convert 'p' to LD(p), here we
    //just return the 'deref_addr' as result of DEREF(ID(p)) at all.
    xoc::setLineNum(deref_addr, lineno, rg);
    return deref_addr;
}


//Convert array element type, the result type of array operator as well.
//t: array operator xfe::Tree node.
static xoc::Type const* computeArrayElementType(xfe::Tree const* t,
                                                TypeMgr * tm)
{
    ASSERT0(t->getCode() == TR_ARRAY);
    UINT size;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(t->getResultType(), &size, tm);
    if (dt == D_PTR) {
        return tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    }

    if (dt == D_MC) {
        return tm->getMCType(size);
    }

    ASSERT0(IS_SIMPLEX(dt));
    return tm->getSimplexTypeEx(dt);
}


static IR * computeArrayAddr(xfe::Tree * t, IR * ir, Region * rg,
                             T2IRCtx * cont)
{
    ASSERT0(ir->isArrayOp() && t->getCode() == TR_ARRAY);
    OptCtx oc(rg);
    SimpCtx tc(&oc);
    SIMP_array(&tc) = true;
    IR * newir = rg->getIRSimp()->simplifyArrayAddrExp(ir, &tc);
    rg->freeIRTree(ir);
    ir = newir;
    ASSERT0(ir->is_ptr());

    if (t->getResultType()->is_array()) {
        //We are in CASE 3.
        //Revise pointer-base size according to
        //the pointed array dimension.
        TY_ptr_base_size(ir->getType()) = t->getResultType()->
            getArrayElemByteSize();
    }

    if (SIMP_stmtlist(&tc) != nullptr) {
        cont->appendTailOfTopLevelIRList(SIMP_stmtlist(&tc));
    }
    return ir;
}


static xoc::Type const* convertCallReturnType(xfe::Tree const* t,
                                              xoc::TypeMgr * tm)
{
    ASSERT0(t->getCode() == TR_CALL);
    if (t->getResultType()->regardAsPointer()) {
        return tm->getPointerType(t->getResultType()->
                                  getPointerBaseSize());
    }
    if (t->getResultType()->is_any()) {
        //The function does NOT have return value.
        return tm->getAny();
    }
    UINT size = 0;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(t->getResultType(), &size, tm);
    xoc::Type const* rettype = nullptr;
    if (dt == D_MC) {
        rettype = tm->getMCType(size);
    } else {
        rettype = tm->getSimplexTypeEx(dt);
    }
    return rettype;
}


//
//START CTree2IR
//
//Generate IR for field-access if the base-region of Aggr Operation is a
//structure that returned by a function call.
IR * CTree2IR::convertFieldAccessForReturnValAggr(
    xfe::Tree const* t, IR * retval, T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_CALL);
    //CASE:If base-region of Aggr Operation is IR_CALL, the result value might
    //be stored in PR. However during Aggregate Accessing, the convertor try
    //to set OFFET to the result IR of base-region, but PR does not have offset.
    //As a solution, we intend to allocate local memory to implement the
    //operation, namely, store PR to local memory, then set the OFFSET.
    xoc::Type const* retvaltype = convertCallReturnType(t, m_tm);
    if (isReturnValueNeedBuffer(retvaltype, m_tm)) {
        //Return value has been stored in buffer.
        UINT retvalsize = 0;
        xoc::Var * var = convertReturnValBufVar(retvaltype, &retvalsize, cont);
        ASSERT0(var);
        return m_irmgr->buildLoad(var, retvaltype);
    }
    //Return value is small enough to store in PR.
    xcom::StrBuf tmp(64);
    ASSERT0(retval->isPROp());
    tmp.sprint("$local_copy_of_$%d", retval->getPrno());
    xoc::Var * var = genLocalVar(tmp.buf, retvaltype);
    var->setToFormalParam();
    cont->appendTailOfTopLevelIRList(m_irmgr->buildStore(var, retval));
    return m_irmgr->buildLoad(var, retvaltype);
}


void * CTree2IR::xmalloc(UINT size)
{
    ASSERTN(size > 0, ("xmalloc: size less zero!"));
    ASSERTN(m_pool != nullptr, ("need pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    ASSERT0(p);
    ::memset((void*)p, 0, size);
    return p;
}


//Generate IR for xfe::Tree identifier.
//And calculate the byte-size of identifier.
IR * CTree2IR::buildId(Decl const* id)
{
    xoc::Var * var_info = m_dvmap.mapDecl2Var(id);
    ASSERT0(var_info);
    return m_irmgr->buildId(var_info);
}


//Generate IR for Identifier.
//Calculate the byte-size of identifier.
IR * CTree2IR::buildId(IN xfe::Tree * t)
{
    ASSERTN(t->getCode() == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl * decl = TREE_id_decl(t);
    //TREE_result_type is useless for C,
    //but keep it for another langages used.
    return buildId(decl);
}


IR * CTree2IR::buildLda(xfe::Tree const* t)
{
    ASSERTN(t->getCode() == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl const* decl = TREE_id_decl(t);
    xoc::Var * var_info = m_dvmap.mapDecl2Var(decl);
    ASSERT0(var_info);
    return m_irmgr->buildLda(var_info);
}


IR * CTree2IR::buildLoad(IN xfe::Tree * t)
{
    ASSERTN(t->getCode() == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl * decl = TREE_id_decl(t);
    ASSERT0(decl);
    xoc::Var * var_info = m_dvmap.mapDecl2Var(decl);
    ASSERT0(var_info);
    return m_irmgr->buildLoad(var_info);
}


bool CTree2IR::is_istore_lhs(xfe::Tree const* t) const
{
    ASSERT0(t != nullptr &&
            (TR_ASSIGN == TREE_parent(t)->getCode()) &&
            t == TREE_lchild(TREE_parent(t)));
    return t->getCode() == TR_DEREF;
}


static IR * convertAssignImpl(
    Region * rg, IN IR * ist_mem_addr, IR * l, IR * r, bool is_ild_array_case,
    UINT lineno, xoc::Type const* restype, T2IRCtx * cont)
{
    IR * ir = nullptr;
    if (ist_mem_addr != nullptr) {
        if (is_ild_array_case) {
            IR * tmpir = rg->getIRMgr()->buildStorePR(
                ist_mem_addr->getType(), ist_mem_addr);
            ASSERTN(tmpir->is_ptr(),
                    ("I think tmpir should already be set to"
                     "pointer in buildStore()"));
            xoc::setLineNum(tmpir, lineno, rg);
            cont->appendTailOfTopLevelIRList(tmpir);
            ir = rg->getIRMgr()->buildIStore(
                rg->getIRMgr()->buildPRdedicated(
                STPR_no(tmpir), tmpir->getType()), r, restype);
        } else {
            ir = rg->getIRMgr()->buildIStore(ist_mem_addr, r, restype);
        }
        ir->setOffset(ir->getOffset() + l->getOffset());
    } else if (l->is_ld()) {
        //Generate IR_ST.
        //Normalize LHS.
        ir = rg->getIRMgr()->buildStore(LD_idinfo(l), l->getType(), r);
        ir->setOffset(ir->getOffset() + LD_ofst(l));
    } else if (l->is_array()) {
        //Generate IR_STARRAY.
        //Normalize LHS.
        ir = rg->getIRMgr()->buildStoreArray(ARR_base(l),
            ARR_sub_list(l), l->getType(), ARR_elemtype(l),
            ((CArray*)l)->getDimNum(), ARR_elem_num_buf(l), r);
        ARR_base(l) = nullptr;
        ARR_sub_list(l) = nullptr;
        ir->setOffset(ARR_ofst(l));
    } else {
        ASSERTN(0, ("unsupport lhs IR code."));
    }
    rg->freeIRTree(l); //l is useless.
    return ir;
}


//Convert the in-place assign operation, e.g:+=, -=, *=, /=, etc.
static IR * convertInPlaceAssignImpl(IR_CODE inplaceopc,
                                     Region * rg, IN IR * ist_mem_addr,
                                     IR * l, IR * r, Type const* restype)
{
    xoc::TypeMgr * tm = rg->getTypeMgr();
    xoc::Type const* type = nullptr;
    if (!l->is_ptr() && !r->is_ptr()) {
        type = tm->hoistDTypeForBinOp(l, r);
    } else {
        type = tm->getAny();
        //buildBinaryOp will inefer the type of result ir.
    }
    IR * ir = rg->getIRMgr()->buildBinaryOp(inplaceopc, type, l, r);
    if (ist_mem_addr != nullptr) {
        ir = rg->getIRMgr()->buildIStore(ist_mem_addr, ir, restype);
        ir->setOffset(ir->getOffset() + l->getOffset());
    } else if (l->is_ld()) {
        //Generate IR_ST.
        ir = rg->getIRMgr()->buildStore(LD_idinfo(l), l->getType(), ir);
        ir->setOffset(ir->getOffset() + LD_ofst(l));
    } else if (l->is_array()) {
        //Generate IR_STARRAY.
        ir = rg->getIRMgr()->buildStoreArray(
            rg->dupIRTree(ARR_base(l)),
            rg->dupIRTreeList(ARR_sub_list(l)),
            l->getType(), ARR_elemtype(l), ((CArray*)l)->getDimNum(),
            ARR_elem_num_buf(l), ir);
        ir->setOffset(ARR_ofst(l));
    } else {
        ASSERTN(0, ("unsupport lhs IR code."));
    }
    return ir;
}


IR * CTree2IR::convertTakenAddrOfCallRetVal(IR * retvalexp, T2IRCtx * cont)
{
    xoc::Type const* retvaltype = retvalexp->getType();
    if (retvalexp->isPROp()) {
        //Return value has been stored in buffer.
        UINT retvalsize = 0;
        xoc::Var * var = genReturnValBufVar(retvaltype, &retvalsize);
        ASSERT0(var);
        IR * stretvaltobuf = m_irmgr->buildStore(var, retvaltype, retvalexp);
        cont->appendTailOfTopLevelIRList(stretvaltobuf);
        retvalexp = m_rg->dupIsomoExpTree(stretvaltobuf);
        ASSERT0(retvalexp->is_exp() && retvalexp->hasIdinfo());
    }
    //CASE: given x = &(foo().arr);
    //User required taking the address of 'arr' and assign it to x.
    ASSERT0(retvalexp->is_ld());
    return m_irmgr->buildLda(retvalexp->getIdinfo());
}


IR * CTree2IR::convertTakenAddrOfTree(
    IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    T2IRCtx tc(*cont);
    CONT_is_compute_addr(&tc) = true;
    IR * ret = convert(t, &tc);
    ASSERT0(ret->is_ptr());
    return ret;
}


IR * CTree2IR::convertAssign(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    //One of '='   '*='   '/='   '%='  '+='
    //       '-='  '<<='  '>>='  '&='  '^='  '|='
    IR * ist_mem_addr = nullptr; //record mem_addr if 't' is an ISTORE
    IR * epilog_ir_list = nullptr;
    IR * epilog_ir_list_last = nullptr;

    //Note the outermost TR_ARRAY means the lowest dimension element of array
    //will be accessed.
    T2IRCtx tc(*cont);

    //Do not propagate address-taken operation information to embeded
    //assignment operation.
    CONT_is_compute_addr(&tc) = false;
    CONT_is_lvalue(&tc) = true;
    CONT_is_record_epilog(&tc) = true;
    CONT_epilogirlist(&tc) = &epilog_ir_list;
    CONT_epilogirlist_last(&tc) = &epilog_ir_list_last;
    IR * l = convert(TREE_lchild(t), &tc);
    bool is_ild_array_case = false;
    ASSERTN(l != nullptr && l->is_single(),
            ("Lchild cannot be nullptr and must be single"));
    if (l->is_ild()) {
        //If 'lchild' is dereference, such as '*p', and the
        //converted STORE IR tree is in the form:
        //  ST
        //   |-ILD, ofst <== ir would be removed
        //      |-LD 'p'
        //then generating IST instead of ILD.
        //The legitimate IR tree should be:
        //  IST ofst
        //   |-LD 'p'
        ASSERT0(ILD_base(l) != nullptr);
        if (ILD_base(l)->is_array()) {
            ist_mem_addr = m_rg->dupIRTree(ILD_base(l));
            is_ild_array_case = true;
            //In the situation, e.g: *(a[1]) = 10
            //we generate like this:
            //  pr = a[1]
            //  *pr = 10
            //and the IR tree is in the form:
            //  ST(PR, ARRAY)
            //  IST(PR, 10);
        } else {
            ist_mem_addr = m_rg->dupIRTree(ILD_base(l));
        }
    } else if (l->is_array()) {
        ist_mem_addr = nullptr;
    } else {
        ist_mem_addr = nullptr; //current tree stmt 't' is NOT ISTORE.
    }

    //Compute result type of ST|IST
    xoc::Type const* rtype = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        rtype = m_tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, m_tm);
        if (IS_SIMPLEX(dt)) {
            rtype = m_tm->getSimplexTypeEx(dt);
        } else {
            //Memory Chunk type.
            ASSERT0(dt == D_MC);
            rtype = m_tm->getMCType(s);
        }
    }

    //Convert RHS of tree.
    CONT_is_lvalue(&tc) = false;
    IR * r = convert(TREE_rchild(t), &tc);
    IR * ir = nullptr;
    switch (TREE_token(t)) {
    case T_ASSIGN:
        ir = convertAssignImpl(m_rg, ist_mem_addr, l, r,
                               is_ild_array_case, lineno, rtype, &tc);
        break;
    case T_BITANDEQU:
        ir = convertInPlaceAssignImpl(
            IR_BAND, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_BITOREQU:
        ir = convertInPlaceAssignImpl(IR_BOR, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_ADDEQU:
        ir = convertInPlaceAssignImpl(IR_ADD, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_SUBEQU:
        ir = convertInPlaceAssignImpl(IR_SUB, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_MULEQU:
        ir = convertInPlaceAssignImpl(IR_MUL, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_DIVEQU:
        ir = convertInPlaceAssignImpl(IR_DIV, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_XOREQU:
        ir = convertInPlaceAssignImpl(IR_XOR, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_REMEQU:
        ir = convertInPlaceAssignImpl(IR_REM, m_rg, ist_mem_addr, l, r, rtype);
        break;
    case T_RSHIFTEQU: {
        ASSERTN(!l->is_fp() && !r->is_fp(),
                ("illegal shift operation of float point"));
        ASSERTN(r->is_int(), ("type of shift-right should be integer"));
        if (r->is_signed()) {
            IR_dt(r) = m_tm->getSimplexTypeEx(
                m_tm->getIntDType(
                    m_tm->getDTypeBitSize(
                        TY_dtype(r->getType())), false));
        }
        IR_CODE opc = l->is_sint() ? IR_ASR : IR_LSR;
        ir = convertInPlaceAssignImpl(opc, m_rg, ist_mem_addr, l, r, rtype);
        break;
    }
    case T_LSHIFTEQU:
        ASSERTN(!l->is_fp() && !r->is_fp(),
                ("illegal shift operation of float point"));
        ASSERTN(r->is_int(), ("type of shift-right should be integer"));
        if (r->is_signed()) {
            IR_dt(r) = m_tm->getSimplexTypeEx(
                m_tm->getIntDType(
                    m_tm->getDTypeBitSize(
                        TY_dtype(r->getType())), false));
        }
        ir = convertInPlaceAssignImpl(IR_LSL, m_rg, ist_mem_addr, l, r, rtype);
        break;
    default: UNREACHABLE();
    }
    ASSERT0(ir->is_st() || ir->is_starray() || ir->is_ist());
    if (t->getResultType()->regardAsPointer()) {
        ir->setPointerType(t->getResultType()->getPointerBaseSize(), m_tm);
    }

    CONT_is_record_epilog(&tc) = false;
    xoc::setLineNum(ir, lineno, m_rg);
    tc.appendTailOfTopLevelIRList(ir);

    //Record the post-side-effect operations, such as:x++.
    tc.appendTailOfTopLevelIRList(epilog_ir_list);

    if (CONT_is_compute_addr(cont)) {
        //CASE: given x = &(p=q);
        //User required taking the address of p and assign it to x.
        IR * retv = convertTakenAddrOfTree(
            TREE_lchild(t), TREE_lineno(TREE_lchild(t)), &tc);
        return retv;
    }
    //Return RHS as the result of STMT.
    ir = xcom::get_last(ir); //make sure get the last IR in epilog.
    IR * retv = ir->getRHS();
    ASSERT0(retv);
    return m_rg->dupIRTree(retv);
}


//Convert prefix ++/--:
// ++x => x=x+1; return x;
// --x => x=x-1; return x;
// *++x=0 => x=x+1; *x=0;
// b=*++x => x=x+1; b=*x;
// b=++*x => *x=*x+1; b=*x;
IR * CTree2IR::convertIncDec(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    //Generate base region IR node used into ST
    T2IRCtx ct(*cont);

    //In C spec, pre-increment-op has sideeffect, it has WRITE-property.
    //e.g: ++p, p is inc_exp.
    CONT_is_lvalue(&ct) = true;

    IR * inc_exp = convert(TREE_inc_exp(t), &ct); //need ID

    CONT_is_lvalue(&ct) = false; //begin processing READ-property.

    //Compute pointer addend.
    INT addend = 1;
    xoc::Type const* addend_type = inc_exp->getType();
    ASSERT0(!inc_exp->is_mc());
    if (inc_exp->is_ptr()) {
        addend = TY_ptr_base_size(inc_exp->getType());
        addend_type = m_tm->getSimplexTypeEx(m_tm->getPointerSizeDtype());
    }
    IR * imm = m_irmgr->buildImmInt(addend, addend_type);

    //Generate ADD/SUB.
    IR_CODE irt = IR_UNDEF;
    if (t->getCode() == TR_INC) {
        irt = IR_ADD;
    } else if (t->getCode() == TR_DEC) {
        irt = IR_SUB;
    } else {
        UNREACHABLE();
    }
    IR * ir = m_irmgr->buildBinaryOpSimp(irt, addend_type, inc_exp, imm);

    if (inc_exp->is_ptr()) {
        //buildBinaryOpSimp does not change the result type.
        IR_dt(ir) = inc_exp->getType();
    }

    if (inc_exp->is_ild()) {
        //e.g:
        //        int * p;
        //        *++p = 10;
        //    =>
        //        p = p+4;
        //        *p = 10; //IST
        //    Result type of IST is the type of '*p', is INT.
        //e.g2:
        //        int * p;
        //        a = ++*p;
        //    =>
        //        *p = *p + 1;
        //        a = *p;
        ASSERT0(ILD_base(inc_exp)->is_ptr());
        ir = m_irmgr->buildIStore(m_rg->dupIRTree(ILD_base(inc_exp)),
            ir, inc_exp->getType());
    } else if (inc_exp->is_array()) {
        ir = m_irmgr->buildStoreArray(
            m_rg->dupIRTree(ARR_base(inc_exp)),
            m_rg->dupIRTreeList(ARR_sub_list(inc_exp)),
            inc_exp->getType(),
            ARR_elemtype(inc_exp),
            ((CArray*)inc_exp)->getDimNum(),
            ARR_elem_num_buf(inc_exp),
            ir);
    } else {
        ASSERT0(inc_exp->is_ld());
        ir = m_irmgr->buildStore(LD_idinfo(inc_exp), ir);
    }

    //ST is statement, and only can be appended on top level
    //statement list.
    xoc::setLineNum(ir, lineno, m_rg);
    cont->appendTailOfTopLevelIRList(ir);
    return m_rg->dupIRTree(inc_exp);
}


IR * CTree2IR::convertPointerDeref(xfe::Tree * t, UINT lineno,
                                   T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_ARRAY);
    ASSERT0(TREE_result_type(TREE_array_base(t))->regardAsPointer());

    //CASE 2. Pointer but used as array.
    //     char * p;
    //     p[i] = 11; //=> ST(ILD(LD(p)+LD(i)), 11) => IST((LD(p)+LD(i)), 11)
    //     a = p[i]; //=> ST(a, ILD(LD(p)+LD(i)))

    //We are in CASE 2, dealing with pointer, but using array-operator.
    //Operator [] is dereference of pointer actually.

    T2IRCtx tc(*cont);

    //Base and Index have to be value no matter whether Caller
    //requires address.
    CONT_is_compute_addr(&tc) = false;

    IR * base = convert(TREE_array_base(t), &tc);
    IR * index = convert(TREE_array_indx(t), &tc);
    ASSERT0(base->is_ptr());

    //In this situation, given 'int ** p';
    //then address of p[2][3] will be converted(or linearized) to:
    //    t1 = p
    //    t2 = t1 + 2*sizeof(int)
    //    t3 = ild(t2)
    //    t4 = t3 + 3*sizeof(int)
    //    t5 = ild(t4)
    IR * mem_addr = m_irmgr->buildBinaryOp(
        IR_ADD, base->getType(), base, index);
    ASSERT0(mem_addr->is_ptr() &&
            TY_ptr_base_size(mem_addr->getType()) != 0);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = m_tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, m_tm);
        if (dt == D_MC) {
            type = m_tm->getMCType(s);
        } else {
            ASSERT0(IS_SIMPLEX(dt));
            type = m_tm->getSimplexTypeEx(dt);
        }
    }

    IR * ir = mem_addr; //return the address of pointer.
    if (!CONT_is_compute_addr(cont)) {
        //Return the value that is of the pointer pointed to.
        ir = m_irmgr->buildILoad(ir, type);
    }
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


//The function handles the array accessing for real array type declaration.
//e.g: int a[10][20];
//     ..= a[i][j], where a is real array.
//     ..= ((int*)0x1234)[i], where 0x1234 is not real array.
//  The array which is not real only could using 1-dimension array operator.
//  namely, ..= ((int*)0x1234)[i][j] is illegal.
IR * CTree2IR::convertArraySubExpForArray(
    xfe::Tree * t, xfe::Tree * base, UINT n, TMWORD * elem_nums, T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_ARRAY);
    ASSERT0(n >= 1);
    ASSERT0(base->getCode() == TR_LDA);
    base = TREE_lchild(base);

    Decl * arr_decl = base->getResultType();
    ASSERT0(arr_decl->is_array());

    xfe::Tree * lt = t;
    UINT dim = n - 1;
    IR * sublist = nullptr;
    IR * last = nullptr;
    UINT i = 0;

    //Note the outermost TR_ARRAY means the lowest dimension element of array
    //will be accessed.
    T2IRCtx tc(*cont);

    //Do not propagate information to subscribe-expression convertion.
    CONT_is_compute_addr(&tc) = false;

    //Iterate array-dimension from lowest dimension to highest.
    while (lt->getCode() == TR_ARRAY) {
        IR * subexp = convert(TREE_array_indx(lt), &tc);
        ASSERT0(subexp);
        xcom::add_next(&sublist, &last, subexp);
        elem_nums[i] = (TMWORD)arr_decl->getArrayElemnumToDim(dim);
        lt = TREE_array_base(lt);
        dim--;
        i++;
    }
    return sublist;
}


//base: base xfe::Tree node of ARRAY.
IR * CTree2IR::convertArraySubExp(xfe::Tree * t, TMWORD * elem_nums,
                                  T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_ARRAY);
    xfe::Tree * base = t;
    UINT n = 0;
    while (base->getCode() == TR_ARRAY) {
        base = TREE_array_base(base);
        ASSERT0(base);
        n++;
    }
    ASSERT0(n >= 1);
    if (base->getCode() == TR_LDA &&
        base->getResultType()->isPointerPointToArray()) {
        return convertArraySubExpForArray(t, base, n, elem_nums, cont);
    }

    ASSERT0(n == 1);
    Decl * base_decl = base->getResultType();
    ASSERT0_DUMMYUSE(base_decl->isPointer());

    //Note the outermost TR_ARRAY means the lowest dimension element of array
    //will be accessed.
    T2IRCtx tc(*cont);

    //Do not propagate information to subscribe-expression convertion.
    CONT_is_compute_addr(&tc) = false;

    //Iterate array-dimension from lowest dimension to highest.
    IR * subexp = convert(TREE_array_indx(t), &tc);
    ASSERT0(subexp);
    elem_nums[0] = 0;
    return subexp;
}


IR * CTree2IR::convertArray(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_ARRAY);
    xfe::Tree * basetree = TREE_array_base(t);
    UINT n = 1;
    while (basetree->getCode() == TR_ARRAY &&
           basetree->getResultType()->is_array()) {
        basetree = TREE_array_base(basetree);
        ASSERT0(basetree);
        n++;
    }

    //There are following situations need to be resolved:
    //CASE 1. Regular array.
    //  char p[100];
    //  p[i] = 11; //=> ST(ILD(LDA(p)+LD(i)), 11) => IST((LDA(p)+LD(i)), 11)
    //  a = p[i]; //=> a=ILD(LDA(p)+LD(i))
    //  p is base, i is index.
    //
    //CASE 2. The base of ARRAY operator is a pointer, but is used as array.
    //  e.g1:
    //  char * p;
    //  p[i] = 11; //=> ST(ILD(LD(p)+LD(i)), 11) => IST((LD(p)+LD(i)), 11)
    //  a = p[i]; //=> ST(a, ILD(LD(p)+LD(i)))
    //
    //  e.g2: the 1th dimension operator is a pointer dereference.
    //  char * string_array[2];
    //  char c = string_array[1][0]; //=> ST(c, ILD(ARRAY(string_array, 1)))
    //
    //CASE 3. Taken regular array element address.
    //  char p[2][3];
    //  char * q = p[i]+1; //=> ST(q, LDA(p)+LD(i)+1)
    //  char * w = &p[i][j]; //=> ST(w, LDA(p)+LD(i)*3+LD(j))
    //
    //CASE 4. Taken regular array element address.
    //  char p[2][3];
    //  char * w = &p[i][j]; //=> ST(w, LDA(p)+LD(i)*3+LD(j))
    //
    //CASE 5: Taken array address.
    //  char p[2];
    //  char * q = p;
    //----------------------------------------------------------------
    if (basetree->getCode() != TR_LDA) {
        ASSERT0(basetree->getResultType()->isPointer() ||
                basetree->getResultType()->is_array());
        //e.g: int (*a)[10]; .. = (*a)[i]; the base of array operator is
        //     DEREF which result-type is ARRAY.
        return convertPointerDeref(t, lineno, cont);
    }

    ASSERT0(basetree->getResultType()->regardAsPointer());
    ASSERT0(n >= 1);
    //Handle regular array.
    //Compute the number of elements for each dimensions.
    T2IRCtx tc(*cont);

    //IR_ARRAY operator requires that the base have to POINTER type.
    //Array's base have been canonicalized to LDA in TreeCanon already.
    CONT_is_compute_addr(&tc) = false;

    //Convert base of array.
    IR * base = convert(basetree, &tc);
    if (base->is_array()) {
        //CASE:
        //struct S {int w; short e[100];};
        //struct S s[100][200];
        //short a1 = s[5][6].e[7];
        //
        //The base of e[7] is still an array.
        OptCtx oc(m_rg);
        SimpCtx cont2(&oc);
        SIMP_ret_array_val(&cont2) = false;
        SIMP_array(&cont2) = true;
        base = m_rg->getIRSimp()->simplifyExpression(base, &cont2);
        ASSERT0(SIMP_stmtlist(&cont2) == nullptr);
    }
    ASSERT0(base->is_ptr());

    //Convert subscript-expression.
    TMWORD * elem_nums = (TMWORD*)::malloc(sizeof(TMWORD) * n);
    IR * sublist = convertArraySubExp(t, elem_nums, cont);
    ASSERT0(sublist);

    //Convert array element type, and result type of array operator.
    xoc::Type const* type = computeArrayElementType(t, m_tm);

    //There is C language specific situtation, 'base' of array is either
    //a pointer or another array. Do calibration if that is the case.
    if (TREE_array_base(t)->getCode() != TR_ARRAY && !base->is_ptr()) {
        //Revise array base's data-type.
        base->setPointerType(base->getTypeSize(m_tm), m_tm);
    }

    IR * ir = m_irmgr->buildArray(base, sublist, type, type, n, elem_nums);

    if (t->getResultType()->is_array() || CONT_is_compute_addr(cont)) {
        //If current tree node is just array symbol reference, then we have to
        //take the address of the array, whereas we are in CASE 5.
        ir = computeArrayAddr(t, ir, m_rg, cont);
    }

    ::free(elem_nums);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertPragma(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    StrBuf buf(16);
    for (TokenList * tl = TREE_token_lst(t);
         tl != nullptr; tl = TL_next(tl)) {
        switch (TL_tok(tl)) {
        case T_IMM:
        case T_IMML:
        case T_IMMU:
        case T_IMMUL:
            buf.strcat("%u ", TL_imm(tl));
            break;
        case T_ID:
            buf.strcat(TL_id_name(tl)->getStr());
            buf.strcat(" ");
            break;
        case T_STRING:
            buf.strcat(TL_str(tl)->getStr());
            buf.strcat(" ");
            TL_str(tl) = g_fe_sym_tab->add(g_real_token_string);
            break;
        case T_CHAR_LIST:
            buf.strcat(TL_chars(tl)->getStr());
            buf.strcat(" ");
            break;
        default:;
        }
    }
    IR * ir = m_irmgr->buildLabel(m_rg->genPragmaLabel(buf.buf));
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


//Return true if function do not modify any variable.
bool CTree2IR::is_readonly(xoc::Var const* v) const
{
    CHAR const* vname = SYM_name(VAR_name(v));
    if (::strcmp(vname, "printf") == 0) {
        //CASE: If 'printf' is set to be READONLY, DeadCodeElim will remove
        //the call-site if there is no use of printf's return value, thus
        //test/exec will failed.
        //return true;
        return false;
    }
    return false;
}


bool CTree2IR::is_align(Sym const* sym) const
{
    return ::strcmp(sym->getStr(), "align") == 0;
}


//Return true if function allocate memory from heap.
bool CTree2IR::is_alloc_heap(xoc::Var const* v) const
{
    CHAR const* vname = SYM_name(VAR_name(v));
    if (::strcmp(vname, "malloc") == 0 ||
        ::strcmp(vname, "alloca") == 0 ||
        ::strcmp(vname, "calloc") == 0) {
        return true;
    }
    return false;
}


IR * CTree2IR::convertCallee(xfe::Tree const* t, bool * is_direct,
                             T2IRCtx const* cont)
{
    ASSERT0(t->getCode() == TR_CALL);
    T2IRCtx tcont(*cont);
    CONT_is_parse_callee(&tcont) = true;
    IR * callee = convert(TREE_fun_exp(t), &tcont);

    //Handle C language trait.
    //Omit meaningless ILD operation.
    IR * real_callee = callee;
    while (real_callee->is_ild()) {
        //e.g:given int (*f)();
        //In C language, even the clueless syntax is legal: (***********f)();
        real_callee = ILD_base(real_callee);
    }

    //Omit meaningless CVT operation.
    //e.g:given int (*f)();
    //    ((char*)f)(); does not have any certain effect.
    if (real_callee->is_cvt()) {
        real_callee = ((CCvt*)real_callee)->getLeafExp();
    }

    if (real_callee != callee) {
        //Apply new callee.
        IR * old = callee;
        callee = m_rg->dupIRTree(real_callee);
        m_rg->freeIRTree(old);
    }

    if (!callee->is_ptr()) {
        //Make sure calllee is pointer-type.
        callee->setPointerType(4, m_tm);
    }

    if (!callee->is_id()) {
        if (callee->is_ptr() ||
            (callee->is_const() && callee->is_int())) {
            *is_direct = false; //Current call is indirect function call.
        } else {
            UNREACHABLE(); //unsupport.
        }
    }
    return callee;
}


xoc::Var * CTree2IR::genReturnValBufVar(
    xoc::Type const* rettype, OUT UINT * return_val_size)
{
    //If memory byte size of return-value is too big to store in
    //return-value registers, generate a return-value-buffer and
    //transfering its address as the first implict parameter to
    //the call.
    xcom::StrBuf tmp(64);
    tmp.sprint("$local_retvalbuf_of_%d_bytes", *return_val_size);
    xoc::Sym const* name = m_rg->getRegionMgr()->addToSymbolTab(tmp.buf);

    //Note all function-calls share the same local-return-value-buffer with
    //same byte size even if they are different function call-site.
    //e.g:given 12 byte size struct S {...}.
    //the call to struct S1 foo() and struct S1 bar() both use the variable
    //'local_retvalbuf_of_12_bytes'.
    xoc::Var * retval_buf = m_rg->getVarMgr()->findVarByName(name);
    if (retval_buf == nullptr) {
        //The return-value-buffer is a local var of current region, whose
        //address will be the first argument to callee function.
        //The Var retval_buf only used in current region.
        retval_buf = genLocalVar(name, rettype);
    }
    return retval_buf;
}


xoc::Var * CTree2IR::convertReturnValBufVar(
    xoc::Type const* rettype, OUT UINT * return_val_size, T2IRCtx * cont)
{
    ASSERT0(cont);
    *return_val_size = rettype->is_any() ? 0 : m_tm->getByteSize(rettype);
    if (*return_val_size <=
        m_rg->getTargInfo()->getNumOfReturnValueRegister() *
            GENERAL_REGISTER_SIZE) {
        return nullptr;
    }
    return genReturnValBufVar(rettype, return_val_size);
}


//Handle return-value-buffer.
IR * CTree2IR::convertCallReturnBuf(
    xoc::Type const* rettype, IR const* callee, OUT UINT * return_val_size,
    OUT xoc::Var ** retval_buf, T2IRCtx * cont)
{
    xoc::Var * var = convertReturnValBufVar(rettype, return_val_size, cont);
    if (var == nullptr) { return nullptr; }
    //WorkAround: We always translate TR_ID into LD(ID),
    //here it is callee actually.
    ASSERT0(callee->is_id());
    ASSERT0(ID_info(callee));

    //The Var will be the first parameter of current function, however it is
    //hidden and can not be seen by programmer.
    *retval_buf = var;
    return m_irmgr->buildLda(var);
}


IR * CTree2IR::convertCallItself(xfe::Tree * t, IR * arglist, IR * callee,
                                 bool is_direct, UINT lineno, T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_CALL);

    //Convert the real parameter-expression.
    xcom::add_next(&arglist, convert(TREE_para_list(t), cont));

    //Generate CALL stmt.
    IR * call = nullptr;
    if (is_direct) {
        ASSERT0(callee->is_id());
        xoc::Var * v = ID_info(callee);
        call = m_irmgr->buildCall(v, arglist, 0, m_tm->getAny());
        if (is_readonly(v)) {
            CALL_idinfo(call)->setFlag(VAR_READONLY);
        }
        if (is_alloc_heap(v)) {
            CALL_is_alloc_heap(call) = true;
        }
    } else {
        call = m_irmgr->buildICall(callee, arglist, 0, m_tm->getAny());
    }
    call->verify(m_rg);
    xoc::setLineNum(call, lineno, m_rg);
    cont->appendTailOfTopLevelIRList(call);
    return call;
}


IR * CTree2IR::convertCallReturnVal(
    IR * call, UINT return_val_size, xoc::Var * retval_buf,
    xoc::Type const* rettype, UINT lineno, T2IRCtx * cont)
{
    ASSERT0(cont);
    if (return_val_size >
        m_rg->getTargInfo()->getNumOfReturnValueRegister() *
            GENERAL_REGISTER_SIZE) {
        ASSERT0(retval_buf);
        IR * ret_exp = m_irmgr->buildLoad(retval_buf);
        xoc::setLineNum(ret_exp, lineno, m_rg);
        return ret_exp;
    }
    //Note if 'return_val_size' is 0, the CALL does not have a return
    //value according to its declaration. But in C language, this is
    //NOT an error, just a warning. Thus we still give a return result.
    //e.g: void get_bar(void) { return 10; }
    //  C warning: 'return' with a value, in function returning void.
    if (rettype->is_any()) {
        //CASE: In C langage, the return value may be used even if the
        //function declared with no return value! So regard return value
        //is INT type if it is ANY.
        //e.g: void add(int a, int b); if (add(1,2)) { ... }
        rettype = m_tm->getIntType(m_tm->getPointerBitSize(), true);
    }
    IR * respr = m_irmgr->buildPR(rettype);
    xoc::setLineNum(respr, lineno, m_rg);
    CALL_prno(call) = PR_no(respr);
    IR_dt(call) = rettype;
    xoc::setLineNum(respr, lineno, m_rg);
    return respr;
}


IR * CTree2IR::convertCall(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    //Generate return-value type.
    xoc::Type const* rettype = convertCallReturnType(t, m_tm);

    //Generate callee.
    bool is_direct = true;
    IR * callee = convertCallee(t, &is_direct, cont);

    //Generate return-value-buffer.
    UINT return_val_size = 0;
    xoc::Var * retval_buf = nullptr;
    IR * arglist = convertCallReturnBuf(rettype, callee, &return_val_size,
                                        &retval_buf, cont);
    IR * call = convertCallItself(t, arglist, callee, is_direct, lineno, cont);

    //Generate return-exprssion.
    IR * retvalexp = convertCallReturnVal(call, return_val_size, retval_buf,
                                          rettype, lineno, cont);
    if (!CONT_is_compute_addr(cont)) { return retvalexp; }

    //CASE: given x = &(foo().arr);
    //User required taking the address of 'arr' and assign it to x.
    return convertTakenAddrOfCallRetVal(retvalexp, cont);
}


IR * CTree2IR::convertLogicalAND(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_LOGIC_AND);
    ASSERT0(cont);
    T2IRCtx tcont(*cont);
    IR * stmt_in_rchild = nullptr;
    IR * stmt_in_rchild_last = nullptr;

    //Override topirlist and corresponding last.
    CONT_toplirlist(&tcont) = &stmt_in_rchild;
    CONT_toplirlist_last(&tcont) = &stmt_in_rchild_last;
    IR * op0 = convert(TREE_lchild(t), cont);
    IR * op1 = convert(TREE_rchild(t), &tcont);
    if (tcont.getTopIRList() == nullptr) {
        IR * ir = m_irmgr->buildCmp(IR_LAND, op0, op1);
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //if (!cond1)
    //  res=false
    //else
    //  stmt-list
    //  if (cond2)
    //    res=true
    //  else
    //    res=false
    //  endif
    //endif
    UINT res_prno = m_irmgr->buildPrno(m_tm->getBool());
    if (!op0->is_judge()) {
        op0 = m_irmgr->buildJudge(op0);
    }
    op0 = IR::invertIRCode(op0, m_rg);
    if (!op0->is_judge()) {
        //op0 pointer changed.
        op0 = m_irmgr->buildJudge(op0);
    }
    IR * ifir1 = m_irmgr->buildIf(op0, nullptr, nullptr);
    IF_truebody(ifir1) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(false, m_tm->getBool()));
    IF_falsebody(ifir1) = tcont.getTopIRList();

    if (!op1->is_judge()) {
        op1 = m_irmgr->buildJudge(op1);
    }
    IR * ifir2 = m_irmgr->buildIf(op1, nullptr, nullptr);
    IF_truebody(ifir2) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(true, m_tm->getBool()));
    IF_falsebody(ifir2) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(false, m_tm->getBool()));
    xoc::setLineNum(ifir2, lineno, m_rg);

    xcom::add_next(&IF_falsebody(ifir1), ifir2);

    ifir1->setParentPointer(true);

    xoc::setLineNum(ifir1, lineno, m_rg);

    cont->appendTailOfTopLevelIRList(ifir1);
    return m_irmgr->buildPRdedicated(res_prno, m_tm->getBool());
}


IR * CTree2IR::convertLogicalOR(IN xfe::Tree * t, UINT lineno,
                                T2IRCtx * cont)
{
    ASSERT0(t->getCode() == TR_LOGIC_OR);
    ASSERT0(cont);
    T2IRCtx tcont(*cont);
    IR * stmt_in_rchild = nullptr;
    IR * stmt_in_rchild_last = nullptr;

    //Override topirlist and corresponding last.
    CONT_toplirlist(&tcont) = &stmt_in_rchild;
    CONT_toplirlist_last(&tcont) = &stmt_in_rchild_last;
    IR * op0 = convert(TREE_lchild(t), cont);
    IR * op1 = convert(TREE_rchild(t), &tcont);
    if (tcont.getTopIRList() == nullptr) {
        IR * ir = m_irmgr->buildCmp(IR_LOR, op0, op1);
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //if (cond1)
    //  res=true
    //else
    //  stmt-list
    //  if (cond2)
    //    res=true
    //  else
    //    res=false
    //  endif
    //endif
    UINT res_prno = m_irmgr->buildPrno(m_tm->getBool());
    if (!op0->is_judge()) {
        op0 = m_irmgr->buildJudge(op0);
    }
    IR * ifir1 = m_irmgr->buildIf(op0, nullptr, nullptr);
    IF_truebody(ifir1) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(true, m_tm->getBool()));
    IF_falsebody(ifir1) = tcont.getTopIRList();

    if (!op1->is_judge()) {
        op1 = m_irmgr->buildJudge(op1);
    }
    IR * ifir2 = m_irmgr->buildIf(op1, nullptr, nullptr);
    IF_truebody(ifir2) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(true, m_tm->getBool()));
    IF_falsebody(ifir2) = m_irmgr->buildStorePR(res_prno, m_tm->getBool(),
        m_irmgr->buildImmInt(false, m_tm->getBool()));
    xoc::setLineNum(ifir2, lineno, m_rg);
    xcom::add_next(&IF_falsebody(ifir1), ifir2);
    ifir1->setParentPointer(true);
    xoc::setLineNum(ifir1, lineno, m_rg);
    cont->appendTailOfTopLevelIRList(ifir1);
    return m_irmgr->buildPRdedicated(res_prno, m_tm->getBool());
}


IR * CTree2IR::convertFP(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    //An unsuffixed floating constant has type double. If suffixed
    //by the letter f or F, it has type float.
    //If suffixed by the letter l or L, it has type long double.

    //Convert string to hex value , that is in order to generate
    //single load instruction to load float point value during
    //Code Generator.
    Sym const* fp = TREE_fp_str_val(t);

    //Default float point type is 64bit.
    IR * ir = m_irmgr->buildImmFP(::atof(fp->getStr()),
        m_tm->getSimplexTypeEx(t->getCode() == TR_FPF ? D_F32 : D_F64));
    BYTE mantissa_num = getMantissaNum(fp->getStr());
    if (mantissa_num > DEFAULT_MANTISSA_NUM) {
        CONST_fp_mant(ir) = mantissa_num;
    }
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertPostIncDec(IN xfe::Tree * t, UINT lineno,
                                 T2IRCtx * cont)
{
    //CASE: int * a;
    //    return a++;
    //=>
    //    inc_exp = a
    //    a = inc_exp + 1
    //    return inc_exp
    IR_CODE irt;
    if (t->getCode() == TR_POST_INC) {
        irt = IR_ADD;
    } else {
        irt = IR_SUB;
    }

    //Get result IR, it must be pseduo register.
    T2IRCtx ct(*cont);

    //Post-increment-op has sideeffect, it has both WRITE and READ property.
    //Begin processing READ-property.
    CONT_is_lvalue(&ct) = false;

    //inc_exp is the base of post-dec/inc.
    IR * inc_exp = convert(TREE_inc_exp(t), &ct);
    bool is_ptr = inc_exp->is_ptr();
    xoc::Type const* type;
    if (is_ptr) {
        type = m_tm->getSimplexTypeEx(m_tm->getPointerSizeDtype());
    } else {
        type = inc_exp->getType();
    }

    IR * imm1 = nullptr;
    if (type->is_int()) {
        imm1 = m_irmgr->buildImmInt(1, type);
    } else {
        ASSERT0(type->is_fp());
        imm1 = m_irmgr->buildImmFP(1, type);
    }

    if (is_ptr) {
        type = inc_exp->getType();
    } else {
        type = m_tm->hoistDTypeForBinOp(inc_exp, imm1);
    }

    IR * addsub = m_irmgr->buildBinaryOp(irt, type, inc_exp, imm1);
    ASSERT0(addsub->is_ptr() == inc_exp->is_ptr());

    //Generate inc_exp = inc_exp + 1
    IR * xstpr = nullptr;
    IR * xincst = nullptr;
    if (inc_exp->is_ild()) {
        ASSERT0(ILD_base(inc_exp));
        //If inc_exp is ILD, generate IST(inc_exp's mem_loc) = ILD + 1.
        //
        //Both of Post Dec/Inc will take side effects for base region.
        //So we must append the side effect STORE operation as the last stmt,
        //and record the side-effect IR as epilog of current stmt.
        //e.g  *p++ = *p & 0xff, '++' effect should be record in epilog-field.
        //
        //Generate:
        //    pr = ild(x)
        //    ist(x) = ild(x) + 1
        //    return pr
        ASSERT0(ILD_base(inc_exp)->is_ptr());
        xstpr = m_irmgr->buildStorePR(inc_exp->getType(),
            m_rg->dupIRTree(inc_exp));
        xincst = m_irmgr->buildIStore(
            m_rg->dupIRTree(ILD_base(inc_exp)), addsub, inc_exp->getType());
    } else if (inc_exp->is_array()) {
        //If inc_exp is ARRAY, generate IST(ARRAY) = ARRAY + 1.
        //Generate:
        //    pr = array(x)
        //    array(x) = array(x) + 1
        //    return pr
        xstpr = m_irmgr->buildStorePR(inc_exp->getType(),
            m_rg->dupIRTree(inc_exp));
        xincst = m_irmgr->buildStoreArray(
            m_rg->dupIRTree(ARR_base(inc_exp)),
            m_rg->dupIRTreeList(ARR_sub_list(inc_exp)),
            inc_exp->getType(),
            ARR_elemtype(inc_exp),
            ((CArray*)inc_exp)->getDimNum(),
            ARR_elem_num_buf(inc_exp), addsub);
    } else {
        //Generate:
        // pr = x
        // x = x + 1
        // return pr
        if (!inc_exp->is_ld()) {
            err(lineno, "'%s' needs l-value",
                t->getCode() == TR_POST_INC ? "++" : "--");
            return nullptr;
        }
        xstpr = m_irmgr->buildStorePR(inc_exp->getType(),
            m_rg->dupIRTree(inc_exp));
        xincst = m_rg->dupIsomoStmt(inc_exp, addsub);
    }
    //Record source code info.
    xoc::setLineNum(xstpr, lineno, m_rg);
    xoc::setLineNum(xincst, lineno, m_rg);
    cont->appendTailOfTopLevelIRList(xstpr);

    //Record extra generated stmt.
    if (CONT_is_record_epilog(cont)) {
        cont->appendTailOfEpilogLevelIRList(xincst);
    } else {
        cont->appendTailOfTopLevelIRList(xincst);
    }
    return m_irmgr->buildPRdedicated(STPR_no(xstpr), xstpr->getType());
}


IR * CTree2IR::convertSwitch(IN xfe::Tree * t, UINT lineno, T2IRCtx *)
{
    IR * vexp = convert(TREE_switch_det(t), nullptr);
    vexp = only_left_last(vexp);
    ASSERT0(vexp);

    //Record current processing of TR_CASE.
    m_case_list_stack.push(m_case_list);
    List<CaseValue*> case_list;
    m_case_list = &case_list;

    IR * body = convert(TREE_switch_body(t), nullptr);

    #ifdef _DEBUG_
    bool find_default_lab = false; //indicate if we find the default label.
    #endif

    LabelInfo const* deflab = nullptr;
    IR * casev_list = nullptr;
    IR * last = nullptr;
    for (CaseValue * casev = case_list.get_head();
         casev != nullptr; casev = case_list.get_next()) {
        if (CASEV_is_default(casev)) {
            ASSERTN(!find_default_lab, ("redefined DEFAULT in SWITCH"));
            #ifdef _DEBUG_
            find_default_lab = true;
            #endif
            deflab = CASEV_lab(casev);
        } else {
            xoc::DATA_TYPE dt = m_tm->getAlignedDType(
                WORD_LENGTH_OF_TARGET_MACHINE, true);
            IR * imm = m_irmgr->buildImmInt(
                CASEV_constv(casev), m_tm->getSimplexTypeEx(dt));
            xcom::add_next(&casev_list, &last,
                           m_irmgr->buildCase(imm, CASEV_lab(casev)));
        }
    }

    m_case_list = m_case_list_stack.pop();

    IR * ir = m_irmgr->buildSwitch(vexp, casev_list, body, deflab);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


xoc::Var * CTree2IR::genLocalVar(xoc::Sym const* sym, xoc::Type const* ty)
{
    xoc::Var * v = m_rg->getVarMgr()->registerVar(
        sym, ty, STACK_ALIGNMENT, VAR_LOCAL, SS_UNDEF);
    m_rg->addToVarTab(v);
    return v;
}


xoc::Var * CTree2IR::genLocalVar(CHAR const* name, xoc::Type const* ty)
{
    Sym const* sym = m_rm->addToSymbolTab(name);
    return genLocalVar(sym, ty);
}


//Convert direct memory access.
//Return the value of field, or the address of field.
IR * CTree2IR::convertDirectMemAccess(
    xfe::Tree const* t, UINT lineno, T2IRCtx * cont)
{
    Decl const* base_decl = TREE_result_type(TREE_base_region(t));
    ASSERTN(!base_decl->isPointer(), ("base of dmem can not be pointer type"));

    TypeAttr const* ty = base_decl->getTypeAttr();
    ASSERTN(ty->isAggrExpanded(), ("base of dmem must be aggregate"));
    xfe::Tree const* field = TREE_field(t);
    ASSERTN(field->getCode() == TR_ID, ("field must be ID"));
    Decl const* field_decl = TREE_result_type(field);

    //Indicates whether if current convertion should return address.
    bool is_parent_require_addr = CONT_is_compute_addr(cont);
    T2IRCtx tc(*cont);
    IR * ir = convert(TREE_base_region(t), &tc);

    //Compute 'byte-ofst' of 'ir' according to field in structure type.
    UINT field_ofst = 0; //All field of union start at offset 0.
    ASSERTN(field->getCode() == TR_ID, ("illegal struct/union exp"));
    if (ty->isStructExpanded()) {
        if (!get_aggr_field(ty, TREE_id_name(field)->getStr(),
                            nullptr, &field_ofst)) {
            UNREACHABLE();
        }
    }

    //field_dt will be the result type of the DirectMemoryAccess.
    UINT field_size = 0;
    xoc::DATA_TYPE field_dt = CTree2IR::get_decl_dtype(
        field_decl, &field_size, m_tm);
    if (is_parent_require_addr) {
        //Parent tree node requires address.
        //The returned ir should be pointer.
        ASSERT0(ir->is_ptr());
        ir->setPointerType(field_size, m_tm);
        if (ir->hasOffset()) {
            ir->setOffset(ir->getOffset() + field_ofst);
        } else if (field_ofst != 0) {
            //ir is ADD, SUB, etc, and there is no offset field in ir.
            //Plus the offset of field in Aggr.
            ir = m_irmgr->buildBinaryOpSimp(IR_ADD, ir->getType(), ir,
                m_irmgr->buildImmInt(
                    field_ofst, m_tm->getSimplexTypeEx(
                        m_tm->getPointerSizeDtype())));
        }
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //Parent tree node requires value.
    ASSERT0(!ir->is_lda());
    if (TREE_base_region(t)->getCode() == TR_CALL && ir->isPROp()) {
        //CASE:If base-region of Aggr Operation is IR_CALL, we have to utilize
        //local memory to implement the operation, because PR does not have
        //OFFSET.
        ir = convertFieldAccessForReturnValAggr(TREE_base_region(t), ir, cont);
    }
    ASSERT0(ir->hasOffset());
    ir->setOffset(ir->getOffset() + field_ofst);

    //Revise result type of 'ir' according to field_dt.
    reviseResultTypeViaFieldType(ir, field_dt, field_size, field_decl, m_tm);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


//Convert indirect memory access.
//Return the value of field, or the address of field.
IR * CTree2IR::convertIndirectMemAccess(xfe::Tree const* t, UINT lineno,
                                        T2IRCtx * cont)
{
    Decl const* base_decl = TREE_result_type(TREE_base_region(t));
    ASSERTN(base_decl->isPointer(), ("'->' node must be pointer type"));

    TypeAttr const* ty = base_decl->getTypeAttr();
    ASSERTN(ty->isAggrExpanded(), ("illegal base type"));
    ASSERTN(TREE_field(t)->getCode() == TR_ID, ("illegal offset type"));

    //Compute 'field_offst' according to 'field' of struct.
    UINT field_ofst = 0; //All field of union start at offset 0.
    if (ty->isStructExpanded()) {
        if (!get_aggr_field(ty, TREE_id_name(TREE_field(t))->getStr(),
                            nullptr, &field_ofst)) {
            ASSERT0(0);
        }
    }

    Decl const* field_decl = TREE_result_type(TREE_field(t));

    T2IRCtx tc(*cont);

    //INDEM node always computes the value of base, whether if parent
    //node required.
    CONT_is_compute_addr(&tc) = false;
    IR * base = convert(TREE_base_region(t), &tc);
    ASSERTN(base->is_ptr(),
            ("base of indirect memory access must be pointer."));

    UINT field_size = 0;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(field_decl, &field_size, m_tm);
    if (CONT_is_compute_addr(cont) || field_decl->is_array()) {
        //Parent node requires address.
        //Compute the address of field.

        //CASE: If field is just array identifier, return the address
        //expression of field, rather than value of whole array.
        //e.g1: struct { char b[] } * p; p->s.b;
        //e.g2: struct { char b   } * p; &(p->s.b);
        //Both e.g1 and e.g2 generetes:
        //  t1 = ld(p) + ofst(b)
        //where does not have to generete ILD.

        IR * ir = nullptr;
        if (field_ofst != 0) {
            ir = m_irmgr->buildBinaryOpSimp(IR_ADD, base->getType(),
                base, m_irmgr->buildImmInt(field_ofst,
                    m_tm->getSimplexTypeEx(
                        m_tm->getPointerSizeDtype())));
        } else {
            ir = base;
        }
        ir->setPointerType(field_size, m_tm);
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //Compute the value of field.
    if (dt == D_MC) {
        //Field is memory block, means user wants to the value of memory block.
        //Return the value of field.
        IR * ir = m_irmgr->buildILoad(base, m_tm->getMCType(field_size));
        if (field_ofst != 0) {
            ir->setOffset(field_ofst);
        }
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    xoc::Type const* type = nullptr;
    if (dt == D_PTR) {
        //e.g: struct { ... char * b ... } * p; then p->b will generate:
        type = m_tm->getPointerType(field_decl->getPointerBaseSize());
        ASSERT0(type->verify(m_rg->getTypeMgr()));
    } else {
        //e.g: struct { ... int b ... } * p; then p->b will generate:
        ASSERT0(IS_SIMPLEX(dt));
        type = m_tm->getSimplexTypeEx(dt);
    }

    //Return the value of field.
    //Generate ild to access field's value.
    //CASE1: struct { ... char * b ... } * p; then p->b will generate:
    //CASE2: struct { ... int b ... } * p; then p->b will generate:
    //  field_ofst = offset(b)
    //  t1 = ld(p)
    //  t2 = ild:field_ofst (t1)
    IR * ir = m_irmgr->buildILoad(base, type);
    if (field_ofst != 0) {
        ir->setOffset(field_ofst);
    }

    ASSERT0(ir);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertDeref(IN xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    xfe::Tree * base = TREE_lchild(t);
    IR * deref_addr = convert(base, cont);
    ASSERT0(deref_addr && deref_addr->is_ptr());
    if (CONT_is_compute_addr(cont)) { return deref_addr; }

    //Compute value by dereferencing pointer.
    if (base->getResultType()->isPointerPointToArray()) {
        return convertDerefPointToArray(deref_addr, t, base, m_tm,
                                        m_rg, lineno);
    }
    IR * ir = m_irmgr->buildILoad(deref_addr, determineIRCode(t, m_tm));
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertSelect(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    //t->getResultType() is nullptr, may be it is necessary.
    IR * det = convert(TREE_det(t), cont);
    ASSERT0(det);

    //true and false part may be list in C spec.
    IR * texp = convert(TREE_true_part(t), cont);
    IR * fexp = convert(TREE_false_part(t), cont);
    ASSERT0(texp && fexp);
    while (IR_next(texp) != nullptr) {
        m_rg->freeIRTree(xcom::removehead(&texp));
    }
    while (IR_next(fexp) != nullptr) {
        m_rg->freeIRTree(xcom::removehead(&fexp));
    }

    //Determine the IR data type.
    xoc::Type const* d0 = texp->getType();
    xoc::Type const* d1 = fexp->getType();
    ASSERT0(d0 && d1);
    //Try to infer result type of SELECT operation.
    xoc::Type const* type = nullptr;
    if (d0->is_pointer() || d1->is_pointer()) {
        //Pointer should have same ptr-base-size.
        if (texp->is_const() && CONST_int_val(texp) == 0) {
            type = fexp->getType();
        } else if (fexp->is_const() && CONST_int_val(fexp) == 0) {
            type = texp->getType();
        } else {
            type = d0;
            //Pointer base size may be differnt.
            //e.g: char * xx;
            //     cond ? xx : "abcdef"; #S1
            // Data type of xx in #s1 is *<1>, data type of constant string
            // "abcdef" is *<4>.
            //ASSERT0(d0 == d1);
        }
    } else if (d0->is_mc() || d1->is_mc()) {
        //Should be same MC type.
        ASSERT0(d0 == d1);
        type = texp->getType();
    } else if (d0->is_any() || d1->is_any()) {
        //C language permits either TrueExp or FalseExp to be VOID.
        if (d0->is_any()) {
            type = d1;
        } else {
            type = d0;
        }
    } else {
        type = m_tm->hoistDTypeForBinOp(texp, fexp);
        if (texp->getType() != type) {
            IR * cvt = m_irmgr->buildCvt(texp, type);
            texp = cvt;
        }
        if (fexp->getType() != type) {
            IR * cvt = m_irmgr->buildCvt(fexp, type);
            fexp = cvt;
        }
    }
    if (!det->is_bool()) {
        IR * old = det;
        det = m_irmgr->buildJudge(det);
        copyDbx(det, old, m_rg);
    }
    IR * ir = m_irmgr->buildSelect(det, texp, fexp, type);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


//Generate a xoc::Var if the bytesize of RETURN is bigger than total size of
//return-registers.
//e.g: Given 32bit target machine, the return register is a0, a1,
//If the return type is structure whose size is bigger than 64bit, we need
//to generate an implcitly xoc::Var to indicate the stack buffer which used
//to hold the return value.
IR * CTree2IR::genReturnValBuf(IR * ir)
{
    ASSERT0(ir->is_return());
    IR * retval = RET_exp(ir);
    if (retval == nullptr) { return ir; }

    //If memory size of return-value is too big to store in
    //return registers, generate a return-value-buffer and
    //transfering its address as the first implict parameter to
    //the call.
    UINT return_val_bytesize = m_tm->getByteSize(retval->getType());
    if (return_val_bytesize <=
        m_rg->getTargInfo()->getNumOfReturnValueRegister() *
            GENERAL_REGISTER_SIZE) {
        return ir;
    }
    if (m_retval_buf == nullptr) {
        CHAR const* rname = m_rg->getRegionName();
        ASSERT0(rname);
        xcom::StrBuf tmp((UINT)(::strlen(rname) + 32));
        tmp.sprint("#param_retvalbuf_of_%sbyte", rname);
        xoc::Type const* ptr = m_rg->getTypeMgr()->getPointerType(
            retval->getTypeSize(m_rg->getTypeMgr()));
        //retval_buf only used in current region.
        m_retval_buf = genLocalVar(tmp.buf, ptr);
        m_retval_buf->setToFormalParam();
        VAR_formal_param_pos(m_retval_buf) = g_formal_parameter_start - 1;
    }
    ASSERT0(VAR_formal_param_pos(m_retval_buf) ==
            g_formal_parameter_start - 1);
    IR * ist = m_irmgr->buildIStore(
        m_irmgr->buildLoad(m_retval_buf), retval, retval->getType());
    RET_exp(ir) = nullptr;
    xcom::add_next(&ist, ir);
    return ist;
}


//Generate CVT if type conversion from src to tgt is indispensable.
xoc::Type const* CTree2IR::checkAndGenCVTType(
    Decl const* tgt, Decl const* src)
{
    ASSERT0(tgt && src);
    UINT tgt_dt_sz, src_dt_sz;
    xoc::DATA_TYPE tgt_dt = CTree2IR::get_decl_dtype(tgt, &tgt_dt_sz, m_tm);
    xoc::DATA_TYPE src_dt = CTree2IR::get_decl_dtype(src, &src_dt_sz, m_tm);

    bool need_cvt = false;
    if (IS_FP(src_dt)) {
        if (IS_INT(tgt_dt) ||
            IS_BOOL(tgt_dt) ||
            IS_PTR(tgt_dt) ||
            (IS_FP(tgt_dt) && tgt_dt_sz != src_dt_sz)) {
            need_cvt = true;
        }
    } else if (IS_FP(tgt_dt)) {
        if (IS_INT(src_dt) ||
            IS_BOOL(src_dt) ||
            IS_PTR(src_dt) ||
            (IS_FP(src_dt) && tgt_dt_sz != src_dt_sz)) {
            need_cvt = true;
        }
    } else if (IS_INT(tgt_dt) && tgt_dt_sz > GENERAL_REGISTER_SIZE) {
        if (IS_INT(src_dt) && tgt_dt_sz > src_dt_sz) {
            //Integer extension.
            need_cvt = true;
        }
    }

    if (!need_cvt) { return nullptr; }

    xoc::Type const* type = nullptr;
    if (IS_SIMPLEX(tgt_dt)) {
        type = m_tm->getSimplexTypeEx(tgt_dt);
    } else if (IS_PTR(tgt_dt)) {
        type = m_tm->getPointerType(tgt->getPointerBaseSize());
    } else {
        UNREACHABLE();
    }

    return type;
}


IR * CTree2IR::convertReturn(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    IR * ir = m_irmgr->buildReturn(convert(TREE_ret_exp(t), cont));
    if (RET_exp(ir) != nullptr) {
        ASSERTN(IR_next(RET_exp(ir)) == nullptr, ("unsupport"));
        ir->setParentPointer(false);
        xoc::Type const* cvttype = checkAndGenCVTType(
            getDeclaredReturnType(), TREE_ret_exp(t)->getResultType());
        if (cvttype != nullptr) {
            IR * cvt = m_irmgr->buildCvt(RET_exp(ir), cvttype);
            RET_exp(ir) = cvt;
            ir->setParent(cvt);
        }
    }
    xoc::setLineNum(ir, lineno, m_rg);
    ir = genReturnValBuf(ir);
    return ir;
}


IR * CTree2IR::convertLDA(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    xfe::Tree * kid = TREE_lchild(t);

    //Actually, convertor does not have to do anything. The processing of kid
    //should consider the situation that its parent is LDA.
    T2IRCtx tc(*cont);
    IR * base = nullptr;
    if (kid->getCode() == TR_DEREF) {
        //LDA operator will counteract the effect of DEREF, just leave the
        //type of DEREF as the pointer-base-type of LDA.
        //e.g: &*(int*)p;  where p is char*.
        //  The result type of LDA is int*.
        CONT_is_compute_addr(&tc) = false;
        base = convert(TREE_lchild(kid), &tc);
        base->setPointerType(kid->getResultType()->getDeclByteSize(), m_tm);
        xoc::setLineNum(base, lineno, m_rg);
        return base;
    }
    if (kid->getCode() == TR_ID) {
        base = buildLda(kid);
        ASSERT0(base->is_lda());
        xoc::setLineNum(base, lineno, m_rg);
        return base;
    }
    CONT_is_compute_addr(&tc) = true;
    base = convert(kid, &tc);

    //Use t's pointer type as the output ir's type. Because the pointer
    //base size that computed by 'kid' may be not same with 't'.
    ASSERT0(t->getResultType()->regardAsPointer());
    base->setPointerType(t->getResultType()->getPointerBaseSize(), m_tm);
    if (base->is_ld() || base->is_array() || base->is_ild() ||
        base->is_lda()) {
        xoc::setLineNum(base, lineno, m_rg);
        return base;
    }
    if (base->is_add() && base->is_ptr()) {
        //e.g:struct S { int a; int b; } ** p;
        //    ... = &((*p)->b);
        //For now base will be:
        //  ADD (PTR)
        //    ILD (PTR)
        //      LD ('p', PTR)
        //    IMM(4)
        //It already be the address what we want, just
        //return the ADD as result.
        xoc::setLineNum(base, lineno, m_rg);
        return base;
    }
    UNREACHABLE();
    return nullptr;
}


IR * CTree2IR::convertCVT(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    IR * ir = nullptr;
    Decl * cvtype = TREE_type_name(TREE_cvt_type(t));
    if (cvtype->regardAsPointer()) {
        //decl is pointer variable
        ir = m_irmgr->buildCvt(convert(TREE_cvt_exp(t), cont),
            m_tm->getPointerType(cvtype->getPointerBaseSize()));
    } else {
        UINT size = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(cvtype, &size, m_tm);
        xoc::Type const* type = nullptr;
        if (IS_SIMPLEX(dt)) {
            type = m_tm->getSimplexTypeEx(dt);
        } else if (IS_PTR(dt)) {
            type = m_tm->getPointerType(size);
        } else if (IS_MC(dt)) {
            type = m_tm->getMCType(size);
        } else {
            UNREACHABLE();
        }
        ir = m_irmgr->buildCvt(convert(TREE_cvt_exp(t), cont), type);
    }
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertId(xfe::Tree * t, UINT lineno, T2IRCtx * cont)
{
    IR * ir = nullptr;
    Decl * idty = t->getResultType();
    ASSERT0(idty);
    if (idty->is_array()) {
        //In C language, identifier of array is just a label.
        //The reference of the label should be represented as LDA of the label.
        //e.g: int * p; int a[10]; p = a;
        //  will generate: ST(p) = LDA(a). Thus the LDA will be inserted in
        //  TreeCanon stage.
        ASSERTN(TREE_parent(t)->getCode() != TR_ARRAY,
                ("should be handled in TreeCanon"));
    }

    if (idty->is_fun_decl()) {
        //If current xfe::Tree is fun-decl, there will be two case:
        //tree is the callee or parameter of call.
        if (cont != nullptr && CONT_is_parse_callee(cont)) {
            //tree is the callee.
            ir = buildId(t);
        } else {
            //In C, function name referrence can be represented
            //as LDA operation.
            //e.g: f takes the address of hook.
            //void hook();
            //void foo() {
            //    typedef void (*F)();
            //    F f = hook;
            //}
            ir = buildLda(t);
            ASSERT0(LDA_idinfo(ir)->is_func());
        }
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    if (CONT_is_compute_addr(cont)) {
        //In this case, ancestors tree should include TR_LDA operator,
        ASSERT0_DUMMYUSE(isAncestorsIncludeLDA(t));
        ir = buildLda(t);
        ASSERT0(ir->is_lda());
        //ir = buildId(t);
        xoc::setLineNum(ir, lineno, m_rg);
        return ir;
    }

    if (idty->is_bitfield()) {
        //e.g: int a:3;
        //     int b:15;
        //    b = 10;
        //=>
        //    pr1=10;
        //    pr1=pr1<<3;
        //    id(b)=ld(b) bor pr1;
        UNREACHABLE();
    }

    //Normal load.
    ir = buildLoad(t);
    xoc::setLineNum(ir, lineno, m_rg);
    return ir;
}


//Given a list of IR, only keep the leftmost.
IR * CTree2IR::only_left_last(IR * head)
{
    IR * last = removetail(&head);
    while (head != nullptr) {
        IR * t = xcom::removehead(&head);
        m_rg->freeIRTree(t);
    }
    return last;
}


//Return the number of mantissa.
BYTE CTree2IR::getMantissaNum(CHAR const* fpval)
{
    ASSERT0(fpval);
    BYTE mantissa_num = 0;
    bool mantissa_start = false;
    for (CHAR const* p = fpval; *p != 0; p++) {
        if (mantissa_start) {
            mantissa_num++;
        }
        if (*p == '.') {
            mantissa_start = true;
        }
    }

    return mantissa_num;
}


class VFToBody {
public:
    xcom::TTab<IR*> ir_tab;
public:
    VFToBody() {}
    bool visitIR(IR * ir, OUT bool & is_terminate)
    {
        if (ir->isCFSThatControlledBySCO()) {
            //Do NOT access these structure's kid.
            return false;
        }
        if (ir->is_continue()) {
            ir_tab.append(ir);
        }
        return true;
    }
};


static void prependStepBeforeContinue(IR const* step, IR * body, Region * rg)
{
    class IterTree : public VisitIRTree<VFToBody> {
    public:
        IterTree(VFToBody & vf) : VisitIRTree(vf) {}
    };
    if (step == nullptr) { return; }
    VFToBody vf;
    IterTree it(vf);
    it.visitWithSibling(body);
    xcom::TTabIter<IR*> irit;
    for (IR * t = vf.ir_tab.get_first(irit);
         t != nullptr; t = vf.ir_tab.get_next(irit)) {
        ASSERT0(t->is_continue());
        IR * dup_step = rg->dupIRTreeList(step);
        IR * prev = t->get_prev();
        IR * parent = t->getParent();
        ASSERTN(parent, ("dangled IR"));
        if (prev != nullptr) {
            xcom::insertafter(&prev, dup_step);
            parent->setParent(dup_step);
        } else {
            IR * dup_cnt = rg->dupIRTree(t);
            xcom::add_next(&dup_step, dup_cnt);
            parent->replaceKidWithIRList(t, dup_step, true);
        }
    }
}


static IR * convertRev(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * opnd = t2ir->convert(TREE_lchild(t), cont);
    IR * ir = irmgr->buildUnaryOp(IR_BNOT, opnd->getType(), opnd);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertMinus(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * opnd = t2ir->convert(TREE_lchild(t), cont);
    IR * ir = irmgr->buildUnaryOp(IR_NEG, opnd->getType(), opnd);
    xoc::setLineNum(ir, lineno, rg);
    ir->setParentPointer(false);
    return ir;
}


static IR * convertDefault(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * ir = irmgr->buildIlabel();
    CaseValue * casev = (CaseValue*)t2ir->xmalloc(sizeof(CaseValue));
    CASEV_lab(casev) = LAB_lab(ir);
    CASEV_constv(casev) = 0;
    CASEV_is_default(casev) = true;
    ASSERT0(t2ir->getCaseList());
    t2ir->getCaseList()->append_head(casev);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertCase(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * ir = irmgr->buildIlabel();
    CaseValue * casev = (CaseValue*)t2ir->xmalloc(sizeof(CaseValue));
    CASEV_lab(casev) = LAB_lab(ir);
    CASEV_constv(casev) = TREE_case_value(t);
    CASEV_is_default(casev) = false;
    ASSERT0(t2ir->getCaseList());
    t2ir->getCaseList()->append_tail(casev);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertFor(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * last = nullptr;
    IR * init = t2ir->convert(TREE_for_init(t), nullptr);
    IR * ir = nullptr;
    xcom::add_next(&ir, &last, init);

    T2IRCtx ct2;
    ASSERT0(cont);
    IR * stmt_in_det = nullptr;
    IR * stmt_in_det_last = nullptr;

    //Override topirlist and corresponding last.
    CONT_toplirlist(&ct2) = &stmt_in_det;
    CONT_toplirlist_last(&ct2) = &stmt_in_det_last;
    IR * det = t2ir->convert(TREE_for_det(t), &ct2);
    if (stmt_in_det != nullptr) {
        xcom::add_next(&ir, &last, stmt_in_det);
    }

    det = t2ir->only_left_last(det);
    if (det == nullptr) {
        det = irmgr->buildJudge(irmgr->buildImmInt(1, tm->getI32()));
    }

    IR * body = t2ir->convert(TREE_for_body(t), nullptr);

    //The step ir-list is only used for reference.
    //Duplicate it if needed.
    IR * step_readonly = t2ir->convert(TREE_for_step(t), nullptr);
    xcom::add_next(&body, rg->dupIRTreeList(step_readonly));
    if (stmt_in_det != nullptr) {
        xcom::add_next(&body, rg->dupIRTreeList(stmt_in_det));
    }
    if (!det->is_judge()) {
        IR * old = det;
        det = irmgr->buildJudge(det);
        copyDbx(det, old, rg);
    }

    IR * whiledo = irmgr->buildWhileDo(det, body);
    xoc::setLineNum(whiledo, lineno, rg);
    xcom::add_next(&ir, &last, whiledo);
    prependStepBeforeContinue(step_readonly, body, rg);
    rg->freeIRTreeList(step_readonly);
    return ir;
}


static IR * convertWhileDo(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont,
    MOD IR ** ir_list, MOD IR ** ir_list_last)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * prolog = nullptr;
    IR * epilog = nullptr;
    IR * prolog_last = nullptr;
    IR * epilog_last = nullptr;
    T2IRCtx ct2;

    //Override topirlist and corresponding last.
    CONT_toplirlist(&ct2) = &prolog;
    CONT_toplirlist_last(&ct2) = &prolog_last;

    //Override epilogirlist and corresponding last.
    CONT_epilogirlist(&ct2) = &epilog;
    CONT_epilogirlist_last(&ct2) = &epilog_last;
    CONT_is_record_epilog(&ct2) = false;

    IR * det = t2ir->convert(TREE_whiledo_det(t), &ct2);
    det = t2ir->only_left_last(det);
    if (det == nullptr) {
        det = irmgr->buildJudge(irmgr->buildImmInt(1, tm->getI32()));
    }

    IR * dup_prolog = nullptr;
    if (prolog != nullptr) {
        dup_prolog = rg->dupIRTreeList(prolog);
        xcom::add_next(ir_list, ir_list_last, prolog);
    }
    ASSERT0(epilog == nullptr);

    IR * body = t2ir->convert(TREE_whiledo_body(t), nullptr);
    if (dup_prolog != nullptr) {
        xcom::add_next(&body, dup_prolog);
    }

    if (!det->is_judge()) {
        IR * old = det;
        det = irmgr->buildJudge(det);
        copyDbx(det, old, rg);
    }

    IR * ir = irmgr->buildWhileDo(det, body);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertDoWhile(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * prolog = nullptr;
    IR * epilog = nullptr;
    IR * prolog_last = nullptr;
    IR * epilog_last = nullptr;
    T2IRCtx ct2;

    //Override topirlist and corresponding last.
    CONT_toplirlist(&ct2) = &prolog;
    CONT_toplirlist_last(&ct2) = &prolog_last;

    //Override epilogirlist and corresponding last.
    CONT_epilogirlist(&ct2) = &epilog;
    CONT_epilogirlist_last(&ct2) = &epilog_last;
    CONT_is_record_epilog(&ct2) = true;

    IR * det = t2ir->convert(TREE_dowhile_det(t), &ct2);
    det = t2ir->only_left_last(det);
    if (det == nullptr) {
        det = irmgr->buildJudge(
            irmgr->buildImmInt(1, tm->getI32()));
    }

    if (prolog != nullptr) {
        //Do NOT add prolog before DO_WHILE stmt.
        //dup_prolog = rg->dupIRTreeList(prolog);
        //xcom::add_next(&ir_list, &ir_list_last, prolog);
    }

    IR * body = t2ir->convert(TREE_dowhile_body(t), nullptr);
    if (prolog != nullptr) {
        //Put prolog at end of body.
        xcom::add_next(&body, prolog);
    }
    if (epilog != nullptr) {
        xcom::add_next(&body, epilog);
    }

    if (!det->is_judge()) {
        IR * old = det;
        det = irmgr->buildJudge(det);
        xoc::copyDbx(det, old, rg);
    }
    IR * ir = irmgr->buildDoWhile(det, body);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertIF(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * det = t2ir->convert(TREE_if_det(t), cont);
    det = t2ir->only_left_last(det);
    if (det == nullptr) {
        det = irmgr->buildJudge(
            irmgr->buildImmInt(1, tm->getI32()));
    }

    IR * truebody = t2ir->convert(TREE_if_true_stmt(t), nullptr);
    IR * falsebody = t2ir->convert(TREE_if_false_stmt(t), nullptr);
    if (!det->is_judge()) {
        IR * old = det;
        det = irmgr->buildJudge(det);
        copyDbx(det, old, rg);
    }
    IR * ir = irmgr->buildIf(det, truebody, falsebody);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertMulti(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    IR * ir;
    if (TREE_token(t) == T_ASTERISK) {
        ir = irmgr->buildBinaryOp(IR_MUL, type, l, r);
    } else if (TREE_token(t) == T_DIV) {
        ir = irmgr->buildBinaryOp(IR_DIV, type, l, r);
    } else {
        ir = irmgr->buildBinaryOp(IR_REM, type, l, r);
    }
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertAdditive(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    IR * ir;
    if (TREE_token(t) == T_ADD) {
        IR_dt(l) = type;
        ir = irmgr->buildBinaryOp(IR_ADD, type, l, r);
    } else {
        ir = irmgr->buildBinaryOp(IR_SUB, type, l, r);
    }
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertShift(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    IR * ir;
    if (TREE_token(t) == T_RSHIFT) {
        ir = irmgr->buildBinaryOp(l->is_signed() ?
            IR_ASR : IR_LSR, type, l, r);
    } else {
        ir = irmgr->buildBinaryOp(IR_LSL, type, l, r);
    }
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertRelation(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    IR * ir;
    switch (TREE_token(t)) {
    case T_LESSTHAN:     // <
        ir = irmgr->buildCmp(IR_LT, l, r);
        break;
    case T_MORETHAN:     // >
        ir = irmgr->buildCmp(IR_GT, l, r);
        break;
    case T_NOLESSTHAN:   // >=
        ir = irmgr->buildCmp(IR_GE, l, r);
        break;
    case T_NOMORETHAN:   // <=
        ir = irmgr->buildCmp(IR_LE, l, r);
        break;
    default: UNREACHABLE();
    }
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertEQ(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    IR * ir;
    if (TREE_token(t) == T_EQU) {
        ir = irmgr->buildCmp(IR_EQ, l, r);
    } else {
        ir = irmgr->buildCmp(IR_NE, l, r);
    }
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertExclusiveOR(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    return irmgr->buildBinaryOp(IR_XOR, type, l, r);
}


static IR * convertInclusiveAND(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    IR * ir = irmgr->buildBinaryOp(IR_BAND, type, l, r);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertInclusiveOR(
    xfe::Tree * t, UINT lineno, CTree2IR * t2ir, T2IRCtx * cont)
{
    TypeMgr * tm = t2ir->getTypeMgr();
    IRMgr * irmgr = t2ir->getIRMgr();
    Region * rg = t2ir->getRegion();
    IR * l = t2ir->convert(TREE_lchild(t), cont);
    IR * r = t2ir->convert(TREE_rchild(t), cont);
    xoc::Type const* type = nullptr;
    if (t->getResultType()->regardAsPointer()) {
        type = tm->getPointerType(t->getResultType()->
            getPointerBaseSize());
    } else {
        UINT s = 0;
        xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
            t->getResultType(), &s, tm);
        if (dt == D_MC) {
            type = tm->getMCType(s);
        } else {
            type = tm->getSimplexTypeEx(dt);
        }
    }
    IR * ir = irmgr->buildBinaryOp(IR_BOR, type, l, r);
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertString(xfe::Tree * t, UINT lineno, CTree2IR * t2ir)
{
    Region * rg = t2ir->getRegion();
    IR * ir = t2ir->getIRMgr()->buildLdaString(nullptr, TREE_string_val(t));
    ASSERT0(ir->is_lda() &&
            LDA_idinfo(ir)->is_global() &&
            rg->getTopRegion() != nullptr &&
            rg->getTopRegion()->is_program());
    rg->getTopRegion()->addToVarTab(LDA_idinfo(ir));
    xoc::setLineNum(ir, lineno, rg);
    return ir;
}


static IR * convertEnum(xfe::Tree * t, UINT lineno, CTree2IR * t2ir)
{
    INT v = get_enum_const_val(TREE_enum(t),TREE_enum_val_idx(t));
    //Default const type of enumerator is 'unsigned int'
    //of target machine
    IR * ir = t2ir->getIRMgr()->buildImmInt(
        v, t2ir->getTypeMgr()->getSimplexTypeEx(
        t2ir->getTypeMgr()->getAlignedDType(
            WORD_LENGTH_OF_TARGET_MACHINE, true)));
    xoc::setLineNum(ir, lineno, t2ir->getRegion());
    return ir;
}


static IR * convertImmL(xfe::Tree * t, UINT lineno, CTree2IR * t2ir)
{
    UINT s = 0;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
        t->getResultType(), &s, t2ir->getTypeMgr());
    ASSERT0(dt == D_I64 || dt == D_U64);

    //The maximum integer supported is 64bit.
    IR * ir = t2ir->getIRMgr()->buildImmInt(TREE_imm_val(t),
        t2ir->getTypeMgr()->getSimplexTypeEx(dt));
    xoc::setLineNum(ir, lineno, t2ir->getRegion());
    return ir;
}


static IR * convertImm(xfe::Tree * t, UINT lineno, CTree2IR * t2ir)
{
    UINT s = 0;
    xoc::DATA_TYPE dt = CTree2IR::get_decl_dtype(
        t->getResultType(), &s, t2ir->getTypeMgr());

    //The maximum integer supported is 64bit.
    IR * ir = t2ir->getIRMgr()->buildImmInt(TREE_imm_val(t),
        t2ir->getTypeMgr()->getSimplexTypeEx(dt));
    xoc::setLineNum(ir, lineno, t2ir->getRegion());
    return ir;
}


//Convert TREE AST to IR.
IR * CTree2IR::convert(IN xfe::Tree * t, T2IRCtx * cont)
{
    IR * ir = nullptr;
    IR * ir_list = nullptr; //record ir list generated.
    IR * ir_list_last = nullptr;
    T2IRCtx ct;
    if (cont == nullptr) {
        cont = &ct;
        CONT_toplirlist(cont) = &ir_list;
        CONT_toplirlist_last(cont) = &ir_list_last;
    }
    while (t != nullptr) {
        UINT lineno = TREE_lineno(t);
        switch (t->getCode()) {
        case TR_ASSIGN:
            ir = convertAssign(t, lineno, cont);
            break;
        case TR_ID:
            ir = convertId(t, lineno, cont);
            break;
        case TR_IMM:
        case TR_IMMU:
            ir = convertImm(t, lineno, this);
            break;
        case TR_IMML:
        case TR_IMMUL:
            ir = convertImmL(t, lineno, this);
            break;
        case TR_FP:
        case TR_FPF:
        case TR_FPLD:
            ir = convertFP(t, lineno, cont);
            break;
        case TR_ENUM_CONST:
            ir = convertEnum(t, lineno, this);
            break;
        case TR_STRING:
            ir = convertString(t, lineno, this);
           break;
        case TR_LOGIC_OR: //logical OR ||
            ir = convertLogicalOR(t, lineno, cont);
            break;
        case TR_LOGIC_AND: //logical AND &&
            ir = convertLogicalAND(t, lineno, cont);
            break;
        case TR_INCLUSIVE_OR: //inclusive OR |
            ir = convertInclusiveOR(t, lineno, this, cont);
            break;
        case TR_INCLUSIVE_AND: //inclusive AND &
            ir = convertInclusiveAND(t, lineno, this, cont);
            break;
        case TR_XOR: //exclusive or
            ir = convertExclusiveOR(t, lineno, this, cont);
            break;
        case TR_EQUALITY: // == !=
            ir = convertEQ(t, lineno, this, cont);
            break;
        case TR_RELATION: // < > >= <=
            ir = convertRelation(t, lineno, this, cont);
            break;
        case TR_SHIFT: // >> <<
            ir = convertShift(t, lineno, this, cont);
            break;
        case TR_ADDITIVE: // '+' '-'
            ir = convertAdditive(t, lineno, this, cont);
            break;
        case TR_MULTI: // '*' '/' '%'
            ir = convertMulti(t, lineno, this, cont);
            break;
        case TR_SCOPE:
            ASSERT0(TREE_scope(t));
            ir = convert(TREE_scope(t)->getStmtList(), nullptr);
            break;
        case TR_IF:
            ir = convertIF(t, lineno, this, cont);
            break;
        case TR_DO:
            ir = convertDoWhile(t, lineno, this, cont);
            break;
        case TR_WHILE:
            ir = convertWhileDo(t, lineno, this, cont, &ir_list, &ir_list_last);
            break;
        case TR_FOR:
            ir = convertFor(t, lineno, this, cont);
            break;
        case TR_SWITCH:
            ir = convertSwitch(t, lineno, cont);
            break;
        case TR_BREAK:
            ir = m_irmgr->buildBreak();
            xoc::setLineNum(ir, lineno, m_rg);
            break;
        case TR_CONTINUE:
            ir = m_irmgr->buildContinue();
            xoc::setLineNum(ir, lineno, m_rg);
            break;
        case TR_RETURN:
            ir = convertReturn(t, lineno, cont);
            break;
        case TR_GOTO:
            ir = m_irmgr->buildGoto(getUniqueLabel(
                const_cast<LabelInfo*>(TREE_lab_info(t))));
            xoc::setLineNum(ir, lineno, m_rg);
            break;
        case TR_LABEL:
            ir = m_irmgr->buildLabel(getUniqueLabel(
                const_cast<LabelInfo*>(TREE_lab_info(t))));
            xoc::setLineNum(ir, lineno, m_rg);
            break;
        case TR_CASE:
            ir = convertCase(t, lineno, this, cont);
            break;
        case TR_DEFAULT:
            ir = convertDefault(t, lineno, this, cont);
            break;
        case TR_COND: //formulized log_OR_exp ? exp : cond_exp
            ir = convertSelect(t, lineno, cont);
            break;
        case TR_CVT: //type convertion
            ir = convertCVT(t, lineno, cont);
            break;
        case TR_LDA: // &a, get address of 'a'
            ir = convertLDA(t, lineno, cont);
            break;
        case TR_DEREF: //*p dereferencing the pointer 'p'
            ir = convertDeref(t, lineno, cont);
            break;
        case TR_PLUS: // +123
            ir = convert(TREE_lchild(t), cont);
            break;
        case TR_MINUS: // -123
            ir = convertMinus(t, lineno, this, cont);
            break;
        case TR_REV: // Reverse
            ir = convertRev(t, lineno, this, cont);
            break;
        case TR_NOT: // get non-value
            ir = m_irmgr->buildLogicalNot(convert(TREE_lchild(t), cont));
            xoc::setLineNum(ir, lineno, m_rg);
            break;
        case TR_INC: //++a
        case TR_DEC: //--a
            ir = convertIncDec(t, lineno, cont);
            break;
        case TR_POST_INC: //a++  / (*a)++
        case TR_POST_DEC: //a--
            ir = convertPostIncDec(t, lineno, cont);
            break;
        case TR_DMEM:
            ir = convertDirectMemAccess(t, lineno, cont);
            break;
        case TR_INDMEM:
            ir = convertIndirectMemAccess(t, lineno, cont);
            break;
        case TR_ARRAY:
            ir = convertArray(t, lineno, cont);
            break;
        case TR_CALL:
            ir = convertCall(t, lineno, cont);
            break;
        case TR_PRAGMA:
            ir = convertPragma(t, lineno, cont);
            break;
        case TR_PREP:
        case TR_DECL:
            break;
        default: ASSERTN(0, ("unknown tree type:%d", t->getCode()));
        }
        t = TREE_nsib(t);
        if (ir != nullptr) {
            if (xoc::getLineNum(ir, xoc::LANG_CPP, m_rg->getDbxMgr()) == 0) {
                xoc::setLineNum(ir, lineno, m_rg);
            }
            xcom::add_next(&ir_list, &ir_list_last, ir);
        }
        ir = nullptr;
    }
    return ir_list;
}


//Return XOC data type according to given CFE declaration.
//If 'decl' presents a simply type, convert type-spec to xoc::DATA_TYPE
//descriptor.
//If 'decl' presents a pointer type, convert pointer-type to D_PTR.
//If 'decl' presents an array, convert type to D_M descriptor.
//size: return byte size of decl.
xoc::DATA_TYPE CTree2IR::get_decl_dtype(
    Decl const* decl, UINT * size, xoc::TypeMgr * tm)
{
    ASSERT0(decl && tm);
    xoc::DATA_TYPE dtype = xoc::D_UNDEF;
    *size = 0;
    ASSERTN(DECL_dt(decl) == DCL_DECLARATION ||
            DECL_dt(decl) == DCL_TYPE_NAME, ("TODO"));
    if (decl->regardAsPointer()) {
        *size = BYTE_PER_POINTER;
        return xoc::D_PTR;
    }
    if (decl->is_array()) {
        dtype = xoc::D_MC;
        *size = decl->getDeclByteSize();
        return dtype;
    }
    TypeAttr * ty = decl->getTypeAttr();
    bool is_signed;
    if (ty->is_unsigned()) {
        is_signed = false;
    } else {
        is_signed = true;
    }

    ASSERTN(ty, ("Type-SPEC in DCRLARATION cannot be nullptr"));
    *size = ty->getSpecTypeSize();
    if (ty->is_void() || ty->is_integer()) {
        dtype = tm->getIntDType(*size * BIT_PER_BYTE, is_signed);
    } else if (ty->is_fp()) {
        dtype = tm->getFPDType(*size * BIT_PER_BYTE, false);
    } else if (ty->is_aggr()) {
        dtype = xoc::D_MC;
        ASSERT0(*size == decl->getDeclByteSize());
    } else if (ty->is_user_type_ref()) {
        ty = ty->getPureTypeAttr();

        //USER Type should NOT be here.
        ASSERTN(0, ("You should factorize the type specification "
                    "into pure type during declaration()"));

        //dtype = get_decl_dtype(USER_TYPE_decl(TYPE_user_type(ty)), size);
    } else {
        ASSERTN(0, ("failed in xoc::DATA_TYPE converting"));
    }
    return dtype;
}
//END CTree2IR


//
//START CScopeIR
//
//Count up the number of local-variables.
void CScope2IR::scanDeclList(
    Scope const* s, OUT xoc::Region * rg, bool scan_sib)
{
    if (s == nullptr) { return; }

    for (Decl * decl = s->getDeclList();
         decl != nullptr; decl = DECL_next(decl)) {
        if (decl->is_formal_param() && decl->getDeclSym() == nullptr) {
            //void function(void*), parameter does not have name.
            continue;
        }
        if (decl->is_user_type_decl()) {
            //Note type-variable that defined by 'typedef'
            //will not be mapped to xoc::Var.
            continue;
        }

        xoc::Var * v = m_dvmap.mapDecl2Var(decl);
        ASSERTN(v, ("nullptr variable correspond to"));
        if (v->is_global()) {
            xoc::Region * topru = rg->getTopRegion();
            ASSERT0(topru);
            if (topru->is_program()) {
                topru->addToVarTab(v);
            }
        } else {
            rg->addToVarTab(v);
        }
    }

    scanDeclList(SCOPE_sub(s), rg, true);

    if (scan_sib) {
        scanDeclList(SCOPE_nsibling(s), rg, true);
    }
}


//Ensure IR_RETURN at the end of function if its return-type is ANY.
IR * CScope2IR::addReturn(IR * irs, Region * rg)
{
    IR * last = xcom::get_last(irs);
    if (last == nullptr) {
        return rg->getIRMgr()->buildReturn(nullptr);
    }

    if (!last->is_return()) {
        IR * ret = rg->getIRMgr()->buildReturn(nullptr);
        if (irs == nullptr) {
            irs = ret;
        } else {
            xcom::insertafter_one(&last, ret);
        }
    }
    return irs;
}


//retty: declared return-type of function. It could be NULL if there is no
//       return-type.
bool CScope2IR::convertTreeStmtList(xfe::Tree * stmts, Region * rg,
                                    Decl const* retty)
{
    if (stmts == nullptr) { return true; }

    //Convert C-language AST into XOC IR.
    CTree2IR ct2ir(rg, retty, m_dvmap);
    xoc::IR * irs = ct2ir.convert(stmts, nullptr);
    if (g_err_msg_list.has_msg()) {
        return false;
    }

    //Convertion is successful.
    xoc::RegionMgr * rm = rg->getRegionMgr();
    if (xoc::g_dump_opt.isDumpAll()) {
        xoc::note(rm, "\n==---- AFTER TREE2IR CONVERT '%s' -----==",
                  rg->getRegionName());
        rg->getLogMgr()->incIndent(2);
        xoc::dumpIRListH(irs, rg);
        rg->getLogMgr()->decIndent(2);
    }

    if (rg->is_function()) {
        //Ensure RETURN IR at the end of function
        //if its return-type is ANY.
        irs = addReturn(irs, rg);
    }

    //In order to sanitize and optimize IRs, we have to canonicalize IR tree
    //into well-formed layout that conform to the guidelines of XOC IR pass
    //preferred.
    Canon ic(rg);
    bool change = false;
    irs = ic.handle_stmt_list(irs, change);

    //Reshape IR tree to well formed outlook.
    if (xoc::g_dump_opt.isDumpAll()) {
        xoc::note(rm, "\n==---- TREE2IR: AFTER CANONICALE IR '%s' -----==",
                  rg->getRegionName());
        rg->getLogMgr()->incIndent(2);
        xoc::dumpIRListH(irs, rg);
        rg->getLogMgr()->decIndent(2);
    }

    //Refine and perform peephole optimizations.
    xoc::OptCtx oc(rg);
    xoc::RefineCtx rc(&oc);

    //Do not perform following optimizations.
    RC_refine_div_const(rc) = false;
    RC_refine_mul_const(rc) = false;
    RC_maintain_du(rc) = false; //DU still not ready.

    rg->initPassMgr(); //Optimizations needs PassMgr.
    rg->initDbxMgr();
    rg->initIRMgr();
    rg->initIRBBMgr();
    xoc::Refine * rf = (Refine*)rg->getPassMgr()->registerPass(PASS_REFINE);
    bool change_refine = false;
    irs = rf->refineIRList(irs, change_refine, rc);
    ASSERT0(xoc::IRMgr::verify(rg));
    ASSERT0(xoc::verifyIRList(irs, nullptr, rg));
    ASSERT0(irs);
    rg->addToIRList(irs);
    if (xoc::g_dump_opt.isDumpAll()) {
        xoc::note(rm, "\n==---- TREE2IR: AFTER REFINE IR '%s' -----==",
                  rg->getRegionName());
        rg->getLogMgr()->incIndent(2);
        xoc::dumpIRListH(rg->getIRList(), rg);
        rg->getLogMgr()->decIndent(2);
    }
    return true;
}


//Convert C-language AST into XOC IR.
//NOTICE: Before the converting, declaration must wire up a XOC xoc::Var.
bool CScope2IR::generateFuncRegion(Decl * dcl, OUT CLRegionMgr * rm)
{
    ASSERT0(DECL_is_fun_def(dcl));

    //In C language, there are two levels kind of region, program region and
    //function region. Program region describes the whole program that begins
    //from 'main()' function, and function region describes the normal function
    //in C-spec.

    //Each region needs a xoc::Var.
    xoc::Var * rvar = m_dvmap.mapDecl2Var(dcl);
    ASSERT0(rvar);

    //Start a timer to evaluate compilation speed.
    START_TIMER_FMT(t, ("GenerateFuncRegion '%s'",
                        rvar->get_name()->getStr()));

    //Generates a function region to describe current C-language function.
    xoc::Region * r = rm->newRegion(xoc::REGION_FUNC);
    r->setRegionVar(rvar);
    r->initAttachInfoMgr();
    r->initPassMgr();
    r->initDbxMgr();
    r->initIRMgr();
    r->initIRBBMgr();
    REGION_is_expect_inline(r) = dcl->is_inline();

    //To faciliate RegionMgr to manage the resource of each Region, you have to
    //add it explicitly.
    rm->addToRegionTab(r);

    //Build the relationship of current region and its parent region.
    ASSERT0(rm->getProgramRegion());
    REGION_parent(r) = rm->getProgramRegion();

    //Add current function region into VarTab of program region.
    REGION_parent(r)->addToVarTab(r->getRegionVar());

    //Build current function region to be a
    //Region IR of program region.
    rm->getProgramRegion()->addToIRList(
        rm->getProgramRegion()->getIRMgr()->buildRegion(r));

    //Itertive scanning scope to collect and append
    //all local-variable into VarTab of current region.
    scanDeclList(DECL_fun_body(dcl), r, false);
    if (xoc::g_dump_opt.isDumpAll()) {
        DECL_fun_body(dcl)->dump();
    }
    if (!convertTreeStmtList(DECL_fun_body(dcl)->getStmtList(), r,
                             dcl->getReturnType())) {
        return false;
    }
    END_TIMER_FMT(t, ("GenerateFuncRegion '%s'",
                      r->getRegionVar()->get_name()->getStr()));
    return true;
}


bool CScope2IR::scanProgramDeclList(Scope const* s, OUT xoc::Region * rg)
{
    //Iterates each declarations that is in program region/scope.
    for (Decl * dcl = s->getDeclList(); dcl != nullptr; dcl = DECL_next(dcl)) {
        if (dcl->is_fun_decl()) {
            if (dcl->is_fun_def()) {
                //'dcl' is function definition.
                if (!generateFuncRegion(
                        dcl, (CLRegionMgr*)rg->getRegionMgr())) {
                    return false;
                }
                if (g_err_msg_list.has_msg()) {
                    return false;
                }
                continue;
            }

            //'dcl' is function declaration, not definition.
            xoc::Var * v = m_dvmap.mapDecl2Var(dcl);
            if (v != nullptr) {
                //Note type-variable that defined by 'typedef'
                //will not be mapped to xoc::Var.
                v->setFlag((VAR_FLAG)(VAR_IS_FUNC|VAR_IS_DECL|VAR_IS_REGION));
                rg->addToVarTab(v);
            }
            continue;
        }

        //'dcl' has to be normal variable declaration, or function declaration.
        //rather than function definition.
        xoc::Var * v = m_dvmap.mapDecl2Var(dcl);
        if (v != nullptr) {
            rg->addToVarTab(v);
        }
    }
    return true;
}


//Construct Region and convert C-Language-Ast to XOC IR.
bool CScope2IR::generateScope(Scope const* s)
{
    START_TIMER(t, "Scope2IR");

    //Generate Program region.
    Region * program = m_rm->newRegion(REGION_PROGRAM);
    program->initAttachInfoMgr();
    program->initPassMgr();
    program->initDbxMgr();
    program->initIRMgr();
    program->initIRBBMgr();
    m_rm->addToRegionTab(program);
    m_rm->setProgramRegion(program);
    program->setRegionVar(
        program->getVarMgr()->registerVar(".program",
        m_rm->getTypeMgr()->getMCType(0), 1, VAR_GLOBAL|VAR_FAKE, SS_UNDEF));
    scanProgramDeclList(s, program);
    if (!convertTreeStmtList(s->getStmtList(), program, nullptr)) {
        return false;
    }

    //Free the temprary memory pool.
    END_TIMER(t, "CScope2IR");
    return true;
}
//END CScope2IR

} //namespace xocc
