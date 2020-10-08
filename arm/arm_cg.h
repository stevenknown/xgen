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
#ifndef _ARM_CG_H_
#define _ARM_CG_H_

//
//START ARMCG
//
class ARMCG : public CG {
private:
    void buildStoreCase1(IN SR * store_val,
                         IN SR * base,
                         IN SR * sr_ofst,
                         Var const* v,
                         OUT ORList & ors,
                         IN IOC * cont);
    void buildStoreCase2(IN SR * store_val,
                         IN SR * base,
                         IN SR * sr_ofst,
                         Var const* v,
                         OUT ORList & ors,
                         IN IOC * cont);
    void buildShiftRightCase1(IN SR * src,
                              ULONG sr_size,
                              IN SR * shift_ofst,
                              bool is_signed,
                              OUT ORList & ors,
                              IN OUT IOC * cont);
    void buildShiftRightCase2(IN SR * src,
                              ULONG sr_size,
                              IN SR * shift_ofst,
                              bool is_signed,
                              OUT ORList & ors,
                              IN OUT IOC * cont);
    void buildShiftRightCase3(IN SR * src,
                              ULONG sr_size,
                              IN SR * shift_ofst,
                              bool is_signed,
                              OUT ORList & ors,
                              IN OUT IOC * cont);
    void buildShiftRightCase3_1(IN SR * src,
                                ULONG sr_size,
                                IN SR * shift_ofst,
                                bool is_signed,
                                OUT ORList & ors,
                                IN OUT IOC * cont);
    void buildShiftRightCase3_2(IN SR * src,
                                ULONG sr_size,
                                IN SR * shift_ofst,
                                bool is_signed,
                                OUT ORList & ors,
                                IN OUT IOC * cont);
    void buildShiftRightCase3_3(IN SR * src,
                                ULONG sr_size,
                                IN SR * shift_ofst,
                                bool is_signed,
                                OUT ORList & ors,
                                IN OUT IOC * cont);
    //This is an util function.
    //Build several [tgt] <- [src] operations accroding unrolling factor.
    //e.g: given unrolling factor is 2, two memory assignments will be generated:
    //     [tgt] <- [src];
    //     src <- src + GENERAL_REGISTER_SIZE;
    //     tgt <- tgt + GENERAL_REGISTER_SIZE;
    //     [tgt] <- [src];
    //tgt: target memory address register.
    //src: source memory address register.
    void buildMemAssignUnroll(SR * tgt,
                              SR * src,
                              UINT unroll_factor,
                              OUT ORList & ors,
                              IN IOC * cont);

    //This is an util function.
    //Build [tgt] <- [src] operation.
    //tgt: target memory address register.
    //src: source memory address register.
    void buildMemAssign(SR * tgt,
                        SR * src,
                        OUT ORList & ors,
                        IN IOC * cont);

