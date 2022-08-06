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

#ifndef __TREE_TO_IR_H__
#define __TREE_TO_IR_H__

namespace xocc {

#define CASEV_lab(c) (c)->label
#define CASEV_constv(c) (c)->constv
#define CASEV_is_default(c) (c)->is_default
class CaseValue {
public:
    LabelInfo const* label;
    INT constv;
    bool is_default;
};


#define CONT_toplirlist(s) ((s)->top_level_irlist)
#define CONT_epilogirlist(s) ((s)->epilog_ir_list)
#define CONT_is_lvalue(s) ((s)->is_lvalue)
#define CONT_is_parse_callee(s) ((s)->is_parse_callee)
#define CONT_is_record_epilog(s) ((s)->is_record_epilog)
#define CONT_is_compute_addr(s) ((s)->is_compute_addr)
class T2IRCtx {
public:
    //Progagate information top down and collect information bottom up.
    //Top level of current ir list.
    //Note the top level of ir list must be stmt.
    IR ** top_level_irlist;

    //Collect information bottom up.
    //Both of Post Dec/Inc will take side effects for base-region,
    //thus we must append the side-effect stmt followed, and
    //record the side-effect ir as epilog of current statement.
    IR ** epilog_ir_list;

    //Progagate information top down.
    //Inform the AST convertor that the current Tree is callee.
    //Generate ID for callee rather than LD if it is direct call.
    bool is_parse_callee;

    //Progagate information top down.
    //For lvalue expression, TR_ID should corresponding
    //with IR_ID, but IR_LD.
    bool is_lvalue;

    //Progagate information top down.
    //Set to true if we need to record operations at epilog.
    //e.g a++, post-add is epilog operation.
    bool is_record_epilog;

    //Progagate information top down.
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
    //when accessing b,
    //    t = ld(q)
    //    t = t + ofst(b)
    //    x = array(t, i)
    //
    //Accessing pattern: x = q->a
    //generate code to load value of a,
    //    t = ld(q)
    //    x = ild(t, ofst(b))
    bool is_compute_addr;
public:
    T2IRCtx() { ::memset(this, 0, sizeof(T2IRCtx)); }
    T2IRCtx(T2IRCtx const& src) { *this = src; }

