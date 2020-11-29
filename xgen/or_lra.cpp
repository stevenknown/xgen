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
//Dumpf() for Vector<TY>.
void Dumpf_Svec(void * vec, UINT ty, CHAR * name, bool is_del)
{
    if (name == nullptr) {
        name = "matrix.tmp";
    }
    if (is_del) {
        UNLINK(name);
    }
    static INT g_count = 0;
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!", name));
    fprintf(h, "\nSVECOTR dump id:%d\n", g_count++);

    ///
    switch (ty) {
    case D_BOOL: {
        Vector<bool> * p = (Vector<bool>*)vec;
        for (INT i = 0; i <= p->get_last_idx(); i++) {
            fprintf(h, "%d", (INT)p->get(i));
            if (i != p->get_last_idx()) {
                fprintf(h, ", ");
            }
        }
        break;
    }
    case D_INT: {
        Vector<INT> * p = (Vector<INT> *)vec;
        for (INT i = 0; i <= p->get_last_idx(); i++) {
            fprintf(h, "%d", (INT)p->get(i));
            if (i != p->get_last_idx()) {
                fprintf(h, ", ");
            }
        }
        break;
    }
    default: ASSERTN(0, ("illegal ty"));
    }

    fprintf(h, "\n");
    fclose(h);
}
#endif


//
//START VarMap
//
VarMap::VarMap(ORBB * bb)
{
    m_num_or = ORBB_ornum(bb);
    UINT i = 0;
    m_vecidx2or.set(m_num_or, 0);
    for (OR * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb), i++) {
        m_oridx2vecidx.set(o->id(), i);
        m_vecidx2or.set(i, o);
    }
}


//Mapping from OR and cycle to constraint variable.
UINT VarMap::map_or_cyc2varidx(UINT or_idx, UINT cyc)
{
    ASSERT0(m_oridx2vecidx.get(or_idx) >= 0);
    INT vecidx = m_oridx2vecidx.get(or_idx);
    ASSERT0(vecidx <= m_vecidx2or.get_last_idx());
    return cyc * m_num_or + vecidx;
}


//Mapping from constraint variable to OR and cycle.
void VarMap::map_varidx2or_cyc(UINT var_idx,
                                OUT OR * & o,
                                OUT UINT & cyc)
{
    INT vecidx = var_idx % m_num_or;
    o = m_vecidx2or.get(vecidx);
    cyc = xfloor(var_idx, m_num_or);
}


OR * VarMap::map_vecidx2or(UINT i)
{
    return m_vecidx2or.get(i);
}


UINT VarMap::map_or2vecidx(OR * o)
{
    return m_oridx2vecidx.get(o->id());
}


//Mapping from inter-cluster variable index to coeff of the variable.
UINT VarMap::map_icc_varidx2coeff(UINT varidx)
{
    return varidx % (get_clust_num() + 1) == 0 ? 0 : get_exec_cyc_of_bus_or();
}
//END VarMap


//
//START ORMap
//
//ORMap
void * ORMap::xmalloc(INT size)
{
    ASSERTN(m_is_init, ("xcom::Graph must be initialized before clone."));
    ASSERTN(size > 0, ("xmalloc: size less zero!"));
    ASSERTN(m_pool != nullptr,("need graph pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    if (p == nullptr) { return nullptr; }
    ::memset(p,0,size);
    return p;
}


void ORMap::init()
{
    if (m_is_init) { return; }
    m_or2orlist_map.init();
    m_or_list.init();
    m_pool = smpoolCreate(64,MEM_COMM);
    m_is_init = true;
}


void ORMap::destroy()
{
    if (!m_is_init) { return; }
    ORId2ORListIter it;
    List<OR*> * orlist = nullptr;
    for (m_or2orlist_map.get_first(it, &orlist);
         orlist != nullptr; m_or2orlist_map.get_next(it, &orlist)) {
        orlist->destroy();
    }
    m_or2orlist_map.destroy();
    m_or_list.destroy();
    smpoolDelete(m_pool);
    m_is_init = false;
}


void ORMap::addOR(OR * o, OR * mapped)
{
    DUMMYUSE(mapped);

    ASSERTN(m_is_init, ("List not yet initialized."));
    List<OR*> * orlist = m_or2orlist_map.get(o->id());
    if (orlist == nullptr) {
        orlist = (List<OR*>*)xmalloc(sizeof(List<OR*>));
        orlist->init();
        m_or2orlist_map.set(o->id(), orlist);
    }
    ASSERTN(false == orlist->find(mapped), ("o layered already!"));
    orlist->append_tail(o);
    m_or_list.append_tail(o);
}


void ORMap::addORList(OR * o, ORList * mapped)
{
    ASSERTN(m_is_init, ("List not yet initialized."));
    List<OR*> * orlist = m_or2orlist_map.get(o->id());
    if (orlist == nullptr) {
        orlist = (List<OR*>*)xmalloc(sizeof(List<OR*>));
        orlist->init();
        m_or2orlist_map.set(o->id(), orlist);
    }
    for (OR * m = mapped->get_head(); m; m = mapped->get_next()) {
        ASSERTN(false == orlist->find(m), ("o layered already!"));
        orlist->append_tail(m);
    }
    m_or_list.append_tail(o);
}
//END ORMap


//
//START RefORBBList
//
ORBBUnit * RefORBBList::getBBUnit(ORBB * bb)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    for (ORBBUnit * bu = get_head(); bu != nullptr; bu = get_next()) {
        if (OR_BBUNIT_bb(bu) == bb) {
            return bu;
        }
    }
    return nullptr;
}


ORBBUnit * RefORBBList::addBB(ORBB * bb)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    //Keep bu unique.
    ORBBUnit * bu = getBBUnit(bb);
    if (bu != nullptr) {
        return bu;
    }
    bu = allocORBBUnit();
    OR_BBUNIT_bb(bu) = bb;
    OR_BBUNIT_or_list(bu) = (List<OR*>*)xmalloc(sizeof(List<OR*>));
    OR_BBUNIT_or_list(bu)->init();
    append_tail(bu);
    return bu;
}


OR * RefORBBList::addOR(ORBB * bb, OR * o)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    ORBBUnit * bu = addBB(bb);
    List<OR*> * or_list = OR_BBUNIT_or_list(bu);
    for (OR * tor = or_list->get_head();
         tor != nullptr; tor = or_list->get_next()) {
        if (tor == o) {
            return o;
        }
    }
    or_list->append_tail(o);
    return o;
}


List<OR*> * RefORBBList::getORList(ORBB * bb)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    ORBBUnit * bu = getBBUnit(bb);
    if (bu != nullptr) {
        return OR_BBUNIT_or_list(bu);
    }
    return nullptr;
}


OR * RefORBBList::removeOR(ORBB * bb, OR * o)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    ORBBUnit * bu = getBBUnit(bb);
    if (bu != nullptr) {
        return OR_BBUNIT_or_list(bu)->remove(o);
    }
    return nullptr;
}


//This function remove 'bb' from current list.
//It can be used to find designate bb.
//Return false when not found.
bool RefORBBList::removeBB(ORBB const* bb)
{
    ASSERTN(m_pool, ("List not yet initialized."));
    ORBBUnit * bu;

    //Keep bu unique.
    for (bu = get_head(); bu != nullptr; bu = get_next()) {
        if (OR_BBUNIT_bb(bu) == bb) {
            break;
        }
    }
    if (bu == nullptr) {
        return false;
    }
    remove(bu);
    freeORBBUnit(bu);
    return true;
}


void RefORBBList::init()
{
    if (m_pool != nullptr) { return; }
    m_pool = smpoolCreate(sizeof(ORBBUnit) * 8);
    List<ORBBUnit*>::init();
}


void RefORBBList::destroy()
{
    if (m_pool == nullptr) { return; }
    for (ORBBUnit * bu = get_head(); bu != nullptr; bu = get_next()) {
        freeORBBUnit(bu);
    }
    List<ORBBUnit*>::destroy();
    smpoolDelete(m_pool);
    m_pool = nullptr;
}
//END RefORBBList


//
//START RegFileAffinityGraph
//
//Initialize register dependence graph.
//    1. Create position vector.
void RegFileAffinityGraph::init(ORBB * bb, bool is_enable_far)
{
    if (m_is_init) { return; }
    xcom::Graph::init();
    m_pool = smpoolCreate(64,MEM_COMM);
    m_bb = bb;
    ASSERT0(ORBB_cg(bb));
    m_cg = ORBB_cg(bb);
    ASSERT0(m_cg);

    m_is_enable_far_edge = is_enable_far;
    set_unique(true);
    set_direction(false);
    m_id2lt.init();
    m_is_init = true;
}


void * RegFileAffinityGraph::cloneEdgeInfo(xcom::Edge * e)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    RDGEdgeInfo * ei =(RDGEdgeInfo*)xmalloc(sizeof(RDGEdgeInfo));
    ::memcpy(ei, EDGE_info(e), sizeof(RDGEdgeInfo));
    return (void*)ei;
}


void * RegFileAffinityGraph::cloneVertexInfo(xcom::Vertex * v)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    RDGVexInfo *vi = (RDGVexInfo*)xmalloc(sizeof(RDGVexInfo));
    ::memcpy(vi, VERTEX_info(v), sizeof(RDGVexInfo));
    return (void*)vi;
}


void RegFileAffinityGraph::clone(RegFileAffinityGraph & rdg)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    m_bb = rdg.bb();
    m_id2lt.copy(rdg.m_id2lt);
    xcom::Graph::clone((xcom::Graph&)rdg, true, true);
}


xcom::Edge * RegFileAffinityGraph::clustering(SR * sr1, SR * sr2,
                                              LifeTimeMgr & mgr)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    LifeTime * lt1 = mgr.getLifeTime(sr1);
    LifeTime * lt2 = mgr.getLifeTime(sr2);
    m_id2lt.set(lt1->id(), lt1);
    m_id2lt.set(lt2->id(), lt2);
    return addEdge(lt1->id(), lt2->id());
}


RDGEdgeInfo * RegFileAffinityGraph::getEdgeInfo(UINT start, UINT end)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    xcom::Edge *e = getEdge(start, end);
    if (e) {
        return (RDGEdgeInfo*)EDGE_info(e);
    }
    return nullptr;
}


//Contructing register-file affinity graph.
void RegFileAffinityGraph::build(LifeTimeMgr &, DataDepGraph &)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
}


void RegFileAffinityGraph::dump()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    #undef INF_RF_NAME
    #define INF_RF_NAME "zRF_AFFINI_GRAPH.vcg"
    UNLINK(INF_RF_NAME);
    FILE * h = fopen(INF_RF_NAME,"a+");
    ASSERTN(h, ("%s create failed!!!", INF_RF_NAME));
    fprintf(h, "\n/*\n");

    StrBuf tmp(64);
    for (INT i = 0; i <= m_id2lt.get_last_idx(); i++) {
        LifeTime * lt = m_id2lt[i];
        if (lt == nullptr) { continue; }

        fprintf(h, "\nLT%d:\t", lt->id());
        ASSERT0(m_cg);
        tmp.clean();
        fprintf(h, "[%s]", LT_sr(lt)->get_name(tmp, m_cg));
    }

    fprintf(h, "\n*/\n");
    fprintf(h, "graph: {"
          "title: \"xcom::Graph\"\n"
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
          "layoutalgorithm: mindepthslow\n"
          "node.borderwidth: 3\n"
          "node.color: lightcyan\n"
          "node.textcolor: darkred\n"
          "node.bordercolor: red\n"
          "edge.color: darkgreen\n");

    //Print nodes
    INT c;
    for(xcom::Vertex * v = get_first_vertex(c);
        v != nullptr; v = get_next_vertex(c)){
        fprintf(h,
        "\nnode: { title:\"%d\" label:\"%d\" shape:circle color:orange }",
        VERTEX_id(v), VERTEX_id(v));
    }

    //Print edges
    EdgeIter ite;
    for(xcom::Edge * e = get_first_edge(ite); e != nullptr;
        e = get_next_edge(ite)){
        ASSERTN(EDGE_info(e) != nullptr, ("Lost edge information"));
        fprintf(h,
                "\nedge: { sourcename:\"%d\" targetname:\"%d\" %s label:\"%d\"}",
                VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)),
                m_is_direction?"":"arrowstyle:none", RDGEI_exp_val(EDGE_info(e)));
    }

    fprintf(h, "\n}\n");
    fclose(h);
}


//
//START InterfGraph
//
InterfGraph::InterfGraph()
{
    m_is_init = false;
}


InterfGraph::~InterfGraph()
{
    destroy();
}


//Initialize interference graph.
//Create position vector.
void InterfGraph::init(ORBB * bb, bool clustering, bool estimate)
{
    if (m_is_init) { return; }

    xcom::Graph::init();
    m_bb = bb;
    m_clustering = clustering;
    m_is_estimate = estimate;
    m_lt_mgr = nullptr;
    set_unique(true);
    set_direction(false);
    m_is_init = true;
}


void InterfGraph::destroy()
{
    INT i;
    if (!m_is_init) { return; }
    m_lt_mgr = nullptr;
    for (i = 0; i < RF_NUM; i++) {
        m_lt_rf_group[i].clean();
    }
    for (i = 0; i < CLUST_NUM; i++) {
        m_lt_cl_group[i].clean();
    }
    xcom::Graph::destroy();
    m_is_init = false;
}


void InterfGraph::clone(InterfGraph & ig)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    m_bb = ig.bb();
    m_clustering = ig.m_clustering;
    m_is_estimate = ig.m_is_estimate;
    xcom::Graph::clone(ig, true, true);
}


ORBB * InterfGraph::bb()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    return m_bb;
}


//Is 'lt' on graph node.
bool InterfGraph::isGraphNode(LifeTime const* lt) const
{
    return getVertex(lt->id()) != nullptr;
}


//More considerations tend to reduce edge of interference graph.
//Omitting interference caused by predicate register,
//since we do not allocate predicate register during LRA, but in GRA.
//Similarly it also could be handled in If-Conversion.
bool InterfGraph::isInterferred(LifeTime * lt1, LifeTime * lt2)
{
    ASSERTN(LT_sr(lt1)->is_reg() && LT_sr(lt2)->is_reg(),
            ("sr is not register"));
    if (lt1->id() == lt2->id() || !LT_pos(lt1)->is_intersect(*LT_pos(lt2))) {
        return false;
    }

    //e.g: SR428, cluster is BUS, here its life time is still
    //interfered with lifetimes of GSR361 whose cluster is other one.
    //SR428[R1] , :- copy_b SR97(p0)[P1] , GSR361(r7)[R1](sv:r7); No Bus
    //SR430[A1] , :- bus_b1_to_m1 SR97(p0)[P1] , SR428[R1]; BUS

    //if (LT_cluster(lt1) == LT_cluster(lt2) || !m_clustering)
    if (SR_regfile(LT_sr(lt1)) == SR_regfile(LT_sr(lt2)) ||
        //Just because of the identical regfile
        //Is_Bus_Clsuter(LT_cluster(lt1)) ||
        //Is_Bus_Clsuter(LT_cluster(lt2)) ||
        (m_is_estimate && //In estimating mode, regfile is dispensable.
         (SR_regfile(LT_sr(lt1)) == RF_UNDEF ||
          SR_regfile(LT_sr(lt2)) == RF_UNDEF))) {
        //CASE: Under -O0, we need to allocate the predicate register.
        //if (!SR_is_pred(LT_sr(lt1)) &&
        //    !SR_is_pred(LT_sr(lt2)))
        {
            //Add an edge if one of lt1 and lt2 still not with register
            //assigned. But it may confused if we wanna review the
            //interference degree of whole interference graph. Add an
            //option to enjoy it.
            if (LT_has_allocated(lt1) &&
                LT_has_allocated(lt2) &&
                !m_is_estimate) {
                return false;
            } else {
                return true;
            }
        }
    }

    return false;
}


void InterfGraph::build(LifeTimeMgr & mgr)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(mgr.getMaxLifeTimeLen() > 0, ("Need to create life time at first"));
    if (ORBB_ornum(m_bb) == 0) { return; }
    m_lt_mgr = &mgr;
    //Check interference
    LifeTimeVecIter it;
    for (LifeTime * lt1 = mgr.getFirstLifeTime(it);
         lt1 != nullptr; lt1 = mgr.getNextLifeTime(it)) {
        ASSERTN(LT_cluster(lt1) != CLUST_UNDEF || !m_clustering,
                ("Life time cluster is illegal."));
        m_lt_rf_group[SR_regfile(LT_sr(lt1))].append_tail(lt1);
        ASSERTN(LT_cluster(lt1) >= CLUST_UNDEF, ("?? cluster"));
        m_lt_cl_group[LT_cluster(lt1)].append_tail(lt1);
    }
    for (UINT c = CLUST_UNDEF; c < CLUST_NUM; c++) {
        LifeTimeList * lst = &m_lt_cl_group[c];
        LifeTimeListIter it1;
        for (lst->get_head(&it1); it1 != nullptr; lst->get_next(&it1)) {
            LifeTime * lt1 = it1->val();
            addVertex(lt1->id());
            LifeTimeListIter it2 = it1;
            for (lst->get_next(&it2); it2 != nullptr; lst->get_next(&it2)) {
                LifeTime * lt2 = it2->val();
                if (isInterferred(lt1, lt2)) {
                    addEdge(lt1->id(), lt2->id());
                }
            }
        }
    }
}


void InterfGraph::getLifeTimeList(List<LifeTime*> & lt_group, CLUST clust)
{
    //'clust' can be CLUST_UNDEF
    for (LifeTime *lt = m_lt_cl_group[clust].get_tail();
         lt != nullptr; lt = m_lt_cl_group[clust].get_prev()) {
        lt_group.append_tail(lt);
    }
}


//Get all lifetimes which belong to group of 'regfile'.
void InterfGraph::getLifeTimeList(List<LifeTime*> & lt_group, REGFILE regfile)
{
    ASSERTN(regfile != RF_UNDEF, ("Unknown regfile."));
    for (LifeTime * lt = m_lt_rf_group[regfile].get_tail();
         lt != nullptr;
         lt = m_lt_rf_group[regfile].get_prev()) {
        lt_group.append_tail(lt);
    }
}


void InterfGraph::getLifeTimeList(List<LifeTime*> & lt_group,
                                  REGFILE regfile,
                                  INT start,
                                  INT end)
{
    DUMMYUSE(start);
    DUMMYUSE(end);
    ASSERTN(regfile != RF_UNDEF, ("Unknown regfile."));
    for (LifeTime *lt = m_lt_rf_group[regfile].get_tail();
         lt != nullptr;
         lt = m_lt_rf_group[regfile].get_prev()) {
        lt_group.append_tail(lt);
    }
}


//Move life time from one cluster to another cluster.
void InterfGraph::moveLifeTime(LifeTime * lt,
                               CLUST from_clust,
                               INT from_regfile,
                               CLUST to_clust,
                               INT to_regfile)
{
    ASSERTN(from_clust >= CLUST_UNDEF && to_clust >= CLUST_UNDEF,
            ("Illegal info"));
    ASSERTN(from_regfile >= RF_UNDEF && to_regfile >= RF_UNDEF,
            ("Illegal info"));
    m_lt_rf_group[from_regfile].remove(lt);
    m_lt_rf_group[to_regfile].append_tail(lt);
    m_lt_cl_group[from_clust].remove(lt);
    m_lt_cl_group[to_clust].append_tail(lt);
}


//Upate interferefering neighbors for 'lt' in SAME cluster.
//'prio_regfile' : Prior regfile of 'lt'
void InterfGraph::updateLifeTimeInterf(LifeTime * lt, INT prio_regfile)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    ASSERTN(prio_regfile > RF_UNDEF && prio_regfile < RF_NUM,
            ("Unknown regfile."));
    ASSERTN(lt && SR_regfile(LT_sr(lt)) != RF_UNDEF, ("Unknown regfile."));
    ASSERTN(m_lt_rf_group[prio_regfile].find(lt), ("Unmatch regfile"));
    ASSERTN(!m_lt_rf_group[SR_regfile(LT_sr(lt))].find(lt),
            ("Unmatch regfile"));

    INT new_regfile = SR_regfile(LT_sr(lt));
    if (new_regfile == prio_regfile) {
        return;
    }
    m_lt_rf_group[prio_regfile].remove(lt);
    m_lt_rf_group[new_regfile].append_tail(lt);
    removeVertex(lt->id());//In order to erase all edges correalte to 'lt'
    addVertex(lt->id());

    //Append edge if life times are interfered.
    for (UINT i = 0; i < m_lt_rf_group[new_regfile].get_elem_count(); i++) {
        LifeTime * ni = m_lt_rf_group[new_regfile].get_head_nth(i);
        if (isInterferred(lt, ni)) {
            addEdge(lt->id(), LT_id(ni));
        }
    }
}


UINT InterfGraph::getInterfDegree(LifeTime * lt)
{
    return xcom::Graph::getDegree(lt->id());
}


void InterfGraph::getNeighborList(IN OUT List<LifeTime*> & ni_list,
                                  LifeTime * lt)
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
    List<UINT> int_ni_list;
    xcom::Graph::getNeighborList(int_ni_list, lt->id());
    UINT n = int_ni_list.get_elem_count();
    UINT vid = int_ni_list.get_head();
    for (UINT i = 0; i < n; i++, vid = int_ni_list.get_next()) {
        LifeTime * ni = m_lt_mgr->getLifeTime(vid);
        ASSERTN(ni, ("Negighbor(%d) does not corresponding to life time %d",
                     vid, lt->id()));
        ni_list.append_head(ni);
    }
}


void InterfGraph::dump()
{
    ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));

    #undef INF_IG_NAME
    #define INF_IG_NAME "zif_graph.vcg"
    UNLINK(INF_IG_NAME);

    FILE * h = fopen(INF_IG_NAME, "a+");
    ASSERTN(h, ("%s create failed!!!",INF_IG_NAME));

    fprintf(h, "\n/*\n");
    List<LifeTime*> lt_list[RF_NUM];
    LifeTimeVecIter c;
    for (LifeTime * lt = m_lt_mgr->getFirstLifeTime(c);
         lt != nullptr; lt = m_lt_mgr->getNextLifeTime(c)) {
        REGFILE regfile = SR_regfile(LT_sr(lt));
        ASSERTN(regfile >= RF_UNDEF && regfile < RF_NUM, ("Illegal regfile"));
        lt_list[regfile].append_tail(lt);
    }

    for (INT regfile = RF_UNDEF; regfile < RF_NUM; regfile++) {
        fprintf(h, "\nREG_FILE:%-15s", tmGetRegFileName((REGFILE)regfile));
        for (LifeTime * lt = lt_list[regfile].get_head();
             lt != nullptr; lt = lt_list[regfile].get_next()) {
            fprintf(h, "LT(%d),", lt->id());
        }
    }

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
          "layoutalgorithm: mindepthslow\n"
          "node.borderwidth: 3\n"
          "node.color: lightcyan\n"
          "node.textcolor: darkred\n"
          "node.bordercolor: red\n"
          "edge.color: darkgreen\n");

    //Print nodes
    VertexIter vit;
    for(xcom::Vertex * v = get_first_vertex(vit);
        v != nullptr; v = get_next_vertex(vit)){
        fprintf(h, "\nnode: { title:\"%d\" label:\"%d\" "
                   "shape:circle fontname:\"courB\" color:gold}",
                   VERTEX_id(v), VERTEX_id(v));
    }

    //Print edges
    EdgeIter ite;
    for(xcom::Edge * e = get_first_edge(ite);
        e != nullptr; e = get_next_edge(ite)){
        fprintf(h, "\nedge: { sourcename:\"%d\" targetname:\"%d\" %s}",
                VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)),
                m_is_direction ? "" : "arrowstyle:none");
    }

    fprintf(h,"\n}\n");
    fclose(h);
}
//END InterfGraph

//
//START LifeTime
//
void LifeTime::dump(LifeTimeMgr * mgr)
{
    ASSERT0(mgr);
    xcom::StrBuf buf(6);
    xoc::note(mgr->getRegion(), "\n==-- LT(%3d)[%s] --==", LT_id(this),
              LT_sr(this)->get_name(buf, mgr->getCG()));
    buf.clean();
    for (INT i = LT_pos(this)->get_first();
         i >= 0; i = LT_pos(this)->get_next(i)) {
        PosInfo * pi = LT_desc(this).get(i);
        if (pi == nullptr) { continue; }
        xoc::note(mgr->getRegion(),
                  "\n=- POS%d[%s]:", i, pi->is_def() ? "def" : "use");
        OR const* o = mgr->getOR(i);
        ASSERT0(o);
        buf.clean();
        o->dump(buf, mgr->getCG());
        xoc::prt(mgr->getRegion(), "%s", buf.buf);
    }
    xoc::note(mgr->getRegion(), "\n");
}
//END LifeTime


//
//START SibMgr
//
void SibMgr::destroy()
{
    for (List<LifeTime*> * ltlist = m_ltlist.get_head();
        ltlist != nullptr; ltlist = m_ltlist.get_next()) {
        delete ltlist;
    }
    m_lt2nextsiblist.clean();
    m_lt2prevsiblist.clean();
    m_ltlist.clean();
}


UINT SibMgr::countNumOfPrevSib(LifeTime const* lt) const
{
    UINT count = 0; //the number of previous siblings.
    SibMgr * pthis = const_cast<SibMgr*>(this);
    SibList const* sibs = pthis->getPrevSibList(const_cast<LifeTime*>(lt));
    if (sibs != nullptr) {
        C<LifeTime*> * ct;
        for (sibs->get_head(&ct); ct != sibs->end();
            ct = sibs->get_next(ct)) {
            count++;
            LifeTime const* sib = ct->val();
            count += countNumOfPrevSib(sib);
        }
    }
    return count;
}


UINT SibMgr::countNumOfNextSib(LifeTime const* lt) const
{
    UINT count = 0; //the number of next siblings.
    SibMgr * pthis = const_cast<SibMgr*>(this);
    SibList const* sibs = pthis->getNextSibList(const_cast<LifeTime*>(lt));
    if (sibs != nullptr) {
        C<LifeTime*> * ct;
        for (sibs->get_head(&ct); ct != sibs->end();
            ct = sibs->get_next(ct)) {
            LifeTime const* sib = ct->val();
            count++;
            count += countNumOfNextSib(sib);
        }
    }
    return count;
}


UINT SibMgr::countNumOfSib(LifeTime const* lt) const
{
    return countNumOfPrevSib(lt) + countNumOfNextSib(lt);
}


//Set prev and next are sibling lifetimes.
void SibMgr::setSib(LifeTime * prev, LifeTime * next)
{
    #ifdef _DEBUG_
    if (SR_phy_reg(LT_sr(prev)) != REG_UNDEF &&
        SR_phy_reg(LT_sr(next)) != REG_UNDEF) {
        //Sibling lt should be assigned continuous register.
        int a1 = SR_phy_reg(LT_sr(prev)) + 1;
        int b1 = SR_phy_reg(LT_sr(next));
        DUMMYUSE(a1 && b1);
        ASSERT0(SR_phy_reg(LT_sr(prev)) + 1 == SR_phy_reg(LT_sr(next)));
    }
    #endif
    SibList * nextsibs = m_lt2nextsiblist.get(prev);
    if (nextsibs == nullptr) {
        nextsibs = new SibList();
        m_lt2nextsiblist.set(prev, nextsibs);
        m_ltlist.append_head(nextsibs);
    }
    if (!nextsibs->find(next)) {
        nextsibs->append_head(next);
    }

    SibList * prevsibs = m_lt2prevsiblist.get(next);
    if (prevsibs == nullptr) {
        prevsibs = new SibList();
        m_lt2prevsiblist.set(next, prevsibs);
        m_ltlist.append_head(prevsibs);
    }
    if (!prevsibs->find(prev)) {
        prevsibs->append_head(prev);
    }
}
//END SibMgr


//
//START LifeTimeMgr
//
LifeTimeMgr::LifeTimeMgr(CG * cg)
{
    ASSERT0(cg);
    m_cg = cg;
    m_rg = cg->getRegion();
    ASSERT0(m_rg);
    m_bb = nullptr;
    m_is_init = false;
}


OR * LifeTimeMgr::getOR(UINT pos)
{
    ASSERTN(((INT)pos) >= 0 && ((INT)pos) <= m_pos2or_map.get_last_idx(),
            ("position overrange"));
    return m_pos2or_map.get(pos);
}


INT LifeTimeMgr::getPos(OR * o, bool is_result)
{
    ASSERTN(o != nullptr && m_or2pos_map.get(o->id()) > 0, ("o is illegal"));
    if (is_result) {
        ASSERTN(m_or2pos_map.get(o->id()) > 1, ("o is illegal"));
        return m_or2pos_map.get(o->id());
    } else {
        ASSERTN(m_or2pos_map.get(o->id()) > 0, ("o is illegal"));
        return m_or2pos_map.get(o->id()) - 1;
    }
}


void LifeTimeMgr::init(ORBB * bb, bool is_verify,
                       bool clustering) //Only avaible for multi-cluster machine
{
    if (m_is_init) { return; }
    m_gra_used.clean();
    m_bb = bb;
    m_lt_count = LTID_UNDEF;
    m_max_lt_len = 0;
    m_pool = smpoolCreate(64, MEM_COMM);

    m_lt_tab.init();
    m_sr2lt_map.init();
    m_pos2or_map.init();
    m_or2pos_map.init();
    m_oridx2sr_livein_gsr_spill_pos.init();
    m_oridx2sr_liveout_gsr_reload_pos.init();
    m_lt2usable_reg_set_map.init();
    m_lt2antici_reg_set_map.init();

    m_is_verify = is_verify;
    m_clustering = clustering;
    m_is_init = true;
}


void LifeTimeMgr::destroy()
{
    if (!m_is_init) { return; }
    //Destroy and free allocated memory object.
    freeAllLifeTime();
    m_lt_tab.destroy();
    m_sr2lt_map.destroy();
    m_pos2or_map.destroy();
    m_or2pos_map.destroy();
    m_lt2usable_reg_set_map.destroy();
    m_lt2antici_reg_set_map.destroy();
    m_oridx2sr_livein_gsr_spill_pos.destroy();
    m_oridx2sr_liveout_gsr_reload_pos.destroy();
    m_sibmgr.destroy();
    m_bb = nullptr;
    m_lt_count = LTID_UNDEF;
    m_max_lt_len = 0;
    smpoolDelete(m_pool);
    m_is_init = false;
}


void LifeTimeMgr::computeLifeTimeCluster(LifeTime * lt, OR * o)
{
    SR * sr = LT_sr(lt);
    CLUST clust = CLUST_UNDEF;

    //First finding out bus cluster even if 'o' is given.
    if (m_cg->isBusSR(sr)) {
        if (m_is_verify) {
            if (LT_cluster(lt) != CLUST_UNDEF) {
                ASSERTN(m_cg->isBusCluster(LT_cluster(lt)),
                        ("Unmatch cluster"));
            }
        }
        LT_cluster(lt) = m_cg->computeClusterOfBusOR(o);
        return;
    } else if (m_cg->isDedicatedSR(sr)) {
        LT_cluster(lt) = m_cg->mapSR2Cluster(o, sr);
        ASSERTN(LT_cluster(lt) != CLUST_UNDEF, ("No info of dedicated sr."));
        return;
    } else if (LT_has_allocated(lt)) {
        clust = m_cg->mapReg2Cluster(sr->getPhyReg());
        if (LT_cluster(lt) != CLUST_UNDEF && m_is_verify) {
            //We allow life time that cross different cluster in order
            //to estimating optimization purpose.
            //And that makes LifeTimeMgr be one strong tool in
            //evaluation on register pressure and live variables.
            if (!m_cg->isBusCluster(clust) &&
                !m_cg->isBusCluster(LT_cluster(lt))) {
                //Bus-operation cross multi-clusters.
                ASSERTN(LT_cluster(lt) == clust, ("Unmatch cluster"));
            }
        } else {
            //Omit regfile which can cross bus.
            if (clust != CLUST_UNDEF) {
                //Always assigning life time the latest cluster info.
                LT_cluster(lt) = clust;
            }
        }
    } else if (!m_clustering) {
        return;
    } else if (sr->getRegFile() != RF_UNDEF) {
        clust = m_cg->mapRegFile2Cluster(sr->getRegFile(), sr);
        if (LT_cluster(lt) != CLUST_UNDEF && m_is_verify) {
            //We allow life time that cross different cluster in
            //order to estimating optimization purpose.
            //And that makes LifeTimeMgr be one strong tool in
            //evaluation on register pressure and live variables.
            if (!m_cg->isBusCluster(clust) &&
                !m_cg->isBusCluster(LT_cluster(lt))) {
                //Bus-operation cross multi-clusters.
                ASSERTN(LT_cluster(lt) == clust, ("Unmatch cluster"));
            }
        } else {
            //Omit regfile which can cross bus.
            if (clust != CLUST_UNDEF) {
                //Always assigning life time the latest cluster info.
                LT_cluster(lt) = clust;
            }
        }
    } else if (o != nullptr) {
        clust = m_cg->computeORCluster(o);
        if (m_cg->isBusCluster(clust)) {
            clust = m_cg->mapSR2Cluster(o, sr);
        }
        ASSERTN(!m_cg->isBusCluster(clust),
                ("life time live on bus should not be here"));
        if (m_is_verify) {
            if (LT_cluster(lt) != CLUST_UNDEF && clust != CLUST_UNDEF) {
                ASSERTN(LT_cluster(lt) == clust, ("Unmatch cluster"));
                return;
            }
        }

        //Avoid assigning BUS as possible, hereby need do more check.
        if (clust != CLUST_UNDEF) {
            LT_cluster(lt) = clust;
            return;
        }
    } //end else
}


//Every OR which refering sr must be assigned to same cluster, therefore
//the only time to record cluster information is the first meeting with sr.
LifeTime * LifeTimeMgr::allocLifeTime(SR * sr, OR * o)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    ASSERTN(m_max_lt_len > 0, ("Life time length is overrange."));
    LifeTime * lt = (LifeTime*)xmalloc(sizeof(LifeTime));
    LT_pos(lt) = getBitSetMgr()->create();
    LT_desc(lt).init();
    LT_id(lt) = ++m_lt_count;
    LT_sr(lt) = sr;
    m_lt_tab.set(lt->id(), lt);
    computeLifeTimeCluster(lt, o);
    m_sr2lt_map.set(sr, lt);
    return lt;
}


LifeTime * LifeTimeMgr::clone_lifetime(LifeTime * lt)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    ASSERTN(lt, ("Life time is nullptr."));

    LifeTime * newlt = (LifeTime*)xmalloc(sizeof(LifeTime));
    LT_pos(newlt)->copy(*LT_pos(lt));
    LT_desc(newlt).init();
    for (INT i = LT_pos(lt)->get_first();
         i >= 0; i = LT_desc(lt).get_next(i)) {
        PosInfo *pi = LT_desc(lt).get(i);
        if(pi != nullptr){
            PosInfo *newpi = (PosInfo*)xmalloc(sizeof(PosInfo));
            ::memcpy(newpi, pi, sizeof(PosInfo));
            LT_desc(newlt).set(i, newpi);
        }
    }

    LT_cluster(newlt) = LT_cluster(lt);
    LT_id(newlt) = lt->id();
    LT_sr(newlt) = LT_sr(lt);
    return newlt;
}


//Free memory resource of sub-object of lt.
void LifeTimeMgr::freeLifeTime(LifeTime * lt)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    LT_desc(lt).destroy(); //Destory vector
    m_cg->getBitSetMgr()->free(lt->getPos());
    LT_pos(lt) = nullptr;
}


//Destroy and free allocated memory object.
void LifeTimeMgr::freeAllLifeTime()
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    for (INT i = LTID_UNDEF + 1; i <= m_lt_tab.get_last_idx(); i++) {
        LifeTime * lt = m_lt_tab.get(i);
        if (lt != nullptr) {
            //Some lifetime may be removed during LRA.
            freeLifeTime(lt);
        }
    }
}


