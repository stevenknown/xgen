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

#define DDG_DUMP_CLUSTER_INFO 0x1
#define DDG_DUMP_EDGE_INFO 0x2
#define DDG_DUMP_OP_INFO 0x4
#define DDG_DELETE 0x8
//#define MEM_DEP_HAS_CONSTRUCTED

DataDepGraph::DataDepGraph()
{
    m_is_init = false;
    m_is_clonal = false;
    m_ppm = nullptr; //Parallel part manager
    m_pool = smpoolCreate(32, MEM_COMM);
    setParam(DEP_MEM_FLOW|DEP_MEM_OUT|DEP_CONTROL|DEP_REG_ANTI|
             DEP_MEM_ANTI|DEP_SYM_REG);
    //OR's id is not dense integer, thus do NOT apply dense storage.
    //set_dense(true);
}


DataDepGraph::~DataDepGraph()
{
    destroy();
    smpoolDelete(m_pool);
}


void DataDepGraph::setParallelPartMgr(ParallelPartMgr * ppm)
{
    m_ppm = ppm;
}


void * DataDepGraph::xmalloc(UINT size)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (size == 0) { return nullptr; }
    ASSERTN(m_pool, ("need graph pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    if (p == nullptr) { return nullptr; }
    ::memset(p, 0, size);
    return p;
}


void DataDepGraph::init(ORBB * bb)
{
    if (m_is_init) { return; }

    xcom::Graph::init();
    m_bb = bb;
    m_cg = nullptr;
    m_ormgr = nullptr;
    if (bb != nullptr) {
        ASSERT0(ORBB_cg(bb));
        m_cg = ORBB_cg(bb);
        m_ormgr = m_cg->getORMgr();
    }

    set_unique(true);
    set_direction(true);

    //The default is false in order to reduce number of dep-edge.
    m_unique_mem_loc = false;
    m_params_stack.init();
    m_estart.init();
    m_lstart.init();

    //Dependence Graph of compiler constructed has only one
    //global-structure which is exclusive. It leads to ddg can not be built
    //paralleledly.
    //Build dependence graph first.
    //TODO: processing the memory alias info.
    //DEPComputeGraph(bb,
    //                m_ddg_param.phy_reg_dep,
    //                m_ddg_param.cycd,
    //                m_ddg_param.mem_read_read_dep,
    //                m_ddg_param.mem_flow_dep,
    //                m_ddg_param.cd,
    //                nullptr); //Last parameter is meaning only
    //                       //if the cyclic-dep is built.

    //Compute every OR estart and lstart in terms of dep-graph.

    m_is_init = true;
}


void DataDepGraph::destroy()
{
    if (!m_is_init) { return; }
    m_estart.destroy();
    m_lstart.destroy();
    m_params_stack.destroy();
    xcom::Graph::destroy();
    m_bb = nullptr;
    m_ppm = nullptr;
    m_is_init = false;
    m_is_clonal = false;
}


//Add edge between or in 'orlist' and 'tgt'.
void DataDepGraph::unifyEdge(List<OR*> & orlist, OR * tgt)
{
    for (OR * o = orlist.get_head(); o != nullptr; o = orlist.get_next()) {
        if (o == tgt) { //edge cyclic
            continue;
        }
        if (ORBB_orlist(m_bb)->is_or_precedes(o, tgt)) {
            appendEdge(DEP_HYB, o, tgt);
        } else {
            appendEdge(DEP_HYB, tgt, o);
        }
    }
}


void DataDepGraph::fullyConnectPredAndSucc(OR * o)
{
    xcom::Vertex * v = getVertex(o->id());
    ASSERT0(v);
    xcom::EdgeC * pred_lst = v->getInList();
    xcom::EdgeC * succ_lst = v->getOutList();
    while (pred_lst) {
        OR * from = getOR(pred_lst->getFromId());
        xcom::EdgeC * tmp_succ_lst = succ_lst;
        while (tmp_succ_lst) {
            OR * to = getOR(tmp_succ_lst->getToId());
            appendEdge(DEP_HYB, from, to);
            tmp_succ_lst = EC_next(tmp_succ_lst);
        }
        pred_lst = EC_next(pred_lst);
    }
}


void DataDepGraph::reschedul()
{
    if (!m_is_init) { return; }
    ASSERTN(m_bb, ("xcom::Graph still not yet initialize."));
    ASSERTN(!m_is_clonal,
            ("Since the limitation of compiler, "
             "clonal DDG does not allow rescheduling."));
    //Do NOT destroy 'm_param_stack'.

    //Clean graph
    erase();
    set_unique(true);
    set_direction(true);
    //Build dependence graph first.
    //DEPComputeGraph(m_bb,
    //                    m_ddg_param.phy_reg_dep,
    //                    m_ddg_param.cycd,
    //                    m_ddg_param.mem_read_read_dep,
    //                    m_ddg_param.mem_flow_dep,
    //                    m_ddg_param.cd,
    //                    nullptr);//Last parameter is meaning only if the
    //                          //cyclic-dep is built.

    //Compute every o estart and lstart in terms of dep-graph.

    build();
}


void DataDepGraph::rebuild()
{
    if (!m_is_init) { return; }
    ASSERTN(m_bb, ("xcom::Graph still not yet initialize."));
    ASSERTN(!m_is_clonal,
            ("Since the limitation of some compiler, "
             "clonal DDG does not allow rescheduling."));

    //Clean graph
    erase();
    set_unique(true);
    set_direction(true);

    reschedul(); //for CG DEP rebuild.
    build();
}


//Compute the slack range by lstart - estart.
INT DataDepGraph::get_slack(OR const* o) const
{
    ASSERTN(m_is_init && o, ("xcom::Graph still not yet initialize."));
    ASSERT0(m_lstart.get(o->id()) >= m_estart.get(o->id()));
    return m_lstart.get(o->id()) - m_estart.get(o->id());
}


//Return true if OR is on the critical path.
bool DataDepGraph::isOnCriticalPath(OR const* o) const
{
    ASSERTN(m_is_init && o, ("xcom::Graph still not yet initialize."));
    return m_estart.get(o->id()) == m_lstart.get(o->id());
}


UINT DataDepGraph::computeCriticalPathLen(BBSimulator const& sim) const
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    UINT max = 0;
    ORCt * ct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o;
         o = ORBB_orlist(m_bb)->get_next(&ct)) {
        UINT estart = m_estart.get(o->id());
        UINT lstart = m_lstart.get(o->id());
        if (estart == lstart) {
            UINT l = estart + sim.getExecCycle(o);
            if (max < l) {
                max = l;
            }
        }
    }
    return max;
}


//Return true if there is a data dependence between o1 and o2.
bool DataDepGraph::is_dependent(OR const* o1, OR const* o2) const
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (getEdge(o1->id(), o2->id())) {
        return true;
    }
    return false;
}


