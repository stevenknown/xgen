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


//Simply IR to lower level IR, and insert CVT if needed.
void ARMRegion::simplify(OptCtx & oc)
{
    ASSERTN(getPassMgr(), ("need PassMgr and IRMgr"));
    //Note PRSSA and MDSSA are unavailable.
    if (getBBList() == nullptr || getBBList()->get_elem_count() == 0) {
        return;
    }

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
        CfgOptCtx ctx(oc);
        getCFG()->removeEmptyBB(ctx);

        getCFG()->rebuild(oc);

        //After CFG building.
        //Remove empty bb when cfg rebuilted because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        getCFG()->removeEmptyBB(ctx);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        getCFG()->performMiscOpt(oc);
    } else if (g_do_md_du_analysis || g_do_mdssa) {
        ASSERT0(verifyIRandBB(getBBList(), this));
    }

    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    //for generated code.
    ASSERTN(g_do_refine, ("inserting CVT is expected"));
    RefineCtx rf(&oc);
    RC_insert_cvt(rf) = g_do_refine_auto_insert_cvt;
    Refine * refine = (Refine*)getPassMgr()->registerPass(PASS_REFINE);
    refine->refineBBlist(getBBList(), rf);
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
        CfgOptCtx ctx(oc);
        getCFG()->removeEmptyBB(ctx);

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

    //Assign PR and NonPR Var and MD sperately to make MD id more
    //grouped together.
    getMDMgr()->assignMD(false, true);
    getMDMgr()->assignMD(true, false);
    if (g_do_aa) {
        ASSERT0(g_cst_bb_list && oc.is_cfg_valid());
        getPassMgr()->checkValidAndRecompute(&oc, PASS_DOM, PASS_LOOP_INFO,
                                             PASS_AA, PASS_UNDEF);
    }
    if (g_do_md_du_analysis) {
        ASSERT0(g_cst_bb_list && oc.is_cfg_valid() && oc.is_aa_valid());
        ASSERT0(getPassMgr());
        DUMgr * dumgr = (DUMgr*)getPassMgr()->registerPass(PASS_DU_MGR);
        ASSERT0(dumgr);
        DUOptFlag f(DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF);
        if (g_compute_region_imported_defuse_md) {
            f.set(DUOPT_SOL_REGION_REF);
        }
        if (g_compute_pr_du_chain) {
            f.set(DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU);
        }
        if (g_compute_nonpr_du_chain) {
            f.set(DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_NONPR_DU);
        }
        bool succ = dumgr->perform(oc, f);
        ASSERT0(oc.is_ref_valid());
        if (f.have(DUOPT_SOL_REACH_DEF) && succ) {
            dumgr->computeMDDUChain(oc, false, f);
        }
        bool rmprdu = false;
        bool rmnonprdu = false;
        if (g_do_prssa) {
            ((PRSSAMgr*)getPassMgr()->registerPass(PASS_PRSSA_MGR))->
                construction(oc);
            //If SSA is enabled, disable classic DU Chain.
            //Since we do not maintain both them as some passes.
            //e.g:In RCE, remove PHI's operand will not update the
            //operand DEF's DUSet.
            //CASE:compiler.gr/alias.loop.gr
            oc.setInvalidPRDU();
            rmprdu = true;
        }
        if (g_do_mdssa) {
            ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MDSSA_MGR))->
                construction(oc);
            //If SSA is enabled, disable classic DU Chain.
            //Since we do not maintain both them as some passes.
            //e.g:In RCE, remove PHI's operand will not update the
            //operand DEF's DUSet.
            //CASE:compiler.gr/alias.loop.gr
            oc.setInvalidNonPRDU();
            rmnonprdu = true;
        }
        xoc::removeClassicDUChain(this, rmprdu, rmnonprdu);
        if (g_do_refine_duchain) {
            RefineDUChain * refdu = (RefineDUChain*)getPassMgr()->
                registerPass(PASS_REFINE_DUCHAIN);
            if (g_compute_pr_du_chain && g_compute_nonpr_du_chain) {
                refdu->setUseGvn(true);
                GVN * gvn = (GVN*)getPassMgr()->registerPass(PASS_GVN);
                gvn->perform(oc);
            } else {
                //TODO: do GVN according to SSA.
                refdu->setUseGvn(false);
            }
            refdu->perform(oc);
        }
    }
}


