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

#include "../opt/cominc.h"
#include "../opt/comopt.h"
#include "../opt/cfs_opt.h"
#include "../opt/liveness_mgr.h"
#include "../xgen/xgeninc.h"
#include "../cfe/cfexport.h"
#include "../opt/util.h"
#include "../opt/ir_mdssa.h"

extern void resetMapBetweenVARandDecl(Var * v);

void ARMRegion::destroy()
{
    #ifdef _DEBUG_
    //This map info only used in dump().
    VarTabIter c;
    VarTab * vartab = getVarTab();
    for (Var * v = vartab->get_first(c);
         v != NULL; v = vartab->get_next(c)) {
        resetMapBetweenVARandDecl(v);
    }
    #endif

    Region::destroy();
}


PassMgr * ARMRegion::allocPassMgr()
{
    return new ARMPassMgr(this);
}


void ARMRegion::simplify(OptCtx & oc)
{
    //Note PRSSA and MDSSA are unavailable.
    if (getBBList() == NULL || getBBList()->get_elem_count() == 0) {
        return;
    }

    SimpCtx simp;
    simp.setSimpCFS();
    SIMP_if(&simp) = true;
    SIMP_doloop(&simp) = true;
    SIMP_dowhile(&simp) = true;
    SIMP_whiledo(&simp) = true;
    SIMP_switch(&simp) = true;
    SIMP_break(&simp) = true;
    SIMP_continue(&simp) = true;
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

    simplifyBBlist(getBBList(), &simp);
    if (g_do_cfg &&
        g_cst_bb_list &&
        SIMP_need_recon_bblist(&simp) &&
        reconstructBBList(oc)) {

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
    } else {
        ASSERT0((!g_do_md_du_analysis && !g_do_md_ssa) || verifyMDRef());
    }
}


