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

//Save predicate register at entry BB
//'bblist': records BBs need to reallocate register.
void ARMRaMgr::saveCalleePredicateAtEntry(
        REGFILE regfile,
        IN ORBB * entry,
        IN RegSet used_callee_regs[],
        OUT List<ORBB*> & bblist,
        OUT xcom::TMap<REG, Var*> &)
{
    return;
}



//Save predicate register at exit BB
//'bblist': records BBs need to reallocate register.
void ARMRaMgr::saveCalleePredicateAtExit(
        REGFILE regfile,
        IN ORBB * exit,
        IN RegSet used_callee_regs[],
        OUT List<ORBB*> & bblist,
        xcom::TMap<REG, Var*> const&)
{
    return;
}


//Saving region-used callee registers.
//'bblist': records BBs need to reallocate register.
void ARMRaMgr::saveCalleeRegFileAtEntry(
        REGFILE regfile,
        IN ORBB * entry,
        IN RegSet used_callee_regs[],
        OUT List<ORBB*> & bblist,
        OUT xcom::TMap<REG, Var*> & reg2var)
{
    RaMgr::saveCalleeRegFileAtEntry(regfile,
        entry, used_callee_regs, bblist, reg2var);
}


//Saving region-used callee registers.
//'bblist': records BBs need to reallocate register.
void ARMRaMgr::saveCalleeRegFileAtExit(
        REGFILE regfile,
        IN ORBB * exit,
        IN RegSet used_callee_regs[],
        OUT List<ORBB*> & bblist,
        xcom::TMap<REG, Var*> const& reg2var)
{
    RaMgr::saveCalleeRegFileAtExit(regfile,
        exit, used_callee_regs, bblist, reg2var);
}


LifeTimeMgr * ARMRaMgr::allocLifeTimeMgr()
{
    return (LifeTimeMgr*)new ARMLifeTimeMgr(m_cg);
}


void ARMRaMgr::setUseLR()
{
    RegSet * regset = getLRAUsedCalleeSavedRegSet(RF_R);
    regset->bunion(REG_RETURN_ADDRESS_REGISTER);
}


void ARMRaMgr::postBuild()
{
    //The code saving-LR has been generated at generateFuncUnitDedicatedCode()
    //List<ORBB*> * bblst = m_cg->getORBBList();
    //bool has_call = false;
    //for (ORBB * bb = bblst->get_head();
    //     bblst != NULL; bb = bblst->get_next()) {
    //    if (has_call) {
    //        break;
    //    }
    //    for (OR * o = ORBB_orlist(bb)->get_head();
    //         o != NULL; o = ORBB_orlist(bb)->get_next()) {
    //        if (OR_is_call(o)) {
    //            has_call = true;
    //            break;
    //        }
    //    }
    //}
    //if (has_call) {
    //    setUseLR();
    //}
    RaMgr::postBuild();
}
