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

#define DDG_DUMP_CLUSTER_INFO    0x1
#define DDG_DUMP_EDGE_INFO       0x2
#define DDG_DUMP_OP_INFO         0x4
#define DDG_DELETE               0x8
//#define MEM_DEP_HAS_CONSTRUCTED

DataDepGraph::DataDepGraph()
{
    m_is_init = false;
    m_is_clonal = false;
    m_ppm = NULL; //Parallel part manager
    m_pool = smpoolCreate(32, MEM_COMM);
    setParam(NO_PHY_REG,
              NO_MEM_READ,
              INC_MEM_FLOW,
              INC_MEM_OUT,
              INC_CONTROL,
              NO_REG_READ,
              INC_REG_ANTI,
              INC_MEM_ANTI,
              INC_SYM_REG);
    set_dense(true);
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
    if (size == 0) return NULL;
    ASSERTN(m_pool != 0, ("need graph pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    if (p == NULL) return NULL;
    ::memset(p, 0, size);
    return p;
}


//Return true if current dep graph parameters identical with inputs.
bool DataDepGraph::is_param_equal(
        bool phy_reg_dep,
        bool memread_dep,
        bool memflow_dep,
        bool memout_dep,
        bool control_dep,
        bool reg_read_read_dep,
        bool reganti_dep,
        bool memanti_dep,
        bool sym_reg_dep) const
{
    if (m_ddg_param.phy_reg_dep == phy_reg_dep &&
        m_ddg_param.mem_read_read_dep == memread_dep &&
        m_ddg_param.mem_flow_dep == memflow_dep &&
        m_ddg_param.mem_out_dep == memout_dep &&
        m_ddg_param.control_dep == control_dep &&
        m_ddg_param.reg_read_read_dep == reg_read_read_dep &&
        m_ddg_param.sym_reg_dep == sym_reg_dep &&
        m_ddg_param.reg_anti_dep == reganti_dep &&
        m_ddg_param.mem_anti_dep == memanti_dep) {
        return true;
    }
    return false;
}


void DataDepGraph::setParam(
        bool phy_reg_dep,
        bool memread_dep,
        bool memflow_dep,
        bool memout_dep,
        bool control_dep,
        bool regread_dep,
        bool reganti_dep,
        bool memanti_dep,
        bool symreg_dep)
{
    m_ddg_param.phy_reg_dep = phy_reg_dep;
    m_ddg_param.mem_read_read_dep = memread_dep;
    m_ddg_param.mem_flow_dep = memflow_dep;
    m_ddg_param.mem_out_dep = memout_dep;
    m_ddg_param.control_dep = control_dep;
    m_ddg_param.reg_read_read_dep = regread_dep;
    m_ddg_param.sym_reg_dep = symreg_dep;
    m_ddg_param.reg_anti_dep = reganti_dep;
    m_ddg_param.mem_anti_dep = memanti_dep;
}


void DataDepGraph::init(ORBB * bb)
{
    if (m_is_init) { return; }

    xcom::Graph::init();
    m_bb = bb;
    m_cg = NULL;
    if (bb != NULL) {
        ASSERT0(ORBB_cg(bb));
        m_cg = ORBB_cg(bb);
    }

    set_unique(true);
    set_direction(true);

    //The default is false in order to reduce number of dep-edge.
    m_unique_mem_loc = false;
    m_mapidx2or_map.init();
    m_params_stack.init();
    m_estart_vec.init();
    m_lstart_vec.init();

    //Dependence xcom::Graph of Compiler constructed has only one
    //global-structure which is exclusive. It leads to ddg can not be built
    //paralleledly.
    //Build dependence graph first.
    //TODO processing the memory alias info.
    //DEPComputeGraph(bb,
    //                m_ddg_param.phy_reg_dep,
    //                m_ddg_param.cycd,
    //                m_ddg_param.mem_read_read_dep,
    //                m_ddg_param.mem_flow_dep,
    //                m_ddg_param.cd,
    //                NULL); //Last parameter is meaning only
    //                       //if the cyclic-dep is built.

    //Compute every OR estart and lstart in terms of dep-graph.

    m_is_init = true;
}


void DataDepGraph::destroy()
{
    if (!m_is_init) { return; }
    m_mapidx2or_map.destroy();
    m_estart_vec.destroy();
    m_lstart_vec.destroy();
    m_params_stack.destroy();
    xcom::Graph::destroy();
    m_bb = NULL;
    m_ppm = NULL;
    m_is_init = false;
    m_is_clonal = false;
}


//Add edge between or in 'orlist' and 'tgt'.
void DataDepGraph::union_edge(List<OR*> & orlist, OR * tgt)
{
    for (OR * o = orlist.get_head(); o != NULL; o = orlist.get_next()) {
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


void DataDepGraph::chainPredAndSucc(OR * o)
{
    xcom::Vertex * v = getVertex(OR_id(o));
    ASSERT0(v);
    xcom::EdgeC * pred_lst = VERTEX_in_list(v);
    xcom::EdgeC * succ_lst = VERTEX_out_list(v);
    while (pred_lst) {
        OR * from = getOR(VERTEX_id(EDGE_from(EC_edge(pred_lst))));
        xcom::EdgeC * tmp_succ_lst = succ_lst;
        while (tmp_succ_lst) {
            OR * to = getOR(VERTEX_id(EDGE_to(EC_edge(tmp_succ_lst))));
            appendEdge(DEP_HYB, from, to);
            tmp_succ_lst = EC_next(tmp_succ_lst);
        }
        pred_lst = EC_next(pred_lst);
    }
}


void DataDepGraph::reschedul()
{
    if (!m_is_init) return;
    ASSERTN(m_bb, ("xcom::Graph still not yet initialize."));
    ASSERTN(!m_is_clonal,
    ("Since the limitation of compiler, "
     "clonal DDG does not allow rescheduling."));

    m_mapidx2or_map.destroy();
    m_mapidx2or_map.init();

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
    //                    NULL);//Last parameter is meaning only if the
    //                          //cyclic-dep is built.

    //Compute every o estart and lstart in terms of dep-graph.

    build();
}


void DataDepGraph::rebuild()
{
    if(!m_is_init) return;
    ASSERTN(m_bb, ("xcom::Graph still not yet initialize."));
    ASSERTN(!m_is_clonal,
          ("Since the limitation of some compiler, "
           "clonal DDG does not allow rescheduling."));

    m_mapidx2or_map.destroy();
    m_mapidx2or_map.init();

    //Clean graph
    erase();
    set_unique(true);
    set_direction(true);

    reschedul(); //for CG DEP rebuild.
    build();
}


//Compute the slack range by lstart - estart.
INT DataDepGraph::get_slack(OR * o)
{
    ASSERTN(m_is_init && o, ("xcom::Graph still not yet initialize."));
    ASSERT0(m_lstart_vec.get(OR_id(o)) >= m_estart_vec.get(OR_id(o)));
    return m_lstart_vec.get(OR_id(o)) - m_estart_vec.get(OR_id(o));
}


//Return true if OR is on the critical path.
bool DataDepGraph::isOnCriticalPath(OR * o)
{
    ASSERTN(m_is_init && o, ("xcom::Graph still not yet initialize."));
    return m_estart_vec.get(OR_id(o)) == m_lstart_vec.get(OR_id(o));
}


UINT DataDepGraph::computeCriticalPathLen(BBSimulator & sim)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    UINT max = 0;
    ORCt * ct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o;
         o = ORBB_orlist(m_bb)->get_next(&ct)) {
        UINT estart = m_estart_vec.get(OR_id(o));
        UINT lstart = m_lstart_vec.get(OR_id(o));
        if (estart == lstart) {
            UINT l = estart + sim.getExecCycle(o);
            if (max < l) {
                max = l;
            }
        }
    }
    return max;
}


ORBB * DataDepGraph::bb() const
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return m_bb;
}


//Return true if there is a data dependence between o1 and o2.
bool DataDepGraph::is_dependent(OR const* o1, OR const* o2)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
#define METHOD1
#ifdef METHOD1
    if(getEdge(OR_id(o1), OR_id(o2))) {
#else //Method2
    if(is_reachable(OR_id(o1), OR_id(o2))) {
#endif
        return true;
    }
    return false;
}


//Return true if actually add new edge.
bool DataDepGraph::appendEdge(ULONG deptype, OR const* from, OR const* to)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (!m_ddg_param.reg_read_read_dep && deptype == DEP_REG_READ) {
        //Omit RAR dep.
        return false;
    }
    xcom::Edge * e = addEdge(OR_id(from), OR_id(to));
    if (EDGE_info(e) == NULL) {
        EDGE_info(e) = (DDGEdgeInfo*)xmalloc(sizeof(DDGEdgeInfo));
    }
    DDGEI_deptype(EDGE_info(e)) |= deptype;
    return true;
}


