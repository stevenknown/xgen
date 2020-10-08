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

BBSimulator::BBSimulator(ORBB * bb)
{
    m_pool = NULL;
    m_bb = bb;
    m_cg = ORBB_cg(bb);
    ASSERT0(m_cg);
    m_rg = m_cg->getRegion();
    init();
}


void BBSimulator::init()
{
    if (m_pool != NULL) { return; }
    ASSERTN_DUMMYUSE(FIRST_SLOT == 0, ("illegal slot type"));
    m_cyc_counter = 0;
    m_pool = smpoolCreate(256, MEM_COMM);
    for (INT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        m_slot_lst[i].init();
        m_exec_tab[i].init();
    }
    //Initializing free list
    m_free_list.clean();
    m_free_list.set_clean(true);
}


void BBSimulator::destroy()
{
    if (m_pool == NULL) { return; }
    m_cyc_counter = 0;
    for (INT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        m_slot_lst[i].destroy();
        m_exec_tab[i].destroy();
    }
    //Need not clean free_list.
    smpoolDelete(m_pool);
    m_pool = NULL;
}


void BBSimulator::reset()
{
    m_cyc_counter = 0;
    for (INT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        m_slot_lst[i].clean();
        m_exec_tab[i].clean();
    }
}


//Return true if all of the operations
//have been executed completely.
bool BBSimulator::done()
{
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        if (m_slot_lst[i].get_elem_count() > 0) {
            return false;
        }
    }
    return true;
}


//Allocate ordesc container
ORDesc * BBSimulator::allocORDesc()
{
    ASSERTN(m_pool, ("not yet initialized."));
    ORDesc * c = m_free_list.get_free_elem();
    if (c == NULL) {
        c = (ORDesc*)xmalloc(sizeof(ORDesc));
    }
    return c;
}


//Issue 'o' at slot during current cycle, and o is starting execution.
bool BBSimulator::issue(OR * o, SLOT slot)
{
    DUMMYUSE(slot);
    ASSERTN(m_pool, ("not yet initialized."));
    ASSERTN(slot >= FIRST_SLOT && slot <= LAST_SLOT, ("Unknown slot"));
    ASSERTN(m_exec_tab[slot].get(m_cyc_counter) == NULL,
            ("slot has been occupied by other candidate-OR"));

    //result-available-cycle o execution-cycle should start
    //from the cycle that the instruction to be prefetched
    //to the cycle that the last result to be avaialable.
    //e.g: The available cyc of mem-store is 1, and the shadow is 0,
    //     since mem-store only execute one
    //     cycle(namely, occupied memory-unit one cycle).
    bool occ_slot[SLOT_NUM] = {false};
    getOccupiedSlot(o, occ_slot);
    ASSERT0(occ_slot[slot]);
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        if (occ_slot[i]) {
            ORDesc * ord = allocORDesc();
            ORDESC_or(ord) = o;
            ORDESC_start_cyc(ord) = m_cyc_counter;
            ORDESC_or_sche_info(ord) = tmGetORScheInfo(o->getCode());
            m_exec_tab[i].set(m_cyc_counter, o);
            m_slot_lst[i].append_tail(ord);
        }
    }
    return true;
}


//Query and set corresponding array element to true if 'o' occupied that slot.
void BBSimulator::getOccupiedSlot(OR const* o, OUT bool occ_slot[SLOT_NUM])
{
    occ_slot[m_cg->computeORSlot(o)] = true;
}


//Return the number of cycles if 'o' has delay slots that
//could be used attempt to issue other independent OR.
//e.g:Load's shadow is 2 cycles because that the result
//is available after 3 cycles.
UINT BBSimulator::getShadow(OR const* o) const
{
    //On other compiler, result_available_cycle == shadow length.
    return getExecCycle(o) - 1;
}


//Return the number of cycles that 'o' executed till
//all of results are available.
//That is:
//Result_Available_Cycle = the total cycles from prefetch
//                        stage to result writeback stage.
//e.g:Load, return value is 3 cycles.
UINT BBSimulator::getExecCycle(OR const* o) const
{
    UINT cyc = ORSI_last_result_avail_cyc(tmGetORScheInfo(o->getCode()));
    //CASE: Although asm("Just_a_Label:") is not executable,
    //but the asm-o also participated scheduling as a real opertion.
    //The correct format of this case should be asm("Just_a_Label:":::"memory");
    ASSERTN(cyc >= 1, ("at least execute one cycle"));
    return cyc;
}


//Return the number of cycles that starting from the cycle 'o'
//executed to the cycle all of results were available.
//e.g:given Load, return value is 3 cycles, the load value can be
//used at the 4 cycle.
UINT BBSimulator::getMinLatency(OR * o)
{
    ORScheInfo const* oi = tmGetORScheInfo(o->getCode());
    return ORSI_first_result_avail_cyc(oi);
}


