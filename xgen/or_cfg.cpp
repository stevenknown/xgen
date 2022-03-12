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

//ORCFG
ORCFG::ORCFG(CFG_SHAPE cs, List<ORBB*> * bbl, CG * cg)
    : xoc::CFG<ORBB, OR>(bbl)
{
    m_cg = cg;
    setBitSetMgr(cg->getBitSetMgr());

    if (m_bb_list->get_elem_count() == 0) {
        return;
    }

    //Append bb list to vector and base graph
    ASSERT0(m_bb_vec.get_last_idx() == -1);
    for (ORBB * bb = m_bb_list->get_head();
         bb != nullptr; bb = m_bb_list->get_next()) {
        m_bb_vec.set(bb->id(), bb);
        addVertex(bb->id());
        C<xoc::LabelInfo const*> * ct;
        for (bb->getLabelList().get_head(&ct);
             ct != bb->getLabelList().end();
             ct = bb->getLabelList().get_next(ct)) {
            xoc::LabelInfo const* li = ct->val();
            m_lab2bb.set(li, bb);
        }
    }

    switch (cs) {
    case C_SESE: {  //single entry, single exit
        //The first bb in list is the entry node
        m_entry = m_bb_list->get_head();
        ASSERT0(getVertex(ORBB_id(m_entry))->getInDegree() == 0);
        ORBB_is_entry(m_entry) = true;

        //create exit bb
        ORBB * exitbb = m_cg->allocBB();
        ASSERT0(getVertex(ORBB_id(exitbb))->getOutDegree() == 0);
        ORBB_is_exit(exitbb) = true;
        addBB(exitbb);
        m_bb_list->append_tail(exitbb);
        m_exit_list.append_head(exitbb);
        break;
    }
    case C_SEME: { //single entry, multi exit
        //The first bb in list is the entry node
        m_entry = m_bb_list->get_head();
        ASSERT0(getVertex(ORBB_id(m_entry))->getInDegree() == 0);
        ORBB_is_entry(m_entry) = 1;

        //Collect exit BB.
        for (ORBB * bb = m_bb_list->get_head();
             bb != nullptr; bb = m_bb_list->get_next()) {
            if (ORBB_is_exit(bb)) {
                m_exit_list.append_tail(bb);
            }
        }
        break;
    }
    default: ASSERTN(0, ("strang shape of CFG"));
    }
}


INT ORCFG::getNumOfBB()
{
    return getVertexNum();
}


List<ORBB*> * ORCFG::getBBList()
{
    return m_bb_list;
}


//Add a new vertex into Graph to describe BB, but
//the BB list should be modified out of this function.
void ORCFG::addBB(ORBB * bb)
{
    ASSERT0(bb && m_bb_vec.get(bb->id()) == nullptr);
    m_bb_vec.set(bb->id(), bb);
    addVertex(bb->id());
}


//Move Labels from 'src' to 'tgt'.
void ORCFG::moveLabels(ORBB * src, ORBB * tgt)
{
    //Remap label2bb.
    for (LabelInfo const* li = src->getLabelList().get_head();
         li != nullptr; li = src->getLabelList().get_next()) {
        m_lab2bb.setAlways(li, tgt);
    }
    tgt->mergeLabeInfoList(src);

    //Cutoff the relation between src and labels.
    ASSERT0(src->getLabelList().get_elem_count() == 0);
}


//Cut off the mapping relation bwteen Labels and BB.
void ORCFG::resetMapBetweenLabelAndBB(ORBB * bb)
{
    for (xoc::LabelInfo const* li = bb->getLabelList().get_head();
         li != nullptr; li = bb->getLabelList().get_next()) {
        m_lab2bb.setAlways(li, nullptr);
    }
    bb->cleanLabelInfoList();
}


//Find a list bb that referred labels which is the target of ir.
void ORCFG::findTargetBBOfIndirectBranch(OR const* o, OUT List<ORBB*> & tgtlst)
{
    ASSERT0(o->isIndirectBr());
    for (LabelInfoList const* lablst = o->getLabelList();
         lablst != nullptr; lablst = lablst->get_next()) {
        ORBB * bb = m_lab2bb.get(lablst->getLabel());
        ASSERT0(bb); //no bb is correspond to lab.
        tgtlst.append_tail(bb);
    }
}


void ORCFG::remove_xr(ORBB * bb, OR * o)
{
    ORBB_orlist(bb)->remove(o);
}


ORBB * ORCFG::getBB(UINT id) const
{
    return m_bb_vec.get(id);
}


//Return all successors.
void ORCFG::get_succs(MOD List<ORBB*> & succs, ORBB const* bb)
{
    xcom::Vertex * v = getVertex(bb->id());
    xcom::EdgeC * el = v->getOutList();
    succs.clean();
    while (el != nullptr) {
        ORBB * succ = getBB(el->getToId());
        ASSERTN(succ, ("without bb corresponded"));
        succs.append_tail(succ);
        el = EC_next(el);
    }
}