void DataDepGraph::removeEdge(OR * from, OR * to)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    removeEdge(OR_id(from), OR_id(to));
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
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o != NULL;
         o = ORBB_orlist(m_bb)->get_next(&ct)) {
        ORList succs;
        getSuccsByOrderTraverseNode(succs, o);
        for (OR *succ = succs.get_head();
             succ != NULL; succ = succs.get_next()) {
            appendEdge(DEP_HYB, o, succ);
        }
    }
}


//Return true if o1 is independent with o2.
bool DataDepGraph::is_independent(OR * o1, OR * o2)
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

    xcom::BitSet * or_idxset1 = NULL;
    xcom::BitSet * or_idxset2 = NULL;
    if (or_idxset1 == NULL || or_idxset2 == NULL) {
        return false;
    }

    if (or_idxset1->is_contain(OR_id(o1)) &&
        or_idxset2->is_contain(OR_id(o2))) {
        return true;
    }
    return false;
}


bool DataDepGraph::must_def_sp_reg(OR *)
{
    ASSERTN(0, ("TODO"));
    return false;
}


void DataDepGraph::removeRedundantDep()
{
    xcom::Edge * next;
    INT c;
    for (xcom::Edge * e = get_first_edge(c); e != NULL; e = next) {
        next = get_next_edge(c);
        if (EDGE_info(e) == NULL) {
            continue;
        }
        OR * from = getOR(VERTEX_id(EDGE_from(e)));
        OR * to = getOR(VERTEX_id(EDGE_to(e)));
        ULONG deptype = DDGEI_deptype(EDGE_info(e));
        ASSERT0(from && to);

        //BB_exit_sp_adj_or(m_bb)
        if ((OR_is_mem(from) &&
             (OR_code(to) == OR_spadjust || OR_is_bus(to))) ||
            (OR_is_mem(to) &&
             (OR_code(from) == OR_spadjust || OR_is_bus(from)))) {
            if (HAVE_FLAG(deptype, DEP_HYB) &&
                !ONLY_HAVE_FLAG(deptype, DEP_HYB)) {
                //CASE:
                //  Remove redundant DEP-HYB.
                //  [1] GTN246(r7) :- lw_b TN73(sp3) (gra_spill_temp_113+0)
                //  [2] TN57(sp2), TN73(sp3), :- bus_m1_to_m2_b1 GTN49(sp)
                //  CG_DEP_MISC edge exist for keeping memory
                //  implied depdendence, but it is
                //  redundant dependence if there are another dep-edges.
                REMOVE_FLAG(deptype, DEP_HYB);
                DDGEI_deptype(EDGE_info(e)) = deptype;
            }
        }

        if (OR_is_mem(from) &&
            ONLY_HAVE_FLAG(deptype, DEP_HYB) &&
            !OR_is_side_effect(to) &&
            !OR_is_volatile(to) &&
            !OR_is_side_effect(from) &&
            !OR_is_volatile(from) &&
            (!OR_is_fake(to) ||
             OR_is_bus(to) ||
             OR_code(to) == OR_spadjust) &&
            !OR_is_mem(to)) {

            if (must_def_sp_reg(to)) {
                //With the handling of CG dependence analysis module, the HYB
                //edge that in order to keep relation of the memory operation
                //and the modifying SP/FP operation is redundant.
                SR * base_tn = from->get_mem_base();
                if (!m_cg->mustDef(to, base_tn)) {
                    //Not REG-FLOW dep
                    removeEdge(from, to);
                    continue;
                }
            }
        }

        if (OR_is_mem(to) &&
            ONLY_HAVE_FLAG(deptype, DEP_HYB) &&
            !OR_is_side_effect(to) &&
            !OR_is_volatile(to) &&
            !OR_is_side_effect(from) &&
            !OR_is_volatile(from) &&
            (!OR_is_fake(from) ||
             OR_is_bus(from) ||
             OR_code(from) == OR_spadjust) &&
            !OR_is_mem(from)) {

            if (must_def_sp_reg(from)) {
                //With the handling of CG dependence analysis module, the HYB
                //edge that in order to keep relation of the memory operation
                //and the modifying SP/FP operation is redundant.
                SR * base_tn = to->get_mem_base();
                if (!m_cg->mustDef(from, base_tn)) {
                    //Not REG-FLOW dep
                    removeEdge(from, to);
                    continue;
                }
            }
        }
    }
}