UINT LifeTimeMgr::getLiftTimeCount() const
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    return m_lt_tab.get_last_idx() + 1;
}


void LifeTimeMgr::removeLifeTime(LifeTime * lt)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    m_lt_tab.set(lt->id(), nullptr);
    m_lt2usable_reg_set_map.setAlways(lt, nullptr);
    m_lt2antici_reg_set_map.setAlways(lt, nullptr);
    m_sr2lt_map.setAlways(LT_sr(lt), nullptr);
    freeLifeTime(lt);
}


LifeTime * LifeTimeMgr::getLifeTime(UINT id)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    return m_lt_tab.get(id);
}


LifeTime * LifeTimeMgr::getLifeTime(SR * sr)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    return m_sr2lt_map.get(sr);
}


LifeTime * LifeTimeMgr::getNextLifeTime(LifeTimeVecIter & cur)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    INT i = cur + 1;
    for (; i <= m_lt_tab.get_last_idx(); i++) {
        LifeTime * lt = m_lt_tab.get(i);
        if (lt != nullptr) {
            cur = i;
            return lt;
        }
    }
    cur = i;
    return nullptr; 
}


LifeTime * LifeTimeMgr::getFirstLifeTime(LifeTimeVecIter & cur)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    INT i = 0;
    for (; i <= m_lt_tab.get_last_idx(); i++) {
        LifeTime * lt = m_lt_tab.get(i);
        if (lt != nullptr) {
            cur = i;
            return lt;
        }
    }
    cur = i;
    return nullptr;
}


//Must call init() before invoke this function.
bool LifeTimeMgr::clone(LifeTimeMgr & mgr)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    m_max_lt_len = mgr.getMaxLifeTimeLen();
    m_lt_count = mgr.getLiftTimeCount();
    m_gra_used.copy(mgr.m_gra_used);
    m_bb = bb();
    m_clustering = mgr.m_clustering;
    m_is_verify = mgr.m_is_verify;

    //Clone each life time.
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        LifeTime * newLT = clone_lifetime(lt);
        RegSet * rs = mgr.m_lt2usable_reg_set_map.get(lt);
        //clone
        if (rs) {
            RegSet * new_rs = m_cg->allocRegSet();
            *new_rs = *rs;
            m_lt2usable_reg_set_map.set(newLT, new_rs);
        }

        //clone
        rs = mgr.m_lt2antici_reg_set_map.get(lt);
        if (rs) {
            RegSet * new_rs = m_cg->allocRegSet();
            new_rs->copy(*rs);
            m_lt2antici_reg_set_map.set(newLT, new_rs);
        }

        m_lt_tab.append(newLT);

        //May override prior life time mapping.
        m_sr2lt_map.set(LT_sr(newLT), newLT);
        LT_preferred_reg(newLT) = LT_preferred_reg(lt);
    }

    m_pos2or_map.copy(mgr.m_pos2or_map);
    m_or2pos_map.copy(mgr.m_or2pos_map);
    m_oridx2sr_livein_gsr_spill_pos.copy(mgr.m_oridx2sr_livein_gsr_spill_pos);
    m_oridx2sr_liveout_gsr_reload_pos.copy(
        mgr.m_oridx2sr_liveout_gsr_reload_pos);
    return true;
}


//Verfication to LifeTime and cluster information.
bool LifeTimeMgr::verifyLifeTime()
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    LifeTimeVecIter c;
    for (LifeTime * lt = getFirstLifeTime(c);
         lt != nullptr; lt = getNextLifeTime(c)) {
        ASSERTN(LT_sr(lt), ("Not any SR binding to LifeTime."));
        ASSERTN(LT_cluster(lt) != CLUST_UNDEF, ("Miss cluster info"));

        if (m_cg->isBusSR(LT_sr(lt))) {
            ASSERTN(m_cg->isBusCluster(LT_cluster(lt)),
                    ("Unmatch cluster infor"));
            continue; //Do not need to check occurrence.
        }

        ASSERTN(LT_desc(lt).get(0) == nullptr,
                ("Position 0 is in-exposed use"));
        ASSERTN(LT_desc(lt).get(m_max_lt_len - 1) == nullptr,
                ("Position %d is out-exposed use, last_idx is %d",
                 m_max_lt_len - 1, LT_desc(lt).get_last_idx()));

        //Check for all ORs which lifetime lived through.
        for (INT i = LT_pos(lt)->get_first();
             i >= 0; i = LT_desc(lt).get_next(i)) {
            if (LT_desc(lt).get(i) == nullptr) { continue; }

            OR * o = m_pos2or_map.get(i);
            if (o->is_bus() || o->is_asm() || o->is_fake()) {
                //Inter-cluster communication operations
                //do not belong to any life time.
                //Therefore we do not verify Bus Operation liked operations.

                //ASSERTN(m_cg->isBusCluster(LT_cluster(lt)),
                //        ("Unmatch cluster infor"));
            } else {
                //For target without, all instructions are belong to
                //identical cluster.
                ASSERTN(m_cg->computeORCluster(o) == LT_cluster(lt),
                       ("Unmatch cluster"));
            }
        }
    }
    return true;
}


UINT LifeTimeMgr::getOccCount(LifeTime * lt)
{
    ASSERTN(lt, ("getOccCount:lt is nullptr"));
    INT count = 0;
    for (INT i = LT_pos(lt)->get_first(); i >= 0; i = LT_desc(lt).get_next(i)) {
        if (LT_desc(lt).get(i)) {
            count++;
        }
    }
    return count;
}


//Record all registers have been used during GRA.
void LifeTimeMgr::setGRAUsedReg(SR * sr)
{
    if (sr->is_reg() && sr->getPhyReg() != REG_UNDEF) {
        m_gra_used.bunion(sr->getPhyReg());
    }
}


RegSet * LifeTimeMgr::getGRAUsedReg()
{
    return &m_gra_used;
}


//Get occurrences in between of 'pos1' and 'pos2',
//and also include both of them.
void LifeTimeMgr::getOccInRange(INT start, INT end, IN LifeTime * lt,
                                IN OUT List<INT> &occs)
{
    ASSERTN(start <= end && start >= 0 && end >= 0, ("out of range"));
    ASSERTN(start >= LT_FIRST_POS && end <= (INT)(getMaxLifeTimeLen() - 1),
            ("out of range"));
    start = MAX(start, LT_pos(lt)->get_first());
    end = MIN(end, LT_pos(lt)->get_last());
    PosInfo * pi;
    for (INT i = start; i <= end; i++) {
        if (i == LT_FIRST_POS || i == (INT)(getMaxLifeTimeLen() - 1)) {
            occs.append_tail(i);
        } else if ((pi = LT_desc(lt).get(i)) != nullptr) {
            occs.append_tail(i);
        }
    }
}


//Get backward occurrences of 'pos'
//e.g:Lowest_Pos...Backward_Occ....Pos.....Highest_Pos
INT LifeTimeMgr::getBackwardOcc(INT pos, IN LifeTime * lt, IN OUT bool * is_def)
{
    INT backwpos = -1;
    if (pos <= LT_FIRST_POS) {
        return -1;
    }
    //Find the DEF of pos1.
    ASSERTN(pos >= LT_FIRST_POS && pos <= (INT)(getMaxLifeTimeLen() - 1),
            ("Illegal position"));
    INT i;
    //Get first pos of current life time.
    INT firstpos = LT_pos(lt)->get_first();
    for (i = pos - 1; i >= firstpos; i--) {
        PosInfo * pi = LT_desc(lt).get(i);
        if (pi != nullptr) {
            backwpos = i;
            if (pi->is_def()) {
                *is_def = true;
            } else {
                *is_def = false;
            }
            break;
        }
    }
    if (backwpos == -1) { //Not find any occ.
        if (firstpos == LT_FIRST_POS) {
            backwpos = LT_FIRST_POS;
            *is_def = true;
        }
    }
    return backwpos;
}


//Get forward occurrences of 'pos'
//e.g:Lowest_Pos....Pos...Forward_Occ...Highest_Pos
INT LifeTimeMgr::getForwardOcc(INT pos, IN LifeTime * lt, IN OUT bool * is_def)
{
    INT forwpos = -1;
    ASSERTN(pos >= LT_FIRST_POS && pos <= (INT)(getMaxLifeTimeLen() - 1),
            ("Illegal position"));
    if (pos >= (INT)(getMaxLifeTimeLen() - 1)) {
        return -1;
    }
    INT lastpos = LT_pos(lt)->get_last(); //Get last pos of current life time.
    for (INT i = pos + 1; i <= lastpos; i++) {
        PosInfo * pi = LT_desc(lt).get(i);
        if (pi != nullptr) {
            forwpos = i;
            if (pi->is_def()) {
                *is_def = true;
            } else {
                *is_def = false;
            }
            break;
        }
    }
    if (forwpos == -1) { //Not find any occ.
        if (lastpos == (INT)(getMaxLifeTimeLen() - 1)) {
            forwpos = (getMaxLifeTimeLen() - 1);
            *is_def = false;
        }
    }
    return forwpos;
}


INT LifeTimeMgr::get_backward_use_occ(INT pos, IN LifeTime * lt)
{
    bool is_def = false;
    do {
        pos = getBackwardOcc(pos, lt, &is_def);
        if (!is_def) {
            break;
        }
    } while(pos != -1);
    return pos;
}


INT LifeTimeMgr::getForwardOccForUSE(INT pos, IN LifeTime * lt)
{
    bool is_def = false;
    do {
        pos = getForwardOcc(pos, lt, &is_def);
        if (!is_def) {
            break;
        }
    } while(pos != -1);
    return pos;
}


INT LifeTimeMgr::getBackwardOccForDEF(INT pos, IN LifeTime * lt)
{
    bool is_def = false;
    do {
        pos = getBackwardOcc(pos, lt, &is_def);
        if (is_def) {
            break;
        }
    } while(pos != -1);
    return pos;
}


INT LifeTimeMgr::getForwardOccForDEF(INT pos, IN LifeTime * lt)
{
    bool is_def = false;
    do {
        pos = getForwardOcc(pos, lt, &is_def);
        if (is_def) {
            break;
        }
    } while(pos != -1);
    return pos;
}


void LifeTimeMgr::reviseLTCase1(LifeTime * lt)
{
    bool is_def = false;
    INT first_occ = getForwardOcc(LT_pos(lt)->get_first(), lt, &is_def);
    ASSERTN(first_occ > LT_FIRST_POS, ("empty life tiem, have no any occ!"));

    #ifdef _DEBUG_
    if (is_def) {
        //We allowed life time which live out Cond-Exec operations,
        //but this may incur position be recorded by life time.
        //Here we check it whether a Cond-Exec operation.
        OR * t = getOR(first_occ);
        ASSERTN(m_cg->isCondExecOR(t), ("Illegal life time"));
    }
    #endif

    xcom::BitSet * tmp = LT_pos(lt);
    LT_pos(lt) = LT_pos(lt)->get_subset_in_range(first_occ,
        LT_pos(lt)->get_last(), *getBitSetMgr()->create());
    getBitSetMgr()->free(tmp);
}


void LifeTimeMgr::reviseLifeTime(List<LifeTime*> & lts)
{
    LifeTimeVecIter c;
    for (LifeTime * lt = getFirstLifeTime(c);
         lt != nullptr; lt = getNextLifeTime(c)) {
        if (!SR_is_global(LT_sr(lt)) &&
            !SR_is_dedicated(LT_sr(lt)) &&
            LT_pos(lt)->get_first() == LT_FIRST_POS) {
            //Local SR has LT_FIRST_POS use!
            //lt might be assigned register already. Apart from that,
            //there are followed reasons for the situation at present:
            //CASE 1: Local sr that only has USE point. That becasuse Code
            //    Generation Phase might generate redundant SR reference code.
            //    Or the DEF of local sr is conditional execution.
            reviseLTCase1(lt);
        }
    }

    //Remove life times which live-through exit bb neither have any
    //use/def points nor live-in the exit bb.
    //So far, we only found this case in exit bb. Are there any else?
    //For the sake of that, we only check exit bb for speeding up compiling.
    if (m_bb->is_bb_exit()) {
        for (LifeTime * lt = lts.get_head();
             lt != nullptr; lt = lts.get_next()) {
            if (!m_bb->isLiveIn(LT_sr(lt)) && getOccCount(lt) == 0) {
                removeLifeTime(lt);
            }
        }
    }
}


ORId2SR * LifeTimeMgr::getGSRLiveinSpill()
{
    return &m_oridx2sr_livein_gsr_spill_pos;

}


ORId2SR * LifeTimeMgr::getGSRLiveoutReload()
{
    return &m_oridx2sr_liveout_gsr_reload_pos;
}


//Record the first(OR also named 'Prepend Op' in other compiler),
//spill/reload operation for livein/liveout gsr.
//    e.g: 1.t1 <- sp + Large_Num
//         2.[t1] <- gsr2 //It's the focus.
//Operation 2. is the spill operation for gsr2. And similar for reload.
void LifeTimeMgr::setGRALiveinSpill(OR * o, SR * sr)
{
    ASSERTN(m_is_init, ("not initialize"));
    ASSERTN(sr->is_global() && o->is_store(), ("illegal o"));
    m_oridx2sr_livein_gsr_spill_pos.set(o->id(), sr);
}


void LifeTimeMgr::setGRALiveoutReload(OR * o, SR * sr)
{
    ASSERTN(m_is_init, ("not initialize"));
    ASSERTN(sr->is_global() && o->is_load(), ("illegal o"));
    m_oridx2sr_liveout_gsr_reload_pos.set(o->id(), sr);
}


//Recreate life times, but leave some available info for use.
INT LifeTimeMgr::recreate(ORBB * bb, bool is_verify, bool clustering)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    ORId2SR livein_spill, liveout_reload;
    livein_spill.copy(m_oridx2sr_livein_gsr_spill_pos);
    liveout_reload.copy(m_oridx2sr_liveout_gsr_reload_pos);
    destroy();
    init(bb, is_verify, clustering);
    INT num = create();
    m_oridx2sr_livein_gsr_spill_pos.copy(livein_spill);
    m_oridx2sr_liveout_gsr_reload_pos.copy(liveout_reload);
    return num;
}


//Process the function/region returning ORBB.
void LifeTimeMgr::processFuncExitBB(IN OUT List<LifeTime*> & liveout_exitbb_lts,
                                    IN OUT LifeTimeTab & live_lt_list,
                                    INT pos)
{
    RegSet const* regs = tmGetRegSetOfReturnValue();
    for (INT reg = regs->get_first(); reg != -1; reg = regs->get_next(reg)) {
        SR * ret = m_cg->genDedicatedReg(reg);
        ASSERT0(SR_phy_reg(ret) != REG_UNDEF && SR_regfile(ret) != RF_UNDEF);
        LifeTime * lt = m_sr2lt_map.get(ret);
        if (lt == nullptr) {
            lt = allocLifeTime(ret, nullptr);
        }
        live_lt_list.append(lt); //sr lived at current position
        LT_pos(lt)->bunion(pos);
        liveout_exitbb_lts.append_tail(lt);
        if (m_cg->isGRAEnable()) {
            setGRAUsedReg(ret);
            m_bb->setLiveOut(ret);
        }
    }
}


//Process the live out sr.
void LifeTimeMgr::processLiveOutSR(OUT LifeTimeTab & live_lt_list, INT pos)
{
     for (INT id = ORBB_liveout(m_bb).get_first();
         id != -1; id = ORBB_liveout(m_bb).get_next(id)) {
        SR * sr = m_cg->mapSymbolReg2SR(id);
        if (sr->is_reg() && sr->getPhyReg() != REG_UNDEF) {
            LifeTime * lt = (LifeTime*)m_sr2lt_map.get(sr);
            if (lt == nullptr) {
                lt = allocLifeTime(sr, nullptr);
            }
            setGRAUsedReg(sr);
            live_lt_list.append(lt); //sr lived at current position
        }
    }

    //Keep tracking of live points for each lived SR.
    LifeTimeTabIter c;
    for (LifeTime * lt = live_lt_list.get_first(c);
         lt != nullptr; lt = live_lt_list.get_next(c)) {
        LT_pos(lt)->bunion(pos);
    }
}


//Process the live in sr.
void LifeTimeMgr::processLiveInSR(IN OUT LifeTimeTab & live_lt_list)
{
    //Live in gsr
    for (INT id = ORBB_livein(m_bb).get_first();
         id != -1; id = ORBB_livein(m_bb).get_next(id)) {
        SR * sr = m_cg->mapSymbolReg2SR(id);
        if (sr->is_reg() && sr->getPhyReg() != REG_UNDEF) {
            LifeTime * lt = (LifeTime*)m_sr2lt_map.get(sr);
            if (lt == nullptr) {
                lt = allocLifeTime(sr, nullptr);
            }
            setGRAUsedReg(sr);
            //PosInfo * pi = (PosInfo*)xmalloc(sizeof(PosInfo));
            //POSINFO_is_def(pi) = false;
            //LT_desc(lt).set(pos, pi);
            live_lt_list.append(lt); //sr lived at current position
        }
    }
}


void LifeTimeMgr::appendPosition(IN OUT LifeTimeTab & live_lt_list, INT pos)
{
    //Keep the track of live-point for the remainder live srs.
    LifeTimeTabIter c;
    for (LifeTime * lt = live_lt_list.get_first(c);
         lt != nullptr; lt = live_lt_list.get_next(c)) {
        LT_pos(lt)->bunion(pos);
    }
}


//Life times which got same physical register with 'sr' must
//record the current occurrence.
void LifeTimeMgr::recordPhysicalRegOcc(IN SR * sr,
                                       IN LifeTimeTab & live_lt_list,
                                       INT pos,
                                       IN PosInfo * pi)
{
    if (sr->getPhyReg() == REG_UNDEF) { return; }

    //Record the occurrence before the lived life-time be
    //removed out of 'live_lt_list'.
    LifeTimeTabIter c;
    for (LifeTime * lived = live_lt_list.get_first(c);
         lived; lived = live_lt_list.get_next(c)) {
        if (SR_phy_reg(LT_sr(lived)) != REG_UNDEF &&
            SR_regfile(LT_sr(lived)) == sr->getRegFile() &&
            SR_phy_reg(LT_sr(lived)) == sr->getPhyReg()) {
            //Life time of sr died at current position
            //for a while.
            LT_desc(lived).set(pos, pi);
            LT_pos(lived)->bunion(pos);
        }
    }
}


//Regard live in sr as first point
//Regard live out sr as as last point
//Return the number of life times
//NOTICE:
//  1.Live in sr would be regarded as first point
//    Live out sr would be regarded as last point.
INT LifeTimeMgr::create()
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    if (ORBB_ornum(m_bb) <= 0) {
        return 0;
    }

    //Add two point for live in exposed use and live out exposed use.
    m_max_lt_len = ORBB_ornum(m_bb) * 2 + 2;

    //Used to hold lived SR.
    LifeTimeTab live_lt_list;
    ASSERT0(m_bb);

    INT pos = 0;

    //Recording life times which living out of the function exit-ORBB.
    List<LifeTime*> liveout_exitbb_lts;

    //Looking up for exposing live-out use.
    if (m_cg->isGRAEnable()) {
        pos = m_max_lt_len - 1;
        processLiveOutSR(live_lt_list, pos);
    }

    //Keep USE point of special register in exit ORBB.
    if (m_bb->is_bb_exit()) {
        //Append lt of each return value of register.
        pos = m_max_lt_len - 1;
        processFuncExitBB(liveout_exitbb_lts, live_lt_list, pos);
    }

    //Bottom up scanning each sides of OR to build life time.
    OR * o;
    ORCt * ct;
    LifeTimeTabIter c;
    LifeTimeTabIter c2;
    LifeTimeTabIter cc;
    for (pos = m_max_lt_len - 2, o = ORBB_orlist(m_bb)->get_tail(&ct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_prev(&ct), pos--) {
        m_pos2or_map.set(pos, o); //Map 'pos'ition to relative or
        m_or2pos_map.set(o->id(), pos); //Map 'pos'ition to relative or

        //Keep the track of live points at DEF for each live sr
        c.clean();
        for (LifeTime * lt = live_lt_list.get_first(c);
             lt != nullptr; lt =  live_lt_list.get_next(c)) {
            LT_pos(lt)->bunion(pos);
        }

        //Must-DEF point terminates current life time.
        //But May-DEF is not!
        SR * pd = o->get_pred();
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (!sr->is_reg()) { continue; }

            LifeTime * lt2 = (LifeTime*)m_sr2lt_map.get(sr);
            if (lt2 == nullptr) {
                lt2 = allocLifeTime(sr, o);
            }

            computeLifeTimeCluster(lt2, o);
            PosInfo * pi = (PosInfo*)xmalloc(sizeof(PosInfo));
            POSINFO_is_def(pi) = true;
            LT_desc(lt2).set(pos, pi);
            LT_pos(lt2)->bunion(pos);
            recordPhysicalRegOcc(sr, live_lt_list, pos, pi);

            //Life time problem should be processed else where.
            if (pd == nullptr || pd == m_cg->getTruePred()
                //|| SR_phy_reg(pd) == REG_UNDEF
                ) {
                //Life time of SR died at current position for a while.
                live_lt_list.remove(lt2);

                //phy register definition.
                if (sr->getPhyReg() != REG_UNDEF) {
                    LifeTime * next = nullptr;
                    c2.clean();
                    for (LifeTime * lived = live_lt_list.get_first(c2);
                         lived != nullptr; lived = next) {
                        next = live_lt_list.get_next(c2);
                        if (SR_phy_reg(LT_sr(lived)) != REG_UNDEF &&
                            m_cg->isSREqual(sr, LT_sr(lived))) {
                            //Life time of sr died at current position
                            //for a while.
                            live_lt_list.remove(lived);
                        }
                    }
                }
            } else {
                //pd != m_cg->getTruePred(), lt2 is may defined.
                LT_has_may_def(lt2) = true;
            }
        }

        //Handle USE point
        pos--;
        m_pos2or_map.set(pos, o); //Map 'pos'ition to relative or
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (!sr->is_reg()) { continue; }

            LifeTime * lt = (LifeTime*)m_sr2lt_map.get(sr);
            if (lt == nullptr) {
                lt = allocLifeTime(sr, o);
            }

            computeLifeTimeCluster(lt, o);
            PosInfo * pi = (PosInfo*)xmalloc(sizeof(PosInfo));
            POSINFO_is_def(pi) = false;
            LT_desc(lt).set(pos, pi);
            recordPhysicalRegOcc(sr, live_lt_list, pos, pi);
            live_lt_list.append(lt); //sr lived at current position

            if (pd != m_cg->getTruePred()) {
                LT_has_may_use(lt) = true;
            }
        }

        //Keep the track of live points at USE for each live sr
        cc.clean();
        for (LifeTime * lt = live_lt_list.get_first(cc);
             lt != nullptr; lt = live_lt_list.get_next(cc)) {
            LT_pos(lt)->bunion(pos);
        }

        //Handle sibling life time's preference register.
        handlePreferredReg(o);
    }

    ASSERTN(pos == 0, ("First live position number must be zero"));
    //Looking up for exposing live-in sr.
    if (m_cg->isGRAEnable()) {
        processLiveInSR(live_lt_list);
    }

    //Append the FIRST_POS to complete all remainder life times.
    appendPosition(live_lt_list, pos);

    #ifdef _DEBUG_
    //Life time verification.
    LifeTimeVecIter c4;
    for (LifeTime * lt = getFirstLifeTime(c4);
         lt != nullptr; lt = getNextLifeTime(c4)) {
        if (!SR_is_global(LT_sr(lt)) &&
            !SR_is_dedicated(LT_sr(lt)) &&
            !LT_has_allocated(lt)) {
            //For the sake of the weak implementation of Code Expansion Phase,
            //do not check the existence of the first def-point for local SR,
            //even if it does not have in some case. Because, Code Expansion
            //Phase might generate redundant SR reference.
            //While lt's SR has been assigned a physical register, the life
            //time should be able to represent that register.
            //ASSERTN(LT_pos(lt)->get_first() > LT_FIRST_POS,
            //    ("Local life time has not live in point"));
            ASSERTN(LT_pos(lt)->get_first() < (INT)(getMaxLifeTimeLen() - 1),
                    ("Local life time has not live in point"));
        }
    }
    #endif
    reviseLifeTime(liveout_exitbb_lts);
    return getLiftTimeCount();
}


//Enumerate and collect function units that 'lt' traversed.
void LifeTimeMgr::enumTraversedUnits(LifeTime const* lt, OUT UnitSet & us)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    //Check all instructions corresponded with life time's postition.
    for (INT i = LT_pos(lt)->get_first();
         i >= 0; i = LT_desc(lt).get_next(i)) {
        if (LT_desc(lt).get(i)) {
            OR * o = m_pos2or_map.get(i);
            ASSERTN(o, ("Position:%d has not any o mapped",i));

            UINT unit;
            if (o->is_bus()) {
                //Fake operations may cross multiple function units.
                //Some SR might be belong to BUS unit, such as
                //predicate register.
                unit = m_cg->mapSR2Unit(o, LT_sr(lt));
            } else {
                unit = m_cg->computeORUnit(o)->checkAndGet();
            }

            us.bunion(unit);
        }
    }
}


//Check all occurrence of SR's lifetime, and retrue if all of them
//are recalculable.
bool LifeTimeMgr::isRecalcSR(SR * sr)
{
    LifeTime * lt = m_sr2lt_map.get(sr);
    ASSERT0(lt);
    for (INT i = LT_pos(lt)->get_first(); i >= 0; i = LT_desc(lt).get_next(i)) {
        if (LT_desc(lt).get(i)) {
            OR * o = m_pos2or_map.get(i);
            ASSERTN(o, ("Position:%d has not any o mapped",i));
            if (!m_cg->isRecalcOR(o)) {
                return false;
            }
        }
    } // end for
    return true;
}


//Return true if operation 'ortype' passed through by 'lt',
//otherwise return false.
//'orlst': operations which has the 'ortype'.
bool LifeTimeMgr::isContainOR(IN LifeTime * lt, OR_TYPE ortype,
                              OUT List<OR*> * orlst)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));

    //Traversing
    bool find = false;
    for (INT i = LT_pos(lt)->get_first(); i >= 0; i = LT_desc(lt).get_next(i)) {
        if (LT_desc(lt).get(i)) {
            OR * o = m_pos2or_map.get(i);
            ASSERTN(o, ("Position:%d has not any o mapped",i));
            if (o->getCode() == ortype) {
                find = true;
                if (orlst) {
                    orlst->append_tail(o);
                }
            }
        }
    }
    return find;
}


RegSet * LifeTimeMgr::getUsableRegSet(LifeTime * lt) const
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    RegSet * rs = const_cast<LifeTimeMgr*>(this)->
        m_lt2usable_reg_set_map.get(lt);
    ASSERT0(rs);
    return rs;
}


//Append anticipated register
void LifeTimeMgr::addAnticiReg(LifeTime const* lt, REG reg)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    RegSet * rs = m_lt2antici_reg_set_map.get(lt);
    if (rs == nullptr) {
        rs = m_cg->allocRegSet();
        m_lt2antici_reg_set_map.set(lt, rs);
    }
    if (reg != REG_UNDEF) {
        rs->bunion(reg);
    }
}


//Reset anticipated register set
void LifeTimeMgr::setAnticiReg(LifeTime const* lt, RegSet * rset)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    RegSet * rs = m_lt2antici_reg_set_map.get(lt);
    if (rs == nullptr) {
        rs = m_cg->allocRegSet();
        m_lt2antici_reg_set_map.set(lt, rs);
    }
    if (rset != nullptr) {
        rs->copy(*rset);
    } else {
        rs->clean();
    }
}


RegSet * LifeTimeMgr::getAnticiRegs(LifeTime * lt)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    RegSet * rs = m_lt2antici_reg_set_map.get(lt);
    if (rs == nullptr) {
        rs = m_cg->allocRegSet();
        m_lt2antici_reg_set_map.set(lt, rs);
    }
    return rs;
}


//Computes register sets which can be allocated for every life times.
void LifeTimeMgr::computeUsableRegs()
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    LifeTimeVecIter c;
    for (LifeTime * lt = getFirstLifeTime(c);
         lt != nullptr; lt = getNextLifeTime(c)) {
        setAnticiReg(lt, nullptr); //Clean anticipate set.
        RegSet * rs = m_lt2usable_reg_set_map.get(lt);
        if (rs == nullptr) {
            rs = m_cg->allocRegSet();
            m_lt2usable_reg_set_map.set(lt, rs);
        }
        computeLifeTimeUsableRegs(lt, rs);
    }
}


//Recompute usable register-set of life time.
void LifeTimeMgr::recomputeLTUsableRegs(LifeTime const* lt, RegSet * usable_rs)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    SR * sr  = LT_sr(lt);
    REGFILE rf = sr->getRegFile();
    ASSERT0(rf >= RF_UNDEF && rf < RF_NUM);
    ASSERT0(usable_rs);

    //Machine register information should have been initialized.
    usable_rs->copy(*tmMapCluster2RegSetAlloable(LT_cluster(lt)));
    pickOutUnusableRegs(lt, *usable_rs);

    //If GRA enabled, callee saves registers reserved for GRA use.
    //Sometimes GRA has occupied almost all registers, whereas LRA need more.
    //if (m_cg->isGRAEnable()) {
    //    usable_regs = RegSet_Difference(usable_regs, getGRAUsedReg());
    //}

    //So far yet, there are not any registers can be used!
    //if (usable_regs->is_empty()) {
    //    return usable_regs;
    //}

    //Go through each occurrences at life time to look up if
    //some registers can be used.
    for (INT occ = LT_pos(lt)->get_first();
         occ >= 0; occ = LT_desc(lt).get_next(occ)) {
        PosInfo * pi = LT_desc(lt).get(occ);

        //CASE: Sometime asm-o clobbers phy-registers
        //implicitly. Check them at point.
        OR * asm_or = getOR(occ); //occ may be 0
        if (asm_or && OR_is_asm(asm_or)) {
            ASSERTN(0, ("XOC does not support inline-assembly"));
            //Deduct clobber registers from 'usable_regs'
            //ORAsmInfo * asm_info = m_cg->getAsmInfo(asm_or);

            //Cannot use registers clobbered by an ASM statement
            //if (asm_info) {
            //    usable_regs->diff(*asm_info->get_clobber_set());
            //}
        }

        if (pi != nullptr) {
            OR * o = m_pos2or_map.get(occ);
            ASSERTN(o, ("'pos' to 'o' mapping is illegal, not any content!"));
            considerSpecialConstraints(o, sr, *usable_rs);

            //Deduct clobbered registers from 'usable_regs'.
            if (pi->is_def()) { //Deal with reuslts
                for (UINT i = 0; i < o->result_num(); i++) {
                    SR * res = o->get_result(i);
                    ASSERT0(res);
                    if (SR_regfile(res) != rf || rf == RF_UNDEF) {
                        continue;
                    }
                    if (res != sr) {
                        continue;
                    }
                    RegSet const* rs = tmMapRegFile2RegSet(SR_regfile(res));
                    ASSERT0(rs);
                    usable_rs->intersect(*rs);
                }

                if (m_cg->isCopyOR(o)) {
                    //Reserve anticipated register for allocation.
                    UINT opndidx = m_cg->computeCopyOpndIdx(o);
                    SR * copy_src = o->get_opnd(opndidx);
                    ASSERT0(copy_src);
                    if (copy_src->is_reg() &&
                        SR_regfile(copy_src) == sr->getRegFile() &&
                        sr->getRegFile() != RF_UNDEF) {
                        ASSERTN(copy_src->is_reg(),
                               ("invalid copy operation, "
                                "operand is not a register"));
                        addAnticiReg(lt, SR_phy_reg(copy_src));
                    }
                }
            } else { //Deal with opnds
                for (UINT i = 0; i < o->opnd_num(); i++) {
                    if (o->get_opnd(i) != sr) {
                        continue;
                    }
                    RegSet const* rs = m_cg->getValidRegSet(
                        o->getCode(), i, false);
                    ASSERT0(rs);
                    usable_rs->intersect(*rs);
                }

                if (m_cg->isCopyOR(o)) {
                    //Reserve anticipated register for allocation.
                    SR * copy_tgt = o->get_copy_to();
                    if (copy_tgt->is_reg() &&
                        SR_regfile(copy_tgt) == sr->getRegFile() &&
                        sr->getRegFile() != RF_UNDEF) {
                        ASSERTN(copy_tgt->is_reg(),
                            ("invalid copy operation, "
                             "operand is not a register"));
                        addAnticiReg(lt, SR_phy_reg(copy_tgt));
                    }
                }
            }
        }
    }
}


//Compute the register set that satisfies target-specific constraints
//for the use of 'sr' within its live range 'lt'.
//There are two kinds of constraints:
// 1) If 'sr' of 'lt' is used as an operand or a result from a particular
//     register subclass, then it must be assigned a register that
//     belongs to that subclass.
//
// 2) If 'sr' of 'lt' is used in an OR that requires a specific register,
//     then shinks usable register set of 'sr' to the set only contains that
//     special register.
void LifeTimeMgr::computeLifeTimeUsableRegs(LifeTime * lt, RegSet * usable_rs)
{
    ASSERTN(m_is_init, ("Life time manager should initialized first."));
    ASSERT0(usable_rs);
    if (LT_has_allocated(lt)) {
        ASSERT0(SR_phy_reg(LT_sr(lt)) <= REG_LAST);
        usable_rs->bunion(SR_phy_reg(LT_sr(lt)));
        return;
    }
    recomputeLTUsableRegs(lt, usable_rs);
}



