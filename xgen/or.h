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
#ifndef __OR_H__
#define __OR_H__

namespace xgen {

class OR;
class ORMgr;
class ORBB;
class IR2OR;
class RecycORListMgr;

typedef xcom::C<OR*> ORCt; //OR container
typedef Vector<OR*> ORVec; //OR vector

#define OFST_STORE_BASE 0
#define OFST_STORE_OFST 1
#define OFST_STORE_VAL 2
#define OFST_LOAD_BASE 0
#define OFST_LOAD_OFST 1
#define ORID_UNDEF 0

//OR Descriptor
#define OTD_code(o) ((o)->m_code)
#define OTD_name(o) ((o)->m_name)
#define OTD_unit(o) ((o)->m_unit)
#define OTD_equ_or_codes(o) ((o)->m_equ_or_codes)
#define OTD_srd_group(o) ((o)->m_sr_desc_group)
#define OTD_sche_info(o) ((o)->m_sche_info)
#define OTD_is_fake(o) ((o)->m_is_fake)
#define OTD_is_bus(o) ((o)->m_is_bus)
#define OTD_is_nop(o) ((o)->m_is_nop)
#define OTD_is_volatile(o)  ((o)->m_is_volatile)
#define OTD_is_side_effect(o) ((o)->m_is_side_effect)
#define OTD_is_asm(o) ((o)->m_is_asm)
#define OTD_is_predicated(o) ((o)->m_is_predicated)
#define OTD_is_call(o) ((o)->m_is_call_stmt)
#define OTD_is_cond_br(o) ((o)->m_is_conditional_br)
#define OTD_is_uncond_br(o) ((o)->m_is_unconditional_br)
#define OTD_is_indirect_br(o) ((o)->m_is_indirect_br)
#define OTD_is_return(o) ((o)->m_is_return)
#define OTD_is_unaligned(o) ((o)->m_is_unaligned)
#define OTD_is_store(o) ((o)->m_is_store)
#define OTD_is_load(o) ((o)->m_is_ld)
#define OTD_is_eq(o) ((o)->m_is_eq)
#define OTD_is_lt(o) ((o)->m_is_lt)
#define OTD_is_gt(o) ((o)->m_is_gt)
#define OTD_is_movi(o) ((o)->m_is_movi)
#define OTD_is_addi(o) ((o)->m_is_addi)
#define OTD_is_subi(o) ((o)->m_is_subi)
class ORCodeDesc {
public:
    //////////////////////////////////////////////////////////////////////////
    //NOTE: DO NOT CHANGE THE LAYOUT OF MEMBER.                             //
    //THE LAYOUT OF MEMBER CORRESPONDS TO TERGET DEPENDENT CONFIG FILE.     //
    //////////////////////////////////////////////////////////////////////////
    OR_CODE m_code;
    CHAR const* m_name;

    //Hardware unit.
    UNIT m_unit;

    //The equivalent or-types that are same in utilities for
    //different hardware function-unit.
    EquORCodes * m_equ_or_codes;

    //Description of source operands and result operands.
    SRDescGroup<> * m_sr_desc_group;

    ORScheInfo m_sche_info;

    UINT m_is_predicated:1; //True if OR has a preidcate register.
    UINT m_is_fake:1;
    UINT m_is_bus:1;
    UINT m_is_nop:1;
    UINT m_is_volatile:1;
    UINT m_is_side_effect:1;
    UINT m_is_asm:1;
    UINT m_is_call_stmt:1;
    UINT m_is_conditional_br:1;
    UINT m_is_unconditional_br:1;
    UINT m_is_indirect_br:1;
    UINT m_is_return:1;
    UINT m_is_unaligned:1;
    UINT m_is_store:1;
    UINT m_is_ld:1;
    UINT m_is_eq:1;
    UINT m_is_lt:1;
    UINT m_is_gt:1;
    UINT m_is_movi:1;
    UINT m_is_addi:1;
    UINT m_is_subi:1;
public:
    SRDescGroup<> const* getSRDGroup() const { return OTD_srd_group(this); }
};


//Target Operation Representation
#define OR_code(o) ((o)->code)
#define OR_ct(o) ((o)->container)
#define OR_clust(o) ((o)->clust)
#define OR_id(o) ((o)->uid)
#define OR_order(o) ((o)->order)
#define OR_bb(o) ((o)->ubb)
#define OR_unit(o) OTD_unit(tmGetORCodeDesc(o->getCode()))
#define OR_code_name(o) OTD_name(tmGetORCodeDesc(o->getCode()))
#define OR_dbx(o) ((o)->dbx)
#define OR_is_call(o) OTD_is_call(tmGetORCodeDesc(o->getCode()))
#define OR_is_uncond_br(o) OTD_is_uncond_br(tmGetORCodeDesc(o->getCode()))
#define OR_is_indirect_br(o) OTD_is_indirect_br(tmGetORCodeDesc(o->getCode()))
#define OR_is_cond_br(o) OTD_is_cond_br(tmGetORCodeDesc(o->getCode()))
#define OR_is_return(o) OTD_is_return(tmGetORCodeDesc(o->getCode()))
#define OR_is_br(o) (OR_is_cond_br(o) || OR_is_uncond_br(o) || \
                     OR_is_return(o) || OR_is_call(o))