//Return true if actually add new edge.
bool DataDepGraph::appendEdge(ULONG deptype, OR const* from, OR const* to)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (!has_reg_read_dep() && deptype == DEP_REG_READ) {
        //Omit RAR dep.
        return false;
    }
    xcom::Edge * e = addEdge(from->id(), to->id());
    if (EDGE_info(e) == nullptr) {
        EDGE_info(e) = (DDGEdgeInfo*)xmalloc(sizeof(DDGEdgeInfo));
    }
    DDGEI_deptype(EDGE_info(e)) |= deptype;
    return true;
}


void DataDepGraph::removeEdge(OR * from, OR * to)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    removeEdge(from->id(), to->id());
}


void DataDepGraph::removeEdge(UINT from, UINT to)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    xcom::Graph::removeEdge(getEdge(from, to));
}


//Build edges traversing each successor via DFS.
//The algo may be slowly when edges are messly.
void DataDepGraph::traverse()
{
    ORCt * ct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o != nullptr;
         o = ORBB_orlist(m_bb)->get_next(&ct)) {
        ORList succs;
        getSuccsByOrderTraverseNode(succs, o);
        for (OR *succ = succs.get_head();
             succ != nullptr; succ = succs.get_next()) {
            appendEdge(DEP_HYB, o, succ);
        }
    }
}


//Return true if o1 is independent with o2.
bool DataDepGraph::is_independent(OR const* o1, OR const* o2) const
{
    ASSERTN(m_is_init && m_ppm, ("xcom::Graph still not yet initialize."));
    CLUST clst1 = m_cg->computeORCluster(o1);
    CLUST clst2 = m_cg->computeORCluster(o2);
    if (clst1 == clst2 || clst1 == CLUST_UNDEF || clst2 == CLUST_UNDEF) {
        return false;
    }

    if (!m_ppm->hasParallelPart(clst1) || !m_ppm->hasParallelPart(clst2)) {
        //CASE: GRA or other phase after AutoParallelization generate new code
        //which assigned unmapped cluster.
        return false;
    }

    xcom::BitSet * or_idxset1 = nullptr;
    xcom::BitSet * or_idxset2 = nullptr;
    if (or_idxset1 == nullptr || or_idxset2 == nullptr) {
        return false;
    }

    if (or_idxset1->is_contain(o1->id()) &&
        or_idxset2->is_contain(o2->id())) {
        return true;
    }
    return false;
}


bool DataDepGraph::mustDefSP(OR const*) const
{
    ASSERTN(0, ("Target Dependent Code"));
    return false;
}


void DataDepGraph::removeRedundantDep()
{
    xcom::Edge * next;
    EdgeIter c;
    for (xcom::Edge * e = get_first_edge(c); e != nullptr; e = next) {
        next = get_next_edge(c);
        if (EDGE_info(e) == nullptr) {
            continue;
        }
        OR * from = getOR(VERTEX_id(EDGE_from(e)));
        OR * to = getOR(VERTEX_id(EDGE_to(e)));
        ULONG deptype = DDGEI_deptype(EDGE_info(e));
        ASSERT0(from && to);

        //BB_exit_sp_adj_or(m_bb)
        if ((from->is_mem() &&
             (to->isSpadjust() || to->is_bus())) ||
            (to->is_mem() &&
             (from->isSpadjust() || from->is_bus()))) {
            if (HAVE_FLAG(deptype, DEP_HYB) &&
                !ONLY_HAVE_FLAG(deptype, DEP_HYB)) {
                //CASE:
                //  Remove redundant DEP-HYB.
                //  SR246(r7) :- lw_b TN73(sp3) (gra_spill_temp_113+0)
                //  SR57(sp2), SR73(sp3), :- bus_m1_to_m2_b1 GSR49(sp)
                //  CG_DEP_MISC edge exist for keeping memory
                //  implied depdendence, but it is
                //  redundant dependence if there are another dep-edges.
                REMOVE_FLAG(deptype, DEP_HYB);
                DDGEI_deptype(EDGE_info(e)) = deptype;
            }
        }

        if (from->is_mem() &&
            ONLY_HAVE_FLAG(deptype, DEP_HYB) &&
            !to->is_side_effect() &&
            !to->is_volatile() &&
            !from->is_side_effect() &&
            !from->is_volatile() &&
            (!to->is_fake() ||
             to->is_bus() ||
             to->isSpadjust()) &&
            !to->is_mem()) {

            if (mustDefSP(to)) {
                //With the handling of CG dependence analysis module, the HYB
                //edge that in order to keep relation of the memory operation
                //and the modifying SP/FP operation is redundant.
                SR * base_sr = from->get_mem_base();
                if (!m_cg->mustDef(to, base_sr)) {
                    //Not Reg-FLOW dep
                    removeEdge(from, to);
                    continue;
                }
            }
        }

        if (to->is_mem() &&
            ONLY_HAVE_FLAG(deptype, DEP_HYB) &&
            !to->is_side_effect() &&
            !to->is_volatile() &&
            !from->is_side_effect() &&
            !from->is_volatile() &&
            (!from->is_fake() ||
             from->is_bus() ||
             from->isSpadjust()) &&
            !from->is_mem()) {

            if (mustDefSP(from)) {
                //With the handling of CG dependence analysis module, the HYB
                //edge that in order to keep relation of the memory operation
                //and the modifying SP/FP operation is redundant.
                SR * base_sr = to->get_mem_base();
                if (!m_cg->mustDef(from, base_sr)) {
                    //Not Reg-FLOW dep
                    removeEdge(from, to);
                    continue;
                }
            }
        }
    }
}


//Return true if 'sr' need to be processed.
//If return true, 'sr' and 'or' will be analyzed to build
//dependence if there exist use-OR or def-OR.
//The decision always used to reduce compilation time and memory.
bool DataDepGraph::handleDedicatedSR(SR const*, OR const*, bool is_result) const
{
    DUMMYUSE(is_result);
    return true;
}


//Build dependence of physical register.
void DataDepGraph::handleResultsPhyRegOut(OR const* o, SR const* sr,
                                          OUT Reg2ORList & map_reg2defors,
                                          OUT Reg2ORList & map_reg2useors)
{
    ASSERT0(o && sr);
    Reg reg = sr->getPhyReg();
    ASSERT0(reg != REG_UNDEF);

    m_tmp_alias_regset.clean();
    collectAliasRegSet(reg, m_tmp_alias_regset);
    for (BSIdx i = m_tmp_alias_regset.get_first();
         i != BS_UNDEF; i = m_tmp_alias_regset.get_next(i)) {
        handleResultsPhyRegOut(o, (Reg)i, map_reg2defors, map_reg2useors);
    }
}


