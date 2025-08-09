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

void LIS::init(ORBB * bb, DataDepGraph & ddg, BBSimulator * sim, UINT sch_mode)
{
    if (m_pool != nullptr) { return; }
    ASSERTN(bb && sim, ("invalid parameter"));
    m_pool = smpoolCreate(256, MEM_COMM);
    m_is_regfile_unique = nullptr;
    m_ddg = &ddg;
    m_bb = bb;
    m_sim = sim;
    m_cg = ORBB_cg(bb);
    m_ready_list.init();
    m_br_all_preds.init();
    m_or_changed = false;
    m_sch_mode = sch_mode;

    OR * br = nullptr;
    if (HAVE_FLAG(m_sch_mode, SCH_BRANCH_DELAY_SLOT) &&
        (br = get_br()) != nullptr) {
        m_br_ord = (ORDesc*)xmalloc(sizeof(ORDesc));
        ORDESC_or(m_br_ord) = br;
        ORDESC_start_cyc(m_br_ord) = -1;
        //Note 'sim' can NOT be destructed by caller of schedulor until
        //packaging is finished because sim preserved the cycle-accurated
        //scheduling information.
        ORDESC_sche_info(m_br_ord) = tmGetORScheInfo(OR_code(br));
        ASSERT0(ORDESC_sche_info(m_br_ord));
    } else {
        m_br_ord = nullptr;
    }
}


void LIS::destroy()
{
    if (m_pool == nullptr) { return; }
    smpoolDelete(m_pool);
    m_pool = nullptr;
    m_sch_mode = SCH_UNDEF;
    m_ddg = nullptr;
    m_sim = nullptr;
    m_bb = nullptr;
    m_cg = nullptr;
    m_ready_list.destroy();
    m_br_all_preds.destroy();
    m_br_ord = nullptr;
}


void LIS::computeReadyList(MOD DataDepGraph & ddg,
                           OUT DefSBitSet & visited,
                           bool topdown)
{
    ASSERTN(m_pool, ("uninitialized"));
    PreemptiveORList * nop_list = &m_tmp_orlist; //Regard tmp as nop list.
    ASSERT0(!nop_list->is_occupied());
    nop_list->occupy();
    bool redo = false;

COMP_REDO:
    VertexIter c;
    for (xcom::Vertex * v = ddg.get_first_vertex(c);
         v != nullptr; v = ddg.get_next_vertex(c)) {
        xcom::EdgeC * ck_lst;
        if (topdown)  {
            ck_lst = v->getInList();
        } else {
            ck_lst = v->getOutList();
        }
        if (ck_lst != nullptr || visited.is_contain(v->id())) { continue; }
        OR * o = ddg.getOR(VERTEX_id(v));
        if (OR_is_nop(o)) { //Do not schedule NOP.
            nop_list->append_tail(o);
            continue;
        }
        m_ready_list.append(o);
        visited.bunion(v->id());
    }

    if (nop_list->get_elem_count() > 0) {
        for (OR * o = nop_list->get_head();
             o != nullptr; o = nop_list->get_next()) {
            xcom::Vertex * v = ddg.getVertex(o->id());
            if (ddg.getDegree(v) > 0) {
                redo = true;
            }
            ddg.removeOR(o);
        }
        nop_list->clean();
    }

    if (redo) {
        redo = false;
        goto COMP_REDO;
    }
    nop_list->release();
}


bool LIS::isIssueCand(OR const* o) const
{
    ASSERTN(m_pool, ("uninitialized"));
    SLOT or_slot = m_cg->computeORSlot(o);

    //Schedule the branch delay slot at first.
    if (isScheduleDelaySlot() && isBranch(o)) {
        if (m_br_ord->getStartCycle() != -1 &&
            m_br_ord->getStartCycle() > (INT)m_sim->getCurCycle()) {
            //It is not the issue time of branch yet.
            return false;
        }
    }
    if (m_sim->canBeIssued(o, or_slot, *m_ddg)) {
        return true;
    }
    return false;
}


