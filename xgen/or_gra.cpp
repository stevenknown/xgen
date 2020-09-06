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

//
//START OR_DF_MGR
//
xcom::BitSet * OR_DF_MGR::get_def_var(ORBB * bb)
{
    xcom::BitSet * set = m_def_var_set.get(ORBB_id(bb));
    if (set == NULL) {
        set = m_bs_mgr.create();
        m_def_var_set.set(ORBB_id(bb), set);
    }
    return set;
}


xcom::BitSet * OR_DF_MGR::get_use_var(ORBB * bb)
{
    xcom::BitSet * set = m_use_var_set.get(ORBB_id(bb));
    if (set == NULL) {
        set = m_bs_mgr.create();
        m_use_var_set.set(ORBB_id(bb), set);
    }
    return set;
}


//Dump computational set
void OR_DF_MGR::dump()
{
    StrBuf buf(64);
    note(getRegion(), "\n==---- DUMP Set Info of OR_DF_MGR ----==\n");
    List<ORBB*> * bbl = m_cg->getORBBList();
    getRegion()->getLogMgr()->incIndent(2);
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        note(getRegion(), "\n--- BB%d ---", ORBB_id(bb));
        xcom::BitSet * live_in = &ORBB_livein(bb);
        xcom::BitSet * live_out = &ORBB_liveout(bb);
        xcom::BitSet * def = get_def_var(bb);
        xcom::BitSet * use = get_use_var(bb);
        INT i;

        note(getRegion(), "\nLIVE-IN SR: ");
        for (i = live_in->get_first(); i != -1; i = live_in->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr);

            buf.clean();
            prt(getRegion(), "%s, ", sr->get_name(buf, m_cg));
        }

        note(getRegion(), "\nLIVE-OUT SR: ");
        for (i = live_out->get_first(); i != -1; i = live_out->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr != NULL);

            buf.clean();
            prt(getRegion(), "%s, ", sr->get_name(buf, m_cg));
        }

        note(getRegion(), "\nDEF SR: ");
        for (i = def->get_first(); i != -1; i = def->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr != NULL);

            buf.clean();
            prt(getRegion(), "%s, ", sr->get_name(buf, m_cg));
        }

        note(getRegion(), "\nUSE SR: ");
        for (i = use->get_first(); i != -1; i = use->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr != NULL);

            buf.clean();
            prt(getRegion(), "%s, ", sr->get_name(buf, m_cg));
        }
    }
    getRegion()->getLogMgr()->decIndent(2);
}


Region * OR_DF_MGR::getRegion() const
{
    return m_cg->getRegion();
}


void OR_DF_MGR::computeLocalLiveness(ORBB * bb)
{
    xcom::BitSet * gen = get_def_var(bb);
    xcom::BitSet * use = get_use_var(bb);
    gen->clean();
    use->clean();
    for (OR * o = ORBB_last_or(bb); o != NULL; o = ORBB_prev_or(bb)) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            ASSERT0(sr != NULL);
            if (!SR_is_reg(sr)) { continue; }
            gen->bunion(SR_sregid(sr));
        }
        use->diff(*gen);
        for (i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (!SR_is_reg(sr)) { continue; }
            ASSERT0(sr != NULL);
            use->bunion(SR_sregid(sr));
        }
    }
}


void OR_DF_MGR::computeGlobalLiveness()
{
    List<ORBB*> * bbl = m_cg->getORBBList();
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        ORBB_livein(bb).clean();
        ORBB_liveout(bb).clean();
    }
    bool change;
    INT count = 0;
    OR_CFG * cfg = m_cg->getORCfg();
    xcom::BitSet news;
    List<ORBB*> succs;
    do {
        change = false;
        for (ORBB * bb = bbl->get_tail(); bb != NULL; bb = bbl->get_prev()) {
            //Compute live-in set of bb by unifying of
            //use-set and live through variable of bb.
            //    live_in = use¡È(live_out - def)
            news.copy(ORBB_liveout(bb));
            news.diff(*get_def_var(bb));
            news.bunion(*get_use_var(bb));
            xcom::BitSet * in = &ORBB_livein(bb);
            if (!in->is_equal(news)) {
                in->copy(news);
                change = true;
            }

            //Compute live-out set of bb by unifing of live-in set of all succs of bb.
            //    live_out = ¡È (live_in of each of succ)
            cfg->get_succs(succs, bb);
            xcom::BitSet * out = &ORBB_liveout(bb);
            out->clean();
            for (ORBB * p = succs.get_head(); p != NULL; p = succs.get_next()) {
                out->bunion(ORBB_livein(p));
            }
        }
        count++;
    } while (change && count < 256);
    ASSERTN(!change, ("result of equation is convergent slowly"));
}


