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
#ifndef _CG_H_
#define _CG_H_

namespace xgen {

class OR;
class SR;
class IR2OR;
class ORAsmInfo;

//Map Symbol Register Id to specifical SR.
typedef xcom::HMap<UINT, SR*> SRegid2SR;

class PRNO2SR : public xcom::HMap<PRNO, SR*, xcom::HashFuncBase2<PRNO> > {
public:
    PRNO2SR() : xcom::HMap<PRNO, SR*, xcom::HashFuncBase2<PRNO> >(0) {}
};


typedef xcom::C<Var*> * BBVarListIter;

class BBVarList : public List<xoc::Var*> {
    List<xoc::Var*> m_free_list;
public:
    xoc::Var * getFreeVar() { return m_free_list.remove_tail(); }

    void freeAll()
    {
        UINT i = 0;
        for (xoc::Var * v = get_head(); i < get_elem_count();
             i++, v = get_next()) {
            m_free_list.append_tail(v);
        }
        //clean();
    }
};

typedef List<xoc::Var*> FuncVarList;
typedef xcom::C<Var*> * FuncVarListIter;

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
            for (VecIdx i = 0; i <= get_last_idx(); i++) {
                BBSimulator * sim = get(i);
                if (sim != nullptr) {
                    delete sim;
                }
            }
        }
    };

    //True if accessing local variable via [FP pointer - Offst].
    UINT m_is_use_fp:1;

    //True if compute the section offset for global/local variable.
    //Or false to indicates the accessing to the variable without detailed
    //byte offset information. The option is very useful to dump information.
    UINT m_is_compute_sect_offset:1;

    //True to dump OR id when dumpping OR.
    UINT m_is_dump_or_id:1;
    //Record the max bytesize for callee argument section.
    UINT m_max_real_arg_size:29;
    UINT m_sect_count;
    UINT m_reg_count;
    INT m_param_sect_start_offset; //record the parameter start offset.
    UINT m_mmd_count;
    UINT m_or_bb_idx; //take count of ORBB.
    xoc::Region * m_rg;
    ORMgr * m_or_mgr;
    xoc::TypeMgr * m_tm;
    CGMgr * m_cgmgr;
    ORCFG * m_or_cfg; //CFG of region
    ORBBMgr * m_or_bb_mgr; //manage BB of IR.
    Vector<ParallelPartMgr*> * m_ppm_vec; //Record parallel part for CG.
    SR * m_param_pointer;
    SMemPool * m_pool;

    //Mapping from STORE/LOAD operation to the target address.
    List<ORBB*> m_or_bb_list; //descripting all basic blocks of the region.
    UnitSet m_tmp_us; //Used for temporary purpose.
    Vector<xoc::Var const*> m_params; //record the formal parameter.
    RegSetMgr m_regset_mgr;
    SRegid2SR m_regid2sr_map; //Map Symbol Register Id to specifical SR.
    xcom::BitSetMgr m_bs_mgr;
    PRNO2SR m_pr2sr_map;
    Reg2SR m_dedicate_sr_tab;
    IssuePackageListVector m_ipl_vec; //record IssuePackageList for each ORBB
    IssuePackageMgr m_ip_mgr;

    //Mapping from STORE/LOAD operation to the target address.
    xcom::TMap<OR*, xoc::Var const*> m_or2memaddr_map;
    BBVarList m_bb_level_internal_var_list; //record BB local var.
    FuncVarList m_func_level_internal_var_list; //record func var.

