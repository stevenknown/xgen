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
#ifndef _ARM_CG_H_
#define _ARM_CG_H_

//
//START ARMCG
//
class ARMCG : public CG {
    COPY_CONSTRUCTOR(ARMCG);
private:
    void buildStoreFor3Byte(IN SR * store_val, IN SR * base,
                            IN SR * sr_ofst, Var const* v,
                            OUT ORList & ors, MOD IOC * cont);
    void buildStoreForLessThan4Byte(SR * store_val, IN SR * base,
                                    IN SR * sr_ofst, Var const* v,
                                    bool is_signed, OUT ORList & ors,
                                    MOD IOC * cont);
    void buildStoreFor8Byte(SR const* store_val, SR * base,
                            SR * sr_ofst, Var const* v, bool is_signed,
                            OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase1(IN SR * src, ULONG sr_size,
                              IN SR * shift_ofst, bool is_signed,
                              OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase2(IN SR * src, ULONG sr_size,
                              IN SR * shift_ofst, bool is_signed,
                              OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase3(IN SR * src, ULONG sr_size,
                              IN SR * shift_ofst, bool is_signed,
                              OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase3_1(IN SR * src, ULONG sr_size,
                                IN SR * shift_ofst, bool is_signed,
                                OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase3_2(IN SR * src, ULONG sr_size,
                                IN SR * shift_ofst, bool is_signed,
                                OUT ORList & ors, MOD IOC * cont);
    void buildShiftRightCase3_3(IN SR * src, ULONG sr_size,
                                IN SR * shift_ofst, bool is_signed,
                                OUT ORList & ors, MOD IOC * cont);
    //This is an util function.
    //Build several [tgt] <- [src] operations according unrolling factor.
    //e.g: given unrolling factor is 2, two memory assignments will
    //be generated:
    //  [tgt] <- [src];
    //  src <- src + GENERAL_REGISTER_SIZE;
    //  tgt <- tgt + GENERAL_REGISTER_SIZE;
    //  [tgt] <- [src];
    //tgt: target memory address register.
    //src: source memory address register.
    void buildMemAssignUnroll(SR * tgt, SR * src, UINT unroll_factor,
                              OUT ORList & ors, MOD IOC * cont);

    //This is an util function.
    //Build [tgt] <- [src] operation.
    //tgt: target memory address register.
    //src: source memory address register.
    void buildMemAssign(SR * tgt, SR * src, OUT ORList & ors, MOD IOC * cont);

    //This is an util function.
    //Build [tgt] <- [src] operation.
    //tgt: target memory address register.
    //src: source memory address register.
    //bytesize: assigned bytesize that customized by caller.
    void buildMemAssignBySize(SR * tgt, SR * src, UINT bytesize,
                              OUT ORList & ors, MOD IOC * cont);
    //Increase 'reg' by 'val'.
    virtual void buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);
    //Decrease 'reg' by 'val'.
    virtual void buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont);

    //This is an util function.
    //Build several [tgt] <- [src] operations according unrolling factor.
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
                            MOD IOC * cont);

    SR * computeOfst(OUT SR ** base, SR * ofst, bool is_signed,
                     OUT ORList & ors, MOD IOC * cont);

    void expandFakeShift(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeStore(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeSpadjust(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeLoad(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeMultiLoad(IN OR * o, OUT IssuePackageList * ipl);
    void expandFakeMov32(IN OR * o, OUT IssuePackageList * ipl);

    void genAddrCompFor8ByteStore(MOD SR ** base, MOD SR ** ofst,
                                  OUT ORList & ors);
    void genAddrCompForStoreLessThan4Byte(SR ** base, SR ** ofst,
                                          OUT ORList & ors, IOC const* cont);

    OR_CODE selectORCodeForStoreLessThan4Byte(SR const* ofst,
                                              IOC const* cont) const;
    OR_CODE selectORCodeForStore8Byte(SR const* ofst) const;
protected:
    SR const* m_sp;
    SR const* m_fp;
    SR const* m_gp;
    SR const* m_true_pred;

protected:
    void _output_bss(FILE * asmh, Section & sect);
    void _output_data(FILE * asmh, Section & sect);

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
    SR * genR0();
    SR * genR1();
    SR * genR2();
    SR * genR3();
    SR * genTmp(); //Scratch Register, the synonym is IP register.
    virtual SR * genReturnAddr();
    virtual SR * genRflag();
    virtual SR * genTruePred();

    //ARM Conditions Flag
    //Note LT and GE do not have the completely consistent inverse-behaviors
    //compared to LE and GT, namely, it is not correct to replace GT/LE
    //combination with GE/LT combination, vice versa.
    //e.g: 64bit signed comparation:
    //  if (a > b) TrueBody
    //  FalseBody:
    //
    //Correct code ===>:
    //  cmp b_lo, a_lo //use subs b_lo, a_lo is also work, because compare
    //                 //is substract essentially.
    //  sbcs ip, b_hi, a_hi
    //  bge FalseBody
    //  TrueBody:
    //
    //Incorrect code, swap a, b ===>:
    //  cmp a_lo, b_lo
    //  sbcs ip, a_hi, a_hi
    //  ble FalseBody
    //  TrueBody:
    //
    // Name Condition                    State of Flags
    // EQ   Equal / equals zero          Z set
    // NE   Not equal                    Z clear
    // CS   Carry set                    C set
    // CC   Carry clear                  C clear
    // MI   Minus / negative             N set
    // PL   Plus / positive or zero      N clear
    // VS   Overflow                     V set
    // VC   No overflow                  V clear
    // HS   Unsigned higher or same      C set
    // LO   Unsigned lower               C clear
    // HI   Unsigned higher              C set and Z clear
    // LS   Unsigned lower or same       C clear or Z set
    // GE   Signed greater than or equal N equals V
    // LT   Signed less than             N is not equal to V
    // GT   Signed greater than          Z clear and N equals V
    // LE   Signed less than or equal    Z set or N is not equal to V
    SR * genEQPred();
    SR * genNEPred();
    SR * genCSPred(); //Carry set.(identical to HS, Unsigned GE)
    SR * genCCPred(); //Carry clear.(identical to LO, Unsigned LT)
    SR * genHSPred(); //Signed higher (identical to CS, Unsigned GE).
    SR * genLOPred(); //Unsigned lower (identical to CC, Unsigned LT)
    SR * genMIPred(); //Minus, negative, less-than.
    SR * genPLPred(); //Plus, positive or zero.
    SR * genGEPred(); //Signed greater than or equal, Signed GE
    SR * genLTPred(); //Signed less than, Signed LT.
    SR * genGTPred(); //Signed greater than, Signed GT
    SR * genLEPred(); //Signed less than or equal, Signed LE
    SR * genHIPred(); //Unsigned higher, the synonym of Unsigned GT
    SR * genLSPred(); //Unsigned lower or same, the synonym of Unsigned LE
    SR * getEQPred() const;
    SR * getNEPred() const;
    SR * getCSPred() const; //Carry set.(identical to HS, Unsigned GE)
    SR * getCCPred() const; //Carry clear.(identical to LO, Unsigned LT)
    SR * getHSPred() const; //Signed higher (identical to CS, Unsigned GE).
    SR * getLOPred() const; //Unsigned lower (identical to CC, Unsigned LT)
    SR * getMIPred() const; //Minus, negative, less-than.
    SR * getPLPred() const; //Plus, positive or zero.
    SR * getGEPred() const; //Signed GE
    SR * getLTPred() const; //Signed LT
    SR * getGTPred() const; //Signed GT
    SR * getLEPred() const; //Signed LE
    SR * getHIPred() const; //Unsigned GT
    SR * getLSPred() const; //Unsigned LE

    //FIXME, only for passing the compilation.
    virtual SR * genPredReg() { return genTruePred(); }

    virtual REGFILE getRflagRegfile() const { return RF_CPSR; }
    virtual REGFILE getPredicateRegfile() const { return RF_UNDEF; }

    //Implement memory block copy.
    //Note tgt and src should not overlap.
    virtual void buildMemcpyInternal(SR * tgt, SR * src, UINT bytesize,
                                     OUT ORList & ors, MOD IOC * cont);

    //The function builds stack-pointer adjustment operation.
    //Note XGEN supposed that the direction of stack-pointer is always
    //decrement.
    //bytesize: bytesize that needed to adjust, it can be immediate or register.
    virtual void buildAlloca(OUT ORList & ors, SR * bytesize, MOD IOC * cont);
    virtual void buildSpadjust(OUT ORList & ors, MOD IOC * cont);
    virtual OR * buildNop(UNIT unit, CLUST clust);
    virtual void buildStore(SR * store_val, SR * base, SR * ofst,
                            bool is_signed, OUT ORList & ors, MOD IOC * cont);
    virtual void buildCopy(CLUST clust, UNIT unit, SR * to, SR * from,
                           OUT ORList & ors);
    virtual void buildMove(SR * to, SR * from, OUT ORList & ors,
                           MOD IOC * cont);
    virtual void buildCopyPred(CLUST clust, UNIT unit, IN SR * to,
                               IN SR * from, IN SR * pd, OUT ORList & ors);
    virtual void buildLoad(IN SR * load_val, IN SR * base, IN SR * ofst,
                           bool is_signed, ORList & ors, MOD IOC * cont);
    virtual void buildMul(SR * src1, SR * src2, UINT sr_size,
                          bool is_sign, OUT ORList & ors, MOD IOC * cont);
    virtual OR * buildClusterCopy(IN SR * src, IN SR * tgt, IN SR * pd,
                                  CLUST src_clust, CLUST tgt_clust);
    virtual void buildAddRegImm(SR * src, SR * imm, UINT sr_size,
                                bool is_sign, OUT ORList & ors, MOD IOC * cont);
    virtual void buildAddRegReg(bool is_add, SR * src1, SR * src2, UINT sr_size,
                                bool is_sign, OUT ORList & ors, MOD IOC * cont);
    virtual void buildCondBr(IN SR * tgt_lab, OUT ORList & ors, MOD IOC * cont);
    virtual void buildCompare(OR_CODE br_cond, bool is_truebr,
                              IN SR * opnd0, IN SR * opnd1,
                              OUT ORList & ors, MOD IOC * cont);
    virtual void buildUncondBr(IN SR * tgt_lab, OUT ORList & ors,
                               MOD IOC * cont);
    void buildARMCmp(OR_CODE cmp_ot, IN SR * pred, IN SR * opnd0, IN SR * opnd1,
                     OUT ORList & ors, MOD IOC * cont);
    void buildShiftLeftImm(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                           OUT ORList & ors, MOD IOC * cont);
    void buildShiftLeftReg(IN SR * src, ULONG sr_size, IN SR * shift_ofst,
                           OUT ORList & ors, MOD IOC * cont);
    virtual void buildShiftLeft(IN SR * src, ULONG sr_size,
                                IN SR * shift_ofst, OUT ORList & ors,
                                MOD IOC * cont);
    virtual void buildShiftRight(IN SR * src, ULONG sr_size,
                                 IN SR * shift_ofst, bool is_signed,
                                 OUT ORList & ors, MOD IOC * cont);
    void buildCall(Var const* callee, UINT ret_val_size,
                   OUT ORList & ors, IOC * cont);
    void buildICall(SR * callee, UINT ret_val_size,
                    OUT ORList & ors, IOC * cont);
    //'sr_size': The number of byte-size of SR.
    void buildMulRegReg(SR * src1, SR * src2, UINT sr_size, bool is_sign,
                        OUT ORList & ors, MOD IOC * cont);
    virtual void buildStoreAndAssignRegister(SR * reg, UINT offset,
                                             OUT ORList & ors, MOD IOC * cont);

    virtual bool changeORCode(MOD OR * o, OR_CODE orcode, CLUST src,
                              CLUST tgt, RegFileSet const* regfile_unique);
    virtual CLUST computeORCluster(OR const* o) const;
    virtual INT computeCopyOpndIdx(OR * o);
    virtual INT computeMemByteSize(OR * o);
    virtual CLUST computeClusterOfBusOR(OR * o);
    virtual SLOT computeORSlot(OR const* o);
    virtual UINT computeArgAlign(UINT argsz) const
    {
        #ifdef TO_BE_COMPATIBLE_WITH_ARM_LINUX_GNUEABI
        UINT align = argsz >= BYTESIZE_OF_DWORD ?
            BYTESIZE_OF_DWORD : STACK_ALIGNMENT;
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
    virtual bool isValidResultRegfile(OR_CODE orcode, INT resnum,
                                      REGFILE regfile) const;
    virtual bool isValidOpndRegfile(OR_CODE orcode, INT opndnum,
                                    REGFILE regfile) const;
    virtual bool isSPUnit(UNIT unit) const;
    virtual bool isIntRegSR(OR const* o, SR const* sr, UINT idx,
                            bool is_result) const;
    virtual bool isBusCluster(CLUST clust) const;
    virtual bool isBusSR(SR const* sr) const;
    virtual bool isRecalcOR(OR const* o) const;
    virtual bool isSameCluster(SLOT slot1, SLOT slot2) const;
    virtual bool isSameLikeCluster(SLOT slot1, SLOT slot2) const;
    virtual bool isSameLikeCluster(OR const* or1, OR const* or2) const;
    virtual bool isMultiResultOR(OR_CODE orcode, UINT res_num) const;
    virtual bool isMultiResultOR(OR_CODE orcode) const;
    virtual bool isMultiStore(OR_CODE orcode, INT opnd_num) const;
    virtual bool isMultiLoad(OR_CODE orcode, INT res_num) const;
    virtual bool isCopyOR(OR const* o) const;
    virtual bool isStackPointerValueEqu(SR const* base1, SR const* base2) const;
    virtual bool isSP(SR const* sr) const;
    virtual bool isReduction(OR const* o) const;
    virtual bool isEvenReg(Reg reg) const;
    virtual OR_CODE invertORCode(OR_CODE ot)
    {
        switch (ot) {
        case OR_add_i: return OR_sub_i;
        case OR_sub_i: return OR_add_i;
        default: UNREACHABLE();
        }
        return OR_UNDEF;
    }

    virtual OR_CODE mapIRCode2ORCode(IR_CODE ir_code, UINT ir_opnd_size,
                                     xoc::Type const* ir_type,
                                     IN SR * opnd0, IN SR * opnd1,
                                     bool is_signed);
    virtual UnitSet & mapRegFile2UnitSet(REGFILE regfile, SR const* sr,
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
    virtual CLUST mapReg2Cluster(Reg reg) const;
    virtual CLUST mapSR2Cluster(OR const* o, SR const* sr) const;

    virtual DataDepGraph * allocDDG();
    virtual LIS * allocLIS(ORBB * bb, DataDepGraph * ddg,
                           BBSimulator * sim, UINT sch_mode);
    virtual IR2OR * allocIR2OR();
    virtual BBSimulator * allocBBSimulator(ORBB * bb);
    RaMgr * allocRaMgr(List<ORBB*> * bblist, bool is_func);

    virtual void setSpadjustOffset(OR * spadj, INT size);

    //True if current argument register should be bypassed.
    virtual bool skipArgRegister(Var const* param, xgen::RegSet const& regset,
                                 Reg reg) const;

    //Interface to generate ORs to store physical register on top of stack.
    virtual void storeRegToStack(SR * reg, OUT ORList & ors);

    //Interface to generate ORs to reload physical register from top of stack.
    virtual void reloadRegFromStack(SR * reg, OUT ORList & ors);
};
//END ARMCG

#endif
