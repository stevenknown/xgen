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

//Potentially used throughout the compilation of
//a region including separate compilation of sub-regions.
static xcom::TMap<SR*, xcom::BitSet*> g_sr2bbset_map;

bool ParallelPartMgr::findMainIV(
        ORBB * bb,
        DataDepGraph & ddg,
        OR ** red_or,
        OR ** cmp_or,
        SR ** iv)
{
    DUMMYUSE(bb);
    DUMMYUSE(ddg);
    DUMMYUSE(red_or);
    DUMMYUSE(cmp_or);
    DUMMYUSE(iv);
    ASSERTN(0, ("Target Dependent Code"));
    return false;
}


//
//START ParallelPartMgr
//
ParallelPartMgr::ParallelPartMgr(ORBB * bb)
{
    m_pool = NULL;
    init(bb);
}


ParallelPartMgr::~ParallelPartMgr()
{
    destroy();
}


void ParallelPartMgr::init(ORBB * bb)
{
    if (m_pool != NULL) { return; }
    ASSERT0(bb);
    m_bb = bb;
    m_cg = ORBB_cg(bb);
    ASSERT0(m_cg);
    m_prolog = NULL;
    m_epilog = NULL;
    m_pool = smpoolCreate(256, MEM_COMM);
    m_para_part_orlst.init();
    m_para_part_oridx_lst.init();
    m_para_part_redor_lst.init();
    m_regfile_unique.init();
    m_sr2sr_dmap_lst.init();
    m_gsr.init();
    m_num_cluster = 0;
    m_main_red_or = NULL;
    m_cmp_or = NULL;
    m_iv = NULL;
}


void ParallelPartMgr::destroy()
{
    if (m_pool == NULL) return;

    ///
    if (m_para_part_orlst.get_elem_count() != 0) {
        ASSERTN(m_sr2sr_dmap_lst.get_elem_count() ==
                m_para_part_orlst.get_elem_count(), ("unmatch info"));
        List<OR*> * or_lst;
        for (or_lst = m_para_part_orlst.get_head(); or_lst != NULL;
             or_lst = m_para_part_orlst.get_next()) {
            or_lst->destroy();
        }
        for (or_lst = m_para_part_redor_lst.get_head(); or_lst != NULL;
             or_lst = m_para_part_redor_lst.get_next()) {
            or_lst->destroy();
        }
        for (xcom::BitSet * oridx_lst = m_para_part_oridx_lst.get_head();
             oridx_lst != NULL; oridx_lst = m_para_part_oridx_lst.get_next()) {
            oridx_lst->destroy();
        }
        for (SR2SR_DMAP * map = m_sr2sr_dmap_lst.get_head(); map != NULL;
             map = m_sr2sr_dmap_lst.get_next()) {
            delete(map);
        }
    }
    m_sr2sr_dmap_lst.destroy();
    m_para_part_oridx_lst.destroy();
    m_para_part_redor_lst.destroy();
    m_para_part_orlst.destroy();
    m_regfile_unique.destroy();
    m_gsr.destroy();
    ///

    smpoolDelete(m_pool);
    m_pool = NULL;
}


//Rename and make mapping between newtn and original sr.
//Rename local and global SR in a list of OR with new SR come from 'map'.
//Return overall new global TNs used.
void ParallelPartMgr::renameSR(IN OUT OR * o, IN SR2SR_DMAP & dmap)
{
    UINT i;
    for (i = 0;  i < o->result_num(); i++) {
        SR * res = o->get_result(i);
        if (SR_is_reg(res)) {
            if (!SR_is_dedicated(res)) {
                //CASE: Same sr appeared in twice or more times.
                //    sr2, sr2, or, sr9(p0), sr1, 1
                SR * newtn = dmap.get(res);
                if (newtn == NULL) {
                    newtn = m_cg->dupSR(res);
                    dmap.setAlways(res, newtn);
                }
                if (SR_is_global(res)) {
                    m_gsr.append(res);
                }

                //Rename result sr of o for parallel part.
                o->set_result(i, newtn);
            }
        }
    } //end for

    for (i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd( i);
        if (m_cg->isIntRegSR(o, opnd, i, false)) {
            if (!SR_is_dedicated(opnd)) {
                SR * newtn = dmap.get(opnd);
                if (newtn == NULL) {
                    ASSERTN(SR_is_global(opnd),    ("local sr must have DEF"));
                    newtn = m_cg->dupSR(opnd);
                    m_gsr.append(opnd);
                    dmap.setAlways(opnd, newtn);
                }

                //Rename operand sr of o for parallel part.
                o->set_opnd(i, newtn);
            }
        }
    } //end for
}