//Build dependence of physical register.
void DataDepGraph::handleResultsPhyRegOut(OR const* o, Reg reg,
                                          OUT Reg2ORList & map_reg2defors,
                                          OUT Reg2ORList & map_reg2useors)
{
    ASSERT0(o && reg != REG_UNDEF);
    ConstORList * orlst = map_reg2defors.get(reg);
    if (orlst != nullptr) {
        //Build REG_OUT dependence.
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            if (defor == o) {
                continue;
            }
            appendEdge(DEP_REG_OUT, defor, o);
        }
    }

    orlst = map_reg2useors.get(reg);
    if (orlst != nullptr) {
        //Build REG_ANTI dependence.
        for (OR const* useor = orlst->get_head();
             useor != nullptr; useor = orlst->get_next()) {
            if (useor == o) {
                continue;
            }
            appendEdge(DEP_REG_ANTI, useor, o);
        }
    }

    map_reg2defors.set(reg, o);

    //Kill all USE ors.
    map_reg2useors.clean(reg);
}


void DataDepGraph::handleResultsSymRegOut(OR const* o, SR const* sr,
                                          OUT SR2ORList & map_sr2defors,
                                          OUT SR2ORList & map_sr2useors)
{
    ASSERT0(o && sr);
    SR * tsr = const_cast<SR*>(sr);
    ConstORList * orlst = map_sr2defors.get(tsr);
    if (orlst != nullptr) {
        //Build REG_OUT dependence.
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            if (defor == o) {
                continue;
            }
            appendEdge(DEP_REG_OUT, defor, o);
        }
    }

    orlst = map_sr2useors.get(tsr);
    if (orlst != nullptr) {
        //Build REG_ANTI dependence.
        for (OR const* useor = orlst->get_head();
             useor != nullptr; useor = orlst->get_next()) {
            if (useor == o) {
                continue;
            }
            appendEdge(DEP_REG_ANTI, useor, o);
        }
    }

    map_sr2defors.set(tsr, o);

    //Kill all USE ors.
    map_sr2useors.clean(tsr);
}


void DataDepGraph::handleResults(OR const* o,
                                 OUT Reg2ORList & map_reg2defors,
                                 OUT Reg2ORList & map_reg2useors,
                                 OUT SR2ORList & map_sr2defors,
                                 OUT SR2ORList & map_sr2useors)
{
    for (UINT i = 0; i < o->result_num(); i++) {
        SR const* sr = o->get_result(i);
        if (sr == m_cg->getTruePred() ||
            !sr->is_reg()) {
            continue;
        }

        if (m_cg->isDedicatedSR(sr) && !handleDedicatedSR(sr, o, true)) {
            continue;
        }

        if (has_phy_reg_dep() && sr->getPhyReg() != REG_UNDEF) {
            handleResultsPhyRegOut(o, sr, map_reg2defors, map_reg2useors);
        }

        if (has_sym_reg_dep()) {
            handleResultsSymRegOut(o, sr, map_sr2defors, map_sr2useors);
        }
    }
}


void DataDepGraph::collectAliasRegSet(Reg reg, OUT xgen::RegSet & alias_regset)
{
    alias_regset.bunion(reg);
}


//Build dependence of physical register.
void DataDepGraph::handleOpndsPhyRegFlow(OR const* o, SR const* sr,
                                         OUT Reg2ORList & map_reg2defors,
                                         OUT Reg2ORList & map_reg2useors)
{
    ASSERT0(o && sr);
    Reg reg = sr->getPhyReg();
    ASSERT0(reg != REG_UNDEF);

    m_tmp_alias_regset.clean();
    collectAliasRegSet(reg, m_tmp_alias_regset);
    for (BSIdx i = m_tmp_alias_regset.get_first();
         i != BS_UNDEF; i = m_tmp_alias_regset.get_next(i)) {
        handleOpndsPhyRegFlow(o, (Reg)i, map_reg2defors, map_reg2useors);
    }
}


void DataDepGraph::handleOpndsPhyRegFlow(OR const* o, Reg reg,
                                         OUT Reg2ORList & map_reg2defors,
                                         OUT Reg2ORList & map_reg2useors)
{
    ASSERT0(reg != REG_UNDEF);
    ConstORList * orlst = map_reg2defors.get(reg);
    if (orlst != nullptr) {
        //Build REG_FLOW dependence.
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            appendEdge(DEP_REG_FLOW, defor, o);
        }
    }
    if (has_reg_read_dep()) {
        ConstORList * orlst2 = map_reg2useors.get(reg);
        if (orlst2 != nullptr) {
            //Build REG_READ dependence.
            for (OR const* useor = orlst2->get_head();
                 useor != nullptr; useor = orlst2->get_next()) {
                if (useor == o) {
                    continue;
                }
                appendEdge(DEP_REG_READ, useor, o);
            }
        }
    }
    map_reg2useors.append(reg, const_cast<OR*>(o));
}


void DataDepGraph::handleOpndsSymRegRead(OR const* o, SR const* sr,
                                         OUT SR2ORList & map_sr2defors,
                                         OUT SR2ORList & map_sr2useors)
{
    ASSERT0(o && sr);
    SR * tsr = const_cast<SR*>(sr);
    ConstORList * orlst = map_sr2defors.get(tsr);
    if (orlst != nullptr) {
        //Build REG_FLOW dependence.
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            appendEdge(DEP_REG_FLOW, defor, o);
        }
    }
    if (has_reg_read_dep()) {
        //Build REG_READ dependence.
        ConstORList * orlst2 = map_sr2useors.get(tsr);
        if (orlst2 != nullptr) {
            for (OR const* useor = orlst2->get_head();
                 useor != nullptr; useor = orlst2->get_next()) {
                if (useor == o) {
                    continue;
                }
                appendEdge(DEP_REG_READ, useor, o);
            }
        }
    }
    map_sr2useors.append(tsr, const_cast<OR*>(o));
}


void DataDepGraph::handleOpnds(OR const* o,
                               OUT Reg2ORList & map_reg2defors,
                               OUT Reg2ORList & map_reg2useors,
                               OUT SR2ORList & map_sr2defors,
                               OUT SR2ORList & map_sr2useors)
{
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR const* sr = o->get_opnd(i);
        if (sr == m_cg->getTruePred() || !sr->is_reg()) {
            continue;
        }

        if (m_cg->isDedicatedSR(sr) && !handleDedicatedSR(sr, o, false)) {
            continue;
        }

        if (has_phy_reg_dep() && sr->getPhyReg() != REG_UNDEF) {
            handleOpndsPhyRegFlow(o, sr, map_reg2defors, map_reg2useors);
        }

        if (has_sym_reg_dep()) {
            handleOpndsSymRegRead(o, sr, map_sr2defors, map_sr2useors);
        }
    }
}