//Test code, to force generating as many IR stmts as possible.
//g_is_lower_to_pr_mode = true;
bool ARMRegion::simplifyToPRmode(OptCtx & oc)
{
    ASSERT0(verifyRPO(oc));
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
    ASSERT0(getDUMgr() &&
            getDUMgr()->verifyMDDUChain(DUOPT_COMPUTE_PR_DU|
                                        DUOPT_COMPUTE_NONPR_DU));
    simplifyBBlist(bbl, &simp);
    if (SIMP_need_recon_bblist(&simp) && g_cst_bb_list &&
        reconstructBBList(oc)) {
        ASSERTN(g_do_cfg, ("construct BB list need CFG"));
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
    g_indent = 0;
    SimpCtx simp;
    if (g_do_cfs_opt) {
        IR_CFS_OPT co(this);
        co.perform(simp);
        ASSERT0(verifyIRList(getIRList(), NULL, this));
    }

    //Simplify control-structure to build CFG.
    simp.setSimpCFS();
    SIMP_if(&simp) = true;
    SIMP_doloop(&simp) = true;
    SIMP_dowhile(&simp) = true;
    SIMP_whiledo(&simp) = true;
    SIMP_switch(&simp) = true;
    SIMP_break(&simp) = true;
    SIMP_continue(&simp) = true;
    setIRList(simplifyStmtList(getIRList(), &simp));
    ASSERT0(verifySimp(getIRList(), simp));
    ASSERT0(verifyIRList(getIRList(), NULL, this));
    //partitionRegion(); //Split region.

    //Lower to middle in order to perform analysis.
    if (g_cst_bb_list) {
        constructBBList();
        ASSERT0(verifyIRandBB(getBBList(), this));
        ASSERT0(verifyBBlist(*getBBList()));
        setIRList(NULL); //All IRs have been moved to each IRBB.
    }

    initPassMgr();
    HighProcessImpl(oc);
    return true;
}


void ARMRegion::HighProcessImpl(OptCtx & oc)
{
    if (g_do_cfg) {
        ASSERT0(g_cst_bb_list);
        checkValidAndRecompute(&oc, PASS_CFG, PASS_UNDEF);

        //Remove empty bb when cfg rebuilted because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        getCFG()->removeEmptyBB(oc);

        //Compute exit bb while cfg rebuilt.
        getCFG()->computeExitList();
        ASSERT0(getCFG()->verify());

        if (g_do_cfg_dom) {
            //Infer pointer arith need loopinfo.
            checkValidAndRecompute(&oc, PASS_DOM, PASS_UNDEF);
        }

        if (g_do_loop_ana) {
            //Infer pointer arith need loopinfo.
            checkValidAndRecompute(&oc, PASS_LOOP_INFO, PASS_UNDEF);
        }

        getCFG()->performMiscOpt(oc);
    }

    //Make MD id more grouped together.
    assignMD(false, true);
    assignMD(true, false);

    if (g_do_aa) {
        ASSERT0(g_cst_bb_list && OC_is_cfg_valid(oc));
        checkValidAndRecompute(&oc, PASS_DOM, PASS_LOOP_INFO,
            PASS_AA, PASS_UNDEF);        
    }

    if (g_do_md_du_analysis) {
        ASSERT0(g_cst_bb_list && OC_is_cfg_valid(oc) && OC_is_aa_valid(oc));
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
        ASSERT0(OC_is_ref_valid(oc));
        if (HAVE_FLAG(f, DUOPT_SOL_REACH_DEF) && succ) {
            dumgr->computeMDDUChain(oc, false, f);
        }

        if (g_do_pr_ssa) {
            ((PRSSAMgr*)getPassMgr()->registerPass(PASS_PR_SSA_MGR))->
                construction(oc);
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

    //START HACK CODE
    //Test code, to force recomputing AA and DUChain.
    //AA and DU Chain need not to be recompute, because
    //simplification maintained them.
    bool org_compute_pr_du_chain = g_compute_pr_du_chain;
    bool org_compute_nonpr_du_chain = g_compute_nonpr_du_chain;
    OC_is_ref_valid(oc) = false;
    OC_is_pr_du_chain_valid(oc) = false;
    OC_is_nonpr_du_chain_valid(oc) = false;
    g_compute_pr_du_chain = true;
    g_compute_nonpr_du_chain = true;
    //END HACK CODE

    if (!OC_is_aa_valid(oc) ||
        !OC_is_ref_valid(oc) ||
        !OC_is_pr_du_chain_valid(oc) ||
        !OC_is_nonpr_du_chain_valid(oc)) {
        AliasAnalysis * aa = getAA();
        if (g_do_aa && aa != NULL &&
            (!OC_is_aa_valid(oc) || !OC_is_ref_valid(oc))) {
            //Recompute and set MD reference to avoid AA's complaint.
            //Compute AA to build coarse-grained DU chain.
            assignMD(true, true);
            if (!aa->is_init()) {
                aa->initAliasAnalysis();
            }
            aa->set_flow_sensitive(false);
            aa->perform(oc);
        }

        //Opt phase may cause AA invalid.
        if (g_do_aa && aa != NULL) {
            //Compute PR's definition to improve AA precison.
            if (g_do_pr_ssa) {
                ASSERT0(getPassMgr());
                PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->registerPass(
                    PASS_PR_SSA_MGR);
                ASSERT0(ssamgr);
                if (!ssamgr->is_valid()) {
                    ssamgr->construction(oc);
                }
            } else if (getDUMgr() != NULL) {
                getDUMgr()->perform(oc, DUOPT_COMPUTE_PR_REF|
                    DUOPT_COMPUTE_NONPR_REF|DUOPT_SOL_REACH_DEF|
                    DUOPT_COMPUTE_PR_DU);
                getDUMgr()->computeMDDUChain(oc, false, DUOPT_COMPUTE_PR_DU);
            }
            //checkValidAndRecompute(&oc, PASS_LOOP_INFO, PASS_UNDEF);
            //Recompute and set MD reference to avoid AA's complaint.
            assignMD(true, true);

            //Compute the threshold to perform AA.
            UINT numir = 0;
            for (IRBB * bb = getBBList()->get_head();
                 bb != NULL; bb = getBBList()->get_next()) {
                numir += bb->getNumOfIR();
            }
            aa->set_flow_sensitive(numir < xoc::g_thres_opt_ir_num);
            aa->perform(oc);
        }

        DUMgr * dumgr = getDUMgr();
        if (g_do_md_du_analysis && dumgr != NULL) {
            UINT flag = DUOPT_COMPUTE_PR_REF|DUOPT_COMPUTE_NONPR_REF;
            if (g_compute_pr_du_chain) {
                SET_FLAG(flag, DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_PR_DU);
            }
            if (g_compute_nonpr_du_chain) {
                SET_FLAG(flag, DUOPT_SOL_REACH_DEF|DUOPT_COMPUTE_NONPR_DU);
            }
            dumgr->perform(oc, flag);
            if (OC_is_ref_valid(oc) && OC_is_reach_def_valid(oc)) {
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
}


bool ARMRegion::MiddleProcess(OptCtx & oc)
{
    assignMD(false, true);
    assignMD(true, false);
    if (g_is_lower_to_pr_mode) {
        simplifyToPRmode(oc);
    } else {
        Region::MiddleProcess(oc);
        if (g_do_cp && getPassMgr()->queryPass(PASS_DU_MGR) != NULL) {
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
        if (getPassMgr()->queryPass(PASS_DU_MGR) != NULL) {
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
    if (g_do_licm && g_opt_level > OPT_LEVEL0) {
        LICM * pass = (LICM*)getPassMgr()->registerPass(PASS_LICM);
        if (pass->perform(oc)) {
            getPassMgr()->registerPass(PASS_MD_SSA_MGR)->perform(oc);
        }
    }
    if (g_do_cp && g_opt_level > OPT_LEVEL0) {
        CopyProp * pass = (CopyProp*)getPassMgr()->registerPass(PASS_CP);
        pass->perform(oc);
    }
    if (g_do_dce && g_opt_level > OPT_LEVEL0) {
        DeadCodeElim * pass = (DeadCodeElim*)getPassMgr()->registerPass(
            PASS_DCE);
        pass->perform(oc);
    }
    ASSERT0((!g_do_md_du_analysis && !g_do_md_ssa) || verifyMDRef());
    ASSERT0(verifyIRandBB(getBBList(), this));
    if (OC_is_pr_du_chain_valid(oc) || OC_is_nonpr_du_chain_valid(oc)) {
        ASSERT0(getDUMgr() == NULL ||
                getDUMgr()->verifyMDDUChain(DUOPT_COMPUTE_PR_DU|
                                            DUOPT_COMPUTE_NONPR_DU));
    }
    PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->queryPass(PASS_PR_SSA_MGR);
    if (ssamgr != NULL && ssamgr->is_valid()) {
        //NOTE: ssa destruction might violate classic DU chain.
        ssamgr->destruction(&oc);
    }

    ///////////////////////////////////////////////////
    //DO NOT DO OPTIMIZATION ANY MORE AFTER THIS LINE//
    ///////////////////////////////////////////////////
    //Finial refine to insert CVT if necessary when compile C/C++.
    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    ASSERTN(g_do_refine, ("inserting CVT is expected"));
    g_do_refine_auto_insert_cvt = true;
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