//Whether if the current cycle is in delay slot of 'ord'.
//And if that is true, we say the current cycle is in-shadow.
bool BBSimulator::isInShadow(ORDesc const* ord) const
{
    ASSERTN(m_pool, ("not yet initialized."));
    if (m_cyc_counter > ORDESC_start_cyc(ord)) {
        return true;
    }
    return false;
}


bool BBSimulator::isMemResourceConflict(DEP_TYPE deptype,
                                        ORDesc const* ck_ord,
                                        OR const* cand_or) const
{
    DUMMYUSE(cand_or);
    UINT start_cyc = ORDESC_start_cyc(ck_ord);
    OR * ck_or = ORDESC_or(ck_ord);
    ORScheInfo const* or_info = ORDESC_or_sche_info(ck_ord);
    //Resource is available only if all of memory operands are ready.
    if (HAVE_FLAG(deptype, DEP_MEM_VOL) ||
        HAVE_FLAG(deptype, DEP_MEM_OUT) ||
        HAVE_FLAG(deptype, DEP_MEM_FLOW)) {
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


//NOTICE: Result register include PC register.
bool BBSimulator::isRegResourceConflict(DEP_TYPE deptype,
                                        ORDesc const* ck_ord,
                                        OR const* cand_or) const
{
    DUMMYUSE(cand_or);
    UINT start_cyc = ORDESC_start_cyc(ck_ord);
    ORScheInfo const* or_info = ORDESC_or_sche_info(ck_ord);
    if (HAVE_FLAG(deptype, DEP_REG_OUT) ||
        HAVE_FLAG(deptype, DEP_REG_FLOW)) {
        if (m_cyc_counter <
            (INT)(start_cyc + ORSI_last_result_avail_cyc(or_info))) {
            return true;
        }
    }
    return false;
}


INT BBSimulator::computeDependentResultIndex(OR const* def, OR const* use) const
{
    for (UINT i = 0; i < def->result_num(); i++) {
        SR * res = def->get_result(i);
        ASSERT0(res);
        for (UINT j = 0; j < use->opnd_num(); j++) {
            SR * opnd = use->get_opnd(j);
            ASSERT0(opnd);
            if (res == opnd) {
                return i;
            }
        }
    }
    return -1;
}


//Check if resource conflict exists. Return true if
//the needed resource of 'cand_or' is conflict with 'ck_or'.
//For now, the machine resource include memory and register.
bool BBSimulator::isResourceConflict(ORDesc const* ck_ord,
                                     OR const* cand_or,
                                     DataDepGraph const& ddg) const
{
    OR * ck_or = ORDESC_or(ck_ord);
    xcom::Edge const* e = ddg.getEdge(OR_id(ck_or), OR_id(cand_or));
    ORScheInfo const* or_info = ORDESC_or_sche_info(ck_ord);
    ASSERT0(or_info && e);
    DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
    if (HAVE_FLAG(deptype, DEP_HYB)) {
        INT i = computeDependentResultIndex(ck_or, cand_or);
        UINT cyc = 0;
        if (i < 0) {
            cyc = ORSI_last_result_avail_cyc(or_info);
        } else {
            ASSERT0(((UINT)i) < ck_or->result_num());
            cyc = ORSI_reg_result_avail_cyc(or_info, i);
        }
        if (m_cyc_counter < (INT)(ORDESC_start_cyc(ck_ord) + cyc)) {
            //Result of 'ord' is not available yet.
            return true;
        }
        return false;
    }
    if (isRegResourceConflict(deptype, ck_ord, cand_or)) {
        return true;
    }
    if (isMemResourceConflict(deptype, ck_ord, cand_or)) {
        return true;
    }
    return false;
}


//Only aware of the data flow depdence, and neglected of resource conflict.
//Because we try to change function unit whenever possible.
bool BBSimulator::canBeIssued(OR const* o, SLOT slot, DataDepGraph & ddg) const
{
    DUMMYUSE(slot);
    ASSERTN(m_pool, ("not yet initialized."));
    ASSERTN(slot >= FIRST_SLOT && slot <= LAST_SLOT, ("Unknown slot"));
    ASSERTN(m_exec_tab[slot].get(m_cyc_counter) == NULL, ("slot has issued o"));

    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        C<ORDesc*> * ct;
        for (ORDesc * ord = m_slot_lst[i].get_head(&ct); ord != NULL;
             ord = m_slot_lst[i].get_next(&ct)) {
            OR * ckor = ORDESC_or(ord);
            if (getShadow(ckor) == 0) {
                //'ckor' only execute one cycle, the result is certainly
                //available at cycle 'o' issued.
                continue;
            }

            if (!isInShadow(ord)) {
                //Current cycle is not in delay slot of 'ord'.
                return false;
            }

            //Check if ckor dependented on o.
            if (ddg.is_dependent(ckor, o) &&
                isResourceConflict(ord, o, ddg)) {
                return false;
            }
        }
    }
    return true;
}


void BBSimulator::runOneCycle(IN OUT ORList * finished_ors)
{
    ASSERTN(m_pool, ("not yet initialized."));
    m_cyc_counter++;
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        ORDesc * next = NULL;
        for (ORDesc * ord = m_slot_lst[i].get_head(); ord != NULL; ord = next) {
            next = m_slot_lst[i].get_next();
            //Because cyc_counter start from cycle '0', if simm run
            //to cycle 1, namely, cyc_counter is '1', it has executed 1
            //cycle already.And all of operations which
            //start at cycle 0 and result available time is one cycle
            //are finished.
            INT c = ORDESC_start_cyc(ord) +
                    ORSI_last_result_avail_cyc(ORDESC_or_sche_info(ord));
            if (m_cyc_counter >= c) {
                //Result is available now.
                m_slot_lst[i].remove(ord);
                if (finished_ors != NULL) {
                    finished_ors->append_tail(ORDESC_or(ord));
                }
                freeordesc(ord);
            }
        }
    }
}


