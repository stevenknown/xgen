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

extern void resetMapBetweenVARandDecl(VAR * v);

void ARMRegion::destroy()
{
    #ifdef _DEBUG_
    //This map info only used in dump().
    VarTabIter c;
    VarTab * vartab = getVarTab();
    for (VAR * v = vartab->get_first(c);
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


//Insert CVT for float if necessary.
IR * ARMRegion::insertCvtForFloat(IR * parent, IR * kid, bool & change)
{
    ASSERT0(parent->is_fp() || kid->is_fp());
    UINT tgt_size = parent->getTypeSize(getTypeMgr());
    UINT src_size = kid->getTypeSize(getTypeMgr());

    bool build = false;
    if (parent->is_fp()) {
        if (kid->getType()->is_int()) {
            build = true;
        } else if (kid->is_fp()) {
            if (tgt_size != src_size) {
                build = true;
            }
        } else {
            ASSERTN(0, ("incompatible types in convertion"));
        }
    } else {
        if (parent->getType()->is_int()) {
            build = true;
        } else if (parent->is_fp()) {
            if (tgt_size != src_size) {
                build = true;
            }
        } else {
            ASSERTN(0, ("incompatible types in convertion"));
        }
    }

    if (build) {
        IR * new_kid = buildCvt(kid, parent->getType());
        copyDbx(new_kid, kid, this);
        change = true;
        return new_kid;
    }

    return kid;
}


void ARMRegion::simplify(OptCtx & oc)
{
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


void ARMRegion::low_to_pr_mode(OptCtx & oc)
{
    SimpCtx simp;
    if (g_is_lower_to_pr_mode) {
        simp.setSimpToPRmode();
    }

    if (g_do_pr_ssa) {
        //Note if this flag enable,
        //AA may generate imprecise result.
        //TODO: use SSA info to improve the precision of AA.
        simp.setSimpToPRmode();
        simp.setSimpSelect();
        simp.setSimpLandLor();
        simp.setSimpLnot();
    }

    //dumpBBList(getBBList(),this);
    //Simplify IR tree if it is needed.
    simplifyBBlist(getBBList(), &simp);
    if (SIMP_need_recon_bblist(&simp)) {
        //New BB boundary IR generated, rebuilding CFG.
        if (reconstructBBList(oc)) {
            getCFG()->rebuild(oc);
            getCFG()->removeEmptyBB(oc);
            getCFG()->computeExitList();
        }
    }
    //dumpBBList(getBBList(),this);
}


bool ARMRegion::MiddleProcess(OptCtx & oc)
{
    if (g_opt_level == OPT_LEVEL0) {
        assignPRMD();
    }

    //START HACK CODE
    //Test code, to force generating as many IR stmts as possible.
    //g_is_lower_to_pr_mode = true;
    //END HACK CODE
    bool own = false;
    if (own) {
        ARMMiddleProcess(oc);
    } else {
        Region::MiddleProcess(oc);
        if (g_do_cp &&
            getPassMgr()->queryPass(PASS_DU_MGR) != NULL) {
            IR_CP * cp = (IR_CP*)getPassMgr()->registerPass(PASS_CP);
            cp->perform(oc);
        }
    }

    if (g_do_cfg_dom && !OC_is_dom_valid(oc)) {
        getCFG()->computeDomAndIdom(oc);
    }

    if (g_do_cfg_pdom && !OC_is_pdom_valid(oc)) {
        getCFG()->computePdomAndIpdom(oc);
    }

    //START HACK CODE
    //Test code, to force recomputing AA and DUChain.
    //AA and DU Chain need not to be recompute, because
    //simplification maintained them.
    {
        OC_is_ref_valid(oc) = OC_is_du_chain_valid(oc) = false; int a = 0;
        g_compute_classic_du_chain = true;
        //g_do_md_ssa = true;
    }
    //END HACK CODE

    if (g_opt_level > OPT_LEVEL0 &&
        (!OC_is_aa_valid(oc) ||
         !OC_is_ref_valid(oc) ||
         !OC_is_du_chain_valid(oc))) {
        IR_AA * aa = getAA();
        if (g_do_aa && aa != NULL &&
            (!OC_is_aa_valid(oc) || !OC_is_ref_valid(oc))) {
            if (!aa->is_init()) {
                aa->initAliasAnalysis();
            }
            aa->set_flow_sensitive(false);
            aa->perform(oc);
        }

        //Opt phase may lead it to be invalid.
        if (g_do_aa && aa != NULL) {
            //Compute the threshold to perform AA.
            UINT numir = 0;
            for (IRBB * bb = getBBList()->get_head();
                 bb != NULL; bb = getBBList()->get_next()) {
                numir += bb->getNumOfIR();
            }

            if (numir > g_thres_opt_ir_num) {
                aa->set_flow_sensitive(false);
            }

            //Compute PR's definition to improve AA precison.
            if (g_do_pr_ssa) {
                ASSERT0(getPassMgr());
                PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->
                    registerPass(PASS_PR_SSA_MGR);
                ASSERT0(ssamgr);
                if (!ssamgr->isSSAConstructed()) {
                    ssamgr->construction(oc);
                }
            } else if (getDUMgr() != NULL) {
                getDUMgr()->perform(oc, SOL_REF|SOL_REACH_DEF|COMPUTE_PR_DU);
                getDUMgr()->computeMDDUChain(oc, false, COMPUTE_PR_DU);
            }
            //checkValidAndRecompute(&oc, PASS_LOOP_INFO, PASS_UNDEF);
            aa->perform(oc);
        }

        IR_DU_MGR * dumgr = getDUMgr();
        if (g_do_md_du_analysis && dumgr != NULL) {
            if (g_compute_classic_du_chain) {
                dumgr->perform(oc, SOL_REF|SOL_REACH_DEF|
                                   COMPUTE_PR_DU|COMPUTE_NONPR_DU);
                if (OC_is_ref_valid(oc) && OC_is_reach_def_valid(oc)) {
                    UINT flag = COMPUTE_NONPR_DU;
                    //If PRs have already been in SSA form, compute
                    //DU chain doesn't make any sense.
                    if (getPassMgr() != NULL) {
                        PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->
                            queryPass(PASS_PR_SSA_MGR);
                        if (ssamgr == NULL) {
                            flag |= COMPUTE_PR_DU;
                        }
                    } else {
                        flag |= COMPUTE_PR_DU;
                    }
                    dumgr->computeMDDUChain(oc, false, flag);
                }
            }
            if (g_do_md_ssa) {
                //MDSSA have to be recomputed when DURef and overlap set
                //are available.
                ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MD_SSA_MGR))->
                    construction(oc);
            }
        }
    }

    g_do_gcse = false;
    if (g_do_gcse) {
        MDSSAMgr * mdssamgr = (MDSSAMgr*)
            getPassMgr()->registerPass(PASS_MD_SSA_MGR);
        ASSERT0(mdssamgr);
        if (!mdssamgr->isMDSSAConstructed()) {
            mdssamgr->construction(oc);
        }
        if (getPassMgr()->queryPass(PASS_DU_MGR) != NULL) {
            IR_GCSE * pass = (IR_GCSE*)
                getPassMgr()->registerPass(PASS_GCSE);
            bool changed = false;
            changed = pass->perform(oc);
            if (changed) {
                getDUMgr()->perform(oc,
                    SOL_REACH_DEF |
                    COMPUTE_PR_DU | COMPUTE_NONPR_DU);
                OC_is_du_chain_valid(oc) = false;
                getDUMgr()->computeMDDUChain(oc, false,
                    SOL_REF | SOL_REACH_DEF |
                    COMPUTE_PR_DU | COMPUTE_NONPR_DU);
            }
        }
    }

    g_do_licm = false;
    if (g_do_licm) {
        IR_LICM * pass = (IR_LICM*)getPassMgr()->registerPass(PASS_LICM);
        if (pass->perform(oc)) {
            getPassMgr()->registerPass(PASS_MD_SSA_MGR)->perform(oc);
        }
    }

    g_do_cp = false;
    if (g_do_cp) {
        IR_CP * pass = (IR_CP*)getPassMgr()->registerPass(PASS_CP);
        pass->perform(oc);
    }

    g_do_dce = false;
    if (g_do_dce) {
        IR_DCE * pass = (IR_DCE*)getPassMgr()->registerPass(PASS_DCE);
        pass->perform(oc);
    }

    ASSERT0((!g_do_md_du_analysis && !g_do_md_ssa) || verifyMDRef());
    ASSERT0(verifyIRandBB(getBBList(), this));
    if (OC_is_du_chain_valid(oc)) {
        ASSERT0(getDUMgr() == NULL ||
            getDUMgr()->verifyMDDUChain(COMPUTE_PR_DU | COMPUTE_NONPR_DU));
    }

    PRSSAMgr * ssamgr = (PRSSAMgr*)getPassMgr()->queryPass(PASS_PR_SSA_MGR);
    if (ssamgr != NULL && ssamgr->isSSAConstructed()) {
        //NOTE: ssa destruction might violate classic DU chain.
        ssamgr->destruction(&oc);
    }

    ///////////////////////////////////////////////////
    //DO NOT DO OPTIMIZATION ANY MORE AFTER THIS LINE//
    ///////////////////////////////////////////////////

    //Finial refine to insert CVT if necessary when compile C/C++.
    //Insert int32->int64, int32<->f32, int32<->f64, int64<->f32, int64<->f64
    g_do_refine_auto_insert_cvt = true;
    ASSERTN(g_do_refine, ("inserting CVT is expected"));

    RefineCtx rf;
    RC_insert_cvt(rf) = g_do_refine_auto_insert_cvt;
    refineBBlist(getBBList(), rf);
    ASSERT0((!g_do_md_du_analysis && !g_do_md_ssa) || verifyMDRef());
    ASSERT0(verifyIRandBB(getBBList(), this));

    return true;
}


bool ARMRegion::ARMMiddleProcess(OptCtx & oc)
{
    if (g_opt_level != OPT_LEVEL0) {
        PassMgr * passmgr = getPassMgr();
        ASSERT0(passmgr);

        //Perform scalar optimizations.
        passmgr->performScalarOpt(oc);
    }

    ASSERT0(verifyRPO(oc));
    ASSERT0(getPassMgr());

    BBList * bbl = getBBList();
    if (bbl->get_elem_count() == 0) { return true; }

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
        getDUMgr()->verifyMDDUChain(COMPUTE_PR_DU | COMPUTE_NONPR_DU));

    //dumpBBList(getBBList(), this);
    simplifyBBlist(bbl, &simp);
    if (SIMP_need_recon_bblist(&simp) &&
        g_cst_bb_list &&
        reconstructBBList(oc)) {
        ASSERTN(g_do_cfg, ("construct BB list need CFG"));
        IR_CFG * cfg = getCFG();
        ASSERT0(cfg);
        cfg->rebuild(oc);

        //Remove empty bb when cfg rebuilt because
        //rebuilding cfg may generate redundant empty bb.
        //It disturbs the computation of entry and exit.
        cfg->removeEmptyBB(oc);

        //Compute entry bb, exit bb while cfg rebuilt.
        cfg->computeExitList();
        ASSERT0(cfg->verify());

        bool change = true;
        UINT count = 0;
        while (change && count < 20) {
            change = false;
            if (g_do_cfg_remove_empty_bb &&
                cfg->removeEmptyBB(oc)) {
                cfg->computeExitList();
                change = true;
            }
            if (g_do_cfg_remove_unreach_bb &&
                cfg->removeUnreachBB()) {
                cfg->computeExitList();
                change = true;
            }
            if (g_do_cfg_remove_trampolin_bb &&
                cfg->removeTrampolinEdge()) {
                cfg->computeExitList();
                change = true;
            }
            if (g_do_cfg_remove_unreach_bb &&
                cfg->removeUnreachBB()) {
                cfg->computeExitList();
                change = true;
            }
            if (g_do_cfg_remove_trampolin_bb &&
                cfg->removeTrampolinBB()) {
                cfg->computeExitList();
                change = true;
            }
            count++;
        }
        ASSERT0(!change);
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

    if (g_do_aa) {
        ASSERT0(g_cst_bb_list && OC_is_cfg_valid(oc));
        checkValidAndRecompute(&oc, PASS_DOM, PASS_LOOP_INFO,
            PASS_AA, PASS_UNDEF);
    }

    if (g_do_md_du_analysis) {
        ASSERT0(g_cst_bb_list && OC_is_cfg_valid(oc) && OC_is_aa_valid(oc));
        ASSERT0(getPassMgr());
        IR_DU_MGR * dumgr = (IR_DU_MGR*)getPassMgr()->
            registerPass(PASS_DU_MGR);
        ASSERT0(dumgr);
        UINT f = SOL_REF;
        if (g_compute_region_imported_defuse_md) {
            f |= SOL_RU_REF;
        }
        if (g_compute_classic_du_chain) {
            //Compute du chain in non-ssa form.
            f |= SOL_REACH_DEF|COMPUTE_NONPR_DU|COMPUTE_PR_DU;
            if (dumgr->perform(oc, f) && OC_is_ref_valid(oc)) {
                dumgr->computeMDDUChain(oc, false, f);
            }
        } else if (g_do_md_ssa) {
            f |= COMPUTE_NONPR_DU | COMPUTE_PR_DU;
            //Compute du chain in ssa form.
            if (dumgr->perform(oc, f) && OC_is_ref_valid(oc)) {
                ((MDSSAMgr*)getPassMgr()->registerPass(PASS_MD_SSA_MGR))->
                    construction(oc);                
            }
        }
    
        if (g_do_refine_duchain) {
            RefineDUChain * refdu = (RefineDUChain*)getPassMgr()->
                registerPass(PASS_REFINE_DUCHAIN);
            if (g_compute_classic_du_chain) {
                refdu->setUseGvn(true);
                IR_GVN * gvn = (IR_GVN*)getPassMgr()->registerPass(PASS_GVN);
                gvn->perform(oc);
            }
            refdu->perform(oc);            
        }
    }
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
