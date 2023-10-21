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
#ifndef _ARMIR2OR_H_
#define _ARMIR2OR_H_

class ARMCG;
class ARMCGMgr;

class ARMIR2OR : public IR2OR {
protected:
    void convertAddSubFP(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    void convertMulofFloat(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    void convertMulofLongLong(IR const* ir, OUT RecycORList & ors,
                              IN IOC * cont);
    void convertMulofInt(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertReturnValue(IR const* ir, OUT RecycORList & ors,
                                    IN IOC * cont);
    void convertTruebrFP(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertStoreVar(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont);
    //CASE: integer and pointer convertion.
    void convertCvtIntAndPtr(IR const* ir, OUT RecycORList & ors,
                             MOD IOC * cont);
    //CASE: Load constant-string address into register.
    void convertCvtIntAndStr(IR const* ir, SR * opnd,
                             OUT RecycORList & ors,
                             MOD IOC * cont);
    void convertCvt(IR const* ir, OUT RecycORList & ors, IN IOC * cont);

    //Convert Bitwise NOT into OR list. bnot is unary operation.
    //e.g BNOT(0x0001) = 0xFFFE
    //Note ONLY thumb supports ORN logical OR NOT operation.
    //This implementation is just used to verify the function of BNOT,
    //in the sake of excessive redundant operations has been generated.
    void convertBitNotLowPerformance(IR const* ir, OUT RecycORList & ors,
                                     IN IOC * cont);
    void convertRelationOpDWORDForEquality(IR const* ir, SR * sr0, SR * sr1,
                                           bool is_signed,
                                           OUT RecycORList & ors,
                                           OUT RecycORList & tors,
                                           IN IOC * cont);

    //Info about ARM Conditions Flag that need to know.
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
    void convertRelationOpDWORDForLTandGE(IR const* ir, SR * sr0, SR * sr1,
                                          bool is_signed,
                                          OUT RecycORList & ors,
                                          OUT RecycORList & tors,
                                          IN IOC * cont);
    void convertRelationOpDWORDForLEandGT(IR const* ir, SR * sr0, SR * sr1,
                                          bool is_signed,
                                          OUT RecycORList & ors,
                                          OUT RecycORList & tors,
                                          IN IOC * cont);
    void convertRelationOpDWORDForLTGELEGT(IR const* ir,
                                           SR * sr0_l, SR * sr0_h,
                                           SR * sr1_l, SR * sr1_h,
                                           bool is_signed,
                                           OUT RecycORList & ors,
                                           OUT RecycORList & tors,
                                           IN IOC * cont);

    void getResultPredByIRCode(IR_CODE code, SR ** truepd, SR ** falsepd,
                               bool is_signed);
    ARMCG * getCG() { return (ARMCG*)m_cg; }
    ARMCGMgr * getCGMgr() { return (ARMCGMgr*)m_cg->getCGMgr(); }

    Var const* fp2fp(IR const* tgt, IR const* src);
    Var const* fp2int(IR const* tgt, IR const* src);

    Var const* int2fp(IR const* tgt, IR const* src);
    void invertBoolValue(Dbx * dbx, SR * val, OUT RecycORList & ors);
    IR * insertCvt(IR const* ir);
public:
    ARMIR2OR(CG * cg) : IR2OR(cg) {}
    COPY_CONSTRUCTOR(ARMIR2OR);
    virtual ~ARMIR2OR() {}

    //Interface function.
    virtual void convertBinaryOp(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont);
    virtual void convertLda(Var const* var, HOST_INT lda_ofst,
                            Dbx const* dbx, OUT RecycORList & ors,
                            IN IOC * cont);
    virtual void convertLda(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertICall(IR const* ir, OUT RecycORList & ors,
                              IN IOC * cont)
    { convertCall(ir, ors, cont); }
    virtual void convertDiv(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertRem(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertMul(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertNeg(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    void convertRelationOpFP(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont);
    //Generate compare operations and return the comparation result registers.
    // e.g:
    //     a - 1 > b + 2
    // =>
    //     sr0 = a - 1
    //     sr1 = b + 2
    //     res, truepd, falsepd <- cmp sr0, sr1
    //     return res, truepd, falsepd
    void convertRelationOpDWORD(IR const* ir, OUT RecycORList & ors,
                                IN IOC * cont);
    //Generate compare operations and return the comparation result registers.
    //The output registers in IOC are ResultSR, TruePredicatedSR, and
    //FalsePredicatedSR.
    //The ResultSR records the boolean value of comparison of
    //relation operation.
    // e.g:
    //     a - 1 > b + 2
    // =>
    //     sr0 = a - 1
    //     sr1 = b + 2
    //     res, truepd, falsepd <- cmp sr0, sr1
    //     return res, truepd, falsepd
    virtual void convertRelationOp(IR const* ir, OUT RecycORList & ors,
                                   IN IOC * cont);

    //Bitwise NOT.
    //e.g BNOT(0x0001) = 0xFFFE
    //Note ONLY thumb supports ORN logical OR NOT operation.
    virtual void convertBitNot(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont);
    virtual void convertIgoto(IR const* ir, OUT RecycORList & ors, IN IOC *);
    virtual void convertSelect(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont);
    virtual void convertReturn(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont);
    virtual void convertTruebr(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont);
    virtual void convertSetElem(IR const* ir, OUT RecycORList & ors,
                                MOD IOC * cont);
    virtual void convertGetElem(IR const* ir, OUT RecycORList & ors,
                                MOD IOC * cont);
    virtual void convert(IR const* ir, OUT RecycORList & ors, IN IOC * cont);

    void recordRelationOpResult(IR const* ir,
                                SR * truepd,
                                SR * falsepd,
                                SR * result_pred,
                                OUT RecycORList & ors,
                                MOD IOC * cont);
};
#endif