#define OR_is_predicated(o) OTD_is_predicated(tmGetORCodeDesc(o->getCode()))
#define OR_is_load(o) OTD_is_load(tmGetORCodeDesc(o->getCode()))
#define OR_is_store(o) OTD_is_store(tmGetORCodeDesc(o->getCode()))
#define OR_is_signed(o) ((o)->u1.s1.is_signed)
#define OR_is_spill(o) ((o)->u1.s1.is_spill)
#define OR_is_reload(o) ((o)->u1.s1.is_reload)
#define OR_is_terminate(o) ((o)->u1.s1.is_terminate_control_flow)
#define OR_is_need_compute_var_ofst(o)  ((o)->u1.s1.need_to_compute_var_ofst)
#define OR_is_mem(o) (o->is_store() || o->is_load())
#define OR_is_fake(o) OTD_is_fake(tmGetORCodeDesc(o->getCode()))
#define OR_is_bus(o) OTD_is_bus(tmGetORCodeDesc(o->getCode()))
#define OR_is_nop(o) OTD_is_nop(tmGetORCodeDesc(o->getCode()))
#define OR_is_volatile(o) OTD_is_volatile(tmGetORCodeDesc(o->getCode()))
#define OR_is_side_effect(o) OTD_is_side_effect(tmGetORCodeDesc(o->getCode()))
#define OR_is_asm(o) OTD_is_asm(tmGetORCodeDesc(o->getCode()))
#define OR_is_unaligned(o) OTD_is_unaligned(tmGetORCodeDesc(o->getCode()))
#define OR_is_eq(o) OTD_is_eq(tmGetORCodeDesc(o->getCode()))
#define OR_is_lt(o) OTD_is_lt(tmGetORCodeDesc(o->getCode()))
#define OR_is_gt(o) OTD_is_gt(tmGetORCodeDesc(o->getCode()))
#define OR_is_movi(o) OTD_is_movi(tmGetORCodeDesc(o->getCode()))
#define OR_is_addi(o) OTD_is_addi(tmGetORCodeDesc(o->getCode()))
#define OR_is_subi(o) OTD_is_subi(tmGetORCodeDesc(o->getCode()))
#define OR_flag(o) ((o)->u1.s1byte)
class OR {
    COPY_CONSTRUCTOR(OR);
    friend class ORMgr;
protected:
    typedef SimpleVector<SR*, 2, MAX_OR_OPERAND_NUM> OpndVec;
    typedef SimpleVector<SR*, 2, MAX_OR_RESULT_NUM> ResultVec;
    OpndVec m_opnd; //Operand of micro operation
    ResultVec m_result; //Result of micro operation

protected:
    OpndVec * get_opnd_vec() { return &m_opnd; }
    ResultVec * get_result_vec() { return &m_result; }

public:
    //each op has its own unique id.
    //DO NOT MODIFY 'id' DURING cleaning, cloning or copying of OR.
    UINT uid;

    OR_CODE code; //operation type, various to different target machine.

    //Container of OR, used for List operations,
    //only available when OR in list.
    ORCt * container;

    CLUST clust; //cluster, various to different target machine.
    INT order; //identify the ordinal of OR in BB.
    ORBB * ubb;
    union {
        struct {
            BYTE is_signed:1; //Is OR signed?
            BYTE is_spill:1; //Is OR spilling operation?
            BYTE is_reload:1; //Is OR reloading operation?
            BYTE is_terminate_control_flow:1; //Is OR terminate control flow?

