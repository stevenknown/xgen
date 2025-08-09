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

//Saving region-used callee registers.
//'bblist': records BBs that have to reallocate register.
void ARMRaMgr::spillRegFileAtEntry(REGFILE regfile, IN ORBB * entry,
                                   xgen::RegSet const used_callee_regs[],
                                   OUT ORBBList & bblist,
                                   OUT Reg2Var & reg2var)
{
    RaMgr::spillRegFileAtEntry(regfile, entry, used_callee_regs,
                               bblist, reg2var);
}


//Saving region-used callee registers.
//'bblist': records BBs that have to reallocate register.
void ARMRaMgr::reloadRegFileAtExit(REGFILE regfile, IN ORBB * exit,
                                   xgen::RegSet const used_callee_regs[],
                                   OUT ORBBList & bblist,
                                   Reg2Var const& reg2var)
{
    RaMgr::reloadRegFileAtExit(regfile, exit, used_callee_regs,
                               bblist, reg2var);
}


xgen::LifeTimeMgr * ARMRaMgr::allocLifeTimeMgr()
{
    return (xgen::LifeTimeMgr*)new ARMLifeTimeMgr(m_cg);
}


void ARMRaMgr::setUseLR()
{
    xgen::RegSet * regset = getLRAUsedCalleeSavedRegSet(RF_R);
    regset->bunion(REG_RETURN_ADDRESS_REGISTER);
}


void ARMRaMgr::postProcess()
{
    //The code saving-LR has been generated at generateFuncUnitDedicatedCode()
    //List<ORBB*> * bblst = m_cg->getORBBList();
    //bool has_call = false;
    //for (ORBB * bb = bblst->get_head();
    //     bblst != nullptr; bb = bblst->get_next()) {
    //    if (has_call) {
    //        break;
    //    }
    //    for (OR * o = ORBB_orlist(bb)->get_head();
    //         o != nullptr; o = ORBB_orlist(bb)->get_next()) {
    //        if (OR_is_call(o)) {
    //            has_call = true;
    //            break;
    //        }
    //    }
    //}
    //if (has_call) {
    //    setUseLR();
    //}
    RaMgr::postProcess();
}