void LifeTimeMgr::dump(UINT flag)
{
    #undef INF_LT_NAME
    #define INF_LT_NAME "zmgr.tmp"
    UNLINK(INF_LT_NAME);
    FILE * h = fopen(INF_LT_NAME, "a+");
    ASSERTN(h, ("%s create failed!!!", INF_LT_NAME));

    fprintf(h, "\n==---- DUMP BB%d LOCAL LIFE TIME ----==", m_bb->id());

    //Print gra used registers
    fprintf(h, "\nGRA used registers: ");
    m_gra_used.dump_rs(h);

    //Print live-in SR.
    fprintf(h, "\nlivein SR: ");
    INT i;
    StrBuf buf(64);
    for (i = ORBB_livein(m_bb).get_first();
         i != -1; i = ORBB_livein(m_bb).get_next(i)) {
        SR * sr = m_cg->mapSymbolReg2SR(i);
        ASSERT0(sr != nullptr);
        buf.clean();
        fprintf(h, "%s, ", sr->get_name(buf, m_cg));
    }

    //Print live-out SR.
    fprintf(h, "\nliveout SR: ");
    for (i = ORBB_liveout(m_bb).get_first();
         i != -1; i = ORBB_liveout(m_bb).get_next(i)) {
        SR * sr = m_cg->mapSymbolReg2SR(i);
        ASSERT0(sr);
        buf.clean();
        fprintf(h, "%s, ", sr->get_name(buf, m_cg));
    }

    //Print all local life times.
    LifeTime * lt1;
    DefMiscBitSetMgr sm;
    DefSBitSet visited(sm.getSegMgr());
    INT maxlen = getMaxLifeTimeLen();
    UnitSet us;
    //fprintf(h, "\nList of all local lifetimes:");
    LifeTimeVecIter c;
    for (lt1 = getFirstLifeTime(c); lt1 != nullptr; lt1 = getNextLifeTime(c)) {
        fprintf(h, "\n\t\tLT(%d):", lt1->id());
        buf.clean();
        fprintf(h, "[%s]:", LT_sr(lt1)->get_name(buf, m_cg));
        fprintf(h, "%s", tmGetClusterName(LT_cluster(lt1)));
    }

    for (INT regfile = RF_UNDEF + 1; regfile < RF_NUM; regfile++) {
        fprintf(h, "\n\nREG_FILE:%s", tmGetRegFileName((REGFILE)regfile));
        fprintf(h, "\n  POS:");
        LifeTimeVecIter c2;
        for (lt1 = getFirstLifeTime(c2);
             lt1 != nullptr; lt1 = getNextLifeTime(c2)) {
            if (SR_regfile(LT_sr(lt1)) != regfile) {
                continue;
            }
            visited.bunion(lt1->id());
            fprintf(h, "\n    LT(%3d):", lt1->id());
            //Collects position info
            CHAR * pos_marker = (CHAR*)malloc(maxlen);
            ::memset(pos_marker, 0, sizeof(CHAR) * maxlen);
            INT elt;
            for (elt = LT_pos(lt1)->get_first();
                 elt >= 0; elt = LT_pos(lt1)->get_next(elt)) {
                ASSERTN(elt < maxlen, ("Need bigger space"));
                pos_marker[elt] = 1;
            }
            for (elt = 0; elt < maxlen; elt++) {
                if (pos_marker[elt] == 0) {
                    fprintf(h, "   ,");
                } else {
                    fprintf(h, "%3d,", elt);
                }
            }
            free(pos_marker);
            buf.clean();
            fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
        }

        fprintf(h, "\n\n  DESC:");
        for (lt1 = getFirstLifeTime(c2);
             lt1 != nullptr; lt1 = getNextLifeTime(c2)) {
            if (SR_regfile(LT_sr(lt1)) != regfile) {
                continue;
            }
            fprintf(h, "\n    LT(%3d):", lt1->id());
            //Collects position info

            INT last_idx = LT_desc(lt1).get_last_idx();
            DUMMYUSE(last_idx);

            ASSERTN(last_idx == -1 || last_idx < maxlen,
                ("Depiction of life time long than the finial position"));

            for (INT i2 = LT_FIRST_POS; i2 < maxlen; i2++) {
                PosInfo * pi;
                if ((pi = LT_desc(lt1).get(i2)) != nullptr) {
                    if (pi->is_def()) {
                        fprintf(h, "DEF,");
                    } else {
                        fprintf(h, "USE,");
                    }
                } else {
                    fprintf(h, "   ,");
                }
            }

            buf.clean();
            fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
        }

        if (HAVE_FLAG(flag, DUMP_LT_CLUST)) {
            fprintf(h, "\n\n  CLUST:");
            LifeTimeVecIter c3;
            for (lt1 = getFirstLifeTime(c3);
                 lt1 != nullptr; lt1 = getNextLifeTime(c3)) {
                if (SR_regfile(LT_sr(lt1)) != regfile) {
                    continue;
                }
                fprintf(h, "\n    LT(%3d):", lt1->id());
                fprintf(h, " %s ", tmGetClusterName(LT_cluster(lt1)));

                buf.clean();
                fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
            }
        }

        if (HAVE_FLAG(flag, DUMP_LT_FUNC_UNIT)) {
            fprintf(h, "\n\n  FUNC UNIT:");
            LifeTimeVecIter c3;
            for (lt1 = getFirstLifeTime(c3);
                 lt1 != nullptr; lt1 = getNextLifeTime(c3)) {
                if (SR_regfile(LT_sr(lt1)) != regfile) {
                    continue;
                }
                fprintf(h, "\n    LT(%3d):", lt1->id());
                us.clean();
                enumTraversedUnits(lt1, us);
                for (INT u = UNIT_UNDEF + 1; u < UNIT_NUM; u++) {
                    if (us.is_contain(u)) {
                        fprintf(h, " %s ", tmGetUnitName((UNIT)u));
                    }
                }

                buf.clean();
                fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
            }
        }

        if (HAVE_FLAG(flag, DUMP_LT_USABLE_REG)) {
            fprintf(h, "\n\n  USABLE REGISTERS:");
            LifeTimeVecIter c3;
            for (lt1 = getFirstLifeTime(c3);
                 lt1 != nullptr; lt1 = getNextLifeTime(c3)) {
                if (SR_regfile(LT_sr(lt1)) != regfile) {
                    continue;
                }
                fprintf(h, "\n    LT(%3d):", lt1->id());
                RegSet * rs = getUsableRegSet(lt1);
                if (rs == nullptr) {
                    fprintf(h, "No usable regs");
                } else {
                    rs->dump_rs(h);
                }

                buf.clean();
                fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
            }
        }
    }

    //Print unallocated life time.
    fprintf(h, "\n\nUnallocated life time:");
    for (lt1 = getFirstLifeTime(c); lt1 != nullptr; lt1 = getNextLifeTime(c)) {
        if (visited.is_contain(lt1->id())) { continue; }

        fprintf(h, "\n\nREG_FILE:%s", tmGetRegFileName(SR_regfile(LT_sr(lt1))));
        fprintf(h, "\n  POS:");
        fprintf(h, "\n    LT(%3d):", lt1->id());
        //Collects position info
        CHAR * pos_marker = (CHAR*)malloc(maxlen);
        ::memset(pos_marker, 0, sizeof(CHAR) * maxlen);
        INT elt;
        for (elt = LT_pos(lt1)->get_first();
             elt >= 0; elt = LT_pos(lt1)->get_next(elt)) {
            ASSERTN(elt < maxlen, ("Need bigger space"));
            pos_marker[elt] = 1;
        }

        for (elt = 0; elt < maxlen; elt++) {
            if (pos_marker[elt] == 0) {
                fprintf(h, "   ,");
            } else {
                fprintf(h, "%3d,", elt);
            }
        }
        free(pos_marker);
        buf.clean();
        fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));

        //Print descriptor
        fprintf(h, "\n  DESC:");
        fprintf(h, "\n    LT(%3d):", lt1->id());
        //Collects position info
        PosInfo * pi;
        ASSERTN(LT_desc(lt1).get_last_idx() == -1 ||
                LT_desc(lt1).get_last_idx() < maxlen,
                ("Depiction of life time long than the finial position"));
        for (INT i2 = LT_FIRST_POS; i2 < maxlen; i2++) {
            if ((pi = LT_desc(lt1).get(i2)) != nullptr) {
                if (pi->is_def()) {
                    fprintf(h, "DEF,");
                } else {
                    fprintf(h, "USE,");
                }
            } else {
                fprintf(h, "   ,");
            }
        }

        if (HAVE_FLAG(flag, DUMP_LT_CLUST)) {
            //Print cluster
            fprintf(h, "\n  CLUST:");
            fprintf(h, "\n    LT(%3d):", lt1->id());
            fprintf(h, " %s ", tmGetClusterName(LT_cluster(lt1)));

            buf.clean();
            fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
        }

        if (HAVE_FLAG(flag, DUMP_LT_FUNC_UNIT)) {
            //Print function unit
            fprintf(h, "\n  FUNC UNIT:");
            fprintf(h, "\n    LT(%3d):", lt1->id());
            us.clean();
            enumTraversedUnits(lt1, us);
            for (INT u = UNIT_UNDEF + 1; u < UNIT_NUM; u++) {
                if (us.is_contain(u)) {
                    fprintf(h, " %s ", tmGetUnitName((UNIT)u));
                }
            }

            buf.clean();
            fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
        }

        if (HAVE_FLAG(flag, DUMP_LT_USABLE_REG)) {
            //Print usable registers
            fprintf(h, "\n  USABLE REGISTERS:");
            fprintf(h, "\n    LT(%3d):", lt1->id());
            RegSet * rs = getUsableRegSet(lt1);
            if (rs == nullptr) {
                fprintf(h, "No usable regs");
            } else {
                rs->dump_rs(h);
            }

            buf.clean();
            fprintf(h, "    [%s]", LT_sr(lt1)->get_name(buf, m_cg));
        }
    }
    fclose(h);
}


void LifeTimeMgr::considerSpecialConstraints(IN OR *, SR const*,
                                             OUT RegSet & usable_regs)
{
    DUMMYUSE(usable_regs);
}


xcom::BitSetMgr * LifeTimeMgr::getBitSetMgr() const
{
    return m_cg->getBitSetMgr();
}
//END LifeTimeMgr