//Return all predecessors.
void ORCFG::get_preds(MOD List<ORBB*> & preds, ORBB const* bb)
{
    xcom::Vertex * v = getVertex(bb->id());
    xcom::EdgeC * el = v->getInList();
    preds.clean();
    while (el != nullptr) {
        ORBB * pred = getBB(el->getFromId());
        ASSERTN(pred, ("without bb corresponded"));
        preds.append_tail(pred);
        el = EC_next(el);
    }
}


//Control flow optimization
void ORCFG::cf_opt()
{
    ASSERTN(0, ("TODO"));
    bool change = true;
    while (change) {
        change = false;
        List<ORBB*> * bbl = getBBList();
        for (ORBB * bb = bbl->get_head(); bb; bb = bbl->get_next()) {
            ;
        }
    }
}


//Return the last operation of 'bb'.
OR * ORCFG::get_last_xr(ORBB * bb)
{
    ASSERT0(bb && m_bb_vec.get(bb->id()));
    return ORBB_last_or(bb);
}


//Return the first operation of 'bb'.
OR * ORCFG::get_first_xr(ORBB * bb)
{
    ASSERT0(bb && m_bb_vec.get(bb->id()));
    return ORBB_first_or(bb);
}


ORBB * ORCFG::findBBbyLabel(xoc::LabelInfo const* lab) const
{
    ORBB * bb = m_lab2bb.get(lab);
    ASSERT0(bb);
    return bb;
}


void ORCFG::dump_node(FILE * h, bool detail)
{
    xcom::StrBuf buf(64);
    UINT vertical_order = 1;
    for (ORBB * bb = m_bb_list->get_head();
         bb != nullptr; bb = m_bb_list->get_next()) {
        ASSERTN(getVertex(bb->id()), ("bb is not in cfg"));

        CHAR const* shape = "box";
        CHAR const* font = "courB";
        INT scale = 1;
        CHAR const* color = "gold";
        if (ORBB_is_entry(bb) || ORBB_is_exit(bb)) {
            font = "Times Bold";
            scale = 2;
            color = "cyan";
        }

        fprintf(h,
            "\nnode: {title:\"%d\" vertical_order:%d shape:%s color:%s "
            "fontname:\"%s\" scaling:%d label:\"",
            bb->id(), vertical_order++, shape, color, font, scale);
        fprintf(h, "   BB%d", bb->id());
        fprintf(h, " rpo:%d", bb->rpo());
        fprintf(h, "\n");

        if (detail) {
            for (OR * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb)) {
                buf.clean();
                fprintf(h, "%s\n", o->dump(buf, m_cg));
            }
        }

        fprintf(h, "\"}");
    }
}


void ORCFG::dump_edge(FILE * h)
{
    EdgeIter c;
    for (xcom::Edge * e = get_first_edge(c);
         e != nullptr; e = get_next_edge(c)) {
        fprintf(h,
                "\nedge: { sourcename:\"%d\" targetname:\"%d\" "
                " thickness:4 color:darkred }",
                VERTEX_id(EDGE_from(e)), VERTEX_id(EDGE_to(e)));
    }
}


//Print graph structure description.
void ORCFG::dump_head(FILE * h)
{
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
}


void ORCFG::dumpVCG(CHAR const* name, bool detail)
{
    if (name == nullptr) {
        name = "graph_cfg.vcg";
    }
    UNLINK(name);
    FILE * h = fopen(name, "a+");
    ASSERTN(h != nullptr, ("%s create failed!!!",name));

    //Print comment
    fprintf(h, "\n/*");
    LogCtx lc;
    lc.logfile = h;
    lc.logfile_name = name;
    lc.prt_srcline = false;
    m_cg->getRegion()->getLogMgr()->push(lc);

    //Some source code string may incur VCG parser reporting error,
    //e.g: the dissociate char '"' will lead to syntax error.
    g_cg_dump_src_line = false;

    dumpORBBList(*m_bb_list, getCG());
    m_cg->getRegion()->getLogMgr()->pop();
    fprintf(h, "\n*/\n");

    dump_head(h);
    //Print Region name.
    fprintf(h,
            "\nnode: {title:\"\" vertical_order:0 shape:box color:turquoise "
            "borderwidth:0 fontname:\"Courier Bold\" "
            "scaling:2 label:\"RegionName:%s\" }",
            m_cg->getRegion()->getRegionName());
    dump_node(h, detail);
    dump_edge(h);
    fprintf(h, "\n}\n");
    m_cg->getRegion()->getLogMgr()->pop();
    fclose(h);
}

} //namespace xgen
