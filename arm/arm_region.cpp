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

extern void resetMapBetweenVARandDecl(Var * v);

void ARMRegion::destroy()
{
    #ifdef _DEBUG_
    //This map info only used in dump().
    VarTabIter c;
    VarTab * vartab = getVarTab();
    for (Var * v = vartab->get_first(c);
         v != nullptr; v = vartab->get_next(c)) {
        resetMapBetweenVARandDecl(v);
    }
    #endif

    Region::destroy();
}


PassMgr * ARMRegion::allocPassMgr()
{
    return new ARMPassMgr(this);
}


//To simply IR_SELECT etc, and Insert CVT if needed.
void ARMRegion::simplify(OptCtx & oc)
{
    //Note PRSSA and MDSSA are unavailable.
    if (getBBList() == nullptr || getBBList()->get_elem_count() == 0) {
        return;
    }

    SimpCtx simp;
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
    simplifyBBlist(getBBList(), &simp);
    if (g_do_cfg && g_cst_bb_list && SIMP_need_recon_bblist(&simp) &&
        reconstructBBList(oc) && cfg_is_valid) {

        //Simplification may generate new memory operations.
        ASSERT0(verifyMDRef());

        //Before CFG building.
        getCFG()->removeEmptyBB(oc);

        getCFG()->rebuild(oc);

        //After CFG building.
        //Remove empty bb when cfg rebuilted because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        getCFG()->removeEmptyBB(oc);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        getCFG()->performMiscOpt(oc);
    } else if (g_do_md_du_analysis || g_do_md_ssa) {    
        ASSERT0(verifyMDRef());
        ASSERT0(verifyIRandBB(getBBList(), this));
    }

    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    //for generated code.
    ASSERTN(g_do_refine, ("inserting CVT is expected"));
    RefineCtx rf;
    RC_insert_cvt(rf) = g_do_refine_auto_insert_cvt;
    if (getPassMgr() == nullptr) {
        initPassMgr();
    }
    Refine * refine = (Refine*)getPassMgr()->registerPass(PASS_REFINE);
    refine->refineBBlist(getBBList(), rf, oc);
}


//Test code, to force generating as many IR stmts as possible.
//g_is_lower_to_pr_mode = true;
bool ARMRegion::simplifyToPRmode(OptCtx & oc)
{
    ASSERT0(getCFG()->verifyRPO(oc));
    ASSERT0(getPassMgr());
    BBList * bbl = getBBList();
    if (bbl->get_elem_count() == 0) {
        return true;
    }
    SimpCtx simp;
    simp.setSimpCFS();
    simp.setSimpArray();
    simp.setSimpSelect();
    simp.setSimpLandLor();
    simp.setSimpLnot();
    simp.setSimpILdISt();
    simp.setSimpToLowestHeight();
    ASSERT0(verifyIRandBB(getBBList(), this));
    ASSERT0(verifyMDDUChain(this));
    simplifyBBlist(bbl, &simp);
    if (SIMP_need_recon_bblist(&simp) && g_cst_bb_list &&
        reconstructBBList(oc)) {
        ASSERTN(g_do_cfg, ("construct BB list requires CFG"));
        IRCFG * cfg = getCFG();
        ASSERT0(cfg);
        cfg->rebuild(oc);

        //Remove empty bb when cfg rebuilt because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        cfg->removeEmptyBB(oc);

        //Compute entry bb, exit bb while cfg rebuilt.
        cfg->computeExitList();
        ASSERT0(cfg->verify());
        cfg->performMiscOpt(oc);
    }
    return true;
}


