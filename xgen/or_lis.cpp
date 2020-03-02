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

void LIS::init(ORBB * bb,
               DataDepGraph & ddg,
               BBSimulator * sim,
               UINT sch_mode,
               bool change_slot,
               bool change_cluster)
{
    if (m_pool != NULL) return;
    ASSERTN(bb && sim, ("invalid parameter"));
    m_pool = smpoolCreate(256, MEM_COMM);
    m_is_regfile_unique = NULL;
    m_ddg = &ddg;
    m_bb = bb;
    m_sim = sim;
    m_cg = ORBB_cg(bb);
    m_ready_list.init();
    m_br_all_preds.init();
    m_is_change_slot = change_slot;
    m_or_changed = false;
    m_is_change_cluster = change_cluster;
    m_sch_mode = sch_mode;
    OR * br = NULL;
    if (m_sch_mode == SCH_BRANCH_DELAY_SLOT &&
        (br = get_br()) != NULL) {
        m_br_ord = (ORDesc*)xmalloc(sizeof(ORDesc));
        ORDESC_or(m_br_ord) = br;
        ORDESC_start_cyc(m_br_ord) = -1;
        //Note that 'sim' can NOT be destructed by caller of schedulor,
        //o else the memory in used is unavailable.
        ORDESC_or_sche_info(m_br_ord) = tmGetORScheInfo(OR_code(br));
        ASSERT0(ORDESC_or_sche_info(m_br_ord));
    } else {
        m_br_ord = NULL;
    }
}


void LIS::destroy()
{
    if (m_pool == NULL) return;
    smpoolDelete(m_pool);
    m_pool = NULL;
    m_sch_mode = SCH_UNDEF;
    m_ddg = NULL;
    m_sim = NULL;
    m_bb = NULL;
    m_cg = NULL;
    m_ready_list.destroy();
    m_br_all_preds.destroy();
    m_br_ord = NULL;
}


void LIS::computeReadyList(
        IN OUT DataDepGraph & ddg,
        IN OUT Vector<bool> & visited,
        bool topdown)
{
    ASSERTN(m_pool, ("uninitialized"));
    ORList nop_list;
    bool redo = false;
COMP_REDO:
    INT c;
    for (xcom::Vertex * v = ddg.get_first_vertex(c);
         v != NULL; v = ddg.get_next_vertex(c)) {
        xcom::EdgeC * ck_lst;
        if (topdown)  {
            ck_lst = VERTEX_in_list(v);
        } else {
            ck_lst = VERTEX_out_list(v);
        }
        if (ck_lst == NULL && !visited.get(VERTEX_id(v))) {
            OR * o = ddg.getOR(VERTEX_id(v));
            if (OR_is_nop(o)) { //Do not schedul NOP.
                nop_list.append_tail(o);
                continue;
            }
            m_ready_list.append(o);
            visited.set(VERTEX_id(v), true);
            continue;
        }
    }

    if (nop_list.get_elem_count() > 0) {
        for (OR * o = nop_list.get_head();
             o != NULL; o = nop_list.get_next()) {
            xcom::Vertex * v = ddg.getVertex(OR_id(o));
            if (ddg.getDegree(v) > 0) {
                redo = true;
            }
            ddg.removeOR(o);
        }
        nop_list.clean();
    }

    if (redo) {
        redo = false;
        goto COMP_REDO;
    }
}