//Check the usage of function unit.
bool LIS::isFuncUnitOccupied(UnitSet const& us, CLUST clst,
                             OR const* const issue_ors [SLOT_NUM]) const
{
    for (BSIdx i = us.get_first(); i != BS_UNDEF; i = us.get_next(i)) {
        SLOT s = m_cg->mapUnit2Slot((UNIT)i, clst);
        if (issue_ors[s] != nullptr) {
            //This slot has issued other operation.
            return true;
        }
    }
    return false;
}


//Verficiation of instruction hazard, and change slot of o if possible.
//Return true if 'o' can be issued at 'to_slot'.
//The verification includes hardware resource, instrcution hazard, etc.
bool LIS::makeIssued(OR * o,
                     OR * issue_ors[SLOT_NUM],
                     SLOT to_slot,
                     bool is_change_slot,
                     OR * conflict_ors[SLOT_NUM])
{
    ASSERT0(m_cg->mapSlot2Cluster(to_slot) != CLUST_UNDEF);
    UNIT to_slot_unit = m_cg->mapSlot2Unit(to_slot);
    UnitSet const* us = m_cg->computeORUnit(o);
    CLUST or_clst = m_cg->computeORCluster(o);
    bool need_change_slot = false;
    if (m_cg->computeORSlot(o) != to_slot ||
        !us->is_contain(to_slot_unit) ||
        isFuncUnitOccupied(*us, or_clst, issue_ors)) {
        //'o' required function unit has been occupied.
        need_change_slot = true;
    }
    if (need_change_slot) {
        if (!is_change_slot) {
            return false;
        }
        if (!changeSlot(o, to_slot)) {
            return false;
        }
    }
    return isValidResourceUsage(o, to_slot, issue_ors, conflict_ors);
}


//Select an operation with highest priority.
//Change the function unit of best o if necessary.
OR * LIS::selectBestOR(ORTab & cand_tab, SLOT slot)
{
    DUMMYUSE(slot);
    ASSERT0(cand_tab.get_elem_count() > 0);
    ORTab br_preds_cand;
    ORTab * cand_hash_ptr = &cand_tab;
    if (isScheduleDelaySlot()) {
        //Find ORs which are predecessors of branch-OR in order to
        //schedule branch-OR as earlier as possible.
        ORTabIter c;
        for (OR * o = cand_tab.get_first(c);
             o != nullptr; o = cand_tab.get_next(c)) {
            if (m_br_all_preds.find(o)) {
                br_preds_cand.append(o);
            }
        }
        if (br_preds_cand.get_elem_count() > 0) {
            cand_hash_ptr = &br_preds_cand;
        }
    }

    //Try more particular estimiate.
    return selectBestORByPriority(*cand_hash_ptr);
}


//Find valid OR that can be issued at slot.
//Return true if avaiable issue ors found.
//cand_list: list of candidate operations which can be issue at this cycle.
//slot: slot need to fill
//issue_or: record issued ors selected.
//change_slot: set to true indicate the routine allows modification
//  of operations in other slot. Note that the modification may
//  change the function unit of operation.
//Note this functio will attempt to change OR's slot if possible.
bool LIS::selectIssueOR(IN PreemptiveORList & cand_list, //OR may be changed
                        SLOT slot, OUT OR * issue_ors[SLOT_NUM],
                        bool change_slot)
{
    ASSERTN(m_pool, ("uninitialized"));
    ORTab valid_cands;
    OR * cflct_ors[SLOT_NUM] = {0}; //record conflict ors.
    for (OR * o = cand_list.get_head();
         o != nullptr; o = cand_list.get_next()) {
        if (makeIssued(o, issue_ors, slot, change_slot, cflct_ors)) {
            valid_cands.append(o);
        }
    }
    if (valid_cands.get_elem_count() == 0) {
        return false;
    }
    OR * best_issue_or = selectBestOR(valid_cands, slot);
    ASSERT0(best_issue_or);
    updateIssueORs(cand_list, slot, best_issue_or, issue_ors);
    return true;
}


//Mark and update candidate-list of OR to record which operation
//has been selected to be issued subsequently.
void LIS::updateIssueORs(MOD PreemptiveORList & cand_list,
                         SLOT slot, IN OR * issue_or,
                         MOD OR * issue_ors[SLOT_NUM])
{
    issue_ors[slot - FIRST_SLOT] = issue_or;
    cand_list.remove(issue_or);
}