bool ARMRegion::ARMHighProcess(OptCtx & oc)
{
    SimpCtx simp;
    if (g_do_cfs_opt) {
        CfsOpt co(this);
        co.perform(simp);
        ASSERT0(verifyIRList(getIRList(), nullptr, this));
    }

    //Simplify control-structure to build CFG.
    simp.setSimpCFS();
    setIRList(simplifyStmtList(getIRList(), &simp));
    ASSERT0(verifySimp(getIRList(), simp));
    ASSERT0(verifyIRList(getIRList(), nullptr, this));
    //partitionRegion(); //Split region.

    //Lower to middle in order to perform analysis.
    if (g_cst_bb_list) {
        constructBBList();
        ASSERT0(verifyIRandBB(getBBList(), this));
    }

    initPassMgr();
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
        getCFG()->removeEmptyBB(oc);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        if (g_do_cfg_dom) {
            //Infer pointer arith requires loopinfo.
            getPassMgr()->checkValidAndRecompute(&oc, PASS_DOM, PASS_UNDEF);
        }

        if (g_do_loop_ana) {
            //Infer pointer arith requires loopinfo.
            getPassMgr()->checkValidAndRecompute(&oc, PASS_LOOP_INFO,
                                                 PASS_UNDEF);
        }

        getCFG()->performMiscOpt(oc);
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
        UINT f = DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF;
        if (g_compute_region_imported_defuse_md) {
            f |= DUOPT_SOL_REGION_REF;
        }
        if (g_compute_pr_du_chain) {
            f |= DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU;
        }
        if (g_compute_nonpr_du_chain) {
            f |= DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_NONPR_DU;
        }
        bool succ = dumgr->perform(oc, f);
        ASSERT0(oc.is_ref_valid());
        if (HAVE_FLAG(f, DUOPT_SOL_REACH_DEF) && succ) {
            dumgr->computeMDDUChain(oc, false, f);
        }

        if (g_do_pr_ssa) {
            ((PRSSAMgr*)getPassMgr()->registerPass(PASS_PR_SSA_MGR))->
                construction(oc);
            if (getDUMgr() != nullptr && !oc.is_du_chain_valid()) {
                //PRSSAMgr will destruct classic DU-chain.
                getDUMgr()->cleanDUSet();
                oc.setInvalidDUChain();
            }
        }
        if (g_do_md_ssa) {
            ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MD_SSA_MGR))->
                construction(oc);
        }
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

    //START HACK CODE
    //Test code, to force recomputing AA and DUChain.
    //AA and DU Chain do NOT to be recomputed, because
    //simplification have maintained them.
    bool org_compute_pr_du_chain = g_compute_pr_du_chain;
    bool org_compute_nonpr_du_chain = g_compute_nonpr_du_chain;
    OC_is_ref_valid(oc) = false;
    OC_is_pr_du_chain_valid(oc) = false;
    OC_is_nonpr_du_chain_valid(oc) = false;
    //g_compute_pr_du_chain = true;
    //g_compute_nonpr_du_chain = true;
    //END HACK CODE

    if (!oc.is_aa_valid() ||
        !oc.is_ref_valid() ||
        !oc.is_pr_du_chain_valid() ||
        !oc.is_nonpr_du_chain_valid()) {
        AliasAnalysis * aa = getAA();
        if (g_do_aa && aa != nullptr &&
            (!oc.is_aa_valid() || !oc.is_ref_valid())) {
            //Recompute and set MD reference to avoid AA's complaint.
            //Compute AA to build coarse-grained DU chain.
            getMDMgr()->assignMD(true, true);
            if (!aa->is_init()) {
                aa->initAliasAnalysis();
            }
            aa->set_flow_sensitive(false);
            aa->perform(oc);
        }

        //Opt phase may cause AA invalid.
        if (g_do_aa && aa != nullptr) {
            //Compute PR's definition to improve AA precison.
            if (g_do_pr_ssa) {
                ASSERT0(getPassMgr());
                PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->registerPass(
                    PASS_PR_SSA_MGR);
                ASSERT0(ssamgr);
                if (!ssamgr->is_valid()) {
                    ssamgr->construction(oc);
                    if (getDUMgr() != nullptr && !oc.is_du_chain_valid()) {
                        //PRSSAMgr will destruct classic DU-chain.
                        getDUMgr()->cleanDUSet();
                        oc.setInvalidDUChain();
                    }
                }
            } else if (getDUMgr() != nullptr) {
                getDUMgr()->perform(oc, DUOPT_COMPUTE_PR_REF|
                    DUOPT_COMPUTE_NONPR_REF|DUOPT_SOL_REACH_DEF|
                    DUOPT_COMPUTE_PR_DU);
                getDUMgr()->computeMDDUChain(oc, false, DUOPT_COMPUTE_PR_DU);
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
            UINT flag = DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF;
            if (g_compute_pr_du_chain) {
                SET_FLAG(flag, DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU);
            }
            if (g_compute_nonpr_du_chain) {
                SET_FLAG(flag, DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_NONPR_DU);
            }
            dumgr->perform(oc, flag);
            if (oc.is_ref_valid() && OC_is_reach_def_valid(oc)) {
                dumgr->computeMDDUChain(oc, false, flag);
            }
            if (g_do_md_ssa) {
                //MDSSA have to be recomputed when DURef and overlap set
                //are available.
                ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MD_SSA_MGR))->
                    construction(oc);
            }
        }
    }
    g_compute_pr_du_chain = org_compute_pr_du_chain;
    g_compute_nonpr_du_chain = org_compute_nonpr_du_chain;
    END_TIMER(t, "Middle Process Aggressive Analysis");
}