            //Set to true if OR has a dummy offset which indicated by Var.
            //And the offset should be caclulated to be an integer
            //before emiting assembly.
            BYTE need_to_compute_var_ofst:1;
        } s1;
        BYTE s1byte;
    } u1;
    Dbx dbx;

public:
    OR() { init(); }
    virtual ~OR() { destroy(); }

    void clean();
    virtual void clone(OR const* o, CG * cg);
    void copyDbx(IR const* ir);

    void destroy()
    {
        m_opnd.destroy();
        m_result.destroy();
    }
    virtual CHAR const* dump(StrBuf & buf, CG * cg) const;
    virtual void dump(CG * cg) const;

    OR_CODE getCode() const { return code; }
    CHAR const* getCodeName() const { return OR_code_name(this); }
    ORBB * getBB() const { return OR_bb(this); }
    SR * get_opnd(UINT i) const
    {
        ASSERT0(i <= m_opnd.getElemNum());
        return m_opnd.get(i);
    }

    SR * get_result(UINT i) const
    {
        ASSERT0(i <= m_result.getElemNum());
        return m_result.get(i);
    }

    virtual SR * get_pred() //get predicate register
    {
        ASSERTN(HAS_PREDICATE_REGISTER, ("target not support"));
        return get_opnd(0);
    }

    virtual SR * get_mov_to() { return get_result(0); }

    virtual SR * get_mov_from()
    { return get_opnd(HAS_PREDICATE_REGISTER + 0); }

    virtual SR * get_copy_to() { return get_result(0); }
    virtual SR * get_lda_result() { return get_result(0); }

    virtual SR * get_copy_from()
    { return get_opnd(HAS_PREDICATE_REGISTER + 0); }

    virtual SR * get_lda_base()
    { return get_opnd(HAS_PREDICATE_REGISTER + 0); }

    //Get Label operand, it should be a label SR, the default operand index
    //is HAS_PREDICATE_REGISTER + 0.
    //Layout of opnds:
    //-----------------------------------------------------------
    // 0         | 1     | 2   |
    // predicate | label | ... |
    // register  |       |     |
    //-----------------------------------------------------------
    virtual LabelInfo const* getLabel() const
    {
        SR * sr = get_opnd(HAS_PREDICATE_REGISTER + 0);
        ASSERT0(sr && sr->is_label());
        return SR_label(sr);
    }

    //Get Label List, it should be a label-list SR, the default operand index
    //is HAS_PREDICATE_REGISTER + 0.
    //Layout of opnds:
    //-----------------------------------------------------------
    // 0         | 1     | 2   |
    // predicate | label | ... |
    // register  | list  |     |
    //-----------------------------------------------------------
    virtual LabelInfoList const* getLabelList() const
    {
        SR * sr = get_opnd(HAS_PREDICATE_REGISTER + 0);
        ASSERT0(sr && sr->is_label_list());
        return SR_label_list(sr);
    }

