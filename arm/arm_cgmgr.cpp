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
#include "../xgen/xgeninc.h"

static BuiltinDesc g_arm_builtin_desc [] = {
    { BUILTIN_UIMOD, "__aeabi_uidivmod" },
    { BUILTIN_IMOD, "__aeabi_idivmod" },
    { BUILTIN_UIDIV, "__aeabi_uidiv" },
    { BUILTIN_ASHLDI3, "__ashldi3" },
    { BUILTIN_LSHRDI3, "__lshrdi3" },
    { BUILTIN_ASHRDI3, "__ashrdi3" },
    { BUILTIN_MODSI3, "__modsi3" },
    { BUILTIN_UMODSI3, "__umodsi3" },
    { BUILTIN_MODDI3, "__moddi3" },
    { BUILTIN_UMODDI3, "__umoddi3" },
    { BUILTIN_ADDSF3, "__addsf3" },
    { BUILTIN_ADDDF3, "__adddf3" },
    { BUILTIN_SUBSF3, "__subsf3" },
    { BUILTIN_SUBDF3, "__subdf3" },
    { BUILTIN_DIVSI3, "__divsi3" },
    { BUILTIN_UDIVSI3, "__udivsi3" },
    { BUILTIN_DIVSF3, "__divsf3" },
    { BUILTIN_DIVDI3, "__divdi3" },
    { BUILTIN_UDIVDI3, "__udivdi3" },
    { BUILTIN_DIVDF3, "__divdf3" },
    { BUILTIN_MULDI3, "__muldi3" },
    { BUILTIN_MULSF3, "__mulsf3" },
    { BUILTIN_MULDF3, "__muldf3" },
    { BUILTIN_LTSF2, "__ltsf2" },
    { BUILTIN_GTSF2, "__gtsf2" },
    { BUILTIN_GESF2, "__gesf2" },
    { BUILTIN_EQSF2, "__eqsf2" },
    { BUILTIN_NESF2, "__nesf2" },
    { BUILTIN_LESF2, "__lesf2" },
    { BUILTIN_LTDF2, "__ltdf2" },
    { BUILTIN_GTDF2, "__gtdf2" },
    { BUILTIN_GEDF2, "__gedf2" },
    { BUILTIN_EQDF2, "__eqdf2" },
    { BUILTIN_NEDF2, "__nedf2" },
    { BUILTIN_LEDF2, "__ledf2" },
    { BUILTIN_FIXSFSI, "__fixsfsi" },
    { BUILTIN_FIXDFSI, "__fixdfsi" },
    { BUILTIN_FIXUNSSFSI, "__fixunssfsi" },
    { BUILTIN_FIXUNSDFSI, "__fixunsdfsi" },
    { BUILTIN_FIXUNSSFDI, "__fixunssfdi" },
    { BUILTIN_FIXUNSDFDI, "__fixunsdfdi" },
    { BUILTIN_TRUNCDFSF2, "__truncdfsf2" },
    { BUILTIN_FLOATSISF, "__floatsisf" },
    { BUILTIN_FLOATDISF, "__floatdisf" },
    { BUILTIN_FLOATSIDF, "__floatsidf" },
    { BUILTIN_FLOATDIDF, "__floatdidf" },
    { BUILTIN_FIXSFDI, "__fixsfdi" },
    { BUILTIN_FIXDFDI, "__fixdfdi" },
    { BUILTIN_FLOATUNSISF, "__floatunsisf" },
    { BUILTIN_FLOATUNDISF, "__floatundisf" },
    { BUILTIN_FLOATUNSIDF, "__floatunsidf" },
    { BUILTIN_FLOATUNDIDF, "__floatundidf" },
    { BUILTIN_EXTENDSFDF2, "__extendsfdf2" },
};

UINT g_arm_builtin_num = sizeof(g_arm_builtin_desc) /
                         sizeof(g_arm_builtin_desc[0]);

//Allocate CG.
CG * ARMCGMgr::allocCG(Region * rg)
{
    return new ARMCG(rg, this);
}


//Allocate VarMgr.
AsmPrinter * ARMCGMgr::allocAsmPrinter(CG const* cg)
{
    return new ARMAsmPrinter(cg, getAsmPrtMgr());
}


void ARMCGMgr::initBuiltin()
{
    for (UINT i = 0; i < g_arm_builtin_num; i++) {
        m_builtin_var.set(g_arm_builtin_desc[i].type,
                          addBuiltinVar(g_arm_builtin_desc[i].name));
    }
}



