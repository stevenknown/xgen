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
#include "xgeninc.h"

namespace xgen {

#ifdef _DEBUG_
SR const* checkSRVAR(SR const* sr)
{
    ASSERT0(sr->is_var());
    return sr;
}


SR const* checkSRLAB(SR const* sr)
{
    ASSERT0(sr->is_label());
    return sr;
}


SR const* checkSRREG(SR const* sr)
{
    ASSERT0(sr->is_reg());
    return sr;
}


SR const* checkSRSTR(SR const* sr)
{
    ASSERT0(SR_is_str(sr));
    return sr;
}


SR const* checkSRIMM(SR const* sr)
{
    ASSERT0(SR_is_imm(sr));
    return sr;
}
#endif


//
//START SR
//
void SR::copy(SR const* sr, bool is_clone_vec, IN CG * cg)
{
    type = sr->type;
    u2.bitflags = sr->u2.bitflags;
    u1 = sr->u1;
    if (is_clone_vec && sr->is_vec()) {
        ASSERT0(cg);
        List<SR*> l;
        for (UINT i = 0; i < SR_vec(sr)->get_elem_count(); i++) {
            SR * sv = cg->genSR();
            sv->copy(SR_vec(sr)->get(i), is_clone_vec, cg);
            l.append_tail(sv);
        }
        SR_vec(this) = SR_vec(cg->getSRVecMgr()->genSRVec(l));
    }
}


void SR::clean()
{
    //There is virtual table.
    type = SR_UNDEF;
    u2.bitflags = 0;
    u1.u2.symbol_regid = SYMREG_UNDEF;
    u1.u2.regfile = RF_UNDEF;
    u1.u2.phy_regid = REG_UNDEF;
    u1.u2.spill_var = nullptr;
    m_sr_vec = nullptr; //vec will be dropped to pool. TODO: recycle it.
    m_sr_vec_idx = SRVEC_IDX_UNDEF;
}


//Return SR name during print assembly file.
CHAR const* SR::getAsmName(StrBuf & buf, CG const* cg)
{
    CHAR const* strformatx = "0x%x";
    CHAR const* strformatd = "%d";
    switch (SR_type(this)) {
    case SR_INT_IMM:
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
        break;
    case SR_FP_IMM:
        buf.strcat("%f", SR_fp_imm(this));
        break;
    case SR_VAR:
        if (SR_var_ofst(this) != 0) {
            buf.strcat("%s#+%d", SYM_name(SR_var(this)->get_name()),
                       SR_var_ofst(this));
            break;
        } else {
            buf.strcat("%s#", SYM_name(SR_var(this)->get_name()));
            break;
        }
    case SR_REG:
        return tmGetRegName(getPhyReg());
    case SR_STR:
        return SYM_name(SR_str(this));
    case SR_LAB:
        ASSERT0(cg);
        return cg->formatLabelName(SR_label(this), buf);
    default: UNREACHABLE();
    }
    return buf.buf;
}


//Return symbol register name and info, used by tracing routines.
//'buf': output string buffer.
CHAR const* SR::get_name(StrBuf & buf, CG const* cg) const
{
    switch (SR_type(this)) {
    case SR_INT_IMM:
        buf.strcat("#%lld", (LONGLONG)getInt());
        break;
    case SR_FP_IMM:
        buf.strcat("#%f", SR_fp_imm(this));
        break;
    case SR_VAR: {
        ASSERT0(SR_var(this));
        ASSERT0(SR_var(this)->get_name());
        CHAR * name = SYM_name(SR_var(this)->get_name());
        ASSERT0(SR_var_ir(this) == nullptr || SR_var_ir(this)->is_id());
        buf.strcat("'%s'", name);

        if (SR_var_ir(this) != nullptr) {
            UINT ofst = SR_var_ir(this)->getOffset();
            if (ofst != 0) {
                buf.strcat("+%u", ofst);
            }
        }

        if (SR_var_ofst(this) != 0) {
            buf.strcat("+%u", SR_var_ofst(this));
        }
        break;
    }
    case SR_REG:
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
            buf.strcat("sr%lu(PARAM)", (ULONG)SR_sregid(this));
        } else if (HAS_PREDICATE_REGISTER && this == cg->getTruePred()) {
            //True predicate register.
            buf.strcat("tp");
        } else {
            //General Purpose Register
            if (is_global()) {
                buf.strcat("gsr%lu", (ULONG)SR_sregid(this));
            } else {
                buf.strcat("sr%lu", (ULONG)SR_sregid(this));
            }
        }

        //Print physical register id and register file.
        if (getPhyReg() != REG_UNDEF) {
            buf.strcat("(%s)", tmGetRegName((REG)getPhyReg()));
        }
        if (getRegFile() != RF_UNDEF) {
            buf.strcat("(%s)", tmGetRegFileName(getRegFile()));
        }
        break;
    case SR_STR: {
        CHAR * s = SYM_name(SR_str(this));
        buf.nstrcat(MAX_SR_NAME_BUF_LEN, "\"%s\"", s);
        break;
    }
    case SR_LAB:
        ASSERT0(cg);
        cg->formatLabelName(SR_label(this), buf);
        break;
    default: UNREACHABLE();
    }
    return buf.buf;
}


