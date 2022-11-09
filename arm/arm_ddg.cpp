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

ARMDDG::ARMDDG(ARMCG * cg)
{
    //Initialize predicated regiseter alias set.
    m_alias_regset_pred.bunion(cg->genRflag()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genEQPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genNEPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genCSPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genCCPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genHSPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genLOPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genMIPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genPLPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genGEPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genLTPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genGTPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genLEPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genHIPred()->getPhyReg());
    m_alias_regset_pred.bunion(cg->genLSPred()->getPhyReg());
}


//Return true if 'sr' need to be processed.
bool ARMDDG::handleDedicatedSR(SR const* sr, OR const*, bool is_result) const
{
    //if (is_result) {
    //    ASSERT0(m_cg);
    //    if (sr == m_cg->getRflag()) {
    //        return false;
    //    }
    //}
    return true;
}


void ARMDDG::collectAliasRegSet(Reg reg, OUT xgen::RegSet & alias_regset)
{
    if (m_alias_regset_pred.is_contain(reg)) {
        alias_regset.bunion(m_alias_regset_pred);
        return;
    }
    DataDepGraph::collectAliasRegSet(reg, alias_regset);
}