void DataDepGraph::buildRegDep()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (ORBB_ornum(m_bb) == 0) { return; }

    Reg2ORList map_reg2defors;
    Reg2ORList map_reg2useors;
    SR2ORList map_sr2defors;
    SR2ORList map_sr2useors;
    ORCt * ct;
    for (((List<OR*>*)m_bb->getORList())->get_head(&ct);
         ct != ORBB_orlist(m_bb)->end();
         ct = ((List<OR*>*)ORBB_orlist(m_bb))->get_next(ct)) {
        OR * o = ct->val();
        addVertex(o->id());
        handleOpnds(o, map_reg2defors, map_reg2useors,
                     map_sr2defors, map_sr2useors);
        handleResults(o, map_reg2defors, map_reg2useors,
                       map_sr2defors, map_sr2useors);
    }
}


//Return memory operations which may access same memory location with node.
void DataDepGraph::getORListWhichAccessSameMem(OUT ORList & mem_ors,
                                               OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    mem_ors.clean();
    ASSERTN(o, ("Node:%d is not on DDG.", o->id()));
    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) { return; }

    List<xcom::Vertex*> worklst;
    ORList tmplst;
    DefMiscBitSetMgr sm;
    DefSBitSet visited(sm.getSegMgr());

    worklst.append_tail(v);
    visited.bunion(v->id());
    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * head = worklst.remove_head();
        //Add succ nodes.
        xcom::EdgeC * el = head->getOutList();
        while (el != nullptr) {
            xcom::Edge * e = EC_edge(el);
            el = EC_next(el);
            if (EDGE_info(e) == nullptr) {
                continue;
            }
            DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
            if (HAVE_FLAG(deptype, DEP_MEM_OUT) ||
                HAVE_FLAG(deptype, DEP_MEM_READ) ||
                HAVE_FLAG(deptype, DEP_MEM_FLOW) ||
                HAVE_FLAG(deptype, DEP_MEM_ANTI) ||
                HAVE_FLAG(deptype, DEP_MEM_VOL)) {
                xcom::Vertex * to = EDGE_to(e);
                if (!visited.is_contain(to->id())) {
                    worklst.append_tail(to);
                    visited.bunion(to->id());
                }
            }
        }

        if (m_unique_mem_loc) {
            //The memory location is unique, and each of location
            //descripting the individual memory.
            //case: 'z' alias with both [sp+0] and [sp+4],
            //  so there are two dep-groups and each of the group own the o[0].
            //    dep-group1:
            //    from:[0]sr231 <- load  sr49(sp) 'z'
            //    to  :[8]      <- store sr234 sr49(sp) #0

            //Add pred nodes.
            el = VERTEX_in_list(head);
            while (el != nullptr) {
                xcom::Edge * e = EC_edge(el);
                el = EC_next(el);
                if (EDGE_info(e) == nullptr) {
                    continue;
                }

                DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
                if (HAVE_FLAG(deptype, DEP_MEM_OUT) ||
                    HAVE_FLAG(deptype, DEP_MEM_READ) ||
                    HAVE_FLAG(deptype, DEP_MEM_FLOW) ||
                    HAVE_FLAG(deptype, DEP_MEM_ANTI) ||
                    HAVE_FLAG(deptype, DEP_MEM_VOL)) {
                    xcom::Vertex * from = EDGE_from(e);
                    if (!visited.is_contain(from->id())) {
                        worklst.append_tail(from);
                        visited.bunion(from->id());
                    }
                }
            }
        }

        OR * head_or = getOR(head->id());
        ASSERT0(head_or->is_mem());
        tmplst.append_tail(head_or);
    }

    //Sequencing
    for (OR * succ = tmplst.get_head();
         succ != nullptr; succ = tmplst.get_next()) {
        OR * tmp = nullptr;
        for (tmp = mem_ors.get_head(); tmp != nullptr; tmp = mem_ors.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, tmp)) {
                break;
            }
        }

        ASSERTN(false == mem_ors.find(succ), ("Repetitive o"));

        if (tmp != nullptr) {
            mem_ors.insert_before(succ, tmp);
        } else {
            mem_ors.append_tail(succ);
        }
    }
}


//OR is LOAD.
void DataDepGraph::handleLoad(OR const* o, ULONG mem_loc_idx,
                              OUT UINT2ConstORList & map_memloc2defors,
                              OUT UINT2ConstORList & map_memloc2useors)
{
    ASSERT0(o->is_load());
    //Processing DEF of memory location.
    ConstORList * orlst = map_memloc2defors.get(mem_loc_idx);
    if (orlst != nullptr) {
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            ASSERT0(defor->is_store());
            appendEdge(DEP_MEM_FLOW, defor, o);
        }
    }

    if (has_mem_read_dep()) {
        //Processing USE of memory location.
        ConstORList * orlst2 = map_memloc2useors.get(mem_loc_idx);
        if (orlst2 != nullptr) {
            for (OR const* useor = orlst2->get_head();
                 useor != nullptr; useor = orlst2->get_next()) {
                ASSERT0(useor->is_load());
                appendEdge(DEP_MEM_READ, useor, o);
            }
        }
    }

    map_memloc2useors.set(mem_loc_idx, o);
}


//OR is STORE.
void DataDepGraph::handleStore(OR const* o, ULONG mem_loc_idx,
                               OUT UINT2ConstORList & map_memloc2defors,
                               OUT UINT2ConstORList & map_memloc2useors)
{
    ASSERT0(o->is_store());

    //Processing DEF of memory location.
    ConstORList * orlst = map_memloc2defors.get(mem_loc_idx);
    if (orlst != nullptr) {
        for (OR const* defor = orlst->get_head();
             defor != nullptr; defor = orlst->get_next()) {
            ASSERT0(defor->is_store());
            appendEdge(DEP_MEM_OUT, defor, o);
        }
    }

    if (has_mem_read_dep()) {
        //Processing USE of memory location.
        ConstORList * orlst2 = map_memloc2useors.get(mem_loc_idx);
        if (orlst2 != nullptr) {
            for (OR const* useor = orlst2->get_head();
                 useor != nullptr; useor = orlst2->get_next()) {
                ASSERT0(useor->is_load());
                appendEdge(DEP_MEM_ANTI, useor, o);
            }
        }
    }

    map_memloc2defors.set(mem_loc_idx, o);
}


