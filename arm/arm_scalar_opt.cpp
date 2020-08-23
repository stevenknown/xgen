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

bool ARMScalarOpt::perform(OptCtx & oc)
{
    ASSERT0(OC_is_cfg_valid(oc));
    ASSERT0(m_rg && m_rg->getCFG()->verify());
    List<Pass*> passlist; //A list of Optimization.

    if (g_do_ivr) { passlist.append_tail(m_pass_mgr->registerPass(PASS_IVR)); }
    if (g_do_licm) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_LICM));
    }
    if (g_do_cp) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_CP));
        ((CopyProp*)m_pass_mgr->registerPass(PASS_CP))->setPropagationKind(
            CP_PROP_SIMPLEX);
    }

    if (g_do_rp) { passlist.append_tail(m_pass_mgr->registerPass(PASS_RP)); }
    if (g_do_rce) { passlist.append_tail(m_pass_mgr->registerPass(PASS_RCE)); }
    if (g_do_gcse) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_GCSE));
    }

    if (g_do_cp) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_CP));
        ((CopyProp*)m_pass_mgr->queryPass(PASS_CP))->setPropagationKind(
            CP_PROP_SIMPLEX);
    }

    if (g_do_dce) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_DCE));        
    }

    if (g_do_dse) { passlist.append_tail(m_pass_mgr->registerPass(PASS_DSE)); }
    if (g_do_loop_convert) {
        passlist.append_tail(m_pass_mgr->registerPass(PASS_LOOP_CVT));
    }

    bool res = false;
    bool change;
    UINT count = 0;
    ASSERT0(PRSSAMgr::verifyPRSSAInfo(m_rg));
    do {
        change = false;
        for (Pass * pass = passlist.get_head();
             pass != NULL; pass = passlist.get_next()) {
            ASSERT0(verifyIRandBB(m_rg->getBBList(), m_rg));
            bool doit = pass->perform(oc);
            if (doit) {
                CHAR const* passname = pass->getPassName();
                //RefineCtx rf;
                //RC_insert_cvt(rf) = false;
                //m_rg->getRefine()->refineBBlist(m_rg->getBBList(), rf);
                change = true;
            }
            res |= doit;

            ASSERT0(m_rg->verifyMDRef());
            if (OC_is_pr_du_chain_valid(oc) ||
                OC_is_nonpr_du_chain_valid(oc)) {
                //DU reference and du chain has maintained.
                UINT flag = 0;
                if (OC_is_pr_du_chain_valid(oc)) {
                    SET_FLAG(flag, DUOPT_COMPUTE_PR_DU);
                }
                if (OC_is_nonpr_du_chain_valid(oc)) {
                    SET_FLAG(flag, DUOPT_COMPUTE_NONPR_DU);
                }
                ASSERT0(m_rg->getDUMgr());
                ASSERT0(m_rg->getDUMgr()->verifyMDDUChain(flag));
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
