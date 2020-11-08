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
#ifndef _CG_H_
#define _CG_H_

namespace xgen {

class IR2OR;

//Map Symbol Register Id to specifical SR.
typedef xcom::HMap<UINT, SR*> SRegid2SR;

class PRNO2SR : public xcom::HMap<UINT, SR*, xcom::HashFuncBase2<UINT> > {
public:
    PRNO2SR() : xcom::HMap<UINT, SR*, xcom::HashFuncBase2<UINT> >(0) {}
};


class BBVarList : public List<xoc::Var*> {
    List<xoc::Var*> m_free_list;
public:
    xoc::Var * get_free() { return m_free_list.remove_tail(); }

    void free()
    {
        UINT i = 0;
        for (xoc::Var * v = get_head(); i < get_elem_count();
             i++, v = get_next()) {
            m_free_list.append_tail(v);
        }
        clean();
    }
};


class ArgDesc {
public:
    //If is_record_addr is true, src_value records
    //the start address to copy. Otherwise it records the value
    //that to be passed.
    union {
        SR * src_value;
        SR * src_startaddr;
    };

    xoc::Dbx const* arg_dbx;
    UINT is_record_addr:1;
    UINT arg_size:30; //stack byte size to be passed.

    //byte offset to the base SR record in
    //src_startaddr if is_record_addr is true.
    UINT src_ofst;

    //byte offset to SP.
    UINT tgt_ofst;

public:
    ArgDesc() { init(); }

    void init()
    {
        src_value = nullptr;
        src_startaddr = nullptr;
        is_record_addr = false;
        arg_dbx = nullptr;
        arg_size = 0;
        src_ofst = 0;
        tgt_ofst = 0;
    }
};


class ArgDescMgr {
protected:
    Vector<ArgDesc*> m_arg_desc;
    List<ArgDesc*> m_arg_list;
    RegSet m_argregs;

protected:
    List<ArgDesc*> * getArgList() { return &m_arg_list; }

public:
    //A counter that record byte size of argument that have been passed.
    //User should update the value when one argument passed.
    UINT m_passed_arg_in_register_byte_size;

    //A counter that record byte size of argument that have been passed.
    //User should update the value when one argument passed.
    UINT m_passed_arg_in_stack_byte_size;

public:
    ArgDescMgr()
    {
        RegSet const* regs = tmGetRegSetOfArgument();
        if (regs != nullptr && regs->get_elem_count() != 0) {
            m_argregs.copy(*regs);
        }
        m_passed_arg_in_register_byte_size = 0;
        m_passed_arg_in_stack_byte_size = 0;
    }
    ~ArgDescMgr()
    {
        for (INT i = 0; i <= m_arg_desc.get_last_idx(); i++) {
            ArgDesc * desc = m_arg_desc.get(i);
            delete desc;
        }
        m_arg_desc.clean();
    }

    //Add arg-desc to the tail of argument-list.
    ArgDesc * addDesc()
    {
        ArgDesc * desc = new ArgDesc();
        m_arg_desc.set(m_arg_desc.get_last_idx() < 0 ?
            0 : m_arg_desc.get_last_idx() + 1, desc);
        m_arg_list.append_tail(desc);
        return desc;
    }

    ArgDesc * addDesc(bool is_record_addr,
                      SR * value_or_addr,
                      xoc::Dbx const* dbx,
                      UINT align,
                      UINT arg_size,
                      UINT src_ofst)
    {
        ArgDesc * desc = addDesc();
        desc->is_record_addr = is_record_addr;
        if (is_record_addr) {
            desc->src_startaddr = value_or_addr;
        } else {
            desc->src_value = value_or_addr;
        }
        desc->arg_dbx = dbx;
        desc->arg_size = arg_size;
        desc->src_ofst = src_ofst;
        desc->tgt_ofst = (UINT)xcom::ceil_align(
            getArgStartAddrOnStack(), align);
        updatePassedArgInStack(arg_size);
        return desc;
    }

    ArgDesc * addValueDesc(SR * src_value,
                           xoc::Dbx const* dbx,
                           UINT align,
                           UINT arg_size,
                           UINT src_ofst)
    { return addDesc(false, src_value, dbx, align, arg_size, src_ofst); }

    ArgDesc * addAddrDesc(SR * src_addr,
                          xoc::Dbx const* dbx,
                          UINT align,
                          UINT arg_size,
                          UINT src_ofst)
    { return addDesc(true, src_addr, dbx, align, arg_size, src_ofst); }

    //Allocate dedicated argument register.
    SR * allocArgRegister(CG * cg);

    ArgDesc * pullOutDesc() { return m_arg_list.remove_head(); }

    //Abandon the first argument phy-register.
    //The allocation will start at the next register.
    void dropArgRegister();

    //Get the total bytesize of current parameter section.
    UINT getArgSectionSize() const
    {
        return (UINT)xcom::ceil_align(
            getPassedRegisterArgSize() + getPassedStackArgSize(),
            STACK_ALIGNMENT);
    }
    ArgDesc * getCurrentDesc() { return m_arg_list.get_head(); }

    //Return the number of available physical reigsters.
    UINT getNumOfAvailArgReg() const
    { return getArgRegSet()->get_elem_count(); }
    RegSet const* getArgRegSet() const { return &m_argregs; }
    UINT getPassedRegisterArgSize() const
    { return m_passed_arg_in_register_byte_size; }
    UINT getPassedStackArgSize() const
    { return m_passed_arg_in_stack_byte_size; }
    UINT getArgStartAddrOnStack() const { return getPassedStackArgSize(); }

    //This function record the total bytesize that passed through registers.
    //This function should be invoked after passing one argument through
    //register.
    //Decrease tgt_ofst for each desc by the bytesize.
    //e.g: pass 2 argument:
    //  %r0 = arg1;
    //  mgr.updatePassedArgInRegister(GNENERAL_REGISTER_SIZE);
    //  %r1 = arg2;
    //  mgr.updatePassedArgInRegister(arg2.bytesize);
    void updatePassedArgInRegister(UINT bytesize);