List<OR*> * ParallelPartMgr::getClusterReductionOR(UINT n)
{
    if (n >= m_para_part_redor_lst.get_elem_count()) {
        return NULL;
    }
    return m_para_part_redor_lst.get_head_nth(n);
}


List<OR*> * ParallelPartMgr::getClusterParallelPart(
        UINT n,
        OUT xcom::BitSet ** oridx_lst)
{
    ASSERTN(m_pool, ("not yet initialize."));
    if (n >= m_para_part_oridx_lst.get_elem_count()) {
        if (oridx_lst) {
            *oridx_lst = NULL;
        }
        return NULL;
    }
    if (oridx_lst) {
        *oridx_lst = m_para_part_oridx_lst.get_head_nth(n);
    }
    return m_para_part_orlst.get_head_nth(n);
}


SR2SR_DMAP * ParallelPartMgr::getClusterDMap(UINT n)
{
    ASSERTN(m_pool, ("not yet initialize."));
    if (n >= m_sr2sr_dmap_lst.get_elem_count()) {
        return NULL;
    }
    return m_sr2sr_dmap_lst.get_head_nth(n);
}


//Generate bus-operation to broadcast
void ParallelPartMgr::genBusCopy(
        IN OUT ORBB * bb,
        CLUST from_clust,
        IN List<CLUST> & to_clust_lst,
        IN SR * from_sr,
        IN List<SR*> & to_sr_lst)
{
    DUMMYUSE(bb);
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERT0(to_clust_lst.get_elem_count() == 1);

    //copy to scalar o cluster-2 unit
    CLUST to_clust = to_clust_lst.get_head();
    SR * to_tn = to_sr_lst.get_head();
    OR * cp = m_cg->buildBusCopy(from_sr, to_tn,
        m_cg->genTruePred(), from_clust, to_clust);
    ORBB_orlist(m_bb)->append_tail(cp);
}


//Return true if there is corresponding parallel part of 'clust'.
bool ParallelPartMgr::hasParallelPart(CLUST clust) const
{
    if (clust == CLUST_UNDEF) {
        return false;
    }
    for (UINT i = 0; i < numOfParallelPart(); i++) {
        if (clust == getCluster(i)) {
            return true;
        }
    }
    return false;
}


//Return ture if 'gsr' has been referred in else ORBB, which is
//post-dominate 'bb'.
bool ParallelPartMgr::hasPDomOcc(ORBB * bb, SR * gsr)
{
    ASSERTN(m_pool, ("not yet initialize."));
    xcom::BitSet * occ = g_sr2bbset_map.get(gsr);
    ASSERT0(occ);
    if (occ->get_elem_count() > 1) {
        xcom::BitSet * pdom = ORBB_cg(bb)->getORCfg()->get_pdom_set(ORBB_id(bb));
        ASSERT0((pdom != NULL && !pdom->is_empty()) || !bb->isLiveOut(gsr));
        for (UINT bbid = occ->get_first();
             bbid != 0; bbid = occ->get_next(bbid)) {
            if (bbid == ORBB_id(bb)) {
                continue;
            }

            if (pdom->is_contain(bbid)) {
                return true;
            }
        }
    }
    return false;
}


void ParallelPartMgr::genReductionRestore(SR * red_var)
{
    List<SR*> dest_tn;
    List<CLUST>    dest_clust;
    //The value of 0 indicates cluster-1 as default.
    for (UINT i = 0; i < m_num_cluster; i++) {
        if (i == getFirstClusterIdx()) {
            continue;
        }
        SR2SR_DMAP * dmap = getClusterDMap(i);
        SR * dupsr = dmap->get(red_var);
        ASSERT0(dupsr);

        //Copy from the results SR of each parallel-parts in first-part.
        SR * tmp = m_cg->dupSR(red_var);
        dest_clust.append_tail(getCluster(getFirstClusterIdx()));
        dest_tn.append_tail(tmp);
        genBusCopy(m_epilog, getCluster(i),
            dest_clust, dupsr, dest_tn);
        m_bb->setLiveOut(dupsr);
        m_epilog->setLiveIn(dupsr);

        //Generate accumulating operation.
        OR * red_or = m_red_mgr.get(red_var);
        ASSERT0(red_or);
        ORList ors;
        m_cg->buildAccumulate(red_or, red_var, tmp, ors);
        ORBB_orlist(m_epilog)->append_tail(ors);
    }
}