    //OR format is : <- base, ofst, store_value0, store_value1, ...
    SR * get_store_ofst()
    {
        ASSERT0(OR_is_store(this));
        return get_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_OFST);
    }

    //Return the base SR for memory load/store.
    SR * get_mem_base()
    {
        ASSERT0(OR_is_store(this) || OR_is_load(this));
        if (OR_is_store(this)) {
            return get_store_base();
        }
        return get_load_base();
    }

    //Get LOAD VALUE, the default LOAD has one load-value opnd.
    //There are may be multiple load-value opnds if target
    //is, for example SIMD machine.
    //Layout of opnds:
    //---------------------------------------------------------
    // 0         | 1    | 2      | 3         | 4         |     |
    // predicate | base | offset | load-val1 | load-val2 | ... |
    // register  |      |        |           |           |     |
    //---------------------------------------------------------
    //'idx': index of load-values.
    SR * get_load_val(UINT idx)
    {
        ASSERT0(OR_is_load(this));
        return get_result(idx);
    }

    UINT get_num_load_val() const { return result_num(); }

    //Get BASE of LOAD MEMORY ADDRESS.
    SR * get_load_base()
    {
        ASSERT0(OR_is_load(this));
        return get_opnd(HAS_PREDICATE_REGISTER + OFST_LOAD_BASE);
    }

    //Get OFFSET of BASE of LOAD MEMORY ADDRESS.
    SR * get_load_ofst()
    {
        ASSERT0(OR_is_load(this));
        return get_opnd(HAS_PREDICATE_REGISTER + OFST_LOAD_OFST);
    }

    //Get opnd SR, the default STORE has at least one store-value.
    //OR format is : <- base, ofst, store_value0, store_value1, ...
    //'idx': index of store-value.
    SR * get_store_val(UINT idx)
    {
        ASSERT0(OR_is_store(this));
        return get_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_VAL + idx);
    }
    SR * get_first_store_val() { return get_store_val(0); }

    UINT get_num_store_val() const
    { return opnd_num() - OFST_STORE_VAL - HAS_PREDICATE_REGISTER; }

    //OR format is : <- base, ofst, store_value0, store_value1, ...
    SR * get_store_base()
    {
        ASSERT0(OR_is_store(this));
        return get_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_BASE);
    }
    INT get_opnd_idx(SR * sr) const;
    INT get_result_idx(SR * sr) const;
    virtual SR * get_imm_sr();

    UINT opnd_num() const { return tmGetOpndNum(OR_code(this)); }
    UINT result_num() const { return tmGetResultNum(OR_code(this)); }

    bool hasSideEffect(bool) const { return OR_is_side_effect(this); }

    UINT id() const { return OR_id(this); }
    void init()
    {
        code = OR_UNDEF;
        container = nullptr;
        clust = CLUST_UNDEF;
        uid = ORID_UNDEF;
        order = -1;
        ubb = nullptr;
        OR_flag(this) = 0;
        m_opnd.init();
        m_result.init();
        dbx.clean();
    }
    virtual bool is_equal(OR const* o) const;
    //Return true if 'o' depicted a label.
    bool is_label_or() const
    {
        #ifdef _DEBUG_
        if (OR_code(this) == OR_label) {
            ASSERT0(getLabel());
        }
        #endif
        return OR_code(this) == OR_label;
    }
    bool isConditionalBr() const { return OR_is_cond_br(this); }
    bool isUnconditionalBr() const { return OR_is_uncond_br(this); }
    bool isIndirectBr() const { return OR_is_indirect_br(this); }
    bool is_return() const { return OR_is_return(this); }
    bool is_call() const { return OR_is_call(this); }
    bool is_uncond_br() const { return OR_is_uncond_br(this); }
    bool is_indirect_br() const { return OR_is_indirect_br(this); }
    bool is_cond_br() const { return OR_is_cond_br(this); }
    bool is_br() const { return OR_is_br(this); }
    bool is_pred() const { return OR_is_predicated(this); }
    bool is_load() const { return OR_is_load(this); }
    bool is_store() const { return OR_is_store(this); }
    bool is_signed() const { return OR_is_signed(this); }
    bool is_spill() const { return OR_is_spill(this); }
    bool is_reload() const { return OR_is_reload(this); }
    bool is_mem() const { return OR_is_mem(this); }
    bool is_fake() const { return OR_is_fake(this); }
    bool is_bus() const { return OR_is_bus(this); }
    bool is_nop() const { return OR_is_nop(this); }
    bool is_volatile() const { return OR_is_volatile(this); }
    bool is_side_effect() const { return OR_is_side_effect(this); }
    bool is_asm() const { return OR_is_asm(this); }
    bool is_unaligned() const { return OR_is_unaligned(this); }
    bool is_eq() const { return OR_is_eq(this); }
    bool is_lt() const { return OR_is_lt(this); }
    bool is_gt() const { return OR_is_gt(this); }
    bool is_movi() const { return OR_is_movi(this); }
    bool is_addi() const { return OR_is_addi(this); }
    bool is_subi() const { return OR_is_subi(this); }
    bool isMultiConditionalBr() const { return false; }
    bool isSpadjustImm() const { return OR_code(this) == OR_spadjust_i; }
    bool isSpadjustReg() const { return OR_code(this) == OR_spadjust_r; }
    bool isSpadjust() const { return isSpadjustImm() || isSpadjustReg(); }
    //Return true if ir terminates the control flow.
    bool is_terminate() const { return OR_is_terminate(this); }

    bool needComputeVAROfst() const
    { return OR_is_need_compute_var_ofst(this); }

    //Set STORE VALUE, it should be a register, the default STORE
    //has one store-value opnd.
    //There are may be multiple store-value opnds if target is SIMD machine.
    //Layout of opnds:
    //-----------------------------------------------------------
    // 0         | 1    | 2      | 3          | 4          |     |
    // predicate | base | offset | store-val1 | store-val2 | ... |
    // register  |      |        |            |            |     |
    //-----------------------------------------------------------
    //'idx': index of store-values.
    void set_store_val(SR * val, CG * cg, UINT idx)
    {
        ASSERT0(OR_is_store(this));
        set_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_VAL + idx, val, cg);
    }
    void set_first_store_val(SR * val, CG * cg) { set_store_val(val, cg, 0); }

    //Set BASE of STORE MEMORY ADDRESS.
    void set_store_base(SR * base, CG * cg)
    {
        ASSERT0(OR_is_store(this));
        set_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_BASE, base, cg);
    }

    //Set OFFSET of BASE of STORE MEMORY ADDRESS.
    void set_store_ofst(SR * ofst, CG * cg)
    {
        ASSERT0(OR_is_store(this));
        set_opnd(HAS_PREDICATE_REGISTER + OFST_STORE_OFST, ofst, cg);
    }

    //Set result register, default load-operation has one result.
    void set_load_val(SR * val, CG * cg, UINT idx)
    {
        ASSERT0(OR_is_load(this));
        set_result(idx, val, cg);
    }
    void set_first_load_val(SR * val, CG * cg) { set_load_val(val, cg, 0); }

    //OR format is : result <- base, ofst
    void set_load_base(SR * base, CG * cg)
    {
        ASSERT0(OR_is_load(this));
        set_opnd(HAS_PREDICATE_REGISTER + OFST_LOAD_BASE, base, cg);
    }

    //OR format is : result <- base, ofst
    void set_load_ofst(SR * ofst, CG * cg)
    {
        ASSERT0(OR_is_load(this));
        set_opnd(HAS_PREDICATE_REGISTER + OFST_LOAD_OFST, ofst, cg);
    }

    void set_opnd(INT i, SR * sr, CG * cg);
    void set_result(INT i, SR * sr, CG * cg);
    virtual void set_pred(SR * v, CG * cg) //set predicate register
    {
        ASSERTN(HAS_PREDICATE_REGISTER, ("target not support"));
        set_opnd(0, v, cg);
    }

    virtual void set_mov_from(SR * v, CG * cg)
    { set_opnd(HAS_PREDICATE_REGISTER + 0, v, cg); }

    virtual void set_copy_from(SR * v, CG * cg)
    { set_opnd(HAS_PREDICATE_REGISTER + 0, v, cg); }

    virtual void set_lda_base(SR * v, CG * cg)
    { set_opnd(HAS_PREDICATE_REGISTER + 0, v, cg); }

    //Set Label operand, it should be a label SR, the default operand index
    //is HAS_PREDICATE_REGISTER + 0.
    //Layout of opnds:
    //-----------------------------------------------------------
    // 0         | 1     | 2   |
    // predicate | label | ... |
    // register  |       |     |
    //-----------------------------------------------------------
    virtual void setLabel(SR * v, CG * cg);

    //Get Label List, it should be a label-list SR, the default operand index
    //is HAS_PREDICATE_REGISTER + 0.
    //Layout of opnds:
    //-----------------------------------------------------------
    // 0         | 1     | 2   |
    // predicate | label | ... |
    // register  | list  |     |
    //-----------------------------------------------------------
    virtual void setLabelList(SR * v, CG * cg);
    virtual void set_lda_result(SR * v, CG * cg) { set_result(0, v, cg); }
    virtual void set_mov_to(SR * v, CG * cg) { set_result(0, v, cg); }
    virtual void set_copy_to(SR * v, CG * cg) { set_result(0, v, cg); }
};