    //This function record the total bytesize that passed via stack.
    //This function should be invoked after passing one argument to arg-
    //section. The argument may be passed through register or stack memory.
    //e.g: pass 2 argument:
    //  %r0 = arg1;
    //  mgr.updatePassedArgInRegister(GNENERAL_REGISTER_SIZE);
    //  [sp+0] = arg2;
    //  mgr.updatePassedArgInStack(arg2.bytesize);
    void updatePassedArgInStack(UINT bytesize)
    {
        m_passed_arg_in_stack_byte_size += (INT)xcom::ceil_align(
            bytesize, STACK_ALIGNMENT);
    }
};


//The module will do the follows works:
// Instruction selection,
// Calling conventions lowering,
// Control flow (dominators, loop nesting) analyzes,
// Data flow (liveness, reaching definitions) analyzes,
// Register allocation,
// Stack frame building,
// Peephole optimizations,
// Branch optimizations,
// Loop unrolling,
// Basic block replication,
// Extended block optimizations,
// Instruction scheduling,
// Matching code idioms such as
// DSP arithmetic by target processor instructions,
// If-conversion based on conditional MOVEs, SELECTs,
// or fully predicated instructions.
//
// Taking advantage of specialized addressing modes and of
// hardware looping capabilities.
// Rewriting loops in order to exploit SIMD instructions
// management of register tuples and complex software pipelining
// in case of clustered architectures tricks to reduce code size
// or enhance code compressibility.
#define CG_or2memaddr_map(r) ((r)->m_or2memaddr_map)
#define CG_max_real_arg_size(r) ((r)->m_max_real_arg_size)
#define CG_bb_level_internal_var_list(r) ((r)->m_bb_level_internal_var_list)
#define CG_func_level_internal_var_list(r) ((r)->m_func_level_internal_var_list)
#define CG_builtin_memcpy(r) ((r)->m_builtin_memcpy)
class CG {
    COPY_CONSTRUCTOR(CG);
public:
    friend class OR;
    friend class SR;
protected:
    class SimVec : public xcom::Vector<BBSimulator*> {
    public:
        ~SimVec() 
        {
            for (INT i = 0; i <= get_last_idx(); i++) {
                BBSimulator * sim = get(i);
                if (sim != nullptr) {
                    delete sim;
                }
            }
        }
    };

    xoc::Region * m_rg;
    ORMgr * m_or_mgr;
    xoc::TypeMgr * m_tm;
    UnitSet m_tmp_us; //Used for temporary purpose.
    CGMgr * m_cgmgr;
    //Mapping from STORE/LOAD operation to the target address.
    List<ORBB*> m_or_bb_list; //descripting all basic blocks of the region.
    OR_CFG * m_or_cfg; //CFG of region
    Vector<ParallelPartMgr*> * m_ppm_vec; //Record parallel part for CG.
    Vector<xoc::Var const*> m_params; //record the formal parameter.
    ORBBMgr * m_or_bb_mgr; //manage BB of IR.
    UINT m_or_bb_idx; //take count of ORBB.
    RegSetMgr m_regset_mgr;
    SRegid2SR m_regid2sr_map; //Map Symbol Register Id to specifical SR.
    PRNO2SR m_pr2sr_map;
    UINT m_sect_count;
    UINT m_reg_count;
    SMemPool * m_pool;
    SR * m_param_pointer;
    Reg2SR m_dedicate_sr_tab;
    Section m_code_sect;
    Section m_data_sect;
    Section m_rodata_sect;
    Section m_bss_sect;
    StackSection m_stack_sect;
    Section m_param_sect;
    INT m_param_sect_start_offset; //record the parameter start offset.
    UINT m_mmd_count;
    IssuePackageListVector m_ipl_vec; //record IssuePackageList for each ORBB
    IssuePackageMgr m_ip_mgr;

    //True if accessing local variable via [FP pointer - Offst].
    bool m_is_use_fp;

    //True if compute the section offset for global/local variable.
    bool m_is_compute_sect_offset;

    //Record the max bytesize for callee argument section.
    UINT m_max_real_arg_size;

protected:
    void addSerialDependence(ORBB * bb, DataDepGraph * ddg);

    UINT compute_pad();
    SR * computeAndUpdateOffset(SR * sr);
    UINT calcSizeOfParameterPassedViaRegister(
        List<Var const*> const* param_lst) const;

    SMemPool * get_pool() const { return m_pool; }