//Return true if 'sr' need to process.
bool DataDepGraph::handleDedicatedSR(SR const*, OR const*, bool is_result)
{
    DUMMYUSE(is_result);
    return true;
}


void DataDepGraph::handle_results(
        OR const* o,
        OUT Reg2ORList & map_reg2defors,
        OUT Reg2ORList & map_reg2useors,
        OUT SR2ORList & map_sr2defors,
        OUT SR2ORList & map_sr2useors)
{
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (sr == m_cg->genTruePred() ||
            !SR_is_reg(sr)) {
            continue;
        }

        if (m_cg->isDedicatedSR(sr) &&
            !handleDedicatedSR(sr, o, true)) {
            continue;
        }

        if (m_ddg_param.phy_reg_dep && SR_phy_regid(sr) != REG_UNDEF) {
            //Dep of physical register.
            REG reg = SR_phy_regid(sr);
            List<OR*> * orlst = map_reg2defors.get(reg);
            if (orlst != NULL) {
                for (OR * defor = orlst->get_head();
                     defor != NULL; defor = orlst->get_next()) {
                    if (defor == o) {
                        continue;
                    }
                    appendEdge(DEP_REG_OUT, defor, o);
                }
            }

            orlst = map_reg2useors.get(reg);
            if (orlst != NULL) {
                for (OR * useor = orlst->get_head();
                     useor != NULL; useor = orlst->get_next()) {
                    if (useor == o) {
                        continue;
                    }
                    appendEdge(DEP_REG_ANTI, useor, o);
                }
            }

            map_reg2defors.set(reg, const_cast<OR*>(o));

            //Kill all USE ors.
            map_reg2useors.clean(reg);
        } //end if (m_ddg_param.phy_reg_dep)

        if (m_ddg_param.sym_reg_dep) {
            List<OR*> * orlst = map_sr2defors.get(sr);
            if (orlst != NULL) {
                for (OR * defor = orlst->get_head();
                     defor != NULL; defor = orlst->get_next()) {
                    if (defor == o) {
                        continue;
                    }
                    appendEdge(DEP_REG_OUT, defor, o);
                }
            }

            orlst = map_sr2useors.get(sr);
            if (orlst != NULL) {
                for (OR * useor = orlst->get_head();
                     useor != NULL; useor = orlst->get_next()) {
                    if (useor == o) {
                        continue;
                    }
                    appendEdge(DEP_REG_ANTI, useor, o);
                }
            }

            map_sr2defors.set(sr, const_cast<OR*>(o));

            //Kill all USE ors.
            map_sr2useors.clean(sr);
        } //end if
    } //end for each of result
}


