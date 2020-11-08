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

//Return true if OR can be issued at given slot.
bool ARMLIS::isValidResourceUsage(OR const* o,
                                  SLOT slot,
                                  OR * issue_ors[SLOT_NUM],
                                  OR * conflict_ors[SLOT_NUM]) const
{
    ASSERT0(m_cg->computeORSlot(o) == slot);
    ASSERT0(issue_ors[slot] == nullptr);
    return true;
}


//Change OR to issue slot 'to_slot'.
bool ARMLIS::changeSlot(OR * o, SLOT to_slot)
{
    CLUST or_clst = m_cg->computeORCluster(o);
    CLUST to_slot_clst = m_cg->mapSlot2Cluster(to_slot);
    ASSERT0(to_slot_clst != CLUST_UNDEF);
    if (or_clst != CLUST_UNDEF && or_clst != to_slot_clst) {
        if (!allowChangeCluster()) {
            return false;
        }
    }

    UNIT slot_unit = m_cg->mapSlot2Unit(to_slot);
    bool succ = m_cg->changeORUnit(o, slot_unit, to_slot_clst,
                                   m_is_regfile_unique, false);
    return succ;
}