    void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        //return MEM_POOL_Alloc(&m_mempool, size);
        ASSERTN(m_pool != nullptr, ("need graph pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        if (p == nullptr) return nullptr;
        ::memset(p, 0, size);
        return p;
    }

    void initSections(xoc::VarMgr * vm);

    virtual void finiFuncUnit();

    void localizeBB(SR * sr, ORBB * bb);
    void localizeBBTab(SR * sr, xcom::TTab<ORBB*> * orbbtab);

    //Destroy useless resource.
    void postLS(LIS * lis, DataDepGraph * ddg);
    void preLS(IN ORBB * bb,
               IN RaMgr * ra_mgr,
               OUT DataDepGraph ** ddg,
               OUT BBSimulator ** sim,
               OUT LIS ** lis);

    CG * self() { return this; }
public:
    //Mapping from STORE/LOAD operation to the target address.
    xcom::TMap<OR*, xoc::Var const*> m_or2memaddr_map;

    BBVarList m_bb_level_internal_var_list; //record local pr at OR.
    List<xoc::Var*> m_func_level_internal_var_list; //record global pr at OR.

    //Builtin function should be initialized in initBuiltin().
    xoc::Var const* m_builtin_memcpy;

public:
    CG(xoc::Region * rg, CGMgr * cgmgr);
    virtual ~CG();

    void addBBLevelNewVar(IN xoc::Var * var);
    void addFuncLevelNewVar(IN xoc::Var * var);
    void appendReload(ORBB * bb, ORList & ors);
    inline xoc::Var * addBuiltinVar(CHAR const* buildin_name)
    {
        ASSERT0(m_rg);
        xoc::Sym * s = m_rg->getRegionMgr()->addToSymbolTab(buildin_name);
        return m_rg->getVarMgr()->registerStringVar(buildin_name,
                                                    s, MEMORY_ALIGNMENT);
    }
    ORBB * allocBB();
    RegSet * allocRegSet() { return m_regset_mgr.allocRegSet(); }
    virtual BBSimulator * allocBBSimulator(ORBB * bb);
    virtual LIS * allocLIS(ORBB * bb, DataDepGraph * ddg,
                           BBSimulator * sim, UINT sch_mode);
    virtual DataDepGraph * allocDDG();
    void assembleSRVec(SRVec * srvec, SR * sr1, SR * sr2);
    virtual IR2OR * allocIR2OR() = 0;
    virtual OR_CFG * allocORCFG();
    virtual IssuePackageMgr * allocIssuePackageMgr();
    virtual RaMgr * allocRaMgr(List<ORBB*> * bblist, bool is_func);

    //OR Builder
    //Build pseduo instruction that indicate LabelInfo.
    //Note OR_label must be supplied by Target.
    void buildLabel(xoc::LabelInfo const* li, OUT ORList & ors, IN IOC * cont);

    //Build nop instruction.
    virtual OR * buildNop(UNIT unit, CLUST clust) = 0;
    virtual OR * buildOR(OR_TYPE orty, UINT resnum, UINT opndnum, ...);
    virtual OR * buildOR(OR_TYPE orty, IN SRList & result, IN SRList & opnd);
    virtual void buildSpadjust(OUT ORList & ors, IN IOC * cont);
    virtual void buildSpill(IN SR * store_val,
                            IN xoc::Var * spill_loc,
                            IN ORBB * bb,
                            OUT ORList & ors);
    virtual void buildReload(IN SR * result_val,
                             IN xoc::Var * reload_loc,
                             IN ORBB * bb,
                             OUT ORList & ors);

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildMul(SR * src1,
                          SR * src2,
                          UINT sr_size,
                          bool is_sign,
                          OUT ORList &,
                          IN IOC *)
    {
        DUMMYUSE(is_sign);
        DUMMYUSE(src1);
        DUMMYUSE(src2);
        DUMMYUSE(sr_size);
        ASSERTN(0, ("Target Dependent Code"));
    }

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildDiv(SR * src1,
                          SR * src2,
                          UINT sr_size,
                          bool is_sign,
                          OUT ORList &,
                          IN IOC *)
    {
        DUMMYUSE(is_sign);
        DUMMYUSE(src1);
        DUMMYUSE(src2);
        DUMMYUSE(sr_size);
        ASSERTN(0, ("Target Dependent Code"));
    }

    //This function build OR according to given 'code'.
    //Implement the target dependent version if needed.
    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildBinaryOR(IR_TYPE code,
                               SR * src1,
                               SR * src2,
                               bool is_signed,
                               OUT ORList & ors,
                               IN IOC * cont);

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildAdd(SR * src1,
                          SR * src2,
                          UINT sr_size,
                          bool is_sign,
                          OUT ORList & ors,
                          IN IOC * cont);

    //'sr_size': The number of integral substract of byte-size of single SR.
    virtual void buildSub(SR * src1,
                          SR * src2,
                          UINT sr_size,
                          bool is_sign,
                          OUT ORList & ors,
                          IN IOC * cont);
    virtual void buildAddRegImm(SR * src,
                                SR * imm,
                                UINT sr_size,
                                bool is_sign,
                                OUT ORList & ors,
                                IN IOC * cont) = 0;
    virtual void buildAddRegReg(bool is_add,
                                SR * src1,
                                SR * src2,
                                UINT sr_size,
                                bool is_sign,
                                OUT ORList & ors,
                                IN IOC * cont) = 0;
    virtual void buildMod(CLUST clust,
                          SR ** result,
                          SR * src1,
                          SR * src2,
                          UINT sr_size,
                          bool is_sign,
                          OUT ORList & ors,
                          IN IOC * cont);

    //Build copy-operation with given predicate register.
    virtual void buildCopyPred(CLUST clust,
                               UNIT unit,
                               IN SR * to, //should not add 'const' qualifier
                                           //because RA will change the value.
                               IN SR * from,
                               IN SR * pd,
                               OUT ORList & ors) = 0;

    //Build function call instruction.
    virtual void buildCall(xoc::Var const* callee,
                           UINT ret_val_size,
                           OUT ORList & ors,
                           IOC * cont) = 0;

    //Build indirect function call instruction.
    //Function-Call may violate SP,FP,GP,
    //RFLAG register, return-value register,
    //return address register.
    virtual void buildICall(SR * callee,
                            UINT ret_val_size,
                            OUT ORList & ors,
                            IOC * cont) = 0;

    //Build load-address instruction.
    virtual void buildStore(SR * store_val,
                            xoc::Var const* base,
                            HOST_INT ofst,
                            OUT ORList & ors,
                            IN IOC * cont);
    virtual void buildLoad(SR * load_val,
                           xoc::Var const* base,
                           HOST_INT ofst,
                           OUT ORList & ors,
                           IN IOC * cont)
    {
        ASSERT0(SR_is_reg(load_val));
        SR * mem_base = genVAR(base);
        buildLoad(load_val, mem_base, genIntImm(ofst, true), ors, cont);
    }
    virtual void buildGeneralLoad(IN SR * val,
                                  HOST_INT ofst,
                                  OUT ORList & ors,
                                  IN IOC * cont);
    virtual void buildTypeCvt(IR const* tgt,
                              IR const* src,
                              OUT ORList & ors,
                              IN OUT IOC * cont);

    //Implement memory block copy.
    //Note tgt and src should not overlap.
    virtual void buildMemcpyInternal(SR * tgt,
                                     SR * src,
                                     UINT bytesize,
                                     OUT ORList & ors,
                                     IN IOC * cont) = 0;
    //Generate ::memcpy.
    void buildMemcpy(SR * tgt,
                     SR * src,
                     UINT bytesize,
                     OUT ORList & ors,
                     IN IOC * cont);

    //Generate operations: reg = &var
    virtual void buildLda(xoc::Var const* var,
                          HOST_INT lda_ofst,
                          Dbx const* dbx,
                          OUT ORList & ors,
                          IN IOC * cont);

    //Generate opcode of accumulating operation.
    //Form as: A = A op B
    virtual void buildAccumulate(OR * red_or,
                                 SR * red_var,
                                 SR * restore_val,
                                 List<OR*> & ors);
    virtual void buildLoad(IN SR * load_val,
                           IN SR * base,
                           IN SR * ofst,
                           OUT ORList & ors,
                           IN IOC * cont) = 0;
    virtual void buildStore(SR * store_val,
                            SR * base,
                            SR * ofst,
                            OUT ORList & ors,
                            IN IOC * cont) = 0;

    //Build a general copy operation from register to register.
    //to: source register
    //from: target register
    //unit: function unit.
    //clust: cluster.
    virtual void buildCopy(CLUST clust,
                           UNIT unit,
                           SR * to,
                           SR * from,
                           OUT ORList & ors) = 0;

    //Generate copy operation from source to register.
    //Source can be immediate or register, and target must be register.
    //Note there is no difference between signed and unsigned number moving.
    virtual void buildMove(IN SR * to,
                           IN SR * from,
                           OUT ORList & ors,
                           IN IOC * cont) = 0;
    virtual void buildUncondBr(IN SR * tgt_lab,
                               OUT ORList & ors,
                               IN IOC * cont) = 0;
    virtual void buildCondBr(IN SR * tgt_lab,
                             OUT ORList & ors,
                             IN IOC * cont) = 0;
    virtual void buildCompare(OR_TYPE br_cond,
                              bool is_truebr,
                              IN SR * opnd0,
                              IN SR * opnd1,
                              OUT ORList & ors,
                              IN IOC * cont) = 0;

    //Generate inter-cluster copy operation.
    //Copy from 'src' in 'src_clust' to 'tgt' of in 'tgt_clust'.
    virtual OR * buildBusCopy(IN SR * src,
                              IN SR * tgt,
                              IN SR * pd,
                              CLUST src_clust,
                              CLUST tgt_clust) = 0;
    virtual void buildShiftLeft(IN SR * src,
                                ULONG sr_size,
                                IN SR * shift_ofst,
                                OUT ORList & ors,
                                IN OUT IOC * cont) = 0;
    virtual void buildShiftRight(IN SR * src,
                                 ULONG sr_size,
                                 IN SR * shift_ofst,
                                 bool is_signed,
                                 OUT ORList & ors,
                                 IN OUT IOC * cont) = 0;
    //Build memory store operation that store 'reg' into stack.
    //NOTE: user have to assign physical register manually if there is
    //new OR generated and need register allocation.
    //reg: register to be stored.
    //offset: bytesize offset related to SP.
    //ors: record output.
    //cont: context.
    virtual void buildStoreAndAssignRegister(SR * reg,
                                             UINT offset,
                                             OUT ORList & ors,
                                             IN IOC * cont);
    //Increase 'reg' by 'val'.
    virtual void buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);
    //Decrease 'reg' by 'val'.
    virtual void buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);

