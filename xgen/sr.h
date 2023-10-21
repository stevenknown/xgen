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
#ifndef __SR_H__
#define __SR_H__

namespace xgen {

class CG;
class SR;
class SRVec;
class SRVecMgr;

#define SR_COUNT_INIT_VAL 1
#define LILIST_label(l) ((l)->m_label)

class LabelInfoList {
public:
    xoc::LabelInfo const* m_label;
    LabelInfoList * prev;
    LabelInfoList * next;
public:
    LabelInfoList() {}

    LabelInfoList * get_prev() const { return prev; }
    LabelInfoList * get_next() const { return next; }
    xoc::LabelInfo const* getLabel() const { return m_label; }
};

//This macro defined the maximum number of element that SRVec can recorded.
//User can extend this value if it is not big enough.
#define MAX_SRVEC_NUM 64
#define SYMREG_UNDEF 0
#define SRVEC_IDX_UNDEF -1
#define SRID_UNDEF 0

typedef enum _SR_TYPE {
    SR_UNDEF = 0,
    SR_REG,
    SR_INT_IMM,
    SR_FP_IMM,
    SR_VAR,
    SR_STR,
    SR_LAB,
    SR_LAB_LIST,
} SR_CODE;

#define MAX_SR_NAME_BUF_LEN 1024

#ifdef _DEBUG_
SR const* checkSRVAR(SR const* ir);
SR const* checkSRLAB(SR const* ir);
SR const* checkSRLABList(SR const* ir);
SR const* checkSRREG(SR const* ir);
SR const* checkSRSTR(SR const* ir);
SR const* checkSRIMM(SR const* ir);

#define CK_SR_VAR(sr) (const_cast<SR*>(checkSRVAR(sr)))
#define CK_SR_LAB(sr) (const_cast<SR*>(checkSRLAB(sr)))
#define CK_SR_LAB_LIST(sr) (const_cast<SR*>(checkSRLABList(sr)))
#define CK_SR_IMM(sr) (const_cast<SR*>(checkSRIMM(sr)))
#define CK_SR_STR(sr) (const_cast<SR*>(checkSRSTR(sr)))
#define CK_SR_REG(sr) (const_cast<SR*>(checkSRREG(sr)))
#else
#define CK_SR_VAR(sr) (sr)
#define CK_SR_LAB(sr) (sr)
#define CK_SR_LAB_LIST(sr) (sr)
#define CK_SR_IMM(sr) (sr)
#define CK_SR_STR(sr) (sr)
#define CK_SR_REG(sr) (sr)
#endif

#define SR_code(sr) (sr)->m_code
#define SR_id(sr) (sr)->m_id //unique id for each SRs

//If sr belong to a sr-vector, record the vector.
//sr is either register or immeidate.
#define SR_vec(sr) ((sr)->m_sr_vec)

//Record the sr's position in the vector, start at 0.
//sr is either register or immeidate.
#define SR_vec_idx(sr) ((sr)->m_sr_vec_idx)
#define SR_is_str(sr) (SR_code(sr) == SR_STR)
#define SR_is_label(sr) (SR_code(sr) == SR_LAB)
#define SR_is_var(sr) (SR_code(sr) == SR_VAR)
#define SR_is_label_list(sr) (SR_code(sr) == SR_LAB_LIST)
#define SR_is_int_imm(sr) (SR_code(sr) == SR_INT_IMM)
#define SR_is_fp_imm(sr) (SR_code(sr) == SR_FP_IMM)
#define SR_is_reg(sr) (SR_code(sr) == SR_REG)
#define SR_is_vec(sr) (SR_vec(sr) != nullptr)
#define SR_is_sp(sr) ((sr)->u2.s2.is_sp)
#define SR_is_fp(sr) ((sr)->u2.s2.is_fp)
#define SR_is_gp(sr) ((sr)->u2.s2.is_gp)
#define SR_is_param_pointer(sr) ((sr)->u2.s2.is_param_pointer)
#define SR_is_rflag(sr) ((sr)->u2.s2.is_rflag)
#define SR_is_dedicated(sr) ((sr)->u2.s2.is_dedicated)
#define SR_is_ra(sr) ((sr)->u2.s2.is_return_address)
#define SR_is_global(sr) ((sr)->u2.s2.is_global)
#define SR_is_pred(sr) ((sr)->u2.s2.is_predicated)
#define SR_is_imm(sr) (SR_is_int_imm(sr) || SR_is_fp_imm(sr))
#define SR_is_constant(sr) (SR_is_imm(sr) || SR_is_label(sr) || \
                            SR_is_var(sr) || SR_is_label_list(sr))