protected:
    //Guarantee IR_RETURN at the end of function.
    void addReturnForEmptyRegion();
    void addSerialDependence(ORBB * bb, DataDepGraph * ddg);

    void createORCFG(OptCtx & oc);
    UINT compute_pad();
    SR * computeAndUpdateOffset(SR * sr);
    UINT calcSizeOfParameterPassedViaRegister(
        List<Var const*> const* param_lst) const;

    //Free md's id and var's id back to MDSystem and VarMgr.
    //The index of MD and Var is important resource if there
    //are a lot of CG.
    void destroyVAR();

    void evaluateCallArgSize(IR const* ir);

    SMemPool * get_pool() const { return m_pool; }

    virtual void finiFuncUnit();

    //The function inserts code to initialize and finialize frame pointer register.
    //e.g: foo() { int x; x = 20; }
    //    --- frame start ---
    //    sp <- sp - 4 #spadjust operation.
    //    [sp + 0] = 20
    //    sp <- sp + 4 #spadjust operation.
    //    --- frame end ---
    // If user invoke alloca, spadjust operation will be generated. Thus local
    // variable should be accessed through FP.
    //e.g: foo() { int x; x = 20; void * p = alloca(64); ... }
    //    --- frame start ---
    //    fp <- sp # sp store operation.
    //    sp <- sp - 4 #spadjust operation.
    //    [fp + 0] = 20
    //    sp <- sp - 64 #alloca
    //    ...
    //    sp <- fp # sp reload operation.
    //    --- frame end ---
    void insertCodeToUseFP(List<ORBB*> & entry_lst,
                           List<ORBB*> & exit_lst);

    void localizeBB(SR * sr, ORBB * bb);
    void localizeBBTab(SR * sr, xcom::TTab<ORBB*> * orbbtab);

    //Destroy useless resource.
    void postLS(LIS * lis, DataDepGraph * ddg);
    void preLS(IN ORBB * bb, IN RaMgr * ra_mgr,
               OUT DataDepGraph ** ddg, OUT BBSimulator ** sim,
               OUT LIS ** lis);

    CG * self() { return this; }

    void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        //return MEM_POOL_Alloc(&m_mempool, size);
        ASSERTN(m_pool != nullptr, ("requires graph pool"));
        void * p = smpoolMalloc(size, m_pool);
        if (p == nullptr) return nullptr;
        ::memset(p, 0, size);
        return p;
    }

