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
#include "../xgen/xgeninc.h"

PassMgr * ARMRegion::allocPassMgr()
{
    return new ARMPassMgr(this);
}


bool ARMRegion::isParticipateInOpt() const
{
    if (g_exclude_region.find(getRegionName())) { return false; }
    if (g_include_region.isEmpty()) { return true; }
    return g_include_region.find(getRegionName());
}


//Simply IR to lower level IR, and insert CVT if needed.
void ARMRegion::simplify(OptCtx & oc)
{
    ASSERTN(getPassMgr(), ("need PassMgr and IRMgr"));
    //Note PRSSA and MDSSA are unavailable.
    ASSERT0(getBBList());
    if (getBBList()->is_empty()) { return; }
    SimpCtx simp(&oc);
    simp.setSimpCFS();
    simp.setSimpArray();
    simp.setSimpSelect();
    simp.setSimpLandLor();
    simp.setSimpLnot();
    simp.setSimpILdISt();
    simp.setSimpToLowestHeight();
    if (g_is_lower_to_pr_mode) {
        simp.setSimpToPRmode();
    }
    bool cfg_is_valid = getCFG() != nullptr && oc.is_cfg_valid();
    getIRSimp()->simplifyBBlist(getBBList(), &simp);
    if (g_do_cfg && g_cst_bb_list && SIMP_need_recon_bblist(&simp) &&
        reconstructBBList(oc) && cfg_is_valid) {

        //Simplification may generate new memory operations.
        ASSERT0(getDUMgr() && getDUMgr()->verifyMDRef());

        //Before CFG building.
        IRCfgOptCtx ctx(&oc);
        RemoveEmptyBBCtx rmctx(ctx);
        getCFG()->removeEmptyBB(rmctx);

        getCFG()->rebuild(oc);

        //After CFG building.
        //Remove empty BB when CFG rebuilt because
        //rebuilding CFG may generate redundant empty BB.
        //It disturbs the computation of entry and exit.
        RemoveEmptyBBCtx rmctx2(ctx);
        getCFG()->removeEmptyBB(rmctx2);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        getCFG()->performMiscOpt(oc);
    } else if (g_do_md_du_analysis || g_do_mdssa) {
        ASSERT0(verifyIRandBB(getBBList(), this));
    }
    if (g_insert_cvt) {
        //Finial refine to insert CVT if necessary.
        getPassMgr()->registerPass(PASS_INSERT_CVT)->perform(oc);
    }
}


bool ARMRegion::ARMHighProcess(OptCtx & oc)
{
    SimpCtx simp(&oc);
    if (g_do_cfs_opt) {
        CfsOpt co(this);
        co.perform(simp);
        ASSERT0(verifyIRList(getIRList(), nullptr, this));
    }

    //Simplify control-structure to build CFG.
    simp.setSimpCFS();
    setIRList(getIRSimp()->simplifyStmtList(getIRList(), &simp));
    ASSERT0(verifySimp(getIRList(), simp));
    ASSERT0(verifyIRList(getIRList(), nullptr, this));
    //partitionRegion(); //Split region.

    //Lower to middle in order to perform analysis.
    if (g_cst_bb_list) {
        constructBBList();
        ASSERT0(verifyIRandBB(getBBList(), this));
    }

    initPassMgr();
    initDbxMgr();
    initIRMgr();
    initIRBBMgr();
    HighProcessImpl(oc);
    return true;
}


void ARMRegion::HighProcessImpl(OptCtx & oc)
{
    if (g_do_cfg) {
        ASSERT0(g_cst_bb_list);
        getPassMgr()->checkValidAndRecompute(&oc, PASS_CFG, PASS_UNDEF);

        //Remove empty bb when cfg rebuilted because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        IRCfgOptCtx ctx(&oc);
        RemoveEmptyBBCtx rmctx(ctx);
        getCFG()->removeEmptyBB(rmctx);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        bool org = g_do_cfg_remove_unreach_bb;
        //Unreachable BB have to be removed before RPO computation.
        g_do_cfg_remove_unreach_bb = true;

        getCFG()->performMiscOpt(oc);

        g_do_cfg_remove_unreach_bb = org;

        if (g_do_cfg_dom) {
            //Infer pointer arith requires loopinfo.
            getPassMgr()->checkValidAndRecompute(&oc, PASS_DOM, PASS_UNDEF);
        }

        if (g_do_loop_ana) {
            //Infer pointer arith requires loopinfo.
            getPassMgr()->checkValidAndRecompute(&oc, PASS_LOOP_INFO,
                                                 PASS_UNDEF);
        }
    }
    if (g_infer_type) {
        //Infer type for more precision type.
        getPassMgr()->registerPass(PASS_INFER_TYPE)->perform(oc);
    }
    if (g_insert_cvt) {
        //Finial refine to insert CVT if necessary.
        getPassMgr()->registerPass(PASS_INSERT_CVT)->perform(oc);
    }
    doAA(oc);
    doDUAna(oc);
    if (g_do_rce) {
        //Do RCE and REFINE on higher level IR.
        getPassMgr()->registerPass(PASS_RCE)->perform(oc);
        if (g_do_refine) {
            getPassMgr()->registerPass(PASS_REFINE)->perform(oc);
        }
    }
}