//Generate prolog and epilog ORBB if needed.
//'red_or': reduction operation of induction variable
//'iv': induction variable
void ParallelPartMgr::genPrologAndEpilog()
{
    ASSERTN(0, ("Target Dependent Code"));
}


//Verify the legality of parallelizing of loop.
bool ParallelPartMgr::verifyReductionOR()
{
    for (OR * o = ORBB_first_or(m_bb); o; o = ORBB_next_or(m_bb)) {
        CLUST op_clst = m_cg->computeORCluster(o);
        if (op_clst != CLUST_UNDEF && op_clst != CLUST_FIRST) {
            return false;
        }

        //CASE 2: Reduction operations only support ADD and SUB.
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (SR_is_global(sr)) {
                if (m_bb->isLiveOut(sr) &&
                    hasPDomOcc(m_bb, sr) &&
                    ORBB_liveout(m_bb).is_contain(SR_sregid(sr))) {
                    if (!m_cg->isReductionOR(o)) {
                        return false;
                    }
                    //alway set the last DEF to be reduction-o.
                    m_red_mgr.set(sr, o);
                }
                m_red_mgr.set_live_def(sr, o);
            }
        }
    }
    return true;
}


//Verify the legality of parallelizing the loop, and
//do some preparation of distribution.
bool ParallelPartMgr::prepare_distribute(OR * red_or, OR * cmp_or, SR * iv)
{
    ASSERTN(m_pool, ("not yet initialize."));
    if (numOfParallelPart() == 0) {
        return false;
    }
    if (!verifyReductionOR()) {
        return false;
    }
    if (!red_or || !cmp_or || !iv) {
        //ORBB's dependence graph.
        DataDepGraph ddg;
        ddg.set_param(NO_PHY_REG,
                      NO_MEM_READ,
                      INC_MEM_FLOW,
                      INC_MEM_OUT,
                      INC_CONTROL,
                      NO_REG_READ,
                      INC_REG_ANTI,
                      INC_MEM_ANTI,
                      INC_SYM_REG);
        ddg.init(m_bb);
        ddg.build();
        findMainIV(m_bb, ddg, &red_or, &cmp_or, &iv);
        ASSERT0(red_or && iv && cmp_or && SR_is_global(iv));
    }
    m_main_red_or = red_or;
    m_iv = iv;
    m_cmp_or = cmp_or;
    dupORForParallelPart();
    return true;
}


//This function will finish the following tasks:
//    1.Duplicate operations for each of parallel parts,
//      and also rename sr.
//    2.Recognizing reduction operations.
void ParallelPartMgr::dupORForParallelPart()
{
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERT0(m_main_red_or && m_cmp_or);
    UINT i;
    for (i = 0; i < numOfParallelPart(); i++) {
        List<OR*> * dist_or_list =
            (List<OR*>*)xmalloc(sizeof(ORList));
        dist_or_list->init();
        List<OR*> * red_or_list =
            (List<OR*>*)xmalloc(sizeof(ORList));
        red_or_list->init();
        xcom::BitSet * op_idxset = (xcom::BitSet*)xmalloc(sizeof(xcom::BitSet));
        op_idxset->init();
        //call for generating _vfptr
        SR2SR_DMAP * dist_sr2sr_dmap = new(SR2SR_DMAP);

        m_para_part_orlst.append_tail(dist_or_list);
        m_para_part_redor_lst.append_tail(red_or_list);
        m_para_part_oridx_lst.append_tail(op_idxset);
        m_sr2sr_dmap_lst.append_tail(dist_sr2sr_dmap);
    }

    //Duplicate bb without branch, compare, reduction o.
    UINT first_cluster_idx = getFirstClusterIdx();
    List<OR*> * first_part_or_list =
       m_para_part_orlst.get_head_nth(first_cluster_idx);
    List<OR*> * first_part_redop_list =
        m_para_part_redor_lst.get_head_nth(first_cluster_idx);
    for (OR * o = ORBB_first_or(m_bb); o;
         o = ORBB_next_or(m_bb)) {
        if (!OR_is_br(o) &&
            //o != m_main_red_or &&
            o != m_cmp_or) {
            first_part_or_list->append_tail(o);
            if (m_cg->isReduction(o)) { //record other red-ors
                first_part_redop_list->append_tail(o);
            }
        }

    }

    //Duplicate ors for other parallel part.
    for (i = 0; i < numOfParallelPart(); i++) {
        if (i == first_cluster_idx) {
            continue;
        }
        List<OR*> * or_lst = m_para_part_orlst.get_head_nth(i);
        List<OR*> * redor_lst = m_para_part_redor_lst.get_head_nth(i);
        SR2SR_DMAP * dmap = m_sr2sr_dmap_lst.get_head_nth(i);
        //xcom::BitSet * op_idxset = m_para_part_oridx_lst.get_head_nth(i);

        for (OR * orig_or = first_part_or_list->get_head(); orig_or;
             orig_or = first_part_or_list->get_next()) {
            OR * o = m_cg->dupOR(orig_or);
            or_lst->append_tail(o);
            if (m_cg->isReduction(orig_or)) {
                redor_lst->append_tail(o);
            }
            renameSR(o, *dmap);
        }
    }
}


