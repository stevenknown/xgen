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

ARMSim::ARMSim(ORBB * bb) : BBSimulator(bb)
{
}


void ARMSim::getOccupiedSlot(OR const* o, OUT bool occ_slot[SLOT_NUM])
{
    if (o->is_bus() || o->is_asm() || o->isSpadjust()) {
        for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
            occ_slot[i] = true;
        }
    } else {
        BBSimulator::getOccupiedSlot(o, occ_slot);
    }
}


bool ARMSim::isMemResourceConflict(DEP_TYPE deptype,
                                   ORDesc const* ck_ord,
                                   OR const* cand_or) const
{
    DUMMYUSE(cand_or);
    UINT start_cyc = ck_ord->getStartCycle();
    OR const* ck_or = ck_ord->getOR();
    ORScheInfo const* or_info = ck_ord->getScheInfo();
    if (HAVE_FLAG(deptype, DEP_MEM_FLOW)) {
        for (UINT i = 0; i < numOfMemResult(ck_or); i++) {
            if (m_cyc_counter <
                (INT)(start_cyc + ORSI_mem_result_avail_cyc(or_info, i))) {
                return true;
            }
        }
    }

    if (HAVE_FLAG(deptype, DEP_MEM_OUT) || HAVE_FLAG(deptype, DEP_MEM_FLOW)) {
        if (ORSI_mem_result_cyc_buf(or_info) == nullptr) {
            //Dependency carried by special memory operation.
            //e.g:spadjust -> asm, etc.
            return true;
        }
        if (m_cyc_counter <
            (INT)(start_cyc + ORSI_mem_result_avail_cyc(or_info, 0))) {
            return true;
        }
    }

    if (HAVE_FLAG(deptype, DEP_MEM_VOL)) {
        UINT last_mem_cyc = 0;
        for (UINT i = 0; i < numOfMemResult(ck_or); i++) {
            last_mem_cyc = MAX(last_mem_cyc,
                               ORSI_mem_result_avail_cyc(or_info, i));
        }
        if (m_cyc_counter < (INT)(start_cyc + last_mem_cyc)) {
            return true;
        }
    }
    return false;
}


//NOTE: If an anti-dependence exist, the sink node can not execute
//prior to the souce node.
//e.g: The following case is correct:
//  1. t2 = t1 || t1 = 10
//  2. t2 = t1 || nop
//     nop     || t1 = 10
bool ARMSim::isRegResourceConflict(DEP_TYPE deptype,
                                   ORDesc const* ck_ord,
                                   OR const* cand_or) const
{
    UINT start_cyc = ck_ord->getStartCycle();
    OR const* ck_or = ck_ord->getOR();
    ORScheInfo const* or_info = ck_ord->getScheInfo();

    if (HAVE_FLAG(deptype, DEP_REG_FLOW)) {
        for (UINT i = 0; i < ck_or->result_num(); i++) {
            if (m_cg->mustUse(cand_or, ck_or->get_result(i)) &&
                m_cyc_counter < (INT)(start_cyc +
                                 ORSI_reg_result_avail_cyc(or_info, i))) {
                return true;
            }
        }
    }

    if (HAVE_FLAG(deptype, DEP_REG_OUT)) {
        for (UINT i = 0; i < ck_or->result_num(); i++) {
            SR * res = ck_or->get_result(i);
            if (res != m_cg->getRflag() &&
                m_cg->mustDef(cand_or, res) &&
                m_cyc_counter < (INT)(start_cyc +
                                      ORSI_reg_result_avail_cyc(or_info, i))) {
                return true;
            }
        }
    }

    return false;
}


UINT ARMSim::numOfMemResult(OR const* o) const
{
    //All of StoreMemory operations have one memory result.
    return o->is_store() ? 1 : 0;
}