//When we could not find any OR that can be issued at
//'to_slot', the function will be invokoed to attemp to move outside
//operations have been selected to 'to_slot'.
//
//e.g:Now the ready ors are Load, Mul, the slot are MEM, ALU,
//    and Load can issue at slot MEM, Mul can issue at slot
//    both ALU and MEM.
//    Suppose we have chosen Mul for slot MEM, then there is
//    not any candidate for slot ALU because Load only can be
//    issued at MEM. So we are going to move Mul at slot ALU,
//    and reschedul operation for slot MEM.
//    before roll back:
//        {Mul | nop}
//        {Load  | nop}
//    after roll back:
//        {Load  | Mul}
SLOT LIS::rollBackORs(bool be_changed[SLOT_NUM], OR * issue_ors[SLOT_NUM],
                      SLOT to_slot)
{
    ASSERT0(issue_ors[to_slot] == nullptr);
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        if (!be_changed[i] && issue_ors[i] != nullptr) {
            if (!changeSlot(issue_ors[i], to_slot)) {
                continue;
            }
            ASSERTN(i != (UINT)to_slot, ("illegal!"));
            m_or_changed = true;
            issue_ors[to_slot] = issue_ors[i];
            issue_ors[i] = nullptr;
            be_changed[to_slot] = true;
            return (SLOT)i;
        }
    }
    return (SLOT)(to_slot + 1);
}


//Select instructions to fill issue slot as much as possible.
//Return true if the candidate has been found.
bool LIS::selectIssueORs(IN PreemptiveORList & cand_list,
                         OUT OR * issue_ors[SLOT_NUM])
{
    ASSERTN(m_pool, ("uninitialized"));
    bool find = true; //Find at least one slot can be issued.
    bool be_changed[SLOT_NUM] = {false};
    SLOT next_slot;
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i = next_slot) {
        next_slot = (SLOT)(i + 1);
        if (cand_list.get_elem_count() == 0) {
            break;
        }
        SLOT slot = (SLOT)(((INT)FIRST_SLOT) + i);
        if (selectIssueOR(cand_list, slot, issue_ors, allowChangeSlot())) {
            find = true;
            continue;
        }
        if (allowChangeSlot()) {
            next_slot = rollBackORs(be_changed, issue_ors, slot);
        }
    }
    return find;
}


//Add more ready-OR into ready-list when removing 'or'.
//o: OR will be removed.
//ddg: dependent graph, vertex of 'o' should be removed after function return.
//visited: bitset that used to update ready-or-list. It can be nullptr if you
//         are not going to update ready-list.
void LIS::updateReadyList(OR const* o, DataDepGraph const& ddg,
                          DefSBitSet * visited)
{
    if (visited == nullptr) { return; }

    Vertex const* or_vex = ddg.getVertex(o->id());
    for (xcom::EdgeC const* el = or_vex->getOutList();
         el != nullptr; el = el->get_next()) {
        Vertex * vex_succ = el->getTo();

        //Determine if in-degree is bigger than 1.
        UINT in_degrees = 0;
        for (xcom::EdgeC const* el2 = vex_succ->getInList();
             el2 != nullptr && in_degrees <= 1;
             el2 = el2->get_next(), in_degrees++) {;}

        ASSERT0(in_degrees >= 1);
        if (in_degrees == 1) {
            m_ready_list.append(m_cg->getOR(vex_succ->id()));
            ASSERT0(!visited->is_contain(vex_succ->id()));
            visited->bunion(vex_succ->id());
        }
    }
}