    void constructORBBList(IN ORList  & or_list);
    void computeEntryAndExit(IN OR_CFG & cfg,
                             OUT List<ORBB*> & entry_lst,
                             OUT List<ORBB*> & exit_lst);
    INT computeOpndIdx(OR * o, SR const* opnd);
    INT computeResultIdx(OR * o, SR const* res);
    void computeMaxRealParamSpace();
    virtual void computeAndUpdateGlobalVarLayout(xoc::Var const* var,
                                                 OUT SR ** base,
                                                 OUT SR ** base_ofst);
    virtual void computeAndUpdateStackVarLayout(xoc::Var const* var,
                                                OUT SR ** sp, //stack pointer
                                                OUT SR ** sp_ofst);
    virtual void computeParamLayout(xoc::Var const* id,
                                    OUT SR ** base,
                                    OUT SR ** ofst);
    virtual UINT computeTotalParameterStackSize(IR * ir);
    virtual void computeVarBaseOffset(xoc::Var const* var,
                                      ULONGLONG var_ofst,
                                      OUT SR ** base,
                                      OUT SR ** ofst);
    virtual CLUST computeAsmCluster(OR * o);

    //Return the index of copied source operand.
    virtual INT computeCopyOpndIdx(OR *)
    {
        ASSERTN(0, ("Target Dependent Code")); return -1;
    }

    //Compute the byte size of memory which will be loaded/stored.
    virtual INT computeMemByteSize(OR * o)
    {
        CHECK_DUMMYUSE(o);
        ASSERTN(o->is_mem(), ("Need memory operation"));
        ASSERTN(0, ("Target Dependent Code"));
        return -1;
    }

    //Return the alternative equivalent o-type of 'from'
    //that correspond with 'to_unit' and 'to_clust'.
    virtual OR_TYPE computeEquivalentORType(OR_TYPE from,
                                            UNIT to_unit,
                                            CLUST to_clust)
    {
        DUMMYUSE(to_clust);
        ASSERTN(tmGetEqualORType(from), ("miss EquORTypes information"));
        return EQUORTY_unit2ortype(tmGetEqualORType(from), to_unit);
    }

