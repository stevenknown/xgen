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
#include "../xgen/xgeninc.h"
#include "feinc.h"

#define RETVAL_BUFFER_NAME "retval_buf_of_non_name_func_pointer"

//Check if t is formed in following:
//e.g:s.bg[3] = ...
//  ARRAY
//   |---3
//   |----DMEM/INDMEM
//             |----s
//             |----bg
static bool is_field_ref_whole_array(Tree const* t)
{
    if (TREE_type(t) != TR_DMEM && TREE_type(t) != TR_INDMEM) { return false; }

    Tree * parent = TREE_parent(t);
    if (parent == nullptr) { return false; }

    Decl const* field_decl = TREE_result_type(TREE_field(t));
    if (is_array(field_decl) && TREE_type(parent) != TR_ARRAY) { return true; }

    return false;
}


//Generate IR for Tree identifier.
//And calculate the byte-size of identifier.
IR * CTree2IR::buildId(IN Decl * id)
{
    Var * var_info = mapDecl2VAR(id);
    ASSERT0(var_info);
    return m_rg->buildId(var_info);
}


//Generate IR for Identifier.
//Calculate the byte-size of identifier.
IR * CTree2IR::buildId(IN Tree * t)
{
    ASSERTN(TREE_type(t) == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl * decl = TREE_id_decl(t);
    //TREE_result_type is useless for C,
    //but keep it for another langages used.
    return buildId(decl);
}


IR * CTree2IR::buildLda(Tree const* t)
{
    ASSERTN(TREE_type(t) == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl * decl = TREE_id_decl(t);
    Var * var_info = mapDecl2VAR(decl);
    ASSERT0(var_info);
    return m_rg->buildLda(var_info);
}


IR * CTree2IR::buildLoad(IN Tree * t)
{
    ASSERTN(TREE_type(t) == TR_ID, ("illegal tree node , expected TR_ID"));
    Decl * decl = TREE_id_decl(t);
    ASSERT0(decl);
    Var * var_info = mapDecl2VAR(decl);
    ASSERT0(var_info);
    return m_rg->buildLoad(var_info);
}


bool CTree2IR::is_istore_lhs(Tree const* t) const
{
    ASSERT0(t != nullptr &&
            (TR_ASSIGN == TREE_type(TREE_parent(t))) &&
            t == TREE_lchild(TREE_parent(t)));
    return TREE_type(t) == TR_DEREF;
}


IR * CTree2IR::convertAssign(IN Tree * t, INT lineno, IN T2IRCtx * cont)
{
    //One of '='   '*='   '/='   '%='  '+='
    //       '-='  '<<='  '>>='  '&='  '^='  '|='
    IR * ist_mem_addr = nullptr; //record mem_addr if 't' is an ISTORE
    IR * epilog_ir_list = nullptr;
    CONT_is_lvalue(cont) = true;
    CONT_is_record_epilog(cont) = true;
    CONT_epilogirlist(cont) = &epilog_ir_list;
    IR * l = convert(TREE_lchild(t), cont);
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
    Type const* rtype = nullptr;
    if (is_pointer(TREE_result_type(t))) {
        rtype = m_tm->getPointerType(get_pointer_base_size(
            TREE_result_type(t)));
    } else {
        UINT s;
        DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t), &s, m_tm);
        if (IS_SIMPLEX(dt)) {
            rtype = m_tm->getSimplexTypeEx(dt);
        } else {
            //Memory Chunk type.
            ASSERT0(dt == D_MC);
            rtype = m_tm->getMCType(s);
        }
    }

    //Convert RHS of tree.
    CONT_is_lvalue(cont) = false;
    IR * r = convert(TREE_rchild(t), cont);

    IR * ir = nullptr;
    switch (TREE_token(t)) {
    Type const* type;
    case T_ASSIGN:
        if (ist_mem_addr != nullptr) {
            if (is_ild_array_case) {
                IR * tmpir = m_rg->buildStorePR(ist_mem_addr->getType(),
                                                ist_mem_addr);
                ASSERTN(tmpir->is_ptr(),
                        ("I think tmpir should already be set to"
                         "pointer in buildStore()"));
                setLineNum(tmpir, lineno, m_rg);
                xcom::add_next(CONT_toplirlist(cont), tmpir);
                ir = m_rg->buildIStore(m_rg->buildPRdedicated(
                    STPR_no(tmpir), tmpir->getType()), r, rtype);
            } else {
                ir = m_rg->buildIStore(ist_mem_addr, r, rtype);
            }
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            //Normalize LHS.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), r);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            //Normalize LHS.
            ir = m_rg->buildStoreArray(ARR_base(l), ARR_sub_list(l),
                                       l->getType(), ARR_elemtype(l),
                                       ((CArray*)l)->getDimNum(),
                                       ARR_elem_num_buf(l), r);
            ARR_base(l) = nullptr;
            ARR_sub_list(l) = nullptr;
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        m_rg->freeIRTree(l); //l is useless.
        break;
    case T_BITANDEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }

        ir = m_rg->buildBinaryOp(IR_BAND, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else {
            //Generate IR_ST.
            ASSERT0(l->is_ld());

            //Normalize LHS.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        }
        break;
    case T_BITOREQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_BOR, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else {
            //Generate IR_ST.
            ASSERT0(l->is_ld());

            //Normalize LHS.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        }
        break;
    case T_ADDEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }

        ir = m_rg->buildBinaryOp(IR_ADD, type, l, r);

        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(
                m_rg->dupIRTree(ARR_base(l)),
                m_rg->dupIRTree(ARR_sub_list(l)), l->getType(),
                                ARR_elemtype(l), ((CArray*)l)->getDimNum(),
                                ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_SUBEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_SUB, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(
                m_rg->dupIRTree(ARR_base(l)),
                m_rg->dupIRTree(ARR_sub_list(l)), l->getType(),
                                ARR_elemtype(l), ((CArray*)l)->getDimNum(),
                                ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_MULEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_MUL, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(
                m_rg->dupIRTree(ARR_base(l)),
                m_rg->dupIRTree(ARR_sub_list(l)), l->getType(),
                                ARR_elemtype(l), ((CArray*)l)->getDimNum(),
                                ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_DIVEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_DIV, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(
                m_rg->dupIRTree(ARR_base(l)),
                m_rg->dupIRTree(ARR_sub_list(l)), l->getType(),
                                ARR_elemtype(l), ((CArray*)l)->getDimNum(),
                                ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_XOREQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_XOR, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(
                m_rg->dupIRTree(ARR_base(l)),
                m_rg->dupIRTree(ARR_sub_list(l)), l->getType(),
                                ARR_elemtype(l), ((CArray*)l)->getDimNum(),
                                ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_RSHIFTEQU:
        ASSERTN(!l->is_fp() && !r->is_fp(),
                ("illegal shift operation of float point"));
        ASSERTN(r->is_int(), ("type of shift-right should be integer"));
        if (r->is_signed()) {
            IR_dt(r) = m_tm->getSimplexTypeEx(
                m_tm->get_int_dtype(
                    m_tm->getDTypeBitSize(
                        TY_dtype(r->getType())), false));
        }

        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(l->is_sint() ? IR_ASR : IR_LSR, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(m_rg->dupIRTree(ARR_base(l)),
                                       m_rg->dupIRTree(ARR_sub_list(l)),
                                       l->getType(), ARR_elemtype(l),
                                       ((CArray*)l)->getDimNum(),
                                       ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_LSHIFTEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }
        ir = m_rg->buildBinaryOp(IR_LSL, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(m_rg->dupIRTree(ARR_base(l)),
                                       m_rg->dupIRTree(ARR_sub_list(l)),
                                       l->getType(),
                                       ARR_elemtype(l),
                                       ((CArray*)l)->getDimNum(),
                                       ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    case T_REMEQU:
        if (!l->is_ptr() && !r->is_ptr()) {
            type = m_tm->hoistDtypeForBinop(l, r);
        } else {
            type = m_tm->getAny();
            //buildBinaryOp will inefer the type of result ir.
        }

        ir = m_rg->buildBinaryOp(IR_REM, type, l, r);
        if (ist_mem_addr != nullptr) {
            ir = m_rg->buildIStore(ist_mem_addr, ir, rtype);
            ir->setOffset(ir->getOffset() + l->getOffset());
        } else if (l->is_ld()) {
            //Generate IR_ST.
            ir = m_rg->buildStore(LD_idinfo(l), l->getType(), ir);
            ir->setOffset(ir->getOffset() + LD_ofst(l));
        } else if (l->is_array()) {
            //Generate IR_STARRAY.
            ir = m_rg->buildStoreArray(m_rg->dupIRTree(ARR_base(l)),
                                       m_rg->dupIRTree(ARR_sub_list(l)),
                                       l->getType(),
                                       ARR_elemtype(l),
                                       ((CArray*)l)->getDimNum(),
                                       ARR_elem_num_buf(l), r);
            ir->setOffset(ARR_ofst(l));
        } else {
            ASSERTN(0, ("unsupport lhs IR type."));
        }
        break;
    default: UNREACHABLE();
    }

    ASSERT0(ir->is_st() || ir->is_starray() || ir->is_ist());
    if (is_pointer(TREE_result_type(t))) {
        ir->setPointerType(get_pointer_base_size(TREE_result_type(t)), m_tm);
    }

    CONT_is_record_epilog(cont) = false;
    setLineNum(ir, lineno, m_rg);
    xcom::add_next(CONT_toplirlist(cont), ir);

    //Record the post side effect operations.
    xcom::add_next(CONT_toplirlist(cont), epilog_ir_list);

    ir = xcom::get_last(ir);

    //return rhs as the result of STMT.
    IR * retv = ir->getRHS();
    ASSERT0(retv);
    ir = m_rg->dupIRTree(retv);
    return ir;
}


//Convert prefix ++/--:
// ++a => a=a+1; return a;
// --a => a=a-1; return a;
// *++a=0 => a=a+1; *a=0;
// b=*++a => a=a+1; b=*a;
// b=++*a => *a=*a+1; b=*a;
IR * CTree2IR::convertIncDec(IN Tree * t, INT lineno, IN T2IRCtx * cont)
{
    //Generate base region IR node used into ST
    T2IRCtx ct(*cont);
    CONT_toplirlist(&ct) = CONT_toplirlist(cont);

    //In C spec, pre-increment-op has sideeffect, it has WRITE-property.
    //e.g: ++p, p is inc_exp.
    CONT_is_lvalue(&ct) = true;

    IR * inc_exp = convert(TREE_inc_exp(t), &ct); //need ID

    CONT_is_lvalue(&ct) = false; //begin processing READ-property.

    //Compute pointer addend.
    INT addend = 1;
    Type const* addend_type = inc_exp->getType();
    ASSERT0(!inc_exp->is_mc());
    if (inc_exp->is_ptr()) {
        addend = TY_ptr_base_size(inc_exp->getType());
        addend_type = m_tm->getSimplexTypeEx(m_tm->getPointerSizeDtype());
    }
    IR * imm = m_rg->buildImmInt(addend, addend_type);

    //Generate ADD/SUB.
    IR_TYPE irt = IR_UNDEF;
    if (TREE_type(t) == TR_INC) {
        irt = IR_ADD;
    } else if (TREE_type(t) == TR_DEC) {
        irt = IR_SUB;
    } else {
        UNREACHABLE();
    }
    IR * ir = m_rg->buildBinaryOpSimp(irt, addend_type, inc_exp, imm);

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
        ir = m_rg->buildIStore(m_rg->dupIRTree(ILD_base(inc_exp)),
            ir, inc_exp->getType());
    } else if (inc_exp->is_array()) {
        ir = m_rg->buildStoreArray(
            m_rg->dupIRTree(ARR_base(inc_exp)),
            m_rg->dupIRTreeList(ARR_sub_list(inc_exp)),
            inc_exp->getType(),
            ARR_elemtype(inc_exp),
            ((CArray*)inc_exp)->getDimNum(),
            ARR_elem_num_buf(inc_exp),
            ir);
    } else {
        ASSERT0(inc_exp->is_ld());
        ir = m_rg->buildStore(LD_idinfo(inc_exp), ir);
    }

    //ST is statement, and only can be appended on top level
    //statement list.
    setLineNum(ir, lineno, m_rg);
    xcom::add_next(CONT_toplirlist(cont), ir);
    return m_rg->dupIRTree(inc_exp);
}


IR * CTree2IR::convertPointerDeref(Tree * t, INT lineno, IN T2IRCtx * cont)
{
    ASSERT0(TREE_type(t) == TR_ARRAY);
    ASSERT0(is_pointer(TREE_result_type(TREE_array_base(t))));

    //CASE 2. Pointer but used as array.
    //     char * p;
    //     p[i] = 11; //=> ST(ILD(LD(p)+LD(i)), 11) => IST((LD(p)+LD(i)), 11)
    //     a = p[i]; //=> ST(a, ILD(LD(p)+LD(i)))

    //We are in CASE 2, dealing with pointer, but using array-operator.
    //Operator [] is dereference of pointer actually.

    T2IRCtx tc(*cont);

    //Base and Index have to be value whether or not Caller requires address.
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
    IR * mem_addr = m_rg->buildBinaryOp(IR_ADD, base->getType(),
                                        base, index);
    ASSERT0(mem_addr->is_ptr() &&
            TY_ptr_base_size(mem_addr->getType()) != 0);
    Type const* type = nullptr;
    if (is_pointer(TREE_result_type(t))) {
        type = m_tm->getPointerType(get_pointer_base_size(
            TREE_result_type(t)));
    } else {
        UINT s;
        DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t), &s, m_tm);
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
        ir = m_rg->buildILoad(ir, type);
    }
    setLineNum(ir, lineno, m_rg);
    return ir;
}


//Convert array element type, the result type of array operator as well.
//t: array operator Tree node.
static Type const* computeArrayElementType(Tree const* t, TypeMgr * tm)
{
    UINT size;
    DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t), &size, tm);
    if (dt == D_PTR) {
        return tm->getPointerType(get_pointer_base_size(TREE_result_type(t)));
    }

    if (dt == D_MC) {
        return tm->getMCType(size);
    }

    ASSERT0(IS_SIMPLEX(dt));
    return tm->getSimplexTypeEx(dt);
}


static IR * computeArrayAddr(Tree * t, IR * ir, Region * rg, T2IRCtx * cont)
{
    ASSERT0(ir->isArrayOp() && TREE_type(t) == TR_ARRAY);
    SimpCtx tc;
    SIMP_array(&tc) = true;
    IR * newir = rg->simplifyArrayAddrExp(ir, &tc);
    rg->freeIRTree(ir);
    ir = newir;
    ASSERT0(ir->is_ptr());

    if (is_array(TREE_result_type(t))) {
        //We are in CASE 3.
        //Revise pointer-base size according to
        //the pointed array dimension.
        TY_ptr_base_size(ir->getType()) = get_array_elem_bytesize(
            TREE_result_type(t));
    }

    if (SIMP_stmtlist(&tc) != nullptr) {
        xcom::add_next(CONT_toplirlist(cont), SIMP_stmtlist(&tc));
    }
    return ir;
}


//base: base Tree node of ARRAY.
IR * CTree2IR::convertArraySubExp(Tree * t, TMWORD * elem_nums, T2IRCtx * cont)
{
    ASSERT0(TREE_type(t) == TR_ARRAY);
    Tree * base = t;
    UINT n = 0;
    while (TREE_type(base) == TR_ARRAY) {
        base = TREE_array_base(base);
        ASSERT0(base);
        n++;
    }
    ASSERT0(n >= 1);

    Decl * arr_decl = TREE_result_type(base);
    Tree * lt = t;
    UINT dim = n - 1;
    IR * sublist = nullptr;
    IR * last = nullptr;
    UINT i = 0;

    //Note the outermost TR_ARRAY means the lowest dimension element of array
    //will be accessed.
    T2IRCtx tc(*cont);

    //Do not propagate information to subscribe-expression convertion.
    CONT_is_compute_addr(&tc) = false;

    while (TREE_type(lt) == TR_ARRAY) {
        IR * subexp = convert(TREE_array_indx(lt), &tc);
        ASSERT0(subexp);
        xcom::add_next(&sublist, &last, subexp);
        elem_nums[i] = (TMWORD)get_array_elemnum_to_dim(arr_decl, dim);
        lt = TREE_array_base(lt);
        dim--;
        i++;
    }
    return sublist;
}


IR * CTree2IR::convertArray(Tree * t, INT lineno, IN T2IRCtx * cont)
{
    ASSERT0(TREE_type(t) == TR_ARRAY);
    //There are following situations need to be resolved:
    //CASE 1. Regular array.
    //     char p[100];
    //     p[i] = 11; //=> ST(ILD(LDA(p)+LD(i)), 11) => IST((LDA(p)+LD(i)), 11)
    //     a = p[i]; //=> a=ILD(LDA(p)+LD(i))
    //     p is base, i is index.
    //
    //CASE 2. The base of ARRAY operator is a pointer, but is used as array.
    //     char * p;
    //     p[i] = 11; //=> ST(ILD(LD(p)+LD(i)), 11) => IST((LD(p)+LD(i)), 11)
    //     a = p[i]; //=> ST(a, ILD(LD(p)+LD(i)))
    //
    //CASE 3. Taken regular array element address.
    //     char p[2][3];
    //     char * q = p[i]+1; //=> ST(q, LDA(p)+LD(i)+1)
    //     char * w = &p[i][j]; //=> ST(w, LDA(p)+LD(i)*3+LD(j))
    //
    //CASE 4. Taken regular array element address.
    //     char p[2][3];
    //     char * w = &p[i][j]; //=> ST(w, LDA(p)+LD(i)*3+LD(j))
    //
    //CASE 5: Taken array address.
    //     char p[2];
    //     char * q = p;
    //----------------------------------------------------------------
    if (is_pointer(TREE_result_type(TREE_array_base(t)))) {
        return convertPointerDeref(t, lineno, cont);
    }

    //Handle regular array.
    //Compute the number of elements for each dimensions.
    Tree * basetree = t;
    UINT n = 0;
    while (TREE_type(basetree) == TR_ARRAY) {
        basetree = TREE_array_base(basetree);
        ASSERT0(basetree);
        n++;
    }
    ASSERT0(n >= 1);

    T2IRCtx tc(*cont);

    //IR_ARRAY operator requires that the base have to POINTER type.
    CONT_is_compute_addr(&tc) = true;

    //Convert base of array.
    IR * base = convert(basetree, &tc);
    if (base->is_array()) {
        //CASE:
        //struct S {int w; short e[100];};
        //struct S s[100][200];
        //short a1 = s[5][6].e[7];
        //
        //The base of e[7] is still an array.
        SimpCtx cont2;
        SIMP_ret_array_val(&cont2) = false;
        SIMP_array(&cont2) = true;
        base = m_rg->simplifyExpression(base, &cont2);
        ASSERT0(SIMP_stmtlist(&cont2) == nullptr);
    }
    ASSERT0(base->is_ptr());

    //Convert subscript-expression.
    TMWORD * elem_nums = (TMWORD*)::malloc(sizeof(TMWORD) * n);
    IR * sublist = convertArraySubExp(t, elem_nums, cont);
    ASSERT0(sublist);

    //Convert array element type, and result type of array operator.
    Type const* type = computeArrayElementType(t, m_tm);

    //There is C language specific situtation, 'base' of array is either
    //a pointer or another array. Do calibration if that is the case.
    if (TREE_type(TREE_array_base(t)) != TR_ARRAY && !base->is_ptr()) {
        //Revise array base's data-type.
        base->setPointerType(base->getTypeSize(m_tm), m_tm);
    }

    IR * ir = m_rg->buildArray(base, sublist, type, type, n, elem_nums);

    if (is_array(TREE_result_type(t)) || CONT_is_compute_addr(cont)) {
        //If current tree node is just array symbol reference, then we have to
        //take the address of the array, whereas we are in CASE 5.
        ir = computeArrayAddr(t, ir, m_rg, cont);
    }

    ::free(elem_nums);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertPragma(IN Tree * t, INT lineno, IN T2IRCtx * cont)
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
    IR * ir = m_rg->buildLabel(m_rg->genPragmaLabel(buf.buf));
    setLineNum(ir, lineno, m_rg);
    return ir;
}


//Return true if function do not modify any variable.
bool CTree2IR::is_readonly(Var const* v) const
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
bool CTree2IR::is_alloc_heap(Var const* v) const
{
    CHAR const* vname = SYM_name(VAR_name(v));
    if (::strcmp(vname, "malloc") == 0 ||
        ::strcmp(vname, "alloca") == 0 ||
        ::strcmp(vname, "calloc") == 0) {
        return true;
    }
    return false;
}


IR * CTree2IR::convertCall(IN Tree * t, INT lineno, IN T2IRCtx * cont)
{
    //Generate return value type.
    Type const* rettype = nullptr;
    if (is_pointer(TREE_result_type(t))) {
        rettype = m_tm->getPointerType(
            get_pointer_base_size(TREE_result_type(t)));
    } else {
        UINT size = 0;
        DATA_TYPE dt = get_decl_dtype(TREE_result_type(t), &size, m_tm);
        if (dt == D_MC) {
            rettype = m_tm->getMCType(size);
        } else {
            rettype = m_tm->getSimplexTypeEx(dt);
        }
    }

    //Generate callee.
    T2IRCtx tcont(*cont);
    CONT_is_parse_callee(&tcont) = true;
    IR * callee = convert(TREE_fun_exp(t), &tcont);

    //----------
    //Recognize ICALL
    IR * real_callee = callee;
    while (real_callee->is_ild()) {
        //This is an indirect call.
        //e.g:given int (*f)();
        //In C, even the clueless syntax is legal: (***********f)();
        real_callee = ILD_base(real_callee);
    }

    //Omit the CVT operation.
    while (!real_callee->is_id() &&
           !(real_callee->is_int() && real_callee->is_const()) &&
           !real_callee->is_ld() &&
           !real_callee->is_lda()) {
        if (real_callee->is_cvt()) {
            if (CVT_exp(real_callee)->is_const()) {
                //keep CVT around with const.
                //e.g: function call via literal (*0)();
                break;
            }
            real_callee = CVT_exp(real_callee);
        } else {
            ASSERTN(0, ("illegal IR expression"));
        }
    }

    if (real_callee != callee) {
        IR * old = callee;
        callee = m_rg->dupIRTree(real_callee);
        m_rg->freeIRTree(old);
    }

    if (!callee->is_ptr()) {
        //Make sure calllee is pointer-type.
        callee->setPointerType(4, m_tm);
    }

    bool is_direct = true;
    if (!callee->is_id()) {
        if (callee->is_ptr()) {
            is_direct = false; //Current call is indirect function call.
            ASSERTN(callee->is_ld() || callee->is_lda() || callee->is_cvt(),
                    ("enable more case"));
        } else if (callee->is_const() && callee->is_int()) {
            is_direct = false; //Current call is indirect function call.
        } else {
            UNREACHABLE(); //unsupport.
        }
    }
    //----------

    //Handle return value buffer.
    //If memory size of return-value is too big to store in
    //return registers, generate a return-value-buffer and
    //transfering its address as the first implict parameter to
    //the call.
    IR * callarglist = nullptr;
    Var * retval_buf = nullptr;
    UINT return_val_size = m_tm->getByteSize(rettype);
    if (return_val_size > NUM_OF_RETURN_VAL_REGISTERS *
                          GENERAL_REGISTER_SIZE) {
        xcom::StrBuf tmp(64);

        //WorkAround: We always translate TR_ID into LD(ID),
        //here it is callee actually.
        ASSERT0(callee->is_id());
        ASSERT0(ID_info(callee));
        tmp.sprint("$retval_buf_of_%d_bytes", return_val_size);
        Sym const* name = m_rg->getRegionMgr()->addToSymbolTab(tmp.buf);
        retval_buf = m_rg->getVarMgr()->findVarByName(name);
        if (retval_buf == nullptr) {
            retval_buf = m_rg->getVarMgr()->registerVar(SYM_name(name),
                rettype, STACK_ALIGNMENT, VAR_LOCAL);
            //retval_buf only used in current region.
            m_rg->addToVarTab(retval_buf);
            VAR_formal_param_pos(retval_buf) = g_formal_parameter_start - 1;
        }

        ASSERT0(VAR_formal_param_pos(retval_buf) ==
                g_formal_parameter_start - 1);

        //The var will be the first parameter of current function,
        //it is hidden and can not see by programmer.
        IR * first_arg = m_rg->buildLda(retval_buf);
        xcom::add_next(&callarglist, first_arg);
    }
    //----------

    //----------
    //Convert the real parameter-expression.
    xcom::add_next(&callarglist, convert(TREE_para_list(t), cont));
    IR * call = nullptr;
    if (is_direct) {
        ASSERT0(callee->is_id());
        Var * v = ID_info(callee);
        call = m_rg->buildCall(v, callarglist, 0, m_tm->getAny());
        if (is_readonly(v)) {
            CALL_is_readonly(call) = true;
        }
        if (is_alloc_heap(v)) {
            CALL_is_alloc_heap(call) = true;
        }
    } else {
        call = m_rg->buildICall(callee, callarglist, 0, m_tm->getAny());
    }
    call->verify(m_rg);
    setLineNum(call, lineno, m_rg);
    xcom::add_next(CONT_toplirlist(cont), call);
    //----------

    //Generate return-exprssion.
    IR * ret_exp = nullptr;
    if (return_val_size > NUM_OF_RETURN_VAL_REGISTERS *
                          GENERAL_REGISTER_SIZE) {
        ASSERT0(retval_buf);
        ret_exp = m_rg->buildLoad(retval_buf);
    } else {
        IR * respr = m_rg->buildPR(rettype);
        setLineNum(respr, lineno, m_rg);
        CALL_prno(call) = PR_no(respr);
        IR_dt(call) = rettype;
        ret_exp = respr;
    }
    setLineNum(ret_exp, lineno, m_rg);
    return ret_exp;
}


IR * CTree2IR::convertPostIncDec(IN Tree * t, INT lineno, IN T2IRCtx * cont)
{
    //CASE: int * a;
    //    return a++;
    //=>
    //    inc_exp = a
    //    a = inc_exp + 1
    //    return inc_exp
    IR_TYPE irt;
    if (TREE_type(t) == TR_POST_INC) {
        irt = IR_ADD;
    } else {
        irt = IR_SUB;
    }

    //Get result IR, it must be pseduo register.
    T2IRCtx ct(*cont);
    CONT_toplirlist(&ct) = CONT_toplirlist(cont);
    CONT_epilogirlist(&ct) = CONT_epilogirlist(cont);

    //Post-increment-op has sideeffect, it has both WRITE and READ property.
    //Begin processing READ-property.
    CONT_is_lvalue(&ct) = false;

    //inc_exp is the base of post-dec/inc.
    IR * inc_exp = convert(TREE_inc_exp(t), &ct);
    bool is_ptr = inc_exp->is_ptr();
    Type const* type;
    if (is_ptr) {
        type = m_tm->getSimplexTypeEx(m_tm->getPointerSizeDtype());
    } else {
        type = inc_exp->getType();
    }

    IR * imm1 = nullptr;
    if (type->is_int()) {
        imm1 = m_rg->buildImmInt(1, type);
    } else {
        ASSERT0(type->is_fp());
        imm1 = m_rg->buildImmFp(1, type);
    }

    if (is_ptr) {
        type = inc_exp->getType();
    } else {
        type = m_tm->hoistDtypeForBinop(inc_exp, imm1);
    }

    IR * addsub = m_rg->buildBinaryOp(irt, type, inc_exp, imm1);
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
        xstpr = m_rg->buildStorePR(inc_exp->getType(),
            m_rg->dupIRTree(inc_exp));
        xincst = m_rg->buildIStore(m_rg->dupIRTree(ILD_base(inc_exp)),
            addsub, inc_exp->getType());
    } else if (inc_exp->is_array()) {
        //If inc_exp is ARRAY, generate IST(ARRAY) = ARRAY + 1.
        //Generate:
        //    pr = array(x)
        //    array(x) = array(x) + 1
        //    return pr
        xstpr = m_rg->buildStorePR(inc_exp->getType(),
            m_rg->dupIRTree(inc_exp));
        xincst = m_rg->buildStoreArray(m_rg->dupIRTree(ARR_base(inc_exp)),
                                       m_rg->dupIRTree(ARR_sub_list(inc_exp)),
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
                TREE_type(t) == TR_POST_INC ? "++" : "--");
            return nullptr;
        }

        xstpr = m_rg->buildStorePR(inc_exp->getType(),
            m_rg->buildLoad(LD_idinfo(inc_exp)));
        xincst = m_rg->buildStore(LD_idinfo(inc_exp), addsub);
    }

    setLineNum(xstpr, lineno, m_rg);
    setLineNum(xincst, lineno, m_rg);
    xcom::add_next(CONT_toplirlist(cont), xstpr);

    if (CONT_is_record_epilog(cont)) {
        xcom::add_next(CONT_epilogirlist(cont), xincst);
    } else {
        xcom::add_next(CONT_toplirlist(cont), xincst);
    }

    return m_rg->buildPRdedicated(STPR_no(xstpr), xstpr->getType());
}


IR * CTree2IR::convertSwitch(IN Tree * t, INT lineno, IN T2IRCtx *)
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
            DATA_TYPE dt = m_tm->getDType(WORD_LENGTH_OF_TARGET_MACHINE, true);
            IR * imm = m_rg->buildImmInt(CASEV_constv(casev),
                                         m_tm->getSimplexTypeEx(dt));
            xcom::add_next(&casev_list, &last,
                           m_rg->buildCase(imm, CASEV_lab(casev)));
        }
    }

    m_case_list = m_case_list_stack.pop();

    IR * ir = m_rg->buildSwitch(vexp, casev_list, body, deflab);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


//Convert direct memory access.
//Return the value of field, or the address of field.
IR * CTree2IR::convertDirectMemAccess(Tree const* t, INT lineno, T2IRCtx * cont)
{
    Decl const* base_decl = TREE_result_type(TREE_base_region(t));
    ASSERTN(!is_pointer(base_decl), ("'.' node must not be pointer type"));

    TypeSpec const* ty = DECL_spec(base_decl);
    ASSERTN(is_struct(ty) || is_union(ty), ("illegal base type"));
    ASSERTN(TREE_type(TREE_field(t)) == TR_ID, ("illegal offset type"));

    Decl const* field_decl = TREE_result_type(TREE_field(t));
    T2IRCtx tc(*cont);

    //Indicates whether if current convertion should return address.
    bool is_require_addr = CONT_is_compute_addr(cont);

    if (is_field_ref_whole_array(t)) {
        //In C spec, if user writes code like 'a = s.array;', actually user
        //expects taking address of array, rather than value.
        CONT_is_compute_addr(&tc) = true;
        is_require_addr = true;
    }

    IR * ir = convert(TREE_base_region(t), &tc);

    //Compute 'byte ofst' of 'ir' accroding to field in structure type.
    UINT field_ofst = 0; //All field of union start at offset 0.
    ASSERTN(TREE_type(TREE_field(t)) == TR_ID, ("illegal struct/union exp"));
    if (is_struct(ty)) {
        Struct const* st = TYPE_struct_type(ty);
        field_ofst = get_aggr_field(st, TREE_id(TREE_field(t))->getStr(),
                                    nullptr);
    }

    UINT field_size = 0;
    DATA_TYPE dt = ::get_decl_dtype(field_decl, &field_size, m_tm);
    if (is_require_addr) {
        //Parent tree node requires address.
        //The returned ir should be pointer.
        ASSERT0(ir->is_ptr());
        ir->setPointerType(field_size, m_tm);
        if (ir->hasOffset()) {
            ir->setOffset(ir->getOffset() + field_ofst);
        } else if (field_ofst != 0) {
            //ir is ADD, SUB, etc, and there is no offset field in ir.
            //Plus the offset of field in Aggr.
            ir = m_rg->buildBinaryOpSimp(IR_ADD, ir->getType(), ir,
                m_rg->buildImmInt(field_ofst,
                                  m_tm->getSimplexTypeEx(
                                      m_tm->getPointerSizeDtype())));
        }
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //Parent tree node requires value.
    ASSERT0(!ir->is_lda());

    //Revise result type of 'ir' accroding to 'field_decl'.
    if (dt == D_PTR) {
        ir->setPointerType(get_pointer_base_size(field_decl), m_tm);
    } else if (dt == D_MC) {
        //DMEM is accessing a memory block. It may be lead a memopy-copy or
        //vector-operation.
        IR_dt(ir) = m_tm->getMCType(field_size);
    } else {
        ASSERT0(IS_SIMPLEX(dt));
        IR_dt(ir) = m_tm->getSimplexTypeEx(dt);
    }
    ir->setOffset(ir->getOffset() + field_ofst);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


//Convert indirect memory access.
//Return the value of field, or the address of field.
IR * CTree2IR::convertIndirectMemAccess(Tree const* t, INT lineno,
                                        T2IRCtx * cont)
{
    Decl const* base_decl = TREE_result_type(TREE_base_region(t));
    ASSERTN(is_pointer(base_decl), ("'->' node must be pointer type"));

    TypeSpec const* ty = DECL_spec(base_decl);
    ASSERTN(is_aggr(ty), ("illegal base type"));
    ASSERTN(TREE_type(TREE_field(t)) == TR_ID, ("illegal offset type"));

    //Compute 'field_offst' according to 'field' of struct.
    UINT field_ofst = 0; //All field of union start at offset 0.
    if (is_struct(ty)) {
        Struct const* st = TYPE_struct_type(ty);
        field_ofst = get_aggr_field(st, TREE_id(TREE_field(t))->getStr(),
                                    nullptr);
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
    DATA_TYPE dt = ::get_decl_dtype(field_decl, &field_size, m_tm);
    if (CONT_is_compute_addr(cont) || is_array(field_decl)) {
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
            ir = m_rg->buildBinaryOpSimp(IR_ADD, base->getType(),
                base, m_rg->buildImmInt(field_ofst,
                    m_tm->getSimplexTypeEx(
                        m_tm->getPointerSizeDtype())));
        } else {
            ir = base;
        }
        ir->setPointerType(field_size, m_tm);
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    //Compute the value of field.
    if (dt == D_MC) {
        //Field is memory block, means user wants to the value of memory block.
        //Return the value of field.
        IR * ir = m_rg->buildILoad(base, m_tm->getMCType(field_size));
        if (field_ofst != 0) {
            ir->setOffset(field_ofst);
        }
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    Type const* type = nullptr;
    if (dt == D_PTR) {
        //e.g: struct { ... char * b ... } * p; then p->b will generate:
        type = m_tm->getPointerType(get_pointer_base_size(field_decl));
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
    IR * ir = m_rg->buildILoad(base, type);
    if (field_ofst != 0) {
        ir->setOffset(field_ofst);
    }

    ASSERT0(ir);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertDeref(IN Tree * t, INT lineno, IN T2IRCtx * cont)
{
    if (TREE_parent(t) == nullptr) { return nullptr; }

    Tree * base = TREE_lchild(t);
    IR * deref_addr = convert(base, cont);
    ASSERT0(deref_addr && deref_addr->is_ptr());

    IR * ir = nullptr;
    if (is_pointer_point_to_array(TREE_result_type(base))) {
        //Here we are computing the base address of ARRAY accessing.
        //In C spec, base of array should be address expression,
        //e.g:
        //  1. a[i] indicates a is symbol, the base address is LDA(a),
        //  2. (*p)[i] indicates p is variable, the base address is LD(p), it
        //     different to a[i].
        //For now, it is the second scenario. Because the convertion of DEREF's
        //lchild is 'deref_addr' who has already convert 'p' to LD(p), here we
        //just return the 'deref_addr' as result of DEREF(ID(p)) at all.
        ir = deref_addr;
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    Type const* type = nullptr;
    if (is_pointer(TREE_result_type(t))) {
        type = m_tm->getPointerType(get_pointer_base_size(
            TREE_result_type(t)));
    } else {
        UINT s;
        DATA_TYPE dt = get_decl_dtype(TREE_result_type(t), &s, m_tm);
        if (dt == D_MC) {
            type = m_tm->getMCType(s);
        } else {
            ASSERT0(IS_SIMPLEX(dt));
            type = m_tm->getSimplexTypeEx(dt);
        }
    }
    ir = m_rg->buildILoad(deref_addr, type);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertSelect(Tree * t, INT lineno, T2IRCtx * cont)
{
    //TREE_result_type(t) is nullptr, may be it is necessary.
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

    Type const* d0 = texp->getType();
    Type const* d1 = fexp->getType();
    ASSERT0(d0 && d1);
    Type const* type = nullptr;
    if (d0->is_pointer() || d1->is_pointer()) {
        //Pointer should have same ptr-base-size.
        if (texp->is_const() && CONST_int_val(texp) == 0) {
            type = fexp->getType();
        } else if (fexp->is_const() && CONST_int_val(fexp) == 0) {
            type = texp->getType();
        } else {
            type = d0;
            ASSERT0(d0 == d1);
        }
    } else if (d0->is_mc() || d1->is_mc()) {
        //Should be same mc type.
        ASSERT0(d0 == d1);
        type = texp->getType();
    } else {
        type = m_tm->hoistDtypeForBinop(texp, fexp);
        if (texp->getType() != type) {
            IR * cvt = m_rg->buildCvt(texp, type);
            texp = cvt;
        }
        if (fexp->getType() != type) {
            IR * cvt = m_rg->buildCvt(fexp, type);
            fexp = cvt;
        }
    }

    if (!det->is_bool()) {
        IR * old = det;
        det = m_rg->buildJudge(det);
        copyDbx(det, old, m_rg);
    }
    IR * ir = m_rg->buildSelect(det, texp, fexp, type);
    setLineNum(ir, lineno, m_rg);
    return ir;
}


//Generate a Var if the bytesize of RETURN is bigger than total size of
//return-registers.
//e.g: Given 32bit target machine, the return register is a0, a1,
//If the return type is structure whose size is bigger than 64bit, we need
//to generate an implcitly Var to indicate the stack buffer which used
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
    if (return_val_bytesize <= NUM_OF_RETURN_VAL_REGISTERS *
                               GENERAL_REGISTER_SIZE) {
        return ir;
    }
    if (m_retval_buf == nullptr) {
        CHAR const* rname = m_rg->getRegionName();
        ASSERT0(rname);
        xcom::StrBuf tmp((UINT)(::strlen(rname) + 32));
        tmp.sprint("#retval_buf_of_%s", rname);

        Type const* ptr = m_rg->getTypeMgr()->getPointerType(
            retval->getTypeSize(m_rg->getTypeMgr()));
        m_retval_buf = m_rg->getVarMgr()->registerVar(
            tmp.buf, ptr, STACK_ALIGNMENT,
            VAR_LOCAL | VAR_IS_FORMAL_PARAM);

        //retval_buf only used in current region.
        m_rg->addToVarTab(m_retval_buf);
    }

    IR * ist = m_rg->buildIStore(
        m_rg->buildLoad(m_retval_buf), retval, retval->getType());
    RET_exp(ir) = nullptr;

    xcom::add_next(&ist, ir);
    return ist;
}


//Generate CVT if type conversion from src to tgt is indispensable.
xoc::Type const* CTree2IR::checkAndGenCVTType(Decl const* tgt, Decl const* src)
{
    ASSERT0(tgt && src);
    UINT tgt_dt_sz, src_dt_sz;
    DATA_TYPE tgt_dt = get_decl_dtype(tgt, &tgt_dt_sz, m_tm);
    DATA_TYPE src_dt = get_decl_dtype(src, &src_dt_sz, m_tm);

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

    Type const* type = nullptr;
    if (IS_SIMPLEX(tgt_dt)) {
        type = m_tm->getSimplexTypeEx(tgt_dt);
    } else if (IS_PTR(tgt_dt)) {
        type = m_tm->getPointerType(get_pointer_base_size(tgt));
    } else {
        UNREACHABLE();
    }

    return type;
}


IR * CTree2IR::convertReturn(Tree * t, INT lineno, T2IRCtx * cont)
{
    IR * ir = m_rg->buildReturn(convert(TREE_ret_exp(t), cont));
    if (RET_exp(ir) != nullptr) {
        ASSERTN(IR_next(RET_exp(ir)) == nullptr, ("unsupport"));
        ir->setParentPointer(false);
        xoc::Type const* cvttype = checkAndGenCVTType(
            m_return_type, TREE_result_type(TREE_ret_exp(t)));
        if (cvttype != nullptr) {
            IR * cvt = m_rg->buildCvt(RET_exp(ir), cvttype);
            RET_exp(ir) = cvt;
            ir->setParent(cvt);
        }
    }
    setLineNum(ir, lineno, m_rg);
    ir = genReturnValBuf(ir);
    return ir;
}


IR * CTree2IR::convertLDA(Tree * t, INT lineno, T2IRCtx * cont)
{
    IR * ir = nullptr;
    Tree * kid = TREE_lchild(t);

    //Actually, convertor does not have to do anything. The processing of kid
    //should consider the situation that its parent is LDA.
    T2IRCtx tc(*cont);
    IR * base = nullptr;
    if (TREE_type(kid) == TR_DEREF) {
        //LDA operator will counteract the effect of DEREF, just leave the
        //type of DEREF as the pointer-base-type of LDA.
        //e.g: &*(int*)p;  where p is char*.
        //  The result type of LDA is int*.
        CONT_is_compute_addr(&tc) = false;
        base = convert(TREE_lchild(kid), &tc);
        base->setPointerType(get_decl_size(TREE_result_type(kid)), m_tm);
        setLineNum(base, lineno, m_rg);
        return base;
    }

    CONT_is_compute_addr(&tc) = true;
    base = convert(kid, &tc);

    if (base->is_ld()) {
        setLineNum(base, lineno, m_rg);
        return base;
    }

    if (base->is_array()) {
        setLineNum(base, lineno, m_rg);
        return base;
    }

    if (base->is_ild()) {
        setLineNum(base, lineno, m_rg);
        return base;
    }

    if (base->is_lda()) {
        setLineNum(base, lineno, m_rg);
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
        setLineNum(base, lineno, m_rg);
        return base;
    }

    UNREACHABLE();
    return nullptr;
}


IR * CTree2IR::convertCVT(Tree * t, INT lineno, T2IRCtx * cont)
{
    IR * ir = nullptr;
    Decl * cvtype = TREE_type_name(TREE_cvt_type(t));
    if (is_pointer(cvtype)) {
        //decl is pointer variable
        ir = m_rg->buildCvt(convert(TREE_cast_exp(t), cont),
            m_tm->getPointerType(get_pointer_base_size(cvtype)));
    } else {
        UINT size = 0;
        DATA_TYPE dt = get_decl_dtype(cvtype, &size, m_tm);
        Type const* type = nullptr;
        if (IS_SIMPLEX(dt)) {
            type = m_tm->getSimplexTypeEx(dt);
        } else if (IS_PTR(dt)) {
            type = m_tm->getPointerType(size);
        } else if (IS_MC(dt)) {
            type = m_tm->getMCType(size);
        } else {
            UNREACHABLE();
        }
        ir = m_rg->buildCvt(convert(TREE_cast_exp(t), cont), type);
    }
    setLineNum(ir, lineno, m_rg);
    return ir;
}


IR * CTree2IR::convertId(Tree * t, INT lineno, T2IRCtx * cont)
{
    IR * ir = nullptr;
    Decl * res_ty = TREE_result_type(t);

    if (is_array(res_ty)) {
        //In C spec, identifier of array is just a label.
        //The reference of the label should be represented as LDA.
        //e.g: int * p; int a[10]; p = a;
        //  will generate: ST(p) = LDA(a)
        if (TREE_type(TREE_parent(t)) == TR_ARRAY) {
            ASSERT0(t == TREE_array_base(TREE_parent(t)));
        }

        ir = buildLda(t);
        ASSERT0(ir->is_lda());
        ASSERT0(VAR_is_array(LDA_idinfo(ir)));
        ASSERTN(!VAR_is_formal_param(LDA_idinfo(ir)),
                ("array parameter should be transformed "
                 "to pointer type at FrontEnd's compound_stmt()"));
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    if (is_fun_decl(res_ty)) {
        //If current Tree is fun-decl, there will be two case:
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
            ASSERT0(VAR_is_func_decl(LDA_idinfo(ir)));
        }
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    if (CONT_is_compute_addr(cont)) {
        //e.g: int v[100]; ..=&v
        //In this case, parent tree node should be TR_LDA operator,
        ir = buildLda(t);
        ASSERT0(ir->is_lda());
        //ir = buildId(t);
        setLineNum(ir, lineno, m_rg);
        return ir;
    }

    if (is_bitfield(res_ty)) {
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
    setLineNum(ir, lineno, m_rg);
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


//Convert TREE AST to IR.
IR * CTree2IR::convert(IN Tree * t, IN T2IRCtx * cont)
{
    IR * ir = nullptr;
    IR * ir_list = nullptr; //record ir list generated.
    IR * l = nullptr, * r = nullptr;
    T2IRCtx ct;
    if (cont == nullptr) {
        cont = &ct;
        CONT_toplirlist(cont) = &ir_list;
    }

    while (t != nullptr) {
        INT lineno = TREE_lineno(t);
        switch (TREE_type(t)) {
        case TR_ASSIGN:
            ir = convertAssign(t, lineno, cont);
            break;
        case TR_ID:
            ir = convertId(t, lineno, cont);
            break;
        case TR_IMM:
        case TR_IMMU: {
            UINT s = 0;
            DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t), &s, m_tm);
            //The maximum integer supported is 64bit.
            ir = m_rg->buildImmInt(TREE_imm_val(t), m_tm->getSimplexTypeEx(dt));
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_IMML:
        case TR_IMMUL: {
            UINT s = 0;
            DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t), &s, m_tm);
            ASSERT0(dt == D_I64 || dt == D_U64);
            //The maximum integer supported is 64bit.
            ir = m_rg->buildImmInt(TREE_imm_val(t),
                m_tm->getSimplexTypeEx(dt));
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_FP:
        case TR_FPF:
        case TR_FPLD: {
            //An unsuffixed floating constant has type double. If suffixed
            //by the letter f or F, it has type float.
            //If suffixed by the letter l or L, it has type long double.

            //Convert string to hex value , that is in order to generate
            //single load instruction to load float point value during
            //Code Generator.
            Sym const* fp = TREE_fp_str_val(t);

            //Default float point type is 64bit.
            ir = m_rg->buildImmFp(::atof(SYM_name(fp)),
                m_tm->getSimplexTypeEx(
                    TREE_type(t) == TR_FPF ? D_F32 : D_F64));
            BYTE mantissa_num = getMantissaNum(SYM_name(fp));
            if (mantissa_num > DEFAULT_MANTISSA_NUM) {
                CONST_fp_mant(ir) = mantissa_num;
            }
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_ENUM_CONST: {
            INT v = get_enum_const_val(TREE_enum(t),TREE_enum_val_idx(t));
            //Default const type of enumerator is 'unsigned int'
            //of target machine
            ir = m_rg->buildImmInt(v, m_tm->getSimplexTypeEx(
                m_tm->getDType(WORD_LENGTH_OF_TARGET_MACHINE, true)));
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_STRING:
            ir = m_rg->buildLdaString(nullptr, TREE_string_val(t));
            ASSERT0(ir->is_lda() &&
                    LDA_idinfo(ir)->is_global() &&
                    m_rg->getTopRegion() != nullptr &&
                    m_rg->getTopRegion()->is_program());
            m_rg->getTopRegion()->addToVarTab(LDA_idinfo(ir));
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_LOGIC_OR: //logical or        ||
            l = convert(TREE_lchild(t),cont);
            r = convert(TREE_rchild(t),cont);
            ir = m_rg->buildCmp(IR_LOR, l, r);
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_LOGIC_AND: //logical and      &&
            l = convert(TREE_lchild(t),cont);
            r = convert(TREE_rchild(t),cont);
            ir = m_rg->buildCmp(IR_LAND, l, r);
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_INCLUSIVE_OR: { //inclusive or  |
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt = ::get_decl_dtype(
                    TREE_result_type(t), &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            ir = m_rg->buildBinaryOp(IR_BOR, type, l, r);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_INCLUSIVE_AND: { //inclusive and &
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt = ::get_decl_dtype(
                    TREE_result_type(t), &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            ir = m_rg->buildBinaryOp(IR_BAND, type, l, r);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_XOR: { //exclusive or
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt = ::get_decl_dtype(
                    TREE_result_type(t), &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            ir = m_rg->buildBinaryOp(IR_XOR, type, l, r);
            break;
        }
        case TR_EQUALITY: // == !=
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            if (TREE_token(t) == T_EQU) {
                ir = m_rg->buildCmp(IR_EQ, l, r);
            } else {
                ir = m_rg->buildCmp(IR_NE, l, r);
            }
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_RELATION: // < > >= <=
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            switch (TREE_token(t)) {
            case T_LESSTHAN:     // <
                ir = m_rg->buildCmp(IR_LT, l, r);
                break;
            case T_MORETHAN:     // >
                ir = m_rg->buildCmp(IR_GT, l, r);
                break;
            case T_NOLESSTHAN:   // >=
                ir = m_rg->buildCmp(IR_GE, l, r);
                break;
            case T_NOMORETHAN:   // <=
                ir = m_rg->buildCmp(IR_LE, l, r);
                break;
            default: UNREACHABLE();
            }
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_SHIFT: {  // >> <<
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt =
                    ::get_decl_dtype(TREE_result_type(t), &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            if (TREE_token(t) == T_RSHIFT) {
                ir = m_rg->buildBinaryOp(l->is_signed() ?
                    IR_ASR : IR_LSR, type, l, r);
            } else {
                ir = m_rg->buildBinaryOp(IR_LSL, type, l, r);
            }
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_ADDITIVE: { // '+' '-'
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt = ::get_decl_dtype(
                    TREE_result_type(t), &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            if (TREE_token(t) == T_ADD) {
                IR_dt(l) = type;
                ir = m_rg->buildBinaryOp(IR_ADD, type, l, r);
            } else {
                ir = m_rg->buildBinaryOp(IR_SUB, type, l, r);
            }
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_MULTI: {   // '*' '/' '%'
            l = convert(TREE_lchild(t), cont);
            r = convert(TREE_rchild(t), cont);
            Type const* type = nullptr;
            if (is_pointer(TREE_result_type(t))) {
                type = m_tm->getPointerType(
                    get_pointer_base_size(TREE_result_type(t)));
            } else {
                UINT s = 0;
                DATA_TYPE dt = ::get_decl_dtype(TREE_result_type(t),
                    &s, m_tm);
                if (dt == D_MC) {
                    type = m_tm->getMCType(s);
                } else {
                    type = m_tm->getSimplexTypeEx(dt);
                }
            }
            if (TREE_token(t) == T_ASTERISK) {
                ir = m_rg->buildBinaryOp(IR_MUL, type, l, r);
            } else if (TREE_token(t) == T_DIV) {
                ir = m_rg->buildBinaryOp(IR_DIV, type, l, r);
            } else {
                ir = m_rg->buildBinaryOp(IR_REM, type, l, r);
            }
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_SCOPE:
            ASSERT0(TREE_scope(t));
            ir = convert(SCOPE_stmt_list(TREE_scope(t)), nullptr);
            break;
        case TR_IF: {
            IR * det = convert(TREE_if_det(t), cont);
            det = only_left_last(det);
            if (det == nullptr) {
                det = m_rg->buildJudge(m_rg->buildImmInt(1, m_tm->getI32()));
            }

            IR * truebody = convert(TREE_if_true_stmt(t), nullptr);
            IR * falsebody = convert(TREE_if_false_stmt(t), nullptr);
            if (!det->is_judge()) {
                IR * old = det;
                det = m_rg->buildJudge(det);
                copyDbx(det, old, m_rg);
            }
            ir = m_rg->buildIf(det, truebody, falsebody);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_DO: {
            IR * prolog = nullptr;
            IR * epilog = nullptr;
            T2IRCtx ct2;
            CONT_toplirlist(&ct2) = &prolog;
            CONT_epilogirlist(&ct2) = &epilog;
            CONT_is_record_epilog(&ct2) = true;

            IR * det = convert(TREE_dowhile_det(t), &ct2);
            det = only_left_last(det);
            if (det == nullptr) {
                det = m_rg->buildJudge(m_rg->buildImmInt(1,
                    m_tm->getI32()));
            }

            if (prolog != nullptr) {
                //Do NOT add prolog before DO_WHILE stmt.
                //dup_prolog = m_rg->dupIRTreeList(prolog);
                //xcom::add_next(&ir_list, prolog);
            }

            IR * body = convert(TREE_dowhile_body(t), nullptr);
            if (prolog != nullptr) {
                //Put prolog at end of body.
                xcom::add_next(&body, prolog);
            }
            if (epilog != nullptr) {
                xcom::add_next(&body, epilog);
            }

            if (!det->is_judge()) {
                IR * old = det;
                det = m_rg->buildJudge(det);
                copyDbx(det, old, m_rg);
            }
            ir = m_rg->buildDoWhile(det, body);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_WHILE: {
            IR * prolog = nullptr;
            IR * epilog = nullptr;
            T2IRCtx ct2;
            CONT_toplirlist(&ct2) = &prolog;
            CONT_epilogirlist(&ct2) = &epilog;
            CONT_is_record_epilog(&ct2) = false;

            IR * det = convert(TREE_whiledo_det(t), &ct2);
            det = only_left_last(det);
            if (det == nullptr) {
                det = m_rg->buildJudge(m_rg->buildImmInt(1,
                    m_tm->getI32()));
            }

            IR * dup_prolog = nullptr;
            if (prolog != nullptr) {
                dup_prolog = m_rg->dupIRTreeList(prolog);
                xcom::add_next(&ir_list, prolog);
            }
            ASSERT0(epilog == nullptr);

            IR * body = convert(TREE_whiledo_body(t), nullptr);
            if (dup_prolog != nullptr) {
                xcom::add_next(&body, dup_prolog);
            }

            if (!det->is_judge()) {
                IR * old = det;
                det = m_rg->buildJudge(det);
                copyDbx(det, old, m_rg);
            }

            ir = m_rg->buildWhileDo(det, body);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_FOR: {
            IR * last = nullptr;
            IR * init = convert(TREE_for_init(t), nullptr);
            xcom::add_next(&ir, &last, init);

            T2IRCtx ct2;
            ASSERT0(cont);
            IR * stmt_in_det = nullptr;
            CONT_toplirlist(&ct2) = &stmt_in_det;
            IR * det = convert(TREE_for_det(t), &ct2);
            if (stmt_in_det != nullptr) {
                xcom::add_next(&ir, &last, stmt_in_det);
            }

            det = only_left_last(det);
            if (det == nullptr) {
                det = m_rg->buildJudge(m_rg->buildImmInt(1, m_tm->getI32()));
            }

            IR * body = convert(TREE_for_body(t), nullptr);
            IR * step = convert(TREE_for_step(t), nullptr);
            xcom::add_next(&body, step);
            if (stmt_in_det != nullptr) {
                xcom::add_next(&body, m_rg->dupIRTreeList(stmt_in_det));
            }

            if (!det->is_judge()) {
                IR * old = det;
                det = m_rg->buildJudge(det);
                copyDbx(det, old, m_rg);
            }

            IR * whiledo = m_rg->buildWhileDo(det, body);
            setLineNum(whiledo, lineno, m_rg);
            xcom::add_next(&ir, &last, whiledo);
            break;
        }
        case TR_SWITCH:
            ir = convertSwitch(t, lineno, cont);
            break;
        case TR_BREAK:
            ir = m_rg->buildBreak();
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_CONTINUE:
            ir = m_rg->buildContinue();
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_RETURN:
            ir = convertReturn(t, lineno, cont);
            break;
        case TR_GOTO:
            ir = m_rg->buildGoto(getUniqueLabel(
                const_cast<LabelInfo*>(TREE_lab_info(t))));
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_LABEL:
            ir = m_rg->buildLabel(getUniqueLabel(
                const_cast<LabelInfo*>(TREE_lab_info(t))));
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_CASE: {
            ir = m_rg->buildIlabel();
            CaseValue * casev = (CaseValue*)xmalloc(sizeof(CaseValue));
            CASEV_lab(casev) = LAB_lab(ir);
            CASEV_constv(casev) = TREE_case_value(t);
            CASEV_is_default(casev) = false;
            m_case_list->append_tail(casev);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_DEFAULT: {
            ir = m_rg->buildIlabel();
            CaseValue * casev = (CaseValue*)xmalloc(sizeof(CaseValue));
            CASEV_lab(casev) = LAB_lab(ir);
            CASEV_constv(casev) = 0;
            CASEV_is_default(casev) = true;
            m_case_list->append_head(casev);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_COND: //formulized log_OR_exp ? exp : cond_exp
            ir = convertSelect(t, lineno, cont);
            break;
        case TR_CVT: //type convertion
            ir = convertCVT(t, lineno, cont);
            break;
        case TR_LDA:   // &a, get address of 'a'
            ir = convertLDA(t, lineno, cont);
            break;
        case TR_DEREF: //*p dereferencing the pointer 'p'
            ir = convertDeref(t, lineno, cont);
            break;
        case TR_PLUS:  // +123
            ir = convert(TREE_lchild(t), cont);
            break;
        case TR_MINUS: { // -123
            IR * opnd = convert(TREE_lchild(t), cont);
            ir = m_rg->buildUnaryOp(IR_NEG, opnd->getType(), opnd);
            setLineNum(ir, lineno, m_rg);
            ir->setParentPointer(false);
            break;
        }
        case TR_REV: { // Reverse
            IR * opnd = convert(TREE_lchild(t), cont);
            ir = m_rg->buildUnaryOp(IR_BNOT, opnd->getType(), opnd);
            setLineNum(ir, lineno, m_rg);
            break;
        }
        case TR_NOT:  // get non-value
            ir = m_rg->buildLogicalNot(convert(TREE_lchild(t), cont));
            setLineNum(ir, lineno, m_rg);
            break;
        case TR_INC:   //++a
        case TR_DEC:   //--a
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
        default: ASSERTN(0, ("unknown tree type:%d", TREE_type(t)));
        } //end switch

        t = TREE_nsib(t);
        if (ir != nullptr) {
            if (getLineNum(ir) == 0) {
                setLineNum(ir, lineno, m_rg);
            }
            xcom::add_next(&ir_list, ir);
        }

        ir = nullptr;
    }
    return ir_list;
}


//Count up the number of local-variables.
static void scanScopeDeclList(Scope * s, OUT xoc::Region * rg, bool scan_sib)
{
    if (s == nullptr) { return; }
    Decl * decl = SCOPE_decl_list(s);
    while (decl != nullptr) {
        if (DECL_is_formal_para(decl) && get_decl_sym(decl) == nullptr) {
            //void function(void*), parameter does not have name.
            decl = DECL_next(decl);
            continue;
        }
        xoc::Var * v = mapDecl2VAR(decl);
        ASSERTN(v, ("nullptr variable correspond to"));
        if (VAR_is_global(v)) {
            xoc::Region * topru = rg->getTopRegion();
            ASSERT0(topru);
            if (topru->is_program()) {
                topru->addToVarTab(v);
            }
        } else {
            rg->addToVarTab(v);
        }
        decl = DECL_next(decl);
    }
    scanScopeDeclList(SCOPE_sub(s), rg, true);
    if (scan_sib) {
        scanScopeDeclList(SCOPE_nsibling(s), rg, true);
    }
}


//Ensure IR_RETURN at the end of function if its return-type is VOID.
static IR * addReturn(IR * irs, Region * rg)
{
    IR * last = xcom::get_last(irs);
    if (last == nullptr) {
        return rg->buildReturn(nullptr);
    }

    if (!last->is_return()) {
        IR * ret = rg->buildReturn(nullptr);
        if (irs == nullptr) {
            irs = ret;
        } else {
            xcom::insertafter_one(&last, ret);
        }
    }

    return irs;
}


//If 'decl' presents a simply type, convert type-spec to DATA_TYPE descriptor.
//If 'decl' presents a pointer type, convert pointer-type to D_PTR.
//If 'decl' presents an array, convert type to D_M descriptor.
//size: return byte size of decl.
xoc::DATA_TYPE get_decl_dtype(Decl const* decl, UINT * size, xoc::TypeMgr * tm)
{
    ASSERT0(decl && tm);
    xoc::DATA_TYPE dtype = xoc::D_UNDEF;
    *size = 0;
    ASSERTN(DECL_dt(decl) == DCL_DECLARATION ||
            DECL_dt(decl) == DCL_TYPE_NAME, ("TODO"));

    TypeSpec * ty = DECL_spec(decl);
    bool is_signed;

    if (is_pointer(decl)) {
        *size = BYTE_PER_POINTER;
        return xoc::D_PTR;
    }

    if (is_array(decl)) {
        dtype = xoc::D_MC;
        *size = get_decl_size(decl);
        return dtype;
    }

    if (IS_TYPE(ty, T_SPEC_UNSIGNED)) {
        is_signed = false;
    } else {
        is_signed = true;
    }

    ASSERTN(ty, ("Type-SPEC in DCRLARATION cannot be nullptr"));
    *size = getSpecTypeSize(ty);
    if (IS_TYPE(ty, T_SPEC_VOID) ||
        IS_TYPE(ty, T_SPEC_CHAR) ||
        IS_TYPE(ty, T_SPEC_BOOL) ||
        IS_TYPE(ty, T_SPEC_SHORT) ||
        IS_TYPE(ty, T_SPEC_INT) ||
        IS_TYPE(ty, T_SPEC_LONGLONG) ||
        IS_TYPE(ty, T_SPEC_LONG) ||
        IS_TYPE(ty, T_SPEC_ENUM) ||
        IS_TYPE(ty, T_SPEC_SIGNED) ||
        IS_TYPE(ty, T_SPEC_UNSIGNED)) {
        dtype = tm->get_int_dtype(*size * BIT_PER_BYTE, is_signed);
    } else if (IS_TYPE(ty, T_SPEC_FLOAT) || IS_TYPE(ty, T_SPEC_DOUBLE)) {
        dtype = tm->get_fp_dtype(*size * BIT_PER_BYTE);
    } else if (IS_STRUCT(ty) || IS_UNION(ty)) {
        dtype = xoc::D_MC;
        ASSERT0(*size == get_decl_size(decl));
    } else if (IS_TYPE(ty, T_SPEC_USER_TYPE)) {
        ty = get_pure_type_spec(ty);

        //USER Type should NOT be here.
        ASSERTN(0, ("You should factorize the type specification "
                    "into pure type during declaration()"));

        //dtype = get_decl_dtype(USER_TYPE_decl(TYPE_user_type(ty)), size);
    } else {
        ASSERTN(0, ("failed in DATA_TYPE converting"));
    }
    return dtype;
}


//Convert C-language AST into XOC IR.
//NOTICE: Before the converting, declaration must wire up a XOC Var.
static INT generateFuncRegion(Decl * dcl, OUT CLRegionMgr * rm)
{
    ASSERT0(DECL_is_fun_def(dcl));

    //In C language, there are two levels kind of region, program region and
    //function region. Program region describes the whole program that begins
    //from 'main()' function, and function region describes the normal function
    //in C-spec.

    //Each region needs a Var.
    xoc::Var * rvar = mapDecl2VAR(dcl);
    ASSERT0(rvar);

    //Start a timer to evaluate compilation speed.
    START_TIMER_FMT(t, ("GenerateFuncRegion '%s'",
                        rvar->get_name()->getStr()));

    //Generates a function region to describe current C-language function.
    xoc::Region * r = rm->newRegion(xoc::REGION_FUNC);
    r->setRegionVar(rvar);
    REGION_is_expect_inline(r) = is_inline(dcl);

    //To faciliate RegionMgr to manage the resource of each Region, you have to
    //add it explicitly.
    rm->addToRegionTab(r);

    //Build the relationship of current region and its parent region.
    ASSERT0(rm->get_program());
    REGION_parent(r) = rm->get_program();

    //Add current function region into VarTab of program region.
    REGION_parent(r)->addToVarTab(r->getRegionVar());

    //Build current function region to a
    //Region IR, and appent to IR list of program region.
    xoc::IR * lst = rm->get_program()->getIRList();
    xcom::add_next(&lst, rm->get_program()->buildRegion(r));
    rm->get_program()->setIRList(lst);

    //Itertive scanning scope to collect and append
    //all local-variable into VarTab of current region.
    scanScopeDeclList(DECL_fun_body(dcl), r, false);

    if (xoc::g_dump_opt.isDumpALL()) {
        dump_scope(DECL_fun_body(dcl), 0xffffFFFF);
    }

    //Convert C-language AST into XOC IR.
    CTree2IR ct2ir(r, dcl);
    xoc::IR * irs = ct2ir.convert(SCOPE_stmt_list(DECL_fun_body(dcl)),
                                  nullptr);
    if (g_err_msg_list.get_elem_count() > 0) {
        return ST_ERR;
    }

    //Convertion is successful.

    if (xoc::g_dump_opt.isDumpALL()) {
        xoc::note(rm, "\n==---- AFTER TREE2IR CONVERT '%s' -----==",
                  get_decl_name(dcl));
        xoc::dumpIRListH(irs, r);
    }

    //Ensure RETURN IR at the end of function
    //if its return-type is VOID.
    irs = addReturn(irs, r);

    //Reshape IR tree to well formed outlook.
    if (xoc::g_dump_opt.isDumpALL()) {
        xoc::note(rm, "\n==---- AFTER RESHAPE IR -----==", get_decl_name(dcl));
        xoc::dumpIRListH(irs, r);
    }

    //In order to sanitize and optimize IRs, we have to canonicalize IR tree
    //into well-formed layout that conform to the guidelines of XOC IR pass
    //preferred.
    Canon ic(r);
    bool change = false;
    irs = ic.handle_stmt_list(irs, change);

    //Refine and perform peephole optimizations.
    xoc::RefineCtx rc;

    //Do not perform following optimizations.
    RC_refine_div_const(rc) = false;
    RC_refine_mul_const(rc) = false;
    RC_update_mdref(rc) = false;

    change = false;
    r->initPassMgr(); //Optimizations in XOC needs PassMgr.
    Refine * rf = (Refine*)r->getPassMgr()->registerPass(PASS_REFINE);
    irs = rf->refineIRlist(irs, change, rc);
    ASSERT0(xoc::verifyIRList(irs, nullptr, r));
    r->setIRList(irs);

    if (xoc::g_dump_opt.isDumpALL()) {
        xoc::note(rm, "\n==---- AFTER REFINE IR -----==", get_decl_name(dcl));
        xoc::dumpIRListH(irs, r);
        //rg->dumpVARInRegion();
    }

    END_TIMER_FMT(t, ("GenerateFuncRegion '%s'",
                      r->getRegionVar()->get_name()->getStr()));
    return ST_SUCC;
}


//Construct Region and convert C-Language-Ast to XOC IR.
bool generateRegion(RegionMgr * rm)
{
    START_TIMER(t, "Tree2IR");
    Scope * s = get_global_scope();

    //Generate Program region.
    Region * program = rm->newRegion(REGION_PROGRAM);
    program->registerGlobalVAR();
    rm->addToRegionTab(program);

    //Record the program region in RegionMgr.
    ((CLRegionMgr*)rm)->set_program(program);
    program->setRegionVar(program->getVarMgr()->registerVar(".program",
                          rm->getTypeMgr()->getMCType(0),
                          1, VAR_GLOBAL|VAR_FAKE));

    //Iterates each declarations that is in global scope.
    //In C-language, global scope amounts to file scope.
    for (Decl * dcl = SCOPE_decl_list(s);
         dcl != nullptr; dcl = DECL_next(dcl)) {
        if (is_fun_decl(dcl)) {
            if (DECL_is_fun_def(dcl)) {
                //'dcl' is function definition.
                if (ST_ERR == generateFuncRegion(dcl, (CLRegionMgr*)rm)) {
                    return ST_ERR;
                }
                if (g_err_msg_list.get_elem_count() > 0) {
                    return false;
                }
                continue;
            }

            //'dcl' is function declaration, not definition.
            Var * v = mapDecl2VAR(dcl);
            ASSERT0(v);
            SET_FLAG(VAR_flag(v), VAR_FUNC_DECL);
            program->addToVarTab(v);
            continue;
        }

        //'dcl' has to be normal variable declaration, or function declaration.
        //rather than function definition.
        program->addToVarTab(mapDecl2VAR(dcl));
    }

    //Free the temprary memory pool.
    END_TIMER(t, "CAst2IR");
    return true;
}