void DataDepGraph::handle_opnds(
        IN OR const* o,
        OUT Reg2ORList & map_reg2defors,
        OUT Reg2ORList & map_reg2useors,
        OUT SR2ORList & map_sr2defors,
        OUT SR2ORList & map_sr2useors)
{
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (sr == m_cg->genTruePred() || !SR_is_reg(sr)) {
            continue;
        }
        if (m_cg->isDedicatedSR(sr) &&
            !handleDedicatedSR(sr, o, false)) {
            continue;
        }

        if (m_ddg_param.phy_reg_dep && SR_phy_regid(sr) != REG_UNDEF) {
            //Dep of physical register.
            REG reg = SR_phy_regid(sr);
            List<OR*> * orlst = map_reg2defors.get(reg);
            if (orlst != NULL) {
                for (OR * defor = orlst->get_head();
                     defor != NULL; defor = orlst->get_next()) {
                    appendEdge(DEP_REG_FLOW, defor, o);
                }
            }
            if (m_ddg_param.reg_read_read_dep) {
                //Dep of read-read of register.
                List<OR*> * orlst2 = map_reg2useors.get(reg);
                if (orlst2 != NULL) {
                    for (OR * useor = orlst2->get_head();
                         useor != NULL; useor = orlst2->get_next()) {
                        if (useor == o) {
                            continue;
                        }
                        appendEdge(DEP_REG_READ, useor, o);
                    }
                }
            }
            map_reg2useors.append(reg, const_cast<OR*>(o));
        }

        if (m_ddg_param.sym_reg_dep) {
            List<OR*> * orlst = map_sr2defors.get(sr);

            if (orlst != NULL) {
                for (OR * defor = orlst->get_head();
                     defor != NULL; defor = orlst->get_next()) {
                    appendEdge(DEP_REG_FLOW, defor, o);
                }
            }
            if (m_ddg_param.reg_read_read_dep) {
                //Dep of read-read of register.
                List<OR*> * orlst2 = map_sr2useors.get(sr);
                if (orlst2 != NULL) {
                    for (OR * useor = orlst2->get_head();
                         useor != NULL; useor = orlst2->get_next()) {
                        if (useor == o) {
                            continue;
                        }
                        appendEdge(DEP_REG_READ, useor, o);
                    }
                }
            }
            map_sr2useors.append(sr, const_cast<OR*>(o));
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
    int i = 0;
    OR * last = ORBB_orlist(m_bb)->get_tail();
    m_mapidx2or_map.set(OR_id(last), last);
    for (((List<OR*>*)ORBB_orlist(m_bb))->get_head(&ct);
         ct != ORBB_orlist(m_bb)->end();
         ct = ((List<OR*>*)ORBB_orlist(m_bb))->get_next(ct), i++) {
        OR * o = ct->val();
        m_mapidx2or_map.set(OR_id(o), o);
        addVertex(OR_id(o));
        handle_opnds(o, map_reg2defors, map_reg2useors,
            map_sr2defors, map_sr2useors);
        handle_results(o, map_reg2defors, map_reg2useors,
            map_sr2defors, map_sr2useors);
    }
}


//Return memory operations which may access same memory location with node.
void DataDepGraph::getORListWhichAccessSameMem(
        OUT ORList & mem_ors,
        OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    mem_ors.clean();
    ASSERTN(o, ("Node:%d is not on DDG.", OR_id(o)));
    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) { return; }

    List<xcom::Vertex*> worklst;
    ORList tmplst;
    Vector<bool> visited(this->getVertexNum());

    worklst.append_tail(v);
    visited.set(VERTEX_id(v), true);
    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * head = worklst.remove_head();
        //Add succ nodes.
        xcom::EdgeC * el = VERTEX_out_list(head);
        while (el != NULL) {
            xcom::Edge * e = EC_edge(el);
            el = EC_next(el);
            if (EDGE_info(e) == NULL) {
                continue;
            }
            DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
            if (HAVE_FLAG(deptype, DEP_MEM_OUT) ||
                HAVE_FLAG(deptype, DEP_MEM_READ) ||
                HAVE_FLAG(deptype, DEP_MEM_FLOW) ||
                HAVE_FLAG(deptype, DEP_MEM_ANTI) ||
                HAVE_FLAG(deptype, DEP_MEM_VOL)) {
                xcom::Vertex * to = EDGE_to(e);
                if (!visited.get(VERTEX_id(to))) {
                    worklst.append_tail(to);
                    visited.set(VERTEX_id(to), true);
                }
            }
        }//end while (el)

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
            while (el != NULL) {
                xcom::Edge * e = EC_edge(el);
                el = EC_next(el);
                if (EDGE_info(e) == NULL) {
                    continue;
                }

                DEP_TYPE deptype = (DEP_TYPE)DDGEI_deptype(EDGE_info(e));
                if (HAVE_FLAG(deptype, DEP_MEM_OUT) ||
                    HAVE_FLAG(deptype, DEP_MEM_READ) ||
                    HAVE_FLAG(deptype, DEP_MEM_FLOW) ||
                    HAVE_FLAG(deptype, DEP_MEM_ANTI) ||
                    HAVE_FLAG(deptype, DEP_MEM_VOL)) {
                    xcom::Vertex * from = EDGE_from(e);
                    if (!visited.get(VERTEX_id(from))) {
                        worklst.append_tail(from);
                        visited.set(VERTEX_id(from), true);
                    }
                }
            }
        }

        OR * head_or = getOR(VERTEX_id(head));
        ASSERT0(OR_is_mem(head_or));
        tmplst.append_tail(head_or);
    }

    //Sequencing
    for (OR * succ = tmplst.get_head();
         succ != NULL; succ = tmplst.get_next()) {
        OR * tmp = NULL;
        for (tmp = mem_ors.get_head(); tmp != NULL; tmp = mem_ors.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, tmp)) {
                break;
            }
        }

        ASSERTN(false == mem_ors.find(succ), ("Repetitive o"));

        if (tmp != NULL) {
            mem_ors.insert_before(succ, tmp);
        } else {
            mem_ors.append_tail(succ);
        }
    }
}


//OR is LOAD.
void DataDepGraph::handle_load(
        IN OR * o,
        ULONG mem_loc_idx,
        OUT UINT2ORList & map_memloc2defors,
        OUT UINT2ORList & map_memloc2useors)
{
    ASSERT0(OR_is_load(o));
    //Processing DEF of mem
    List<OR*> * orlst = map_memloc2defors.get(mem_loc_idx);
    if (orlst != NULL) {
        for (OR * defor = orlst->get_head();
             defor != NULL; defor = orlst->get_next()) {
            ASSERT0(OR_is_store(defor));
            appendEdge(DEP_MEM_FLOW, defor, o);
        }
    }

    if (m_ddg_param.mem_read_read_dep) {
        //Processing USE of mem
        List<OR*> * orlst2 = map_memloc2useors.get(mem_loc_idx);
        if (orlst2) {
            for (OR * useor = orlst2->get_head();
                 useor != NULL; useor = orlst2->get_next()) {
                ASSERT0(OR_is_load(useor));
                appendEdge(DEP_MEM_READ, useor, o);
            }
        }
    }

    map_memloc2useors.set(mem_loc_idx, o);
}


