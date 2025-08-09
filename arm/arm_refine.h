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
#ifndef _ARM_REFINE_H_
#define _ARM_REFINE_H_

namespace xoc {
class CalcDerivative;
};
class ARMDerivative;

class ARMRefine : public Refine {
protected:
    xoc::CalcDerivative * m_cd;
protected:
    IR * foldConstRelu(IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstConv(IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstConvOpndGrad(IR * ir, bool & change, RefineCtx & rc);
    IR * foldConstBinaryTensorByTensor(
        IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstBinaryTensorByScalar(
        IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstBinaryScalarByTensor(
        IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstTrigonometricScalar(
        IR * ir, bool & change, RefineCtx & rc);
    IR * foldConstTrigonometricTensor(
        IR * ir, bool & change, RefineCtx const& rc);
    virtual IR * foldConstExtExp(
        IR * ir, bool & change, RefineCtx & rc) override;
    virtual IR * foldConstUnary(
        IR * ir, bool & change, RefineCtx &) override;
    virtual IR * foldConstBinary(
        IR * ir, bool & change, RefineCtx & rc) override;
    IR * foldConstTrigonometric(IR * ir, bool & change, RefineCtx & rc);
    IR * foldConstBinaryTensor(IR * ir, bool & change, RefineCtx const& rc);
    IR * foldConstUnaryTensor(IR * ir, bool & change, RefineCtx const& rc);

    xoc::CalcDerivative * getCalcDer() const { return m_cd; }

    IR * refineBroadCast(IR * ir, bool & change, RefineCtx & rc);
    IR * refineConvOpndGrad(IR * ir, bool & change, RefineCtx & rc);
    IR * refineConv(IR * ir, bool & change, RefineCtx & rc);
    IR * refineRelu(IR * ir, bool & change, RefineCtx & rc);
    virtual IR * refineExtOp(IR * ir, bool & change, RefineCtx & rc) override;
    IR * refineTrigonometricTensor(IR * ir, bool & change, RefineCtx & rc);
    virtual IR * refineTrigonometric(
        IR * ir, bool & change, RefineCtx & rc) override;
public:
    ARMRefine(Region * rg) : Refine(rg), m_cd(nullptr) {}
    virtual ~ARMRefine() {}

    void initCalcDer(MOD OptCtx & oc);

    virtual bool perform(OptCtx & oc) override;
};

#endif