#ifdef MEM_DEP_HAS_CONSTRUCTED
void DataDepGraph::buildMemDep()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (ORBB_ornum(m_bb) == 0) { return; }

    UINT2ConstORList map_memloc2defors;
    UINT2ConstORList map_memloc2useors;
    ULONG mem_loc_idx = 1;
    DefSBitSet visited;
    ORCt * ct;
    ORList mem_ors;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_next(&ct)) {
        if (!o->is_mem() || visited.is_contain(o->id())) { continue; }

        mem_ors.clean();
        //mem-ors have been sorted by order.
        getORListWhichAccessSameMem(mem_ors, o);
        for (OR * mem = mem_ors.get_head(); mem != nullptr;
             mem = mem_ors.get_next()) {
            ASSERT0(!m_unique_mem_loc || //mem loc is unique
                    !visited.get(mem->id()));
            ASSERT0(mem->is_mem());

            if (mem->is_load()) {
                handleLoad(mem, mem_loc_idx, map_memloc2defors,
                            map_memloc2useors);
            }

            if (mem->is_store()) {
                handleStore(mem, mem_loc_idx, map_memloc2defors,
                             map_memloc2useors);
            }

            visited.set(mem->id(), true);
        }
        mem_loc_idx++;
    }
}
#else
void DataDepGraph::buildMemDep()
{
    if (!has_mem_flow_dep() &&
        !has_mem_out_dep() &&
        !has_mem_anti_dep() &&
        !has_mem_read_dep()) {
        return;
    }

    ORCt * ct;
    List<OR*> mem_ors;

    //Pick out memory OR.
    for (m_bb->getORList()->get_head(&ct);
         ct != m_bb->getORList()->end();
         ct = ((List<OR*>*)m_bb->getORList())->get_next(ct)) {
        OR * o = ct->val();
        if (o->is_mem()) {
            mem_ors.append_tail(o);
        }
    }

    for (mem_ors.get_head(&ct);
         ct != mem_ors.end(); ct = mem_ors.get_next(ct)) {
        ORCt * to_ct = ct;
        OR * from = ct->val();
        Var const* from_loc = m_cg->computeSpillVar(from);
        for (OR * to = mem_ors.get_next(&to_ct);
             to != nullptr; to = mem_ors.get_next(&to_ct)) {
            if (has_mem_flow_dep()) { //flow-dependence
                buildMemFlowDep(from, to);
            }

            if (has_mem_out_dep()) { //out-dependence
                buildMemOutDep(from, to, from_loc);
            }

            if (has_mem_anti_dep()) { //anti
                //Do not need MEM ANTI dependence.
                continue;
            }

            if (has_mem_read_dep()) { //read-dependence
                buildMemInDep(from, to, from_loc);
            }

            //Memory-read-dependence (volatile data): the succ and pred
            //both read the same memory location, and the reference order
            //must be preserved.
            buildMemVolatileDep(from, to);
        }
    }
}
#endif


//This function will establish dependency for memory operation one by one
//rather than doing any analysis which is in order to speedup compilation.
void DataDepGraph::buildMemDepWithOutAnalysis()
{
    ORCt * ct;
    OR const* prev = nullptr;
    for (m_bb->getORList()->get_head(&ct);
         ct != m_bb->getORList()->end();
         ct = ((List<OR*>*)m_bb->getORList())->get_next(ct)) {
        OR const* o = ct->val();
        if (o->is_mem()) {
            if (prev != nullptr) {
                appendEdge(DEP_HYB, prev, o);
            }
            prev = o;
        }
    }
}


void DataDepGraph::buildMemOutDep(OR const* from, OR const* to,
                                  Var const* from_loc)
{
    Var const* to_loc = m_cg->computeSpillVar(to);

    if (from_loc != nullptr && to_loc != nullptr) {
        if (from_loc != to_loc) {
            //Not same spill-loc.
            return;
        }
    } else if (from_loc != nullptr || to_loc != nullptr) {
        //Normal memory location in load/store does not
        //alias with spill-memory-location.
        return;
    }

    //Build dependence for conservation.
    appendEdge(DEP_MEM_OUT, from, to);
}


void DataDepGraph::buildMemFlowDep(OR const* from, OR const* to)
{
    appendEdge(DEP_MEM_FLOW, from, to);
}


void DataDepGraph::buildMemInDep(OR const*, OR const*, Var const*)
{
    ASSERTN(0, ("TODO"));
}


void DataDepGraph::buildMemVolatileDep(OR const* from, OR const* to)
{
    appendEdge(DEP_MEM_VOL, from, to);
}


void * DataDepGraph::cloneEdgeInfo(xcom::Edge * e)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if(!EDGE_info(e)) return nullptr;
    DDGEdgeInfo * dei =(DDGEdgeInfo*)xmalloc(sizeof(DDGEdgeInfo));
    ::memcpy(dei, EDGE_info(e), sizeof(DDGEdgeInfo));
    return (void*)dei;
}


void * DataDepGraph::cloneVertexInfo(xcom::Vertex * v)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if(!VERTEX_info(v)) return nullptr;
    DDGNodeInfo *vi = (DDGNodeInfo*)xmalloc(sizeof(DDGNodeInfo));
    ::memcpy(vi, VERTEX_info(v), sizeof(DDGNodeInfo));
    return (void*)vi;
}


void DataDepGraph::clone(DataDepGraph & ddg)
{
    ASSERTN(m_is_init, ("xcom::Graph already initialized."));
    m_bb = ddg.m_bb;
    m_ppm = ddg.m_ppm;
    m_ddg_param = ddg.m_ddg_param;
    m_estart.copy(ddg.m_estart);
    m_lstart.copy(ddg.m_lstart);
    xcom::Graph::clone((xcom::Graph&)ddg, true, true);
    m_is_clonal = true;
}


//Append node on graph.
void DataDepGraph::appendOR(OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERT0(o);
    addVertex(o->id());
}


//Remove node on graph.
void DataDepGraph::removeORIdx(UINT oridx)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    OR * o = getOR(oridx);
    removeOR(o);
}


//Remove node on graph.
void DataDepGraph::removeOR(OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERT0(o);
    removeVertex(o->id());
}


//Get first node on graph.
OR * DataDepGraph::getFirstOR(VertexIter & cur)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return getOR(VERTEX_id(get_first_vertex(cur)));
}


//Get next node on graph.
OR * DataDepGraph::getNextOR(VertexIter & cur)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return getOR(VERTEX_id(get_next_vertex(cur)));
}


//Return all predecessors with ordered lexicographicly.
void DataDepGraph::getPredsByOrder(MOD ORList & preds, IN OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();

    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) return;
    xcom::EdgeC * el = v->getInList();
    if (el == nullptr) return;

    while (el != nullptr) {
        OR * pred = getOR(el->getFromId());
        OR * marker;
        for (marker = preds.get_head(); marker; marker = preds.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(pred, marker)) {
                break;
            }
        }

        if (marker) {
            preds.insert_before(pred, marker);
        } else {
            preds.append_tail(pred);
        }
        el = EC_next(el);
    }
}


//Return all successors with ordered lexicographicly.
void DataDepGraph::getSuccsByOrder(MOD ORList & succs, IN OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != nullptr, ("Node:%d is not on DDG.", o->id()));
    succs.clean();

    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) { return; }

    for (xcom::EdgeC * el = v->getOutList(); el != nullptr; el = EC_next(el)) {
        OR * succ = getOR(el->getToId());
        ASSERTN(succ, ("DDG is invalid."));
        OR * marker;
        for (marker = succs.get_head(); marker; marker = succs.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, marker)) {
                break;
            }
        }

        if (marker != nullptr) {
            succs.insert_before(succ, marker);
        } else {
            succs.append_tail(succ);
        }
    }
}