void SR::dump(CG * cg) const
{
    StrBuf buf(64);
    note(cg->getRegion(), "%s:", get_name(buf, cg));
    //TODO: dump more info
}


bool SR::is_equal(SR const* sr)
{
    return type == sr->type &&
           u2.bitflags == sr->u2.bitflags &&
           getInt() == sr->getInt() &&
           SR_imm_size(this) == SR_imm_size(sr) &&
           SR_spill_var(this) == SR_spill_var(sr) &&
           SR_vec(this) == SR_vec(sr);
}
//END SR


//
//START SRMgr
//
void SRMgr::clean()
{
    for (INT i = SRID_UNDEF + 1; i <= m_sridx2sr.get_last_idx(); i++) {
        SR * sr = m_sridx2sr.get(i);
        ASSERT0(sr); 
        delete sr; //invoke virtual destructor of SR.
    }
    m_sridx2sr.clean();
    m_freesr_list.clean();
}


SR * SRMgr::allocSR()
{
    return new SR();
}


SR * SRMgr::genSR()
{
    SR * sr = m_freesr_list.remove_head();
    if (sr != nullptr) {
        ASSERT0(sr->is_free());
        SR_is_free(sr) = false;
        return sr;
    }
    sr = allocSR();
    ASSERT0(SRID_UNDEF == 0);
    //Do not use SRID_UNDEF as index.
    INT idx = m_sridx2sr.get_last_idx();
    SR_id(sr) = idx < 0 ? SRID_UNDEF + 1 : idx + 1;
    m_sridx2sr.set(SR_id(sr), sr);
    return sr;
}


SR * SRMgr::get(UINT unique_id)
{
    return m_sridx2sr.get(unique_id);
}


void SRMgr::freeSR(SR * sr)
{
    if (sr->is_free()) { return; }
    ASSERT0(!SR_is_dedicated(sr));
    sr->clean();
    SR_is_free(sr) = true;
    m_freesr_list.append_head(sr);
}
//END SRMgr


//
//START SRVecMgr
//
void SRVecMgr::init()
{
    if (m_pool != nullptr) { return; }
    m_pool = smpoolCreate(sizeof(SRVec), MEM_COMM);
}


SRVec * SRVecMgr::allocSRVec()
{
    SRVec * vec = (SRVec*)smpoolMalloc(sizeof(SRVec), m_pool);
    ::memset(vec, 0, sizeof(SRVec));
    vec->init();
    return vec;
}


//Return first SR in vector.
SR * SRVecMgr::genSRVec(UINT num, ...)
{
    SRVec * sv = allocSRVec();

    //This manner worked well on ia32, but is not on x8664.
    //SR ** p = (SR**)(((CHAR*)&num) + sizeof(UINT));

    sv->grow(num, m_pool);
    UINT i = 0;
    SR * first = nullptr;
    va_list ptr;
    va_start(ptr, num);
    while (i < num) {
        SR * sr = va_arg(ptr, SR*);
        if (i == 0) {
            first = sr;
        }
        SR_vec(sr) = sv;
        SR_vec_idx(sr) = i;
        sv->set(i, sr, this);
        i++;
    }
    va_end(ptr);
    return first;
}


//Return first SR in vector.
SR * SRVecMgr::genSRVec(List<SR*> & lst)
{
    SRVec * sv = allocSRVec();
    UINT i = 0;
    SR * first = nullptr;
    sv->grow(lst.get_elem_count(), m_pool);
    for (SR * sr = lst.get_head(); sr != nullptr; sr = lst.get_next()) {
        if (first == nullptr) { first = sr; }
        SR_vec(sr) = sv;
        SR_vec_idx(sr) = i;
        sv->set(i, sr, this);
        i++;
    }
    return first;
}


//Present the bytesize that is equal to the accumulation of the
//bytesize of each register-element in 'm_sr_vec'.
UINT SR::getByteSize() const
{
    if (is_vec()) {
        return SR_vec(this)->get_elem_count() * GENERAL_REGISTER_SIZE;
    }
    return GENERAL_REGISTER_SIZE;
}

} //namespace xgen