//
//START GroupMgr
//
void * GroupMgr::xmalloc(INT size)
{
    ASSERTN(m_is_init, ("xcom::Graph must be initialized before clone."));
    ASSERTN(size > 0, ("xmalloc: size less zero!"));
    ASSERTN(m_pool != 0,("need graph pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    if (p == nullptr) { return nullptr; }
    ::memset(p,0,size);
    return p;
}


void GroupMgr::init(ORBB * bb, CG * cg)
{
    if (m_is_init) { return; }
    m_bb = bb;
    m_cg = cg;
    m_groupidx2ors_map.init();
    m_oridx2group_map.init();
    m_pool = smpoolCreate(64, MEM_COMM);
    m_is_init = true;
}


void GroupMgr::destroy()
{
    if (!m_is_init) return;
    for (INT i = 0; i <= m_groupidx2ors_map.get_last_idx(); i++) {
        List<OR*> * orlist = m_groupidx2ors_map.get(i);
        if (orlist == nullptr) {
            continue;
        }
        orlist->destroy();
    }
    m_groupidx2ors_map.destroy();
    m_oridx2group_map.destroy();
    smpoolDelete(m_pool);
    m_is_init = false;
}


INT GroupMgr::get_groups() const
{
    ASSERTN(m_is_init, ("not yet initialized."));
    INT count = 0;
    for (INT i = 0; i <= m_groupidx2ors_map.get_last_idx(); i++) {
        if (m_groupidx2ors_map.get(i) != nullptr) {
            count++;
        }
    }
    return count;
}


List<OR*> * GroupMgr::get_orlist_in_group(INT i)
{
    ASSERTN(i >= 0, ("idx must be positive."));
    ASSERTN(m_is_init, ("not yet initialized."));
    return m_groupidx2ors_map.get(i);
}


void GroupMgr::addORList(ORList & ors, INT group)
{
    ASSERTN(m_is_init, ("not yet initialized."));
    if (ors.get_elem_count() <= 0) { return; }
    ASSERTN(group >= 1, ("Illegal info"));
    List<OR*> * orlist = m_groupidx2ors_map.get(group);
    if (orlist == nullptr) {
        orlist = (List<OR*>*)xmalloc(sizeof(List<OR*>));
        orlist->init();
        m_groupidx2ors_map.set(group, orlist);
    }
    for (OR * o = ors.get_head(); o != nullptr; ors.get_next()) {
        ASSERTN(false == orlist->find(o), ("o grouped already!"));
        orlist->append_tail(o);
        m_oridx2group_map.set(o->id(), group);
    }
}


void GroupMgr::addOR(OR * o, INT group)
{
    ASSERTN(m_is_init, ("not yet initialized."));
    ASSERTN(o && group >= 1, ("Illegal info"));
    List<OR*> * orlist = m_groupidx2ors_map.get(group);
    if (orlist == nullptr) {
        orlist = (List<OR*>*)xmalloc(sizeof(List<OR*>));
        orlist->init();
        m_groupidx2ors_map.set(group, orlist);
    }
    ASSERTN(false == orlist->find(o), ("o grouped already!"));
    orlist->append_tail(o);
    m_oridx2group_map.set(o->id(), group);
}


//Union group 'src' into group 'tgt'.
void GroupMgr::union_group(INT tgt, INT src)
{
    ASSERTN(m_is_init, ("not yet initialized."));
    ASSERTN(src >= 1 && tgt >= 1, ("Illegal info"));
    if (src == tgt) return;
    List<OR*> * src_oplist = m_groupidx2ors_map.get(src);
    List<OR*> * tgt_oplist = m_groupidx2ors_map.get(tgt);
    if (src_oplist == nullptr) return;
    if (tgt_oplist == nullptr) {
        m_groupidx2ors_map.set(tgt, src_oplist);
        for (OR * o = src_oplist->get_head();
             o != nullptr; o = src_oplist->get_next()) {
            m_oridx2group_map.set(o->id(), tgt);
        }
        return;
    }

    for (OR * o = src_oplist->get_head();
         o != nullptr; o = src_oplist->get_next()) {
        m_oridx2group_map.set(o->id(), tgt);
        tgt_oplist->append_tail(o);
    }
    m_groupidx2ors_map.set(src, nullptr);
    src_oplist->destroy();
}


void GroupMgr::dump()
{
    FILE * h = m_cg->getRegion()->getLogMgr()->getFileHandler();
    ASSERT0(h);
    fprintf(h, "\nCur_Func:%s, ORBB:%d, Group Info: ",
            m_cg->getRegion()->getRegionName(), m_bb->id());

    StrBuf buf(64);
    for (INT i = 0; i <= m_groupidx2ors_map.get_last_idx(); i++) {
        fprintf(h, "\n\tGroup(%d):\n", i);
        List<OR*> * orlist = m_groupidx2ors_map.get(i);
        if (orlist == nullptr) { continue; }

        for (OR * o = orlist->get_head();
             o != nullptr; o = orlist->get_next()) {
            fprintf(h, "\t\t");
            buf.clean();
            fprintf(h, "%s", o->dump(buf, m_cg));
        }
    }
    fprintf(h, "\n");
    fflush(h);
}
//END GroupMgr


//
//START LRA
//
LRA::LRA(ORBB * bb, RaMgr * ra_mgr)
{
    ASSERT0(bb && ra_mgr);
    m_bb = bb;

    m_ppm = nullptr;
    m_rg = ra_mgr->getRegion();
    ASSERT0(m_rg);

    m_cg = ra_mgr->getCG();
    ASSERT0(m_cg);

    ASSERT0(m_cg);

    ASSERT0(m_cg->getRegion() == ra_mgr->getRegion());

    m_ramgr = ra_mgr;
    m_mem_pool = smpoolCreate(32, MEM_COMM);
    m_cur_phase = PHASE_INIT;
    m_opt_phase = 0;
}


//Display phase name
void LRA::show_phase(CHAR * phase_name)
{
    if (!g_dump_opt.isDumpRA() || !getRegion()->isLogMgrInit()) { return; }
    note(getRegion(), "\nREGION(%d)%s, ORBB(%d)Len(%d), PHASE:%s",
         m_rg->id(),
         m_rg->getRegionName() != nullptr ? m_rg->getRegionName() : "",
         m_bb->id(), ORBB_ornum(m_bb), phase_name);
    note(getRegion(), "\nREGION(%d)%s, ORBB(%d)Len(%d), PHASE:%s",
         m_rg->id(),
         m_rg->getRegionName() != nullptr ? m_rg->getRegionName() : "",
         m_bb->id(), ORBB_ornum(m_bb), phase_name);
}


//Select register from 'regs', we handle this at the
//policy that the selecting should obviate choose identical
//register with those registers which are other ors at same
//layer with the first occurrence o of 'lt'.
REG LRA::chooseByRegFileGroup(RegSet & regs,
                              LifeTime * lt,
                              LifeTimeMgr & mgr,
                              RegFileGroup * rfg)
{
    if (rfg == nullptr) {
        return REG_UNDEF;
    }
    if (regs.get_elem_count() <= 1) {
        return REG_UNDEF;
    }
    ASSERTN(!LT_has_allocated(lt) &&
           !SR_is_dedicated(LT_sr(lt)) &&
           !SR_is_global(LT_sr(lt)),
           ("Assign register for dedicated"));

    //TODO: Split life time. Here we only process the first OR of 'lt'.
    OR * o = nullptr;
    for (INT i = LT_pos(lt)->get_first();
         i >= 0; i = LT_desc(lt).get_next(i)) {
        PosInfo * pi;
        if ((pi = LT_desc(lt).get(i)) != nullptr) {
            o = mgr.getOR(i);
            break;
        }
    }

    if (o == nullptr) {
        return REG_UNDEF;
    }

    INT cur_layer = rfg->getORGroup(o);
    List<OR*> * ni_oplist = rfg->getORListInGroup(cur_layer);
    if (ni_oplist == nullptr || ni_oplist->get_elem_count() == 0) {
        goto FIN;
    }

    for (OR * ni = ni_oplist->get_head();
         ni != nullptr; ni = ni_oplist->get_next()) {
        for (UINT i = 0; i < ni->result_num(); i++) {
            SR * sr = ni->get_result(i);
            if (!sr->is_reg() || sr->getPhyReg() == REG_UNDEF) {
                continue;
            }
            if (regs.get_elem_count() <= 1) {
                goto FIN;
            }
            regs.diff(sr->getPhyReg());
        }
    }

    for (OR * ni = ni_oplist->get_head();
         ni != nullptr; ni = ni_oplist->get_next()) {
        for (UINT i = 0; i < ni->opnd_num(); i++) {
            SR * sr = ni->get_opnd(i);
            if (!sr->is_reg()) {
                continue;
            }
            if (sr->getPhyReg() == REG_UNDEF) {
                continue;
            }
            if (regs.get_elem_count() <= 1) {
                goto FIN;
            }
            regs.diff(sr->getPhyReg());
        }
    }
FIN:
    return regs.get_first();
}


//Return true if allocation success, otherwise return false
//When register assigned to 'lt', it must be deducted from
//the usable_register_set of all its neighbors.
bool LRA::assignRegister(LifeTime * lt,
                         InterfGraph & ig,
                         LifeTimeMgr & mgr,
                         RegFileGroup * rfg)
{
    SR * sr = LT_sr(lt);

    REGFILE regfile = sr->getRegFile();
    ASSERTN(regfile != RF_UNDEF, ("Regfile undefined."));
    REG reg = REG_UNDEF;
    RegSet * usable = mgr.getUsableRegSet(lt);
    RegSet * antici = mgr.getAnticiRegs(lt);

    //Shrink to register set 'regfile' allowed.
    RegSet const* regfile_usable_reg_set = tmMapRegFile2RegSet(regfile);
    usable->intersect(*regfile_usable_reg_set);
    if (m_ramgr != nullptr && !m_ramgr->canAllocCallee()) {
        usable->diff(*tmGetRegSetOfCalleeSaved());
    }
    if (usable->get_elem_count() == 0) {
        return false;
    }
    List<LifeTime*> ni_list;
    ig.getNeighborList(ni_list, lt);

    //Deduct the used register by neighbors from 'usable'.
    LifeTime * ni = nullptr;
    for (ni = ni_list.get_head(); ni; ni = ni_list.get_next()) {
        if (LT_has_allocated(ni)) {
            usable->diff(SR_phy_reg(LT_sr(ni)));
        }
    }

    if (usable->get_elem_count() == 0) {
        return false;
    }

    //First select preference register.
    REG pref_reg = LT_preferred_reg(lt);
    if (pref_reg != REG_UNDEF) {
        if (usable->is_contain(pref_reg)) {
            reg = pref_reg; //Not any choose.
            goto FIN;
        } else {
            return false;
        }
    }

    //We have been handled(deducted already) the
    //special registers in
    //'LifeTimeMgr::considerSpecialConstraints'.

    //Avoid allocating the registers which are neighbors anticipated.
    for (ni = ni_list.get_head(); ni; ni = ni_list.get_next()) {
        if (!LT_has_allocated(ni)) {
            RegSet * ni_rset = mgr.getUsableRegSet(ni);

            //Avoid select the reg which ni preferable.
            if (LT_preferred_reg(ni) != REG_UNDEF) {
                ni_rset->bunion(LT_preferred_reg(ni));
            }

            //Only aware of those usable registers which more fewer than
            //current lt.
            if (ni_rset->get_elem_count() <
                usable->get_elem_count()) {
                usable->diff(*ni_rset);
            }
            ASSERTN(usable->get_elem_count() > 0,
                    ("At least one available resgister left."));
        }
    }

    ASSERTN(usable->get_elem_count() > 0,
            ("At least one available resgister left."));

    if (antici->is_intersect(*usable)) {
        //Try to allocate register 'lt' anticipated primarily.
        RegSet tmp(*antici);
        tmp.intersect(*usable);
        reg = tmp.get_first();
    } else {
        //Try to allocate caller-saved registers
        RegSet try_caller_regs(*usable);
        try_caller_regs.intersect(*tmGetRegSetOfCallerSaved());
        reg = chooseByRegFileGroup(try_caller_regs, lt, mgr, rfg);
        if (reg != REG_UNDEF) {
            goto FIN;
        }

        reg = try_caller_regs.get_first();
        if ((INT)reg != -1 && reg != REG_UNDEF) {
            goto FIN;
        }

        reg = chooseByRegFileGroup(*usable, lt, mgr, rfg);
        if (reg != REG_UNDEF) {
            goto FIN;
        }

        reg = usable->get_first(); //Gotta use callee-saved.
    }

FIN:
    //We need deal with its sibling.
    RegSet const* usable_rs = tmMapRegFile2RegSetAllocable(regfile);
    if (!checkAndAssignNextSiblingLT(reg, lt, &mgr, usable_rs)) {
        //Current assignment will incur lt's sibling allcation always fail.
        return false;
    }
    if (!checkAndAssignPrevSiblingLT(reg, lt, &mgr, usable_rs)) {
        //Current assignment will incur lt's sibling allcation always fail.
        return false;
    }

    SR_phy_reg(sr) = reg;
    if (m_ramgr != nullptr) {
        m_ramgr->updateCallee(sr->getRegFile(), sr->getPhyReg());
    }

    //Update usable register set information of neighbors those
    //are still unallocated.
    for (ni = ni_list.get_head(); ni; ni = ni_list.get_next()) {
        if (LT_has_allocated(ni)) {
            continue;
        }
        mgr.getUsableRegSet(ni)->diff(reg);
    }
    return true;
}


//Return true if registers of all sibling of lt are continuous and valid.
bool LRA::checkAndAssignNextSiblingLT(REG treg,
                                      LifeTime const* lt,
                                      LifeTimeMgr * mgr,
                                      RegSet const* usable_rs)
{
    SibList * siblist = mgr->getSibMgr()->getNextSibList(
        const_cast<LifeTime*>(lt));
    if (siblist == nullptr) {
        return true;
    }
    for (LifeTime * sib = siblist->get_head();
         sib != nullptr; sib = siblist->get_next()) {
        treg++;
        if (treg > REG_LAST || !usable_rs->is_contain(treg)) {
            //Current assignment will incur lt's sibling allcation always fail.
            return false;
        }
        if (!checkAndAssignNextSiblingLT(treg, sib, mgr, usable_rs)) {
            //Current assignment will incur lt's
            //sibling allcation always fail.
            return false;
        }
        if (LT_has_allocated(sib)) {
            ASSERTN(treg == SR_phy_reg(LT_sr(sib)),
                ("Unmatch register"));
            continue;
        }
        LT_preferred_reg(sib) = treg;
    }
    return true;
}


//Return true if registers of all sibling of lt are continuous and valid.
bool LRA::checkAndAssignPrevSiblingLT(REG treg,
                                      LifeTime const* lt,
                                      LifeTimeMgr * mgr,
                                      RegSet const* usable_rs)
{
    SibList * siblist = mgr->getSibMgr()->getPrevSibList(
        const_cast<LifeTime*>(lt));
    if (siblist == nullptr) {
        return true;
    }
    for (LifeTime * sib = siblist->get_head();
        sib != nullptr; sib = siblist->get_next()) {
        treg--;
        if (treg == REG_UNDEF || !usable_rs->is_contain(treg)) {
            //Current assignment will incur lt's sibling allcation always fail.
            return false;
        }
        if (!checkAndAssignPrevSiblingLT(treg, sib, mgr, usable_rs)) {
            //Current assignment will incur lt's
            //sibling allcation always fail.
            return false;
        }
        if (LT_has_allocated(sib)) {
            ASSERTN(treg == SR_phy_reg(LT_sr(sib)), ("Unmatch register"));
            continue;
        }
        LT_preferred_reg(sib) = treg;
    }
    return true;
}


//Spilling for Def.
//    e.g:sr1 = ...
//
//    after spilling:
//
//    sr1 = ...
//    [spill_var] = sr1
//
//oldsr: def-sr need to spill, it must locate on lhs of def-o
//    at position 'pos'. Note that 'oldsr' may not be same as lt->sr.
//
//NOTICE:
//    Each of spilling code generated are executed unconditionally.
//    Allow to spill unallocated 'lt'.
void LRA::genSpill(LifeTime * lt,
                   SR * oldsr,
                   INT pos,
                   xoc::Var * spill_var,
                   LifeTimeMgr & mgr,
                   bool is_rename,
                   ORList * sors)
{
    ASSERT0(spill_var && VAR_is_spill(spill_var));
    ORList spill_ors;
    if (sors == nullptr) {
        sors = &spill_ors;
    }
    if (SR_is_global(LT_sr(lt))) {
        m_spilled_gsr.append_tail(LT_sr(lt));
    }

    //Prepend store at start pos of BB.
    if (pos == LT_FIRST_POS) {
        m_cg->buildSpill(oldsr, spill_var, m_bb, *sors);
        OR * sw_or = nullptr;
        for (OR * tmp = sors->get_head();
             tmp != nullptr; tmp = sors->get_next()) {
            if (OR_is_store(tmp)) {
                ASSERTN(sw_or == nullptr, ("multi stores in spill?"));
                sw_or = tmp;
                //Do not break! Continuing the loop and assertion
                //if there are two store.
            }
        }

        m_cg->prependSpill(m_bb, *sors); //Generate opidx

        //There is not any generated memory operations for spill sometime.
        //Substituting the recalculation of SR value for the memory operation.
        //It is so called rematerializing.
        if (sw_or != nullptr) {
            mgr.setGRALiveinSpill(sw_or, LT_sr(lt));
        }
        m_spilled_live_in_gsr.append(oldsr);
    } else if (is_rename) {
        OR * o = mgr.getOR(pos);
        ASSERTN(o, ("OR is nullptr"));
        ASSERTN(pos > (INT)LT_FIRST_POS && pos < (INT)LT_LAST_POS,
                ("pos is out of boundary"));
        SR * newsr = nullptr;
        INT opndnum, resnum;
        if (m_cg->isOpndSameWithResult(oldsr, o, &opndnum, &resnum)) {
            //Cannot do renaming. Consequently0
            //result-sr must same as operand-sr.
            newsr = oldsr;
        } else {
            newsr = m_cg->genReg(); //like oldsr
            SR_phy_reg(newsr) = REG_UNDEF;
            SR_regfile(newsr) = SR_regfile(oldsr);

            //Record new spill location in newsr
            SR_spill_var(newsr) = spill_var;
            m_cg->renameResult(o, oldsr, newsr, false);
        }
        m_cg->buildSpill(newsr, spill_var, m_bb, *sors);
        ORBB_orlist(m_bb)->insert_after(*sors, o);
    } else {
        OR * o = mgr.getOR(pos);
        ASSERTN(o, ("OR is nullptr"));
        ASSERTN(pos > (INT)LT_FIRST_POS && pos < (INT)LT_LAST_POS,
                ("pos is out of boundary"));
        if (!oldsr->is_global() &&  !SR_is_dedicated(oldsr)) {
            //Expect to choose a new register.
            SR_phy_reg(oldsr) = REG_UNDEF;
        }
        SR_spill_var(oldsr) = spill_var; //Record new spilling memory.
        m_cg->buildSpill(oldsr, spill_var, m_bb, *sors);
        ORBB_orlist(m_bb)->insert_after(*sors, o);
    }
    m_cg->setCluster(*sors, LT_cluster(lt));
    m_cg->fixCluster(*sors, LT_cluster(lt));
}


//Build new sr that same like 'oldsr' for reloading.
SR * LRA::genNewReloadSR(SR * oldsr, xoc::Var * spill_var)
{
    SR * newsr = nullptr;
    if (oldsr->is_global() || SR_is_dedicated(oldsr)) {
        newsr = oldsr;
    } else {
        //oldsr is local variable.
        newsr = m_cg->genReg(); //like oldsr
        SR_phy_reg(newsr) = REG_UNDEF;
        SR_regfile(newsr) = SR_regfile(oldsr);
        SR_spill_var(newsr) = spill_var;
    }
    ASSERTN(newsr, ("Miss info"));
    return newsr;
}


//Reloading for Use.
//    e.g:... = sr1
//
//    after reloading:
//
//        sr2 = [spill_var]
//        ... = sr2
//Return the new-sr generated.
//
//'ors': if it is NOT nullptr, return the ORs generated.
//
//NOTICE:
//    Each of reloading code generated are executed unconditionally.
SR * LRA::genReload(IN LifeTime * lt,
                    IN SR * oldsr,
                    INT pos,
                    IN xoc::Var * spill_var,
                    IN LifeTimeMgr & mgr,
                    OUT ORList * ors)
{
    ASSERT0(spill_var && VAR_is_spill(spill_var));
    SR * newsr = nullptr;
    ORList spill_ors;
    if (ors == nullptr) {
        ors = &spill_ors;
    }

    //Note: oldsr may not be the original that recored in LT any more if
    //we are spilling all positions in a LT one by one. Because the original
    //SR will be renamed with the spilling process going on.
    //e.g: sr1 is the SR that recorded in lt1.
    //     O1: sr1 = ...
    //     O6: ... = sr1
    //     O9: sr1 = ...
    //Suppose all three sr1 have to be spilled or reloaded.
    //After sr1 in O1 have been spilled, all the followed sr1
    //will be renamed, such as:
    //     O1: sr2 = ...
    //     O2: [x] = sr2
    //     O6: ... = sr2
    //     O9: sr2 = ...
    //Then sr2 is not the original SR recorded in lt1.
    //Here the most useful information is 'pos' and 'spill_var'
    //before they are recomputed.
    //Just generate reload code here and skip the assert.
    //ASSERT0(oldsr == LT_sr(lt));

    if (pos == (INT)LT_LAST_POS) {
        //Append a reload to bottom of bb.
        m_cg->buildReload(oldsr, spill_var, m_bb,    *ors);
        OR * ld_op = nullptr;
        for (OR * tmp = ors->get_head(); tmp != nullptr; tmp = ors->get_next()) {
            if (OR_is_load(tmp)) {
                ASSERTN(ld_op == nullptr, ("multi loads in reload?"));
                ld_op = tmp;
            }
        }
        m_cg->appendReload(m_bb, *ors);

        //Sometimes reload operation is dispensible.
        //Substitute memory operation for recalclate the value.
        //That is so called 'rematerializing'.
        if (ld_op) {
            mgr.setGRALiveoutReload(ld_op, LT_sr(lt));
        }
        m_spilled_live_out_gsr.append(oldsr);
        newsr = oldsr;
    } else { //Reloading and Renaming
        OR * o = mgr.getOR(pos);
        ASSERTN(o, ("OR is nullptr"));
        newsr = genNewReloadSR(oldsr, spill_var);
        INT opndnum, resnum;
        if (m_cg->isOpndSameWithResult(oldsr, o, &opndnum, &resnum)) {
            m_cg->renameResult(o, oldsr, newsr, false);
        }
        m_cg->renameOpnd(o, oldsr, newsr, false);
        m_cg->buildReload(newsr, spill_var, m_bb, *ors);
        ORBB_orlist(m_bb)->insert_before(*ors, o);
    }

    //Do we really have to differentiate the predicate register.
    //CASE: the spilling code of t1 should be same execute condition.
    //    t1    = 10 (p2)
    //    [tmp] = t1 (p2) //spilling code
    //m_cg->setORListWithSamePredicate(spill_ors, o);

    m_cg->setCluster(*ors, LT_cluster(lt));
    m_cg->fixCluster(*ors, LT_cluster(lt));
    return newsr;
}


//Spilling global SR that passed through the basic block.
void LRA::spillPassthroughGSR(LifeTime * lt, LifeTimeMgr & mgr)
{
    ASSERTN(SR_is_global(LT_sr(lt)), ("Spilling local register."));

    //Get temporary memory from stack.
    xoc::Var * spill_var = m_cg->genSpillVar(LT_sr(lt));
    ASSERTN(spill_var, ("Not any spill loc."));

    //Prepend store tor of bb
    genSpill(lt, LT_sr(lt), 0, spill_var, mgr, true, nullptr);

    //Append reload bottom of bb
    genReload(lt, LT_sr(lt), LT_LAST_POS, spill_var, mgr, nullptr);
}


//Insert spilling code at tor of ORBB, and reloading code
//at bottom of ORBB for gsr.
//
//NOTICE:
//    Since all of available positions during life time will
//    be processed. The generate code should be the same
//    predicate register with original DEF o USE o.
//    That will supply the opportunity for following Load/Store optimization.
void LRA::spillGSR(LifeTime * lt, LifeTimeMgr & mgr)
{
    ASSERTN(lt, ("getOccCount:lt is nullptr"));
    //Handling pass through global sr.
    if (mgr.getOccCount(lt) == 0) {
        spillPassthroughGSR(lt, mgr);
        return;
    }

    //Get temporary memory from stack.
    xoc::Var * spill_var = m_cg->genSpillVar(LT_sr(lt));
    ASSERTN(spill_var, ("Not any spill loc."));

    //Prepend spill code top of bb
    if (LT_pos(lt)->is_contain(LT_FIRST_POS)) { //lifetime live in bb
        genSpill(lt, LT_sr(lt), LT_FIRST_POS, spill_var, mgr, true, nullptr);
    }

    //Append reload bottom of bb
    if (LT_pos(lt)->is_contain(LT_LAST_POS)) { //lifetime live out of bb
        genReload(lt, LT_sr(lt), LT_LAST_POS, spill_var, mgr, nullptr);
    }

    INT first_pos = LT_pos(lt)->get_first();

    //for case of the o that has same result with operand.
    SR * same_with_result_sr = nullptr;
    for (INT i = first_pos == LT_FIRST_POS ? LT_FIRST_POS + 1 : first_pos;
         i >= 0; i = LT_desc(lt).get_next(i)) {
        PosInfo * pi;

        //CASE: livein gsr
        // ORBB tor:
        // S0: ...
        // S1: gsr = gsr + 1
        // S2: ...
        //       ...
        // Sn:     = gsr
        //
        // Steps:
        //   1.Create spill_var
        //   2.Insert spill code before tor o of bb, at S0.
        //
        //     3.Insert reload code before S1.
        //   4.Build new-sr like gsr.
        //   5.Rename orig-sr into new-gsr in between S0 and S1.
        //
        //   6.Insert spill code after S1.
        //   7.Build new-sr like gsr.
        //   8.Rename orig-sr into new-gsr in between S1 and S2.
        //
        //   9.Insert reload code after Sn. gsr = [spill_var]
        if ((pi = LT_desc(lt).get(i)) != nullptr) {
            ORList ors;
            if (pi->is_def()) {
                SR * oldsr = LT_sr(lt);
                if (same_with_result_sr != nullptr) {
                    oldsr = same_with_result_sr;
                    same_with_result_sr = nullptr;
                }
                genSpill(lt, oldsr, i, spill_var, mgr, true, &ors);
            } else {
                SR * newsr = genReload(lt, LT_sr(lt), i,
                                       spill_var, mgr, &ors);
                if (m_cg->isOpndSameWithResult(newsr, mgr.getOR(i),
                                               nullptr, nullptr)) {
                    //CASE: same operand with the result sr
                    //sr10841 :- fmacuu_i sr97(p0) sr4452(d12) gsr445(d13) sr10841
                    //    The sr belong to 'lt' is sr10841, after the reloading,
                    //    rhs of this o will be replaced by newsr sr10845,
                    //sr10845 :- fmacuu_i sr97(p0) sr4452(d12) gsr445(d13) sr10845
                    //    Because the lhs of the o has same register with
                    //    the USE-SR that replaced, the same goes for the
                    //    result sr also.
                    //    And more important is that followed spill operation
                    //    for next position of DEF should use this 'newsr',
                    //    since the original sr upon 'lt' has not over there.
                    same_with_result_sr = newsr;
                }
            }
            //CASE: In order to increase optimizing opportunity,
            //    all spill codes of 't1' should be executed under
            //    same condition.
            //    t1 = 10 (p1) //def-o of 't1'
            //    [tmp] = t1 (p1) //spill code
            m_cg->setORListWithSamePredicate(ors, mgr.getOR(i));
        }
    }
}


//Insert spilling code at start of OR list in BB, and reloading code
//at the end.
//NOTICE:
//    Since all available positions during life time will be
//    processed. The generate code should be the same predicate
//    register with original DEF and USE operations.
//    That will supply the opportunity for subsequently
//    Load/Store optimizations.
void LRA::spillLSR(LifeTime * lt, LifeTimeMgr & mgr)
{
    ASSERTN(lt, ("getOccCount:lt is nullptr"));
    ASSERTN(mgr.getOccCount(lt), ("Empty life time."));
    //Get temporary memory from stack.
    xoc::Var * spill_var = m_cg->genSpillVar(LT_sr(lt));
    ASSERTN(spill_var, ("Not any spill loc."));
    ASSERTN(LT_pos(lt)->get_first() != LT_FIRST_POS,
            ("Illegal local sr life time."));

    //for case of the o that has same result with operand.
    SR * same_with_result_sr = nullptr;
    //The first occurrence during life time.
    bool first_occ = true;
    for (INT i = LT_pos(lt)->get_first(); i >= 0;
         i = LT_desc(lt).get_next(i)) {
        PosInfo * pi;
        //CASE:
        //S1: sr = sr + 1
        //S2: ...
        //       ...
        //Sn:     = sr
        //
        //Steps:
        //    1.Create spill_var
        //    2.Insert reload code before S1.
        //    3.Build new-sr like gsr.
        //    4.Rename orig-sr into new-gsr in between S0 and S1.
        //    5.Insert spill code after S1.
        //    6.Build new-sr like gsr.
        //    7.Rename orig-sr into new-gsr in between S1 and S2.
        if ((pi=LT_desc(lt).get(i)) != nullptr) {
            ORList ors;
            if (first_occ) {
                first_occ = false;
                ASSERTN(pi->is_def(),
                        ("First occurrence must be DEF for local life time"));
            }
            if (pi->is_def()) {
                SR *oldsr = LT_sr(lt);
                if (same_with_result_sr != nullptr) {
                    oldsr = same_with_result_sr;
                    same_with_result_sr = nullptr;
                }
                genSpill(lt, oldsr, i, spill_var, mgr, true, &ors);
            } else {
                SR * newsr = genReload(lt, LT_sr(lt), i, spill_var, mgr, &ors);
                if (m_cg->isOpndSameWithResult(newsr, mgr.getOR(i),
                                               nullptr, nullptr)) {
                    //CASE: same operand with the result sr
                    //SR10841 :- fmacuu_i SR97(p0) SR4452(d12) GSR445(d13) SR10841
                    //    The sr belong to 'lt' is SR10841, after the reloading,
                    //    rhs of this o will be replaced by newsr SR10845,
                    //SR10845 :- fmacuu_i SR97(p0) SR4452(d12) GSR445(d13) SR10845
                    //    Because the lhs of the o has same register with the USE-SR
                    //    that replaced, the same goes for the result sr also.
                    //    And more important is that followed spill operation for
                    //    next position of DEF should use this 'newsr',
                    //    since the original sr upon 'lt' has not over there.
                    same_with_result_sr = newsr;
                }
            }

            //CASE: In order to increase optimizing opportunity,
            //  all spill codes of 't1' should be executed under same condition.
            //  t1 = 10 (p1) //def-o of 't1'
            //  [tmp] = t1 (p1) //spill code
            m_cg->setORListWithSamePredicate(ors, mgr.getOR(i));
        }
    }
}


bool LRA::processORSpill(OR * sw,
                         LifeTimeMgr & mgr,
                         List<LifeTime*> & uncolored_list)
{
    ASSERTN(OR_is_store(sw), ("need store"));
    SR * base = sw->get_store_base();
    if (SR_phy_reg(base) == REG_UNDEF) {
        ORCt * ct;
        ORBB_orlist(m_bb)->find(sw, &ct);
        ASSERT0(ct);
        OR * calc_base_or = ORBB_orlist(m_bb)->get_prev(&ct);
        SR * stack_pointer = nullptr;
        SR * lit = nullptr;
        ASSERTN(calc_base_or != nullptr && calc_base_or->get_result(0) == base,
                ("unknown case"));
        if (OR_is_addi(calc_base_or) || OR_is_subi(calc_base_or)) {
            ASSERT0(calc_base_or->opnd_num() == HAS_PREDICATE_REGISTER + 2);
            stack_pointer = calc_base_or->get_opnd(HAS_PREDICATE_REGISTER + 0);
            lit = calc_base_or->get_opnd(HAS_PREDICATE_REGISTER + 1);
            SR_int_imm(lit) = -SR_int_imm(lit);
        } else {
            ASSERTN(0, ("unknown base compute"));
        }
        m_cg->renameResult(calc_base_or, base, stack_pointer, false);
        m_cg->renameOpnd(sw, base, stack_pointer, false);

        SR * pd = calc_base_or->get_pred();
        if (pd != nullptr) {
            ASSERTN(pd == sw->get_pred(), ("illegal predicated sr"));
        }
        ORList ors;
        IOC tmp;
        m_cg->buildAdd(stack_pointer, lit, GENERAL_REGISTER_SIZE, false,
                       ors, &tmp);
        stack_pointer = tmp.get_reg(0);
        ASSERT0(stack_pointer != nullptr);
        if (pd != nullptr) {
            ASSERT0(ors.get_elem_count() == 1);
            ors.get_head()->set_pred(pd, getCG());
        }

        //Do not care about cluster info unless it is available.
        if (HAVE_FLAG(m_cur_phase, PHASE_CA_DONE)) {
            m_cg->setCluster(ors, m_cg->computeORCluster(calc_base_or));
        }
        ORBB_orlist(m_bb)->insert_after(ors, sw);

        //Remove life-time from 'uncolored_list'.
        LifeTime * base_lt = mgr.getLifeTime(base);
        uncolored_list.remove(base_lt);
        return true;
    }
    return false;
}


//If stack variables can not access via SP+IMM, one need insert
//instruction like 'sp = sp + ofst' to extend stack space.
//
//e.g:
//    We are going to spill GSR1, and generating memory store operation like:
//        SW GSR1, [SP + ofst].
//
//    If 'ofst' is larger than the maximum offset 'SW' supported,
//    (On some target, maximum offset is 24bits or smaller number),
//    more ALU operations were needed, such as:
//
//        t1 = SP + ofst
//        SW GSR1 [t1]
//
//    But t1 needs a physical register yet. There will be a dead-lock!
//    We simply handle this situation as followed right now!
//        SP = SP + ofst
//        SW GSR1 [SP]
//        SP = SP - ofst
//TODO: try better method.
bool LRA::reviseORBase(LifeTimeMgr & mgr, List<LifeTime*> & uncolored_list)
{
    ORId2SR * livein_spill = mgr.getGSRLiveinSpill();
    ORId2SRIter it;
    ASSERT0(ORID_UNDEF == 0);
    for (UINT id = livein_spill->get_first(it); id != ORID_UNDEF;
         id = livein_spill->get_next(it)) {
        OR * sw = m_cg->getOR(id);
        ASSERT0(sw);
        if (processORSpill(sw, mgr, uncolored_list)) {
            return true;
        }
    }
    return false;
}


bool LRA::canBeSpilled(LifeTime * lt, LifeTimeMgr & mgr)
{
    DUMMYUSE(mgr);
    SR * sr = LT_sr(lt);
    if (m_cg->isBusSR(sr)) {
        return false;
    }
    if (m_cg->isSP(sr)) {
        return false;
    }

    UINT c = LT_pos(lt)->get_elem_count();
    if (c <= 1) {
        return false;
    }

    if (c == 2) {
        INT pos1 = -1, pos2 = -1;
        for (INT i = LT_pos(lt)->get_first();
            i >= 0; i = LT_desc(lt).get_next(i)) {
            if (LT_desc(lt).get(i)) {
                if (pos1 == -1) {
                    pos1 = i;
                } else if (pos2 == -1) {
                    pos2 = i;
                } else {
                    ASSERTN(0, ("More than 2"));
                }
            }
        }
        ASSERTN(pos1 != -1 && pos2 != -1, ("???"));
        if (pos2 == pos1 + 1) { //We canot benefit from spilling this lifetime.
            return false;
        }
    }

    return true;
}


bool LRA::canBeSpillCandidate(LifeTime * lt, LifeTime * cand)
{
    if (lt == cand) { return true; }
    for (INT i = LT_pos(cand)->get_first();
        i >= 0; i = LT_desc(cand).get_next(i)) {
        if (LT_desc(cand).get(i) != nullptr && LT_desc(lt).is_contain(i)) {
            //e.g: sr1<-sr2, sr3
            //sr3 should not be spill-candidate if spilling sr2.
            return false;
        }
    }
    return true;
}


//Reassign reg file if there are srs without regfile assigned.
void LRA::reassignRegFileForNewSR(IN OUT ClustRegInfo cri[CLUST_NUM],
                                  IN LifeTimeMgr & mgr,
                                  IN DataDepGraph & ddg)
{
    bool doit = false;
    RegFileSet is_regfile_unique;
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        SR * sr = LT_sr(lt);
        ASSERTN(sr->is_reg(), ("sr is not a register"));
        if (LT_has_allocated(lt) || sr->getRegFile() != RF_UNDEF) {
            is_regfile_unique.bunion(SR_sregid(sr));
        }
        if (sr->getRegFile() == RF_UNDEF) {
            doit = true;
        }
    }

    if (doit) {
        //Build Affinity xcom::Graph
        RegFileAffinityGraph * rdg = allocRegFileAffinityGraph();
        rdg->init(m_bb);
        rdg->build(mgr, ddg);
        assignRegFile(cri, is_regfile_unique, mgr, ddg, *rdg);
    }
}


//Rename SR and the match of physical register is neglected.
//Rename the operand and result SR till the end position of LifeTime.
void LRA::renameOpndsFollowedLT(SR * oldsr,
                                SR * newsr,
                                INT start,
                                LifeTime * lt,
                                LifeTimeMgr & mgr)
{
    PosInfo * pi = nullptr;
    for (INT i = start; i >= 0; i = LT_desc(lt).get_next(i)) {
        if ((pi = LT_desc(lt).get(i)) != nullptr) {
            OR * o = mgr.getOR(i);
            m_cg->renameOpnd(o, oldsr, newsr, false);
            m_cg->renameResult(o, oldsr, newsr, false);
        }
    }
}


//Rename opnds in between 'start' and 'end' occurrencens within life time.
void LRA::renameOpndInRange(SR * oldsr,
                            SR * newsr,
                            INT start,
                            INT end,
                            LifeTime * lt,
                            LifeTimeMgr & mgr)
{
    ASSERTN(start <= end, ("Ivalid range"));
    PosInfo * pi = nullptr;
    end = MIN(end, LT_desc(lt).get_last_idx());
    for (INT i = start; i <= end; i = LT_desc(lt).get_next(i)) {
        ASSERTN(i >= 0, ("out of boundary"));
        if ((pi = LT_desc(lt).get(i)) != nullptr) {
            OR * o = mgr.getOR(i);
            if (pi->is_def()) {
                m_cg->renameResult(o, oldsr, newsr, false);
            } else {
                m_cg->renameOpnd(o, oldsr, newsr, false);
            }
        }
    }
}


void LRA::reallocateLifeTime(List<LifeTime*> & prio_list,
                             List<LifeTime*> & uncolored_list,
                             LifeTimeMgr & mgr,
                             DataDepGraph & ddg,
                             RegFileGroup * rfg,
                             InterfGraph & ig,
                             IN OUT ClustRegInfo cri[CLUST_NUM])
{
    //Need to update mgr, ig, ddg, prio_list.
    prio_list.clean();
    uncolored_list.clean();

    if (ORBB_ornum(m_bb) <= 0) {
        mgr.destroy();
        ddg.destroy();
        ig.destroy();
        return;
    }

    show_phase("---At Split():ReAllocate_LifeTime():mgr.recreate");
    mgr.recreate(m_bb, true, true);
    ASSERT0(mgr.verifyLifeTime());
    mgr.computeUsableRegs();
    ig.destroy();
    ig.init(m_bb);
    ig.build(mgr);

    show_phase("---At Split():ReAllocate_LifeTime():ddg.reschedul");
    ddg.destroy();
    ddg.init(m_bb);
    ddg.setParallelPartMgr(m_ppm);
    ddg.build();

    if (rfg != nullptr) {
        //Regfile of each new srs should be allocated right now.
        rfg->recomputeGroup(m_bb);
    }

    reassignRegFileForNewSR(cri, mgr, ddg);
    //Calculate the prioirtys.
    buildPriorityList(prio_list, ig, mgr, ddg);
    computePrioList(prio_list, uncolored_list, ig, mgr, rfg);
}


void LRA::pure_spill(LifeTime * lt, LifeTimeMgr & mgr)
{
    ASSERTN(canBeSpilled(lt, mgr), ("Spilling is forbiddend"));
    SR * sr = LT_sr(lt);
    if (sr->is_global()) {
        spillGSR(lt, mgr);
    } else {
        spillLSR(lt, mgr);
    }
}


void LRA::spill(LifeTime * lt,
                List<LifeTime*> & prio_list,
                List<LifeTime*> & uncolored_list,
                LifeTimeMgr & mgr,
                DataDepGraph & ddg,
                RegFileGroup * rfg,
                InterfGraph & ig,
                REG spill_location,
                Action & action,
                IN OUT ClustRegInfo cri[CLUST_NUM])
{
    DUMMYUSE(spill_location);
    bool has_hole;
    lt = computeBestSpillCand(lt, ig, mgr, true, &has_hole);
    pure_spill(lt, mgr);

    //After spilling...
    //Need to update mgr, ig, ddg, prio_list.
    mgr.recreate(m_bb, true, true);
    ASSERT0(mgr.verifyLifeTime());
    mgr.computeUsableRegs();

    if (isOpt()) { ddg.reschedul(); }

    //In this case, we are not going to rebuild regfile group
    //since the increased instructions were not modified
    //original dependences which inheret.
    //if (rfg != nullptr) {
    //    rfg->recomputeGroup();
    //}

    ig.destroy();
    ig.init(m_bb);
    ig.build(mgr);

    //Calculate the prioirtys.
    prio_list.clean();
    uncolored_list.clean();
    buildPriorityList(prio_list, ig, mgr, ddg);
    reallocateLifeTime(prio_list, uncolored_list,
                       mgr, ddg, rfg, ig, cri);
    for (LifeTime * tmplt = uncolored_list.get_head();
         tmplt != nullptr; tmplt = uncolored_list.get_next()) {
        action.set_action(tmplt, Action::BFS_REASSIGN_REGFILE);
    }
}


//Generate spilling and reloading code at position 'start' and 'end' of life time
//'lt' respectively.
//
//NOTICE:
//    Neglact 'start' if it equals -1, and similar for 'end'.
void LRA::splitLTAt(INT start,
                    INT end,
                    bool is_start_spill,
                    bool is_end_spill,
                    LifeTime * lt,
                    LifeTimeMgr & mgr)
{
    ASSERTN(lt, ("lt is nullptr"));
    SR * sr = LT_sr(lt);
    #ifdef _DEBUG_
    if (!sr->is_global() && !SR_is_dedicated(sr)) {
        ASSERTN(start != LT_FIRST_POS || LT_desc(lt).get(start) != nullptr,
                ("Miss pos-info"));
        ASSERTN(end != (INT)LT_LAST_POS || LT_desc(lt).get(end) != nullptr,
                ("Miss pos-info"));
        ASSERTN(mgr.getOccCount(lt),
                ("Empty life time."));
    }
    #endif
    ORList spill_ors;

    //spill var is gra_spill_var if sr is global reg.
    xoc::Var * spill_var = m_cg->genSpillVar(sr);
    ASSERTN(spill_var, ("Not any spill loc."));
    if (start != -1) {
        if (start == LT_FIRST_POS) {
            genSpill(lt, sr, start, spill_var, mgr, false, nullptr);
        } else {
            ASSERTN(mgr.getOR(start), ("Illegal o mapping."));
            if (is_start_spill) { //Store to memory
                genSpill(lt, sr, start, spill_var, mgr, false, nullptr);
            } else { //Reload from memory
                ASSERTN(0, ("Reload at the start position "
                            "of Hole? Performance Gap!"));
                //genReload(lt, sr, start, SR_spill_var(sr), mgr, nullptr);
            }
        }
    }

    if (end != -1) {
        if (end == (INT)LT_LAST_POS) {
            SR * newsr = genReload(lt, sr, end, spill_var, mgr, nullptr);
            DUMMYUSE(newsr);
            ASSERTN(newsr == sr,
                    ("Should not rename global register, since that "
                     "global information needs update."));
        } else {
            ASSERTN(mgr.getOR(end), ("Illegal o mapping."));
            if (is_end_spill) { //Store to memory
                //I think hereon that operations should be reloading!
                ASSERTN(0,
                    ("Store at the end position of Hole? Performance Gap!"));
                //genSpill(lt, sr, end, spill_var, mgr, false, nullptr);
            } else { //Reload from memory
                SR * newsr = genReload(lt, sr, end, spill_var, mgr, nullptr);
                if (newsr != sr) {
                    //Do renaming.
                    INT forward_def = mgr.getForwardOccForDEF(end, lt);

                    //May be same result as operand.
                    if (forward_def != -1 && forward_def == (end+1)) {
                        OR * occ_or = mgr.getOR(end);
                        ASSERTN(mgr.getOR(forward_def) == occ_or,
                                ("o should be same result and operand."));
                        if (!m_cg->isOpndSameWithResult(nullptr, occ_or,
                                                        nullptr, nullptr)) {
                            //Generate new sr again.
                            newsr = genNewReloadSR(sr, spill_var);
                        }

                        //Rename all follows REFs.
                        forward_def = -1;
                    }
                    if (forward_def != -1) {
                        renameOpndInRange(sr, newsr, end,
                                          forward_def-1, lt, mgr);
                    } else {
                        renameOpndsFollowedLT(sr, newsr, end, lt, mgr);
                    }
                }
            }
        }
    }
}


void LRA::splitOneLT(LifeTime * lt,
                     List<LifeTime*> & prio_list,
                     List<LifeTime*> & uncolored_list,
                     LifeTimeMgr & mgr,
                     InterfGraph & ig,
                     REG spill_location,
                     Action & action)
{
    DUMMYUSE(prio_list);
    DUMMYUSE(uncolored_list);
    DUMMYUSE(ig);
    DUMMYUSE(spill_location);
    DUMMYUSE(action);
    //Spilling for simplicity.
    pure_spill(lt, mgr);
}


//CASE:lt1: D............U
//     lt2:    D......U
bool LRA::splitTwoLTContained(LifeTime * lt1,
                              LifeTime * lt2,
                              LifeTimeMgr & mgr)
{
    //Reload of lt1
    List<INT> occs;
    INT hole_start = LT_pos(lt2)->get_first();
    INT hole_end = LT_pos(lt2)->get_last();

    //Get intersectant part.
    mgr.getOccInRange(hole_start, hole_end, lt1, occs);
    INT pos;
    bool is_def;
    SR * sr = LT_sr(lt1); //original sr
    pos = occs.get_head();
    for (UINT i = 0; i < occs.get_elem_count(); pos = occs.get_next(), i++) {
        if (pos == (INT)LT_FIRST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = true;
        } else if (pos == (INT)LT_LAST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = false;
        } else {
            PosInfo * pi = LT_desc(lt1).get(pos);
            ASSERTN(pi, ("Missing info"));
            if (pi->is_def()) {
                is_def = true;
            } else {
                is_def = false;
            }
        }

        //spill var is gra_spill_var if sr is global reg.
        xoc::Var * spill_var = m_cg->genSpillVar(sr);
        ASSERTN(spill_var, ("Not any spill loc."));
        if (is_def) {
            if (pos != LT_FIRST_POS) {
                ASSERTN(m_cg->mayDef(mgr.getOR(pos), sr),
                        ("Illegal OR mapping."));
            }
            genSpill(lt1, sr, pos, spill_var, mgr, true, nullptr);
        } else {
            SR * newsr = genReload(lt1, sr, pos, spill_var, mgr, nullptr);
            if (newsr != sr) {
                //Do renaming.
                renameOpndsFollowedLT(sr ,newsr, pos, lt1, mgr);
                sr = newsr;
            }
        }
    }

    //Processing remainder occs of lt1

    //lt1: D.Spill...| <----hole----> |....Reload.U
    pos = mgr.getForwardOcc(hole_end, lt1, &is_def);
    //if (pos != -1 && is_def) {
    //    CASE: If o at position is cond-exec, we need to reload
    //    register value correctly.
    //    OR *testdefop = mgr.getOR(pos);
    //    if (testdefop && isCondExecOR(testdefop)) {
    //        is_def = false;
    //    }
    //}
    if (pos != -1 && !is_def) {
        //This may generate redundant reload-code when 'occ' is a DEF.
        //It can removing them under followed-phase(Dead Ld/St Elimi).
        SR * newsr = genReload(lt1, sr, pos, SR_spill_var(sr), mgr, nullptr);
        if (newsr != sr) {
            //Renaming all 'sr' followed to 'newsr'.
            renameOpndsFollowedLT(sr ,newsr, pos, lt1, mgr);
        }
    }

    //Processing lt2.
    //This is a hole with non-zero elements within lt1.
    if (occs.get_elem_count() > 0) {
        //Spilling for simplicity.
        pure_spill(lt2, mgr);
    }
    return true;
}


//CASE:lt1: D............U
//     lt2:    D............U
bool LRA::splitTwoLTCross(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr)
{
    //Generate reload of lt1
    List<INT> occs;
    INT hole_start = LT_pos(lt2)->get_first();
    INT hole_end = LT_pos(lt1)->get_last();

    //Get intersectant part.
    mgr.getOccInRange(hole_start, hole_end, lt1, occs);
    bool is_def;
    SR * sr = LT_sr(lt1);
    for (UINT i = 0, pos = occs.get_head();
         i < occs.get_elem_count();
         pos = occs.get_next(), i++) {
        if (pos == (INT)LT_FIRST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = true;
        } else if (pos == (UINT)LT_LAST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = false;
        } else {
            PosInfo * pi = LT_desc(lt1).get(pos);
            if (pi->is_def()) {
                is_def = true;
            } else {
                is_def = false;
            }
        }

        //spill var is gra_spill_var if sr is global reg.
        xoc::Var * spill_var = m_cg->genSpillVar(sr);
        ASSERTN(spill_var, ("Not any spill loc."));
        if (is_def) {
            if (pos != LT_FIRST_POS) {
                OR * o = mgr.getOR(pos);
                CHECK_DUMMYUSE(o);
                ASSERTN(m_cg->mayDef(o, sr), ("Illegal o mapping."));
            }
            genSpill(lt1, sr, pos, spill_var, mgr, true, nullptr);
        } else {
            SR * newsr = genReload(lt1, sr, pos, spill_var, mgr, nullptr);
            if (newsr != sr) {
                //Do renaming.
                renameOpndsFollowedLT(sr ,newsr, pos, lt1, mgr);
                sr = newsr;
            }
        }
    }

    //Generate reload of lt2
    occs.clean();
    mgr.getOccInRange(hole_start, hole_end, lt2, occs);
    sr = LT_sr(lt2);
    for (UINT i = 0, pos = occs.get_head();
         i < occs.get_elem_count();
         pos = occs.get_next(), i++) {
        if (pos == (INT)LT_FIRST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = true;
        } else if (pos == (UINT)LT_LAST_POS) {
            ASSERTN(sr->is_global(), ("Only global reg permit"));
            is_def = false;
        } else {
            PosInfo * pi = LT_desc(lt2).get(pos);
            if (pi->is_def()) {
                is_def = true;
            } else {
                is_def = false;
            }
        }

        //spill var is gra_spill_var if sr is global reg.
        xoc::Var * spill_var = m_cg->genSpillVar(sr);
        ASSERTN(spill_var, ("Not any spill loc."));
        if (is_def) {
            if (pos != LT_FIRST_POS) {
                OR * o = mgr.getOR(pos);
                CHECK_DUMMYUSE(o);
                ASSERTN(m_cg->mayDef(o, sr), ("Illegal o mapping."));
            }
            //Each of DEFs will be new-sr.
            genSpill(lt2, sr, pos, spill_var, mgr, true, nullptr);
        } else {
            SR * newsr = genReload(lt2, sr, pos, spill_var, mgr, nullptr);
            if (newsr != sr) {
                //Do renaming.
                renameOpndsFollowedLT(sr, newsr, pos, lt2, mgr);
                sr = newsr;
            }
        }
    }

    //Processing remainder occs of lt2
    INT pos = mgr.getForwardOcc(hole_end, lt2, &is_def);
    //if (pos != -1 && is_def) {
    //    CASE: If o under pos is cond-exec, we need to reload
    //    register value correctly.
    //    OR *testdefop = mgr.getOR(pos);
    //    if (testdefop &&
    //        m_cg->isCondExecOR(testdefop)) {
    //        is_def = false;
    //    }
    //}
    if (pos != -1 && !is_def) {
        SR * newsr = genReload(lt2, sr, pos, SR_spill_var(sr), mgr, nullptr);
        if (newsr != LT_sr(lt2)) {
            //renaming.
            renameOpndsFollowedLT(sr, newsr, pos, lt2, mgr);
        }
    }
    return true;
}


//Spill the closest DEF to lt2.
bool LRA::spillFirstDef(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr)
{
    //Spill
    INT defpos = LT_pos(lt1)->get_first();
    do {
        defpos = mgr.getForwardOccForDEF(defpos, lt1);

        //Find the nearest DEF of first-pos of lt2.
        if (!(defpos != -1 && defpos <= LT_pos(lt2)->get_first())) {
            //Need inserts spilling at first pos of life time.
            defpos = LT_pos(lt1)->get_first();
            break;
        }
    } while (defpos > LT_pos(lt2)->get_first());
    ASSERTN(defpos >= (INT)LT_FIRST_POS && defpos < (INT)LT_LAST_POS,
        ("Illegal start position"));

    //Op on 'pos' must be DEF of 'sr'
    if (defpos != LT_FIRST_POS) {
        OR * o = mgr.getOR(defpos);
        CHECK_DUMMYUSE(o);
        ASSERTN(m_cg->mayDef(o, LT_sr(lt1)), ("Illegal o mapping."));
    }
    //spill var is gra_spill_var if sr is global reg.
    xoc::Var * spill_var = m_cg->genSpillVar(LT_sr(lt1));
    ASSERTN(spill_var, ("Not any spill loc."));

    genSpill(lt1, LT_sr(lt1), defpos, spill_var, mgr, false, nullptr);
    return true;
}


//In two case we considered:
//    1. lt1: D............U
//       lt2:    D............U
//    2. lt1: D............U
//       lt2:    D......U
bool LRA::splitTwoLT(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr)
{
    if (LT_pos(lt2)->is_contained_in_range(LT_pos(lt1)->get_first(),
            LT_pos(lt1)->get_last(), true) ||
        LT_pos(lt1)->get_first() > LT_pos(lt2)->get_first()) {
        LifeTime * tmp = lt1;
        lt1 = lt2;
        lt2 = tmp;
    }

    ASSERTN(!(LT_has_allocated(lt1) && LT_has_allocated(lt2)),
        ("Both have allocated"));

    //Spilling is necessary here!!
    //CASE: param.c:BB1
    //    sr12 =
    //    ...
    //      sr13 =
    //      ...
    //           = sr12
    //      ...
    //           = sr13
    //  Spill sr12 is necessary.
    spillFirstDef(lt1, lt2, mgr);

    //Processing reload.
    if (LT_pos(lt1)->is_contained_in_range(LT_pos(lt2)->get_first(),
            LT_pos(lt2)->get_last(), true)) {
        //lt1: D............U
        //lt2:    D......U
        splitTwoLTContained(lt1, lt2, mgr);
    } else if (LT_pos(lt1)->get_first() < LT_pos(lt2)->get_first()) {
        //lt1: D............U
        //lt2:    D............U
        splitTwoLTCross(lt1, lt2, mgr);
    } else {
        ASSERTN(0, ("Unsupport"));
    }
    return true;
}


//Given two position within life time 'lt', tring to choose the most
//appropriate split point and inserting the spill/reload code at them.
//'is_pos1_spill': if true indicate that a spilling is needed at pos1,
//    otherwise to insert a reload.
//'is_pos2_spill': if true indicate that a spilling is needed at pos2,
//    otherwise to insert a reload.
//
//e.g: Given pos1, pos2, both of them are USE.
//    We need to find the DEF to insert the spill code. And choosing
//    the best USE between 'pos1' and 'pos2' to insert reload code.
//    While both positions are useless, we do not insert any code in
//    those positions, and set 'pos1' and 'pos2' to -1.
void LRA::selectReasonableSplitPos(IN OUT INT * pos1,
                                   IN OUT INT * pos2,
                                   IN OUT bool * is_pos1_spill,
                                   IN OUT bool * is_pos2_spill,
                                   LifeTime * lt,
                                   LifeTimeMgr & mgr)
{
    ASSERTN(lt && *pos1 >= 0 && *pos2 > 0 && *pos1 < *pos2,
        ("Illegal position"));
    SR * sr = LT_sr(lt);
    CHECK_DUMMYUSE(sr);

    INT p1 = *pos1, p2 = *pos2;
    bool is_p1_def = false, is_p2_def = false;

    #ifdef _DEBUG_
    if (!sr->is_global() && !SR_is_dedicated(sr)) {
        ASSERTN(mgr.getOccCount(lt), ("Empty life time."));
        ASSERTN(p1 != (INT)LT_FIRST_POS ||
               LT_desc(lt).get(p1) != nullptr, ("Miss pos-info"));
        ASSERTN(p2 != (INT)LT_LAST_POS ||
               LT_desc(lt).get(p2) != nullptr, ("Miss pos-info"));
    }
    #endif
    //Compute the status the pos shows.
    PosInfo * pi = nullptr;
    bool proc = true;
    while (proc) {
        if (p1 == LT_FIRST_POS) {
            is_p1_def = true;
            break;
        } else {
            pi = LT_desc(lt).get(p1);
            if (pi == nullptr) {
                //CASE: image.c:copy_rdopt_data:BB1
                //     live in and out gsr: GSR277
                //     first pos:0
                //     last pos:83
                //     LT(33): 0,,...,28,29,30...83
                //There is a invalid region in between 0~28,and in actually,
                //position 28 has not any PI corresponding to!
                //Since GSR238(a7) also allocate the same register as
                //GSR277 and it has a def at position 28.
                //Lra life time manager handled the situation conservatively.
                //But I thought this is a GRA bug:
                //    See BB5 for more details:
                //        SR284, SR282, SR283, :- seqi_m GSR1294(a6) (0x0)
                //        GSR277(a7)[A1] :- copy_m SR283[P1] SR278
                //        GSR277(a7)[A1] :- copy_m SR282[P1] SR280
                //        j_b (lab:.Lt_22_433) ; cycle:0
                //GSR277 was cond-defined and should be considered as an USE,
                //but data-flow solver cannot distingwish that because of
                //the cond-def.
                p1--;
                *pos1 = p1;
                continue;
            }
            if (pi->is_def()) {
                is_p1_def = true;
            } else {
                is_p1_def = false;
            }
            break;
        }
    }

    proc = true;
    while (proc) {
        if (p2 == (INT)LT_LAST_POS) {
            is_p2_def = false;
            break;
        } else {
            pi = LT_desc(lt).get(p2);
            if (pi == nullptr) {
                p2++;
                *pos2 = p2;
                continue;
            }
            if (pi->is_def()) {
                OR * o = mgr.getOR(p2);
                ASSERT0(o);
                if (m_cg->isCondExecOR(o)) {
                    //CASE: 20020402-3.c:blockvector_for_pc_sect():BB10
                    //    gsr275(a3) lived in and lived out.
                    //    first pos:0
                    //    last pos:12, cond def
                    //
                    //    sr268[A1] :- lw_m sr97(p0)[P1] gsr263(a4)[A1] (0x0)
                    //    sr266(d10)[D2] :- lw_m sr97(p0)[P1] sr268[A1] (0x8)
                    //    sr267(d2)[D1] :- lw_m sr97(p0)[P1] sr268[A1] (0xc)
                    //    ...
                    //    gsr275(a3)[A1] gsr271(p7)[P1] gsr272(p6)[P1] :- sgtu_m sr270(p1)[P1] ...
                    //
                    //The spliting candidate is GSR275.
                    //Although the operator at postition p2 is a DEF,
                    //but it was a conditional DEF! So we regard position p2 as
                    //an USE in order to insert a reloading before the cond DEF and
                    //add a spilling followed the FIRST position to supply the spill
                    //temp memory location.
                    //result code can be:
                    //    FIRST position
                    //    sw_m gsr275(a3)[A1], gra_spill_temp
                    //    ...
                    //    gsr275(a3)[A1] = lw_m gra_spill_temp
                    //    gsr275(a3)[A1] gsr271(p7)[P1] gsr272(p6)[P1] :- sgtu_m sr270(p1)[P1] ...
                    is_p2_def = false;
                } else {
                    is_p2_def = true;
                }
            } else {
                is_p2_def = false;
            }
            break;
        }
    }

    //4 plots
    if (is_p1_def && !is_p2_def) { //def ... use
        *is_pos1_spill = true;
        *is_pos2_spill = false;
        return;
    } else if (is_p1_def && is_p2_def) { //def ... def
        *pos1 = *pos2 = -1;
        return;
    } else if (!is_p1_def) {
        if (is_p2_def) { //use ... def
            //pos2 do not need any reloading.
            *pos2 = -1;
        } else { //use ... use
            *is_pos2_spill = false; //reload before pos2
        }

        //Find the DEF of pos1.
        *pos1 = mgr.getBackwardOccForDEF(p1, lt);
        if (*pos1 != -1) {
            *is_pos1_spill = true;
        } else if (SR_is_dedicated(LT_sr(lt)) ||
                   SR_is_global(LT_sr(lt))) { //Might be live-in lifetime
            *pos1 = p1;
            *is_pos1_spill = true;
        } else {
            ASSERTN(0, ("local use without DEF, dead use?"));
        }
        return;
    }
    ASSERTN(0, ("Should we here?"));
}


//e.g: Try to remove 'o' and replacing 'succ' by copy.
//    [x] = t1 //Might be redundant store
//    t2 = [x]
//=>
//    t2 = t1
bool LRA::mergeRedundantStoreLoad(OR * o,
                                  OR * succ,
                                  ORList & remainder_succs,
                                  ORBB * bb,
                                  xoc::Var const* spill_var,
                                  DataDepGraph & ddg)
{
    ASSERTN(o->is_store() &&
            OR_is_load(succ) &&
            m_cg->computeSpillVar(o) == m_cg->computeSpillVar(succ),
            ("Illegal pattern"));

    if ((!m_cg->isSameCondExec(o, succ, ORBB_orlist(bb)) &&
        
        //'o' and 'succ' must be in same conditional execute path.
        !m_cg->isSafeToOptimize(o, succ)) ||

        !m_cg->isSameCluster(o, succ) ||
        OR_unit(o) != OR_unit(succ)) {
        return false;
    }

    //Check in ORBB whether there are some other load-ors that loads value
    //from same spill location.
    bool or_can_be_removed = true; //Checking for another use
                                   //of spill-memory(reload).

    //'succ' is predecessor of the first element
    //inside remainder ors list.
    for (OR * tmp = remainder_succs.get_next();
         tmp != nullptr;
         tmp = remainder_succs.get_next()) {
        //check e.g:
        //[x] =      //or
        //    = [x]  //succ
        //...
        //    = [x]  //another load
        if (OR_is_load(tmp)) {
            if (m_cg->isSameSpillLoc(spill_var, tmp, o)) {
                or_can_be_removed = false;
            }
        }
    }

    //Checking for global xoc::Var referencing.
    if (m_ramgr != nullptr) {
        RefORBBList * rbl = m_ramgr->getVar2OR()->get(spill_var);
        ASSERT0(m_ramgr->getBBList());
        if ((rbl && rbl->get_elem_count() > 1) ||
             //Only occurrence in one ORBB.
             m_ramgr->getBBList()->get_elem_count() == 1) {

            //There exist reloads at other BBs.
            or_can_be_removed = false;
        }
    } else {
        //For conservative purpose.
        or_can_be_removed = false;
    }

    //'o' does not can be removed if it is the dominator of 'succ'.
    if (!m_cg->isSameCondExec(o, succ, ORBB_orlist(bb))) {
        or_can_be_removed = false;
    }

    //Build copy register operation.
    ORList ors;
    SR * pd = succ->get_pred(); //Must use pred-sr of 'succ'
    ASSERTN(pd, ("No predicated sr"));
    //TODO: Support usage of 'OR_Find_Operand_Use(OR_code(succ), OU_result0)'
    INT result_idx = 0;
    UNIT succ_unit = m_cg->computeORUnit(succ)->checkAndGet();
    CLUST succ_clust = m_cg->computeORCluster(succ);
    m_cg->buildCopy(succ_clust, succ_unit,
                    succ->get_result(result_idx),
                    o->get_first_store_val(), ors);
    ASSERT0(ors.get_elem_count() == 1);
    OR * new_cp = ors.get_tail();
    ASSERT0(new_cp);
    new_cp->set_pred(pd, getCG());

    //Dont aware cluster info before it was available.
    if (HAVE_FLAG(m_cur_phase, PHASE_CA_DONE)) {
        m_cg->setCluster(ors, m_cg->computeORCluster(o));
    }
    ORBB_orlist(m_bb)->insert_after(ors, succ);

    //Union edges of 'o' and 'succ' to 'new_cp'.
    ddg.appendOR(new_cp);
    ORList succ_preds, succ_succs;
    ORList op_preds, op_succs;
    ddg.get_succs(op_succs, o);
    ddg.get_preds(op_preds, o);
    ddg.get_succs(succ_succs, succ);
    ddg.get_preds(succ_preds, succ);
    ddg.unifyEdge(op_succs, new_cp);
    ddg.unifyEdge(op_preds, new_cp);
    ddg.unifyEdge(succ_succs, new_cp);
    ddg.unifyEdge(succ_preds, new_cp);

    if (or_can_be_removed) {
        ddg.chainPredAndSucc(o);
        ORBB_orlist(m_bb)->remove(o);
        ddg.removeOR(o);
    }
    ddg.chainPredAndSucc(succ);
    ORBB_orlist(m_bb)->remove(succ);
    ddg.removeOR(succ);
    return true;
}


//Remove redundant spill/reload code.
bool LRA::removeRedundantStoreLoadAfterLoad(OR * o,
                                            ORList & succs,
                                            ORBB * bb,
                                            xoc::Var const* spill_var,
                                            DataDepGraph & ddg)
{
    bool has_def = false, has_use = false; //record reg DU info.
    bool has_store = false; //record memory DU info.
    bool is_resch = false;
    SR * op_ld_res = o->get_result(0);
    for (OR * succ = succs.get_head(); succ != nullptr; succ = succs.get_next()) {
        if (OR_is_asm(succ) || OR_is_br(succ)) {
            has_use = has_def = true;
            break;
        }

        if (OR_is_store(succ) && m_cg->isSameSpillLoc(spill_var, o, succ)) {
            has_store = true;
            if (!has_def &&
                !OR_is_volatile(succ) &&
                m_cg->isSameCluster(o, succ) &&
                m_cg->isSameCondExec(o, succ, ORBB_orlist(bb))) {

                //CASE 1:
                //t1    = [xxx]
                //...   = t1
                //[xxx] = t1 //Redundant store
                //...
                //      = t1

                SR * succ_use = succ->get_first_store_val();
                if (m_cg->isSREqual(op_ld_res, succ_use)) {
                    ddg.chainPredAndSucc(succ);
                    ORBB_orlist(m_bb)->remove(succ);
                    ddg.removeOR(succ);
                    is_resch = true;
                    return true; //Processing start from scratch
                }
            } //end if
        } else if (OR_is_load(succ) &&
                   m_cg->isSameSpillLoc(spill_var, o, succ) &&
                   !has_def &&
                   has_use &&
                   !has_store &&
                   !OR_is_volatile(succ) &&
                   m_cg->isSameCluster(o, succ) &&
                   m_cg->isSameCondExec(o, succ, ORBB_orlist(bb)) &&
                   HAVE_FLAG(m_cur_phase, PHASE_CA_DONE)) {
            //In some case, DEF has been placed at end of BB,
            //The operation can reduce life time actually,
            //especially live-outed GSR.

            //CASE 3:
            // t1(r1) = [xxx]
            // ...
            //        = t1(r1) //Real USE, not to be the USE in spilling code.
            // t2(r1) = [xxx] //Redundant load
            // ...
            //        = t2(r1)
            SR * succ_res = succ->get_result(0);
            ASSERTN(succ_res && succ_res->is_reg(), ("Illegal o"));
            if (m_cg->isSREqual(op_ld_res, succ_res)) {
                ddg.chainPredAndSucc(succ);
                ORBB_orlist(m_bb)->remove(succ);
                ddg.removeOR(succ);
                is_resch = true;
                return true;
            }
        }

//#define USE_EDGE_INFO
#ifdef USE_EDGE_INFO
        xcom::Edge * e = ddg.getEdge(o->id(), succ->id());
        ASSERTN(e, ("absent edge."));
        if (DDGEI_deptype(EDGE_info(e)) & DEP_REG_FLOW) {
            has_use = true;
        }
        if (DDGEI_deptype(EDGE_info(e)) & DEP_REG_OUT) {
            has_def = true;
        }
        if (DDGEI_deptype(EDGE_info(e)) & DEP_HYB) {
            has_use = has_def = true;
        }
#else
        //Since edge info may be incorrect when after operands were changed,
        //recompute the dependence info.
        if (m_cg->isRegflowDep(o, succ)) {
            has_use = true;
        }
        if (m_cg->isRegoutDep(o, succ)) {
            has_def = true;
        }
#endif
    }
    return is_resch;
}


bool LRA::elimRedundantStoreLoad(DataDepGraph & ddg)
{
    if (!HAVE_FLAG(m_opt_phase, (UINT)LRA_OPT_RSLE)) {
        return false;
    }

    bool is_resch = false;
    ORList succs;
    ORCt * next_orct = nullptr;
    ORCt * orct = nullptr;
    ORList op_preds;
    ORList succ_preds;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (o->getCode() == OR_spadjust ||
            o->is_asm() ||
            o->is_fake() ||
            OR_is_volatile(o)) {
            continue;
        }

        if (o->is_load()) {
            if (m_cg->isMultiLoad(o->getCode(), 2)) { continue; }

            xoc::Var const* ld_spill_loc = m_cg->computeSpillVar(o);
            if (ld_spill_loc != nullptr) { //o is a spilling.
                bool case_succ = false;
                ASSERTN(o->get_result(0) && o->get_result(0)->is_reg(),
                       ("Illegal load"));
                ddg.getSuccsByOrder(succs, o);
                case_succ = removeRedundantStoreLoadAfterLoad(
                    o, succs, m_bb, ld_spill_loc, ddg);

                is_resch |= case_succ;

                if (case_succ) {
                    ORBB_orlist(m_bb)->get_head(&next_orct);
                    continue;
                }
            }
        } else if (o->is_store()) {
            if (m_cg->isMultiStore(o->getCode(), -1)) {
                continue;
            }

            xoc::Var const* st_spill_loc = m_cg->computeSpillVar(o);
            if (st_spill_loc == nullptr ||
                //[rn] = xxx, keep the OR for conservative.
                //[.data+N] = xxx, keep the OR for conservative
                //unless IPA tell us the refferrence detail.
                VAR_is_global(st_spill_loc)) {
                continue;
            }

            SR * or_storeval = o->get_first_store_val();
            ddg.getSuccsByOrder(succs, o);

            //CASE 1:
            //...
            //[xxx] = t1 //redundant store if 'xxx' has not other use.
            //      = t1
            //...
            //Check for both global and local memory load of 'xxx'.
            if (m_ramgr != nullptr) {
                RefORBBList * rbl = m_ramgr->getVar2OR()->get(st_spill_loc);
                ASSERT0(m_ramgr->getBBList());

                if (rbl == nullptr || //No any spill locations
                                      //allocating for GSR.

                    //Local spill loc o single m_bb global spill loc.
                    ((rbl->get_elem_count() == 1) &&

                     //Only occurrence in one ORBB.
                     m_ramgr->getBBList()->get_elem_count() > 1)) {

                    bool remove = true;
                    for (OR * succ = succs.get_head();
                         succ != nullptr; succ = succs.get_next()) {
                        if (hasMemSideEffect(succ)) {
                            remove = false;
                            break;
                        }
                        if (OR_is_load(succ)) {
                            if (m_cg->isSameSpillLoc(st_spill_loc, o, succ)) {
                                //Read and write same spill location.
                                //We cannot remove o even if they
                                //are executed under different predication.
                                remove = false;
                                break;
                            }
                        }
                    }
                    if (remove) {
                        ddg.chainPredAndSucc(o);
                        ORBB_orlist(m_bb)->remove(orct);
                        ddg.removeOR(o);
                        ORBB_orlist(m_bb)->get_head(&next_orct);
                        is_resch = true;
                        continue;
                    }
                }
            }

            //CASE 2:
            //  [xxx] = t1 //redundant store
            //  t2    = [xxx]
            //=>
            //  t2 = t1 //reg copy
            bool case_success = false;
            ORCt * succct;
            OR * succ;
            for (succ = succs.get_head(&succct);
                 succ != nullptr; succ = succs.get_next(&succct)) {
                if (hasMemSideEffect(succ)) {
                    break;
                }

                //The nearest memload-succ-or
                if (OR_is_load(succ)) {
                    if (m_cg->isSameSpillLoc(st_spill_loc, o, succ)) {
                        //BUG: Followed consideration may override other life
                        //time by same physical register.
                        //
                        //e.g:
                        // [xxx] = t1(a4)
                        // ...
                        // t3(a4)=       <---- Need check physical register def.
                        // t4    = [xxx]
                        //
                        //That should not be transformed to
                        //
                        // [xxx] = t1(a4)
                        // ...
                        // t3(a4)=
                        // t4    = t1(a4)
                        //
                        //Only consider adjacent ors to
                        //reduce register pressure.
                        ORCt * tmp_orct = orct;
                        ORCt * tmp_succct;
                        succs.get_head(&tmp_succct);
                        OR * immediate_succ = ORBB_orlist(m_bb)->
                            get_next(&tmp_orct);
                        if (//Load is just Store
                            succ == immediate_succ ||

                            //Physical reigster of Store-Value might
                            //be overrided before 'succ'.
                            (HAVE_FLAG(m_cur_phase, PHASE_RA_DONE) &&
                             !m_cg->mayDefInRange(or_storeval, tmp_succct,
                                                     succct, succs))) {
                            case_success = mergeRedundantStoreLoad(
                                o, succ, succs, m_bb, st_spill_loc, ddg);
                            is_resch |= case_success;
                            break;
                        }

                        //Hereon, we only consider adjacent ors to reduce
                        //register pressure.
                        //And unadjacent case will be
                        //handled during hoistSpillLoc
                    }
                }

                if (OR_is_store(succ)) {
                    //Memory out dep
                    if (m_cg->isSameSpillLoc(st_spill_loc, o, succ)) {
                        break;
                    }
                }
            } //end for

            if (case_success) {
                ORBB_orlist(m_bb)->get_head(&next_orct);
                continue;
            }

            //CASE 3:
            //  s1: [xxx] = t1
            //       ...  = t1
            //  s2: [xxx] = t1
            //  one of s1 o s2 may be redundant if both of them have
            //  not any DEF, but we can only remove one of them.
            //o
            //  s3: [xxx] = t1
            //      ...
            //  s4: [xxx] = t2
            //  one of s3 o s4 may be redundant if both of them have
            //  not any DEF, but we can only remove one of them.
            succct = nullptr;
            for (succ = succs.get_head(&succct);
                 succ != nullptr; succ = succs.get_next(&succct)) {
                if (hasMemSideEffect(succ)) {
                    break;
                }

                if (OR_is_load(succ) &&
                    m_cg->isSameSpillLoc(st_spill_loc, o, succ)) {
                    break;
                }

                if (OR_is_store(succ)) {
                    if (m_cg->isSameSpillLoc(st_spill_loc, o, succ)) {
                        if (m_cg->isSameCondExec(o, succ, ORBB_orlist(m_bb)) &&
                            m_cg->isSameCluster(o, succ) &&
                            !OR_is_volatile(o)) {

                            //The spill code is redundant if it
                            //has not any predecessors.
                            op_preds.clean();
                            succ_preds.clean();
                            ddg.get_preds(op_preds, o);
                            ddg.get_preds(succ_preds, succ);
                            bool op_has_pred = false, succ_has_pred = false;
                            for (OR * pred = op_preds.get_tail();
                                 pred != nullptr; pred = op_preds.get_prev()) {
                                if (m_cg->mayDef(pred, or_storeval)) {
                                    op_has_pred = true;
                                    break;
                                }
                            }

                            SR * succ_storeval = succ->get_first_store_val();
                            for (OR * pred = succ_preds.get_tail();
                                 pred != nullptr;
                                 pred = succ_preds.get_prev()) {
                                if (m_cg->mayDef(pred, succ_storeval)) {
                                    succ_has_pred = true;
                                    break;
                                }
                            }

                            //May be all of them have not DEFs sometime.
                            //ASSERTN(op_has_pred || succ_has_pred,
                            //            ("stoveval has not any preds"));

                            if (!op_has_pred) {
                                ddg.chainPredAndSucc(o);
                                ORBB_orlist(m_bb)->remove(orct);
                                ddg.removeOR(o);

                                //More chance to optimize
                                ORBB_orlist(m_bb)->get_head(&next_orct);
                                is_resch = true;
                            } else if (!succ_has_pred) {
                                ddg.chainPredAndSucc(succ);
                                ORBB_orlist(m_bb)->remove(succct);
                                ddg.removeOR(succ);

                                //More chance to optimize
                                ORBB_orlist(m_bb)->get_head(&next_orct);
                                is_resch = true;
                            }
                            case_success = true;
                            break;
                        }
                    }
                } //end if
            } //end for

            if (case_success) {
                continue;
            }

            //CASE4: code pertain to the case moved to elimRedundantUse()
            //The store is redundant if there is not any DEF for local store
            //value, expect the dedicated sr.
        } //end if (o->is_store()...
    } //end for each OR
    return is_resch;
}