void BBSimulator::dump(CHAR * name, bool is_del, bool dump_exec_detail)
{
    ASSERT0(m_pool);
    #define SIMM_DUMP_NAME "zsim.tmp"

    if (name == NULL) {
        name = SIMM_DUMP_NAME;
    }

    if (is_del) {
        UNLINK(name);
    }

    FILE * h = fopen(name,"a+");
    ASSERTN(h, ("%s create failed!!!", name));

    //cyc may be zero
    INT cyc_counter = getCurCycle();

    //Because execution of simm to be more one cycle than in acutally.
    cyc_counter--;

    INT bbid = -1;

    if (m_bb != NULL) {
        bbid = m_bb->id();
    }

    ORVec const* ors_vec = getExecSnapshot();

    fprintf(h, "\nRegion(%s), BB(%d)", m_rg->getRegionName(), bbid);

    //Preparing slot name buffer
    CHAR slot_name[SLOT_NUM][SLOT_NAME_MAX_LEN];
    for (UINT slot = FIRST_SLOT; slot < SLOT_NUM; slot++) {
        CHAR const* sn = tmGetSlotName((SLOT)(FIRST_SLOT + slot));
        sprintf(slot_name[slot], "%s", sn);
    }

    //Print execution detail for each of cycle.
    if (dump_exec_detail) {
        StrBuf buf(64);
        for (INT i = 0; i <= cyc_counter; i++) {
            fprintf(h, "\n\tCycle(%d):", i);
            for (UINT slot = FIRST_SLOT; slot < SLOT_NUM; slot++) {
                fprintf(h, "\n\t\t%s: ", slot_name[slot]);
                OR * o = ors_vec[slot].get(i);
                if (o != NULL) {
                    buf.clean();
                    fprintf(h, "%s", o->dump(buf, m_cg));
                } else {
                    fprintf(h, "\n\t\t");
                }
            }
        }
        fprintf(h, "\n");
    }

    //Print coarse grain looks.
    StrBuf ornamebuf1(64);
    StrBuf ornamebuf2(64);
    StrBuf bundle(256);

    //Print slot name.
    fprintf(h, "\n==---- CoarseView, BB(%d) ----==", bbid);

    fprintf(h, "\n%-10s", " ");
    for (UINT slot2 = FIRST_SLOT; slot2 < SLOT_NUM; slot2++) {
        fprintf(h, "%-19s   ", slot_name[slot2]);
    }
    fprintf(h, "\n");

    //Print abbreviated o name with its map-idx.
    for (INT i = 0; i <= cyc_counter; i++) {
        fprintf(h, "\nCyc:%-4d:", i);
        fprintf(h, "{");
        bundle.clean();
        for (UINT s = FIRST_SLOT; s < SLOT_NUM; s++) {
            OR * o = ors_vec[s].get(i);
            if (o != NULL) {
                ornamebuf1.sprint("%s(%d)", OR_code_name(o), OR_id(o));
            } else {
                ornamebuf1.sprint(" ");
            }

            ornamebuf2.sprint("%-19s", ornamebuf1.buf);

            bundle.strcat(ornamebuf2.buf);

            if (s < SLOT_NUM - 1) {
                bundle.strcat(" | ");
            }
        }

        fprintf(h, "%s", bundle.buf);
        fprintf(h, "}");
    }

    fprintf(h, "\n");
    fclose(h);
}

} //namespace xgen
