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

//
//START ARMSR
//
//Return SR name during print assembly file.
CHAR const* ARMSR::getAsmName(StrBuf & buf, CG * cg)
{
    switch (SR_type(this)) {
    case SR_INT_IMM:
        buf.strcat("#");
        return SR::getAsmName(buf, cg);
    case SR_VAR:
        if (SR_var_ofst(this) != 0) {
            buf.strcat("%s+%d", SYM_name(SR_var(this)->get_name()),
                       SR_var_ofst(this));
            return buf.buf;
        } else {
            buf.strcat("%s", SYM_name(SR_var(this)->get_name()));
            return buf.buf;
        }
    case SR_REG:
        if (is_sp()) {
            buf.strcat("sp");
            return buf.buf;
        }
        //fall through
    default:
        return SR::getAsmName(buf, cg);
    }
    return nullptr;
}


//Return symbol register name and info, used by tracing routines.
//'buf': output string buffer.
CHAR const* ARMSR::get_name(StrBuf & buf, CG * cg) const
{
    switch (SR_type(this)) {
    case SR_REG: {
        return SR::get_name(buf, cg);

        //Print physical register id and register file.
        if (SR_phy_reg(this) != REG_UNDEF) {
            buf.strcat("(%s)", tmGetRegName(SR_phy_reg(this)));
        }
        if (SR_regfile(this) != RF_UNDEF) {
            buf.strcat("(%s)", tmGetRegFileName(SR_regfile(this)));
        }
        break;
    }
    default: return SR::get_name(buf, cg);
    }
    return buf.buf;
}
//END ARMSR
