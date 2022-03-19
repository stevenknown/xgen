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
#ifndef _OR_CFG_H_
#define _OR_CFG_H_

namespace xgen {

typedef xcom::TMap<xoc::LabelInfo const*, ORBB*> Lab2ORBB;

//NOTICE:
//1. For accelerating perform operation of each vertex, e.g
//   compute dominator, please try best to add vertex with
//   topological order.
class ORCFG : public CFG<ORBB, OR> {
protected:
    Vector<ORBB*> m_bb_vec;
    Lab2ORBB m_lab2bb;
    CG * m_cg;
protected:
    void dump_node(FILE * h, bool detail);
    void dump_edge(FILE * h);
    //Print graph structure description.
    void dump_head(FILE * h);
public:
    ORCFG(CFG_SHAPE cs, List<ORBB*> * bbl, CG * cg);
    virtual ~ORCFG() {}
    virtual void addBB(ORBB * bb);

    virtual void cf_opt();

    void dumpVCG(CHAR const* name = nullptr, bool detail = true);

    virtual ORBB * findBBbyLabel(LabelInfo const* lab) const;
    virtual void findTargetBBOfMulticondBranch(OR const*, OUT List<ORBB*>&)
    { ASSERTN(0, ("Target Dependent Code")); }
    virtual void findTargetBBOfIndirectBranch(OR const*, OUT List<ORBB*>&);

    virtual ORBB * getBB(UINT id) const;
    virtual INT getNumOfBB();
    virtual List<ORBB*> * getBBList();
    virtual void get_preds(MOD List<ORBB*> & preds, ORBB const* bb);
    virtual void get_succs(MOD List<ORBB*> & succs, ORBB const* bb);
    OR * get_first_xr(ORBB * bb);
    OR * get_last_xr(ORBB * bb);
    CG * getCG() const { return m_cg; }

    virtual bool isRegionEntry(ORBB * bb) const { return ORBB_is_entry(bb); }
    virtual bool isRegionExit(ORBB * bb) const { return ORBB_is_exit(bb); }

    virtual void removeDomInfo(C<ORBB*> * bbct, CfgOptCtx const& ctx)
    {
        if (ctx.need_update_dominfo()) {
            DGraph::removeDomInfo(bbct->val()->id());
        }
    }

    //Remove 'bb' from CFG, vector and bb-list.
    virtual void removeBB(C<ORBB*> * bbct, CfgOptCtx const& ctx)
    {
        ASSERT0(bbct && m_bb_list->in_list(bbct));
        ORBB * bb = bbct->val();
        m_bb_vec.set(bb->id(), nullptr);
    
        //Note labels should have been moved to other BB.
        //C<LabelInfo const*> * ct;
        //for (lablst.get_head(&ct);
        //     ct != lablst.end(); ct = lablst.get_next(ct)) {
        //    m_lab2bb.remove(ct->val());
        //}

        removeVertex(bb->id());
        m_bb_list->remove(bbct);
    }

    //Remove 'bb' from CFG, vector and bb-list.
    virtual void removeBB(ORBB * bb, CfgOptCtx const& ctx)
    {
        C<ORBB*> * bbct;
        m_bb_list->find(bb, &bbct);
    }
    virtual void remove_xr(ORBB * bb, OR * o);
    virtual void resetMapBetweenLabelAndBB(ORBB * bb);
    
    //Remove related PHI operand from successor BB.
    //Before removing current BB or change BB's successor,
    //you need remove the related PHI operand if BB successor has PHI.
    virtual void removeSuccPhiOpnd(ORBB const* bb) {}

    virtual void setRPO(ORBB * bb, INT order) { ORBB_rpo(bb) = order; }
    virtual void moveLabels(ORBB * src, ORBB * tgt);
};

} //namespace xgen
#endif