//Distribute OPs into parallel part of 'n'.
bool ParallelPartMgr::distributeToCluster(UINT n)
{
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERT0(n < m_num_cluster);
    xcom::BitSet * oridx_lst = NULL;
    List<OR*> * or_lst = getClusterParallelPart(n, &oridx_lst);
    ASSERT0(or_lst && oridx_lst);
    //SR2SR_DMAP * dmap = getClusterDMap(n);
    //Add new GSR into ORBB live-in, live-out, live-defreachin,
    //live-defreachout to maintain data flow info.
    CLUST to_clust = getCluster(n);
    OR * o;
    for (o = or_lst->get_head(); o; o = or_lst->get_next()) {
        if (!m_cg->changeORCluster(o, to_clust, m_regfile_unique, false)) {
            UNREACHABLE();
        }
    }
    if (n != getFirstClusterIdx()) { //Clus1 is to run the main part.
        //Insert ors at start of ORBB.
        ORBB_orlist(m_bb)->append_head(*or_lst);
    }
    //OR map idx allocate after append into ORBB.
    for (o = or_lst->get_head(); o; o = or_lst->get_next()) {
        oridx_lst->bunion(OR_id(o));
    }
    return true;
}


CLUST ParallelPartMgr::getCluster(UINT n) const
{
    ASSERTN(n == CLUST_FIRST, ("Target Dependent Code"));
    return (CLUST)n;
}


UINT ParallelPartMgr::getFirstClusterIdx() const
{
    ASSERT_DUMMYUSE(CLUST_FIRST + 1 == CLUST_NUM, ("Target Dependent Code"));
    return 0;
}


INT ParallelPartMgr::getClusterIdx(CLUST clt) const
{
    ASSERTN(clt == CLUST_FIRST, ("Target Dependent Code"));
    return 0;
}


//Distribute OPs into parallel part.
bool ParallelPartMgr::distribute()
{
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERT0(numOfParallelPart() > 0);
    if (m_num_cluster > 0) {
        for (UINT i = 0; i < m_num_cluster; i++) {
            if (!distributeToCluster(i)) {
                return false;
            }
        }
    }
    genPrologAndEpilog();
    if (!modifyReductionOR()) {
        UNREACHABLE();
    }
    return true;
}