public:
    CG(xoc::Region * rg, CGMgr * cgmgr);
    virtual ~CG();

    xoc::Var * addBBLevelVar(xoc::Type const* type, UINT align);
    xoc::Var * addFuncLevelVar(xoc::Type const* type, UINT align);
    void appendReload(ORBB * bb, ORList & ors);
    LabelInfoList * allocLabelInfoList()
    { return (LabelInfoList*)xmalloc(sizeof(LabelInfoList)); }
    ORBB * allocBB();
    RegSet * allocRegSet() { return m_regset_mgr.allocRegSet(); }
    //Assign physical register manually.
    //During some passes, e.g IR2OR, user expects to assign physical register
    //to SR which is NOT dedicated register.
    virtual void assignPhyRegister(SR * sr, Reg reg, REGFILE rf);
    virtual BBSimulator * allocBBSimulator(ORBB * bb);
    virtual LIS * allocLIS(ORBB * bb, DataDepGraph * ddg,
                           BBSimulator * sim, UINT sch_mode);
    virtual DataDepGraph * allocDDG();
    void assembleSRVec(SRVec * srvec, SR * sr1, SR * sr2);
    virtual IR2OR * allocIR2OR() = 0;
    virtual ORCFG * allocORCFG();
    virtual IssuePackageMgr * allocIssuePackageMgr();
    virtual RaMgr * allocRaMgr(List<ORBB*> * bblist, bool is_func);

    //OR Builder
    //Build pseduo instruction that indicate LabelInfo.
    //Note OR_label must be supplied by Target.
    void buildLabel(xoc::LabelInfo const* li, OUT ORList & ors, MOD IOC * cont);

    LabelInfoList * buildLabelInfoList(IR const* castlst);

    //Build nop instruction.
    virtual OR * buildNop(UNIT unit, CLUST clust) = 0;

    //Generate OR with variant number of operands and results.
    //Note user should pass into the legal number of result and operand SRs
    //that corresponding to 'orty'.
    virtual OR * buildOR(OR_CODE orty, UINT resnum, UINT opndnum, ...);
    virtual OR * buildOR(OR_CODE orty, IN SRList & result, IN SRList & opnd);

    //Generate OR with variant number of operands and results.
    //Note user should pass into the legal number of result and operand SRs
    //that corresponding to 'orty'.
    OR * buildOR(OR_CODE orty, ...);

    //The function builds stack-pointer adjustment operation.
    //Note XGEN supposed that the direction of stack-pointer is always
    //decrement.
    //bytesize: bytesize that needed to adjust, it can be immediate or register.
    virtual void buildAlloca(OUT ORList & ors, SR * bytesize, MOD IOC * cont);
    virtual void buildSpadjust(OUT ORList & ors, MOD IOC * cont);
    virtual void buildSpill(IN SR * store_val, IN xoc::Var * spill_loc,
                            IN ORBB * bb, OUT ORList & ors);
    virtual void buildReload(IN SR * result_val, IN xoc::Var * reload_loc,
                             IN ORBB * bb, OUT ORList & ors);

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildMul(SR * src1, SR * src2, UINT sr_size,
                          bool is_sign, OUT ORList &, IN IOC *)
    {
        DUMMYUSE(is_sign);
        DUMMYUSE(src1);
        DUMMYUSE(src2);
        DUMMYUSE(sr_size);
        ASSERTN(0, ("Target Dependent Code"));
    }

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildDiv(SR * src1, SR * src2, UINT sr_size,
                          bool is_sign, OUT ORList &, IN IOC *)
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
    virtual void buildBinaryOR(IR_CODE code, SR * src1, SR * src2,
                               bool is_signed, OUT ORList & ors, MOD IOC * cont);

    //'sr_size': The number of integral multiple of byte-size of single SR.
    virtual void buildAdd(SR * src1, SR * src2, UINT sr_size,
                          bool is_sign, OUT ORList & ors, MOD IOC * cont);

    //'sr_size': The number of integral substract of byte-size of single SR.
    virtual void buildSub(SR * src1, SR * src2, UINT sr_size,
                          bool is_sign, OUT ORList & ors, MOD IOC * cont);
    virtual void buildAddRegImm(SR * src, SR * imm, UINT sr_size,
                                bool is_sign, OUT ORList & ors,
                                MOD IOC * cont) = 0;
    virtual void buildAddRegReg(bool is_add, SR * src1, SR * src2, UINT sr_size,
                                bool is_sign, OUT ORList & ors,
                                MOD IOC * cont) = 0;
    virtual void buildMod(CLUST clust, SR ** result, SR * src1, SR * src2,
                          UINT sr_size, bool is_sign, OUT ORList & ors,
                          MOD IOC * cont);

    //Build copy-operation with given predicate register.
    virtual void buildCopyPred(CLUST clust, UNIT unit,
                               IN SR * to, //should not add 'const' qualifier
                                           //because RA will change the value.
                               IN SR * from, IN SR * pd, OUT ORList & ors) = 0;

    //Build function call instruction.
    virtual void buildCall(xoc::Var const* callee, UINT ret_val_size,
                           OUT ORList & ors, IOC * cont) = 0;

    //Build indirect function call instruction.
    //Function-Call may violate SP,FP,GP,
    //RFLAG register, return-value register,
    //return address register.
    virtual void buildICall(SR * callee, UINT ret_val_size,
                            OUT ORList & ors, IOC * cont) = 0;

    //Build load-address instruction.
    virtual void buildStore(SR * store_val, xoc::Var const* base,
                            HOST_INT ofst, bool is_signed,
                            OUT ORList & ors, MOD IOC * cont);
    virtual void buildLoad(SR * load_val, xoc::Var const* base,
                           HOST_INT ofst, bool is_signed,
                           OUT ORList & ors, MOD IOC * cont)
    {
        ASSERT0(load_val->is_reg());
        buildLoad(load_val, genVAR(base), genIntImm(ofst, true), is_signed,
                  ors, cont);
    }
    virtual void buildGeneralLoad(IN SR * val, HOST_INT ofst, bool is_signed,
                                  OUT ORList & ors, MOD IOC * cont);
    virtual void buildTypeCvt(SR * src, UINT tgt_size, UINT src_size,
                              bool is_signed, Dbx const* dbx, OUT ORList & ors,
                              MOD IOC * cont);
    virtual void buildTypeCvt(IR const* tgt, IR const* src,
                              OUT ORList & ors, MOD IOC * cont);

    //Implement memory block copy.
    //Note tgt and src should not overlap.
    virtual void buildMemcpyInternal(SR * tgt, SR * src, UINT bytesize,
                                     OUT ORList & ors, MOD IOC * cont) = 0;
    //Generate ::memcpy.
    void buildMemcpy(SR * tgt, SR * src, UINT bytesize,
                     OUT ORList & ors, MOD IOC * cont);

    //Generate operations: reg = &var + lda_ofst
    //lda_ofst: the offset based to var.
    virtual void buildLda(xoc::Var const* var, HOST_INT lda_ofst,
                          Dbx const* dbx, OUT ORList & ors, MOD IOC * cont);

    //Generate opcode of accumulating operation.
    //Form as: A = A op B
    virtual void buildAccumulate(OR * red_or, SR * red_var, SR * restore_val,
                                 OUT List<OR*> & ors);
    virtual void buildLoad(IN SR * load_val, IN SR * base,
                           IN SR * ofst, bool is_signed, OUT ORList & ors,
                           MOD IOC * cont) = 0;
    virtual void buildStore(SR * store_val, SR * base,
                            SR * ofst, bool is_signed, OUT ORList & ors,
                            MOD IOC * cont) = 0;

    //Build a general copy operation from register to register.
    //to: source register
    //from: target register
    //unit: function unit.
    //clust: cluster.
    virtual void buildCopy(CLUST clust, UNIT unit, SR * to, SR * from,
                           OUT ORList & ors) = 0;

    //Generate copy operation from source to register.
    //Source can be immediate or register, and target must be register.
    //Note there is no difference between signed and unsigned number moving.
    virtual void buildMove(IN SR * to, IN SR * from, OUT ORList & ors,
                           MOD IOC * cont) = 0;
    virtual void buildUncondBr(IN SR * tgt_lab, OUT ORList & ors,
                               MOD IOC * cont) = 0;
    virtual void buildCondBr(IN SR * tgt_lab, OUT ORList & ors,
                             MOD IOC * cont) = 0;
    virtual void buildCompare(OR_CODE br_cond, bool is_truebr,
                              IN SR * opnd0, IN SR * opnd1, OUT ORList & ors,
                              MOD IOC * cont) = 0;

    //Generate inter-cluster copy operation.
    //Copy from 'src' in 'src_clust' to 'tgt' of in 'tgt_clust'.
    virtual OR * buildBusCopy(IN SR * src, IN SR * tgt, IN SR * pd,
                              CLUST src_clust, CLUST tgt_clust) = 0;
    virtual void buildShiftLeft(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                                OUT ORList & ors, MOD IOC * cont) = 0;
    virtual void buildShiftRight(IN SR * src, ULONG sr_size,
                                 IN SR * shift_ofst, bool is_signed,
                                 OUT ORList & ors, MOD IOC * cont) = 0;
    //Build memory store operation that store 'reg' into stack.
    //NOTE: user have to assign physical register manually if there is
    //new OR generated and requires register allocation.
    //reg: register to be stored.
    //offset: bytesize offset related to SP.
    //ors: record output.
    //cont: context.
    virtual void buildStoreAndAssignRegister(SR * reg, UINT offset,
                                             OUT ORList & ors, MOD IOC * cont);
    //Increase 'reg' by 'val'.
    virtual void buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);
    //Decrease 'reg' by 'val'.
    virtual void buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);

    //The function update offset of a Load or Store operation with an given
    //immediate.
    void calcOfstByImm(SR * ofst, HOST_INT imm);
    void constructORBBList(IN ORList  & or_list);
    void computeEntryAndExit(IN ORCFG & cfg, OUT List<ORBB*> & entry_lst,
                             OUT List<ORBB*> & exit_lst);

    //Compute the index of operand, return OR_SR_IDX_UNDEF if 'opnd' is not an
    //opnd of 'o'.
    INT computeOpndIdx(OR * o, SR const* opnd);

    //Compute the index of 'res', return OR_SR_IDX_UNDEF if 'res' is not a
    //result of 'o'.
    INT computeResultIdx(OR * o, SR const* res);

    //Estimate and reserve stack memory space for real parameters.
    //The function also collecting the information to determine whether enable
    //the using of FP register.
    void computeMaxRealParamSpace();
    virtual void computeAndUpdateGlobalVarLayout(xoc::Var const* var,
                                                 OUT SR ** base,
                                                 OUT SR ** base_ofst);
    //Allocate 'var' on stack.
    //base: can be one of stack pointer(SP) or frame pointer(FP).
    //base_ofst: the byte offset related to 'base'.
    virtual void computeAndUpdateStackVarLayout(xoc::Var const* var,
                                                OUT SR ** sp, //stack pointer
                                                OUT SR ** sp_ofst);
    virtual void computeParamLayout(xoc::Var const* id, OUT SR ** base,
                                    OUT SR ** ofst);

    //Calculate total memory space for parameters, with considering
    //memory alignment.
    virtual UINT computeTotalParameterStackSize(IR const* ir) const;

    //The function compute the base SR and offset SR to given 'var'.
    //Note var can be global or local.
    //var_ofst: byte offset related to begin address of 'var'.
    virtual void computeVarBaseAndOffset(xoc::Var const* var,
                                         ULONGLONG var_ofst,
                                         OUT SR ** base, OUT SR ** ofst);
    virtual CLUST computeAsmCluster(OR * o);

    //Return the index of copied source operand.
    virtual INT computeCopyOpndIdx(OR *)
    {
        ASSERTN(0, ("Target Dependent Code"));
        return -1;
    }

    //Compute the byte size of memory which will be loaded/stored.
    virtual INT computeMemByteSize(OR * o)
    {
        ASSERT0_DUMMYUSE(o);
        ASSERTN(o->is_mem(), ("Need memory operation"));
        ASSERTN(0, ("Target Dependent Code"));
        return -1;
    }

    //Return the alternative equivalent o-type of 'from'
    //that correspond with 'to_unit' and 'to_clust'.
    virtual OR_CODE computeEquivalentORCode(OR_CODE from, UNIT to_unit,
                                            CLUST to_clust);

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
    //is_test: only test purpose
    virtual bool changeORUnit(OR * o, UNIT to_unit, CLUST to_clust,
                              RegFileSet const* regfile_unique,
                              bool is_test);

    //Return the combination of all of available function unit of 'o'.
    //This function will modify m_tmp_us internally.
    virtual UnitSet const* computeORUnit(OR const*, OUT UnitSet*);
    virtual UnitSet const* computeORUnit(OR const* o)
    { return computeORUnit(o, nullptr); }

    //Change the correlated cluster of 'o'
    //If is_test is true, this function only check whether the given
    //OR can be changed.
    //is_test: true to query whether have to change OR's cluster.
    virtual bool changeORCluster(OR * o, CLUST to_clust,
                                 RegFileSet const* regfile_unique,
                                 bool is_test);

    //Change 'o' to 'ot', modifing all operands and results.
    virtual bool changeORCode(OR * o, OR_CODE ot, CLUST src,
                              CLUST tgt, RegFileSet const* regfile_unique);

    virtual SR * dupSR(SR const* sr);
    virtual OR * dupOR(OR const* o);
    virtual void dumpSection();
    void dumpPackage();
    void dumpORBBList() const;
    void dumpVar() const;

    //Expand pseudo SpAdjust operation to real target AddInteger instruction.
    //Note this function is target dependent.
    virtual void expandSpadjust(OR * o, OUT IssuePackageList * ipl);

    //Expand pseudo operation to real target AddInteger instruction.
    //Note this function is target dependent.
    virtual void expandFakeOR(OR * o, OUT IssuePackageList * ipl);

    //Format label list string in 'buf'.
    CHAR * formatLabelListString(LabelInfoList const* lablst,
                                 OUT xcom::StrBuf & buf) const
    {
        for (LabelInfoList const* ll = lablst;
             ll != nullptr; ll = ll->get_next()) {
            formatLabelName(ll->getLabel(), buf);
            if (ll->get_next() != nullptr) {
                buf.strcat(",");
            }
        }
        return buf.buf;
    }

    //Format label name string in 'buf'.
    CHAR * formatLabelName(xoc::LabelInfo const* lab,
                           OUT xcom::StrBuf & buf) const
    {
        CHAR const* prefix = m_rg->getRegionVar()->get_name()->getStr();
        buf.strcat("%s_", prefix);
        if (LABELINFO_type(lab) == L_ILABEL) {
            buf.strcat(ILABEL_STR_FORMAT, ILABEL_CONT(lab));
        } else if (LABELINFO_type(lab) == L_CLABEL) {
            buf.strcat(CLABEL_STR_FORMAT, CLABEL_CONT(lab));
        } else if (LABELINFO_type(lab) == L_PRAGMA) {
            buf.strcat(PRAGMA_STR_FORMAT, PRAGMA_CONT(lab));
        } else {
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
    SR * genLabelList(LabelInfoList const* lilst);
    SR * genSR(SR_CODE c);
    SR * genSR(Reg reg, REGFILE regfile);

    //Generate a SR if bytes_size not more than GENERAL_REGISTER_SIZE,
    //otherwise generate a vector or SR.
    //Return the first register if vector generated.
    SR * genReg(UINT bytes_size = GENERAL_REGISTER_SIZE);

    //Generate SR by specified Symbol Register Id.
    SR * genReg(SymRegId regid);

    //Generate a global SR that bytes_size is not more than
    //GENERAL_REGISTER_SIZE.
    SR * genRegWithPhyReg(Reg reg, REGFILE rf);

    //Generate SR that indicates const value.
    SR * genIntImm(HOST_INT val, bool is_signed);
    SR * genFpImm(HOST_FP val);
    SR * genZero() { return genIntImm((HOST_INT)0, false); }
    SR * genOne() { return genIntImm((HOST_INT)1, false); }

    Region * getRegion() const { return m_rg; }
    //Generate dedicated register by specified physical register.
    SR * genDedicatedReg(Reg phy_reg);
    //Get dedicated register by specified physical register.
    SR * getDedicatedReg(Reg phy_reg) const;
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
    CGMgr * getCGMgr() const { return m_cgmgr; }
    xcom::BitSetMgr * getBitSetMgr() { return &m_bs_mgr; }
    //Generate spill location that same like 'sr'.
    //Or return the spill location if exist.
    //'sr': the referrence SR.
    xoc::Var * genSpillVar(SR * sr);
    void generateFuncUnitDedicatedCodeForEntryBB(List<ORBB*> const& entry_lst,
                                                 bool has_call,
                                                 OUT ORList & ors);
    void generateFuncUnitDedicatedCodeForExitBB(List<ORBB*> const& exit_lst,
                                                bool has_call,
                                                OUT ORList & ors);
    void generateFuncUnitDedicatedCode();
    ORCFG * getORCFG() const { return m_or_cfg; }
    xoc::TypeMgr * getTypeMgr() const { return m_tm; }
    List<ORBB*> * getORBBList() { return &m_or_bb_list; }

    BBVarList * getBBLevelVarList() { return &m_bb_level_internal_var_list; }
    FuncVarList * getFuncLevelVarList()
    { return &m_func_level_internal_var_list; }

    //Construct a name for Var that will lived in a ORBB.
    CHAR const* genBBLevelNewVarName(OUT xcom::StrBuf & name);

    //Construct a name for Var that will lived in Region.
    CHAR const* genFuncLevelNewVarName(OUT xcom::StrBuf & name);
    UINT getMaxArgSectionSize() const { return m_max_real_arg_size; }
    xoc::Var * genTempVar(xoc::Type const* type, UINT align, bool func_level);
    OR * getOR(UINT id);
    OR * genOR(OR_CODE ort) { return m_cgmgr->getORMgr()->genOR(ort, this); }
    virtual REGFILE getRflagRegfile() const = 0;
    virtual REGFILE getPredicateRegfile() const;
    Vector<xoc::Var const*> const& get_param_vars() const { return m_params; }
    virtual ORAsmInfo * getAsmInfo(OR const*) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        return nullptr;
    }

    virtual RegFileSet const* getValidRegfileSet(OR_CODE orcode, UINT idx,
                                                 bool is_result) const;
    virtual RegSet const* getValidRegSet(OR_CODE orcode, UINT idx,
                                         bool is_result) const;
    SRMgr * getSRMgr() { return m_cgmgr->getSRMgr(); }
    ORMgr * getORMgr() { return m_cgmgr->getORMgr(); }
    SRVecMgr * getSRVecMgr() { return m_cgmgr->getSRVecMgr(); }
    IssuePackageListVector * getIssuePackageListVec() { return &m_ipl_vec; }
    IssuePackageMgr * getIssuePackageMgr() { return &m_ip_mgr; }

    //Map phsical register to dedicated symbol register if exist.
    SR * getDedicatedSRForPhyReg(Reg reg);

    //Generate and return the mask code that used to reserved the lower
    //'bytesize' bytes.
    static HOST_UINT getMaskByByte(UINT bytesize);

    void flattenInVec(SR * argval, Vector<SR*> * vec);

    virtual void initFuncUnit();
    //Generate dedicated SR for subsequent use.
    virtual void initDedicatedSR();
    virtual bool isPassArgumentThroughRegister() = 0;
    void initGlobalVar(VarMgr * vm);
    bool isComputeStackOffset() const { return m_is_compute_sect_offset; }
    bool isGRAEnable() const;
    bool isEvenReg(Reg reg) const
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
    bool isOpndSameWithResult(SR const* test_sr, OR const* o,
                              OUT INT * opndnum, OUT INT * resnum) const;
    //Return true if specified operand or result is Rflag register.
    bool isRflagRegister(OR_CODE ot, UINT idx, bool is_result) const;
    virtual bool isReductionOR(OR const* o) const;

    //Return true if specified immediate operand is in valid range.
    bool isValidImmOpnd(OR_CODE ot, UINT idx, HOST_INT imm) const;
    bool isValidImmOpnd(OR_CODE ot, HOST_INT imm) const;
    bool isValidImmOpnd(OR const* o) const;
    //Return true if specified immediate in
    //valid range that described with bitsize.
    bool isValidImm(UINT bitsize, HOST_INT imm) const;

    //Return true if regfile can be assigned to referred operand.
    virtual bool isValidRegFile(OR_CODE ot, INT opndnum, REGFILE regfile,
                                bool is_result) const;

    //Return true if regfile can be assigned to referred operand.
    virtual bool isValidRegFile(OR * o, SR const* opnd, REGFILE regfile,
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
    virtual bool isMultiResultOR(OR_CODE, UINT res_num) const = 0;

    //Return true if or-type has multiple results.
    virtual bool isMultiResultOR(OR_CODE) const = 0;

    //'opnd_num': -1 means return true if or-type has multiple storre-val.
    virtual bool isMultiStore(OR_CODE, INT opnd_num) const = 0;

    //'res_num': -1 means return true if ot has multiple result SR.
    //    non -1 means return true if ot has the number of 'res_num' results.
    virtual bool isMultiLoad(OR_CODE, INT res_num) const = 0;

    virtual bool isValidOpndRegfile(OR_CODE opcode, INT opndnum,
                                    REGFILE regfile) const;
    virtual bool isValidResultRegfile(OR_CODE opcode, INT resnum,
                                      REGFILE regfile) const;

    //Return true of 'sr' has assigned physical-register that obey the target
    //machine vector register rule, or not assign any physical-register.
    //Return false if 'sr' assigned incorrect phyiscal-register that violate the
    //target machine vector register layout rule.
    //idx: opnd/result index of 'sr'.
    //is_result: it is true if 'sr' being the result of 'o'.
    virtual bool isValidRegInSRVec(OR  const* o, SR  const* sr,
                                   UINT idx, bool is_result) const;

    //Return true of all SR in 'srvec' have assigned physical-register that
    //obey the target machine vector register rule, or not assign any
    //physical-register.
    //Return false if SR assigned incorrect phyiscal-register that violate the
    //target machine vector register layout rule.
    //is_result: it is true if 'sr' being the result of some OR.
    bool isValidRegInSRVec(SRVec const* srvec, bool is_result) const;

    //Return true if the runtime value of base1 is equal to base2.
    //Some target might use mulitple stack pointers.
    virtual bool isStackPointerValueEqu(SR const* base1, SR const* base2) const
    {
        DUMMYUSE(base1);
        DUMMYUSE(base2);
        ASSERTN(0, ("Target Dependent Code"));
        return false;
    }
    bool isDumpORId() const { return m_is_dump_or_id; }

    //Return true if function code uses stack frame pointer register.
    bool isUseFP() const { return m_is_use_fp && g_is_enable_fp; }

    virtual OR_CODE invertORCode(OR_CODE)
    {
        ASSERTN(0, ("Target Dependent Code"));
        return OR_UNDEF;
    }

    //Map from IR code to OR code.
    //Target may apply comparing instruction to calculate boolean value.
    //e.g:
    //     r1 <- 0x1,
    //     r2 <- 0x2,
    //     r0 <- eq, r1, r2 ;then r0 is 0
    virtual OR_CODE mapIRCode2ORCode(IR_CODE ir_code, UINT ir_opnd_size,
                                     IN SR * opnd0, IN SR * opnd1,
                                     bool is_signed) = 0;
    SR * mapPR2SR(PRNO prno) const
    { return const_cast<CG*>(this)->m_pr2sr_map.get(prno); }
    SR * mapSymbolReg2SR(UINT regid) const
    { return const_cast<CG*>(this)->m_regid2sr_map.get(regid); }
    bool mustAsmDef(OR const* o, SR const* sr) const;
    bool mustDef(OR const* o, SR const* sr) const;
    bool mustUse(OR const* o, SR const* sr) const;
    bool mayDefInRange(SR const* sr, IN ORCt * start, IN ORCt * end,
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
    virtual CLUST mapReg2Cluster(Reg) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        CLUST clust = CLUST_UNDEF;
        return clust;
    }


    //Return the function unit which can operate on 'regfile'.
    virtual UnitSet & mapRegFile2UnitSet(REGFILE regfile, SR const* sr,
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

    //Return true if var need to allocate memory space.
    //e.g: function declaration does not need to allocate space.
    virtual bool needAllocateMemorySpace(Var const* var) const
    { return !var->is_decl() && !var->is_func(); }

    void relocateStackVarOffset();
    void renameResult(OR * o, SR * oldsr, SR * newsr, bool match_phy_reg);
    void renameOpnd(OR * o, SR * oldsr, SR * newsr, bool match_phy_reg);
    void renameOpndAndResultFollowed(SR * oldsr, SR * newsr,
                                     ORCt * start, BBORList * ors);
    void renameOpndAndResultFollowed(SR * oldsr, SR * newsr,
                                     OR * start, BBORList * ors);
    void renameOpndAndResultInRange(SR * oldsr, SR * newsr,
                                    ORCt * start, ORCt * end,
                                    BBORList * orlist);
    void renameOpndAndResultInRange(SR * oldsr, SR * newsr,
                                    OR * start, OR * end,
                                    BBORList * orlist);
    virtual void reviseFormalParameterAndSpadjust(List<ORBB*> & entry_lst,
                                                  List<ORBB*> & exit_lst);
    virtual void reviseFormalParamAccess(UINT lv_size);

    //Generate code to store SR on top of stack.
    //argdescmgr: record the parameter which tend to store on the stack.
    void storeArgToStack(ArgDescMgr * argdesc, OUT ORList & ors, IN IOC *);
    void setMapPR2SR(PRNO prno, SR * sr) { m_pr2sr_map.set(prno, sr); }
    void setMapSymbolReg2SR(UINT regid, SR * sr)
    {
        DUMMYUSE(regid);
        m_regid2sr_map.set(SR_sym_reg(sr), sr);
    }

    void setCluster(ORList & ors, CLUST clust);
    void setDumpORId(bool doit) { m_is_dump_or_id = doit; }
    void setComputeSectOffset(bool doit) { m_is_compute_sect_offset = doit; }
    void storeRegisterParameterBackToStack(List<ORBB*> * entry_lst,
                                           UINT param_start);

    //Interface to generate ORs to store physical register on top of stack.
    virtual void storeRegToStack(SR*, OUT ORList &)
    { ASSERTN(0, ("Target Dependent Code")); }

    //Interface to generate ORs to reload physical register from top of stack.
    virtual void reloadRegFromStack(SR*, OUT ORList &)
    { ASSERTN(0, ("Target Dependent Code")); }

    //True if current argument register should be bypassed.
    virtual bool skipArgRegister(Var const* param, RegSet const* regset,
                                 Reg reg) const
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

    void prependSpill(ORBB * bb, ORList & ors);

    //Perform Instruction Scheduling.
    virtual void performIS(OUT Vector<BBSimulator*> & simvec, RaMgr * ra_mgr);

    //Perform Register Allocation.
    virtual RaMgr * performRA();

    //Generate target dependent information.
    bool perform();

    void passArgVariant(ArgDescMgr * argdescmgr, OUT ORList & ors,
                        UINT num, ...);

    //This function try to pass all arguments through registers.
    //Otherwise pass remaining arguments through stack memory.
    //ir: the first parameter of CALL.
    //argval: if it is not emtpy, the current argument is value.
    //        Note only one of argval and argaddr is available.
    //argaddr: if it is not emtpy, the current argument is address.
    //         Note only one of argval and argaddr is available.
    //argsz: argument byte size.
    //ors: record the generated ORs.
    //cont: context.
    void passArg(SR * argval, SR * argaddr, UINT argsz,
                 OUT ArgDescMgr * argdescmgr, OUT ORList & ors, MOD IOC * cont);
    bool passArgInRegister(SR * argval, UINT * sz, ArgDescMgr * argdescmgr,
                           ORList & ors, IOC const* cont);
    bool passArgInMemory(SR * argaddr, UINT * argsz,
                         OUT ArgDescMgr * argdescmgr, OUT ORList & ors,
                         IOC * cont);

    bool tryPassArgThroughRegister(SR * regval, SR * start_addr,
                                   UINT * argsz, OUT ArgDescMgr * argdescmgr,
                                   OUT ORList & ors, IOC * cont);

    bool verifyPackageList();
    bool verifyOR(OR const* o);

    void updateMaxCalleeArgSize(UINT maxsize)
    {
        ASSERT0(maxsize < MAX_STACK_SPACE);
        UINT sz = m_max_real_arg_size;
        sz = MAX(sz, maxsize);
        sz = MAX(sz, (UINT)xcom::ceil_align(sz, STACK_ALIGNMENT));
        m_max_real_arg_size = sz;
    }
};

} //namespace xgen
#endif