//OR is STORE.
void DataDepGraph::handle_store(
        IN OR * o,
        ULONG mem_loc_idx,
        OUT UINT2ORList & map_memloc2defors,
        OUT UINT2ORList & map_memloc2useors)
{
    ASSERT0(OR_is_store(o));

    //Processing DEF of mem
    List<OR*> * orlst = map_memloc2defors.get(mem_loc_idx);
    if (orlst != NULL) {
        for (OR * defor = orlst->get_head();
             defor != NULL; defor = orlst->get_next()) {
            ASSERT0(OR_is_store(defor));
            appendEdge(DEP_MEM_OUT, defor, o);
        }
    }

    if (m_ddg_param.mem_read_read_dep) {
        //Processing USE of mem
        List<OR*> * orlst2 = map_memloc2useors.get(mem_loc_idx);
        if (orlst2 != NULL) {
            for (OR * useor = orlst2->get_head();
                 useor != NULL; useor = orlst2->get_next()) {
                ASSERT0(OR_is_load(useor));
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
    if (ORBB_ornum(m_bb) == 0) return;

    UINT2ORList map_memloc2defors;
    UINT2ORList map_memloc2useors;
    ULONG mem_loc_idx = 1;
    Vector<bool> visited;
    ORCt * ct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct);
         o != NULL; o = ORBB_orlist(m_bb)->get_next(&ct)) {
        if (!OR_is_mem(o) || visited.get(OR_id(o))) { continue; }

        ORList mem_ors;
        //mem-ors have been sorted by order.
        getORListWhichAccessSameMem(mem_ors, o);
        for (OR * mem = mem_ors.get_head(); mem != NULL;
             mem = mem_ors.get_next()) {
            ASSERT0(!m_unique_mem_loc || //mem loc is unique
                    !visited.get(OR_id(mem)));
            ASSERT0(OR_is_mem(mem));

            if (OR_is_load(mem)) {
                handle_load(mem, mem_loc_idx,
                    map_memloc2defors, map_memloc2useors);
            }

            if (OR_is_store(mem)) {
                handle_store(mem, mem_loc_idx,
                    map_memloc2defors, map_memloc2useors);
            }

            visited.set(OR_id(mem), true);
        }
        mem_loc_idx++;
    }
}
#else
void DataDepGraph::buildMemDep()
{
    if (!m_ddg_param.mem_flow_dep &&
        !m_ddg_param.mem_out_dep &&
        !m_ddg_param.mem_anti_dep &&
        !m_ddg_param.mem_read_read_dep) {
        return;
    }

    ORCt * ct;
    List<OR*> mem_ors;

    //Pick out memory OR.
    for (ORBB_orlist(m_bb)->get_head(&ct);
         ct != ORBB_orlist(m_bb)->end();
         ct = ((List<OR*>*)ORBB_orlist(m_bb))->get_next(ct)) {
        OR * o = ct->val();
        if (OR_is_mem(o)) {
            mem_ors.append_tail(o);
        }
    }

    for (mem_ors.get_head(&ct);
         ct != mem_ors.end(); ct = mem_ors.get_next(ct)) {
        ORCt * to_ct = ct;
        OR * from = ct->val();
        VAR const* from_loc = m_cg->computeSpillVar(from);
        for (OR * to = mem_ors.get_next(&to_ct);
             to != NULL; to = mem_ors.get_next(&to_ct)) {
            if (m_ddg_param.mem_flow_dep) { //flow-dependence
                buildMemFlowDep(from, to);
            }

            if (m_ddg_param.mem_out_dep) { //out-dependence
                buildMemOutDep(from, to, from_loc);
            }

            if (m_ddg_param.mem_anti_dep) { //anti
                //Do not need MEM ANTI dependence.
                continue;
            }

            if (m_ddg_param.mem_read_read_dep) { //read-dependence
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


void DataDepGraph::buildMemOutDep(OR * from, OR * to, VAR const* from_loc)
{
    VAR const* to_loc = m_cg->computeSpillVar(to);

    if (from_loc != NULL && to_loc != NULL) {
        if (from_loc != to_loc) {
            //Not same spill-loc.
            return;
        }
    } else if (from_loc != NULL || to_loc != NULL) {
        //Normal memory location in load/store does not
        //alias with spill-memory-location.
        return;
    }

    //Build dependence for conservation.
    appendEdge(DEP_MEM_OUT, from, to);
}


void DataDepGraph::buildMemFlowDep(OR * from, OR * to)
{
    appendEdge(DEP_MEM_FLOW, from, to);
}


void DataDepGraph::buildMemInDep(OR *, OR *, VAR const*)
{
    ASSERTN(0, ("TODO"));
}


void DataDepGraph::buildMemVolatileDep(OR * from, OR * to)
{
    appendEdge(DEP_MEM_VOL, from, to);
}


void DataDepGraph::build()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (ORBB_ornum(m_bb) == 0) return;
    preBuild();
    buildRegDep();
    buildMemDep();
    removeRedundantDep();
}


void * DataDepGraph::cloneEdgeInfo(xcom::Edge * e)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if(!EDGE_info(e)) return NULL;
    DDGEdgeInfo * dei =(DDGEdgeInfo*)xmalloc(sizeof(DDGEdgeInfo));
    ::memcpy(dei, EDGE_info(e), sizeof(DDGEdgeInfo));
    return (void*)dei;
}


void * DataDepGraph::cloneVertexInfo(xcom::Vertex * v)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if(!VERTEX_info(v)) return NULL;
    DDGNodeInfo *vi = (DDGNodeInfo*)xmalloc(sizeof(DDGNodeInfo));
    ::memcpy(vi, VERTEX_info(v), sizeof(DDGNodeInfo));
    return (void*)vi;
}


void DataDepGraph::clone(DataDepGraph & ddg)
{
    ASSERTN(m_is_init, ("xcom::Graph already initialized."));
    m_bb = ddg.bb();
    m_ppm = ddg.m_ppm;
    m_ddg_param = ddg.m_ddg_param;
    m_mapidx2or_map.clean();
    for(INT i = 0; i <= ddg.m_mapidx2or_map.get_last_idx(); i++) {
        m_mapidx2or_map.set(i, ddg.m_mapidx2or_map.get(i));
    }
    m_estart_vec.copy(ddg.m_estart_vec);
    m_lstart_vec.copy(ddg.m_lstart_vec);
    xcom::Graph::clone((xcom::Graph&)ddg, true, true);
    m_is_clonal = true;
}


//Append node on graph.
void DataDepGraph::appendOR(OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (o == NULL) return;
    addVertex(OR_id(o));
    m_mapidx2or_map.set(OR_id(o), o);
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
    if (o == NULL) return;
    if (removeVertex(OR_id(o)) == NULL) {
        return;
    }
    m_mapidx2or_map.set(OR_id(o), NULL);
}


//Get first node on graph.
OR * DataDepGraph::getFirstOR(INT & cur)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return getOR(VERTEX_id(m_vertices.get_first(cur)));
}


//Get next node on graph.
OR * DataDepGraph::getNextOR(INT & cur)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return getOR(VERTEX_id(m_vertices.get_next(cur)));
}