bool LRA::split(LifeTime * lt,
                List<LifeTime*> & prio_list,
                List<LifeTime*> & uncolored_list,
                LifeTimeMgr & mgr,
                DataDepGraph & ddg,
                RegFileGroup * rfg,
                InterfGraph & ig,
                REG spill_location,
                Action & action,
                IN OUT ClustRegInfo cri[CLUST_NUM])
{
    show_phase("---Split start");
    bool has_hole = false;
    LifeTime * cand = computeBestSpillCand(lt, ig, mgr, true, &has_hole);

    INT hole_startpos, hole_endpos;
    bool split_hole = false;
    if (has_hole && cand != lt) {
        split_hole = getResideinHole(&hole_startpos,
                                     &hole_endpos, cand, lt, mgr);
    }

    if (split_hole) {
        bool is_start_spill, is_end_spill;
        selectReasonableSplitPos(&hole_startpos, &hole_endpos, &is_start_spill,
                                 &is_end_spill, cand, mgr);
        splitLTAt(hole_startpos, hole_endpos,
                  is_start_spill, is_end_spill, cand, mgr);
    } else if (lt == cand) {
        splitOneLT(lt, prio_list, uncolored_list, mgr,
                   ig, spill_location, action);
    } else if (!canBeSpilled(lt, mgr)) {
        splitOneLT(cand, prio_list, uncolored_list, mgr,
                   ig, spill_location, action);
    } else {
        splitTwoLT(lt, cand, mgr);
    }
    show_phase("---Split,before ReAllocate_LifeTime");
    reallocateLifeTime(prio_list, uncolored_list, mgr, ddg, rfg, ig, cri);
    for (LifeTime * tmplt = uncolored_list.get_head();
         tmplt != nullptr; tmplt = uncolored_list.get_next()) {
        if (HAVE_FLAG(m_cur_phase, PHASE_FINIAL_FIXUP_DONE)) {
            //Should not change regfile again.
            action.set_action(tmplt, Action::SPLIT);
        } else {
            action.set_action(tmplt, Action::BFS_REASSIGN_REGFILE);
        }
    }
    show_phase("---Split finished");
    return true;
}


//Return true if there is hole in lifetime of 'owner' that
//'inner' can be lived in, and 'startpos','endpos' represented the hole.
bool LRA::getResideinHole(IN OUT INT * startpos,
                          IN OUT INT * endpos,
                          IN LifeTime * owner,
                          IN LifeTime * inner,
                          IN LifeTimeMgr & mgr)
{
    INT start = 0;
    *startpos = 0;
    *endpos = 0;
    INT i = -1, next_i = -1;
    for (i = LT_pos(owner)->get_first(), start = i;
         i >= 0 && i <= LT_pos(owner)->get_last();
         i = next_i) {
        next_i = i + 1;

        //Current pos is not a point of life time.
        if (!LT_pos(owner)->is_contain(i)) {
            next_i = LT_pos(owner)->get_next(i); //'next_i' may be -1
            start = next_i;
            continue;
        }
        if ((i == (INT)LT_FIRST_POS &&
             LT_pos(owner)->is_contain(LT_FIRST_POS)) || //life time live in.
            ((i == (INT)LT_LAST_POS) &&
             LT_pos(owner)->is_contain(LT_LAST_POS)) || //life time live out.
            LT_desc(owner).get(i) != nullptr) {

            if (i > start) {
                if (LT_pos(inner)->is_contained_in_range(start, i, true)) {
                    *startpos = start;
                    *endpos = i;
                    return true;
                }
            }
            start = i;
        }
    }
    return false;
}


//Return true if there is hole in lifetime of 'lt',
//and 'startpos','endpos' represented the hole.
bool LRA::getMaxHole(IN OUT INT * startpos,
                     IN OUT INT * endpos,
                     IN LifeTime * lt,
                     InterfGraph & ig,
                     IN LifeTimeMgr & mgr,
                     INT info_type)
{
    INT maxlen = 0;
    INT start = 0;
    *startpos = 0;
    *endpos = 0;
    INT i = -1, next_i = -1;
    bool is_first = true;
    List<LifeTime*> lt_group;
    for (i = LT_pos(lt)->get_first(), start = i;
         i >= 0 && i <= LT_pos(lt)->get_last();
         i = next_i) {
        next_i = i + 1;
        if (!LT_pos(lt)->is_contain(i)) {
            next_i = LT_pos(lt)->get_next(i); //'next_i' may be -1
            start = next_i;
            continue;
        }
        if ((i == (INT)LT_FIRST_POS &&
             LT_pos(lt)->is_contain(LT_FIRST_POS)) || //life time live in.
            ((i == (INT)LT_LAST_POS) &&
             LT_pos(lt)->is_contain(LT_LAST_POS)) || //life time live out.
            LT_desc(lt).get(i) != nullptr) {

            if (i > start) {
                switch (info_type) {
                case HOLE_LENGTH:
                    if (maxlen < (i - start)) {
                        maxlen = i - start;
                        *startpos = start;
                        *endpos = i;
                    }
                    break;
                case HOLE_INTERF_LT_NUM: {
                    INT count = 0;
                    if (is_first) {
                        is_first = false;
                        ig.getLifeTimeList(lt_group, SR_regfile(LT_sr(lt)));
                    }
                    for (LifeTime * tmp = lt_group.get_head();
                         tmp != nullptr; tmp = lt_group.get_next()) {
                        if (LT_pos(tmp)->is_contain(start) ||
                            LT_pos(tmp)->is_contain(i)) {
                            count ++;
                        }
                    }
                    if (maxlen < count) {
                        maxlen = count;
                        *startpos = start;
                        *endpos = i;
                    }
                    break;
                }
                default: ASSERTN(0, ("Unknown"));
                }
            }
            start = i;
        }//end if
    }//end for

    if (*startpos != *endpos) {
        return true;
    }
    return false;
}


//Calculate the number of lifetimes which only living in the 'hole'.
//Only compute the longest hole for each of life times.
void LRA::computeLTResideInHole(IN OUT List<LifeTime*> & reside_in_lts,
                                IN LifeTime * lt,
                                IN InterfGraph & ig,
                                IN LifeTimeMgr & mgr)
{
    INT hole_startpos, hole_endpos;
    getMaxHole(&hole_startpos, &hole_endpos, lt, ig, mgr, HOLE_LENGTH);
    xcom::BitSet * hole = LT_pos(lt)->get_subset_in_range(hole_startpos,
        hole_endpos, *getBitSetMgr()->create());
    ASSERTN(hole, ("What's wrong with memory pool?"));
    List<LifeTime*> lt_group;
    ASSERTN(SR_regfile(LT_sr(lt)),("regfile undefined"));
    ig.getLifeTimeList(lt_group, SR_regfile(LT_sr(lt)));
    INT count = 0;
    for (LifeTime * tmp = lt_group.get_head(); tmp != nullptr;
         tmp = lt_group.get_next()) {

        if (tmp == lt) continue;
        if (hole->is_contained_in_range(LT_pos(tmp)->get_first(),
                                        LT_pos(tmp)->get_last(), true)) {
            count++;
            reside_in_lts.append_tail(tmp);
        }
    }
}


//Determining which one should be spilled.
//    Computing spill cost:
//    The quotient is bigger, the spill cost is less,
//    is also the one we expect to spill.
//    <cost = number of uncolored neighbors of 'ni' / 'ni's priority>
//
//Return the spilling candidate life time selected.
//And 'has_hole' will be set to TRUE, if we could find a
//lifetime hole which contained several
//shorter lifetimes in it.
LifeTime * LRA::computeBestSpillCand(LifeTime * lt,
                                     InterfGraph & ig,
                                     LifeTimeMgr & mgr,
                                     bool try_self,
                                     bool * has_hole)
{
    LifeTime * best = nullptr, * better = nullptr;
    *has_hole = false;

    //Calculates the benefits if 'ni' is deal with as 'action' descriptive.
    List<LifeTime*> ni_list; //neighbor list of 'lt'

    //Inspecting all of neighbors of 'lt' even itself.
    ig.getNeighborList(ni_list, lt);
    if (try_self) {
        ni_list.append_head(lt); //May be we should spill 'lt' itself as well.
    }

    Vector<double> ni_cost; //vector is in dense storage.
    INT i;
    //1. Computing the cost of each of neighbours in terms of life time
    //   priority and the benefit when we split the life time.
    i = 0;
    LifeTime * ni;
    List<LifeTime*> ni_ni_list; //neighbor list of 'ni'
    for (ni = ni_list.get_head(); ni != nullptr; ni = ni_list.get_next(), i++) {
        ni_cost.set(i, 0.0);
        if (!canBeSpilled(ni, mgr) || !canBeSpillCandidate(lt, ni)) {
            continue;
        }

        ni_ni_list.clean(); //neighbor list of 'ni'
        ig.getNeighborList(ni_ni_list, ni);
        UINT uncolored_nini = 0;
        for (LifeTime * nini = ni_ni_list.get_head();
             nini != nullptr; nini = ni_ni_list.get_next()) {
            if (!LT_has_allocated(nini)) {
                uncolored_nini++;
            }
        }

        double c = (uncolored_nini + EPSILON) / (LT_prio(ni) + EPSILON);
        ni_cost.set(i, c);
    }


    //Selecting policy.
    //2. Choosing the best one as split-candidate that
    //   constains the most life times which unallocated register till now.
    INT most = 0, most_idx = -1;
    INT minor = 0, minor_idx = -1;
    i = 0;
    List<LifeTime*> residein_lts;
    for (ni = ni_list.get_head(); ni != nullptr; ni = ni_list.get_next(), i++) {
        if (!canBeSpilled(ni, mgr) || !canBeSpillCandidate(lt, ni)) {
            continue;
        }

        residein_lts.clean();
        computeLTResideInHole(residein_lts, ni, ig, mgr);

        //Computing life times which did not assign register yet.
        INT lt_num = 0;
        for (LifeTime * tmp = residein_lts.get_head();
             tmp != nullptr; tmp = residein_lts.get_next()) {
            if (!LT_has_allocated(tmp)) {
                lt_num++;
            }
        }

        if ((most < lt_num) ||
            (most_idx != -1 && most == lt_num &&
             ni_cost.get(most_idx) < ni_cost.get(i))) {
            most_idx = i;
            best = ni;
            most = lt_num;
        }

        //The inferior split candidate selection.
        //We say it is the split-cand if the number of life
        //times which residing in its hole to be the largest.
        if (minor < (INT)residein_lts.get_elem_count() ||
            (minor_idx != -1 &&
             minor == (INT)residein_lts.get_elem_count() &&
             ni_cost.get(minor_idx) < ni_cost.get(i))) {
            minor_idx = i;
            better = ni;
            minor = residein_lts.get_elem_count();
        }
    }
    if (best == nullptr && better != nullptr) { //The alternative choose.
        best = better;
    }
    if (best != nullptr) {
        //Simply select the lift time with largest hole.
        *has_hole = true;
        return best;
    }

    //3. If the first step failed, we choose candidate in terms of
    //   life time priority and the benefit when we split the candidate.
    i = 0;

    //We can obtain some benefits via the adjustment of 'action'.
    double maximal_cost = 0.0;
    for (ni = ni_list.get_head(); ni != nullptr; ni = ni_list.get_next(), i++) {
        if (!canBeSpilled(ni, mgr) || !canBeSpillCandidate(lt, ni)) {
            continue;
        }

        ni_ni_list.clean(); //neighbor list of 'ni'
        ig.getNeighborList(ni_ni_list, ni);
        UINT uncolored_nini = 0;
        for (LifeTime * nini = ni_ni_list.get_head();
             nini != nullptr; nini = ni_ni_list.get_next()) {
            if (!LT_has_allocated(nini)) {
                uncolored_nini++;
            }
        }

        double c = ni_cost.get(i);
        if (best == nullptr || maximal_cost < c) {
            if (ni != lt) {
                //Avoiding the followed case:
                // 1. Same start pos, or
                // 2. Same end pos.
                if ((LT_pos(ni)->get_first() == LT_pos(lt)->get_first()) ||
                    (LT_pos(ni)->get_last() == LT_pos(lt)->get_last())) {
                    continue;
                }
            }
            maximal_cost = c;
            best = ni;
        }
    }
    ASSERTN(best, ("Not any spill candidate."));
    return best;
}


//Try to change the register-file with the one with low pressure.
//Return true if the reassignation successed, otherwise back up
//to the previous status.
bool LRA::reassignRegFile(LifeTime * lt,
                          List<LifeTime*> & prio_list,
                          List<LifeTime*> & uncolored_list,
                          IN OUT ClustRegInfo cri[CLUST_NUM],
                          RegFileSet const& is_regfile_unique,
                          LifeTimeMgr & mgr,
                          InterfGraph & ig,
                          RegFileGroup * rfg)
{
    ASSERT0(lt && SR_regfile(LT_sr(lt)) != RF_UNDEF);
    DUMMYUSE(lt);
    DUMMYUSE(prio_list);
    DUMMYUSE(uncolored_list);
    DUMMYUSE(cri);
    DUMMYUSE(is_regfile_unique);
    DUMMYUSE(mgr);
    DUMMYUSE(ig);
    DUMMYUSE(rfg);
    return false;
}


//Moving life time out of their house to keep away from regfile of 'lt'.
//So that 'lt' could survive get down.
bool LRA::moving_house(LifeTime * lt,
                       List<LifeTime*> & prio_list,
                       List<LifeTime*> & uncolored_list,
                       IN OUT ClustRegInfo cri[CLUST_NUM],
                       RegFileSet const& is_regfile_unique,
                       LifeTimeMgr & mgr,
                       InterfGraph & ig,
                       Action & action,
                       RegFileGroup * rfg)
{
    List<LifeTime*> ni_list, ordered_list;
    ig.getNeighborList(ni_list, lt);

    //Sort by increasing order of priority.
    LifeTime *ni, *tmp;
    for (ni = ni_list.get_head(); ni; ni = ni_list.get_next()) {
        for (tmp = ordered_list.get_head();
             tmp != nullptr; tmp = ordered_list.get_next()) {
            if (LT_prio(tmp) >= LT_prio(ni)) {
                break;
            }
        }
        if (tmp == nullptr) { //Order list is empty.
            ordered_list.append_tail(ni);
        } else {
            ordered_list.insert_before(ni, tmp);
        }
    }

    for (ni = ordered_list.get_head(); ni; ni = ordered_list.get_next()) {
        //Try reassign neighbor's regfile.
        if (reassignRegFile(ni, prio_list, uncolored_list, cri,
                            is_regfile_unique, mgr, ig, rfg)) {
            action.set_action(ni, Action::BFS_REASSIGN_REGFILE);
            return true;
        }
    }
    return false;
}


bool LRA::solveConflictRecursive(LifeTime * lt,
                                 List<LifeTime*> & uncolored_list,
                                 List<LifeTime*> & prio_list,
                                 IN OUT ClustRegInfo cri[CLUST_NUM],
                                 RegFileSet const& is_regfile_unique,
                                 InterfGraph & ig,
                                 LifeTimeMgr & mgr,
                                 DataDepGraph & ddg,
                                 RegFileGroup * rfg,
                                 Action & action)
{
    REG spill_location = REG_UNDEF;
    switch (action.get_action(lt)) {
    case Action::BFS_REASSIGN_REGFILE: { //Try to reassign regfile
        List<LifeTime*> try_list;
        try_list.copy(uncolored_list);
        for (LifeTime * trylt = try_list.get_head();
             trylt != nullptr; trylt = try_list.get_next()) {
            reassignRegFile(trylt, prio_list, uncolored_list, cri,
                            is_regfile_unique, mgr, ig, rfg);
        }
        if (uncolored_list.get_elem_count() > 0) {
            lt = uncolored_list.get_head();
            action.set_action(lt, Action::MOVE_HOUSE);
            return solveConflictRecursive(lt, uncolored_list, prio_list,
                                          cri, is_regfile_unique, ig, mgr,
                                          ddg, rfg, action);
        }
        return true;
    }
    case Action::DFS_REASSIGN_REGFILE:
        //Try to reassign regfile, and DFS solver is enabled.
        if (!reassignRegFile(lt, prio_list, uncolored_list, cri,
                             is_regfile_unique, mgr, ig, rfg)) {
            //Can not hoist regfile of 'lt' from A/AC->D o D->D.
            action.set_action(lt, Action::MOVE_HOUSE);
            return solveConflictRecursive(lt, uncolored_list, prio_list,
                                          cri, is_regfile_unique, ig, mgr,
                                          ddg, rfg, action);
        }
        return true;
    case Action::MOVE_HOUSE: //Try to move life times out of their house.
        if (!moving_house(lt, prio_list, uncolored_list, cri,
                          is_regfile_unique, mgr, ig, action, rfg)) {
            action.set_action(lt, Action::SPLIT);
            return solveConflictRecursive(lt, uncolored_list, prio_list,
                                          cri, is_regfile_unique, ig, mgr,
                                          ddg, rfg, action);
        }
        return true;
    case Action::SPLIT: //Splitting
        if (!split(lt, prio_list, uncolored_list, mgr, ddg, rfg,
                   ig, spill_location, action, cri)) {
            action.set_action(lt, Action::SPILL);
            return solveConflictRecursive(lt, uncolored_list, prio_list,
                                          cri, is_regfile_unique, ig, mgr,
                                          ddg, rfg, action);
        }
        return true;
    case Action::SPILL: //The last answer is spilling
        spill(lt, prio_list, uncolored_list, mgr, ddg, rfg, ig,
              spill_location, action, cri);
        return true;
    default: ASSERTN(0, ("ACTION_NON!"));
    }
    return false;
}


bool LRA::solveConflict(List<LifeTime*> & uncolored_list,
                        List<LifeTime*> & prio_list,
                        IN OUT ClustRegInfo cri[CLUST_NUM],
                        RegFileSet const& is_regfile_unique,
                        InterfGraph & ig,
                        LifeTimeMgr & mgr,
                        DataDepGraph & ddg,
                        RegFileGroup * rfg,
                        Action & action)
{
    for (;uncolored_list.get_elem_count();) {
        LifeTime * lt = uncolored_list.get_head();
        bool succ = solveConflictRecursive(lt, uncolored_list,
                                           prio_list, cri, is_regfile_unique,
                                           ig, mgr, ddg, rfg, action);
        DUMMYUSE(succ);
        ASSERTN(succ, ("Unsolved conflict!!"));
    }
    return true;
}


//Choose and assign regfile according to regfile pressure.
void LRA::assignPreferRegFilePressure(IN LifeTime * lt,
                                      IN ClustRegInfo cri[CLUST_NUM],
                                      IN List<REGFILE> & regfile_cand)
{
    CLUST clust = LT_cluster(lt);

    //Choose regfile with the lowest pressure.
    INT lowest = -1;
    REGFILE best_rf = RF_UNDEF;
    REGFILE rf = regfile_cand.get_head();
    bool first = true;
    for (UINT i = 0; i < regfile_cand.get_elem_count(); i++,
        rf = regfile_cand.get_next()) {
        INT v = CRI_regfile_count(cri[clust], rf);
        if (first) {
            first = false;
            lowest = v;
            best_rf = rf;
        } else if (lowest >= v) {
            lowest = v;
            best_rf = rf;
        }
    }

    ASSERT0(best_rf != RF_UNDEF);
    SR * sr = LT_sr(lt);
    SR_regfile(sr) = best_rf;
}


//Assign 'regfile' to 'lt', and update 'cri'.
void LRA::assignDesignatedRegFile(IN LifeTime * lt,
                                  REGFILE regfile,
                                  IN OUT ClustRegInfo cri[CLUST_NUM])
{
    SR_regfile(LT_sr(lt)) = regfile;
    CLUST clust = LT_cluster(lt);
    CRI_regfile_count(cri[clust], regfile)++;
}


