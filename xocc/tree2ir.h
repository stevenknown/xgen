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

#ifndef __TREE_TO_IR_H__
#define __TREE_TO_IR_H__

#define CASEV_lab(c)               (c)->label
#define CASEV_constv(c)            (c)->constv
#define CASEV_is_def(c)            (c)->is_default
class CaseValue {
public:
    LabelInfo const* label;
    INT constv;
    bool is_default;
};


#define CONT_toplirlist(s)         ((s)->top_level_irlist)
#define CONT_epilogirlist(s)       ((s)->epilog_ir_list)
#define CONT_is_lvalue(s)          ((s)->is_lvalue)
#define CONT_is_parse_callee(s)    ((s)->is_parse_callee)
#define CONT_is_record_epilog(s)   ((s)->is_record_epilog)
#define CONT_is_compute_addr(s)    ((s)->is_compute_addr)
class T2IRCtx {
public:
    //Top level of current ir list.
    //because the top level of ir list must be IR_ST or IR_IST.
    //So store cannot be child node of IR.
    IR ** top_level_irlist;

    //Both of Post Dec/Inc will take side effects for base region
    //So we must append the side effect ST operation followed, and
    //record the side-effect ir as epilog of current statement.
    IR ** epilog_ir_list;

    //source of store node only can be IR_PR, so if we
    //encounter 'b=++a', the recursive ST should be extracted.
    bool is_need_result;

    //Inform the AST convertor that the current Tree is callee.
    //Generate ID for callee rather than LD if it is direct call.
    bool is_parse_callee;

    //for lvalue expression , TR_ID should corresponding
    //with IR_ID, but IR_LD.
    bool is_lvalue;

    //Whether we need to record operations at epilog.
    //e.g a++, post-add is epilog operation.
    bool is_record_epilog;

    //In the case of array address computation, the flag indicate
    //whether generate code to compute address or compute the value.
    //e.g:
    //struct Q {
    //    long long a;
    //    int b[100];
    //};
    //struct Q * q;
    //
    //Accessing pattern: x = q->b[i]
    //The tree convertor generate code to compute address of array
    //when access b£¬
    //    t = ld(q)
    //    t = t + ofst(b)
    //    x = array(t, i)
    //
    //Accessing pattern: x = q->a
    //generate code to load value of a,
    //    t = ld(q)
    //    x = ild(t, ofst(b))
    bool is_compute_addr;

    T2IRCtx()
    {
        ::memset(this, 0, sizeof(T2IRCtx));
    }
};


//Transform from C AST to IR.
class CTree2IR {
protected:
    Decl * m_func_decl;
    Decl const* m_return_type;
    Region * m_rg;
    TypeMgr * m_tm;
    Var * m_retval_buf;
    List<CaseValue*> * m_case_list; //for switch/case used only
    Stack<List<CaseValue*>*> m_case_list_stack; //for switch/case used only
    SMemPool * m_pool;
    LabelTab m_labtab;

protected:
    void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        ASSERTN(m_pool != NULL, ("need pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }

    IR * only_left_last(IR * head);
public:
    CTree2IR(Region * rg, Decl * dcl)
    {
        ASSERT0(rg && dcl);
        m_func_decl = dcl;
        m_return_type = get_return_type(dcl);
        m_rg = rg;
        m_tm = rg->getTypeMgr();
        m_case_list = NULL;
        m_retval_buf = NULL;
        m_pool = smpoolCreate(16, MEM_COMM);
    }
    COPY_CONSTRUCTOR(CTree2IR);
    ~CTree2IR() { smpoolDelete(m_pool); }

    LabelInfo * getUniqueLabel(LabelInfo * lab)
    { return m_labtab.append_and_retrieve(lab); }

    //Generate a Var if the bytesize of RETURN is bigger than total size of
    //return-registers.
    //e.g: Given 32bit target machine, the return register is a0, a1,
    //If the return type is structure whose size is bigger than 64bit, we need
    //to generate an implcitly Var to indicate the stack buffer which used
    //to hold the return value.
    IR * genReturnValBuf(IR * ir);
    BYTE getMantissaNum(CHAR const* fpval);

    IR * buildId(IN Tree * t);
    IR * buildId(IN Decl * id);
    IR * buildLoad(IN Tree * t);

    bool is_istore_lhs(IN Tree * t);
    bool is_readonly(Var const* v);
    bool is_alloc_heap(Var const* v);

    xoc::Type const* checkAndGenCVTType(Decl const* tgt, Decl const* src);
    IR * convertLDA(Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertCVT(Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertId(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertReturn(Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertAssign(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertIncDec(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertPostIncDec(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertCall(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertArray(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertSelect(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertSwitch(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertIndirectMemAccess(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertDirectMemAccess(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertDeref(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertPragma(IN Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convert(IN Tree * t, IN T2IRCtx * cont);
};

DATA_TYPE get_decl_dtype(Decl const* decl, UINT * size, TypeMgr * tm);

//Construct Region and convert C-Language-Ast to XOC IR.
bool generateRegion(RegionMgr * rm);
#endif