//Return all dependent predecessors of OR.
//e.g:a->b->c->d, a->d
//  c and a are immediate predecessors of d, whereas a,b,c are
//  dependent predecessors of d.
void DataDepGraph::getDependentPreds(OUT ORTab & preds, OR const* o)
{
    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) { return; }
    xcom::EdgeC * el = v->getInList();
    if (el == nullptr) { return; }

    preds.clean();
    List<xcom::Vertex*> worklst;
    worklst.append_tail(v);
    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * sv = worklst.remove_head();
        xcom::EdgeC * el2 = VERTEX_in_list(sv);
        while (el2 != nullptr) {
            xcom::Vertex * from = el2->getFrom();
            OR * pred_or = getOR(from->id());
            ASSERTN(pred_or != nullptr, ("not on ddg"));
            if (from->id() != v->id() && !preds.find(pred_or)) {
                worklst.append_tail(from);
                preds.append(pred_or);
            }
            el2 = EC_next(el2);
        }
    }
}


//Return all dependent predecessors in lexicographicly order.
//e.g:a->b->c->d, a->d
//  c and a are immediate predecessors of d, whereas a,b,c are
//  dependent predecessors of d.
void DataDepGraph::getPredsByOrderTraverseNode(MOD ORList & preds,
                                               OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != nullptr, ("Node:%d is not on DDG.", o->id()));
    preds.clean();

    ORTab tmptab;
    getDependentPreds(tmptab, o);

    //Sequencing
    ORTabIter c;
    for (OR * pred = tmptab.get_first(c);
         pred != nullptr; pred = tmptab.get_next(c)) {
        OR * tmp;
        for (tmp = preds.get_head(); tmp != nullptr; tmp = preds.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(pred, tmp)) {
                break;
            }
        }
        ASSERTN(!preds.find(pred), ("Repetitive o"));
        if (tmp != nullptr) {
            preds.insert_before(pred, tmp);
        } else {
            preds.append_tail(pred);
        }
    }
}


//Return all successors with ordered lexicographicly.
void DataDepGraph::getSuccsByOrderTraverseNode(MOD ORList & succs,
                                               OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != nullptr, ("Node:%d is not on DDG.", o->id()));
    succs.clean();

    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) { return; }
    xcom::EdgeC * el = v->getOutList();
    if (el == nullptr) { return; }

    List<xcom::Vertex*> worklst;
    ORTab tmptab;
    worklst.append_tail(v);

    DefMiscBitSetMgr sm;
    DefSBitSet visited(sm.getSegMgr());

    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * sv = worklst.remove_head();
        xcom::EdgeC * el2 = sv->getOutList();
        while (el2 != nullptr) {
            xcom::Vertex * to = el2->getTo();
            if (!visited.is_contain(to->id())) {
                worklst.append_tail(to);
                visited.bunion(to->id());
            }
            el2 = EC_next(el2);
        }
        if (sv == v) {
            continue;
        } else {
            OR * svor = getOR(sv->id());
            ASSERTN(svor != nullptr, ("o not in ddg"));
            tmptab.append(svor);
        }
    }

    //Sequencing
    ORTabIter c;
    for (OR * succ = tmptab.get_first(c);
         succ != nullptr; succ = tmptab.get_next(c)) {
        OR * tmp;
        for (tmp = succs.get_head(); tmp != nullptr; tmp = succs.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, tmp)) {
                break;
            }
        }
        ASSERTN(!succs.find(succ), ("Repetitive o"));
        if (tmp != nullptr) {
            succs.insert_before(succ, tmp);
        } else {
            succs.append_tail(succ);
        }
    }
}


//Return all successors.
void DataDepGraph::get_succs(MOD ORList & succs, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    succs.clean();
    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) return;
    xcom::EdgeC * el = v->getOutList();
    if (el == nullptr) return;

    while (el != nullptr) {
        OR * succ = getOR(el->getToId());
        ASSERTN(succ != nullptr, ("Illegal o list"));
        succs.append_tail(succ);
        el = EC_next(el);
    }
}


//Return all predecessors.
void DataDepGraph::get_preds(MOD List<xcom::Vertex*> & preds,
                             IN xcom::Vertex * v)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();
    if (v == nullptr) { return; }
    xcom::EdgeC * el = v->getInList();
    if (el == nullptr) { return; }
    while (el != nullptr) {
        preds.append_tail(el->getFrom());
        el = EC_next(el);
    }
}


//Return all predecessors.
void DataDepGraph::get_preds(MOD ORList & preds, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();

    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) return;
    xcom::EdgeC * el = v->getInList();
    if (el == nullptr) return;

    while (el != nullptr) {
        OR * pred = getOR(el->getFromId());
        ASSERTN(pred != nullptr, ("Illegal o list"));
        preds.append_tail(pred);
        el = EC_next(el);
    }
}


void DataDepGraph::get_neighbors(MOD ORList & nis, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    xcom::Vertex * v = getVertex(o->id());
    if (v == nullptr) return;
    xcom::EdgeC * el = v->getOutList();
    if (el == nullptr) return;
    while (el != nullptr) {
        OR * succ = getOR(el->getToId());
        ASSERTN(succ, ("Illegal o list"));
        nis.append_tail(succ);
        el = EC_next(el);
    }

    el = v->getInList();
    if (el == nullptr) return;
    while (el != nullptr) {
        OR * pred = getOR(el->getFromId());
        ASSERTN(pred != nullptr, ("Illegal o list"));
        nis.append_tail(pred);
        el = EC_next(el);
    }
}


void DataDepGraph::getEstartAndLstart(OUT UINT & estart,
                                      OUT UINT & lstart,
                                      OR const* o) const
{
    estart = m_estart.get(o->id());
    lstart = m_lstart.get(o->id());
}


