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
@*/
#ifndef _ARM_TARGINFO_MGR_H_
#define _ARM_TARGINFO_MGR_H_

//
//Start ARMTargInfoMgr
//
class ARMTargInfoMgr : public xoc::TargInfoMgr {
    COPY_CONSTRUCTOR(ARMTargInfoMgr);
protected:
    xgen::RegSet m_allocable_scalar;
    xgen::RegSet m_allocable_vector;
    xgen::RegSet m_callee_scalar;
    xgen::RegSet m_callee_vector;
    xgen::RegSet m_caller_scalar;
    xgen::RegSet m_caller_vector;
    xgen::RegSet m_caller;
    xgen::RegSet m_param_scalar;
    xgen::RegSet m_param_vector;
    xgen::RegSet m_retval_scalar;
    xgen::RegSet m_retval_vector;
protected:
    virtual RegDSystem * allocRegDSystem() override;

    virtual void destroy() override;

    virtual UINT getBitSize(Reg r) const override;

    virtual void initAllocableScalar() override;
    virtual void initAllocableVector() override;
    virtual void initCalleeScalar() override;
    virtual void initCalleeVector() override;
    virtual void initCallerScalar() override;
    virtual void initCallerVector() override;
    virtual void initParamScalar() override;
    virtual void initParamVector() override;
    virtual void initRetvalScalar() override;
    virtual void initRetvalVector() override;
    virtual void initCaller() override;
    virtual void initCallee() override;
public:
    ARMTargInfoMgr(RegionMgr const* rm) : TargInfoMgr(rm) {}
    virtual ~ARMTargInfoMgr() {}

    //Get scalar allocable register set.
    xgen::RegSet const* getAllocableScalarRegSet() const
    { return &m_allocable_scalar; }

    //Get vector allocable register set.
    xgen::RegSet const* getAllocableVectorRegSet() const
    { return &m_allocable_vector; }

    //Get scalar callee saved register set.
    xgen::RegSet const* getCalleeScalarRegSet() const
    { return &m_callee_scalar; }

    //Get vector callee saved register set.
    xgen::RegSet const* getCalleeVectorRegSet() const
    { return &m_callee_vector; }

    //Get end scalar caller saved register.
    virtual xgen::Reg getCallerScalarEnd() const override { return REG_R8; }

    //Get scalar caller saved register set.
    xgen::RegSet const* getCallerScalarRegSet() const override
    { return &m_caller_scalar; }

    //Get start scalar caller saved register.
    virtual xgen::Reg getCallerScalarStart() const override { return REG_R1; }

    //Get vector caller saved register set.
    xgen::RegSet const* getCallerVectorRegSet() const override
    { return &m_caller_vector; }

    virtual xgen::RegSet const* getCallerRegSet() const override
    { return &m_caller; }

    //Get the last physical register of the architecture.
    virtual xgen::Reg getRegLast() const override { return REG_LAST; }

    //Get frame pointer register.
    virtual xgen::Reg getFP() const override { return REG_R15; }

    //Get global pointer register.
    virtual xgen::Reg getGP() const override { return REG_UNDEF; }

    //Get number of registers.
    virtual UINT const getNumOfRegister() const override { return REG_NUM; }

    //Get scalar parame register set.
    xgen::RegSet const* getParamScalarRegSet() const override
    { return &m_param_scalar; }

    //Get start scalar parameter register of ARM architecture.
    virtual xgen::Reg getParamScalarStart() const override { return REG_R0; }

    //Get vector parameter register set.
    xgen::RegSet const* getParamVectorRegSet() const
    { return &m_param_vector; }

    //Get program counter register.
    virtual xgen::Reg getPC() const override { return REG_PC; }

    //Get return address of ARM architecture.
    virtual xgen::Reg getRA() const override { return REG_RA; }

    //Get scalar allocable register set.
    xgen::RegSet const* getRetvalScalarRegSet() const
    { return &m_retval_scalar; }

    //Get vector returned value register set.
    xgen::RegSet const* getRetvalVectorRegSet() const
    { return &m_retval_vector; }

    //Get stack pointer register.
    virtual xgen::Reg getSP() const override { return REG_SP; }

    //Get temporary register.
    virtual xgen::Reg getTempScalar(Type const* ty) const override
    {  ASSERT0(!ty->is_vector()); return REG_TMP; }

    virtual xgen::Reg getTempVector() const override
    { return REG_UNDEF; }

    //Get temporary register.
    virtual Reg getZeroScalar() const override { return REG_ZERO; }
    virtual Reg getZeroScalarFP() const override { return REG_UNDEF; }
    virtual Reg getZeroVector() const override { return REG_UNDEF; }

    //Get the cycle count of load operation on chip memory.
    virtual UINT getLoadOnChipMemCycle() const override
    { return ARM_LOAD_ONCHIP_CYC; }

    //Get the cycle count of write operation on chip memory.
    virtual UINT getStoreOnChipMemCycle() const override { return 1; }
};
//End ARMTargInfoMgr.

#endif