bool LIS::isIssueCand(OR * o)
{
    ASSERTN(m_pool, ("uninitialized"));
    SLOT or_slot = m_cg->computeORSlot(o);

    //Schedule the branch delay slot at first.
    if (m_br_ord != NULL && o == ORDESC_or(m_br_ord)) {
        if (ORDESC_start_cyc(m_br_ord) != -1 &&
            ORDESC_start_cyc(m_br_ord) > (INT)m_sim->getCurCycle()) {
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
bool LIS::isFuncUnitOccupied(
        UnitSet const& us,
        CLUST clst,
        OR const* const issue_ors [SLOT_NUM])
{
    for (INT i = us.get_first(); i != -1; i = us.get_next(i)) {
        SLOT s = m_cg->mapUnit2Slot((UNIT)i, clst);
        if (issue_ors[s] != NULL) {
            //This slot has issued other operation.
            return true;
        }
    }
    return false;
}


//Verficiation of instruction hazard, and change slot of o if possible.
//Return true if 'o' can be issued at 'to_slot'.
//The verification includes hardware resource, instrcution hazard, etc.
bool LIS::canBeIssued(
        OR * o,
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

    if (!isValidResourceUsage(o, to_slot, issue_ors, conflict_ors)) {
        return false;
    }
    return true;
}


//Select an operation with highest priority.
//Change the function unit of best o if necessary.
OR * LIS::selectBestOR(OR_HASH & cand_hash, SLOT slot)
{
    DUMMYUSE(slot);

    ASSERT0(cand_hash.get_elem_count() > 0);
    OR_HASH br_preds_cand_hash;
    OR_HASH * cand_hash_ptr = &cand_hash;
    if (m_br_ord != NULL) {
        //Find ors which are predecessors of branch-o in order to
        //schedule branch-o as earlier as possible.
        INT c;
        for (OR * o = cand_hash.get_first(c);
             o != NULL; o = cand_hash.get_next(c)) {
            if (m_br_all_preds.find(o)) {
                br_preds_cand_hash.append(o);
            }
        }
        if (br_preds_cand_hash.get_elem_count() > 0) {
            cand_hash_ptr = &br_preds_cand_hash;
        }
    }

    //Try more particular estimiate.
    return selectBestORByPriority(*cand_hash_ptr);
}


//Find valid OR that can be issued at slot.
//Return true if avaiable issue ors found.
//'cand_list': list of candidate operations which can be issue at this cycle.
//'slot': slot need to fill
//'issue_or': record issued ors selected.
//'change_slot': set to true indicate the routine allows modification
//  of operations in other slot. Note that the modification may
//  change the function unit of operation.
bool LIS::selectIssueOR(
        IN ORList & cand_list,
        SLOT slot,
        OUT OR * issue_ors[SLOT_NUM],
        bool change_slot)
{
    ASSERTN(m_pool, ("uninitialized"));
    OR_HASH valid_cands;
    OR * cflct_ors[SLOT_NUM] = {0}; //record conflict ors.
    for (OR * o = cand_list.get_head();
         o != NULL; o = cand_list.get_next()) {
        if (canBeIssued(o, issue_ors, slot, change_slot, cflct_ors)) {
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
void LIS::updateIssueORs(
        IN OUT ORList & cand_list,
        SLOT slot,
        IN OR * issue_or,
        IN OUT OR * issue_ors[SLOT_NUM])
{
    issue_ors[slot - FIRST_SLOT] = issue_or;
    cand_list.remove(issue_or);
}


//When we could not find any o that can be issued at
//'to_slot', the function will be call to try to move
//the operations have been selected to 'to_slot'.
//
//e.g:Now the ready ors are LW, MUL, the slot are MEM, AU,
//    and LW can issue at slot MEM, MUL can issue at slot
//    both AU and MEM.
//    Suppose we have chosen MUL for slot MEM, then there is
//    not any candidate for slot AU because LW only can be
//    issued at MEM. So we are going to move MUL at slot AU,
//    and reschedul operation for slot MEM.
//    before roll back:
//        {MUL | nop}
//        {Load  | nop}
//    after roll back:
//        {Load  | MUL}
SLOT LIS::rollBackORs(
        bool be_changed[SLOT_NUM],
        OR * issue_ors[SLOT_NUM],
        SLOT to_slot)
{
    ASSERT0(issue_ors[to_slot] == NULL);
    for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
        if (!be_changed[i] && issue_ors[i] != NULL) {
            if (!changeSlot(issue_ors[i], to_slot)) {
                continue;
            }
            ASSERTN(i != (UINT)to_slot, ("illegal!"));
            m_or_changed = true;
            issue_ors[to_slot] = issue_ors[i];
            issue_ors[i] = NULL;
            be_changed[to_slot] = true;
            return (SLOT)i;
        }
    }
    return (SLOT)(to_slot + 1);
}


//Select instructions to fill issue slot as much as possible.
//Return true if the candidate has been found.
bool LIS::selectIssueORs(IN ORList & cand_list, OUT OR * issue_ors[SLOT_NUM])
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
        if (selectIssueOR(cand_list, slot, issue_ors, m_is_change_slot)) {
            find = true;
        } else if (m_is_change_slot) {
            next_slot = rollBackORs(be_changed, issue_ors, slot);
        }
    }
    return find;
}


//Choose candidate OR from ready-list, and fill available issue slot.
//Return true if DDG node has been removed, and ready-list should
//be recomputed.
//
//'stepddg': computing ready instructions step by step during scheduling.
//
//About fill branch shadow:
//    When GIS is calling it, don't try to schedule the operation
//    that has moved from a different block that do not
//    dominate/post-dominate current BB in the delay slot.
//    Frequent observance is that this will unneccessarily
//    restrict further code motion.
bool LIS::fillIssueSlot(DataDepGraph & stepddg)
{
    ASSERTN(m_pool, ("uninitialized"));
    ORList cand_list;

    //Find ors can be issued at current cycle.
    INT c;
    for (OR * o = m_ready_list.get_first(c);
         o != NULL; o = m_ready_list.get_next(c)) {
        if (isIssueCand(o)) {
            cand_list.append_tail(o);
        }
    }

    //Select appropriate issue OR for each slot.
    bool ddg_node_removed = false;

    //Record selected OR that is ready to issue, and these ORs
    //will not has conflict with ones already issued both in
    //hardware resources and data dependences.
    OR * issue_ors[SLOT_NUM] = {0};
    if (cand_list.get_elem_count() > 0) {
        if (selectIssueORs(cand_list, issue_ors)) {
            for (UINT i = FIRST_SLOT; i < SLOT_NUM; i++) {
                if (issue_ors[i] == NULL) { continue; }
                if (m_br_ord != NULL &&
                    issue_ors[i] == ORDESC_or(m_br_ord)) {
                    //Branch-OR is ready to schedule!
                    ORDESC_start_cyc(m_br_ord) = m_sim->getCurCycle();
                }
                if (m_sim->issue(issue_ors[i], (SLOT)(FIRST_SLOT + i))) {
                    m_ready_list.removed(issue_ors[i]);
                }
                stepddg.removeOR(issue_ors[i]);
                ddg_node_removed = true;
            }
        }
    }
    m_sim->runOneCycle(NULL);
    return ddg_node_removed;
}


INT LIS::dcache_miss_rate(OR *)
{
    return 0;
}


INT LIS::dcache_miss_penalty(OR *)
{
    return 0;
}


//Select an operation with highest priority.
OR * LIS::selectBestORByPriority(OR_HASH & cand_hash)
{
    OR * best_cand = NULL;
    float best_prio = 0.0;
    INT c;
    for (OR * o = cand_hash.get_first(c);
         o != NULL; o = cand_hash.get_next(c)) {
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


void LIS::dump(CHAR * name, bool is_del, bool need_exec_detail)
{
    m_sim->dump(name, is_del, need_exec_detail);
}


//Compute the execution time for instructions in order.
//This function does not change any function unit.
void LIS::serialize()
{
    ASSERT0(m_bb && m_ddg);
    DataDepGraph * stepddg = m_cg->allocDDG(false); //Local used.
    stepddg->init(m_bb);
    stepddg->clone(*m_ddg);
    stepddg->computeEstartAndLstart(*m_sim, NULL);

    INT last_res_cyc = 0;
    if (m_br_ord != NULL) {
        last_res_cyc = ORSI_last_result_avail_cyc(
            ORDESC_or_sche_info(m_br_ord));
    }

    xcom::C<OR*> * container = NULL;
    for (ORBB_orlist(m_bb)->get_head(&container);
         container != ORBB_orlist(m_bb)->end();
         ORBB_orlist(m_bb)->get_next(&container)) {
        m_ready_list.append(container->val());
        while (!fillIssueSlot(*stepddg)) {;}
    }

    if (m_br_ord != NULL) {
        UINT end_cyc = m_sim->getCurCycle() - 1;
        INT br_start_cyc = ORDESC_start_cyc(m_br_ord);
        ASSERT0(br_start_cyc >= 0);
        INT pad = br_start_cyc + (last_res_cyc - 1) - end_cyc;
        ASSERT0(pad >= 0);
        for (INT i = 0; i < pad; i++) {
            m_sim->runOneCycle(NULL);
        }
    }

    delete stepddg;
}


//Performing scheduling of basic block.
//Return true if some instructions changed their register file.
bool LIS::schedule()
{
    ASSERTN(m_pool, ("uninitialized"));
    DataDepGraph * stepddg = m_cg->allocDDG(false);
    stepddg->init(m_bb);
    stepddg->clone(*m_ddg);
    stepddg->computeEstartAndLstart(*m_sim, NULL);
    Vector<bool> visited;
    m_or_changed = false;

    INT last_res_cyc = 0;
    if (m_br_ord != NULL) {
        ORDESC_start_cyc(m_br_ord) = -1;
        ORList tmp;
        stepddg->getPredsByOrderTraverseNode(tmp, ORDESC_or(m_br_ord));
        for (OR * o = tmp.get_head(); o != NULL; o = tmp.get_next()) {
            m_br_all_preds.append(o);
        }
        m_br_all_preds.append(ORDESC_or(m_br_ord));

        last_res_cyc = ORSI_last_result_avail_cyc(ORDESC_or_sche_info(m_br_ord));
    }

RESCH:
    computeReadyList(*stepddg, visited, true);
    while (stepddg->getVertexNum() > 0 || !m_sim->done()) {
        fillIssueSlot(*stepddg);
        computeReadyList(*stepddg, visited, true);
    }

    if (m_br_ord != NULL) {
        UINT end_cyc = m_sim->getCurCycle() - 1;
        INT br_start_cyc = ORDESC_start_cyc(m_br_ord);
        ASSERT0(br_start_cyc >= 0);
        if (br_start_cyc <= (INT)(end_cyc - last_res_cyc)) {
            //Once again, 'max_try' can not less than 'last_res_cyc'.
            INT max_try = last_res_cyc +
                          2; //tricky value, range between [0~N]
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
    return m_or_changed;
}

} //namespace xgen
