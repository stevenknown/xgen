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

#include "../xgen/xgeninc.h"

void ARMRegSetImpl::initDebugRegSet()
{
    //Target Dependent Code.
    #define SCALAR_CALLEE_SAVED_REG_START REG_R4
    #define SCALAR_CALLEE_SAVED_REG_END REG_R5
    m_target_callee_scalar = nullptr;
    m_target_caller_scalar = nullptr;
    m_target_param_scalar = nullptr;
    m_target_return_value_scalar = nullptr;
    m_target_callee_vector = nullptr;
    m_target_caller_vector = nullptr;
    m_target_param_vector = nullptr;
    m_target_return_value_vector = nullptr;
    m_target_allocable_scalar = nullptr;
    m_target_allocable_vector = nullptr;

    //User can customize the number of register by the option '-debug_lsra'
    //and '-debug_reg_num', the first one the switch for debug mode, and the
    //later option is the number for the physical register.
    m_target_caller_scalar = new RegSet();
    m_target_allocable_scalar = new RegSet();
    UINT caller_start = SCALAR_CALLEE_SAVED_REG_START;
    UINT caller_end = (caller_start + g_debug_reg_num - 1);
    caller_end = MIN(caller_end, SCALAR_CALLEE_SAVED_REG_END);
    for (UINT i = caller_start; i <= caller_end; i++) {
        const_cast<RegSet*>(m_target_caller_scalar)->bunion(i);
    }

    //Set the allocable registers for scalar.
    const_cast<RegSet*>(m_target_allocable_scalar)->bunion(
        *m_target_caller_scalar);
}


void ARMRegSetImpl::initRegSet()
{
    //Do more initializations for ARM targinfo.
    if (getTIMgr().getCalleeScalarRegSet() != nullptr) {
        m_avail_allocable.bunion(*getTIMgr().getCalleeScalarRegSet());
    }
    if (getTIMgr().getCalleeVectorRegSet() != nullptr) {
        m_avail_allocable.bunion(*getTIMgr().getCalleeVectorRegSet());
    }
    if (getTIMgr().getCallerScalarRegSet() != nullptr) {
        m_avail_allocable.bunion(*getTIMgr().getCallerScalarRegSet());
    }
    if (getTIMgr().getCallerVectorRegSet() != nullptr) {
        m_avail_allocable.bunion(*getTIMgr().getCallerVectorRegSet());
    }
}


//
//Start ARMLTConstraintsMgr.
//
LTConstraints * ARMLTConstraintsMgr::allocLTConstraints()
{
    LTConstraints * lt_constraints = new ARMLTConstraints();
    ASSERT0(lt_constraints);
    m_ltc_list.append_tail(lt_constraints);
    return lt_constraints;
}
//End ARMLTConstraintsMgr.
