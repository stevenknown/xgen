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
#include "../xgen/xgeninc.h"

void ARMTargInfoMgr::initAllocableScalar()
{
    for (Reg i = ALLOCABLE_REG_START; i <= ALLOCABLE_REG_END; i++) {
        m_allocable_scalar.bunion(i);
    }
    //R14(LR), note R14 should be saved at prolog of current function.
    m_allocable_scalar.bunion(REG_RETURN_ADDRESS_REGISTER);
}


void ARMTargInfoMgr::initAllocableVector()
{
    for (Reg i = ALLOCABLE_VEC_REG_D_START;
         i <= ALLOCABLE_VEC_REG_D_END; i++) {
        m_allocable_vector.bunion(i);
    }
    for (Reg i = ALLOCABLE_VEC_REG_Q_START;
         i <= ALLOCABLE_VEC_REG_Q_END; i++) {
        m_allocable_vector.bunion(i);
    }
    for (Reg i = ALLOCABLE_VEC_REG_S_START;
         i <= ALLOCABLE_VEC_REG_S_END; i++) {
        m_allocable_vector.bunion(i);
    }
}


void ARMTargInfoMgr::initCalleeScalar()
{
    for (Reg reg = CALLEE_SAVED_REG_START; reg <= CALLEE_SAVED_REG_END; reg++) {
        m_callee_scalar.bunion(reg);
    }
    //R14(LR)
    m_callee_scalar.bunion(REG_RETURN_ADDRESS_REGISTER);
}


void ARMTargInfoMgr::initCalleeVector()
{
}


void ARMTargInfoMgr::initCallerScalar()
{
    for (Reg reg = CALLER_SAVED_REG_START; reg <= CALLER_SAVED_REG_END; reg++) {
        m_caller_scalar.bunion(reg);
    }
}


void ARMTargInfoMgr::initCallerVector()
{
    for (Reg i = ALLOCABLE_VEC_REG_D_START;
         i <= ALLOCABLE_VEC_REG_D_END; i++) {
        m_caller_vector.bunion(i);
    }
    for (Reg i = ALLOCABLE_VEC_REG_Q_START;
         i <= ALLOCABLE_VEC_REG_Q_END; i++) {
        m_caller_vector.bunion(i);
    }
    for (Reg i = ALLOCABLE_VEC_REG_S_START;
         i <= ALLOCABLE_VEC_REG_S_END; i++) {
        m_caller_vector.bunion(i);
    }
}


void ARMTargInfoMgr::initParamScalar()
{
    for (Reg reg = ARG_REG_START; reg <= ARG_REG_END; reg++) {
        m_param_scalar.bunion(reg);
    }
}


void ARMTargInfoMgr::initParamVector()
{
    m_param_vector.bunion(REG_D0);
    m_param_vector.bunion(REG_D1);
}


void ARMTargInfoMgr::initRetvalScalar()
{
    for (Reg reg = RETVAL_REG_START; reg <= RETVAL_REG_END; reg++) {
        m_retval_scalar.bunion(reg);
    }
}


void ARMTargInfoMgr::initRetvalVector()
{
    m_retval_vector.bunion(REG_D0);
    m_retval_vector.bunion(REG_D1);
}