//Return all predecessors with ordered lexicographicly.
void DataDepGraph::getPredsByOrder(IN OUT ORList & preds, IN OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();

    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_in_list(v);
    if (el == NULL) return;

    while (el != NULL) {
        OR * pred = getOR(VERTEX_id(EDGE_from(EC_edge(el))));
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
void DataDepGraph::getSuccsByOrder(IN OUT ORList & succs, IN OR * o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != NULL, ("Node:%d is not on DDG.", OR_id(o)));
    succs.clean();

    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) { return; }

    for (xcom::EdgeC * el = VERTEX_out_list(v); el != NULL; el = EC_next(el)) {
        OR * succ = getOR(VERTEX_id(EDGE_to(EC_edge(el))));
        ASSERTN(succ, ("DDG is invalid."));
        OR * marker;
        for (marker = succs.get_head(); marker; marker = succs.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, marker)) {
                break;
            }
        }

        if (marker != NULL) {
            succs.insert_before(succ, marker);
        } else {
            succs.append_tail(succ);
        }
    }
}


//Return all predecessors in lexicographicly order.
void DataDepGraph::getPredsByOrderTraverseNode(
        IN OUT ORList & preds,
        OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != NULL, ("Node:%d is not on DDG.", OR_id(o)));
    preds.clean();

    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_in_list(v);
    if (el == NULL) return;

    List<xcom::Vertex*> worklst;
    OR_HASH tmpbuf;
    worklst.append_tail(v);
    Vector<bool> visited(this->getVertexNum());
    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * sv = worklst.remove_head();
        xcom::EdgeC * el2 = VERTEX_in_list(sv);
        while (el2 != NULL) {
            xcom::Vertex * from = EDGE_from(EC_edge(el2));
            if (!visited.get(VERTEX_id(from))) {
                worklst.append_tail(from);
                visited.set(VERTEX_id(from), true);
            }
            el2 = EC_next(el2);
        }
        if (sv == v) {
            continue;
        } else {
            OR * svor = getOR(VERTEX_id(sv));
            ASSERTN(svor != NULL, ("o not in ddg"));
            tmpbuf.append(svor);
        }
    }

    //Sequencing
    INT c;
    for (OR * pred = tmpbuf.get_first(c);
         pred != NULL; pred = tmpbuf.get_next(c)) {
        OR * tmp;
        for (tmp = preds.get_head(); tmp != NULL; tmp = preds.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(pred, tmp)) {
                break;
            }
        }
        ASSERTN(!preds.find(pred), ("Repetitive o"));
        if (tmp != NULL) {
            preds.insert_before(pred, tmp);
        } else {
            preds.append_tail(pred);
        }
    }
}


//Return all successors with ordered lexicographicly.
void DataDepGraph::getSuccsByOrderTraverseNode(
        IN OUT ORList & succs,
        OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(o != NULL, ("Node:%d is not on DDG.", OR_id(o)));
    succs.clean();

    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_out_list(v);
    if (el == NULL) return;

    List<xcom::Vertex*> worklst;
    OR_HASH tmpbuf;
    worklst.append_tail(v);
    Vector<bool> visited;
    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * sv = worklst.remove_head();
        xcom::EdgeC * el2 = VERTEX_out_list(sv);
        while (el2 != NULL) {
            xcom::Vertex * to = EDGE_to(EC_edge(el2));
            if (!visited.get(VERTEX_id(to))) {
                worklst.append_tail(to);
                visited.set(VERTEX_id(to), true);
            }
            el2 = EC_next(el2);
        }
        if (sv == v) {
            continue;
        } else {
            OR * svor = getOR(VERTEX_id(sv));
            ASSERTN(svor != NULL, ("o not in ddg"));
            tmpbuf.append(svor);
        }
    }

    //Sequencing
    INT c;
    for (OR * succ = tmpbuf.get_first(c);
         succ != NULL; succ = tmpbuf.get_next(c)) {
        OR * tmp;
        for (tmp = succs.get_head(); tmp != NULL; tmp = succs.get_next()) {
            if (ORBB_orlist(m_bb)->is_or_precedes(succ, tmp)) {
                break;
            }
        }
        ASSERTN(!succs.find(succ), ("Repetitive o"));
        if (tmp != NULL) {
            succs.insert_before(succ, tmp);
        } else {
            succs.append_tail(succ);
        }
    }
}


//Return all successors.
void DataDepGraph::get_succs(IN OUT ORList & succs, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    succs.clean();
    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_out_list(v);
    if (el == NULL) return;

    while (el != NULL) {
        OR * succ = getOR(VERTEX_id(EDGE_to(EC_edge(el))));
        ASSERTN(succ != NULL, ("Illegal o list"));
        succs.append_tail(succ);
        el = EC_next(el);
    }
}


//Return all predecessors.
void DataDepGraph::get_preds(IN OUT List<xcom::Vertex*> & preds, IN xcom::Vertex * v)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_in_list(v);
    if (el == NULL) return;
    while (el != NULL) {
        preds.append_tail(EDGE_from(EC_edge(el)));
        el = EC_next(el);
    }
}


//Return all predecessors.
void DataDepGraph::get_preds(IN OUT ORList & preds, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    preds.clean();

    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_in_list(v);
    if (el == NULL) return;

    while (el != NULL) {
        OR * pred = getOR(VERTEX_id(EDGE_from(EC_edge(el))));
        ASSERTN(pred != NULL, ("Illegal o list"));
        preds.append_tail(pred);
        el = EC_next(el);
    }
}


void DataDepGraph::get_neighbors(IN OUT ORList & nis, OR const* o)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    xcom::Vertex * v = getVertex(OR_id(o));
    if (v == NULL) return;
    xcom::EdgeC * el = VERTEX_out_list(v);
    if (el == NULL) return;
    while (el != NULL) {
        OR * succ = getOR(VERTEX_id(EDGE_to(EC_edge(el))));
        ASSERTN(succ, ("Illegal o list"));
        nis.append_tail(succ);
        el = EC_next(el);
    }

    el = VERTEX_in_list(v);
    if (el == NULL) return;
    while (el != NULL) {
        OR * pred = getOR(VERTEX_id(EDGE_from(EC_edge(el))));
        ASSERTN(pred != NULL, ("Illegal o list"));
        nis.append_tail(pred);
        el = EC_next(el);
    }
}