//The class is the basic structure to describe miscellaneous
//Specific-Representation of the resource of target machine.
class SR {
    COPY_CONSTRUCTOR(SR);
public:
    SR_CODE m_code;
    UINT m_id; //unique identifier within a region
    union {
        UINT bitflags;
        struct {
            UINT is_sp:1; //stack pointer
            UINT is_fp:1; //frame pointer
            UINT is_gp:1; //global register
            UINT is_param_pointer:1; //param poniter register
            UINT is_rflag:1; //rflag register
            UINT is_dedicated:1; //dedicated register
            UINT is_global:1; //global symbol register
            UINT is_predicated:1; //predicated register
            UINT is_return_address:1; //function return-address register.
        } s2;
    } u2;

    //Record the sr's position in the vector, start at 0.
    //sr is either register or immeidate.
    //Note SR which has composed SRVec can not make up another SRVec.
    //The relationship between SR and SRVec is unique.
    //SRs in SRVec do not have to be consecutive.
    INT m_sr_vec_idx;
    SRVec * m_sr_vec;
public:
    SR() { clean(); }
    virtual ~SR() {}

    virtual void copy(SR const* sr, bool is_clone_vec, CG * cg);
    virtual void copy(SR const* sr);
    virtual void clean()
    {
        m_code = SR_UNDEF;
        u2.bitflags = 0;
        m_sr_vec = nullptr; //vec will be dropped to pool. TODO: recycle it.
        m_sr_vec_idx = SRVEC_IDX_UNDEF;
    }

    //Do not count m_sr_vec into size because it is allocated in outside pool.
    virtual size_t count_mem() const { return sizeof(*this); }

    virtual void dump(CG * cg) const;

    SR_CODE getCode() const { return SR_code(this); }
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const
    { DUMMYUSE((cg, buf)); return nullptr; }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const
    { DUMMYUSE((cg, buf)); return nullptr; }
    virtual UINT getByteSize() const;
    inline REGFILE getRegFile() const;
    inline Reg getPhyReg() const;
    inline UINT getSymReg() const;

    //If sr belong to a sr-vector, record the vector.
    //sr is either register or immeidate.
    SRVec * getVec() const { return SR_vec(this); }

    //Record the sr's position in the vector, start at 0.
    //sr is either register or immeidate.
    INT getVecIdx() const { return SR_vec_idx(this); }
    inline Var const* getVar() const;
    inline Var const* getSpillVar() const;
    inline TMWORD getVarOfst() const;
    inline LabelInfo const* getLabel() const;
    inline LabelInfoList const* getLabelList() const;
    inline IR const* getVarIR() const;
    inline Sym const* getStr() const;
    inline UINT getImmSize() const;
    inline HOST_FP getFP() const;
    inline HOST_INT getInt() const;

    UINT id() const { return SR_id(this); }
    virtual bool is_equal(SR const* v);
    bool is_vec() const { return SR_is_vec(this); }
    bool is_str() const { return SR_is_str(this); }
    bool is_label() const { return SR_is_label(this); }
    bool is_label_list() const { return SR_is_label_list(this); }
    bool is_var() const { return SR_is_var(this); }
    bool is_sp() const { return SR_is_sp(this); }
    bool is_fp() const { return SR_is_fp(this); }
    bool is_gp() const { return SR_is_gp(this); }
    bool is_param_pointer() const { return SR_is_param_pointer(this); }
    bool is_rflag() const { return SR_is_rflag(this); }
    bool is_dedicated() const { return SR_is_dedicated(this); }
    bool is_ra() const { return SR_is_ra(this); }
    bool is_global() const { return SR_is_global(this); }
    bool is_pred() const { return SR_is_pred(this); }
    bool is_int_imm() const { return SR_is_int_imm(this); }
    bool is_fp_imm() const { return SR_is_fp_imm(this); }
    bool is_imm() const { return SR_is_imm(this); }
    bool is_reg() const { return SR_is_reg(this); }
    bool is_constant() const { return SR_is_constant(this); }
    bool is_group() const { return SR_vec(this) != nullptr; }
};

