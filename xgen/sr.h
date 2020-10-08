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
#ifndef __SR_H__
#define __SR_H__

namespace xgen {

class CG;
class SR;
class SRVec;

typedef enum _SR_TYPE {
    SR_UNDEF = 0,
    SR_REG,
    SR_INT_IMM,
    SR_FP_IMM,
    SR_VAR,
    SR_STR,
    SR_LAB,
}  SR_TYPE;

#define MAX_SR_NAME_BUF_LEN 1024

#ifdef _DEBUG_
SR const* checkSRVAR(SR const* ir);
SR const* checkSRLAB(SR const* ir);
SR const* checkSRREG(SR const* ir);
SR const* checkSRSTR(SR const* ir);
SR const* checkSRIMM(SR const* ir);

#define CK_SR_VAR(sr) (const_cast<SR*>(checkSRVAR(sr)))
#define CK_SR_LAB(sr) (const_cast<SR*>(checkSRLAB(sr)))
#define CK_SR_IMM(sr) (const_cast<SR*>(checkSRIMM(sr)))
#define CK_SR_STR(sr) (const_cast<SR*>(checkSRSTR(sr)))
#define CK_SR_REG(sr) (const_cast<SR*>(checkSRREG(sr)))
#else
#define CK_SR_VAR(sr) (sr)
#define CK_SR_LAB(sr) (sr)
#define CK_SR_IMM(sr) (sr)
#define CK_SR_STR(sr) (sr)
#define CK_SR_REG(sr) (sr)
#endif

//This class defined Symbol Register.
#define SR_type(sr) (sr)->type
#define SR_id(sr) (sr)->m_id //unique id for each SRs

//symbol register id.
#define SR_sregid(sr) (CK_SR_REG(sr))->u1.u2.symbol_regid
#define SR_regfile(sr) (CK_SR_REG(sr))->u1.u2.regfile

//Physical Register id, always start at 1.
#define SR_phy_reg(sr) (CK_SR_REG(sr))->u1.u2.phy_regid
#define SR_int_imm(sr) (CK_SR_IMM(sr))->u1.u3.u4.int_imm
#define SR_fp_imm(sr) (CK_SR_IMM(sr))->u1.u3.u4.fp_imm
#define SR_imm_size(sr) (CK_SR_IMM(sr))->u1.u3.size
#define SR_str(sr) (CK_SR_STR(sr))->u1.str
#define SR_var(sr) (CK_SR_VAR(sr))->u1.u4.var
#define SR_var_ofst(sr) (CK_SR_VAR(sr))->u1.u4.ofst
#define SR_spill_var(sr) (CK_SR_REG(sr))->u1.u2.spill_var
#define SR_var_ir(sr) (CK_SR_VAR(sr))->u1.u4.ir
#define SR_label(sr) (CK_SR_LAB(sr))->u1.label
#define SR_label_ofst(sr) (CK_SR_LAB(sr)) //Reserved

//If sr belong to a sr-vector, record the vector.
//sr is either register or immeidate.
#define SR_vec(sr) ((sr)->m_sr_vec)

//Record the sr's position in the vector, start at 0.
//sr is either register or immeidate.
#define SR_vec_idx(sr) ((sr)->m_sr_vec_idx)
#define SR_is_vec(sr) (SR_vec(sr) != NULL)
#define SR_is_str(sr) (SR_type(sr) == SR_STR)
#define SR_is_label(sr) (SR_type(sr) == SR_LAB)
#define SR_is_var(sr) (SR_type(sr) == SR_VAR)
#define SR_is_sp(sr) ((sr)->u2.s2.isSP)
#define SR_is_fp(sr) ((sr)->u2.s2.is_fp)
#define SR_is_gp(sr) ((sr)->u2.s2.is_gp)
#define SR_is_param_pointer(sr) ((sr)->u2.s2.is_param_pointer)
#define SR_is_rflag(sr) ((sr)->u2.s2.is_rflag)
#define SR_is_dedicated(sr) ((sr)->u2.s2.is_dedicated)
#define SR_is_ra(sr) ((sr)->u2.s2.is_return_address)
#define SR_is_global(sr) ((sr)->u2.s2.is_global)
#define SR_is_pred(sr) ((sr)->u2.s2.is_predicated)
#define SR_is_int_imm(sr) ((sr)->type == SR_INT_IMM)
#define SR_is_fp_imm(sr) ((sr)->type == SR_FP_IMM)
#define SR_is_imm(sr) (SR_is_int_imm(sr) || SR_is_fp_imm(sr))
#define SR_is_reg(sr) ((sr)->type == SR_REG)
#define SR_is_constant(sr) (SR_is_imm(sr) || SR_is_label(sr) || SR_is_var(sr))
class SR {
    COPY_CONSTRUCTOR(SR);
public:
    SR_TYPE type;
    UINT m_id; //unique identifier within a region
    union {
        UINT bitflags;
        struct {
            UINT isSP:1; //stack pointer
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

