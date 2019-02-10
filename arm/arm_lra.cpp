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
#include "../opt/prdf.h"
#include "../xgen/xgeninc.h"
#include "../cfe/cfexport.h"
#include "../opt/util.h"


void ARMLifeTimeMgr::considerSpecialConstrains(
        IN OR * o,
        SR const* sr,
        OUT RegSet & usable_regs)
{
    ASSERTN(o && sr, ("NULL input"));
    ARMCG * armcg = (ARMCG*)m_cg;
    switch (OR_code(o)) {
    case OR_strd:
    case OR_strd_i8: {
        //First Rs must be even number register.
        SR * low = o->get_store_val(0);
        SR * high = o->get_store_val(1);
        ASSERT0(low && high);
        if (sr == low) {
             for (REG tmpr = usable_regs.get_first();
                 ((INT)tmpr) != -1;
                 tmpr = usable_regs.get_next(tmpr)) {
                if (!armcg->isEvenReg(tmpr)) {
                    usable_regs.diff(tmpr);
                }
            }
        } else if (sr == high) {
            for (REG tmpr = usable_regs.get_first();
                 ((INT)tmpr) != -1; tmpr = usable_regs.get_next(tmpr)) {
                if (armcg->isEvenReg(tmpr)) {
                    usable_regs.diff(tmpr);
                }
            }
        }
        break;
    }
    case OR_ldrd:
    case OR_ldrd_i8: {
        //First Rd must be even number register.
        SR * low = o->get_result(PAIR_LOW_RES_IDX);
        SR * high = o->get_result(PAIR_HIGH_RES_IDX);
        if (sr == low) {
             for (REG tmpr = usable_regs.get_first();
                 ((INT)tmpr) != -1;
                 tmpr = usable_regs.get_next(tmpr)) {
                if (!armcg->isEvenReg(tmpr)) {
                    usable_regs.diff(tmpr);
                }
            }
        } else if (sr == high) {
            for (REG tmpr = usable_regs.get_first();
                 ((INT)tmpr) != -1; tmpr = usable_regs.get_next(tmpr)) {
                if (armcg->isEvenReg(tmpr)) {
                    usable_regs.diff(tmpr);
                }
            }
        }
        break;
    }
    default:;
    }

}


//Record the preference register information at neighbour life time.
void ARMLifeTimeMgr::handlePreferredReg(OR const* o)
{
    if (!m_cg->isMultiLoad(OR_code(o), 2) &&
        !m_cg->isMultiStore(OR_code(o), 2)) {
        return;
    }

    SR * low = NULL;
    SR * high = NULL;
    bool is_result = false;
    if (m_cg->isMultiLoad(OR_code(o), 2)) {
        low = const_cast<OR*>(o)->get_load_val(0);
        high = const_cast<OR*>(o)->get_load_val(1);
        is_result = true;
    } else {
        low = const_cast<OR*>(o)->get_store_val(0);
        high = const_cast<OR*>(o)->get_store_val(1);
        is_result = false;
    }

    ASSERTN(low && high && SR_is_reg(low) && SR_is_reg(high),
            ("Does it paired o?"));
    LifeTime * lowlt = (LifeTime*)m_sr2lt_map.get(low);
    LifeTime * highlt = (LifeTime*)m_sr2lt_map.get(high);
    ASSERTN(lowlt && highlt, ("Miss life time."));

    getSibMgr()->setSib(lowlt, highlt);

    if (SR_phy_regid(low) != REG_UNDEF && SR_phy_regid(high) == REG_UNDEF) {
        ASSERTN(!SR_is_global(high),
            ("Global sr should be assigned register during GRA."));
        ASSERT0(m_cg->isValidRegInSRVec(
            const_cast<OR*>(o), low, 0, is_result));
        LT_preferred_reg(highlt) = SR_phy_regid(low) + 1;
    }

    if (SR_phy_regid(high) != REG_UNDEF && SR_phy_regid(low) == REG_UNDEF) {
        ASSERTN(!SR_is_global(low),
            ("Global sr should be assigned register during GRA."));
        ASSERT0(m_cg->isValidRegInSRVec(
            const_cast<OR*>(o), high, 1, is_result));
        LT_preferred_reg(lowlt) = SR_phy_regid(high) - 1;
    }
}