void ParallelPartMgr::computeUniqueRegFile(
        OR * o,
        Vector<bool> & is_regfile_unique)
{
    ASSERTN(m_pool, ("not yet initialize."));
    UINT i;
    for (i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (!m_cg->isIntRegSR(o, sr, i, true)) {
            continue;
        }
        if (is_regfile_unique.get(SR_sregid(sr))) {
            continue;
        }
        if (SR_phy_regid(sr) != REG_UNDEF) {
            SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
            ASSERTN(SR_regfile(sr) != RF_UNDEF, ("Unknown regfile"));
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }

        //Can allocate global register over again.
        //if (SR_is_global(sr)) {
        //    is_regfile_unique.set(SR_sregid(sr), true);
        //    continue;
        //}

        if (SR_is_dedicated(sr)) {
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }
        if (SR_is_pred(sr)) {
            SR_regfile(sr) = m_cg->getPredicateRegfile();
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }
        if (SR_is_rflag(sr)) {
            SR_regfile(sr) = m_cg->getRflagRegfile();
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }
        if (OR_is_asm(o)) {
            if (SR_phy_regid(sr) != REG_UNDEF ||
                SR_regfile(sr) != RF_UNDEF) {
                is_regfile_unique.set(SR_sregid(sr), true);
                continue;
            }
        }
    }

    for (i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd( i);
        if (!m_cg->isIntRegSR(o, sr, i, false)) {
            continue;
        }
        if (is_regfile_unique.get(SR_sregid(sr))) {
            continue;
        }
        if (SR_phy_regid(sr) != REG_UNDEF) {
            SR_regfile(sr) = tmMapReg2RegFile(SR_phy_regid(sr));
            ASSERTN(SR_regfile(sr) != RF_UNDEF,
                    ("Unknown regfile"));
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }

        //Can allocate global register over again.
        //if (SR_is_global(sr)) {
        //    is_regfile_unique.set(SR_sregid(sr), true);
        //    continue;
        //}

        if (SR_is_dedicated(sr)) {
            is_regfile_unique.set(SR_sregid(sr), true);
            continue;
        }
        if (OR_is_asm(o)) {
            if (SR_phy_regid(sr) != REG_UNDEF ||
                SR_regfile(sr) != RF_UNDEF) {
                is_regfile_unique.set(SR_sregid(sr), true);
                continue;
            }
        }//end if
    }//end for
}


//Determine the number of clusters in which code can be distributed.
void ParallelPartMgr::computeNumOfParallelPart()
{
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERT_DUMMYUSE(CLUST_FIRST + 1 == CLUST_NUM, ("Target Dependent Code"));
    m_num_cluster = CLUST_NUM;
}


//Change constant step to a factor of 'mul' of it.
//e.g: Given the o as:
//    t1 = t1 + n
//    the output o is:
//    t1 = t1 + mul*n
//'mul': is a multiple of step-sr
bool ParallelPartMgr::modifyReductionOR(OR * o, INT mul)
{
    ASSERTN(m_cg->isReductionOR(o), ("o is not reducible"));
    SR * step = o->get_opnd(2);
    ASSERT0(step && SR_is_imm(step));
    //TODO: Support multiply when step is variant.
    ASSERT0(SR_is_constant(step));
    SR * new_step = m_cg->genIntImm((HOST_INT)SR_int_imm(step) * mul, true);
    o->set_opnd(2, new_step);
    return true;
}


//Find induction variable
SR * ParallelPartMgr::findIV(OR * o)
{
    for (UINT j = 0; j < o->result_num(); j++) {
        SR * res = o->get_result( j);
        if (!m_cg->isIntRegSR(o, res, j, true) ||
            SR_is_pred(res) ||
            !SR_is_global(res)) {
            continue;
        }
        if (ORBB_cg(m_bb)->mustUse(o, res)) {
            return res;
        }
    }
    return NULL;
}


//Modify the increment of reduction operations.
//Return true if modification is legal, otherwise return false.
//e.g: orig loop, increment is 2, and multiple of increment is 3:
//        i=0
//        start:
//        ...
//        i=i+2
//        b start
//    finial loop:
//        prolog:
//        ...
//        main_loop_tart:
//        ...            |...        |...
//        i2=i2+3*2    |i=i+3*2    |i3=i3+3*2
//        b main_loop_start
bool ParallelPartMgr::modifyReductionOR()
{
    ASSERTN(m_pool, ("not yet initialize."));
    ASSERTN(m_num_cluster > 0, ("Not any para part"));
    for (UINT i = 0; i < m_num_cluster; i++) {
        List<OR*> * red_or_lst = getClusterReductionOR(i);
        ASSERT0(red_or_lst);
        for (OR * o = red_or_lst->get_head(); o;
             o = red_or_lst->get_next()) {
            bool succ = modifyReductionOR(o, numOfParallelPart());
            CHECK_DUMMYUSE(succ);
        }
    }//end for
    return true;
}
///END ParallelPartMgr

} //namespace xgen