    //This is an util function.
    //Build [tgt] <- [src] operation.
    //tgt: target memory address register.
    //src: source memory address register.
    //bytesize: assigned bytesize that customized by caller.
    void buildMemAssignBySize(SR * tgt,
                              SR * src,
                              UINT bytesize,
                              OUT ORList & ors,
                              IN IOC * cont);
    //Increase 'reg' by 'val'.
    virtual void buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);
    //Decrease 'reg' by 'val'.
    virtual void buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);

    //This is an util function.
    //Build several [tgt] <- [src] operations accroding unrolling factor.
    //Generate loop to copy from src to tgt.
    //e.g: given tgt address register, src address register, bytesize:
    //  srt2 = bytesize
    //  LOOP_START:
    //  x = [src]
    //  [tgt] = x
    //  src = src + 4byte
    //  tgt = tgt + 4byte
    //  srt2 = srt2 - 4byte
    //  teq_i, cpsr = srt2, 0
    //  b.ne, LOOP_START
    //tgt: target memory address register.
    //src: source memory address register.
    void buildMemAssignLoop(SR * tgt,
                            SR * src,
                            UINT bytesize,
                            OUT ORList & ors,
                            IN IOC * cont);

    void expandFakeShift(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeStore(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeSpadjust(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeLoad(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeMultiLoad(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeMov32(IN OR * o, OUT IssuePackageList * ipl);

protected:
    void _output_bss(FILE * asmh, Section & sect);
    void _output_data(FILE * asmh, Section & sect);
    SR const* m_sp;
    SR const* m_fp;
    SR const* m_gp;
    SR const* m_true_pred;
    virtual void initBuiltin();

public:
    Var const* m_builtin_uimod;
    Var const* m_builtin_imod;
    Var const* m_builtin_uidiv;
    Var const* m_builtin_ashldi3;
    Var const* m_builtin_lshrdi3;
    Var const* m_builtin_ashrdi3;
    Var const* m_builtin_modsi3;
    Var const* m_builtin_umodsi3;
    Var const* m_builtin_moddi3;
    Var const* m_builtin_umoddi3;
    Var const* m_builtin_addsf3;
    Var const* m_builtin_adddf3;
    Var const* m_builtin_subsf3;
    Var const* m_builtin_subdf3;
    Var const* m_builtin_divsi3;
    Var const* m_builtin_udivsi3;
    Var const* m_builtin_divsf3;
    Var const* m_builtin_divdi3;
    Var const* m_builtin_udivdi3;
    Var const* m_builtin_divdf3;
    Var const* m_builtin_muldi3;
    Var const* m_builtin_mulsf3;
    Var const* m_builtin_muldf3;
    Var const* m_builtin_ltsf2;
    Var const* m_builtin_gtsf2;
    Var const* m_builtin_gesf2;
    Var const* m_builtin_eqsf2;
    Var const* m_builtin_nesf2;
    Var const* m_builtin_lesf2;
    Var const* m_builtin_ltdf2;
    Var const* m_builtin_gtdf2;
    Var const* m_builtin_gedf2;
    Var const* m_builtin_eqdf2;
    Var const* m_builtin_nedf2;
    Var const* m_builtin_ledf2;
    Var const* m_builtin_fixsfsi;
    Var const* m_builtin_fixdfsi;
    Var const* m_builtin_fixunssfsi;
    Var const* m_builtin_fixunsdfsi;
    Var const* m_builtin_fixunssfdi;
    Var const* m_builtin_fixunsdfdi;
    Var const* m_builtin_truncdfsf2;
    Var const* m_builtin_floatsisf;
    Var const* m_builtin_floatdisf;
    Var const* m_builtin_floatsidf;
    Var const* m_builtin_floatdidf;
    Var const* m_builtin_fixsfdi;
    Var const* m_builtin_fixdfdi;
    Var const* m_builtin_floatunsisf;
    Var const* m_builtin_floatundisf;
    Var const* m_builtin_floatunsidf;
    Var const* m_builtin_floatundidf;
    Var const* m_builtin_extendsfdf2;

public:
    explicit ARMCG(Region * rg, CGMgr * cgmgr) : CG(rg, cgmgr)
    {
        m_sp = getSP();
        //TODO
        //m_fp = getFP();
        //m_gp = getGP();
        m_true_pred = getTruePred();

        //Xocfe will generate code for big return-value like:
        //extern struct _S {int a ; int b ; int c ; int d ; } foo ();
        //   $1:mc(16) = call 'foo' //<-- So do not gen code to store to $1.
        //   st:mc(16) 's'
        //       ld:mc(16) 'retval_buf_of_foo'
        g_gen_code_for_big_return_value = false;
    }
    COPY_CONSTRUCTOR(ARMCG);
    virtual ~ARMCG() {}

    virtual SR * getSP() const;
    virtual SR * getFP() const;
    virtual SR * getGP() const;
    virtual SR * getReturnAddr() const;
    virtual SR * getRflag() const;
    virtual SR * getTruePred() const;
    virtual SR * genSP();
    virtual SR * genFP();
    virtual SR * genGP();
    SR * gen_r0();
    SR * gen_r1();
    SR * gen_r2();
    SR * gen_r3();
    SR * gen_r12();
    virtual SR * genReturnAddr();
    virtual SR * genRflag();
    virtual SR * genTruePred();
    SR * genEQPred();
    SR * genNEPred();
    SR * genCSPred(); //Carry set.
    SR * genCCPred(); //Carry clear.
    SR * genHSPred(); //Signed higher (identical to CS, Unsigned GE).
    SR * genLOPred(); //Unsigned lower (identical to CC, Unsigned LT)
    SR * genMIPred(); //Minus, negative, less-than.
    SR * genPLPred(); //Plus, positive or zero.
    SR * genGEPred(); //Signed GE
    SR * genLTPred(); //Signed LT
    SR * genGTPred(); //Signed GT
    SR * genLEPred(); //Signed LE
    SR * genHIPred(); //Unsigned GT
    SR * genLSPred(); //Unsigned LE

    //FIXME, only for passing the compilation.
    virtual SR * genPredReg() { return genTruePred(); }

    virtual REGFILE getRflagRegfile() const { return RF_CPSR; }
    virtual REGFILE getPredicateRegfile() const { return RF_UNDEF; }

    //Implement memory block copy.
    //Note tgt and src should not overlap.
    virtual void buildMemcpyInternal(
            SR * tgt,
            SR * src,
            UINT bytesize,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildSpadjust(OUT ORList & ors, IN IOC * cont);
    virtual OR * buildNop(UNIT unit, CLUST clust);
    virtual void buildStore(
            SR * store_val,
            SR * base,
            SR * ofst,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildCopy(
            CLUST clust,
            UNIT unit,
            SR * to,
            SR * from,
            OUT ORList & ors);
    virtual void buildMove(
            SR * to,
            SR * from,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildCopyPred(
            CLUST clust,
            UNIT unit,
            IN SR * to,
            IN SR * from,
            IN SR * pd,
            OUT ORList & ors);
    virtual void buildLoad(
            IN SR * load_val,
            IN SR * base,
            IN SR * ofst,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildMul(
            SR * src1,
            SR * src2,
            UINT sr_size,
            bool is_sign,
            OUT ORList & ors,
            IN IOC * cont);
    virtual OR * buildBusCopy(
            IN SR * src,
            IN SR * tgt,
            IN SR * pd,
            CLUST src_clust,
            CLUST tgt_clust);
    virtual void buildAddRegImm(
            SR * src,
            SR * imm,
            UINT sr_size,
            bool is_sign,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildAddRegReg(
            bool is_add,
            SR * src1,
            SR * src2,
            UINT sr_size,
            bool is_sign,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildCondBr(
            IN SR * tgt_lab,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildCompare(
            OR_TYPE br_cond,
            bool is_truebr,
            IN SR * opnd0,
            IN SR * opnd1,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildUncondBr(
            IN SR * tgt_lab,
            OUT ORList & ors,
            IN IOC * cont);
    void buildARMCmp(OR_TYPE cmp_ot,
            IN SR * pred,
            IN SR * opnd0,
            IN SR * opnd1,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildShiftLeft(
            IN SR * src,
            ULONG sr_size,
            IN SR * shift_ofst,
            OUT ORList & ors,
            IN OUT IOC * cont);
    virtual void buildShiftRight(
            IN SR * src,
            ULONG sr_size,
            IN SR * shift_ofst,
            bool is_signed,
            OUT ORList & ors,
            IN OUT IOC * cont);
    void buildCall(Var const* callee,
            UINT ret_val_size,
            OUT ORList & ors,
            IOC * cont);
    void buildICall(SR * callee,
            UINT ret_val_size,
            OUT ORList & ors,
            IOC * cont);
    //'sr_size': The number of byte-size of SR.
    void buildMulRegReg(
            SR * src1,
            SR * src2,
            UINT sr_size,
            bool is_sign,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void buildStoreAndAssignRegister(
            SR * reg,
            UINT offset,
            OUT ORList & ors,
            IN IOC * cont);

    virtual bool changeORType(
            IN OUT OR * o,
            OR_TYPE ortype,
            CLUST src,
            CLUST tgt,
            Vector<bool> const& regfile_unique);
    virtual CLUST computeORCluster(OR const* o) const;
    virtual INT computeCopyOpndIdx(OR * o);
    virtual INT computeMemByteSize(OR * o);
    virtual CLUST computeClusterOfBusOR(OR * o);
    virtual SLOT computeORSlot(OR const* o);
    virtual UINT computeArgAlign(UINT argsz) const
    {
        #ifdef TO_BE_COMPATIBLE_WITH_ARM_LINUX_GNUEABI
        UINT align = argsz >= GENERAL_REGISTER_SIZE * 2 ?
            GENERAL_REGISTER_SIZE * 2 : STACK_ALIGNMENT;

        return align;
        #else
        return STACK_ALIGNMENT;
        #endif
    }

    virtual void expandFakeOR(IN OR * o, OUT IssuePackageList * ipl);

    virtual void initDedicatedSR();
    virtual bool isPassArgumentThroughRegister() { return true; }
    virtual bool isValidRegInSRVec(OR const* o, SR const* sr,
                                   UINT idx, bool is_result) const;
    virtual bool isValidResultRegfile(
            OR_TYPE ortype,
            INT resnum,
            REGFILE regfile) const;
    virtual bool isValidOpndRegfile(
            OR_TYPE ortype,
            INT opndnum,
            REGFILE regfile) const;
    virtual bool isSPUnit(UNIT unit) const;
    virtual bool isIntRegSR(OR const* o,
                            SR const* sr,
                            UINT idx,
                            bool is_result) const;
    virtual bool isBusCluster(CLUST clust) const;
    virtual bool isBusSR(SR const* sr) const;
    virtual bool isRecalcOR(OR const* o) const;
    virtual bool isSameCluster(SLOT slot1, SLOT slot2) const;
    virtual bool isSameLikeCluster(SLOT slot1, SLOT slot2) const;
    virtual bool isSameLikeCluster(OR const* or1, OR const* or2) const;
    virtual bool isMultiResultOR(OR_TYPE ortype, UINT res_num) const;
    virtual bool isMultiResultOR(OR_TYPE ortype) const;
    virtual bool isMultiStore(OR_TYPE ortype, INT opnd_num) const;
    virtual bool isMultiLoad(OR_TYPE ortype, INT res_num) const;
    virtual bool isCopyOR(OR const* o) const;
    virtual bool isStackPointerValueEqu(SR const* base1, SR const* base2) const;
    virtual bool isSP(SR const* sr) const;
    virtual bool isReduction(OR const* o) const;
    virtual bool isEvenReg(REG reg) const;
    virtual OR_TYPE invertORType(OR_TYPE ot)
    {
        switch (ot) {
        case OR_add_i: return OR_sub_i;
        case OR_sub_i: return OR_add_i;
        default: UNREACHABLE();
        }
        return OR_UNDEF;
    }

    virtual OR_TYPE mapIRType2ORType(IR_TYPE ir_type,
                                     UINT ir_opnd_size,
                                     IN SR * opnd0,
                                     IN SR * opnd1,
                                     bool is_signed);
    virtual UnitSet & mapRegFile2UnitSet(REGFILE regfile,
                                         SR const* sr,
                                         OUT UnitSet & us) const;
    virtual CLUST mapSlot2Cluster(SLOT slot);
    virtual UNIT mapSlot2Unit(SLOT slot) const;
    virtual List<REGFILE> & mapCluster2RegFileList(
        CLUST clust,
        OUT List<REGFILE> & regfiles) const;
    virtual List<REGFILE> & mapUnitSet2RegFileList(
        UnitSet const& us,
        OUT List<REGFILE> & regfiles) const;
    virtual SLOT mapUnit2Slot(UNIT unit, CLUST clst) const;
    virtual CLUST mapRegFile2Cluster(REGFILE regfile, SR const* sr) const;
    virtual CLUST mapReg2Cluster(REG reg) const;
    virtual CLUST mapSR2Cluster(OR const* o, SR const* sr) const;

    virtual LIS * allocLIS(ORBB * bb,
                           DataDepGraph * ddg,
                           BBSimulator * sim,
                           UINT sch_mode,
                           bool change_slot,
                           bool change_cluster,
                           bool is_log);
    virtual IR2OR * allocIR2OR();
    virtual BBSimulator * allocBBSimulator(ORBB * bb, bool is_log);
    RaMgr * allocRaMgr(List<ORBB*> * bblist, bool is_func);

    virtual void setSpadjustOffset(OR * spadj, INT size);

    //True if current argument register should be bypassed.
    virtual bool skipArgRegister(
            Var const* param,
            RegSet const* regset,
            REG reg) const;
};
//END ARMCG

#endif