//This function uses classic DU to perform aggressive AA and costly
//DU analysis.
//1. Perform flow-insensitive AA.
//2. Perform PR DU chain.
//3. Perform flow-sensitive AA.
//4. Perform both classic DU chain and MDSSA DU chain.
void ARMRegion::MiddleProcessAggressiveAnalysis(OptCtx & oc)
{
    if (g_opt_level == OPT_LEVEL0 || !isParticipateInOpt()) { return; }
    START_TIMER(t, "Middle Process Aggressive Analysis");

    //Costly code, to force recomputing AA and DUChain.
    //AA and DU Chain are dispensible to recompute, because
    //simplification maintained them.
    bool org_compute_pr_du_chain = g_compute_pr_du_chain;
    bool org_compute_nonpr_du_chain = g_compute_nonpr_du_chain;
    oc.setInvalidPass(PASS_MD_REF);
    oc.setInvalidPass(PASS_AA);
    oc.setInvalidPass(PASS_CLASSIC_DU_CHAIN);
    oc.setInvalidPass(PASS_PRSSA_MGR);
    oc.setInvalidPass(PASS_MDSSA_MGR);
    doAggressiveAA(oc);
    DUMgr * dumgr = getDUMgr();
    if (g_do_md_du_analysis && dumgr != nullptr) {
        DUOptFlag flag(DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF);
        dumgr->perform(oc, flag);
        ASSERT0(oc.is_ref_valid());
        bool orgval = g_compute_pr_du_chain_by_prssa;
        //Force computing PRDU by REACH_DEF.
        g_compute_pr_du_chain_by_prssa = false;
        dumgr->checkAndComputeClassicDUChain(oc);
        g_compute_pr_du_chain_by_prssa = orgval;
        bool changed_prssa = doPRSSA(oc);
        bool changed_mdssa = doMDSSA(oc);
        bool remove_prdu = changed_prssa ? true : false;
        bool remove_nonprdu = changed_mdssa ? true : false;
        xoc::removeClassicDUChain(this, remove_prdu, remove_nonprdu);
    }
    g_compute_pr_du_chain = org_compute_pr_du_chain;
    g_compute_nonpr_du_chain = org_compute_nonpr_du_chain;
    END_TIMER(t, "Middle Process Aggressive Analysis");
}


//The function will check BB or IR list and insert a RETURN if the region
//is empty.
bool ARMRegion::addReturnIfNeed()
{
    if (getIRList() != nullptr) {
        ASSERT0(getBBList()->is_empty());
        return true;
    }
    BBList * bblst = getBBList();
    ASSERT0(bblst);
    BBListIter it;
    for (IRBB * bb = bblst->get_head(&it);
         bb != nullptr; bb = getBBList()->get_next(&it)) {
        if (bb->getIRList().get_elem_count() != 0) {
            return true;
        }
    }
    //The BB list of region is empty.
    IR * ret = getIRMgr()->buildReturn(nullptr);
    bblst->get_tail()->getIRList().append_tail(ret);
    return true;
}


bool ARMRegion::MiddleProcess(OptCtx & oc)
{
    //Must and May MD reference should be available now.
    //First prefer to perform higher level analysis and optization.
    Region::MiddleProcess(oc);
    if (g_do_cfg_dom && !oc.is_dom_valid()) {
        getCFG()->computeDomAndIdom(oc);
    }
    if (g_do_cfg_pdom && !oc.is_pdom_valid()) {
        getCFG()->computePdomAndIpdom(oc);
    }
    MiddleProcessAggressiveAnalysis(oc);

    //May be generated new code that is not satified lowest or PR mode.
    //Gurrantee IR has been simplified as user demand.
    performSimplify(oc);
    if (g_opt_level > OPT_LEVEL0) {
        //Do scalar optimizations after simplification.
        getPassMgr()->registerPass(PASS_SCALAR_OPT)->perform(oc);
    }
    ASSERT0((!g_do_md_du_analysis && !g_do_mdssa) ||
            getDUMgr()->verifyMDRef());
    ASSERT0(verifyIRandBB(getBBList(), this));
    ASSERT0(verifyClassicDUChain(this, oc));
    //Destruct SSA mode and resource.
    PRSSAMgr * ssamgr = (PRSSAMgr*)getPRSSAMgr();
    if (ssamgr != nullptr && ssamgr->is_valid()) {
        //NOTE: SSA destruction might violate classic DU chain.
        ssamgr->destruction(oc);

        //Note PRSSA will change PRNO during PRSSA destruction.
        //If classic DU chain is valid meanwhile, it might be disrupted
        //as well. A better way is user maintain the classic DU chain,
        //alternatively a conservative way to avoid subsequent verification
        //complaining is set the PRDU invalid.
        //WORKAROUND: use better way to update classic DU chain rather
        //than invalid them.
        oc.setInvalidPass(PASS_CLASSIC_DU_CHAIN);
    }
    ASSERT0(verifyClassicDUChain(this, oc));
    doSimplyCPByClassicDU(oc);

    ///////////////////////////////////////////////////////////////////////////
    //DO NOT DO OPTIMIZATION ANY MORE AFTER THIS LINE                        //
    ///////////////////////////////////////////////////////////////////////////
    addReturnIfNeed();
    #ifdef REF_TARGMACH_INFO
    if (g_do_lsra) {
        LinearScanRA * lsra = (LinearScanRA*)getPassMgr()->registerPass(
            PASS_LINEAR_SCAN_RA);
        lsra->setApplyToRegion(true);
        lsra->perform(oc);
    }
    #endif
    return true;
}


bool ARMRegion::HighProcess(OptCtx & oc)
{
    ARMHighProcess(oc);
    return true;
}
