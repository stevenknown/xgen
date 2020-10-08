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
#include "xgeninc.h"

namespace xgen {

//Allocate VarMgr.
AsmPrinter * CGMgr::allocAsmPrinter(CG * cg, AsmPrinterMgr * asmprtmgr)
{
    return new AsmPrinter(cg, asmprtmgr);
}


bool CGMgr::GenAndPrtGlobalVariable(Region * rg, FILE * asmh)
{
    if (asmh == NULL) { return false; }
    START_TIMER(t, "Generate and Print Global Variable");
    ASSERT0(rg->is_program());
    CG * cg = allocCG(rg);
    VarVec * varvec = rg->getVarMgr()->get_var_vec();
    for (INT i = 0; i <= varvec->get_last_idx(); i++) {
        Var * v = varvec->get(i);
        if (v == NULL ||
            !v->is_global() ||
            v->is_fake() ||
            v->is_unallocable() ||
            v->is_func_decl()) {
            continue;
        }
        cg->computeAndUpdateGlobalVarLayout(v, NULL, NULL);
    }

    AsmPrinter * ap = allocAsmPrinter(cg, &m_asmprtmgr);
    ap->printData(asmh);
    ap->printCode(asmh);
    fflush(asmh);

    delete cg;
    delete ap;
    END_TIMER(t, "Generate and Print Global Variable");
    return true;
}


//Generate code and perform target machine dependent operations.
//Basis step to do:
//    1. Generate target dependent micro operation representation(named as OR).
//    2. Print micro operation into ASM file.
//    3. Machine resource allocation.
//
//Optimizations to be performed:
//    1. Instruction scheduling.
//    2. Loop optimization.
//    3. Software pipelining.
//    4. Control flow optimization(predicational).
//    5. GRA.
//    6. LRA.
//    7. Peephole.
bool CGMgr::CodeGen(Region * region, FILE * asmh)
{
    ASSERT0(region && asmh);
    START_TIMER(t, "Code Generation");
    CG * cg = allocCG(region);
    cg->initFuncUnit();
    cg->initBuiltin();
    cg->initDedicatedSR();
    //cg->initGlobalVar(m_rg->getVarMgr());
    cg->perform();
    if (region->is_function()) {
        AsmPrinter * ap = allocAsmPrinter(cg, &m_asmprtmgr);
        ap->printData(asmh);
        ap->printCode(asmh);
        fflush(asmh);
        delete ap;
    }
    delete cg;
    END_TIMER(t, "Code Generation");
    return true;
}

} //namespace xgen