bool ARMRegion::MiddleProcess(OptCtx & oc)
{
    //Must and May MD reference should be available now.
    if (g_is_lower_to_pr_mode) {
        simplifyToPRmode(oc);
    } else {
        Region::MiddleProcess(oc);
        if (g_do_cp && getPassMgr()->queryPass(PASS_DU_MGR) != nullptr) {
            ((CopyProp*)getPassMgr()->registerPass(PASS_CP))->perform(oc);
        }
    }

    if (g_do_cfg_dom && !OC_is_dom_valid(oc)) {
        getCFG()->computeDomAndIdom(oc);
    }
    if (g_do_cfg_pdom && !OC_is_pdom_valid(oc)) {
        getCFG()->computePdomAndIpdom(oc);
    }

    MiddleProcessAggressiveAnalysis(oc);

    g_do_gcse = false;
    if (g_do_gcse && g_opt_level > OPT_LEVEL0) {
        MDSSAMgr * mdssamgr = (MDSSAMgr*)getPassMgr()->registerPass(
            PASS_MD_SSA_MGR);
        ASSERT0(mdssamgr);
        if (!mdssamgr->is_valid()) {
            mdssamgr->construction(oc);
        }
        if (getPassMgr()->queryPass(PASS_DU_MGR) != nullptr) {
            GCSE * pass = (GCSE*)getPassMgr()->registerPass(PASS_GCSE);
            bool changed = false;
            changed = pass->perform(oc);
            if (changed) {
                getDUMgr()->perform(oc, DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU|
                    DUOPT_COMPUTE_NONPR_DU);
                OC_is_pr_du_chain_valid(oc) = false;
                OC_is_nonpr_du_chain_valid(oc) = false;
                getDUMgr()->computeMDDUChain(oc, false,
                    DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF|
                    DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU|
                    DUOPT_COMPUTE_NONPR_DU);
            }
        }
    }

    ASSERT0((!g_do_md_du_analysis && !g_do_md_ssa) || verifyMDRef());
    ASSERT0(verifyIRandBB(getBBList(), this));
    if (oc.is_pr_du_chain_valid() || oc.is_nonpr_du_chain_valid()) {
        ASSERT0(verifyMDDUChain(this));
    }

    PRSSAMgr * ssamgr = (PRSSAMgr*)getPRSSAMgr();
    if (ssamgr != nullptr && ssamgr->is_valid()) {
        //NOTE: ssa destruction might violate classic DU chain.
        ssamgr->destruction(&oc);

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

    UINT duflag = oc.is_pr_du_chain_valid() ? DUOPT_COMPUTE_PR_DU : 0;
    duflag |= oc.is_nonpr_du_chain_valid() ? DUOPT_COMPUTE_NONPR_DU : 0;
    ASSERT0(verifyMDDUChain(this, duflag));

    ///////////////////////////////////////////////////
    //DO NOT DO OPTIMIZATION ANY MORE AFTER THIS LINE//
    ///////////////////////////////////////////////////

    //Finial refine to insert CVT if necessary when compile C/C++.
    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    ASSERTN(g_do_refine, ("inserting CVT is expected"));
    RefineCtx rf;
    RC_insert_cvt(rf) = g_do_refine_auto_insert_cvt;
    Refine * refine = (Refine*)getPassMgr()->registerPass(PASS_REFINE);
    refine->refineBBlist(getBBList(), rf, oc);
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