typedef List<OR const*> ConstORList;

class ORList : public List<OR*> {
    COPY_CONSTRUCTOR(ORList);
public:
    ORList() {}
    ~ORList() {}

    void append_tail(OR * o);
    //Move elements in 'ors' to tail of current list.
    void move_tail(MOD ORList & ors);

    void copyDbx(IR const* ir)
    {
        ASSERT0(ir);
        if (IR_ai(ir) == nullptr) { return; }
        DbxAttachInfo * da = (DbxAttachInfo*)IR_ai(ir)->get(AI_DBX);
        if (da == nullptr) { return; }
        for (OR * o = get_head(); o != nullptr; o = get_next()) {
            OR_dbx(o).copy(da->dbx);
        }
    }
    void copyDbx(Dbx const* dbx)
    {
        ASSERT0(dbx);
        for (OR * o = get_head(); o != nullptr; o = get_next()) {
            OR_dbx(o).copy(*dbx);
        }
    }

    void dump(CG * cg);

    void set_pred(IN SR * pred, CG * cg)
    {
        ASSERT0(pred);
        for (OR * o = get_head(); o != nullptr; o = get_next()) {
            o->set_pred(pred, cg);
        }
    }
};

//This class defined ORList that often used as shared object during functions.
//If an preemptive object is set as 'occupied', it can not be used for new
//propuse until it released.
class PreemptiveORList : public ORList {
    COPY_CONSTRUCTOR(PreemptiveORList);
    BYTE m_is_occupied:1;
public:
    PreemptiveORList() { m_is_occupied = false; }
    bool is_occupied() const { return m_is_occupied; }
    void occupy() { clean(); m_is_occupied = true; }
    void release() { m_is_occupied = false; }
};


