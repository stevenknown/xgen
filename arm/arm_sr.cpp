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

CHAR const* ARMIntSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    ASSERT0(getCode()== SR_INT_IMM);
    buf.strcat("#");
    return IntSR::getAsmName(buf, cg);
}


CHAR const* ARMVarSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    ASSERT0(getCode()== SR_VAR);
    if (SR_var_ofst(this) != 0) {
        buf.strcat("%s+%d", SYM_name(SR_var(this)->get_name()),
                   SR_var_ofst(this));
        return buf.buf;
    }
    buf.strcat("%s", SYM_name(SR_var(this)->get_name()));
    return buf.buf;
}


CHAR const* ARMRegSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    ASSERT0(getCode()== SR_REG);
    if (is_sp()) {
        buf.strcat("sp");
        return buf.buf;
    }
    return RegSR::getAsmName(buf, cg);
}


CHAR const* ARMRegSR::get_name(StrBuf & buf, CG const* cg) const
{
    ASSERT0(getCode() == SR_REG);
    ////Print physical register id and register file.
    //if (getPhyReg() != REG_UNDEF) {
    //    buf.strcat("(%s)", tmGetRegName(getPhyReg()));
    //}
    //if (getRegFile() != RF_UNDEF) {
    //    buf.strcat("(%s)", tmGetRegFileName(SR_regfile(this)));
    //}
    return RegSR::get_name(buf, cg);
}


//
//START ARMSRMgr
//
SR * ARMSRMgr::allocSR(SR_CODE c)
{
    switch (c) {
    case SR_REG: return new ARMRegSR();
    case SR_INT_IMM: return new ARMIntSR();
    case SR_VAR: return new ARMVarSR();
    default: return SRMgr::allocSR(c);
    }
    return nullptr;
}
//END ARMSRMgr
