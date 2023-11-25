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

#ifdef _DEBUG_
SR const* checkSRVAR(SR const* sr)
{
    ASSERT0(sr->is_var());
    return sr;
}


SR const* checkSRLABList(SR const* sr)
{
    ASSERT0(sr->is_label_list());
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
    ASSERT0(sr->is_str());
    return sr;
}


SR const* checkSRIMM(SR const* sr)
{
    ASSERT0(sr->is_imm());
    return sr;
}
#endif


//
//START SRList
//
void SRList::appendTailFromSRVec(SRVec const& srvec)
{
    for (VecIdx i = 0; i < (VecIdx)srvec.get_elem_count(); i++) {
         SR * sr = srvec.get(i);
         append_tail(sr);
    }
}
//END SRList


//
//START SR
//
void SR::copy(SR const* sr)
{
    SR_code(this) = sr->getCode();
    u2.bitflags = sr->u2.bitflags;
}


void SR::copy(SR const* sr, bool is_clone_vec, IN CG * cg)
{
    copy(sr);
    if (is_clone_vec && sr->is_vec()) {
        ASSERT0(cg && SR_vec(sr));
        List<SR*> l;
        for (UINT i = 0; i < SR_vec(sr)->get_elem_count(); i++) {
            SR const* src = SR_vec(sr)->get(i);
            SR * sv = cg->genSR(src->getCode());
            sv->copy(src, false, cg);
            l.append_tail(sv);
        }
        SR_vec(this) = SR_vec(cg->getSRVecMgr()->genSRVec(l));
    }
}


void SR::dump(CG const* cg) const
{
    StrBuf buf(64);
    note(cg->getRegion(), "%s:", get_name(buf, cg));
    //TODO: dump more info
}


bool SR::is_equal(SR const* sr)
{
    return m_code == sr->getCode() &&
           u2.bitflags == sr->u2.bitflags &&
           getInt() == sr->getInt() &&
           SR_imm_size(this) == SR_imm_size(sr) &&
           SR_spill_var(this) == SR_spill_var(sr) &&
           SR_vec(this) == SR_vec(sr);
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
//END SR


//
//START SRMgr
//
void SRMgr::clean()
{
    for (VecIdx i = SRID_UNDEF + 1; i <= m_sridx2sr.get_last_idx(); i++) {
        SR * sr = m_sridx2sr.get(i);
        if (sr != nullptr) {
            delete sr; //invoke virtual destructor of SR.
        }
    }
    m_sridx2sr.clean();
    m_sr_count = SRID_UNDEF;
}


SR * SRMgr::allocSR(SR_CODE c)
{
    switch (c) {
    case SR_REG: return new RegSR();
    case SR_INT_IMM: return new IntSR();
    case SR_FP_IMM: return new FPSR();
    case SR_VAR: return new VarSR();
    case SR_STR: return new StrSR();
    case SR_LAB: return new LabSR();
    case SR_LAB_LIST: return new LabListSR();
    default: UNREACHABLE();
    }
    return nullptr;
}


SR * SRMgr::genSR(SR_CODE c)
{
    SR * sr = allocSR(c);
    ASSERT0(sr && sr->getCode() != SR_UNDEF);
    //DO NOT USE SRID_UNDEF AS INDEX.
    m_sr_count++;
    SR_id(sr) = m_sr_count;
    m_sridx2sr.set(sr->id(), sr);
    return sr;
}


void SRMgr::freeSR(SR * sr)
{
    ASSERT0(!SR_is_dedicated(sr));
    m_sridx2sr.set(sr->id(), nullptr);
    sr->clean();
    delete sr;
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
    ::memset((void*)vec, 0, sizeof(SRVec));
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
//END SRVecMgr

} //namespace xgen