//Return memory operations which may access same memory location.
//Calculate the length of critical path.
//fin_or: record the OR which at the end of maximum path length.
//        Note the fin_or may not be unique.
//Record the result.
UINT DataDepGraph::computeEstartAndLstart(BBSimulator const& sim,
                                          OUT OR ** fin_or)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    m_estart.clean();
    m_lstart.clean();
    List<xcom::Vertex*> worklst;
    DefMiscBitSetMgr sm;
    //Inspecting whether if there is a cycle in graph.
    DefSBitSet visited(sm.getSegMgr());
    TMap<UINT, UINT> in_degree; //record in/out degree of graph node.
    TMap<UINT, UINT> out_degree; //record in/out degree of graph node.

    INT max_length = -1;
    INT max_estart = -1;
    OR * max_length_or = nullptr;

    //Find root nodes.
    VertexIter c;
    for (xcom::Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        if (v->getInDegree() == 0) {
            worklst.append_tail(v);
            m_estart.setAlways(v->id(), 0);
        }
        //Top down scans the graph.
        in_degree.setAlways(v->id(), v->getInDegree());;
    }

    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * v = worklst.remove_head();
        ASSERTN(!visited.is_contain(v->id()), ("circuit exists in graph"));
        INT estart = 0;
        //Scan pred nodes.
        xcom::EdgeC * el = v->getInList();
        while (el != nullptr) {
            xcom::Vertex * from = el->getFrom();
            ASSERTN(visited.is_contain(from->id()),
                    ("illegal path in the graph"));
            OR * o = getOR(from->id());
            estart = MAX(estart,
                         (INT)m_estart.get(from->id()) +
                         (INT)sim.getMinLatency(o));
            el = EC_next(el);
        }

        ASSERT0(estart >= 0);
        m_estart.setAlways(v->id(), estart);
        //dump_int_vec(&m_estart);
        visited.bunion(v->id());

        //Scan succ nodes.
        el = v->getOutList();
        if (el == nullptr) {
            //Calculate length of critical path and record
            //the 'o' relevant.
            OR * o = getOR(v->id());
            INT l = m_estart.get(v->id()) + sim.getExecCycle(o);
            if (max_length < l) {
                max_estart = m_estart.get(v->id());
                max_length = l;
                max_length_or = o;
            }
        } else {
            while (el != nullptr) {
                xcom::Vertex * to = el->getTo();
                UINT in = in_degree.get(to->id());
                ASSERT0(in > 0);
                if (in == 1) {
                    worklst.append_tail(to);
                }
                in_degree.setAlways(to->id(), --in);
                el = EC_next(el);
            } //end while (el)
        }
    }

    //max_estart is the earest start.
    DUMMYUSE(max_estart);

    //Find anti-root nodes.
    for (xcom::Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        if (v->getOutDegree() == 0) {
            worklst.append_tail(v);
        }
        //Bottom up scans the graph.
        out_degree.setAlways(v->id(), v->getOutDegree());
    }

    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * v = worklst.remove_head();
        //Initial value of maximum.
        OR * vor = getOR(v->id());
        INT lstart = max_length - (INT)sim.getMinLatency(vor);
        ASSERT0(lstart >= 0);
        //Scan succ nodes to calculate 'lstart' of 'v'.
        xcom::EdgeC * el = v->getOutList();
        while (el != nullptr) {
            xcom::Vertex * to = el->getTo();
            lstart = MIN(lstart, ((INT)m_lstart.get(to->id())) -
                         (INT)sim.getMinLatency(vor));
            ASSERT0(lstart >= 0);
            el = EC_next(el);
        }
        m_lstart.setAlways(v->id(), lstart);

        //Scan pred nodes.
        el = v->getInList();
        while (el != nullptr) {
            xcom::Vertex * from = el->getFrom();
            UINT out = out_degree.get(from->id());
            ASSERT0(out > 0);
            if (out == 1) {
                worklst.append_tail(from);
            }
            out_degree.setAlways(from->id(), --out);
            el = EC_next(el);
        }
    }

    if (fin_or != nullptr) {
        *fin_or = max_length_or;
    }

    return max_length;
}


//Algo:
//1. Arrange vertices in order of torological.
//2. Given a set of N edges, associate each edges with an
//   indicator and record in N*N matrix.
//3. Scan each rows of the matrix to remove all immediate edges
//   which the sink node has been marked at else rows.
//e.g: There are dependence edges: 0->1, 0->3, 0->4, 0->9
//     If we can find 1->3 be marked in dependence matrix, then edge 0->3
//     is redundant, and the same goes for the rest of edges.
void DataDepGraph::simplifyGraph()
{
    Vector<Vertex*> vex_vec; //it is in dense storage.
    sortInTopologOrder(vex_vec);
    TMap<UINT, UINT> vid2pos_in_bitset_map;
    //Mapping vertex id to its position in 'vex_vec'.
    VecIdx i;
    for (i = 0; i <= vex_vec.get_last_idx(); i++) {
        vid2pos_in_bitset_map.set(vex_vec.get(i)->id(), i);
    }

    xcom::BitSetMgr * bs_mgr = m_cg->getRegion()->getBitSetMgr();
    //edge_indicator is stored in dense manner.
    Vector<xcom::BitSet*> edge_indicator; //container of bitset.
    EdgeIter c;
    for (xcom::Edge * e = get_first_edge(c);
         e != nullptr; e = get_next_edge(c)) {
        UINT from = VERTEX_id(EDGE_from(e));
        UINT to = VERTEX_id(EDGE_to(e));

        UINT frompos = vid2pos_in_bitset_map.get(from);
        xcom::BitSet * bs = edge_indicator.get(frompos);
        if (bs == nullptr) {
            bs = bs_mgr->create();
            edge_indicator.set(frompos, bs);
        }

        //Each from-vertex associates with a bitset that
        //recorded all to-vertices.
        bs->bunion(vid2pos_in_bitset_map.get(to));
    }

    //Scanning vertices in torological order.
    for (i = 0; i < vex_vec.get_last_idx(); i++) {
        //Get the successor set.
        xcom::BitSet * bs = edge_indicator.get(i);
        if (bs && bs->get_elem_count() >= 2) {
            //Do not remove the first edge.
            for (BSIdx pos_i = bs->get_first();
                 pos_i != BS_UNDEF; pos_i = bs->get_next(pos_i)) {
                INT kid_from_vid = vex_vec.get(pos_i)->id();
                INT kid_from_pos = vid2pos_in_bitset_map.get(kid_from_vid);

                //Get bitset that 'pos_i' associated.
                xcom::BitSet * kid_from_bs = edge_indicator.get(kid_from_pos);
                if (kid_from_bs) {
                    for (BSIdx pos_j = bs->get_next(pos_i);
                         pos_j != BS_UNDEF; pos_j = bs->get_next(pos_j)) {
                        if (kid_from_bs->is_contain(pos_j)) {
                            //The edge 'i->pos_j' is redundant.
                            INT to_vid = vex_vec.get(pos_j)->id();
                            UINT src_vid = vex_vec.get(i)->id();
                            removeEdge(src_vid, to_vid);
                            bs->diff(pos_j);
                        }
                    }
                }
            }
        }
    }
}