void OR_DF_MGR::computeLiveness()
{
    List<ORBB*> * bbl = m_cg->getORBBList();
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        computeLocalLiveness(bb);
    }
    computeGlobalLiveness();
    dump();
}
//END OR_DF_MGR



//
//START GLT_MGR Global Life Time Manager
//
GLT_MGR::GLT_MGR(RaMgr * ra_mgr, CG * cg) : m_sr2glt_map(128)
{
    m_glt_count = 0;
    m_ramgr = ra_mgr;
    m_cg = cg;
    m_pool = smpoolCreate(64, MEM_COMM);
}


GLT_MGR::~GLT_MGR()
{
    for (INT i = 0; i <= m_map_bb2ltmgr.get_last_idx(); i++) {
        LifeTimeMgr * ltmgr = m_map_bb2ltmgr.get(i);
        if (ltmgr != NULL) {
            delete ltmgr;
        }
    }
    smpoolDelete(m_pool);
}


Region * GLT_MGR::getRegion() const
{
    return m_cg->getRegion();
}


void GLT_MGR::dump()
{
    UINT max_name_len = 0;
    StrBuf buf(64);
    if (!getRegion()->isLogMgrInit()) { return; }
    xcom::BitSet srbs;
    List<ORBB*> * bbl = m_cg->getORBBList();
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        for (OR * o = ORBB_first_or(bb); o != NULL; o = ORBB_next_or(bb)) {
            UINT i;
            for (i = 0; i < o->opnd_num(); i++) {
                SR * sr = o->get_opnd(i);
                ASSERT0(sr != NULL);
                if (!SR_is_reg(sr)) { continue; }

                buf.clean();
                sr->get_name(buf, m_cg);
                max_name_len = MAX(max_name_len, (UINT)buf.strlen());
                srbs.bunion(SR_sregid(sr));
            }
            for (i = 0; i < o->result_num(); i++) {
                SR * sr = o->get_result(i);
                ASSERT0(sr != NULL);
                if (!SR_is_reg(sr)) { continue; }
                sr->get_name(buf, m_cg);
                max_name_len = MAX(max_name_len, (UINT)buf.strlen());
                srbs.bunion(SR_sregid(sr));
            }
        }
    }

    note(getRegion(), "\n==---- DUMP Global LIFE TIME ----==\n");
    prt(getRegion(), "---- SR lived BB\n");
    for (INT i = srbs.get_first(); i >= 0; i = srbs.get_next(i)) {
        SR * sr = m_cg->mapSymbolReg2SR(i);
        ASSERT0(sr != NULL);
        xcom::BitSet * livebbs = map_sr2livebbs(sr);

        //Print SR name.
        buf.clean();
        note(getRegion(), "\n%s", sr->get_name(buf, m_cg));
        for (UINT v = 0; v < max_name_len - buf.strlen(); v++) {
            prt(getRegion(), " ");
        }
        prt(getRegion(), ":");

        //Print live BB.
        if (livebbs == NULL || livebbs->is_empty()) { continue; }
        INT start = 0;
        for (INT u = livebbs->get_first(); u >= 0; u = livebbs->get_next(u)) {
            for (INT j = start; j < u; j++) {
                buf.sprint("%d,", j);
                for (UINT k = 0; k < buf.strlen(); k++) {
                    prt(getRegion(), " ");
                }
            }
            prt(getRegion(), "%d,", u);
            start = u + 1;
        }
    }
}


xcom::BitSet * GLT_MGR::map_sr2livebbs(IN SR * sr)
{
    xcom::BitSet * bs = m_map_sr2livebb.get(SR_sregid(sr));
    if (bs == NULL) {
        bs = m_bs_mgr.create();
        m_map_sr2livebb.set(SR_sregid(sr), bs);
    }
    return bs;
}


LifeTimeMgr * GLT_MGR::map_bb2ltmgr(ORBB * bb)
{
    LifeTimeMgr * ltmgr = m_map_bb2ltmgr.get(ORBB_id(bb));
    if (ltmgr == NULL) {
        //Allocated object managed by RaMgr, and do not delete it youself.
        ltmgr = GLT_MGR_ra_mgr(*this)->allocLifeTimeMgr();
        m_map_bb2ltmgr.set(ORBB_id(bb), ltmgr);
    }
    return ltmgr;
}


