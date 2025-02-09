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
//
class ARMTargInfoMgr : public TargInfoMgr {
    COPY_CONSTRUCTOR(ARMTargInfoMgr);
protected:
    xgen::RegSet m_allocable_scalar;
    xgen::RegSet m_allocable_vector;
    xgen::RegSet m_callee_scalar;
    xgen::RegSet m_callee_vector;
    xgen::RegSet m_caller_scalar;
    xgen::RegSet m_caller_vector;
    xgen::RegSet m_param_scalar;
    xgen::RegSet m_param_vector;
    xgen::RegSet m_retval_scalar;
    xgen::RegSet m_retval_vector;
protected:
    virtual void initAllocableScalar();
    virtual void initAllocableVector();
    virtual void initCalleeScalar();
    virtual void initCalleeVector();
    virtual void initCallerScalar();
    virtual void initCallerVector();
    virtual void initParamScalar();
    virtual void initParamVector();
    virtual void initRetvalScalar();
    virtual void initRetvalVector();
public:
    ARMTargInfoMgr() { init(); }
    virtual ~ARMTargInfoMgr() {}

    //Get scalar allocable register set of T1 architecture.
    xgen::RegSet const* getAllocableScalarRegSet() const
    { return &m_allocable_scalar; }

    //Get vector allocable register set of T1 architecture.
    xgen::RegSet const* getAllocableVectorRegSet() const
    { return &m_allocable_vector; }

    //Get base pointer register of T1 architecture.
    virtual xgen::Reg getBP() const { return REG_R9; }

    //Get scalar callee saved register set of T1 architecture.
    xgen::RegSet const* getCalleeScalarRegSet() const
    { return &m_callee_scalar; }

    //Get vector callee saved register set of T1 architecture.
    xgen::RegSet const* getCalleeVectorRegSet() const
    { return &m_callee_vector; }

    //Get end scalar caller saved register of T1 architecture.
    virtual xgen::Reg getCallerScalarEnd() const { return REG_R8; }

    //Get scalar caller saved register set of T1 architecture.
    xgen::RegSet const* getCallerScalarRegSet() const
    { return &m_caller_scalar; }

    //Get start scalar caller saved register of T1 architecture.
    virtual xgen::Reg getCallerScalarStart() const { return REG_R1; }

    //Get vector caller saved register set of T1 architecture.
    xgen::RegSet const* getCallerVectorRegSet() const override
    { return &m_caller_vector; }

    //Get the last physical register of the architecture.
    virtual xgen::Reg getRegLast() const override { return REG_LAST; }

    //Get frame pointer register of T1 architecture.
    virtual xgen::Reg getFP() const override { return REG_R15; }

    //Get global pointer register of T1 architecture.
    virtual xgen::Reg getGP() const override { return REG_UNDEF; }

    //Get number of registers of T1 architecture.
    virtual UINT const getNumOfRegister() const { return 65; }

    //Get scalar parame register set of T1 architecture.
    xgen::RegSet const* getParamScalarRegSet() const
    { return &m_param_scalar; }

    //Get start scalar parameter register of ARM architecture.
    virtual xgen::Reg getParamScalarStart() const { return REG_R0; }

    //Get vector parameter register set of T1 architecture.
    xgen::RegSet const* getParamVectorRegSet() const
    { return &m_param_vector; }

    //Get program counter register of T1 architecture.
    virtual xgen::Reg getPC() const { return REG_PC; }

    //Get return address of ARM architecture.
    virtual xgen::Reg getRA() const { return REG_RA; }

    //Get scalar allocable register set of T1 architecture.
    xgen::RegSet const* getRetvalScalarRegSet() const
    { return &m_retval_scalar; }

    //Get vector returned value register set of T1 architecture.
    xgen::RegSet const* getRetvalVectorRegSet() const
    { return &m_retval_vector; }

    //Get stack pointer register of T1 architecture.
    virtual xgen::Reg getSP() const { return REG_SP; }

    //Get temporary register of T1 architecture.
    virtual xgen::Reg getTemp() const { return REG_TMP; }

    //Get zero register of T1 architecture.
    virtual xgen::Reg getZero() const { return REG_ZERO; }
};
//End ARMTargInfoMgr.

#endif
