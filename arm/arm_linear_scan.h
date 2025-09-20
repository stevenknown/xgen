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
#ifndef _ARM_LINEAR_SCAN_H_
#define _ARM_LINEAR_SCAN_H_

class ARMRegSetImpl : public RegSetImpl {
    COPY_CONSTRUCTOR(ARMRegSetImpl);
protected:
    //Get the type of callee-save register.
    virtual Type const* getCalleeRegisterType(
        Reg r, TypeMgr * tm) const override
    {
        ASSERT0(isCallee(r) && tm != nullptr);
        return isCalleeScalar(r) ? tm->getTargMachRegisterType() :
            tm->getTargMachMaxVectorRegisterType();
    }

    //Return true if Type matches the register type.
    virtual bool isRegTypeMatch(Type const* ty, Reg r) const override
    {
        ASSERT0(ty->is_vector() || ty->is_int() || ty->is_fp() || ty->is_any());
        return (ty->is_vector() && isVector(r)) ||
            (!ty->is_vector() && (isCalleeScalar(r) || isCallerScalar(r)));
    }
    void initRegSet();
    virtual void initDebugRegSet();
public:
    ARMRegSetImpl(LinearScanRA & ra) : RegSetImpl(ra) { initRegSet(); }
};


//
//Start ARMLTConstraintsStrategy.
//
//This class is a concrete implementation of the lifetime constraint set
//strategy for the ARM architecture.
class ARMLTConstraintsStrategy : public LTConstraintsStrategy {
    COPY_CONSTRUCTOR(ARMLTConstraintsStrategy);
public:
    ARMLTConstraintsStrategy(LinearScanRA & ra) :
        LTConstraintsStrategy(ra) {}
    ~ARMLTConstraintsStrategy() {}

    //Set a constraint set for the lifetime of each PR
    //in each IR on the ARM architecture.
    virtual void applyConstraints(IR * ir) override {}
};
//End ARMLTConstraintsStrategy.


//
//Start ARMLTConstraints.
//
//The ARMLTConstraints class represents the lifetime constraint set
//specific to the ARM architecture. It works in conjunction with
//ARMLTConstraintsStrategy, which serves as the strategy for generating
//ARM-specific lifetime constraints. ARMLTConstraints acts as the
//representation of the constraint set, while ARMLTConstraintsStrategy
//defines how these constraints are produced.
//Some common constraint sets in the base class LTConstraints can be
//used directly. for details, please refer to LTConstraints.
class ARMLTConstraints : public LTConstraints {
    COPY_CONSTRUCTOR(ARMLTConstraints);
public:
    ARMLTConstraints() : LTConstraints() {}
    virtual ~ARMLTConstraints() {}
};
//End ARMLTConstraints.


//
//Start ARMLTConstraintsMgr.
//
class ARMLTConstraintsMgr : public LTConstraintsMgr {
    COPY_CONSTRUCTOR(ARMLTConstraintsMgr);
public:
    ARMLTConstraintsMgr() : LTConstraintsMgr() {}
    virtual ~ARMLTConstraintsMgr() {}
    virtual LTConstraints * allocLTConstraints() override;
};
//End ARMLTConstraintsMgr.


class ARMLinearScanRA : public LinearScanRA {
    COPY_CONSTRUCTOR(ARMLinearScanRA);
protected:
    virtual RegSetImpl * allocRegSetImpl() { return new ARMRegSetImpl(*this); }

    //Allocate memory for the lifetime constraint set strategy
    //for ARM architecture.
    virtual LTConstraintsStrategy * allocLTConstraintsStrategy() override
    { return new ARMLTConstraintsStrategy(*this); }

    virtual LTConstraintsMgr * allocLTConstraintsMgr() override
    { return new ARMLTConstraintsMgr(); }
public:
    ARMLinearScanRA(Region * rg) : LinearScanRA(rg) {}
    virtual ~ARMLinearScanRA() {}

    virtual bool isTmpRegAvailable(Type const* ty) const override
    {
        ASSERT0(ty);
        return getTempScalar(ty) != REG_UNDEF &&
            (ty->is_scalar() || ty->is_pointer() || ty->is_any());
    }

};

#endif
