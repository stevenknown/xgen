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
#include "xgeninc.h"

namespace xgen {

//Return symbol register name and info, used by tracing routines.
//buf: output string buffer.
CHAR const* IntSR::get_name(StrBuf & buf, CG const* cg) const
{
    buf.strcat("#%lld", (LONGLONG)getInt());
    return buf.buf;
}


CHAR const* FPSR::get_name(StrBuf & buf, CG const* cg) const
{
    buf.strcat("#%f", SR_fp_imm(this));
    return buf.buf;
}


CHAR const* VarSR::get_name(StrBuf & buf, CG const* cg) const
{
    ASSERT0(SR_var(this));
    ASSERT0(SR_var(this)->get_name());
    CHAR const* name = SR_var(this)->get_name()->getStr();
    ASSERT0(SR_var_ir(this) == nullptr || SR_var_ir(this)->is_id());
    buf.strcat("'%s'", name);
    if (SR_var_ir(this) != nullptr) {
        TMWORD ofst = SR_var_ir(this)->getOffset();
        if (ofst != 0) {
            buf.strcat("+%u", ofst);
        }
    }
    if (SR_var_ofst(this) != 0) {
        buf.strcat("+%u", SR_var_ofst(this));
    }
    return buf.buf;
}


CHAR const* RegSR::get_name(StrBuf & buf, CG const* cg) const
{
    if (is_sp()) {
        if (this == cg->getSP()) {
            buf.strcat("sp");
        } else {
            UNREACHABLE();
        }
    } else if (is_fp()) {
        if (this == cg->getFP()) {
            buf.strcat("fp");
        } else {
            UNREACHABLE();
        }
    } else if (is_gp()) {
        if (this == cg->getGP()) {
            buf.strcat("gp");
        } else {
            UNREACHABLE();
        }
    } else if (is_rflag()) {
        //Rflag register
        buf.strcat("rflag");
    } else if (is_param_pointer()) {
        //Parameter pointer.
        buf.strcat("sr%lu(PARAM)", (ULONG)getSymReg());
    } else if (HAS_PREDICATE_REGISTER && this == cg->getTruePred()) {
        //True predicate register.
        buf.strcat("tp");
    } else {
        //General Purpose Register
        if (is_global()) {
            buf.strcat("gsr%lu", (ULONG)getSymReg());
        } else {
            buf.strcat("sr%lu", (ULONG)getSymReg());
        }
    }

    //Print physical register and physical register file.
    if (getPhyReg() != REG_UNDEF) {
        buf.strcat("(%s)", xgen::tmGetRegName((Reg)getPhyReg()));
    }
    if (getRegFile() != RF_UNDEF) {
        buf.strcat("(%s)", xgen::tmGetRegFileName(getRegFile()));
    }
    return buf.buf;
}


CHAR const* StrSR::get_name(StrBuf & buf, CG const* cg) const
{
    CHAR const* s = SR_str(this)->getStr();
    buf.strcat(MAX_SR_NAME_BUF_LEN, "\"%s\"", s);
    return buf.buf;
}


CHAR const* LabSR::get_name(StrBuf & buf, CG const* cg) const
{
    ASSERT0(cg);
    cg->formatLabelName(SR_label(this), buf);
    return buf.buf;
}


CHAR const* LabListSR::get_name(StrBuf & buf, CG const* cg) const
{
    ASSERT0(cg);
    cg->formatLabelListString(SR_label_list(this), buf);
    return buf.buf;
}


//Return SR name during print assembly file.
CHAR const* IntSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    CHAR const* strformatx = "0x%x";
    CHAR const* strformatd = "%d";
    if (GENERAL_REGISTER_SIZE == sizeof(INT)) {
        strformatx = "0x%x";
        strformatd = "%d";
    } else if (GENERAL_REGISTER_SIZE == sizeof(LONG)) {
        strformatx = "0x%lx";
        strformatd = "%ld";
    } else if (GENERAL_REGISTER_SIZE == sizeof(LONGLONG)) {
        strformatx = "0x%llx";
        strformatd = "%lld";
    }
    if (getInt() > THRESHOLD_DISPLAY_IN_HEX) {
        buf.strcat(strformatx, getInt());
    } else {
        buf.strcat(strformatd, getInt());
    }
    return buf.buf;
}


CHAR const* FPSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    buf.strcat("%f", SR_fp_imm(this));
    return buf.buf;
}


CHAR const* VarSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    if (SR_var_ofst(this) != 0) {
        buf.strcat("%s#+%d", SYM_name(SR_var(this)->get_name()),
                   SR_var_ofst(this));
        return buf.buf;
    }
    buf.strcat("%s#", SYM_name(SR_var(this)->get_name()));
    return buf.buf;
}


CHAR const* RegSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    return xgen::tmGetRegName(getPhyReg());
}


CHAR const* StrSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    return SYM_name(SR_str(this));
}


CHAR const* LabSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    ASSERT0(cg);
    return cg->formatLabelName(SR_label(this), buf);
}


CHAR const* LabListSR::getAsmName(StrBuf & buf, CG const* cg) const
{
    //LabelList string is not necessary to asm file.
    return nullptr;
}

} //namespace xgen