G_LIFE_TIME * GLT_MGR::get_glt(UINT id)
{
    return m_id2glt_map.get(id);
}


//Every OR which refering sr must be assigned to same cluster, therefore
//the only time to record cluster information is the first meeting with sr.
G_LIFE_TIME * GLT_MGR::new_glt(SR * sr)
{
    G_LIFE_TIME * glt;
    if ((glt = m_sr2glt_map.get(sr)) != NULL) {
        return glt;
    }
    glt = (G_LIFE_TIME*)xmalloc(sizeof(G_LIFE_TIME));
    GLT_id(glt) = ++m_glt_count;
    GLT_sr(glt) = sr;
    SR_is_global(sr) = 1;
    m_sr2glt_map.set(sr, glt);
    m_id2glt_map.set(GLT_id(glt), glt);
    return glt;
}


void GLT_MGR::build(IN OR_DF_MGR & df_mgr)
{
    DUMMYUSE(df_mgr);
    List<ORBB*> * bbl = m_cg->getORBBList();
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        xcom::BitSet * livein = &ORBB_livein(bb);
        xcom::BitSet * liveout = &ORBB_liveout(bb);
        if ((livein == NULL || livein->is_empty()) &&
            (liveout == NULL || liveout->is_empty())) {
                continue;
        }
        //The sr which has occurred at more than two BBs will be gsr.
        for (INT i = livein->get_first(); i >= 0; i = livein->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr != NULL);
            xcom::BitSet * livebbs = map_sr2livebbs(sr);
            livebbs->bunion(ORBB_id(bb));
            if (livebbs->get_elem_count() > 1) {
                G_LIFE_TIME * glt = new_glt(sr);
                GLT_livebbs(glt) = livebbs;
            }
        }
        for (INT i = liveout->get_first(); i >= 0; i = liveout->get_next(i)) {
            SR * sr = m_cg->mapSymbolReg2SR(i);
            ASSERT0(sr != NULL);
            xcom::BitSet * livebbs = map_sr2livebbs(sr);
            livebbs->bunion(ORBB_id(bb));
            if (livebbs->get_elem_count() > 1) {
                G_LIFE_TIME * glt = new_glt(sr);
                GLT_livebbs(glt) = livebbs;
            }
        }
        LifeTimeMgr * ltmgr = map_bb2ltmgr(bb);
        ltmgr->init(bb, false, true);
        ltmgr->create();
        ltmgr->computeUsableRegs();
        ltmgr->dump(0);
    }
    dump();
}
//END GLT_MGR


//
//START G_INTERF_GRAPH
//
G_INTERF_GRAPH::G_INTERF_GRAPH(IN RaMgr * ra_mgr, IN GLT_MGR * glt_mgr, CG * cg)
{
    ASSERT0(ra_mgr != NULL && glt_mgr != NULL);
    m_glt_mgr = glt_mgr;
    m_ramgr = ra_mgr;
    m_cg = cg;
    set_direction(false);
}


void G_INTERF_GRAPH::rebuild()
{
    erase();
    build();
}


void G_INTERF_GRAPH::dump(IN CHAR * name)
{
    if (name == NULL) {
        name = "zgif_graph.vcg";
    }
    UNLINK(name);
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!",name));
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
              "splines: yes\n"
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
              "node.borderwidth: 2\n"
              "node.color: lightcyan\n"
              "node.textcolor: black\n"
              "node.bordercolor: blue\n"
              "edge.color: darkgreen\n");

    StrBuf buf(64);
    //Print node
    INT c;
    for (xcom::Vertex const* v = m_vertices.get_first(c);
         v != NULL;  v = m_vertices.get_next(c)) {
        G_LIFE_TIME * glt = m_glt_mgr->get_glt(VERTEX_id(v));
        buf.sprint("GLT%d:", VERTEX_id(v));
        GLT_sr(glt)->get_name(buf, m_cg);
        fprintf(h, "\nnode: { title:\"%d\" label:\"%s\" "
                   "shape:circle fontname:\"courB\" color:gold}",
                   VERTEX_id(v), buf.buf);
    }

    //Print edge
    for (xcom::Edge const* e = m_edges.get_first(c);
         e != NULL;  e = m_edges.get_next(c)) {
        fprintf(h, "\nedge: { sourcename:\"%d\" targetname:\"%d\" %s}",
            VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)),
            m_is_direction ? "" : "arrowstyle:none" );
    }
    fprintf(h, "\n}\n");
    fclose(h);
}