void DataDepGraph::getEstartAndLstart(
        OUT UINT & estart,
        OUT UINT & lstart,
        OR const* o) const
{
    estart = m_estart_vec.get(OR_id(o));
    lstart = m_lstart_vec.get(OR_id(o));
}


//Return memory operations which may access same memory location.
//Calculate the length of critical path.
//'fin_op': record the o with the maximum path length.
UINT DataDepGraph::computeEstartAndLstart(IN BBSimulator & sim, OUT OR ** fin_or)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    m_estart_vec.clean();
    m_lstart_vec.clean();
    List<xcom::Vertex*> worklst;
    ORList tmplst;
    Vector<bool> visited; //inspecting whether if there is a cycle in graph.
    Vector<UINT> in_degree; //record in/out degree of graph node.
    Vector<UINT> out_degree; //record in/out degree of graph node.

    INT max_length = -1;
    INT max_estart = -1;
    OR * max_length_or = NULL;

    //Find root nodes.
    INT c;
    for (xcom::Vertex * v = get_first_vertex(c); v != NULL; v = get_next_vertex(c)) {
        if (getInDegree(v) == 0) {
            worklst.append_tail(v);
            m_estart_vec.set(VERTEX_id(v), 0);
        }
        //Top down scans the graph.
        in_degree.set(VERTEX_id(v), getInDegree(v));;
    }

    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * v = worklst.remove_head();
        ASSERTN(!visited.get(VERTEX_id(v)), ("circuit exists in graph"));
        INT estart = 0;
        //Scan pred nodes.
        xcom::EdgeC * el = VERTEX_in_list(v);
        while (el != NULL) {
            xcom::Vertex * from = EDGE_from(EC_edge(el));
            ASSERTN(visited.get(VERTEX_id(from)), ("illegal path in the graph"));
            OR * o = getOR(VERTEX_id(from));
            estart = MAX(estart,
                        (INT)m_estart_vec.get(VERTEX_id(from)) +
                        (INT)sim.getMinLatency(o));
            el = EC_next(el);
        }

        ASSERT0(estart >= 0);
        m_estart_vec.set(VERTEX_id(v), estart);
        //dump_int_vec(&m_estart_vec);
        visited.set(VERTEX_id(v), true);

        //Scan succ nodes.
        el = VERTEX_out_list(v);
        if (el == NULL) {
            //Calculate length of critical path and record
            //the 'o' relevant.
            OR * o = getOR(VERTEX_id(v));
            INT l = m_estart_vec.get(VERTEX_id(v)) + sim.getExecCycle(o);
            if (max_length < l) {
                max_estart = m_estart_vec.get(VERTEX_id(v));
                max_length = l;
                max_length_or = o;
            }
        } else {
            while (el != NULL) {
                xcom::Vertex * to = EDGE_to(EC_edge(el));
                UINT in = in_degree.get(VERTEX_id(to));
                ASSERT0(in > 0);
                if (in == 1) {
                    worklst.append_tail(to);
                }
                in_degree.set(VERTEX_id(to), --in);
                el = EC_next(el);
            } //end while (el)
        }
    }

    //max_estart is the earest start.
    DUMMYUSE(max_estart);

    //Find anti-root nodes.
    for (xcom::Vertex * v = get_first_vertex(c); v != NULL; v = get_next_vertex(c)) {
        if (getOutDegree(v) == 0) {
            worklst.append_tail(v);
        }
        //Bottom up scans the graph.
        out_degree.set(VERTEX_id(v), getOutDegree(v));
    }

    while (worklst.get_elem_count() > 0) {
        xcom::Vertex * v = worklst.remove_head();
        //Initial value of maximum.
        OR * vor = getOR(VERTEX_id(v));
        INT lstart = max_length - (INT)sim.getMinLatency(vor);
        ASSERT0(lstart >= 0);
        //Scan succ nodes to calculate 'lstart' of 'v'.
        xcom::EdgeC * el = VERTEX_out_list(v);
        while (el != NULL) {
            xcom::Vertex * to = EDGE_to(EC_edge(el));
            lstart = MIN(lstart, ((INT)m_lstart_vec.get(VERTEX_id(to))) -
                (INT)sim.getMinLatency(vor));
            ASSERT0(lstart >= 0);
            el = EC_next(el);
        }
        m_lstart_vec.set(VERTEX_id(v), lstart);

        //Scan pred nodes.
        el = VERTEX_in_list(v);
        while (el != NULL) {
            xcom::Vertex * from = EDGE_from(EC_edge(el));
            UINT out = out_degree.get(VERTEX_id(from));
            ASSERT0(out > 0);
            if (out == 1) {
                worklst.append_tail(from);
            }
            out_degree.set(VERTEX_id(from), --out);
            el = EC_next(el);
        }
    }

    if (fin_or != NULL) {
        *fin_or = max_length_or;
    }

    return max_length;
}