//Pass 'root',  when we need to see all
//the elements which belong to the group root at 'root'.
void DataDepGraph::dump(INT flag, INT rootoridx, CHAR const* name) const
{
    DUMMYUSE(rootoridx);
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    #undef INF_DDG_NAME
    #define INF_DDG_NAME "zddgraph.vcg"
    if (name == nullptr) {
        name = INF_DDG_NAME;
    }
    FileObj fo(name, HAVE_FLAG(flag, DDG_DELETE), false);
    FILE * h = fo.getFileHandler();
    ASSERTN(h, ("%s create failed!!!",name));
    fprintf(h, "\n/*\n");
    StrBuf misc(64);
    if (HAVE_FLAG(flag, DDG_DUMP_OP_INFO)) {
        //Print issure interval.
        ORCt * ct;
        for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o != nullptr;
             o = ORBB_orlist(m_bb)->get_next(&ct)) {
            fprintf(h, "\n\n");

            misc.clean();
            o->dump(misc, m_cg);
            fprintf(h, "%s", misc.buf);
            fprintf(h, "\tESTART:%5d, LSTART:%5d",
                    m_estart.get(o->id()),
                    m_lstart.get(o->id()));
        }
    }
    if (HAVE_FLAG(flag, DDG_DUMP_EDGE_INFO)) {
        //Print edge info
        fprintf(h, "\n\nEDGE INFO:\n");
        EdgeIter c;
        for (xcom::Edge * e = get_first_edge(c);
             e != nullptr; e = get_next_edge(c)){
            OR * from = getOR(VERTEX_id(EDGE_from(e)));
            OR * to = getOR(VERTEX_id(EDGE_to(e)));

            fprintf(h, "\tfrom:");
            if (from != nullptr) {
                misc.clean();
                from->dump(misc, m_cg);
            } else {
                //ASSERT0(0); //Miss OR.
                //Still dump the illegal graph.
            }

            fprintf(h, "%s", misc.buf);
            fprintf(h, "\tto  :");
            if (to != nullptr) {
                misc.clean();
                to->dump(misc, m_cg);
            } else {
                //ASSERT0(0); //Miss OR.
                //Still dump the illegal graph.
            }

            fprintf(h, "%s", misc.buf);
            fprintf(h, "\tDEP-Type:");
            DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
            if (HAVE_FLAG(deptype, DEP_HYB)) {
                fprintf(h, "DEP_HYB,");
            }
            if (HAVE_FLAG(deptype, DEP_REG_FLOW)) {
                fprintf(h, "DEP_REG_FLOW,");
            }
            if (HAVE_FLAG(deptype, DEP_REG_ANTI)) {
                fprintf(h, "DEP_REG_ANTI,");
            }
            if (HAVE_FLAG(deptype, DEP_REG_OUT)) {
                fprintf(h, "DEP_REG_OUT,");
            }
            if (HAVE_FLAG(deptype, DEP_REG_READ)) {
                fprintf(h, "DEP_REG_READ,");
            }
            if (HAVE_FLAG(deptype, DEP_MEM_FLOW)) {
                fprintf(h, "DEP_MEM_FLOW,");
            }
            if (HAVE_FLAG(deptype, DEP_MEM_ANTI)) {
                fprintf(h, "DEP_MEM_ANTI,");
            }
            if (HAVE_FLAG(deptype, DEP_MEM_OUT)) {
                fprintf(h, "DEP_MEM_OUT,");
            }
            if (HAVE_FLAG(deptype, DEP_MEM_VOL)) {
                fprintf(h, "DEP_MEM_VOL,");
            }
            if (HAVE_FLAG(deptype, DEP_MEM_READ)) {
                fprintf(h, "DEP_MEM_READ,");
            }
            fprintf(h, "\n\n");
        }
    }

    //Generate graphic script.
    fprintf(h, "\n*/\n");
    fprintf(h, "graph: {"
            "title: \"Graph\"\n"
            "shrink:  15\n"
            "stretch: 27\n"
            "layout_downfactor: 1\n"
            "layout_upfactor: 1\n"
            "layout_nearfactor: 1\n"
            "layout_splinefactor: 70\n"
            "spreadlevel: 1\n"
            "treefactor: 0.500000\n"
            "node_alignment: center\n"
            "orientation: top_to_bottom\n"
            "late_edge_labels: no\n"
            "display_edge_labels: yes\n"
            "dirty_edge_labels: no\n"
            "finetuning: no\n"
            "nearedges: no\n"
            "splines: no\n"
            "ignoresingles: no\n"
            "straight_phase: no\n"
            "priority_phase: no\n"
            "manhatten_edges: no\n"
            "smanhatten_edges: no\n"
            "port_sharing: no\n"
            "crossingphase2: yes\n"
            "crossingoptimization: yes\n"
            "crossingweight: bary\n"
            "arrow_mode: free\n"
            "layoutalgorithm: minbackward\n"
            "node.borderwidth: 3\n"
            "node.color: lightcyan\n"
            "node.textcolor: darkred\n"
            "node.bordercolor: red\n"
            "edge.color: darkgreen\n");

    //Print nodes
    VertexIter c;
    for (xcom::Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        OR * o = getOR(v->id());
        if (o == nullptr) {
            //No o responsed, need to update graph.
            misc.sprint(" shape:rhomb  color:white ");
            fprintf(h, "\nnode: { title:\"%d\" label:\"%d\" %s}",
                    v->id(), v->id(), misc.buf);
            continue;
        }

        //Cluster info
        CHAR const* shape = "";
        if (HAVE_FLAG(flag, DDG_DUMP_CLUSTER_INFO)) {
            CLUST clst = m_cg->computeORCluster(o);
            if (clst == CLUST_UNDEF) {
                shape = "triangle";
                misc.sprint("bordercolor:black ");
            } else {
                shape = "circle";
                misc.sprint("width:%d ", 1 + ((UINT)clst) * 20);
            }
            misc.strcat(" shape:%s ", shape);
        } else {
            shape = "circle";
            if (o->is_store()) {
                misc.sprint("width:40 ");
            } else if (o->is_load()) {
                shape = "box";
            }
            misc.strcat(" shape:%s ", shape);
        }

        //Node color
        bool has_color = false;
        if (!has_color) {
            if (isOnCriticalPath(o)) {
                misc.strcat(" color:green ");
            } else {
                misc.strcat(" color:white ");
            }
        }

        fprintf(h, "\nnode: { title:\"%d\" label:\"%d\" %s}",
                v->id(), v->id(), misc.buf);
    } //end for each of vertex

    //Print edges
    EdgeIter ite;
    for (xcom::Edge * e = get_first_edge(ite);
         e != nullptr; e = get_next_edge(ite)) {
        fprintf(h, "\nedge: { sourcename:\"%d\" targetname:\"%d\"}",
                VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)));
    }
    fprintf(h, "\n}\n");
}


bool DataDepGraph::isGraphNode(OR const* o) const
{
    return getOR(o->id()) != nullptr;
}


void DataDepGraph::build()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (m_bb->getORNum() == 0) { return; }
    buildRegDep();
    //buildMemDep();
    buildMemDepWithOutAnalysis();
    removeRedundantDep();
}


void DataDepGraph::buildSerialDependence()
{
    xcom::C<OR*> * ct = NULL;
    OR * prev = NULL;
    m_bb->getORList()->get_head(&ct);
    if (ct != m_bb->getORList()->end()) {
        //Append vertex if there is only one vertex on graph.
        appendOR(ct->val());
    }
    for (; ct != m_bb->getORList()->end();
         ct = m_bb->getORList()->get_next(ct)) {
        if (prev != NULL) {
            appendEdge(DEP_HYB, prev, ct->val());
        }
        prev = ct->val();
    }
}