//Only considering D1,D2 regfile of 'expvalue'
//Assign the same regfile to 'lt' with the regfile of the most
//expectant lifetime.
void LRA::assignPreferAnticipation(IN OUT LifeTime * lt,
                                   IN RegFileAffinityGraph & rdg,
                                   IN ClustRegInfo expvalue[CLUST_NUM],
                                   IN OUT ClustRegInfo cri[CLUST_NUM],
                                   REGFILE best_rf,
                                   REGFILE better_rf)
{
    //Evaluate register pressure.
    INT expv_1 = CRI_regfile_count(expvalue[LT_cluster(lt)], best_rf);
    INT expv_2 = CRI_regfile_count(expvalue[LT_cluster(lt)], better_rf);

    INT cri_1 = CRI_regfile_count(cri[LT_cluster(lt)], best_rf);
    INT cri_2 = CRI_regfile_count(cri[LT_cluster(lt)], better_rf);

    float ratio =
        ((float)MAX(abs(cri_1), abs(cri_2))) /
        ((float)MIN(abs(cri_1), abs(cri_2)));

    //We prefer balancing the register pressure in between
    //D1 and D2 to assign the same regfile of the expectant one.
    if ((expv_1 == expv_2) ||
        (ratio > UNBALANCE_RATE &&
         abs(expv_1) != INFINITE &&
         abs(expv_2) != INFINITE)) {

        //Means regfile of 'lt' must be same as expectant.
        if (cri_1 > cri_2) {
            assignDesignatedRegFile(lt, better_rf, cri);
        } else {
            assignDesignatedRegFile(lt, best_rf, cri);
        }
    } else {
        if (rdg.isEnableFarEdge()) {
            if (expv_1 > 0 && expv_2 <= 0) {
                assignDesignatedRegFile(lt, best_rf, cri);
            } else if (expv_2 > 0 && expv_1 <= 0) {
                assignDesignatedRegFile(lt, better_rf, cri);
            } else if (abs(expv_1) > abs(expv_2)) {
                assignDesignatedRegFile(lt, best_rf, cri);
            } else {
                assignDesignatedRegFile(lt, better_rf, cri);
            }
        } else {
            ASSERTN(expv_1 >= 0 && expv_2 >= 0, ("disable far edge"));
            if (expv_1 > expv_2) {
                assignDesignatedRegFile(lt, best_rf, cri);
            } else {
                assignDesignatedRegFile(lt, better_rf, cri);
            }
        }
    }
}


//Return true if sr's register-file is unique.
//'o': sr belongs to
bool LRA::assignUniqueRegFile(SR * sr, OR *, bool is_result)
{
    DUMMYUSE(is_result);
    ASSERT0(sr->is_reg());
    if (sr->getPhyReg() != REG_UNDEF) {
        SR_regfile(sr) = tmMapReg2RegFile(sr->getPhyReg());
        ASSERTN(sr->getRegFile() != RF_UNDEF, ("Unknown regfile"));
        return true;
    }
    if (sr->is_global()) {
        return true;
    }
    if (SR_is_dedicated(sr)) {
        return true;
    }
    if (sr->is_rflag()) {
        SR_regfile(sr) = m_cg->getRflagRegfile();
        return true;
    }
    return false;
}


//Analysis sr's regfile if it is unique and which
//register-file can not be changed.
void LRA::computeUniqueRegFile(IN OUT RegFileSet & is_regfile_unique)
{
    ORCt * orct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&orct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_next(&orct)) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (!sr->is_reg()) {
                continue;
            }
            if (is_regfile_unique.is_contain(SR_sregid(sr))) {
                continue;
            }
            if (o->is_asm()) {
                if (sr->getPhyReg() != REG_UNDEF ||
                    sr->getRegFile() != RF_UNDEF) {
                    is_regfile_unique.bunion(SR_sregid(sr));
                    continue;
                }
            }
            if (assignUniqueRegFile(sr, o, true)) {
                is_regfile_unique.bunion(SR_sregid(sr));
            }
        }
        for (i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (!sr->is_reg()) {
                continue;
            }
            if (is_regfile_unique.is_contain(SR_sregid(sr))) {
                continue;
            }
            if (o->is_asm()) {
                if (sr->getPhyReg() != REG_UNDEF ||
                    sr->getRegFile() != RF_UNDEF) {
                    is_regfile_unique.bunion(SR_sregid(sr));
                    continue;
                }
            }
            if (assignUniqueRegFile(sr, o, false)) {
                is_regfile_unique.bunion(SR_sregid(sr));
            }
        } //end for
    } //end for

    if (m_cg->isGRAEnable()) {
        INT i;
        for (i = ORBB_liveout(m_bb).get_first();
             i != -1; i = ORBB_liveout(m_bb).get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERTN(sr->is_reg() && sr->getPhyReg() != REG_UNDEF,
                    ("GSR without register"));
            is_regfile_unique.bunion(SR_sregid(sr));
        }
        for (i = ORBB_livein(m_bb).get_first();
             i != -1; i = ORBB_livein(m_bb).get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERTN(sr->is_reg() &&
                    sr->getPhyReg() != REG_UNDEF, ("GSR without register"));
            is_regfile_unique.bunion(SR_sregid(sr));
        }
    }
}


//Adjustment. When some regfiles were refered frequently, try
//to change that HOT regfile to another one used seldom.
void LRA::refineAssignedRegFile(IN LifeTimeMgr & mgr,
                                RegFileSet const& is_regfile_unique,
                                IN OUT ClustRegInfo cri[CLUST_NUM])
{
    DUMMYUSE(mgr);
    DUMMYUSE(is_regfile_unique);
    DUMMYUSE(cri);
}


//Evaluate the reference situation by looking through OR
//occurrence of lifetime.
//'units': function units lifetime traversed.
void LRA::chooseRegFileCandInLifeTime(IN UnitSet & us,
                                      IN LifeTime *,
                                      IN LifeTimeMgr &,
                                      IN DataDepGraph &,
                                      OUT List<REGFILE> & regfile_cand)
{
    m_cg->mapUnitSet2RegFileList(us, regfile_cand);
}


void LRA::chooseBestRegFileFromMultipleCand(IN LifeTime * lt,
                                            IN List<REGFILE> & regfile_cand,
                                            IN OUT ClustRegInfo cri[CLUST_NUM],
                                            IN LifeTimeMgr & mgr,
                                            IN RegFileAffinityGraph & rdg)
{
    ASSERT0(regfile_cand.get_elem_count() >= 2);
    //Record sr without register file assigned.
    //and that should be affinitive one with 'sr'.
    List<LifeTime*> affi_near_list;
    List<LifeTime*> affi_far_list;

    //Experimental value used to determination.
    ClustRegInfo expv[CLUST_NUM];
    resetClustRegInfo(expv, (UINT)CLUST_NUM);

    List<UINT> ni_list; //neighbour list
    rdg.getNeighborList(ni_list, lt->id());
    UINT i = 0;
    for (UINT id = ni_list.get_head();
         i < ni_list.get_elem_count();
         i++, id = ni_list.get_next()) {
        LifeTime * ni = mgr.getLifeTime(id);
        ASSERT0(ni);
        ASSERTN(LT_cluster(ni) == LT_cluster(lt),
               ("Not in same cluster"));
        SR * nisr = LT_sr(ni);
        RDGEdgeInfo * ei = rdg.getEdgeInfo(lt->id(), LT_id(ni));
        ASSERTN(ei, ("xcom::Edge info is empty"));

        //Does the strategy aware of the furthest clustering.
        if (RDGEI_exp_val(ei) < 0 && !rdg.isEnableFarEdge()) {
            continue;
        }
        if (SR_regfile(nisr) != RF_UNDEF) {
            //neighbour has been assigned regfile.
            INT v = RDGEI_exp_val(ei);
            if (nisr->is_global() || SR_is_dedicated(nisr)) {
                v = 2 * v;
            }
            CRI_regfile_count(expv[LT_cluster(lt)], SR_regfile(nisr)) += v;
        } else {
            if (RDGEI_exp_val(ei) > 0) {
                affi_near_list.append_tail(ni);
            } else {
                affi_far_list.append_tail(ni);
            }
        }
    } //end for each neighbour

    //Choose more affinitive two regfiles from candidates.
    INT best_v = 0, better_v = 0;
    REGFILE best_rf = RF_UNDEF, better_rf = RF_UNDEF;
    REGFILE rf = regfile_cand.get_head();
    if (regfile_cand.get_elem_count() == 1) {
        best_rf = better_rf = regfile_cand.get_head();
    } else {
        REGFILE rf1 = regfile_cand.get_head();
        REGFILE rf2 = regfile_cand.get_next();
        if (CRI_regfile_count(expv[LT_cluster(lt)], rf1) >
            CRI_regfile_count(expv[LT_cluster(lt)], rf2)) {
            best_v = CRI_regfile_count(expv[LT_cluster(lt)], rf1);
            best_rf = rf1;
            better_v = CRI_regfile_count(expv[LT_cluster(lt)], rf2);
            better_rf = rf2;
        } else {
            best_v = CRI_regfile_count(expv[LT_cluster(lt)], rf2);
            best_rf = rf1;
            better_v = CRI_regfile_count(expv[LT_cluster(lt)], rf1);
            better_rf = rf2;
        }

        for (UINT i2 = 2; i2 < regfile_cand.get_elem_count();
             i2++, rf = regfile_cand.get_next()) {
            INT v = CRI_regfile_count(expv[LT_cluster(lt)], rf);
            if (v >= best_v) {
                best_v = v;
                best_rf = rf;
            } else if (v >= better_v) {
                better_v = v;
                better_rf = rf;
            }
        }
    }

    if (best_v == better_v) {
        //In favor of the regfile with minimal register pressure.
        assignPreferRegFilePressure(lt, cri, regfile_cand);
    } else {
        //Assign the same regfile to 'lt' used by the regfile of
        //the most expectant lifetime.
        assignPreferAnticipation(lt, rdg, expv, cri, best_rf, better_rf);
    }

    //Here we only prefer nearest neighbors with register pressure
    //consideration.
    //for (LifeTime *near = affi_near_list.get_head(); near;
    //    near = affi_near_list.get_next()) {
    //    assignDesignatedRegFile(near, SR_regfile(LT_sr(lt)), cri);
    //}
}


//Assign Register File in term of the affinity, resource pressure,
//and target dependent constraints.
//'lt': assign REGFILE to it.
//'regfile_cand': a list of REGFILE.
//NOTICE: 'cri' should be maintained.
void LRA::assignRegFileInResourceView(IN LifeTime * lt,
                                      IN List<REGFILE> & regfile_cand,
                                      IN OUT ClustRegInfo cri[CLUST_NUM],
                                      IN LifeTimeMgr & mgr,
                                      IN RegFileAffinityGraph & rdg)
{
    ASSERT0(regfile_cand.get_elem_count() > 0);
    if (regfile_cand.get_elem_count() == 1) {
        REGFILE rf = regfile_cand.get_head();
        assignDesignatedRegFile(lt, rf, cri);
    } else {
        //If there are multiple REGFILE candidates, choose the best.
        chooseBestRegFileFromMultipleCand(lt, regfile_cand, cri, mgr, rdg);
    }
}


//Adopt heuristic strategy to assign register file.
void LRA::choose_regfile(IN LifeTime * lt,
                         IN OUT ClustRegInfo cri[CLUST_NUM],
                         IN LifeTimeMgr & mgr,
                         IN DataDepGraph & ddg,
                         IN RegFileAffinityGraph & rdg)
{
    UnitSet us;
    mgr.enumTraversedUnits(lt, us);
    List<REGFILE> regfile_cand;
    chooseRegFileCandInLifeTime(us, lt, mgr, ddg, regfile_cand);
    assignRegFileInResourceView(lt, regfile_cand, cri, mgr, rdg);
}


void LRA::assignRegFile(IN OUT ClustRegInfo cri[CLUST_NUM],
                        RegFileSet const& is_regfile_unique,
                        IN LifeTimeMgr & mgr,
                        IN DataDepGraph & ddg,
                        IN RegFileAffinityGraph & rdg)
{
    //1. Assign sr with local-regbank if sr has
    //    appeared only in same one function unit.
    //2. Assign sr with cross-regbank(D) if sr both has
    //    appeared in M and I function unit.
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        SR * sr = LT_sr(lt);
        ASSERTN(sr->is_reg(), ("sr is not a register"));
        if (is_regfile_unique.is_contain(SR_sregid(sr))) {
            continue;
        }

        //Check for sibling life time
        LifeTime * prev = mgr.getSibMgr()->getFirstPrevSib(lt);
        if (prev != nullptr && SR_regfile(LT_sr(prev)) != RF_UNDEF) {
            assignDesignatedRegFile(lt, SR_regfile(LT_sr(prev)), cri);
            continue;
        }

        //Check for sibling life time
        LifeTime * next = mgr.getSibMgr()->getFirstNextSib(lt);
        if (next != nullptr && SR_regfile(LT_sr(next)) != RF_UNDEF) {
            assignDesignatedRegFile(lt, SR_regfile(LT_sr(next)), cri);
            continue;
        }

        if (SR_is_dedicated(sr)) {
            continue;
        }

        choose_regfile(lt, cri, mgr, ddg, rdg);
    }

    #ifdef _DEBUG_
    //Verifing regfile of all SRs have been assigned.
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (sr->is_reg() && sr->getRegFile() == RF_UNDEF) {
                ASSERTN(0, ("assignRegFile: No regfile assigned"));
            }
        }
        for (i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (sr->is_reg() && sr->getRegFile() == RF_UNDEF) {
                ASSERTN(0, ("assignRegFile: No regfile assigned"));
            }
        }
    }
    #endif
    refineAssignedRegFile(mgr, is_regfile_unique, cri);
}


bool LRA::isAlwaysColored(
        LifeTime * lt,
        InterfGraph & ig,
        LifeTimeMgr & mgr)
{
    RegSet * allowed_reg_set = mgr.getUsableRegSet(lt);
    if (allowed_reg_set == nullptr) {
        return false;
    }
    UINT degree = ig.getDegree(lt->id());

    //lt is allocable
    if (degree + 1 <= allowed_reg_set->get_elem_count()) {
        return true;
    }
    return false;
}


//Compute rematerialized cost.
float LRA::computeRematCost(SR * sr, OR * o)
{
    if (m_cg->isRecalcOR(o)) {
        return 1.0;
    }
    return computeReloadCost(sr, o);
}


//Compute life time priority.
float LRA::computePrority(REG spill_location,
                          IN LifeTime * lt,
                          IN LifeTimeMgr & mgr,
                          IN DataDepGraph & ddg)
{
    #define CRITICAL_PATH_WEIGHT 10.0
    SR * sr = LT_sr(lt);
    bool is_remat = false;
    double prio = 0.0;
    INT occ = 0; //occurrence within own life time
    ORIdTab op_visited;
    //saving = (Number of DEF) * (cost of spilling result of one DEF) +
    //           (Number of USE) * (cost of reloading operand of one USE)
    for (INT i = LT_pos(lt)->get_first(); i >= 0; i = LT_desc(lt).get_next(i)) {
        PosInfo * pi = LT_desc(lt).get(i);
        if (pi != nullptr) {
            OR * o = mgr.getOR(i);
            occ++;

            if (op_visited.find(o->id())) {
                continue;
            }
            op_visited.append(o->id());

            if (ddg.isOnCriticalPath(o)) {
                prio += CRITICAL_PATH_WEIGHT;
            }

            if (pi->is_def()) {
                if (m_cg->isRecalcOR(o)) {
                    is_remat = true;
                    prio += 0.0; //Need not any spill code.
                } else {
                    is_remat = false;
                    if (spill_location != REG_UNDEF) {
                        prio += computeCopyCost(o);
                    } else {
                        prio += computeSpillCost(sr, o);
                    }
                }
            } else {
                if (is_remat) {
                    prio += computeRematCost(sr,o);
                } else {
                    if (spill_location != REG_UNDEF) {
                        prio += computeCopyCost(o);
                    } else {
                        prio += computeReloadCost(sr,o);
                    }
                }
            }
        }
    }

    //The density is higher, the priority is higher.
    //Calculating the density.
    if (occ != 0) {
        ASSERTN(LT_pos(lt)->get_elem_count() > 0,
               ("Not any position of life time %d", lt->id()));
        float denom = (float)LT_pos(lt)->get_elem_count();
        prio *= ((float)occ + EPSILON) / (denom + EPSILON);
    }

    if (prio < EPSILON) { //means saving == 0.0
        prio = (float)EPSILON;
    }

    //The usable registers are fewer, the priority is higher.
    //Spill cost is inverse to the number of usable registers.
    prio /= (float)(mgr.getUsableRegSet(lt)->get_elem_count() + EPSILON);

    UINT cnt = mgr.getSibMgr()->countNumOfSib(lt);
    if (cnt > 0) {
        prio *= (cnt + 1)* 2.0;
    }
    return (float)prio;
}


//Compute priority list and sort life times with descending order of priorities.
//We use some heuristics factors to evaluate the
//priorities of each of life times:
// 1. Life time in critical path will be put in higher-priority.
// 2. Life time whose symbol register referenced in
//     high density will be put in higher-priority.
// 3. Life time whose usable registers are fewer, the priority is higher.
//
// TO BE ESTIMATED:
// * Life time in longer live-range has higher priority.
void LRA::buildPriorityList(IN OUT List<LifeTime*> & prio_list,
                            IN InterfGraph & ig,
                            IN LifeTimeMgr & mgr,
                            DataDepGraph & ddg)
{
    ASSERTN(prio_list.get_elem_count() == 0,
           ("Priority list has been built already."));
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        if (!ig.isGraphNode(lt)) {
            continue;
        }
        if (!isAlwaysColored(lt, ig, mgr)) {
            LT_prio(lt) += 10.0;
            //prio_list.append_tail(lt);
            //continue;
        }
        LT_prio(lt) = computePrority(REG_UNDEF, lt, mgr, ddg);
        ASSERTN(LT_prio(lt) > 0.0, ("No any saving?"));

        //Searching for appropriate position to place
        LifeTime * t;
        xcom::C<LifeTime*> * ct;
        for (t = prio_list.get_head(&ct);
             t != nullptr; t = prio_list.get_next(&ct)) {
            if (LT_prio(t) < LT_prio(lt)) {
                break;
            }
        }
        if (t == nullptr) {
            prio_list.append_tail(lt);
        } else {
            prio_list.insert_before(lt, ct);
        }
    }
}


void LRA::dumpPrioList(List<LifeTime*> & prio_list)
{
    if (!getRegion()->isLogMgrInit()) { return; }
    StrBuf buf(64);
    note(getRegion(), "\nBB%d, Priority List:", m_bb->id());
    UINT i = 0;
    while (i < prio_list.get_elem_count()) {
        LifeTime * lt = prio_list.get_head_nth(i);
        note(getRegion(), "\n  LT(%d):%f ", lt->id(), LT_prio(lt));

        buf.clean();
        note(getRegion(), "[%s]", LT_sr(lt)->get_name(buf, m_cg));

        i++;
    }
    note(getRegion(), "\n");
}


//Reset all life times in order to recompute priority list, since
//critical path will change possible.
void LRA::resetLifeTimeAllocated(LifeTimeMgr & mgr)
{
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        SR * sr = LT_sr(lt);
        if (SR_is_dedicated(sr) ||
            sr->is_global() ||
            m_cg->isBusCluster(LT_cluster(lt))) {
            continue;
        }
        SR_phy_reg(LT_sr(lt)) = REG_UNDEF;
    }
}


void LRA::coalesceCopy(OR * o, DataDepGraph & ddg, bool * is_resch)
{
    //For conservative optimizing, o cannot DEF dedicated register.
    //e.g:
    //    d5 = d4
    //    [xxx] = d5
    //    br r7
    //=>
    //    [xxx] = d4
    //    br r7 //WRONG!! d5 is incorrectly,
    //    since d5 is returnval register.

    //tgt = src
    // ...
    //    = tgt
    SR * src = o->get_opnd(m_cg->computeCopyOpndIdx(o));
    SR * tgt = o->get_result(o->get_result_idx(o->get_copy_to()));
    ASSERTN(src && tgt && src->is_reg() && tgt->is_reg(),
        ("not a register"));
    ORList succs;
    ddg.getSuccsByOrder(succs, o);

    //Termination conditions:
    //Similar as SSA Version, e.g:
    //1.    X = Y
    //        ...
    //2.    Y = X
    //3.      = Y
    //Copy value from 1 to 2, transfer Y, after we added version info,
    //1.    X0 = Y0
    //        ...
    //2.    Y1 = X0
    //3.       = Y1
    //It is clearly that we can only transfer Y0 to X0,
    //but is to cross statement 2.
    bool terminate = false;
    bool do_copy_value = false;
    for (OR * succ = succs.get_head();
         succ != nullptr; succ = succs.get_next()) {
        if (m_cg->mayDef(succ, src) || m_cg->mayDef(succ, tgt)) {
            terminate = true;
        }

        //CASE: Handling bus-or
        //e.g:
        //  SR238(d2)[D1] :- copy_m SR254(a4)[A1]
        //  SR232 [A1] :- bus SR238(d2)[D1]
        //=>
        //  SR232 [A1] :- bus SR254(a4)[A1]
        //So we should determine the condition via 'SameLike' but
        //is not 'Same'.
        if (m_cg->isSameLikeCluster(o, succ) &&
            (m_cg->isSameCondExec(o, succ, ORBB_orlist(m_bb)) ||
                m_cg->isSafeToOptimize(o, succ))) {
            if (m_cg->mustUse(succ, tgt)) {
                bool doit = false;

                //Checking for the safe condition of copy-value.
                if (m_cg->isConsistentRegFileForCopy(
                    SR_regfile(src), SR_regfile(tgt))) {
                    doit = true;
                }

                //CASE: t1(D1) = t2(D1)     (o)
                //      sw val, t1(D1), 100 (succ)
                //t1, is the base register of LD/Var,
                //and may be assigned illegal regfile during
                //assignRegFile(), and avoid the
                //illegal copy. Check it out.
                if (SR_regfile(src) != RF_UNDEF) {
                    if (m_cg->isValidRegFile(succ, tgt,
                        SR_regfile(src), false)) {
                        doit = true;
                    }
                    else {
                        doit = false;
                    }
                }

                //Check OR if it has same result-sr as operand-sr.
                if (doit &&
                    m_cg->isOpndSameWithResult(
                        nullptr, succ, nullptr, nullptr)) {
                    for (UINT i = 0; i < succ->result_num(); i++) {
                        for (UINT j = 0; j < succ->opnd_num(); j++) {
                            if (succ->get_result(i) ==
                                succ->get_opnd(j)) {
                                if (m_cg->isSREqual(tgt,
                                    succ->get_result(i))) {
                                    doit = false;
                                    i = succ->result_num();
                                    j = succ->opnd_num();
                                }
                            }
                        }
                    }
                }

                if (OR_is_asm(succ)) {
                    //CASE: User may write inline ASM as
                    //    asm ("" : "=r" (a) : "0" (b));
                    //then the genereated OR looks like:
                    //    SR232 :- asm SR232
                    //whereas the asm would NOT actually be print as
                    //    sr232 = sr232.
                    //The stupid asm means that copy 'b' to 'a',
                    //thus we could not copy value to 'src'.
                    doit = false;
                }

                if (doit) { //Actually do copy
                    m_cg->renameOpnd(succ, tgt, src, true);
                    //Do not start from the prior one.
                    //BUG:Need shift all edges of 'o' to 'succ'
                    //  e.g: 1. t1 = t2
                    //       2. t3 = t1
                    //       3. t2 = xxx
                    //       4.    = t3
                    //After copy-value: cp t2 to t1
                    //       1. t1 = t2
                    //       2. t3 = t2
                    //       3. t2 = xxx
                    //       4.    = t3
                    //There are only dependence between 2 and 4,
                    //whereas we copy value from 2->4, it
                    //cross the DEF of t2, which in 3.
                    //So we must append all edges of 'o'
                    //to 'succ' if we perform misc transformations
                    //to keep all of data informations.
                    ORList tmp;
                    ddg.get_preds(tmp, o);
                    ddg.unifyEdge(tmp, succ);
                    ddg.get_succs(tmp, o);
                    ddg.unifyEdge(tmp, succ);

                    ddg.get_succs(tmp, succ);
                    ddg.unifyEdge(tmp, o);
                    ddg.get_preds(tmp, succ);
                    ddg.unifyEdge(tmp, o);

                    do_copy_value = true;
                }
            }
        } //end if

        if (terminate) {
            break;
        }
    } //end for (OR * succ =...

    if (do_copy_value) {
        *is_resch = true; //need more optimizations processing.
    }
}


void LRA::coalesceMovi(OR * o, DataDepGraph & ddg, bool * is_resch,
                       ORCt * orct, ORCt ** next_orct)
{
    //CASE: tgt = lit32/lit16/ulit32/ulit16
    //  ...
    //  succ_tgt = tgt
    //=>
    //  succ_tgt = lit
    bool terminate = false, is_remove_op = false;
    SR * tgt = o->get_result(0);
    ASSERTN(tgt != nullptr && tgt->is_reg(), ("sr is not a register"));
    ORList succs;
    ddg.getSuccsByOrder(succs, o);
    for (OR * succ = succs.get_head();
        succ != nullptr; succ = succs.get_next()) {
        if (m_cg->mayDef(succ, tgt)) {
            terminate = true;
        }
        if (m_cg->isSameCluster(o, succ) &&
            m_cg->computeORCluster(o) != CLUST_UNDEF &&
            (m_cg->isSameCondExec(o, succ, ORBB_orlist(m_bb)) ||
                m_cg->isSafeToOptimize(o, succ)) &&
            m_cg->isCopyOR(succ)) {

            if (m_cg->mustUse(succ, tgt)) {
                //Safe constant-transfer condition checking.
                ORList ors;
                //Operand 1 of 'o' must be literal.
                SR * lit = o->get_imm_sr();
                SR * pd = succ->get_pred();
                ASSERTN(pd && lit->is_constant(),
                        ("Operand 1 of Movi must be literal"));

                SR * succ_src = succ->get_opnd(
                    m_cg->computeCopyOpndIdx(succ));
                DUMMYUSE(succ_src);

                SR * succ_tgt = succ->get_result(
                    succ->get_result_idx(succ->get_copy_to()));
                DUMMYUSE(succ_tgt);

                ASSERTN(m_cg->isSREqual(succ_src, tgt), ("copy what?"));

                UNIT succ_unit = m_cg->computeORUnit(succ)->checkAndGet();
                CLUST succ_clust = m_cg->computeORCluster(succ);
                UNIT or_unit = m_cg->computeORUnit(o)->checkAndGet();
                OR_TYPE new_opc = OR_UNDEF;
                if (or_unit != succ_unit) {
                    new_opc = m_cg->computeEquivalentORType(o->getCode(),
                        succ_unit, succ_clust);
                } else {
                    new_opc = o->getCode();
                }
                ASSERTN(new_opc != OR_UNDEF, ("illegal ortype"));
                ors.append_tail(m_cg->buildOR(new_opc,
                                1, 2, succ_tgt, pd, lit));
                if (ors.get_head() != nullptr) {
                    is_remove_op = true;
                    *is_resch = true;
                    m_cg->setCluster(ors, m_cg->computeORCluster(o));

                    //Get preds,succs info
                    ORList succ_preds, succ_succs;
                    ddg.get_succs(succ_succs, succ);
                    ddg.get_preds(succ_preds, succ);

                    //Insert Movi operation following the point.
                    ORBB_orlist(m_bb)->insert_after(ors, succ);

                    //Remove the redundant copy operation.
                    //Does not need holder.
                    ORBB_orlist(m_bb)->remove(succ);
                    ddg.removeOR(succ);

                    //Union all old edges to new o 'new_mov'.
                    ASSERTN(ors.get_elem_count() == 1,("illegal literal move"));
                    OR * new_mov = ors.get_head();
                    ddg.appendOR(new_mov);
                    ddg.unifyEdge(succ_succs, new_mov);
                    ddg.unifyEdge(succ_preds, new_mov);
                }
            }
        } //end if

        if (terminate) {
            break;
        }
    } //end for all succs

    if (is_remove_op) {
        ORCt * prev_orct = orct;
        ORBB_orlist(m_bb)->get_prev(&prev_orct);
        if (prev_orct != nullptr) {
            *next_orct = prev_orct;
        } else {
            ORBB_orlist(m_bb)->get_head(next_orct);
        }
        *is_resch = true;
    }
}


//'cp_any': whether do copy-prop for any register,
//this may increase register pressure.
bool LRA::coalescing(DataDepGraph & ddg, bool cp_any)
{
    bool is_resch = false;
    if (!HAVE_FLAG(m_opt_phase, (UINT)LRA_OPT_RCEL)) {
        return false;
    }

    ORCt * next_orct;
    ORCt * orct = nullptr;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (o->is_asm()) { continue; }
        if (m_cg->isCopyOR(o)) {
            if (!cp_any && !m_cg->isRecalcOR(o)) { continue; }
            coalesceCopy(o, ddg, &is_resch);
        } else if (OR_is_movi(o)) {
            coalesceMovi(o, ddg, &is_resch, orct, &next_orct);
        }
    }
    return is_resch;
}


//Return ture if changed.
//'handled': add OR to table if it has been handled.
bool LRA::cse(IN OUT DataDepGraph & ddg, IN OUT ORIdTab & handled)
{
    bool is_resch = false;
    if (!HAVE_FLAG(m_opt_phase, (UINT)LRA_OPT_RCIE)) {
        return false;
    }
    ORCt * next_orct;
    ORCt * orct = nullptr;
    ORList preds;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (o->is_asm()) {
            continue;
        }
        if (m_cg->isCopyOR(o) && !handled.find(o->id())) {
            //For conservative optimizing, the copy-value
            //sr cannot be dedicated register.
            //e.g:
            //  1.d5 = [xxx]
            //  2.d4 = d5
            //  3.br r7
            //=>
            //  d4 = [xxx]
            //  br r7 //WRONG!! d5 is incorrectly, because
            //          //d5 is returnval register.
            //NOTICE: stmt 2. must keep there, since the removal
            //  will be consider during redundant def eliminiation.
            //  we dont care that here.

            //CASE
            //  src =     //cand
            //  ...       //other code
            //  tgt = src //or
            // =>
            //  tgt =     //cand
            //  src = tgt
            //  ...       //other code
            //            //o, has been removed!
            SR * op_src = o->get_opnd(m_cg->computeCopyOpndIdx(o));
            SR * op_tgt = o->get_result(o->get_result_idx(o->get_copy_to()));
            ASSERTN(op_src != nullptr && op_tgt != nullptr &&
                    op_src->is_reg() && op_tgt->is_reg(),
                    ("sr is not a register"));
            preds.clean();
            ddg.getPredsByOrder(preds, o);
            OR * cand = nullptr;
            OR * first_use_point_of_opsrc = nullptr;
            //Finding mergering candidate.
            for (OR * pred = preds.get_tail();
                 pred != nullptr; pred = preds.get_prev()) {
                if (m_cg->mustDef(pred, op_src) &&
                    !m_cg->isMultiResultOR(OR_code(pred)) &&
                    !m_cg->isOpndSameWithResult(nullptr, pred,
                                                nullptr, nullptr)) {
                    cand = pred;
                    break;
                }

                //CASE:When we transfer 'op_tgt' inversely,
                //    'o' could not cross any DEF of USE of 'op_tgt',
                //    and also the 'op_src'.
                if (m_cg->mayDef(pred, op_src) ||
                    m_cg->mayDef(pred, op_tgt) ||
                    m_cg->mayUse(pred, op_tgt)) {
                    break;
                }
                if (m_cg->mayUse(pred, op_src)) {
                    if (OR_is_volatile(o)) {//Can not move o.
                        break;
                    }

                    //CASE:
                    //  r1 = ...
                    //     = r1
                    //  r2 = r1
                    //     = r1
                    //=>
                    //  r2 = ...
                    //  r1 = r2
                    //     = r1
                    //     = r1
                    //Should move o before of 'first_use_point_of_opsrc'
                    first_use_point_of_opsrc = pred;
                }
            } //end for each of preds
            if (cand == nullptr || hasSideEffect(cand)) {
                continue;
            }

            //CASE: Process bus-or
            //e.g:
            //    SR254(a4)[A1] , :- bus_m2_to_m1 SR232(A4)[A2]
            //    SR238(d2)[D1] , :- copy_m SR254(a4)[A1]
            //=>
            //    SR238(d2)[D1] , :- bus_m2_to_m1 SR232(A4)[A2]
            //So we should ensure the condition via 'SameLike' but is not 'Same'
            if (!m_cg->isSameLikeCluster(o, cand) ||
                !m_cg->isSameCondExec(cand, o, ORBB_orlist(m_bb))) {
                continue;
            }

            //Mergering 'pred' and 'o'
            bool doit = false;
            //Safe copy-value condition checking.
            if (SR_regfile(op_src) == SR_regfile(op_tgt)) {
                doit = true;
            }

            if (SR_regfile(op_tgt) != RF_UNDEF) {
                if (m_cg->isValidRegFile(cand,
                    op_src, SR_regfile(op_tgt), true)) {
                    doit = true;
                } else {
                    doit = false;
                }
            }

            if (doit) { //Actually copy-value
                m_cg->renameResult(cand, op_src, op_tgt, true);
                //For conservative optimizing, the copy-value
                //sr cannot be dedicated register.
                //e.g:
                //    d5 = [xxx]
                //    d4 = d5
                //    br r7
                //=>
                //    d4 = [xxx]
                //    d5 = d4  //New copy operation is needed!
                //    br r7 //WRONG!! d5 is incorrectly,
                //          //since d5 is returnval register.
                //NOTICE: that code can be removed during redundant def elim.
                m_cg->renameOpnd(o, op_src, op_tgt, true);
                m_cg->renameResult(o, op_tgt, op_src, true);
                if (first_use_point_of_opsrc) {
                    ORList orig_op_succs;
                    ORList orig_op_preds;
                    ddg.get_succs(orig_op_succs, o);
                    ddg.get_preds(orig_op_preds, o);

                    ORBB_orlist(m_bb)->remove(orct);
                    ddg.removeOR(o);

                    ORBB_orlist(m_bb)->insert_before(o,
                        first_use_point_of_opsrc);
                    ddg.appendOR(o);
                    ddg.appendEdge(DEP_HYB, cand, o);
                    ddg.unifyEdge(orig_op_succs, o);
                    ddg.unifyEdge(orig_op_preds, o);
                }

                //Union all dependences of 'o' to 'cand', and all
                //dependences of 'cand' to 'o'.
                ORList tmp, op_all_edges, cand_all_edges;
                ddg.get_succs(tmp, o);
                op_all_edges.append_tail(tmp);
                ddg.get_preds(tmp, o);
                op_all_edges.append_tail(tmp);

                ddg.get_succs(tmp, cand);
                cand_all_edges.append_tail(tmp);
                ddg.get_preds(tmp, cand);
                cand_all_edges.append_tail(tmp);

                ddg.unifyEdge(op_all_edges, cand);
                ddg.unifyEdge(cand_all_edges, o);
                handled.append(o->id()); //Mark o map idx scheduled.
                is_resch = true;
            }
        } //end if OR is copy-or
    } //end for each OR in ORBB
    return is_resch;
}


//Return ture if there are ors removed.
bool LRA::elimRedundantCopy(bool gra_enable)
{
    bool removed = false;
    if (!HAVE_FLAG(m_opt_phase, (UINT)LRA_OPT_RCPE)) {
        return false;
    }
    ORCt * next_orct;
    ORCt * orct;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        bool doit = false;
        if (m_cg->isCopyOR(o)) {
            SR * cp_to_sr = o->get_copy_to();
            SR * cp_from_sr = o->get_opnd(m_cg->computeCopyOpndIdx(o));
            if (m_cg->isSREqual(cp_to_sr, cp_from_sr)) {
                if (gra_enable) {
                    if (cp_to_sr->is_global() &&
                        m_bb->isLiveOut(cp_to_sr)) {
                        m_bb->setLiveOut(cp_from_sr);
                    }
                }
                doit = true;
            }
        } else if (OR_is_nop(o)) {
            doit = true;
        }

        if (doit) {
            ORBB_orlist(m_bb)->remove(orct);
            removed = true;
        }
    }
    return removed;
}


bool LRA::hasMemSideEffect(OR * o)
{
    return (o->is_asm() ||
            OR_is_call(o) ||
            OR_is_br(o) ||
            OR_is_side_effect(o) ||
            OR_is_volatile(o));
}


bool LRA::hasSideEffect(OR * o)
{
    return  (o->is_mem() && !o->is_load()) ||
            OR_is_call(o) ||
            o->getCode() == OR_spadjust ||
            OR_is_br(o) ||
            (o->is_fake() && !o->is_bus()) ||
            OR_is_side_effect(o) ||
            OR_is_volatile(o);
}