bool G_INTERF_GRAPH::isInterferred(
        IN G_LIFE_TIME * glt1,
        IN G_LIFE_TIME * glt2)
{
    ASSERTN(SR_is_reg(GLT_sr(glt1)) && SR_is_reg(GLT_sr(glt2)),
        ("sr is not register"));
    if (GLT_id(glt1) == GLT_id(glt2)) return true;
    if (!GLT_livebbs(glt1)->is_intersect(*GLT_livebbs(glt2))) return false;
    OR_CFG * cfg = m_cg->getORCfg();
    xcom::BitSet inte;
    bs_intersect(*GLT_livebbs(glt1), *GLT_livebbs(glt2), inte);
    for (INT i = inte.get_first(); i >= 0; i = inte.get_next(i)) {
        ORBB * bb = cfg->getBB(i);
        ASSERT0(bb != NULL);
        LifeTimeMgr * ltmgr = m_glt_mgr->map_bb2ltmgr(bb);
        LifeTime * lt1 = ltmgr->getLifeTime(GLT_sr(glt1));
        LifeTime * lt2 = ltmgr->getLifeTime(GLT_sr(glt2));
        ASSERT0(lt1 != NULL && lt2 != NULL);
        if (LT_pos(lt1)->is_intersect(*LT_pos(lt2))) {
            return true;
        }
    }
    return false;
}


void G_INTERF_GRAPH::build()
{
    //Check interference
    Vector<G_LIFE_TIME*> * gltvec =
        GLT_MGR_sr2glt_map(*m_glt_mgr).get_tgt_elem_vec();
    for (INT i = 0; i <= gltvec->get_last_idx(); i++) {
        G_LIFE_TIME * lt1 = gltvec->get(i);
        ASSERT0(lt1 != NULL);
        addVertex(GLT_id(lt1));
        for (INT j = i + 1; j <= gltvec->get_last_idx(); j++) {
            G_LIFE_TIME * lt2 = gltvec->get(j);
            ASSERT0(lt2 != NULL);
            if (isInterferred(lt1, lt2)) {
                addEdge(GLT_id(lt1), GLT_id(lt2));
            }
        }
    }
}
//END G_INTERF_GRAPH


//
//START G_ACTION
//
G_ACTION::G_ACTION()
{
}


G_ACTION::~G_ACTION()
{
}


UINT G_ACTION::get_action(G_LIFE_TIME * lt)
{
    return m_lt2action.get(GLT_id(lt));
}


void G_ACTION::set_action(G_LIFE_TIME * lt, UINT action)
{
    m_lt2action.set(GLT_id(lt), action);
}
//END G_ACTION



//START GRA
//
void GRA::assignRegFile(IN GLT_MGR &)
{

}


void GRA::assignCluster()
{

}


void GRA::buildPriorityList(OUT List<G_LIFE_TIME*> &, IN G_INTERF_GRAPH &)
{

}


bool GRA::allocatePrioList(
            OUT List<G_LIFE_TIME*> &,
            OUT List<G_LIFE_TIME*> &,
            IN G_INTERF_GRAPH &)
{
    return false;
}


void GRA::solveConflict(
            OUT List<G_LIFE_TIME*> &,
            OUT List<G_LIFE_TIME*> &,
            IN G_INTERF_GRAPH &,
            G_ACTION &)
{

}


void GRA::perform()
{
    START_TIMER(t, "Perform Global Register Allocation");
    //dumpBBList(*m_cg->getORBBList());
    OR_DF_MGR live_mgr(m_cg);
    live_mgr.computeLiveness();
    GLT_MGR gltmgr(GRA_ra_mgr(this), m_cg);
    gltmgr.build(live_mgr);

    assignCluster();
    assignRegFile(gltmgr);

    G_INTERF_GRAPH ig(GRA_ra_mgr(this), &gltmgr, m_cg);
    ig.build();
    ig.dump();

    List<G_LIFE_TIME*> prio_list;
    buildPriorityList(prio_list, ig);

    List<G_LIFE_TIME*> uncolored_list; //Record uncolored life times.
    if (!allocatePrioList(prio_list, uncolored_list, ig)) {
        G_ACTION action;
        for (G_LIFE_TIME * lt = uncolored_list.get_head();
             lt != NULL; lt = uncolored_list.get_next()) {
            action.set_action(lt, G_ACTION_SPLIT);
        }

        //Start to Solve Conflict.
        solveConflict(uncolored_list, prio_list, ig, action);
    }
    END_TIMER(t, "Perform Global Register Allocation");
}
//END GRA

} //namespace xgen