    //Return the alternative equivalent o-type of 'o'
    //that correspond with 'to_unit' and 'to_clust'.
    virtual UNIT computeEquivalentORUnit(IN OR * from, CLUST to_clust)
    {
        DUMMYUSE(from);
        DUMMYUSE(to_clust);
        ASSERTN(0, ("Target Dependent Code"));
        return UNIT_UNDEF;
    }

    virtual CLUST computeClusterOfBusOR(OR * o);
    virtual CLUST computeORCluster(OR const* o) const;
    virtual SLOT computeORSlot(OR const* o);

    //Compute the stack alignment for argument with given bytesize.
    //This alignment will affect the argument's byte offset to SP.
    //Return arguments alignment.
    //e.g:If argsz is less than 4 bytes, the alignment will be 4; if argsz is
    //    more than 8 bytes, the alignments will be 8.
    virtual UINT computeArgAlign(UINT argsz) const
    {
        DUMMYUSE(argsz);
        ASSERTN(0, ("Target Dependent Code"));
        return 0;
    }
    xoc::Var const* computeSpillVar(OR const* o) const;

    //Change the function unit and related cluster of 'o'.
    //If is_test is true, this function only check whether the given
    //OR can be changed.
    virtual bool changeORUnit(OR * o,
                              UNIT to_unit,
                              CLUST to_clust,
                              RegFileSet const* regfile_unique,
                              bool is_test /*only test purpose*/);

    //Return the combination of all of available function unit of 'o'.
    //This function will modify m_tmp_us internally.
    virtual UnitSet const* computeORUnit(OR const*, OUT UnitSet*);
    virtual UnitSet const* computeORUnit(OR const* o)
    {
        return computeORUnit(o, nullptr);
    }

    //Change the correlated cluster of 'o'
    //If is_test is true, this function only check whether the given
    //OR can be changed.
    //is_test: true to query whether need to change OR's cluster.
    virtual bool changeORCluster(OR * o,
                                 CLUST to_clust,
                                 RegFileSet const* regfile_unique,
                                 bool is_test);

    //Change 'o' to 'ot', modifing all operands and results.
    virtual bool changeORType(OR * o,
                              OR_TYPE ot,
                              CLUST src,
                              CLUST tgt,
                              RegFileSet const* regfile_unique);

    virtual SR * dupSR(SR const* sr);
    virtual OR * dupOR(OR const* o);
    virtual void dumpSection();
    void dumpPackage();
    void dumpORBBList() const;

    //Expand pseudo SpAdjust operation to real target AddInteger instruction.
    //Note this function is target dependent.
    virtual void expandSpadjust(OR * o, OUT IssuePackageList * ipl);

    //Expand pseudo operation to real target AddInteger instruction.
    //Note this function is target dependent.
    virtual void expandFakeOR(OR * o, OUT IssuePackageList * ipl);

    //Format label name string in 'buf'.
    CHAR * formatLabelName(xoc::LabelInfo const* lab, OUT xcom::StrBuf & buf)
    {
        CHAR const* prefix = nullptr;
        prefix = SYM_name(m_rg->getRegionVar()->get_name());
        buf.strcat("%s_", prefix);
        if (LABELINFO_type(lab) == L_ILABEL) {
            buf.strcat(ILABEL_STR_FORMAT, ILABEL_CONT(lab));
        }
        else if (LABELINFO_type(lab) == L_CLABEL) {
            buf.strcat(CLABEL_STR_FORMAT, CLABEL_CONT(lab));
        }
        else {
            ASSERTN(0, ("unknown label type"));
        }
        return buf.buf;
    }
    void freeORBBList();
    virtual void freePackage();
    virtual void fixCluster(ORList & spill_ops, CLUST clust);

    Vector<ParallelPartMgr*> * getParallelPartMgrVec() const
    { return m_ppm_vec; }

    SR * genVAR(xoc::Var const* var);
    SR * genLabel(LabelInfo const* li);
    SR * genSR();
    SR * genSR(REG reg, REGFILE regfile);

    //Generate a SR if bytes_size not more than GENERAL_REGISTER_SIZE,
    //otherwise generate a vector or SR.
    //Return the first register if vector generated.
    SR * genReg(UINT bytes_size = GENERAL_REGISTER_SIZE);

    //Generate SR by specified Symbol Register Id.
    SR * genReg(SymRegId regid);

    //Generate SR that indicates const value.
    SR * genIntImm(HOST_INT val, bool is_signed);
    SR * genFpImm(HOST_FP val);
    SR * genZero() { return genIntImm((HOST_INT)0, false); }
    SR * genOne() { return genIntImm((HOST_INT)1, false); }

    //Generate dedicated register by specified physical register.
    SR * genDedicatedReg(REG phy_reg);
    //Get dedicated register by specified physical register.
    SR * getDedicatedReg(REG phy_reg) const;    
    virtual SR * getSP() const;
    virtual SR * getFP() const;
    virtual SR * getGP() const;
    virtual SR * getParamPointer() const;
    virtual SR * getRflag() const; //Get flag register.
    virtual SR * getTruePred() const; //Get TRUE predicate register.
    //Get function return-address register.
    virtual SR * getReturnAddr() const = 0;
    virtual SR * genSP();
    virtual SR * genFP();
    virtual SR * genGP();
    virtual SR * genParamPointer();
    virtual SR * genRflag(); //Generate flag register.
    virtual SR * genPredReg(); //Generate predicate register.
    virtual SR * genTruePred(); //Generate TRUE predicate register.
    //Generate function return-address register.
    virtual SR * genReturnAddr() = 0;
    xoc::Region * getRegion() const { return m_rg; }

    //Generate spill location that same like 'sr'.
    //Or return the spill location if exist.
    //'sr': the referrence SR.
    xoc::Var * genSpillVar(SR * sr);
    void generateFuncUnitDedicatedCode();
    OR_CFG * getORCfg() const { return m_or_cfg; }
    xoc::TypeMgr * getTypeMgr() const { return m_tm; }
    List<ORBB*> * getORBBList() { return &m_or_bb_list; }