typedef xcom::Hash<OR*> ORHash;
typedef xcom::TTab<OR*> ORTab;
typedef xcom::TTab<UINT> ORIdTab;
typedef xcom::TTabIter<OR*> ORTabIter;

//
//START ORMgr
//
class ORMgr : public Vector<OR*> {
    COPY_CONSTRUCTOR(ORMgr);
    friend class OR;
protected:
    CG * m_cg;
    SRMgr * m_sr_mgr;
    SMemPool * m_pool;
    List<OR*> m_free_or_list;

protected:
    //Alllocate memory of OR.
    virtual OR * allocOR();

public:
    ORMgr(SRMgr * srmgr);
    virtual ~ORMgr() { clean(); }

    void clean();
    virtual size_t count_mem() const
    {
        size_t count = sizeof(*this);
        count += m_free_or_list.count_mem();
        count -= sizeof(m_free_or_list);
        return count;
    }

    SMemPool * get_pool() const { return m_pool; }
    OR * getOR(UINT id);
    //Generate OR object.
    OR * genOR(OR_CODE ort, CG * cg);
    virtual void freeOR(IN OR * o);
    void freeSR(OR * o);
};
//END ORMgr


//
//START RecycORList
//
//This class defined recyclable ORList.
//The object will be recycled when it destructed.
class RecycORList {
    COPY_CONSTRUCTOR(RecycORList);
    ORList * m_entity;
    RecycORListMgr * m_mgr;

    void init(RecycORListMgr * mgr);
public:
    RecycORList(RecycORListMgr * mgr);
    RecycORList(IR2OR * ir2or); //to facilitate IR2OR invocation.
    ~RecycORList();

    void append_tail(OR * o) { m_entity->append_tail(o); }

    //Move elements in 'ors' to tail of current list.
    void move_tail(ORList & ors) { m_entity->move_tail(ors); }
    void move_tail(RecycORList & ors) { m_entity->move_tail(ors.getList()); }

    void copyDbx(IR const* ir) { m_entity->copyDbx(ir); }
    void copyDbx(Dbx const* dbx) { m_entity->copyDbx(dbx); }
    void clean() { m_entity->clean(); }

    void dump(CG * cg) { m_entity->dump(cg); }

    ORList & getList() const { return *m_entity; }
    UINT get_elem_count() const { return m_entity->get_elem_count(); }

    void set_pred(IN SR * pred, CG * cg) { m_entity->set_pred(pred, cg); }
};


class RecycORListMgr {
    COPY_CONSTRUCTOR(RecycORListMgr);
    List<ORList*> m_free_list;
public:
    RecycORListMgr() {}
    ~RecycORListMgr();
    ORList * getFree() { return m_free_list.remove_head(); }
    void addFree(ORList * e) { m_free_list.append_head(e); }
};
//END RecycORList

} //namespace xgen
#endif