bool LRA::hasSideEffectResult(OR * o)
{
    if (hasSideEffect(o)) {
        return true;
    }
    SR * sr = nullptr;
    //Check all result srs.
    for (UINT i = 0; i < o->result_num(); i++) {
        sr = o->get_result(i);
        if (sr->is_global() &&
            (ORBB_liveout(m_bb).is_contain(SR_sregid(sr)))) {
            return true;
        }
        if (SR_is_dedicated(sr)) {
            return true;
        }
    }
    return false;
}


//The store is redundant if there is not any DEF for
//local store value, expect the dedicated sr.
bool LRA::elimRedundantUse(DataDepGraph & ddg)
{
    bool is_resch = false;
    ORList preds;
    ORCt * next_orct = nullptr;
    ORCt * orct;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (OR_is_side_effect(o) || OR_is_volatile(o)) {
            continue;
        }
        ddg.get_preds(preds, o);
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * opnd = o->get_opnd(i);
            if (opnd->is_reg() &&
                !opnd->is_global() &&
                !SR_is_dedicated(opnd)) { //Local sr should has DEF.
                bool remove = true;
                for (OR * pred = preds.get_head();
                     pred != nullptr; pred = preds.get_next()) {
                    if (m_cg->mayDef(pred, opnd)) {
                        remove = false; //Find the DEF point.
                        break;
                    }
                }
                if (remove) {
                    ddg.chainPredAndSucc(o);
                    ORBB_orlist(m_bb)->remove(orct);
                    ddg.removeOR(o);
                    is_resch = true;
                    break;
                }
            }
        }
    }
    return is_resch;
}


bool LRA::elimRedundantUse(LifeTimeMgr & mgr)
{
    bool resched = false;
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        //CASE 1: Local sr only have USE point.
        //Code Expansion Phase may be generating redundant SR reference.
        if (!SR_is_global(LT_sr(lt)) &&
            !SR_is_dedicated(LT_sr(lt)) &&
            !LT_has_allocated(lt) &&
            LT_pos(lt)->get_first() == LT_FIRST_POS) {

            for(INT i = LT_pos(lt)->get_first();
                i >= 0; i = LT_desc(lt).get_next(i)){
                PosInfo *pi = LT_desc(lt).get(i);
                if(pi != nullptr){
                    ASSERTN(!pi->is_def(),
                            ("Life time does not have any DEF."));
                    OR * o = mgr.getOR(i);
                    ASSERTN(!hasSideEffect(o),
                        ("Undefined value was refered by side effect o."));
                    ORBB_orlist(m_bb)->remove(o);
                    resched = true;
                }
            }
        }
    }
    return resched;
}


bool LRA::elimRedundantRegDef(DataDepGraph & ddg)
{
    bool resched = false;
    if (!HAVE_FLAG(m_opt_phase, (UINT)LRA_OPT_DDE)) {
        return false;
    }
    bool need_recover_ddg = false;
    if (!ddg.is_param_equal(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                            DEP_MEM_ANTI|DEP_SYM_REG)) {
        //diff in RRD
        ddg.pushParam();
        ddg.setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                     DEP_MEM_ANTI|DEP_SYM_REG);
        ddg.reschedul();
        need_recover_ddg = true;
    }
    if (elimRedundantCopy(m_cg->isGRAEnable())) {
        //BUG:
        //1. d10 = [x]
        //2. d10 = d10
        //3.     = d10
        //statment 1 was removed firstly, then is statement 2,
        //in such case, DEF of statement 3 is lost.
        ddg.reschedul();
        resched = true;
    }

    ORList succs;
    ORCt * orct = nullptr;
    ORCt * next_orct = nullptr;
    Vector<OR*> second_def; //vector is in dense storage.
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (hasSideEffect(o)) {
            //On here, we will not use 'hasSideEffectResult' because
            //more effective estimations would be performed after a while.
            //Means, even if the result is global o dedicated register,
            //it also can be removed sometimes.
            continue;
        }
        ddg.getSuccsByOrder(succs, o);
        bool has_use = false;
        bool has_second_def = false;//Indicate output dep.
        second_def.clean();
        for (UINT i = 0; i < o->result_num(); i++) {
            for (OR * succ = succs.get_head();
                 succ != nullptr; succ = succs.get_next()) {
                if (m_cg->mayUse(succ, o->get_result(i))) {
                    has_use = true;
                    goto FIN;
                }
                if (m_cg->mustDef(succ, o->get_result(i))) {
                    second_def.set(i, succ);
                    has_second_def = true;
                    //Terminiate scanning if we meet the 2th OUT-DEP o.
                    break;
                }
            }
        }
FIN:
        if (has_second_def && !has_use) {
            //Have only output dep. Implict repesent
            //second DEF o in preceding of third USE o.
            bool doit = true;
            for (UINT i = 0; i < o->result_num(); i++) {
                //CASE 1:
                //    d10 = ... (p1) Cannot be removed, both them
                //                   must execute under same condition.
                //    ...
                //    d11 = ... (p2)
                if (second_def[i] != nullptr &&
                    !(m_cg->isSameCondExec(o, second_def.get(i),
                                           ORBB_orlist(m_bb)) &&
                      m_cg->isSameCluster(o, second_def.get(i)))) {
                    doit = false;
                    break;
                }

                //CASE 2:
                //  gsr269(a4)[A1] , sr131(rflags):-
                //          add_m sr97(p0)[P1] sr244(a6)[A1] gsr269(a4)[A1]
                //  gsr270(d7)[D1] , sr131(rflags):-
                //          add_m sr97(p0)[P1] sr244(a6)[A1] gsr240(d11)[D2]
                //  ## Although gsr269 has not OUT-DEF, but it lives out of bb!
                //     So the o cannot be removed!
                //  ## And similarly for dedicated sr.
                if (second_def[i] == nullptr &&
                    (m_bb->isLiveOut(o->get_result(i)) ||
                     SR_is_dedicated(o->get_result(i)))) {
                    doit = false;
                    break;
                }
            }

            if (doit) {
                //Cleanup live-info
                ddg.chainPredAndSucc(o);
                ORBB_orlist(m_bb)->remove(orct);
                ddg.removeOR(o);
                ORBB_orlist(m_bb)->get_head(&next_orct);
                resched = true;
            }
            continue;
        }
        if (!has_use &&
            (!hasSideEffectResult(o) ||
             (o->result_num() == 1 && //Single def
              o->get_result(0) == m_cg->genReturnAddr() &&

              //ret_addr register's value
              //may be reserved in local stack
              //memory at the beginning of func.
              SR_is_global(o->get_result(0)) &&
              !m_bb->isLiveOut(o->get_result(0))))) {

            //'o' is redundant even if return-address
            //register is result register, since it was
            //used just as a local-sr.
            ddg.chainPredAndSucc(o);
            ORBB_orlist(m_bb)->remove(orct);
            ddg.removeOR(o);
            ORBB_orlist(m_bb)->get_head(&next_orct);
            resched = true;
        }
    }

     if (need_recover_ddg) {
        ddg.popParam();
        ddg.reschedul();
    }
    return resched;
}


//Build copy operation
void LRA::genCopyOR(CLUST clust, UNIT unit, SR * src, SR * tgt, SR * pd,
                    ORList & ors)
{
    m_cg->buildCopyPred(clust, unit, tgt, src, pd, ors);

    //Do not aware cluster info before it was available.
    if (HAVE_FLAG(m_cur_phase, PHASE_CA_DONE)) {
        m_cg->setCluster(ors, clust);
    }
}


bool LRA::canOpndCrossCluster(OR * o)
{
    return o->is_bus() || o->is_asm();
}


bool LRA::canResultCrossCluster(OR * o)
{
    return o->is_bus() || o->is_asm();
}


//If 'reassign_regfile' is true, the regfile of
//local-SR will be reassiged afterwhile.
CLUST LRA::findOpndMustBeCluster(IN OR * o, bool reassign_regfile)
{
    //Inspecting register cluster info.
    CLUST opnd_clust = CLUST_UNDEF;
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (!isSRAffectClusterAssign(sr)) {
            continue;
        }
        if (sr->getPhyReg() != REG_UNDEF) {
            CLUST cur_clust = m_cg->mapReg2Cluster(sr->getPhyReg());
            if (cur_clust != CLUST_UNDEF && opnd_clust != CLUST_UNDEF) {
                if (!canOpndCrossCluster(o)) {
                    ASSERTN(opnd_clust == cur_clust,
                            ("Operands cross multi clusters"));
                }
            }
            if (cur_clust != CLUST_UNDEF) {
                opnd_clust = cur_clust;
            }
        } else if (sr->getRegFile() != RF_UNDEF && !reassign_regfile) {
            CLUST cur_clust = m_cg->mapRegFile2Cluster(sr->getRegFile(), sr);
            if (cur_clust != CLUST_UNDEF && opnd_clust != CLUST_UNDEF) {
                if (!canOpndCrossCluster(o)) {
                    ASSERTN(opnd_clust == cur_clust,
                            ("Operands cross multi clusters"));
                }
            }
            if (cur_clust != CLUST_UNDEF) {
                opnd_clust = cur_clust;
            }
        }
    }
    return opnd_clust;
}


bool LRA::isSRAffectClusterAssign(SR * sr)
{
    if (!sr->is_reg() || sr->is_rflag() || sr->is_pred() || m_cg->isSP(sr)) {
        return false;
    }
    return true;
}


//If 'reassign_regfile' is true, the regfile of
//local-SR will be reassiged afterwhile.
CLUST LRA::findResultMustBeCluster(IN OR * o, bool reassign_regfile)
{
    //Inspecting register cluster info.
    CLUST result_clust = CLUST_UNDEF;
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (!isSRAffectClusterAssign(sr)) {
            continue;
        }
        if (sr->getPhyReg() != REG_UNDEF) {
            CLUST cur_clust = m_cg->mapReg2Cluster(sr->getPhyReg());
            if (cur_clust != CLUST_UNDEF && result_clust != CLUST_UNDEF) {
                if (!canResultCrossCluster(o)) {
                    ASSERTN(result_clust == cur_clust,
                            ("Operands cross multi clusters"));
                }
            }
            if (cur_clust != CLUST_UNDEF) {
                result_clust = cur_clust;
            }
        } else if (sr->getRegFile() != RF_UNDEF && !reassign_regfile) {
            CLUST cur_clust = m_cg->mapRegFile2Cluster(sr->getRegFile(), sr);
            if (cur_clust != CLUST_UNDEF && result_clust != CLUST_UNDEF) {
                if (!canResultCrossCluster(o)) {
                    ASSERTN(result_clust == cur_clust,
                            ("Operands cross multi clusters"));
                }
            }
            if (cur_clust != CLUST_UNDEF) {
                result_clust = cur_clust;
            }
        }
    }
    return result_clust;
}


CLUST LRA::findResultExpectCluster(IN OR * o, IN DataDepGraph & ddg)
{
    //Inspecting register cluster info.
    CLUST result_exp_clust = CLUST_UNDEF;
    //Try to find info among succs.
    ORList succs;
    ddg.getSuccsByOrder(succs, o);
    if (succs.get_elem_count() != 0) {
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (!isSRAffectClusterAssign(sr)) {
                continue;
            }
            for (OR * succ = succs.get_head();
                 succ != nullptr; succ = succs.get_next()) {
                if (m_cg->mustUse(succ, sr)) {
                    if (OR_is_bus(succ)) {
                        result_exp_clust = m_cg->mapSR2Cluster(succ, sr);
                    } else if (OR_is_asm(succ)
                               // || OR_is_fake(succ) //??necessary
                               ) {
                        ; //Cluster of Asm-OR is the first cluster as default.
                    } else {
                        result_exp_clust = m_cg->computeORCluster(succ);
                    }
                    if (result_exp_clust != CLUST_UNDEF) {
                        goto FIN;
                    }
                }
            }
        }
    }
FIN:
    return result_exp_clust;
}


CLUST LRA::findOpndExpectCluster(IN OR * o, IN DataDepGraph & ddg)
{
    CLUST opnd_exp_clust = CLUST_UNDEF;
    ORList preds;
    //Try to find info among preds.
    ddg.getPredsByOrder(preds, o);
    if (preds.get_elem_count() == 0) { return opnd_exp_clust; }

    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (!isSRAffectClusterAssign(sr)) { continue; }

        for (OR * pred = preds.get_tail();
             pred != nullptr; pred = preds.get_prev()) {
            if (!m_cg->mustDef(pred, sr)) { continue; }

            if (OR_is_bus(pred)) {
                opnd_exp_clust = m_cg->mapSR2Cluster(pred, sr);
            } else if (OR_is_asm(pred)
                       // || OR_is_fake(succ) //??necessary
                       ) {
                ; //Cluster of ASM-OR is the first cluster as default.
            } else {
                opnd_exp_clust = m_cg->computeORCluster(pred);
            }

            if (opnd_exp_clust != CLUST_UNDEF) {
                goto FIN;
            }
        }
    }

FIN:
    return opnd_exp_clust;
}


void LRA::chooseExpectClust(
        ORList const ors[CLUST_NUM],
        OUT List<CLUST> & expcls)
{
    //Sort by increasing order of number of OR.
    for (UINT i = CLUST_UNDEF + 1; i < CLUST_NUM; i++) {
        CLUST tmp = expcls.get_head();
        if (m_cg->isBusCluster((CLUST)i)) {
            continue;
        }
        for (UINT j = 0; j < expcls.get_elem_count();
             j++, tmp = expcls.get_next()) {
            if (ors[i].get_elem_count() <= ors[tmp].get_elem_count()) {
                break;
            }
        }
        if (tmp == CLUST_UNDEF) { //Order list is empty.
            expcls.append_tail((CLUST)i);
        } else {
            expcls.insert_before((CLUST)i, (CLUST)tmp);
        }
    }
}


//Return true if 'clust' is reasonable for clustering.
//The consideration must be aware of register pressure estimated.
bool LRA::isReasonableCluster(CLUST clust, List<OR*> & es_op_list,
                              DataDepGraph & ddg,
                              RegFileSet const& is_regfile_unique)
{
    DUMMYUSE(ddg);
    //All elements must be changable.
    for (OR * o = es_op_list.get_head();
         o != nullptr; o = es_op_list.get_next()) {
        if (!m_cg->changeORCluster(o, clust, &is_regfile_unique, true)) {
            return false;
        }
    }
    if (clust == CLUST_UNDEF || es_op_list.get_elem_count() <= 0) {
        return false;
    }
    return true;
}


//Remove those OR which should not particpate in the cluster assignation.
void LRA::deductORCrossBus(DataDepGraph & ddg)
{
    ORCt * orct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&orct); o != nullptr;
         o = ORBB_orlist(m_bb)->get_next(&orct)) {
        if (o->getCode() == OR_spadjust ||
            OR_is_cond_br(o) ||
            OR_is_uncond_br(o)) {
            ddg.removeOR(o);
        }
    }
}


//Attempt to assign 'expected_clust' to each OR in 'orlist'.
bool LRA::tryAssignCluster(CLUST exp_clust,
                           List<OR*> * orlist,
                           RegFileSet const& is_regfile_unique,
                           ORList ors[CLUST_NUM])
{
    ASSERTN(orlist, ("group is empty"));
    OR * o;
    for (o = orlist->get_head(); o != nullptr; o = orlist->get_next()) {
        if (!m_cg->changeORCluster(o, exp_clust, &is_regfile_unique, true)) {
            return false;
        }
    }

    //Actually perform change.
    for (o = orlist->get_head(); o != nullptr; o = orlist->get_next()) {
        if (!m_cg->changeORCluster(o, exp_clust, &is_regfile_unique, false)) {
            ASSERTN(0, ("Change failed"));
        }

        ors[exp_clust].append_tail(o);
    }

    return true;
}


//Choose default cluster.
CLUST LRA::chooseDefaultCluster(OR *)
{
    ASSERT0_DUMMYUSE((CLUST_UNDEF + 1) != CLUST_NUM);
    return (CLUST)(CLUST_UNDEF + 1);
}


//Partitioning operations according to data dependence relations.
//Return true if ORs that have been generated need to be rescheduled.
bool LRA::PureAssignCluster(IN OR * o,
                            IN OUT ORCt **,
                            IN DataDepGraph &,
                            RegFileSet const&)
{
    ASSERT0(!isMultiCluster());
    OR_clust(o) = CLUST_FIRST;
    return false;
}


//Partitioning OPs into groups in heurisit method.
bool LRA::partitionGroup(DataDepGraph & ddg,
                         RegFileSet const& is_regfile_unique)
{
    #define GROUP_DIFF_THRESHOLD 0.85
    if (!HAVE_FLAG(m_opt_phase, LRA_OPT_PG)) {
        return false;
    }
    show_phase("Partition Group");
    GroupMgr gm(m_bb, m_cg);
    DefMiscBitSetMgr sm;
    DefSBitSet visited(sm.getSegMgr()); //Used to speedup algo.
    ORList proclist;
    bool is_resch = false; //need rebuild ddg
    bool change_ddg_param = false;
    if (!ddg.is_param_equal(DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                            DEP_MEM_ANTI|DEP_SYM_REG)) {
        ddg.pushParam();
        ddg.setParam(DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                     DEP_MEM_ANTI|DEP_SYM_REG);
        ddg.reschedul();
        change_ddg_param = true;
    }

    DataDepGraph * tmpddg = m_cg->allocDDG();
    tmpddg->init(nullptr);
    tmpddg->clone(ddg);
    deductORCrossBus(*tmpddg);
    if (tmpddg->getVertexNum() <= 0) {
        if (change_ddg_param) {
            ddg.popParam();
            ddg.reschedul();
            is_resch = false;
        }
        delete tmpddg;
        return is_resch;
    }
    tmpddg->predigest();

    ORMap split_op_map;
    INT last_gp_idx = 1; //group start from idx 1.

    //Staticitics bottom nodes which without successors.
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        xcom::Vertex * v = tmpddg->getVertex(o->id());
        if (!v) { //node has been removed
            continue;
        }
        if (VERTEX_out_list(v) == nullptr) {
            proclist.append_tail(o);
            gm.addOR(o, last_gp_idx);
            last_gp_idx++;
            visited.bunion(o->id());
        }
    }

    //Firstly, find all completely indepent groups.
    while (proclist.get_elem_count() > 0) {
        OR * o = proclist.remove_head();
        ASSERTN(gm.get_or_group(o) != 0, ("o has not yet grouped"));
        xcom::Vertex * v = tmpddg->getVertex(o->id());
        ASSERTN(v, ("o is not node of graph"));

        //Process all preds.
        xcom::EdgeC * el = v->getInList();
        while (el) {
            xcom::Vertex * from = el->getFrom();
            OR * pred = tmpddg->getOR(VERTEX_id(from));
            if (visited.is_contain(pred->id())) {
                if (gm.get_or_group(pred) != gm.get_or_group(o)) {
                    //Mergering current group into group which contains 'pred'
                    gm.union_group(gm.get_or_group(pred), gm.get_or_group(o));
                }
            } else {
                gm.addOR(pred, gm.get_or_group(o));
                visited.bunion(pred->id());
                proclist.append_tail(pred);
            }
            el = EC_next(el);
        }
    }
    ASSERTN (gm.get_groups() > 0, ("not any group"));

    ORList ors[CLUST_NUM];

    //Secondly, traversing each groups to find
    //the unique cluster can be assigned.

    //If OR not process group. vector is in dense storage
    BitSet gp_need_process; //it is a dense storage.
    gp_need_process.bunion(gm.get_last_group());
    gp_need_process.diff(gm.get_last_group());
    for (INT i = 1; i <= gm.get_last_group(); i++) {
        List<OR*> * orlist = gm.get_orlist_in_group(i);
        if (orlist == nullptr) {
            continue;
        }
        gp_need_process.bunion(i);

        //Find the must be cluster.
        CLUST must_clust = CLUST_UNDEF;
        for (OR * o = orlist->get_head(); o != nullptr;
             o = orlist->get_next()) {
            CLUST cur_or_clust = CLUST_UNDEF;
            CLUST opnd_must_clust = findOpndMustBeCluster(o, true);
            CLUST result_must_clust = findResultMustBeCluster(o, true);
            if (opnd_must_clust != CLUST_UNDEF) {
                cur_or_clust = opnd_must_clust;
            } else if (result_must_clust != CLUST_UNDEF) {
                cur_or_clust = result_must_clust;
            }

            if (cur_or_clust != CLUST_UNDEF &&
                must_clust != CLUST_UNDEF) {
                if (cur_or_clust != must_clust) {
                    //There are miscellaneous clusters in this group.
                    //And we need do more considerations.
                    gp_need_process.diff(i);
                    break;
                }
            } else if (cur_or_clust != CLUST_UNDEF) {
                must_clust = cur_or_clust;
            }
        }

        //Assigning the must be cluster for group.
        if (gp_need_process.is_contain(i) && must_clust != CLUST_UNDEF) {
            if ((((float)orlist->get_elem_count()) /
                 ((float)tmpddg->getVertexNum())) < GROUP_DIFF_THRESHOLD) {
                if (isReasonableCluster(must_clust, *orlist,
                        *tmpddg, is_regfile_unique)) {
                    is_resch = true;
                    for (OR * o = orlist->get_head(); o != nullptr;
                         o = orlist->get_next()) {
                        if (!m_cg->changeORCluster(o, must_clust,
                                                   &is_regfile_unique, false)) {
                            ASSERTN(0, ("Change failed"));
                        }
                        ors[must_clust].append_tail(o);
                    }
                }
            }
            gp_need_process.diff(i);
        }
    }

    //Thirdly, assign group accroding to resource pressure.
    List<List<OR*>*> tgroups;
    List<INT> tgroups_idx;
    List<List<OR*>*> ordered_groups;
    List<INT> ordered_groups_idx;

    //Sort out all groups which expected to be assigned.
    for (INT i = 1; i <= gm.get_last_group(); i++) {
        if (!gp_need_process.is_contain(i)) {
            continue;
        }
        List<OR*> * orlist = gm.get_orlist_in_group(i);
        ASSERTN(orlist, ("group is empty"));
        if ((((float)orlist->get_elem_count()) /
             ((float)tmpddg->getVertexNum())) > GROUP_DIFF_THRESHOLD) {
            continue; //Ununiform group partitioning.
        }
        tgroups.append_tail(orlist);
        tgroups_idx.append_tail(i);
    }

    //Sort o-group by descendent order of resource pressure.
    INT tors_idx = tgroups_idx.get_head();
    for (List<OR*> * tors = tgroups.get_head();
         tors != nullptr;
         tors = tgroups.get_next(), tors_idx = tgroups_idx.get_next()) {
        List<OR*> * point = nullptr;
        INT point_idx = -1;
        INT oridx = ordered_groups_idx.get_head();
        for (List<OR*> * orops = ordered_groups.get_head();
             orops != nullptr;
             orops = ordered_groups.get_next(),
             oridx = ordered_groups_idx.get_next()) {
            if (tors->get_elem_count() >= orops->get_elem_count()) {
                point = orops;
                point_idx = oridx;
                break;
            }
        }

        if (point == nullptr) {
            ordered_groups.append_tail(tors);
            ordered_groups_idx.append_tail(tors_idx);
        } else {
            ASSERTN(point_idx > 0, ("Illegal group idx"));
            ordered_groups.insert_before(tors, point);
            ordered_groups_idx.insert_before(tors_idx, point_idx);
        }
    }

    //Assigning group with the expected cluster.
    INT opidx = ordered_groups_idx.get_head();
    for (List<OR*> * orlist = ordered_groups.get_head();
         orlist != nullptr; orlist = ordered_groups.get_next(),
         opidx = ordered_groups_idx.get_next()) {
        List<CLUST> exp_clust_with_prio;
        chooseExpectClust(ors, exp_clust_with_prio);
        for (CLUST expcl = exp_clust_with_prio.get_head();
             expcl != CLUST_UNDEF; expcl = exp_clust_with_prio.get_next()) {
            if (tryAssignCluster(expcl, orlist, is_regfile_unique, ors)) {
                is_resch = true;
                gp_need_process.diff(opidx);
                break;
            }
        }
    }

    //Fourth, inserting inter-cluster communication operation.
    ORList * split_ors = split_op_map.getORList();
    for (OR * splor = split_ors->get_head();
         splor != nullptr; splor = split_ors->get_next()) {
        ASSERTN(0, ("TODO: Find a Pattern for inter-cluster o generation!"));
        if (m_cg->computeORCluster(splor) == CLUST_UNDEF) {
            ORCt * next_orct;
            is_resch |= PureAssignCluster(splor, &next_orct, ddg,
                                          is_regfile_unique);
            ASSERTN(m_cg->computeORCluster(splor) != CLUST_UNDEF,
                    ("Illegal info"));
        }

        CLUST splop_clust = m_cg->computeORCluster(splor);
        List<OR*> cls[CLUST_NUM];
        List<OR*> * clusted_ors = split_op_map.getOR2ORList(splor);
        OR * tmp = nullptr;
        for (OR * o = clusted_ors->get_head();
             o != nullptr; o = clusted_ors->get_next()) {
            tmp = o;
            cls[m_cg->computeORCluster(o)].append_tail(o);
        }

        //Insert BUS operation.
        List<SR*> used_srs;
        for (UINT i = 0; i < splor->result_num(); i++) {
            SR * sr = splor->get_result(i);
            if (!isSRAffectClusterAssign(sr)) {
                continue;
            }
            if (m_cg->mayUse(tmp, sr)) {
                used_srs.append_tail(sr);
            }
        }

        ASSERTN(used_srs.get_elem_count() == 1,
            ("Only support one to one copy."));

        SR * used_sr = used_srs.get_head();
        for (UINT i = CLUST_UNDEF + 1; i < CLUST_NUM; i++) {
            if (splop_clust == (CLUST)i) { continue; }

            if (m_cg->isBusCluster((CLUST)i)) {
                ASSERT0(cls[i].get_elem_count() == 0);
                continue;
            }

            if (cls[i].get_elem_count() > 0) {
                //Build copy operation, and it will be
                //replaced to bus-o after while.
                ORList ors2;
                CLUST cp_clust = (CLUST)i;
                UNIT cp_unit = m_cg->computeORUnit(cls[i].get_head())->
                                                   checkAndGet();
                SR * new_used_sr = genNewReloadSR(used_sr,
                                                  SR_spill_var(used_sr));

                genCopyOR(cp_clust, cp_unit, used_sr, new_used_sr,
                          m_cg->getTruePred(), ors2);

                m_cg->setCluster(ors2, cp_clust);
                ORBB_orlist(m_bb)->insert_after(ors2, splor);

                //Replacing followed reload.
                for (OR * succ = cls[i].get_head();
                     succ != nullptr; succ = cls[i].get_next()) {
                    m_cg->renameOpnd(succ, used_sr, new_used_sr, false);
                }

                is_resch = true;
            }
        }
    }

    goto FIN;

    //Fifth, partition graph into partial group by selecting
    //the most independent group.
    //delete tmpddg;
    //return is_resch;
FIN:
    if (change_ddg_param) {
        ddg.popParam();
        ddg.reschedul();
        is_resch = false;
    }

    delete tmpddg;
    return is_resch;
}


bool LRA::preOpt(IN OUT DataDepGraph & ddg)
{
    bool again;
    INT count = 0;
    ORIdTab handled;
    bool done = false;
    do {
        again = false;
        show_phase("---PREOPT:Eliminate Redundant Code...");
        if (coalescing(ddg, true)) {
            done = true;
            again = true;
        }
        show_phase("---PREOPT:Eliminate Redundant Code Inverse...");
        if (cse(ddg, handled)) {
            done = true;
            again = true;
        }
        show_phase("---PREOPT:Eliminate Dead Def...");
        if (elimRedundantRegDef(ddg)) {
            done = true;
            again = true;
            handled.clean();//try more times.
        }
        count++;
    } while (again && count < 100);

    ASSERTN(!again, ("Unexpected exit."));

    return done;
}


//Return ture if the optimal schedules were found.
bool LRA::optimal_partition(DataDepGraph & ddg,
                            RegFileSet & is_regfile_unique)
{
    preOpt(ddg);
    if (ORBB_ornum(m_bb) == 0) {
        return true;
    }
    CHAR * name = "zsimpddg.vcg";
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!", name));
    fprintf(h, "\nPU:%s,ORBB:%d,len:%d\n",
            m_rg->getRegionName(), m_bb->id(), ORBB_ornum(m_bb));
    fclose(h);

    ddg.simplifyGraph();
    //ddg.dump_graph(DDG_DUMP_OP_INFO, -1, name);
    InstructionPartition<xcom::RMat, xcom::Rational> ip(m_cg, m_bb, &ddg,
                                                        &is_regfile_unique);
    //InstructionPartition<xcom::FloatMat, xcom::Float> ip;
    return ip.partition();
}


//Partition operations into cluster and
//minimize inter-cluster communication.
//'bb': input ORBB with number of OR.
//'ddg': data dependence graph of 'bb'.
//'is_regfile_unique': record property of REGFILE of sr.
//'partitioning': perform clustering partitioning with method of
//    maximum flow and minimum cut.
//NOTICE:
//    1.REGFILE of sr may be changed.
void LRA::assignCluster(DataDepGraph & ddg,
                        RegFileSet const& is_regfile_unique,
                        bool partitioning)
{
    bool resch = false;
    //Assign designated/dedicated cluster.
    assignDedicatedCluster();
    if (partitioning && m_cg->isAssignClust(m_bb)) {
        partitionGroup(ddg, is_regfile_unique);
        ASSERTN(HAVE_FLAG(ddg.get_param(), DEP_REG_ANTI),
                ("at least reg-anti must be set"));
    }

    ORCt * next_orct;
    ORCt * orct;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (OR_clust(o) != CLUST_UNDEF) {
            continue;
        }
        resch |= PureAssignCluster(o, &next_orct, ddg, is_regfile_unique);
    }

    if (isOpt() && resch) {
        ddg.reschedul();
    }

    #ifdef _DEBUG_
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        if (OR_clust(o) == CLUST_UNDEF) {
            ASSERTN(0, ("o:%d is unclusted", o->id()));
        }
    }
    #endif
}


//Return true if all lifetime assigned register.
//'uncolored_list' : sort by descent order of priority.
bool LRA::computePrioList(List<LifeTime*> & prio_list,
                          List<LifeTime*> & uncolored_list,
                          InterfGraph & ig,
                          LifeTimeMgr & mgr,
                          RegFileGroup * rfg)
{
    bool all_assigned = true;
    for (LifeTime * lt = prio_list.get_head();
         lt != nullptr; lt = prio_list.get_next()) {
        if (LT_has_allocated(lt)) { continue; }

        if (SR_is_global(LT_sr(lt))) {
            //GSR may not assign physical register sometimes.
            //ASSERTN(LT_has_allocated(lt),
            //       ("Global register has not allocated"));
            continue;

            //May be GSR only to be used within basic block?
            //Treats it as a local SR?
            //RemoveGSRLivein(bb, sr);
            //RemoveGSRLiveout(bb, sr);
        }

        if (!assignRegister(lt, ig, mgr, rfg)) {
            //No enough register assign to 'lt'.
            all_assigned = false;
            uncolored_list.append_tail(lt);//Sort by descent priority order.
        }
    }
    return all_assigned;
}


bool LRA::verifyUsableRegSet(LifeTimeMgr & mgr)
{
    LifeTimeVecIter c;
    for (LifeTime * lt = mgr.getFirstLifeTime(c);
         lt != nullptr; lt = mgr.getNextLifeTime(c)) {
        if (LT_has_allocated(lt)) {
            continue;
        }
        RegSet const* usable_reg_set = mgr.getUsableRegSet(lt);
        ASSERT0(usable_reg_set);
        if (usable_reg_set->get_elem_count() == 0) {
            ASSERTN(0, ("Empty usable register set for LT:%d!", lt->id()));
        }
    }
    return true;
}


void LRA::verifyRegFileForOpnd(OR * o, INT opnd, bool is_result)
{
    if (o->getCode() == OR_spadjust || o->is_asm()) {
        return;
    }
    SR * sr = is_result ? o->get_result(opnd) : o->get_opnd(opnd);
    if (!sr->is_reg()) {
        return;
    }
    ASSERTN(m_cg->isValidRegFile(o->getCode(), opnd, sr->getRegFile(), is_result),
           ("illegal regfile for No.%d %s of OR(%s)",
           opnd, is_result ? "result" : "operand", o->getCodeName()));
}


void LRA::verifyRegFile()
{
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        //Verify results
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            verifyRegFileForOpnd(o, i, true);
        }
        for (i = 0; i < o->opnd_num(); i++) {
            verifyRegFileForOpnd(o, i, false);
        }
    }
}


bool LRA::verifyCluster()
{
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        ASSERTN(OR_clust(o) != CLUST_UNDEF,
               ("OR[%d]%s's cluster info is none", o->id(), o->getCodeName()));
    }
    return true;
}


//Return the OR with uncolored SR.
bool LRA::isAllAllocated(IN OUT OR ** wop)
{
    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            if (o->get_result(i)->is_reg() &&
                SR_phy_reg(o->get_result(i)) == REG_UNDEF) {
                if (wop != nullptr) {
                    *wop = o;
                }
                return false;
            }
        }

        for (i = 0; i < o->opnd_num(); i++) {
            if (o->get_opnd(i)->is_reg() &&
                SR_phy_reg(o->get_opnd(i)) == REG_UNDEF) {
                if (wop != nullptr) {
                    *wop = o;
                }
                return false;
            }
        }
    }
    return true;
}


//Do verification while LRA finish.
bool LRA::verify(LifeTimeMgr & mgr, InterfGraph & ig)
{
    if (ORBB_ornum(m_bb) <= 0) {
        return true;
    }
    mgr.recreate(m_bb, true, true);
    ASSERT0(mgr.verifyLifeTime());
    mgr.computeUsableRegs();

    ig.destroy();
    ig.init(m_bb);
    ig.build(mgr);

    OR * tor;
    DUMMYUSE(tor);
    ASSERTN(isAllAllocated(&tor), ("SR without register assigned!"));

    for (OR * o = m_bb->getFirstOR(); o != nullptr; o = m_bb->getNextOR()) {
        ASSERTN(o->getBB() == m_bb,
                ("OR[%d]%s not in bb", o->id(), o->getCodeName()));
        ASSERTN(OR_clust(o) != CLUST_UNDEF,
                ("OR[%d]%s's cluster info is none", o->id(), o->getCodeName()));
    }
    verifyRegFile();
    return true;
}


void LRA::resetClustRegInfo(ClustRegInfo * cri, UINT num)
{
    for (UINT i = 0; i < num; i++) {
        for (UINT j = 0; j < RF_NUM; j++) {
            CRI_regfile_count(cri[i], j) = 1;
        }
    }
}


//Mark all register-SRs to be unique in order to prevent
//the schedulor schedule OR to another cluster at the finial stage.
void LRA::markRegFileUnique(RegFileSet & is_regfile_unique)
{
    ORCt * orct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&orct); o != nullptr;
         o = ORBB_orlist(m_bb)->get_next(&orct)) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            if (o->get_result(i)->is_reg()) {
                is_regfile_unique.bunion(SR_sregid(o->get_result(i)));
            }
        }
        for (i = 0; i < o->opnd_num(); i++) {
            if (o->get_opnd(i)->is_reg()) {
                is_regfile_unique.bunion(SR_sregid(o->get_opnd(i)));
            }
        }
    }
}