    IR * getTopIRList() const { return *CONT_toplirlist(this); }
    IR * getEpilogIRList() const { return *CONT_epilogirlist(this); }
};


//Transform from C AST to IR.
class CTree2IR {
    COPY_CONSTRUCTOR(CTree2IR);
protected:
    xfe::Decl const* m_declared_return_type;
    Region * m_rg;
    RegionMgr * m_rm;
    TypeMgr * m_tm;
    xoc::Var * m_retval_buf;
    List<CaseValue*> * m_case_list; //for switch/case used only
    Stack<List<CaseValue*>*> m_case_list_stack; //for switch/case used only
    SMemPool * m_pool;
    xfe::LabelTab m_labtab;
protected:
    IR * convertFP(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertLogicalAND(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertLogicalOR(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertCallee(xfe::Tree const* t, bool * is_direct,
                       T2IRCtx const* cont);
    xoc::Var * convertReturnValBufVar(xoc::Type const* rettype,
                                      OUT UINT * return_val_size);
    //Handle return value buffer.
    IR * convertCallReturnBuf(xoc::Type const* rettype, IR const* callee,
                              OUT UINT * return_val_size,
                              OUT xoc::Var ** retval_buf);
    IR * convertCallReturnVal(IR * call, UINT return_val_size,
                              xoc::Var * retval_buf,
                              xoc::Type const* rettype,
                              INT lineno);
    IR * convertCallItself(xfe::Tree * t, IR * arglist,
                           IR * callee, bool is_direct, INT lineno,
                           T2IRCtx * cont);

    //Generate IR for field-access if the base-region is a structure that
    //returned by a function call.
    IR * convertFieldAccessForReturnValAggr(xfe::Tree const* t, IR * retval,
                                            T2IRCtx * cont);

    xoc::Var * genLocalVar(xoc::Sym const* sym, xoc::Type const* ty);
    xoc::Var * genLocalVar(CHAR const* name, xoc::Type const* ty);

    IR * only_left_last(IR * head);

    void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        ASSERTN(m_pool != nullptr, ("need pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }
public:
    CTree2IR(Region * rg, xfe::Decl const* retty)
    {
        ASSERT0(rg);
        //retty may be NULL.
        m_declared_return_type = retty;
        m_rg = rg;
        m_rm = rg->getRegionMgr();
        m_tm = rg->getTypeMgr();
        m_case_list = nullptr;
        m_retval_buf = nullptr;
        m_pool = smpoolCreate(16, MEM_COMM);
    }
    ~CTree2IR() { smpoolDelete(m_pool); }

    //The function construct an unique LabelInfo by given 'lab'.
    LabelInfo * getUniqueLabel(LabelInfo * lab)
    { return m_labtab.append_and_retrieve(lab); }

    //Generate a xoc::Var if the bytesize of RETURN is bigger than total size of
    //return-registers.
    //e.g: Given 32bit target machine, the return register is a0, a1,
    //If the return type is structure whose size is bigger than 64bit, we need
    //to generate an implcitly xoc::Var to indicate the stack buffer which used
    //to hold the return value.
    IR * genReturnValBuf(IR * ir);

    //Return function declared return-type.
    xfe::Decl const* getDeclaredReturnType() const
    { return m_declared_return_type; }

    //Return the number of mantissa.
    BYTE getMantissaNum(CHAR const* fpval);

    //Return XOC data type according to given CFE declaration.
    //If 'decl' presents a simply type, convert type-spec to xoc::DATA_TYPE
    //descriptor.
    //If 'decl' presents a pointer type, convert pointer-type to D_PTR.
    //If 'decl' presents an array, convert type to D_M descriptor.
    //size: return byte size of decl.
    static xoc::DATA_TYPE get_decl_dtype(xfe::Decl const* decl, UINT * size,
                                         TypeMgr * tm);

    //Construct XOC Region and convert C-Language-Ast to XOC IR.
    static bool generateRegion(RegionMgr * rm);

    IR * buildId(IN xfe::Tree * t);
    IR * buildId(IN xfe::Decl * id);
    IR * buildLoad(IN xfe::Tree * t);
    IR * buildLda(xfe::Tree const* t);

    bool is_istore_lhs(xfe::Tree const* t) const;
    bool is_readonly(xoc::Var const* v) const;
    bool is_alloc_heap(xoc::Var const* v) const;
    bool is_align(Sym const* sym) const;

    xoc::Type const* checkAndGenCVTType(xfe::Decl const* tgt,
                                        xfe::Decl const* src);
    IR * convertLDA(xfe::Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertCVT(xfe::Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertId(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertReturn(xfe::Tree * t, INT lineno, T2IRCtx * cont);
    IR * convertAssign(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertIncDec(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertPostIncDec(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertCall(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertPointerDeref(xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    //The function handles the array accessing for real array type declaration.
    //e.g: int a[10][20];
    //     ..= a[i][j], where a is real array.
    //     ..= ((int*)0x1234)[i], where 0x1234 is not real array.
    //  The array which is not real only could using 1-dimension array operator.
    //  namely, ..= ((int*)0x1234)[i][j] is illegal.
    IR * convertArraySubExpForArray(xfe::Tree * t, xfe::Tree * base, UINT n,
                                    TMWORD * elem_nums, T2IRCtx * cont);
    //base: base xfe::Tree node of ARRAY.
    IR * convertArraySubExp(xfe::Tree * base, TMWORD * elem_nums,
                            T2IRCtx * cont);
    IR * convertArray(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertSelect(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertSwitch(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertIndirectMemAccess(xfe::Tree const* t, INT lineno,
                                  T2IRCtx * cont);
    IR * convertDirectMemAccess(xfe::Tree const* t, INT lineno, T2IRCtx * cont);
    IR * convertDeref(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convertPragma(IN xfe::Tree * t, INT lineno, IN T2IRCtx * cont);
    IR * convert(IN xfe::Tree * t, IN T2IRCtx * cont);
};

} //namespace xocc
#endif