//This function perform aggressive AA and DU analysis.
//1. Perform flow-insensitive AA.
//2. Perform PR DU chain.
//3. Perform flow-sensitive AA.
//4. Perform both classic DU chain and MDSSA DU chain.
void ARMRegion::MiddleProcessAggressiveAnalysis(OptCtx & oc)
{
    if (g_opt_level == OPT_LEVEL0) { return; }

    START_TIMER(t, "Middle Process Aggressive Analysis");
    //Costly code, to force recomputing AA and DUChain.
    //AA and DU Chain are dispensible to recompute, because
    //simplification maintained them.
    bool org_compute_pr_du_chain = g_compute_pr_du_chain;
    bool org_compute_nonpr_du_chain = g_compute_nonpr_du_chain;
    OC_is_ref_valid(oc) = false;
    OC_is_pr_du_chain_valid(oc) = false;
    OC_is_nonpr_du_chain_valid(oc) = false;
    AliasAnalysis * aa = getAA();
    if (g_do_aa && aa != nullptr) {
        //Recompute and set MD reference to avoid AA's complaint.
        //Compute AA to build coarse-grained DU chain.
        getMDMgr()->assignMD(true, true);
        if (!aa->is_init()) {
            aa->initAliasAnalysis();
        }
        aa->set_flow_sensitive(false);
        aa->perform(oc);

        //Compute PR's definition to improve AA precison.
        if (g_do_prssa) {
            ASSERT0(getPassMgr());
            PRSSAMgr * prssamgr = (PRSSAMgr*)getPassMgr()->registerPass(
                PASS_PRSSA_MGR);
            ASSERT0(prssamgr);
            if (!prssamgr->is_valid()) {
                prssamgr->construction(oc);
            }
            //If SSA is enabled, disable classic DU Chain.
            //Since we do not maintain both them as some passes.
            //e.g:In RCE, remove PHI's operand will not update the
            //operand DEF's DUSet.
            //CASE:compiler.gr/alias.loop.gr
            oc.setInvalidPRDU();
        } else if (getDUMgr() != nullptr) {
            //Do not check options, because user may ask neither PRSSA nor
            //ClassicPRDU. Apply ClassicPRDU analysis in silent mode.
            //ASSERT0(g_compute_pr_du_chain);
            getDUMgr()->perform(oc,
                                DUOptFlag(DUOPT_COMPUTE_PR_REF|
                                          DUOPT_COMPUTE_NONPR_REF|
                                          DUOPT_SOL_REACH_DEF|
                                          DUOPT_COMPUTE_PR_DU));
            getDUMgr()->computeMDDUChain(oc, false,
                                         DUOptFlag(DUOPT_COMPUTE_PR_DU));
        }
        //checkValidAndRecompute(&oc, PASS_LOOP_INFO, PASS_UNDEF);
        //Recompute and set MD reference to avoid AA's complaint.
        getMDMgr()->assignMD(true, true);

        //Compute the threshold to perform AA.
        UINT numir = 0;
        for (IRBB * bb = getBBList()->get_head();
             bb != nullptr; bb = getBBList()->get_next()) {
            numir += bb->getNumOfIR();
        }
        aa->set_flow_sensitive(numir < xoc::g_thres_opt_ir_num);
        aa->perform(oc);
    }

    DUMgr * dumgr = getDUMgr();
    if (g_do_md_du_analysis && dumgr != nullptr) {
        DUOptFlag flag(DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF);
        if (g_compute_pr_du_chain) {
            flag.set(DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU);
        }
        if (g_compute_nonpr_du_chain) {
            flag.set(DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_NONPR_DU);
        }
        dumgr->perform(oc, flag);
        if (oc.is_ref_valid() && OC_is_reach_def_valid(oc)) {
            dumgr->computeMDDUChain(oc, false, flag);
        }
        bool rmprdu = false;
        bool rmnonprdu = false;
        if (g_do_prssa) {
            ASSERT0(getPassMgr());
            PRSSAMgr * prssamgr = (PRSSAMgr*)getPassMgr()->registerPass(
                PASS_PRSSA_MGR);
            ASSERT0(prssamgr);
            if (!prssamgr->is_valid()) {
                prssamgr->construction(oc);
            }
            //If SSA is enabled, disable classic DU Chain.
            //Since we do not maintain both them as some passes.
            //e.g:In RCE, remove PHI's operand will not update the
            //operand DEF's DUSet.
            //CASE:compiler.gr/alias.loop.gr
            oc.setInvalidPRDU();
            rmprdu = true;
        }
        if (g_do_mdssa) {
            //MDSSA have to be recomputed when DURef and overlap set
            //are available.
            ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MDSSA_MGR))->
                construction(oc);
            //If SSA is enabled, disable classic DU Chain.
            //Since we do not maintain both them as some passes.
            //e.g:In RCE, remove PHI's operand will not update the
            //operand DEF's DUSet.
            //CASE:compiler.gr/alias.loop.gr
            oc.setInvalidNonPRDU();
            rmnonprdu = true;
        }
        xoc::removeClassicDUChain(this, rmprdu, rmnonprdu);
    }
    g_compute_pr_du_chain = org_compute_pr_du_chain;
    g_compute_nonpr_du_chain = org_compute_nonpr_du_chain;
    END_TIMER(t, "Middle Process Aggressive Analysis");
}


