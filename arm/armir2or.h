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
#ifndef _ARMIR2OR_H_
#define _ARMIR2OR_H_

class ARMCG;

class ARMIR2OR : public IR2OR {
protected:
    void convertAddSubFp(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertMulofFloat(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertMulofLongLong(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertMulofInt(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertReturnValue(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertTruebrFp(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertStoreVar(
            IR const* ir,
            OUT ORList & ors,
            IN IOC * cont);
    void convertCvt(IR const* ir, OUT ORList & ors, IN IOC * cont);

    void getResultPredByIRTYPE(IR_TYPE code, SR ** truepd, SR ** falsepd);
    ARMCG * getCG() { return (ARMCG*)m_cg; }

    VAR const* fp2fp(IR const* tgt, IR const* src);
    VAR const* fp2int(IR const* tgt, IR const* src);

    VAR const* int2fp(IR const* tgt, IR const* src);
    void invertBoolValue(Dbx * dbx, SR * val, OUT ORList & ors);
public:
    ARMIR2OR(CG * cg) : IR2OR(cg) {}
    COPY_CONSTRUCTOR(ARMIR2OR);
    virtual ~ARMIR2OR() {}

    //Interface function.
    virtual void convertBinaryOp(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertLda(
            VAR const* var,
            HOST_INT lda_ofst,
            Dbx const* dbx,
            OUT ORList & ors,
            IN IOC * cont);
    virtual void convertLda(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertIcall(IR const* ir, OUT ORList & ors, IN IOC * cont)
    { convertCall(ir, ors, cont); }
    virtual void convertCall(IR const* ir,    OUT ORList & ors, IN IOC * cont);
    virtual void convertDiv(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertRem(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertMul(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertNeg(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertRelationOpFp(IR const* ir, OUT ORList & ors, IN IOC * cont);
    void convertRelationOpDWORD(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertRelationOp(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertIgoto(IR const* ir, OUT ORList & ors, IN IOC *);
    virtual void convertSelect(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertReturn(IR const* ir, OUT ORList & ors, IN IOC * cont);
    virtual void convertTruebr(IR const* ir, OUT ORList & ors, IN IOC * cont);

    virtual OR_TYPE mapIRType2ORType(
            IR_TYPE ir_type,
            UINT ir_opnd_size,
            IN SR * opnd0,
            IN SR * opnd1,
            bool is_signed);
};
#endif