    //Construct a name for Var that will lived in a ORBB.
    CHAR const* genBBLevelNewVarName(OUT xcom::StrBuf & name);

    //Construct a name for Var that will lived in Region.
    CHAR const* genFuncLevelNewVarName(OUT xcom::StrBuf & name);
    UINT getMaxArgSectionSize() const { return CG_max_real_arg_size(this); }
    xoc::Var * genTempVar(xoc::Type const* type, UINT align, bool func_level);
    OR * getOR(UINT id);
    OR * genOR(OR_TYPE ort) { return m_cgmgr->getORMgr()->genOR(ort, this); }
    Section * getRodataSection() { return &m_rodata_sect; }
    Section * getCodeSection() { return &m_code_sect; }
    Section * getDataSection() { return &m_data_sect; }
    Section * getBssSection() { return &m_bss_sect; }
    Section * getStackSection() { return &m_stack_sect; }
    Section * getParamSection() { return &m_param_sect; }
    virtual REGFILE getRflagRegfile() const = 0;
    virtual REGFILE getPredicateRegfile() const;
    Vector<xoc::Var const*> const& get_param_vars() const { return m_params; }
    virtual ORAsmInfo * getAsmInfo(OR const*) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        return nullptr;
    }

    virtual RegFileSet const* getValidRegfileSet(OR_TYPE ortype,
                                                 UINT idx,
                                                 bool is_result) const;
    virtual RegSet const* getValidRegSet(OR_TYPE ortype,
                                         UINT idx,
                                         bool is_result) const;
    SRMgr * getSRMgr() { return m_cgmgr->getSRMgr(); }
    ORMgr * getORMgr() { return m_cgmgr->getORMgr(); }
    SRVecMgr * getSRVecMgr() { return m_cgmgr->getSRVecMgr(); }
    IssuePackageListVector * getIssuePackageListVec() { return &m_ipl_vec; }
    IssuePackageMgr * getIssuePackageMgr() { return &m_ip_mgr; }

    //Map phsical register to dedicated symbol register if exist.
    SR * getDedicatedSRForPhyReg(REG reg);

    //Generate and return the mask code that used to reserved the lower
    //'bytesize' bytes.
    static HOST_UINT getMaskByByte(UINT bytesize);

    virtual void initFuncUnit();
    virtual void initBuiltin();
    //Generate dedicated SR for subsequent use.
    virtual void initDedicatedSR();    
    virtual bool isPassArgumentThroughRegister() = 0;
    void initGlobalVar(VarMgr * vm);
    bool isComputeStackOffset() const { return m_is_compute_sect_offset; }
    bool isAlloca(IR const* ir) const;
    bool isGRAEnable() const;
    bool isEvenReg(REG reg) const
    {
        //register start from '1'. And '0' denotes memory.
        return (reg % 2) == 1;
    }
    //Return true if ORBB needs to be assigned cluster.
    bool isAssignClust(ORBB const*) const { return true; }
    bool isRegflowDep(OR const* from, OR const* to) const;
    bool isRegoutDep(OR const* from, OR const* to) const;
    
    //Return true if 'test_sr' is the one of operands of 'o' ,
    //it is also the results.
    //'test_sr': can be nullptr. If it is nullptr, we only try to
    //           get the index-info of the same opnd and result.
    bool isOpndSameWithResult(SR const* test_sr,
                              OR const* o,
                              OUT INT * opndnum,
                              OUT INT * resnum) const;
    //Return true if specified operand or result is Rflag register.
    bool isRflagRegister(OR_TYPE ot, UINT idx, bool is_result) const;
    virtual bool isReductionOR(OR const* o) const;

    //Return true if specified immediate operand is in valid range.
    bool isValidImmOpnd(OR_TYPE ot, UINT idx, HOST_INT imm) const;
    bool isValidImmOpnd(OR_TYPE ot, HOST_INT imm) const;
    bool isValidImmOpnd(OR const* o) const;
    //Return true if specified immediate in
    //valid range that described with bitsize.
    bool isValidImm(UINT bitsize, HOST_INT imm) const;

    //Return true if regfile can be assigned to referred operand.
    virtual bool isValidRegFile(OR_TYPE ot,
                                INT opndnum,
                                REGFILE regfile,
                                bool is_result) const;

    //Return true if regfile can be assigned to referred operand.
    virtual bool isValidRegFile(OR * o,
                                SR const* opnd,
                                REGFILE regfile,
                                bool is_result) const;
    virtual bool isConsistentRegFileForCopy(REGFILE rf1, REGFILE rf2);

    //If stack-base-pointer register could use 'unit', return true.
    virtual bool isSPUnit(UNIT unit) const
    {
        DUMMYUSE(unit);
        ASSERTN(0, ("Target Dependent Code"));
        return false;
    }

    //Which case is safe to optimize?
    //If 'prev' is must-execute, 'next' is cond-execute, we say that is
    //safe to optimize.
    //Since must-execute operation is the dominator in execute-path.
    //Otherwise is not absolutely safe.
    virtual bool isSafeToOptimize(OR const* prev, OR const* next) const;
    bool isReturnValueSR(SR const* sr) const
    {
        return sr->getPhyReg() != REG_UNDEF &&
               tmGetRegSetOfReturnValue()->is_contain(sr->getPhyReg());
    }
    bool isArgumentSR(SR const* sr) const
    {
        return sr->getPhyReg() != REG_UNDEF &&
               tmGetRegSetOfArgument()->is_contain(sr->getPhyReg());
    }
    bool isDedicatedSR(SR const* sr) const
    { return SR_is_dedicated(sr) || isReturnValueSR(sr); }
    bool isSREqual(SR const* sr1, SR const* sr2) const;

    //Return true if SR is integer register.
    virtual bool isIntRegSR(OR const*, SR const*,
                            UINT idx, bool is_result) const
    {
        DUMMYUSE(idx);
        DUMMYUSE(is_result);
        ASSERTN(0, ("Target Dependent Code"));
        return false;
    }

    virtual bool isCompareOR(OR const* o) const;
    virtual bool isCondExecOR(OR const* o) const;
    virtual bool isBusCluster(CLUST) const = 0;

    //SR that can used on each clusters.
    virtual bool isBusSR(SR const*) const = 0;

    virtual bool isCopyOR(OR const*) const = 0;

    //Return true if sr is stack base pointer.
    virtual bool isSP(SR const*) const = 0;

    //Return true if the results of 'o' are independent with other ops, and
    //all results can be recalculated any where.
    //We can sum up some typical simply expressoin to rematerialize as followed:
    //1. load constant
    //2. frame/stack pointer +/- constant
    //   (only use frame/stack register as operand)
    //3. load constant parameter
    //4. load from local data memory for simple
    //5. load address of variable
    virtual bool isRecalcOR(OR const*) const = 0;

    bool isSameSpillLoc(OR const* or1, OR const* or2);
    bool isSameSpillLoc(xoc::Var const* or1loc, OR const* or1, OR const* or2);
    virtual bool isReduction(OR const* o) const;
    virtual bool isSameCondExec(OR * prev, OR * next, BBORList const* or_list);

    //Return true if both slot1 and slot2 belong to same cluster.
    virtual bool isSameCluster(OR const* or1, OR const* or2) const;

    //Compare if the first cluster is same with the second cluster.
    virtual bool isSameCluster(SLOT, SLOT) const = 0;

    //Return true if slot1 and slot2 use similar func-unit.
    virtual bool isSameLikeCluster(SLOT, SLOT) const = 0;

    //Return true if the data BUS operation processed that was
    //using by other general operations.
    virtual bool isSameLikeCluster(OR const*, OR const*) const = 0;

    //Return true if or-type has the number of 'res_num' results.
    virtual bool isMultiResultOR(OR_TYPE, UINT res_num) const = 0;

    //Return true if or-type has multiple results.
    virtual bool isMultiResultOR(OR_TYPE) const = 0;

    //'opnd_num': -1 means return true if or-type has multiple storre-val.
    virtual bool isMultiStore(OR_TYPE, INT opnd_num) const = 0;

    //'res_num': -1 means return true if ot has multiple result SR.
    //    non -1 means return true if ot has the number of 'res_num' results.
    virtual bool isMultiLoad(OR_TYPE, INT res_num) const = 0;

    virtual bool isValidOpndRegfile(OR_TYPE opcode,
                                    INT opndnum,
                                    REGFILE regfile) const;
    virtual bool isValidResultRegfile(OR_TYPE opcode,
                                      INT resnum,
                                      REGFILE regfile) const;
    virtual bool isValidRegInSRVec(OR  const* o, SR  const* sr,
                                   UINT idx, bool is_result) const;

    //Return true if the runtime value of base1 is equal to base2.
    //Some target might use mulitple stack pointers.
    virtual bool isStackPointerValueEqu(SR const* base1, SR const* base2) const
    {
        DUMMYUSE(base1);
        DUMMYUSE(base2);
        ASSERTN(0, ("Target Dependent Code"));
        return false;
    }

    virtual OR_TYPE invertORType(OR_TYPE)
    {
        ASSERTN(0, ("Target Dependent Code"));
        return OR_UNDEF;
    }

    //Map from IR type to OR type.
    //Target may apply comparing instruction to calculate boolean value.
    //e.g:
    //     r1 <- 0x1,
    //     r2 <- 0x2,
    //     r0 <- eq, r1, r2 ;then r0 is 0
    virtual OR_TYPE mapIRType2ORType(IR_TYPE ir_type,
                                     UINT ir_opnd_size,
                                     IN SR * opnd0,
                                     IN SR * opnd1,
                                     bool is_signed) = 0;
    SR * mapPR2SR(UINT prno) const
    { return const_cast<CG*>(this)->m_pr2sr_map.get(prno); }
    SR * mapSymbolReg2SR(UINT regid) const
    { return const_cast<CG*>(this)->m_regid2sr_map.get(regid); }
    bool mustAsmDef(OR const* o, SR const* sr) const;
    bool mustDef(OR const* o, SR const* sr) const;
    bool mustUse(OR const* o, SR const* sr) const;
    bool mayDefInRange(SR const* sr,
                       IN ORCt * start,
                       IN ORCt * end,
                       IN ORList & ors);
    bool mayDef(OR const* o, SR const* sr) const;
    bool mayUse(OR const* o, SR const* sr) const;
    virtual xoc::Var const* mapOR2Var(OR const* o) const;
    virtual bool mapRegSet2RegFile(OUT Vector<INT> & regfilev,
                                   RegSet const* regs);
    virtual UNIT mapSR2Unit(OR const* o, SR const* sr) const;
    virtual CLUST mapSlot2Cluster(SLOT slot);

    //Return the regisiter-file set which 'clust' corresponded with.
    virtual List<REGFILE> & mapCluster2RegFileList(
        CLUST clust,
        OUT List<REGFILE> & regfiles) const
    {
        DUMMYUSE(clust);
        ASSERTN(0, ("Target Dependent Code"));
        return regfiles;
    }

    //Return register-file set which the unit set corresponded with.
    //'units': function unit set
    virtual List<REGFILE> & mapUnitSet2RegFileList(
        UnitSet const& us,
        OUT List<REGFILE> & regfiles) const
    {
        DUMMYUSE(us);
        DUMMYUSE(regfiles);
        ASSERTN(0, ("Target Dependent Code"));
        regfiles.clean();
        return regfiles;
    }

    //Mapping from single unit to its corresponed cluster.
    virtual SLOT mapUnit2Slot(UNIT unit, CLUST clst) const
    {
        DUMMYUSE(unit);
        DUMMYUSE(clst);
        ASSERTN(0, ("Target Dependent Code"));
        return FIRST_SLOT;
    }

    //Mapping from single issue slot(for multi-issue architecture) to
    //its corresponed function unit.
    virtual UNIT mapSlot2Unit(SLOT slot) const
    {
        DUMMYUSE(slot);
        ASSERTN(0, ("Target Dependent Code"));
        return UNIT_UNDEF;
    }

    //Return the cluster which owns 'regfile'.
    virtual CLUST mapRegFile2Cluster(REGFILE regfile, SR const* sr) const
    {
        DUMMYUSE(regfile);
        DUMMYUSE(sr);
        ASSERTN(0, ("Target Dependent Code"));
        CLUST clust = CLUST_UNDEF;
        return clust;
    }

    //Return the cluster which owns 'reg'
    virtual CLUST mapReg2Cluster(REG) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        CLUST clust = CLUST_UNDEF;
        return clust;
    }


    //Return the function unit which can operate on 'regfile'.
    virtual UnitSet & mapRegFile2UnitSet(REGFILE regfile,
                                         SR const* sr,
                                         OUT UnitSet & us) const
    {
        DUMMYUSE(regfile);
        DUMMYUSE(sr);
        DUMMYUSE(us);
        ASSERTN(0, ("Target Dependent Code"));
        return us;
    }

    //Return the cluster which owns 'sr'
    virtual CLUST mapSR2Cluster(OR const*, SR const*) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        return CLUST_UNDEF;
    }

    void relocateStackVarOffset();
    void renameResult(OR * o,
                      SR * oldsr,
                      SR * newsr,
                      bool match_phy_reg);
    void renameOpnd(OR * o,
                    SR * oldsr,
                    SR * newsr,
                    bool match_phy_reg);
    void renameOpndAndResultFollowed(SR * oldsr,
                                     SR * newsr,
                                     ORCt * start,
                                     BBORList * ors);
    void renameOpndAndResultFollowed(SR * oldsr,
                                     SR * newsr,
                                     OR * start,
                                     BBORList * ors);
    void renameOpndAndResultInRange(SR * oldsr,
                                    SR * newsr,
                                    ORCt * start,
                                    ORCt * end,
                                    BBORList * orlist);
    void renameOpndAndResultInRange(SR * oldsr,
                                    SR * newsr,
                                    OR * start,
                                    OR * end,
                                    BBORList * orlist);
    virtual void reviseFormalParameterAndSpadjust();
    virtual void reviseFormalParamAccess(UINT lv_size);

    void storeArgToStack(ArgDescMgr * argdesc,
                         OUT ORList & ors,
                         IN IOC *);
    void setMapPR2SR(UINT prno, SR * sr) { m_pr2sr_map.set(prno, sr); }
    void setMapSymbolReg2SR(UINT regid, SR * sr)
    {
        DUMMYUSE(regid);
        m_regid2sr_map.set(SR_sregid(sr), sr);
    }

    void setCluster(ORList & ors, CLUST clust);
    void setComputeSectOffset(bool doit) { m_is_compute_sect_offset = doit; }
    void storeRegisterParameterBackToStack(List<ORBB*> * entry_lst,
                                           UINT param_start);

    //True if current argument register should be bypassed.
    virtual bool skipArgRegister(Var const* param,
                                 RegSet const* regset,
                                 REG reg) const
    {
        DUMMYUSE(param && regset && reg);
        ASSERTN(0, ("Target Dependent Code")); return false;
    }
    bool skipArgRegister(UINT sz, OUT ArgDescMgr * argdescmgr);

    virtual void package(Vector<BBSimulator*> & simvec);

    virtual void setORListWithSamePredicate(ORList & ops, OR * o);
    virtual void setSpadjustOffset(OR * spadj, INT size);
    virtual void setMapOR2Mem(OR * o, xoc::Var const* mid_opt);

    void localize();

    bool verifyPackageList();
    bool verifyOR(OR const* o);

    void updateMaxCalleeArgSize(UINT maxsize)
    {
        ASSERT0(maxsize < MAX_STACK_SPACE);
        UINT sz = CG_max_real_arg_size(this);
        sz = MAX(sz, maxsize);
        sz = MAX(sz, (UINT)xcom::ceil_align(sz, STACK_ALIGNMENT));
        CG_max_real_arg_size(this) = sz;
    }

    void prependSpill(ORBB * bb, ORList & ors);

    //Perform Instruction Scheduling.
    virtual void performIS(OUT Vector<BBSimulator*> & simvec,
                           RaMgr * ra_mgr);

    //Perform Register Allocation.
    virtual RaMgr * performRA();

    //Generate target dependent information.
    bool perform();

    void passArgVariant(ArgDescMgr * argdescmgr,
                        OUT ORList & ors,
                        UINT num,
                        ...);

    //This function try to pass all arguments through registers.
    //Otherwise pass remaingin arguments through stack memory.
    //'ir': the first parameter of CALL.
    void passArg(SR * argval,
                 SR * argaddr,
                 UINT argsz,
                 OUT ArgDescMgr * argdescmgr,
                 OUT ORList & ors,
                 IN IOC * cont);
    bool tryPassArgThroughRegister(SR * regval,
                                   SR * start_addr,
                                   UINT * argsz,
                                   IN OUT ArgDescMgr * argdescmgr,
                                   OUT ORList & ors,
                                   IOC * cont);
    bool passArgInRegister(SR * argval,
                           UINT * sz,
                           ArgDescMgr * argdescmgr,
                           ORList & ors,
                           IOC const* cont);
    bool passArgInMemory(SR * argaddr,
                         UINT * argsz,
                         OUT ArgDescMgr * argdescmgr,
                         OUT ORList & ors,
                         IOC * cont);
    void flattenInVec(SR * argval, Vector<SR*> * vec);
};

} //namespace xgen
#endif