bool ARMRegion::MiddleProcess(OptCtx & oc)
{
    //Must and May MD reference should be available now.
    //First prefer to perform higher level analysis and optization.
    Region::MiddleProcess(oc);
    if (g_do_cp) {
        getPassMgr()->registerPass(PASS_CP)->perform(oc);
    }
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
    ASSERT0(verifyMDDUChain(this, oc));

    //Destruct SSA mode and resource.
    PRSSAMgr * ssamgr = (PRSSAMgr*)getPRSSAMgr();
    if (ssamgr != nullptr && ssamgr->is_valid()) {
        //NOTE: ssa destruction might violate classic DU chain.
        ssamgr->destruction(oc);

        //Note PRSSA will change PR no during PRSSA destruction.
        //If classic DU chain is valid meanwhile, it might be disrupted
        //as well. A better way is user
        //maintain the classic DU chain, alternatively a conservative way to
        //avoid subsequent verification complaining is set the prdu invalid.
        //WORKAROUND: use better way to update classic DU chain rather
        //than invalid them.
        OC_is_pr_du_chain_valid(oc) = false;
        OC_is_nonpr_du_chain_valid(oc) = false;
    }
    ASSERT0(verifyMDDUChain(this, oc));

    ////////////////////////////////////////////////////////////////////////////
    //DO NOT DO OPTIMIZATION ANY MORE AFTER THIS LINE                         //
    ////////////////////////////////////////////////////////////////////////////

    //Finial refine to insert CVT if necessary when compile C/C++.
    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    ASSERTN(g_do_refine, ("inserting CVT is expected"));
    RefineCtx rf(&oc);
    RC_insert_cvt(rf) = g_do_refine_auto_insert_cvt;
    Refine * refine = (Refine*)getPassMgr()->registerPass(PASS_REFINE);
    refine->refineBBlist(getBBList(), rf);
    #ifdef REF_TARGMACH_INFO
    if (g_do_lsra) {
        LinearScanRA * lsra = (LinearScanRA*)getPassMgr()->registerPass(
            PASS_LINEAR_SCAN_RA);
        lsra->setApplyToRegion(false);
        lsra->perform(oc);
    }
    #endif
    return true;
}


bool ARMRegion::HighProcess(OptCtx & oc)
{
    bool own = true;
    if (own) {
        ARMHighProcess(oc);
    } else {
        Region::HighProcess(oc);
    }
    return true;
}
