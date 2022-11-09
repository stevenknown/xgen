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
#ifndef __EQU_OR_CODE_H__
#define __EQU_OR_CODE_H__

namespace xgen {

//The equivalent OR codes are same in utilities for
//different function-unit in hardware.
//e.g:Assume machine has three independent function units M, I, O, and each
//    unit supplies several operations which implement same function.
//    They are add_m, add_i, add_o.
//    The equivalent group is [add_m, add_i, add_o] for each add_m, add_i,
//    add_o operations.
#define EQUORC_unit2orcode(e, u) ((e)->m_func_unit2orcode[u])
#define EQUORC_num_equorcode(e) ((e)->m_num_of_equorcode)

typedef struct EquORCodes {
    //Record the number of Equal OR code.
    UINT m_num_of_equorcode;

    //Record Equal ORCode.
    OR_CODE m_func_unit2orcode[UNIT_NUM];

    void init()
    {
        m_num_of_equorcode = 0;
        for (UINT i = 0; i < UNIT_NUM; i++) {
            m_func_unit2orcode[i] = OR_UNDEF;
        }
    }
} EquORCodes;

} //namespace xgen
#endif
