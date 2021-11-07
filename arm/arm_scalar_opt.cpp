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

static bool worthToDo(Pass const* pass, UINT & cp_count, UINT & licm_count) {
    if (pass->getPassType() == PASS_LICM && licm_count > 1 && cp_count > 1) {
        //LICM has performed at least once.
        //Sometime, LICM doing the counter-effect to CP.
        //We make the simplest choose that if both LICM and CP have performed
        //more than once, says twice, it is not worthy to do any more.
        return false;
    }

    if (pass->getPassType() == PASS_CP && licm_count > 1 && cp_count > 1) {
        //CP has performed at least once.
        //Sometime, LICM doing the counter-effect to CP.
        //We make the simplest choose that if both LICM and CP have performed
        //more than once, says twice, it is not worthy to do any more.
        return false;
    }

    return true;
}


static void updateCounter(Pass const* pass, UINT & cp_count,
                          UINT & licm_count) {
    licm_count += pass->getPassType() == PASS_LICM ? 1 : 0;
    cp_count += pass->getPassType() == PASS_CP ? 1 : 0;
}


bool ARMScalarOpt::perform(OptCtx & oc)
{
    ASSERT0(oc.is_cfg_valid());
    ASSERT0(m_rg && m_rg->getCFG()->verify());
    List<Pass*> passlist; //A list of Optimization.
    if (g_do_ivr) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_IVR));
    }
    if (g_do_licm) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_LICM));
    }

    CopyProp * cp = nullptr;
    if (g_do_cp || g_do_cp_aggressive) {
        cp = (CopyProp*)m_pass_mgr->registerPass(PASS_CP);
        cp->setPropagationKind(g_do_cp_aggressive ?
                               CP_PROP_UNARY_AND_SIMPLEX : CP_PROP_SIMPLEX);
        passlist.append_tail(cp);
    }

    if (g_do_rp) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_RP));
    }
    if (g_do_rce) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_RCE));
    }
    if (g_do_gcse) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_GCSE));
    }
    if (g_do_cp || g_do_cp_aggressive) {
        ASSERT0(cp);
        passlist.append_tail(cp);
    }
    if (g_do_dce || g_do_dce_aggressive) {
        DeadCodeElim * dce = (DeadCodeElim*)m_pass_mgr->registerPass(PASS_DCE);
        dce->set_elim_cfs(g_do_dce_aggressive);
        passlist.append_tail(dce);
    }
    if (g_do_dse) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_DSE));
    }
    if (g_do_loop_convert) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_LOOP_CVT));
    }
    if (g_do_lftr) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_LFTR));
    }

    bool res = false;
    bool change;
    UINT count = 0;
    ASSERT0(PRSSAMgr::verifyPRSSAInfo(m_rg));
    UINT cp_count = 0;
    UINT licm_count = 0;
    do {
        change = false;
        for (Pass * pass = passlist.get_head();
             pass != nullptr; pass = passlist.get_next()) {
            ASSERT0(verifyIRandBB(m_rg->getBBList(), m_rg));
            CHAR const* passname = pass->getPassName();
            DUMMYUSE(passname);
            bool doit = false;
            if (worthToDo(pass, cp_count, licm_count)) {
                doit = pass->perform(oc);
            }
            if (doit) {
                //RefineCtx rf;
                //RC_insert_cvt(rf) = false;
                //m_rg->getRefine()->refineBBlist(m_rg->getBBList(), rf);
                change = true;
                updateCounter(pass, cp_count, licm_count);
            }

            res |= doit;
            ASSERT0(m_rg->verifyMDRef());
            if (oc.is_pr_du_chain_valid() ||
                oc.is_nonpr_du_chain_valid()) {
                //DU reference and du chain has maintained.
                UINT flag = 0;
                if (oc.is_pr_du_chain_valid()) {
                    SET_FLAG(flag, DUOPT_COMPUTE_PR_DU);
                }
                if (oc.is_nonpr_du_chain_valid()) {
                    SET_FLAG(flag, DUOPT_COMPUTE_NONPR_DU);
                }
                ASSERT0(verifyMDDUChain(m_rg, flag));
            }

            ASSERT0(verifyIRandBB(m_rg->getBBList(), m_rg));
            ASSERT0(m_rg->getCFG()->verify());
            ASSERT0(PRSSAMgr::verifyPRSSAInfo(m_rg));
            ASSERT0(MDSSAMgr::verifyMDSSAInfo(m_rg));
            ASSERT0(m_cfg->verifyRPO(oc));
        }
        count++;
    } while (change && count < 20);
    ASSERT0(!change);
    return res;
}