//Sort graph nodes in order of topological.
//'vex_vec': record nodes with topological sort.
void DataDepGraph::sortInTopological(OUT Vector<UINT> & vex_vec)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    if (getVertexNum() == 0) {
        return;
    }
    DataDepGraph tmpddg;
    tmpddg.init(NULL);
    tmpddg.clone(*this);
    List<xcom::Vertex*> norredvex;
    UINT pos = 0;
    vex_vec.clean();
    vex_vec.grow(getVertexNum());
    INT c;
    while (tmpddg.get_first_vertex(c) != NULL) {
        norredvex.clean();
        xcom::Vertex * v;
        for (v = tmpddg.get_first_vertex(c);
             v != NULL; v = tmpddg.get_next_vertex(c)) {
            if (VERTEX_in_list(v) == NULL) {
                norredvex.append_tail(v);
            }
        }
        for (v = norredvex.get_head(); v != NULL; v = norredvex.get_next()) {
            vex_vec.set(pos, VERTEX_id(v));
            pos++;
            tmpddg.removeVertex(v);
        }
    }
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
    Vector<UINT> vex_vec;
    sortInTopological(vex_vec);
    Vector<UINT> vid2pos_in_bitset_map;
    //Mapping vertex id to its position in 'vex_vec'.
    INT i;
    for (i = 0; i <= vex_vec.get_last_idx(); i++) {
        vid2pos_in_bitset_map.set(vex_vec.get(i), i);
    }

    xcom::BitSetMgr * bs_mgr = m_cg->getRegion()->getBitSetMgr();
    Vector<xcom::BitSet*> edge_indicator; //container of bitset.
    INT c;
    for (xcom::Edge * e = m_edges.get_first(c);
         e != NULL; e = m_edges.get_next(c)) {
        UINT from = VERTEX_id(EDGE_from(e));
        UINT to = VERTEX_id(EDGE_to(e));

        UINT frompos = vid2pos_in_bitset_map.get(from);
        xcom::BitSet * bs = edge_indicator.get(frompos);
        if (bs == NULL) {
            bs = bs_mgr->create();
            edge_indicator.set(frompos, bs);
        }

        //Each from-vertex associates with a bitset that
        //recorded all to-vertices.
        bs->bunion(vid2pos_in_bitset_map.get(to));
    }

    //Scanning vertices in torological order.
    for (i = 0; i < vex_vec.get_last_idx(); i++) {
        //Get the successor vector.
        xcom::BitSet * bs = edge_indicator.get(i);
        if (bs && bs->get_elem_count() >= 2) {
            //Do not remove the first edge.
            for (INT pos_i = bs->get_first();
                 pos_i >= 0; pos_i = bs->get_next(pos_i)) {
                INT kid_from_vid = vex_vec.get(pos_i);
                INT kid_from_pos = vid2pos_in_bitset_map.get(kid_from_vid);

                //Get bitset that 'pos_i' associated.
                xcom::BitSet * kid_from_bs = edge_indicator.get(kid_from_pos);
                if (kid_from_bs) {
                    for (INT pos_j = bs->get_next(pos_i);
                         pos_j >= 0; pos_j = bs->get_next(pos_j)) {
                        if (kid_from_bs->is_contain(pos_j)) {
                            //The edge 'i->pos_j' is redundant.
                            INT to_vid = vex_vec.get(pos_j);
                            UINT src_vid = vex_vec.get(i);
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
void DataDepGraph::dump(INT flag, INT rootoridx, CHAR * name)
{
    DUMMYUSE(rootoridx);
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    #undef INF_DDG_NAME
    #define INF_DDG_NAME "zddgraph.vcg"
    if (name == NULL) {
        name = INF_DDG_NAME;
    }
    if (HAVE_FLAG(flag, DDG_DELETE)) {
        UNLINK(name);
    }
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!",name));
    fprintf(h, "\n/*\n");

    StrBuf misc(64);
    if (HAVE_FLAG(flag, DDG_DUMP_OP_INFO)) {
        //Print issure interval.
        ORCt * ct;
        for (OR * o = ORBB_orlist(m_bb)->get_head(&ct); o != NULL;
             o = ORBB_orlist(m_bb)->get_next(&ct)) {
            fprintf(h, "\n\n");

            misc.clean();
            o->dump(misc, m_cg);
            fprintf(h, "%s", misc.buf);
            fprintf(h, "\tESTART:%5d, LSTART:%5d",
                    m_estart_vec.get(OR_id(o)),
                    m_lstart_vec.get(OR_id(o)));
        }
    }

    if (HAVE_FLAG(flag, DDG_DUMP_EDGE_INFO)) {
        //Print edge info
        fprintf(h, "\n\nEDGE INFO:\n");
        INT c;
        for (xcom::Edge * e = m_edges.get_first(c);
             e != NULL; e = m_edges.get_next(c)){
            OR * from = getOR(VERTEX_id(EDGE_from(e)));
            OR * to = getOR(VERTEX_id(EDGE_to(e)));

            fprintf(h, "\tfrom:");
            if (from != NULL) {
                misc.clean();
                from->dump(misc, m_cg);
            } else {
                //ASSERT0(0); //Miss OR.
                //Still dump the illegal graph.
            }

            fprintf(h, "%s", misc.buf);
            fprintf(h, "\tto  :");
            if (to != NULL) {
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
    INT c;
    for (xcom::Vertex * v = m_vertices.get_first(c);
         v != NULL; v = m_vertices.get_next(c)) {
        OR * o = getOR(VERTEX_id(v));
        if (o == NULL) {
            //No o responsed, need to update graph.
            misc.sprint(" shape:rhomb  color:white ");
            fprintf(h, "\nnode: { title:\"%d\" label:\"%d\" %s}",
                    VERTEX_id(v), VERTEX_id(v), misc.buf);
            continue;
        }

        //Cluster info
        CHAR * shape = "";
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
            if (OR_is_store(o)) {
                misc.sprint("width:40 ");
            } else if (OR_is_load(o)) {
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
                VERTEX_id(v), VERTEX_id(v), misc.buf);
    } //end for each of vertex

    //Print edges
    for (xcom::Edge * e = m_edges.get_first(c);
         e != NULL; e = m_edges.get_next(c)) {
        fprintf(h, "\nedge: { sourcename:\"%d\" targetname:\"%d\"}",
                VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)));
    }

    fprintf(h, "\n}\n");
    fclose(h);
}


void DataDepGraph::pushParam()
{
    DDGParam * ps = dup_param();
    m_params_stack.push(ps);
}


void DataDepGraph::popParam()
{
    DDGParam * ps = m_params_stack.pop();
    if (ps == NULL) return;
    m_ddg_param = *ps;
}


bool DataDepGraph::isGraphNode(OR * o)
{
    if (getOR(OR_id(o)) == NULL) {
        return false;
    }
    return true;
}