bool LIS::tryIssueCandList(PreemptiveORList & cand_list, DataDepGraph * stepddg,
                           DefSBitSet * visited)
{
    //Appropriate OR has been selected and issued.
    bool issued = false;

    //Record selected OR that is ready to issue, and these ORs
    //will not has conflict with ones already issued both in
    //hardware resources and data dependences.
    OR * issue_ors[SLOT_NUM] = {0};
    if (cand_list.get_elem_count() > 0) {
        //Note OR in cand_list may be changed.
        if (selectIssueORs(cand_list, issue_ors)) {
            for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
                if (issue_ors[i] == nullptr) { continue; }
                if (isScheduleDelaySlot() &&
                    issue_ors[i] == m_br_ord->getOR()) {
                    //Branch-OR is ready to schedule!
                    ORDESC_start_cyc(m_br_ord) = m_sim->getCurCycle();
                }
                if (m_sim->issue(issue_ors[i], (SLOT)(FIRST_SLOT + i))) {
                    m_ready_list.remove(issue_ors[i]);
                }
                if (stepddg != nullptr) {
                    updateReadyList(issue_ors[i], *stepddg, visited);
                    stepddg->removeOR(issue_ors[i]);
                }
                issued = true;
            }
        }
    }
    m_sim->runOneCycle(nullptr);
    return issued;
}


//Choose candidate OR from ready-list, and fill available issue slot.
//Return true if there are ORs issued, and ready-list should
//be recomputed.
//stepddg: computing ready instructions step by step during scheduling.
//visited: bitset that used to update ready-or-list. It can be nullptr if you
//         are not going to update ready-list.
//
//About fill branch shadow:
//    When GIS is calling it, don't try to schedule the operation
//    that has moved from a different block that do not
//    dominate/post-dominate current BB in the delay slot.
//    Frequent observance is that this will unneccessarily
//    restrict further code motion.
bool LIS::fillIssueSlot(DataDepGraph * stepddg, DefSBitSet * visited)
{
    ASSERTN(m_pool, ("uninitialized"));
    PreemptiveORList * cand_list = &m_tmp_orlist; //OR may be changed.
    ASSERT0(!cand_list->is_occupied());
    cand_list->occupy();

    //Find ors that can be issued at current cycle.
    ORTabIter c;
    for (OR * o = m_ready_list.get_first(c);
         o != nullptr; o = m_ready_list.get_next(c)) {
        if (isIssueCand(o)) {
            cand_list->append_tail(o);
        }
    }

    //Appropriate OR has been selected and issued.
    bool issued = tryIssueCandList(*cand_list, stepddg, visited);
    cand_list->release();
    return issued;
}


INT LIS::dcache_miss_rate(OR const*) const
{
    return 0;
}


INT LIS::dcache_miss_penalty(OR const*) const
{
    return 0;
}


//Select an operation with highest priority.
OR * LIS::selectBestORByPriority(ORTab const& cand_tab) const
{
    OR * best_cand = nullptr;
    float best_prio = 0.0;
    ORTabIter c;
    for (OR * o = cand_tab.get_first(c);
         o != nullptr; o = cand_tab.get_next(c)) {
        float prio = 1.0;

        //Consider execute shadow.
        ASSERTN(m_sim->getShadow(o) >= 0,("illegal info"));
        prio += m_sim->getShadow(o) * 5 +
                dcache_miss_rate(o) * dcache_miss_penalty(o);

        prio /= (float)(m_ddg->get_slack(o) + SL_BASE_FACTOR);
        if (prio > best_prio) {
            best_prio = prio;
            best_cand = o;
        }
    }
    return best_cand;
}


void LIS::dump(bool need_exec_detail) const
{
    if (!m_cg->getRegion()->isLogMgrInit()) { return; }
    m_sim->dump(m_cg->getRegion()->getLogMgr(), need_exec_detail);
}