#include "sr_def.h"

//
//START SR
//
REGFILE SR::getRegFile() const
{
    return ((RegSR*)this)->regfile;
    //return SR_regfile(this);
}


Reg SR::getPhyReg() const
{
    return SR_phy_reg(this);
}


UINT SR::getSymReg() const
{
    return SR_sym_reg(this);
}


Var const* SR::getVar() const
{
    return SR_var(this);
}


Var const* SR::getSpillVar() const
{
    return SR_spill_var(this);
}


TMWORD SR::getVarOfst() const
{
    return SR_var_ofst(this);
}


LabelInfo const* SR::getLabel() const
{
    return SR_label(this);
}


LabelInfoList const* SR::getLabelList() const
{
    return SR_label_list(this);
}


IR const* SR::getVarIR() const
{
    return SR_var_ir(this);
}


Sym const* SR::getStr() const
{
    return SR_str(this);
}


UINT SR::getImmSize() const
{
    return SR_imm_size(this);
}


HOST_FP SR::getFP() const
{
    return SR_fp_imm(this);
}


HOST_INT SR::getInt() const
{
    return SR_int_imm(this);
}
//END SR


typedef List<SR*> SRList;
typedef xcom::Hash<SR*> SRHash; //SR hash table

//This class defined SR manager that used to manange the
//allocation and recycling.
class SRMgr {
protected:
    xcom::Vector<SR*> m_sridx2sr; //sridx is dense integer.
    UINT m_sr_count;
protected:
    virtual SR * allocSR(SR_CODE c);
public:
    SRMgr() { m_sr_count = SRID_UNDEF; }
    virtual ~SRMgr() { clean(); }
    virtual void clean();
    size_t count_mem() const
    {
        size_t count = 0;
        count += m_sridx2sr.count_mem();
        SR * sr = nullptr;
        for (VecIdx i = 0; i <= m_sridx2sr.get_last_idx(); i++) {
            sr = m_sridx2sr[i];
            if (sr != nullptr) { break; }
        }
        if (sr != nullptr) { count += sr->count_mem() * getSRNum(); }
        return count;
    }
    void freeSR(SR * sr);
    UINT getSRNum() const { return m_sridx2sr.get_elem_count(); }
    SR * getSR(UINT unique_id) { return m_sridx2sr.get(unique_id); }
    virtual SR * genSR(SR_CODE c);
};


//This class is used to manage the allocation and recycling of SRVec.
class SRVecMgr {
    SMemPool * m_pool;
protected:
    SRVec * allocSRVec();
public:
    SRVecMgr() { m_pool = nullptr; init(); }
    ~SRVecMgr() { destroy(); }

    size_t count_mem() const
    { return sizeof(*this) + smpoolGetPoolSize(m_pool); }

    void init();
    void destroy()
    {
        if (m_pool == nullptr) { return; }
        smpoolDelete(m_pool);
        m_pool = nullptr;
    }

    SMemPool * get_pool() const { return m_pool; }
    SR * genSRVec(UINT num, ...);
    SR * genSRVec(List<SR*> & lst);
};


//This class defined the vector of SR.
//A SRVec binds several SR into one group, and each element SR in the group
//has a pointer which points to the group that the SR is belonged to.
//Note whole SRVec and its growable buffer are allocated in mempool.
class SRVec : public SimpleVector<SR*, 1, MAX_SRVEC_NUM> {
public:
    UINT get_elem_count() { return getElemNum(); }

    void set(UINT i, SR * elem, SRVecMgr * mgr)
    { SimpleVector<SR*, 1, MAX_SRVEC_NUM>::set(i, elem, mgr->get_pool()); }
};

} //namespace xgen
#endif