    union {
        struct {
            UINT symbol_regid; //symbol register id, start at 1.
            REGFILE regfile; //physical register file.
            REG phy_regid; //physical register id, start at 1.
            xoc::Var * spill_var; //xoc::Var to hold spilled register.
        } u2; //SR is register

        xoc::Sym * str; //present a const string

        struct {
            union {
                //Integer.
                //Note host LONGLONG should not less than HOST_INT,
                //otherwise the integer might be truncated wrongfully.
                HOST_INT int_imm;
                HOST_FP fp_imm; //float point.
            } u4;
            UINT size; //byte size of imm.
        } u3;

        struct {
            xoc::Var const* var; //represent memory.
            UINT ofst; //offset base on var.

            //Record the corresponding IR at middle-end if
            //the variable generated via IR.
            xoc::IR const* ir;
        } u4;

        xoc::LabelInfo const* label; //present Internal-Label or Custom-Label
    } u1;

    //Note SR which has composed SRVec can not make up another SRVec.
    //The relationship between SR and SRVec is unique.
    //SRs in SRVec do not have to be consecutive.
    INT m_sr_vec_idx;
    SRVec * m_sr_vec;

public:
    SR() { clean(); }
    virtual ~SR() {}

    virtual void copy(SR const* sr, bool is_clone_vec = false, CG * cg = NULL);
    virtual void clean();

    virtual void dump(CG * cg) const;

    SR_TYPE getType() const { return SR_type(this); } 
    virtual CHAR const* getAsmName(StrBuf & buf, CG * cg);
    virtual CHAR const* get_name(StrBuf & buf, CG * cg) const;
    virtual UINT getByteSize() const;
    REGFILE getRegFile() const { return SR_regfile(this); }

    //Physical Register id, always start at 1.
    REG getPhyReg() const { return SR_phy_reg(this); }

    //Get Symbol Register id.
    UINT getSReg() const { return SR_sregid(this); }

    //If sr belong to a sr-vector, record the vector.
    //sr is either register or immeidate.
    SRVec * getVec() const { return SR_vec(this); }

    //Record the sr's position in the vector, start at 0.
    //sr is either register or immeidate.
    INT getVecIdx() const { return SR_vec_idx(this); }    
    Var const* getVar() const { return SR_var(this); }
    Var const* getSpillVar() const { return SR_spill_var(this); }
    UINT getVarOfst() const { return SR_var_ofst(this); }
    LabelInfo const* getLabel() const { return SR_label(this); }
    IR const* getVarIR() const { return SR_var_ir(this); }
    Sym const* getStr() const { return SR_str(this); }
    UINT getImmSize() const { return SR_imm_size(this); }
    HOST_FP getFP() const { return SR_fp_imm(this); }
    HOST_INT getInt() const { return SR_int_imm(this); }

    UINT id() const { return SR_id(this); }
    virtual bool is_equal(SR const* v);
    bool is_vec() const { return SR_is_vec(this); }
    bool is_str() const { return SR_is_str(this); }
    bool is_label() const { return SR_is_label(this); }
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
    bool is_group() const { return SR_vec(this) != NULL; }
};

typedef List<SR*> SRList;
typedef xcom::Hash<SR*> SRHash; //SR hash table

//This class defined SR manager that used to manange the
//allocation and recycling.
class SRMgr : public List<SR*> {
protected:
    List<SR*> m_freesr_list;
    xcom::BitSet m_freesr_mark;
    Vector<SR*> m_sridx2sr_map;

    virtual SR * allocSR();
public:
    SRMgr() {}
    virtual ~SRMgr() { clean(); }
    virtual void clean();
    virtual void freeSR(SR * sr);
    virtual SR * get(UINT unique_id);
    virtual SR * genSR();
};


//This class defined the vector of SR.
//A SRVec binds several SR into one group, and each element SR in the group
//has a pointer which points to the group that the SR is belonged to.
class SRVec : public Vector<SR*, 1> {
public:
    SRVec() : Vector<SR*, 1>(2) {}
    UINT get_elem_count() {    return get_last_idx() + 1; }
};


//This class is used to manage the allocation and recycling of SRVec.
class SRVecMgr {
protected:
    List<SRVec*> m_sr_vec_list;
    SRVec * newSRVec();
public:
    SRVecMgr() {}
    virtual ~SRVecMgr() { clean(); }
    virtual void clean();
    virtual SR * genSRVec(UINT num, ...);
    virtual SR * genSRVec(List<SR*> & lst);
};

} //namespace xgen
#endif