//Compute the execution time for instructions in serialization order.
//This function does not change any function unit.
void LIS::serialize()
{
    //START_TIMER_FMT(t, ("LIS: Serialize: BB%d", getBB()->id()));
    ASSERT0(m_bb && m_ddg);
    INT last_res_cyc = 0;
    if (isScheduleDelaySlot()) {
        last_res_cyc = ORSI_last_result_avail_cyc(
            m_br_ord->getScheInfo());
    }

    BBORListIter ct = nullptr;
    for (m_bb->getORList()->get_head(&ct);
         ct != m_bb->getORList()->end();
         m_bb->getORList()->get_next(&ct)) {
        m_ready_list.append(ct->val());
        while (!fillIssueSlot(nullptr, nullptr)) {;}
    }

    if (isScheduleDelaySlot()) {
        UINT end_cyc = m_sim->getCurCycle() - 1;
        INT br_start_cyc = m_br_ord->getStartCycle();
        ASSERT0(br_start_cyc >= 0);
        INT pad = br_start_cyc + (last_res_cyc - 1) - end_cyc;
        ASSERT0(pad >= 0);
        for (INT i = 0; i < pad; i++) {
            m_sim->runOneCycle(nullptr);
        }
    }

    //END_TIMER_FMT(t, ("LIS: Serialize: BB%d", getBB()->id()));
    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpLIS()) {
        dump(true);
    }
}


//Performing scheduling of basic block.
//Return true if some instructions changed their register file.
bool LIS::schedule()
{
    //START_TIMER_FMT(t, ("LIS: Schedule: BB%d", getBB()->id()));
    ASSERTN(m_pool, ("uninitialized"));

    //Stepddg is local used, and should be deleted
    //before leaving this function.
    DataDepGraph * stepddg = m_cg->allocDDG();
    stepddg->init(m_bb);
    stepddg->clone(*m_ddg);
    OR * latest = nullptr;
    stepddg->computeEstartAndLstart(*m_sim, &latest);

    OR * left = nullptr;
    if (!isScheduleDelaySlot()) {
        left = get_br();
        if (left != nullptr) {
            stepddg->removeOR(left);
        }
    }

    DefMiscBitSetMgr sm;
    DefSBitSet visited(sm.getSegMgr());
    m_or_changed = false;

    INT br_latency = 0;
    if (isScheduleDelaySlot() && allowReschedule()) {
        //If strategy is to fill BR delay slot as much as possible, the
        //schedulor will schedule BR operation and its dependence ORs as
        //soon as possible. This may lead rescheduling if current schdueling
        //is illegal, because the scheduling strategy is top-down scheduling.
        ORDESC_start_cyc(m_br_ord) = -1;
        stepddg->getDependentPreds(m_br_all_preds, m_br_ord->getOR());
        m_br_all_preds.append(m_br_ord->getOR());
        br_latency = ORSI_last_result_avail_cyc(m_br_ord->getScheInfo());
    }
RESCH:
    computeReadyList(*stepddg, visited, true);
    while (stepddg->getVertexNum() > 0 || !m_sim->done()) {
        fillIssueSlot(stepddg, &visited);
    }
    if (left != nullptr) {
        ASSERT0(!m_tmp_orlist.is_occupied());
        m_tmp_orlist.occupy();
        m_tmp_orlist.append_tail(left);
        //There is no need to update stepddg any more.
        tryIssueCandList(m_tmp_orlist, nullptr, nullptr);
        m_tmp_orlist.release();
    }
    if (isScheduleDelaySlot() && allowReschedule()) {
        //Select an issue cycle for branch operation according last scheduling
        //result.
        UINT end_cyc = m_sim->getCurCycle() - 1;
        INT br_start_cyc = m_br_ord->getStartCycle();
        ASSERTN(br_start_cyc >= 0, ("BR is not scheduled"));
        if (br_start_cyc < (INT)(end_cyc - br_latency)) {
            //Illegal scheduling, BR will terminate the execution sequence
            //too early. Thus, update BR's start cycle and rescheduling.
            //Note 'max_try' can not less than 'br_latency'.
            INT max_try = br_latency + getAddendOfMaxTryTime();
            if (((INT)(end_cyc - br_start_cyc)) > max_try) {
                ORDESC_start_cyc(m_br_ord) = end_cyc - max_try;
            } else {
                ORDESC_start_cyc(m_br_ord)++;
            }
            stepddg->clone(*m_ddg);
            m_ready_list.destroy();
            m_ready_list.init();
            m_sim->reset();
            visited.clean();
            goto RESCH;
        }
    }

    delete stepddg;
    //END_TIMER_FMT(t, ("LIS: Schedule: BB%d", getBB()->id()));
    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpLIS()) {
        dump(true);
    }
    return m_or_changed;
}

} //namespace xgen