//Schedule function unit and considering resource conflict.
//Return true if REGFILE need to be reassigned.
//NOTICE: Info of cluster of o is necessary.
bool LRA::cyc_estimate(IN DataDepGraph & ddg,
                       IN OUT BBSimulator * sim,
                       IN OUT RegFileSet & is_regfile_unique)
{
    if (sim == nullptr) { return false; }

    show_phase("---Cyc Estimate");
    bool change_ddg_param = false;
    //Do not need travsering each operation, we only need Data
    //Dependence Info, but is not Value Computing Info.
    if (!ddg.is_param_equal(DEP_MEM_FLOW|DEP_MEM_OUT|DEP_CONTROL|DEP_REG_ANTI|
                            DEP_MEM_ANTI|DEP_SYM_REG)) {
        //diff in cd, phy_reg_dep, RRD
        ddg.pushParam();
        ddg.setParam(DEP_MEM_FLOW|DEP_MEM_OUT|DEP_CONTROL|DEP_REG_ANTI|
                     DEP_MEM_ANTI|DEP_SYM_REG);
        ddg.reschedul();
        change_ddg_param = true;
    }

    //Mark all register-SRs into the unique to prevent
    //schedulor move it to another cluster.
    markRegFileUnique(is_regfile_unique);

    LIS * lis = m_cg->allocLIS(m_bb, &ddg, sim,
                               LIS::SCH_TOP_DOWN | LIS::SCH_CHANGE_SLOT);
    lis->set_unique_regfile(&is_regfile_unique);
    lis->schedule();
    if (change_ddg_param) {
        ddg.popParam();
        ddg.reschedul();
    }
    delete lis;
    return false;
}


//Scheduling function unit iteratively.
bool LRA::schedulFuncUnit(IN LifeTimeMgr & mgr,
                          IN DataDepGraph & ddg,
                          IN OUT BBSimulator * sim,
                          IN OUT RegFileSet & is_regfile_unique,
                          ClustRegInfo cri[CLUST_NUM])
{
    if (ORBB_ornum(m_bb) <= 0 || ORBB_ornum(m_bb) > MAX_SCH_OR_BB_LEN) {
        return false;
    }

    if (!HAVE_FLAG(m_opt_phase, LRA_OPT_SCH)) {
        return false;
    }

    show_phase("Func unit scheduling");

    ORList clus[CLUST_NUM];
    bool is_resch = false;
    is_resch |= cyc_estimate(ddg, sim, is_regfile_unique);

    //Reschedul ORs to recover the unsuiteable transformation.
    ORCt * orct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&orct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_next(&orct)) {
        CLUST cl = m_cg->computeORCluster(o);
        ASSERT0(cl != CLUST_UNDEF);
        clus[cl].init();
        clus[cl].append_tail(o);
    }

    for (UINT i = CLUST_UNDEF + 1; i < CLUST_NUM; i++) {
        is_resch |= refineScheduling((CLUST)i, clus[(CLUST)i],
            mgr, ddg, is_regfile_unique, sim);
    }

    if (is_resch) {
        resetClustRegInfo(cri, (UINT)CLUST_NUM);
        RegFileAffinityGraph * rdg = allocRegFileAffinityGraph();
        rdg->init(m_bb);
        rdg->build(mgr, ddg);
        assignRegFile(cri, is_regfile_unique, mgr, ddg, *rdg);
    }
    return true;
}


//Return the registers which have not been allocated within
//life time interval from 'start' to 'end'.
RegSet & LRA::computeUnusedRegSet(REGFILE regfile,
                                  INT start,
                                  INT end,
                                  IN InterfGraph & ig,
                                  OUT RegSet & rs)
{
    ASSERTN(regfile != RF_UNDEF, ("Undefined regfile"));

    //Information should have been initialized in target machine info.
    rs.copy(*tmMapRegFile2RegSetAllocable(regfile));

    List<LifeTime*> lt_lst;
    ig.getLifeTimeList(lt_lst, regfile, start, end);
    for (LifeTime * lt = lt_lst.get_head(); lt != nullptr; lt = lt_lst.get_next()) {
        ASSERTN(LT_has_allocated(lt), ("Life time without register assigned"));
        rs.diff(SR_phy_reg(LT_sr(lt)));
    }
    return rs;
}


//Choose the physical register to be spill location,
//or return REG_UNDEF if not turned up.
REG LRA::chooseAvailSpillLoc(CLUST clust,
                             INT start,
                             INT end,
                             InterfGraph & ig)
{
    RegSet regset;
    List<REGFILE> rfcand;
    m_cg->mapCluster2RegFileList(clust, rfcand);
    xcom::C<REGFILE> * ct;
    for (rfcand.get_head(&ct); ct != rfcand.end(); ct = rfcand.get_next(ct)) {
        REGFILE rf = ct->val();
        ASSERT0(rf > RF_UNDEF && rf < RF_NUM);
        if (!canBeSpillLoc(clust, rf)) {
            continue;
        }

        computeUnusedRegSet(rf, start, end, ig, regset);
        if (!regset.is_empty()) {
            break;
        }
    }

    INT lastone = regset.get_last();
    if (lastone < 0) { return REG_UNDEF; }
    return (REG)lastone;
}


//Check if there are global references of spill location to
//determine whether it can be removed.
bool LRA::checkSpillCanBeRemoved(xoc::Var const* spill_loc)
{
    bool spill_can_be_removed = true;
    if (m_ramgr != nullptr) {
        //Check out if the spill location is used in entire Region.
        RefORBBList * rbl = m_ramgr->getVar2OR()->get(spill_loc);

        //Take a look at the reference of spill-loc.
        if ((rbl != nullptr && rbl->get_elem_count() > 1) ||
            //REGIN only has one BB.
            m_ramgr->getBBList()->get_elem_count() == 1) {
            spill_can_be_removed = false;
        }
    } else {
        //For conservative purpose.
        spill_can_be_removed = false;
    }

    return spill_can_be_removed;
}


//'followed_lds': record reload after spilling, which the
//   access same memory location.
//o: store memory operation
void LRA::findFollowedLoad(OUT ORList & followed_lds,
                           IN OR * o,
                           xoc::Var const* spill_loc,
                           IN OUT bool & spill_can_be_removed,
                           IN DataDepGraph & ddg)
{
    //Check for the case:
    //  [xxx] = t1 //o(SW)
    //  t2 = [xxx] //LD1
    //  ...
    //  t3 = [xxx] //LDn
    //where spilling can NOT be removed.

    ORList succs;
    ddg.getSuccsByOrder(succs, o);
    for (OR * succ = succs.get_head(); succ != nullptr; succ = succs.get_next()) {
        //Reloading does not can be replaced by copy.
        if (hasMemSideEffect(succ)) {
            spill_can_be_removed = false;
            break;
        }

        if (OR_is_load(succ)) {
            if (m_cg->isSameSpillLoc(spill_loc, o, succ)) {
                //Access same spill memory location.
                if (m_cg->isSameCondExec(o, succ, ORBB_orlist(m_bb)) &&
                    m_cg->isSameCluster(o, succ) &&
                    !OR_is_volatile(succ)) {
                    followed_lds.append_tail(succ);
                } else {
                    spill_can_be_removed = false;
                    break;
                }
            }
        } else if (OR_is_store(succ)) {
            if (m_cg->isSameSpillLoc(spill_loc, o, succ)) {
                //New DEF of memloc, namely, MEMOUT dep exist.
                break;
            }
        }
    }
}


SR * LRA::findAvailPhyRegFromLoadList(IN OR * o,
                                      IN OUT bool & spill_can_be_removed,
                                      IN ORList & followed_lds,
                                      IN LifeTimeMgr & mgr,
                                      IN InterfGraph & ig)
{
    SR * spill_loc_sr = nullptr;
    INT start = mgr.getPos(o, true);
    OR * next_followed = nullptr;
    for (OR * ld = followed_lds.get_tail(); ld != nullptr; ld = next_followed) {
        next_followed = followed_lds.get_prev();
        INT end = mgr.getPos(ld, false);
        REG avail_reg = chooseAvailSpillLoc(m_cg->computeORCluster(ld),
            start, end, ig);
        if (avail_reg == REG_UNDEF) {
            followed_lds.remove_tail();
            next_followed = followed_lds.get_tail();
            //Reloading cannot be replaced with copy,
            //thus spilling is necessary.
            spill_can_be_removed = false;
            continue;
        }

        //Find appropriate register to hold value.
        ASSERT0(avail_reg > REG_UNDEF && avail_reg < REG_NUM);
        spill_loc_sr = m_cg->genReg();
        SR_phy_reg(spill_loc_sr) = avail_reg;
        SR_regfile(spill_loc_sr) = tmMapReg2RegFile(avail_reg);
        break;
    }
    return spill_loc_sr;
}


//Hoist spill location for Store operation.
bool LRA::hoistSpillLocForStore(IN OR * o,
                                IN InterfGraph & ig,
                                IN LifeTimeMgr & mgr,
                                IN OUT DataDepGraph & ddg,
                                IN ORCt * orct,
                                IN OUT ORCt ** next_orct)
{
    ASSERT0(orct && next_orct && o->is_store());
    bool is_resch = false;
    if (m_cg->isMultiStore(o->getCode(), -1)) {
        return false;
    }

    xoc::Var const* st_spill_loc = m_cg->computeSpillVar(o);
    if (st_spill_loc == nullptr || //[rn] = xxx, keep the OR for conservative.
        //[.data+N] = xxx, keep the OR for conservative unless IPA
        //tell us the refferrence detail.
        VAR_is_global(st_spill_loc)) {
        return false;
    }

    bool spill_can_be_removed = checkSpillCanBeRemoved(st_spill_loc);
    if (!spill_can_be_removed) { return false; }

    //When we get here, the spill location 'st_spill_loc'
    //is definitly only used in one BB.

    //record reload after spilling, which accessed the same memory location.
    ORList followed_lds;
    findFollowedLoad(followed_lds, o, st_spill_loc, spill_can_be_removed, ddg);

    //Spilling without any reloading succ, remove it.
    if (followed_lds.get_elem_count() == 0) {
        if (spill_can_be_removed) { //'o' should be removed at previously.
            ORCt * prev_orct = orct;
            ORBB_orlist(m_bb)->get_prev(&prev_orct);
            ddg.chainPredAndSucc(o);
            ORBB_orlist(m_bb)->remove(orct);
            ddg.removeOR(o);
            if (prev_orct != nullptr) {
                *next_orct = prev_orct;
            } else {
                ORBB_orlist(m_bb)->get_head(next_orct);
            }
            is_resch = true;
        }
        return is_resch; //process next or
    }

    //Need to update mgr, ig.
    mgr.recreate(m_bb, true, true);
    mgr.computeUsableRegs();
    ig.destroy();
    ig.init(m_bb);
    ig.build(mgr);
    SR * spill_loc_sr = findAvailPhyRegFromLoadList(o,
        spill_can_be_removed, followed_lds, mgr, ig);
    if (spill_loc_sr == nullptr) {
        //Not find available physical register.
        return is_resch;
    }

    //Build copy operation.
    ASSERTN(followed_lds.get_elem_count() > 0, ("what's going on?"));
    CLUST cp_clust = m_cg->computeORCluster(o);
    UNIT cp_unit = m_cg->computeORUnit(o)->checkAndGet();
    SR * pd = o->get_pred();
    SR * or_storeval = o->get_first_store_val();
    ASSERTN(pd != nullptr || !HAS_PREDICATE_REGISTER, ("No predicated sr"));
    ORList ors;
    genCopyOR(cp_clust, cp_unit, or_storeval, spill_loc_sr, pd, ors);
    ASSERT0(ors.get_elem_count() == 1);
    OR * newcp = ors.get_head();
    BBORList * bb_or_lst = ORBB_orlist(m_bb);
    ASSERT0(bb_or_lst != nullptr);
    bb_or_lst->insert_after(newcp, o);
    ddg.appendOR(newcp);

    //Union edges between 'o' and all its 'succ' to new copy-or.
    ORList newcp_lst;
    List<OR*> concern_ors; //record all concerned ORs.
    ddg.get_succs(ors, o);
    concern_ors.append_tail(ors);
    ddg.get_preds(ors, o);
    concern_ors.append_tail(ors);

    //Record new copy-or.
    concern_ors.append_tail(newcp);
    newcp_lst.append_tail(newcp);

    //Replace spilling to copy-or.
    if (spill_can_be_removed) {
        ORCt * prev_orct = orct;
        bb_or_lst->get_prev(&prev_orct);
        ddg.chainPredAndSucc(o);
        bb_or_lst->remove(orct);
        ddg.removeOR(o);
        if (prev_orct != nullptr) {
            *next_orct = prev_orct;
        } else {
            bb_or_lst->get_head(next_orct);
        }
        is_resch = true;
    }

    //Replace reload to copy-or.
    for (OR * f = followed_lds.get_head();
         f != nullptr; f = followed_lds.get_next()) {
        ASSERTN(OR_is_load(f), ("Illegal reload o"));
        ASSERTN(m_cg->computeORCluster(f) == cp_clust,
                ("Placed in different cluster"));
        ASSERTN(f->get_pred() == pd, ("Use different predication"));

        //Gen copy-or and insert into BB after 'f'.
        SR * ldval = f->get_result(0);
        ors.clean();
        genCopyOR(cp_clust, cp_unit, spill_loc_sr, ldval, pd, ors);
        ASSERT0(ors.get_elem_count() == 1);
        OR * new_cp = ors.get_head();
        ddg.appendOR(new_cp);
        concern_ors.append_tail(new_cp);
        newcp_lst.append_tail(new_cp);
        bb_or_lst->insert_after(ors, f);

        //Chain up edges of pred and succ of 'f'.
        ddg.get_succs(ors, f);
        concern_ors.append_tail(ors);
        ddg.get_preds(ors, f);
        concern_ors.append_tail(ors);
        is_resch = true;
    }

    //Remove useless reload-OR out of BB's or-list and DDG.
    //Chain up predecessors and successors for each reload.
    for (OR * r = followed_lds.get_head();
         r != nullptr; r = followed_lds.get_next()) {
        ddg.chainPredAndSucc(r);
        bb_or_lst->remove(r);
        ddg.removeOR(r);
        is_resch = true;
    }

    //Remove those nodes which were not on the graph.
    xcom::C<OR*> * oct;
    xcom::C<OR*> * next_oct = nullptr;
    for (concern_ors.get_head(&oct); oct != concern_ors.end(); oct = next_oct) {
        OR * t = oct->val();
        next_oct = concern_ors.get_next(oct);
        if (!ddg.isGraphNode(t)) {
            concern_ors.remove(oct);
        }
    }

    for (OR * newcp2 = newcp_lst.get_head();
         newcp2 != nullptr; newcp2 = newcp_lst.get_next()) {
        ddg.unifyEdge(concern_ors, newcp2);
    }
    return is_resch;
}


//e.g: Find free physical register to replace memory spill location.
//    [spill_loc1] = srx
//    ...
//    sry = [spill_loc1]
//=>
//    presumably r0 is free.
//    r0 = srx
//    ...
//    sry = r0
bool LRA::hoistSpillLoc(InterfGraph & ig,
                        LifeTimeMgr & mgr,
                        DataDepGraph & ddg)
{
    if (!HAVE_FLAG(m_opt_phase, LRA_OPT_HSL)) { return false; }

    bool is_resch = false;
    ORCt * orct;
    ORCt * next_orct;
    for (ORBB_orlist(m_bb)->get_head(&orct), next_orct = orct;
         orct != nullptr; orct = next_orct) {
        ORBB_orlist(m_bb)->get_next(&next_orct);
        OR * o = orct->val();
        if (o->getCode() == OR_spadjust ||
            o->is_asm() ||
            o->is_fake() ||
            OR_is_volatile(o)) {
            continue;
        }

        if (o->is_store()) {
            is_resch |= hoistSpillLocForStore(o, ig, mgr, ddg,
                                              orct, &next_orct);
        }
    }
    return is_resch;
}


void LRA::reviseMiscOR()
{
}


//Update and record the register usage if there is inline-asm code.
bool LRA::updateInfoEffectedByInlineASM()
{
    if (m_ramgr != nullptr) { return true; }

    ORCt * ct;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_next(&ct)) {
        if (!o->is_asm()) { continue; }

        ORAsmInfo * asm_info = m_cg->getAsmInfo(o);
        if (asm_info == nullptr) { continue; }

        RegSet const* clobber_regs = asm_info->get_clobber_set();
        for (INT reg = clobber_regs->get_first();
             reg != -1; reg = clobber_regs->get_next(reg)) {
            m_ramgr->updateAsmClobberCallee(tmMapReg2RegFile((REG)reg), reg);
        }
    }

    return true;
}


void LRA::middleLRAOpt(IN OUT DataDepGraph & ddg,
                       IN OUT LifeTimeMgr & mgr,
                       IN OUT BBSimulator & sim,
                       IN RegFileSet & is_regfile_unique,
                       IN OUT ClustRegInfo cri[CLUST_NUM])
{
    //Following passes have to analyze REG-IN and
    //PHY-REG dependence.

    bool need_rebuild_mgr = false;
    if ((HAVE_FLAG(m_opt_phase, LRA_OPT_RCEL) ||
         HAVE_FLAG(m_opt_phase, LRA_OPT_DDE)) &&
        ORBB_ornum(m_bb) < OR_BB_LEN_AFTER_RFA) {
        ddg.pushParam();
        ddg.setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                     DEP_MEM_ANTI|DEP_SYM_REG);
        ddg.reschedul();
        if (coalescing(ddg, false)) {
            need_rebuild_mgr = true;
        }

        if (elimRedundantRegDef(ddg)) {
            need_rebuild_mgr = true;
        }

        ddg.popParam();
        ddg.reschedul();
    }

    if (ORBB_ornum(m_bb) <= 0) { return; }

    if (need_rebuild_mgr) {
        mgr.recreate(m_bb, false, true);
        mgr.computeUsableRegs();
    }

    if (schedulFuncUnit(mgr, ddg, &sim, is_regfile_unique, cri)) {
        m_cur_phase |= PHASE_SCH_DONE;
    }

    if (!fixup()) { return; }

    show_phase("Optimizations after fixup().");

    //REGFILE must be valid after performing fixup().
    if (HAVE_FLAG(m_opt_phase, LRA_OPT_RCEL) ||
        HAVE_FLAG(m_opt_phase, LRA_OPT_RCIE) ||
        HAVE_FLAG(m_opt_phase, LRA_OPT_DDE)) {
        bool again;
        INT count = 0;
        ORIdTab handled;
        bool change_ddg_param = false;
        if (!ddg.is_param_equal(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|
                                DEP_REG_ANTI|DEP_MEM_ANTI|DEP_SYM_REG)) {
            change_ddg_param = true;
            ddg.pushParam();
            ddg.setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|
                         DEP_REG_ANTI|DEP_MEM_ANTI|DEP_SYM_REG);
            ddg.reschedul();
        }

        do {
            again = false;
            show_phase("---MIDOPT:Eliminate Redundant Code.");
            if (coalescing(ddg, true)) {
                again = true;
            }
            show_phase("---MIDOPT:Eliminate Redundant Code Inverse.");
            if (cse(ddg, handled)) {
                again = true;
            }
            show_phase("---MIDOPT:Eliminate Dead Def.");
            if (elimRedundantRegDef(ddg)) {
                again = true;
            }
            count++;
        } while (again && count < 128);
        ASSERTN(!again, ("Unexpected exit."));

        if (change_ddg_param) {
            ddg.popParam();
        }

        ddg.reschedul(); //always rebuild DDG after fixup.

        if (ORBB_ornum(m_bb) <= 0) { return; }
    } else {
        //OR generated.
        ddg.reschedul();
    }

    mgr.destroy();
    mgr.init(m_bb, true, true);
    mgr.create();

    //Verfication needs Cluster-Info and RegFile-Info.
    ASSERT0(mgr.verifyLifeTime());

    mgr.computeUsableRegs();
}


//Reset GSR's spill location if it was spilled during LRA.
//This is because that we expect spill locations used by LRA
//could be reclaimed while other BB's LRA processing.
void LRA::resetGSRSpillLocation()
{
    for (SR * gsr = m_spilled_gsr.get_head();
         gsr != nullptr; gsr = m_spilled_gsr.get_next()) {
        ASSERT0(gsr->is_reg());
        if (SR_spill_var(gsr) == nullptr) { continue; }

        xoc::Var * v = SR_spill_var(gsr);
        if (m_cg->getBBLevelVarList()->find(v)) {
            SR_spill_var(gsr) = nullptr;
        }
    }
}


//Renaming local sr
void LRA::renameSR()
{
    if (!HAVE_FLAG(m_opt_phase, LRA_OPT_RNSR)) {
        return;
    }

    ORVec sr2or;
    ORCt * ct = nullptr;
    for (OR * o = ORBB_orlist(m_bb)->get_head(&ct);
         o != nullptr; o = ORBB_orlist(m_bb)->get_next(&ct)) {
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (m_cg->isCondExecOR(o) ||
                o->is_asm() ||
                OR_is_volatile(o) ||
                OR_is_side_effect(o)) {
                continue;
            }

            if (sr2or.get(SR_sregid(sr)) == nullptr ||
                m_cg->isOpndSameWithResult(sr, o, nullptr, nullptr)) {
                sr2or.set(SR_sregid(sr), o);
                continue;
            }

            //Meet the second DEF of sr.
            if (SR_is_dedicated(sr)) {
                //On the target with few register:
                //Rename global SR may fall down on performance.
                //Because we need allocate register
                //for the new localized SRs renamed.
                //
                //sr2or[SR_sregid(sr)] = o;
                //SR * newsr = m_cg->genReg();
                //OR * prev_def = sr2or[SR_sregid(sr)];
                //m_cg->renameResult(prev_def, sr, newsr, false);
                //ORCt * tmp = nullptr;
                //ORBB_orlist(m_bb)->find(prev_def, &tmp);
                //ASSERT0(tmp);
                //OR * next_prev_def = ORBB_orlist(m_bb)->get_next(&tmp);
                //ORCt * tmp_orct = ct;
                //OR * prev_or = ORBB_orlist(m_bb)->get_prev(&tmp_orct);
                //if (next_prev_def != o) {
                //    renameOpndInRange(sr, newsr, next_prev_def,
                //                      prev_or, ORBB_orlist(bb));
                //}
                //m_cg->renameOpnd(o, sr, newsr, false);
                continue;
            }

            //e.g:gsr1 =
            //         = gsr1
            //    gsr1 = gsr1
            //=>
            //    sr2  =
            //         = sr2
            //    gsr1 = sr2
            if (sr->is_global()) {
                //Rename global sr may fall down on performance.
                //Because we need allocate register for the new local sr.
                SR * newsr = m_cg->genReg();
                OR * prev_def = sr2or[SR_sregid(sr)];
                ORCt * prev_def_ct = nullptr, * tmp = nullptr;
                ORBB_orlist(m_bb)->find(prev_def, &prev_def_ct);
                m_cg->renameResult(prev_def, sr, newsr, false);
                tmp = prev_def_ct;
                if (ORBB_orlist(m_bb)->get_next(&tmp) != o) {
                    ORCt * tmp_ct = ct;
                    tmp = prev_def_ct;
                    m_cg->renameOpndAndResultInRange(sr, newsr,
                        ORBB_orlist(m_bb)->get_next(&tmp),
                        ORBB_orlist(m_bb)->get_prev(&tmp_ct),
                        ORBB_orlist(m_bb));
                }
                m_cg->renameOpnd(o, sr, newsr, false);
                sr2or.set(SR_sregid(sr), o);
                continue;
            }

            //local sr
            ASSERTN(sr->getPhyReg() == REG_UNDEF, ("invalid sr"));
            SR * newsr = m_cg->genReg();
            m_cg->renameResult(o, sr, newsr, false);

            ORCt * tmp_orct = ct;
            OR * next_or = ORBB_orlist(m_bb)->get_next(&tmp_orct);
            m_cg->renameOpndAndResultFollowed(sr, newsr,
                next_or, ORBB_orlist(m_bb));
            sr2or.set(SR_sregid(newsr), o);
        }
    }
}


void LRA::finalLRAOpt(LifeTimeMgr * mgr, InterfGraph * ig, DataDepGraph * ddg)
{
    if (HAVE_FLAG(m_opt_phase, LRA_OPT_FINAL_OPT)) {
        ASSERTN(ddg, ("Post opt need DDG"));

        show_phase("Alloca life time completely, start to post optimizations");
        //Data dependence need update while physical registers
        //have been assigned.
        ddg->setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                      DEP_MEM_ANTI|DEP_SYM_REG);
        ddg->reschedul();

        INT count = 0; //time to termination
        ORIdTab handled;
        bool again;
        do {
            again = false;

            show_phase("---PostOPT: Register Coalescing.");
            if (coalescing(*ddg, true)) {
                again = true;
            }

            show_phase("---PostOPT: CSE.");
            if (cse(*ddg, handled)) {
                again = true;
            }

            show_phase("---PostOPT: Eliminate Redundant RegDef.");
            if (elimRedundantRegDef(*ddg)) {
                again = true;
                handled.clean();//try more times.
            }

            show_phase("---PostOPT: Eliminate Redundant LdSt.");
            if (elimRedundantStoreLoad(*ddg)) {
                again = true;
            }

            show_phase("---PostOPT: HoistSpillLoc.");
            if (hoistSpillLoc(*ig, *mgr, *ddg)) {
                again = true;
            }

            //It looks like useless!
            //There NOT any code removed as result of this phase.
            //show_phase("\t\t\tFINOPT:elimRedundantUse...");
            //if (elimRedundantUse(*ddg)) {
            //    again = true;
            //}
            count++;
        } while (again && count < 128);

        ASSERTN(!again, ("Unexpected termination"));
    }

    UINT old = m_opt_phase;
    m_opt_phase = 0;
    show_phase("All passes are completed!");
    ASSERT0(verify(*mgr, *ig));

    reviseMiscOR();
    if (HAVE_FLAG(m_cur_phase, PHASE_INIT)) {
        updateInfoEffectedByInlineASM();
    }

    m_opt_phase = old;
}


void LRA::postLRA()
{
    resetGSRSpillLocation();

    //Unfreeze occupied temporary stack variable of
    //current BB for next processing of LRA.
    m_cg->getBBLevelVarList()->freeAll();
}


RegFileGroup * LRA::allocRegFileGroup()
{
    return new RegFileGroup();
}


RegFileAffinityGraph * LRA::allocRegFileAffinityGraph()
{
    return new RegFileAffinityGraph();
}


InterfGraph * LRA::allocInterfGraph()
{
    return new InterfGraph();
}


//Return true if target machine owns multi-clustered.
bool LRA::isMultiCluster()
{
    if (((UNIT)CLUST_NUM - (UINT)CLUST_FIRST) <= 1) {
        ASSERT0_DUMMYUSE(CLUST_UNDEF == 0);
        return false;
    }
    return true;
}


//Do verification before LRA.
bool LRA::verifyRegSR()
{
    ORCt * orct = nullptr;
    for (ORBB_orlist(m_bb)->get_head(&orct);
         orct != ORBB_orlist(m_bb)->end();
         ORBB_orlist(m_bb)->get_next(&orct)) {
        OR * o = orct->val();
        for (UINT i = 0; i < o->result_num(); i++) {
            SR const* sr = o->get_result(i);
            if (!sr->is_reg() || sr->getPhyReg() == REG_UNDEF) { continue; }
            ASSERTN(SR_is_dedicated(sr) || sr->is_global(),
                   ("local SR has been assigned register before LRA"));
        }

        for (UINT j = 0; j < o->opnd_num(); j++) {
            SR const* sr = o->get_opnd(j);
            if (!sr->is_reg() || sr->getPhyReg() == REG_UNDEF) { continue; }

            ASSERTN(SR_is_dedicated(sr) || sr->is_global(),
                    ("local SR has been assigned register "
                     "before LRA, it should be localized."));
        }
    }

    return true;
}


void LRA::allocAndSolveConflict(List<LifeTime*> & prio_list,
                                List<LifeTime*> & uncolored_list,
                                RegFileSet & is_regfile_unique,
                                Action & action,
                                LifeTimeMgr * mgr,
                                RegFileGroup * rfg,
                                InterfGraph * ig,
                                DataDepGraph * ddg,
                                ClustRegInfo cri[CLUST_NUM])
{
    show_phase("Start to alloca life time");
    if (computePrioList(prio_list, uncolored_list, *ig, *mgr, rfg)) {
        //All life-times have been colored.
        return;
    }

    for (LifeTime * lt = uncolored_list.get_head(); lt != nullptr;
         lt = uncolored_list.get_next()) {
        action.set_action(lt, Action::BFS_REASSIGN_REGFILE);
    }

    show_phase("Start to Solve Conflict");
    if (isOpt() &&
        !ddg->is_param_equal(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|
                             DEP_REG_ANTI|DEP_MEM_ANTI|DEP_SYM_REG)) {
        //diff in RRD
        ddg->setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                      DEP_MEM_ANTI|DEP_SYM_REG);
        ddg->reschedul();
    }

    solveConflict(uncolored_list, prio_list, cri, is_regfile_unique,
                  *ig, *mgr, *ddg, rfg, action);

    fixup(); //Target Dependent.

    //Allocating for new sr that generated by fixup.
    m_cur_phase |= PHASE_FINIAL_FIXUP_DONE;
    if (isAllAllocated(nullptr)) { return; }

    show_phase("New srs have been built during fixup "
               "while after solve conflict, realloca life time also.");
    reallocateLifeTime(prio_list, uncolored_list,
                       *mgr, *ddg, rfg, *ig, cri);
    if (uncolored_list.get_elem_count() == 0) { return; }

    for (LifeTime * lt = uncolored_list.get_head();
        lt != nullptr; lt = uncolored_list.get_next()) {
        action.set_action(lt, Action::SPLIT);
    }
    show_phase("Fixup after solve conflict, and "
               "after reallocate life time, do solve conflict");
    solveConflict(uncolored_list, prio_list, cri,
                  is_regfile_unique, *ig, *mgr, *ddg, rfg, action);
}


xcom::BitSetMgr * LRA::getBitSetMgr() const
{
    return m_cg->getBitSetMgr();
}


bool LRA::perform()
{
    if (ORBB_ornum(m_bb) <= 0) { return true; }
    //START_TIMER_FMT(t, ("Perform Local Register Allocation for ORBB%d",
    //                    m_bb->id()));

    if (ORBB_ornum(m_bb) > MAX_OR_BB_OPT_BB_LEN) {
        xoc::interwarn("During LRA: Length of ORBB%d is larger "
                       "than %d, optimizations are disabled!",
                       m_bb->id(), MAX_OR_BB_OPT_BB_LEN);
    }
    preLRA();
    elimRedundantCopy(m_cg->isGRAEnable());
    renameSR();

    List<LifeTime*> uncolored_list; //Record uncolored life times.
    Action action;
    ClustRegInfo cri[CLUST_NUM];
    resetClustRegInfo(cri, (UINT)CLUST_NUM);

    //'is_regfile_unique' map from Index to SR, in order to determine if
    //register-file of SR is dedicated o unique.
    RegFileSet is_regfile_unique;
    DataDepGraph * ddg = m_cg->allocDDG();

    bool include_mem_dep = false;
    if (isOpt()) {
        include_mem_dep = true;
    }

    DDGParam param = include_mem_dep ? DEP_MEM_FLOW : DEP_UNDEF;
    param |= include_mem_dep ? DEP_MEM_OUT : DEP_UNDEF;
    param |= include_mem_dep ? DEP_MEM_ANTI : DEP_UNDEF;
    ddg->setParam(param|DEP_REG_ANTI|DEP_SYM_REG);
    ddg->init(m_bb);
    ddg->setParallelPartMgr(m_ppm);
    if (isOpt() || isMultiCluster()) {
        ddg->build();
    }

    List<LifeTime*> prio_list;
    LifeTimeMgr * mgr = nullptr;
    RegFileAffinityGraph * rdg = nullptr;
    BBSimulator * sim = nullptr;
    RegFileGroup * rfg = nullptr;
    InterfGraph * ig = nullptr;
    bool enable_optimal_partition = false;
    if (HAVE_FLAG(m_cur_phase, PHASE_INIT) && ORBB_ornum(m_bb) <= 0) {
        postLRA();
        goto SUCC;
    }
    computeUniqueRegFile(is_regfile_unique);

    //Assigninment of cluster must done before life time constructing.
    show_phase("Assign Cluster");
    assignCluster(*ddg, is_regfile_unique, true);
    if (isMultiCluster()) {
        reviseInterClusterOR(*ddg, is_regfile_unique);
    }
    m_cur_phase |= PHASE_CA_DONE;

    //TODO, use optimal partitioning method.
    if (isOpt() && enable_optimal_partition) {
        if (!ddg->is_param_equal(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|
                                 DEP_CONTROL|DEP_REG_ANTI|DEP_MEM_ANTI|
                                 DEP_SYM_REG)) {
            ddg->setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|
                          DEP_CONTROL|DEP_REG_ANTI|DEP_MEM_ANTI|
                          DEP_SYM_REG);
            ddg->reschedul();
        }
        optimal_partition(*ddg, is_regfile_unique);
    }

    if (isOpt()) {
        ddg->setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                      DEP_MEM_ANTI|DEP_SYM_REG);
        ddg->reschedul();
    }

    //Create life time at first.
    mgr = m_ramgr->allocLifeTimeMgr();
    mgr->init(m_bb, false, true);
    mgr->create();
    mgr->computeUsableRegs();
    ASSERT0(verifyUsableRegSet(*mgr));
    //ORBB_liveout(m_bb).dump();

    //Build Affinity xcom::Graph
    rdg = allocRegFileAffinityGraph();
    rdg->init(m_bb);
    rdg->build(*mgr, *ddg);

    //Assign register file.
    show_phase("Assign RegFile");
    assignRegFile(cri, is_regfile_unique, *mgr, *ddg, *rdg);

    //Perform target independent optimizations to simplify code.
    sim = m_cg->allocBBSimulator(m_bb);
    if (isOpt()) {
        middleLRAOpt(*ddg, *mgr, *sim, is_regfile_unique, cri);
    }

    if (ORBB_ornum(m_bb) <= 0) {
        postLRA();
        goto SUCC;
    }

    //Computing usable registers, available for all phases.
    //CASE: In the case of that requires usable-register to be recalculated
    //when either regfile and func-unit were changed, illegal
    //instructions were fixed up.
    //e.g:
    //  orig code: usable registers of SR1008 is empty!
    //      [11] SR1008[D1] :- copy_i SR1093[D1]
    //      [ 1] SR844[A1] :- lw_m SR1008[D1] (0x4)
    //  after cyc-estimate: usable registers of SR1008 need to be recomputed.
    //      [11] SR1008[A1] :- copy_m SR1093[D1]
    //      [ 1] SR844[A1] :- lw_m SR1008[A1] (0x4)
    mgr->computeUsableRegs();
    ASSERT0(verifyUsableRegSet(*mgr));

    //Build interference graph.
    show_phase("Build Interference xcom::Graph");
    ig = allocInterfGraph();
    ig->init(m_bb);
    ig->build(*mgr);

    //Calculate the prioirtys.
    show_phase("Build Priority List");
    buildPriorityList(prio_list, *ig, *mgr, *ddg);
    ASSERT0(verifyUsableRegSet(*mgr));

    show_phase("Compute Layer");
    rfg = allocRegFileGroup();
    rfg->init();
    rfg->setBB(m_bb);
    rfg->computeGroup();
    allocAndSolveConflict(prio_list, uncolored_list, is_regfile_unique, action,
                          mgr, rfg, ig, ddg, cri);
    m_cur_phase |= PHASE_RA_DONE;
    finalLRAOpt(mgr, ig, ddg);
    postLRA();

SUCC:
    //END_TIMER_FMT(t, ("Perform Local Register Allocation for ORBB%d",
    //                  m_bb->id()));
    if (ddg != nullptr) { delete ddg; }
    if (mgr != nullptr) { delete mgr; }
    if (ig != nullptr) { delete ig; }
    if (rdg != nullptr) { delete rdg; }
    if (sim != nullptr) { delete sim; }
    if (rfg != nullptr) { delete rfg; }
    return true;
}
//END LRA

} //namespace xgen
