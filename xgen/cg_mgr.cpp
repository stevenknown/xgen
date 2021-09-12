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
#include "xgeninc.h"

namespace xgen {

CGMgr::CGMgr(RegionMgr * rm)
{
    m_sr_mgr = nullptr;
    m_or_mgr = nullptr;
    m_sect_mgr = nullptr;
    m_code_sect = nullptr;
    m_data_sect = nullptr;
    m_rodata_sect = nullptr;
    m_bss_sect = nullptr;
    m_param_sect = nullptr;
    m_stack_sect = nullptr;
    m_rm = rm;
    m_asm_file_handler = nullptr;
    initBuiltin();
    m_intrin_mgr = new IntrinsicMgr(this);
}


size_t CGMgr::count_mem() const
{
    size_t count = sizeof(*this);
    count -= sizeof(m_sr_vec_mgr);
    count -= sizeof(m_asmprtmgr);
    count -= sizeof(m_builtin_var);
    count += m_sr_vec_mgr.count_mem();
    count += m_asmprtmgr.count_mem();
    count += m_builtin_var.count_mem();
    if (m_sr_mgr != nullptr) {
        count += m_sr_mgr->count_mem();
    }
    if (m_or_mgr != nullptr) {
        count += m_or_mgr->count_mem();
    }
    if (m_sect_mgr != nullptr) {
        count += m_sect_mgr->count_mem();
    }
    return count;
}


void CGMgr::initBuiltin()
{
    m_builtin_var.set(BUILTIN_MEMCPY, addBuiltinVar("memcpy"));
}


//Initialize program-section information, which might include
//but not limited CODE, DATA, RODATA, BSS.
//Generate new symbol for each of sections in order to
//assign concise name of them.
void CGMgr::initSectionMgrAndSections()
{
    ASSERT0(m_sect_mgr == nullptr);
    m_sect_mgr = allocSectionMgr();

    m_code_sect = m_sect_mgr->genSection(".code", false, 0);
    m_data_sect = m_sect_mgr->genSection(".data", false, 0);
    m_rodata_sect = m_sect_mgr->genSection(".rodata", false, 0);
    m_bss_sect = m_sect_mgr->genSection(".bss", false, 0);
    m_param_sect = m_sect_mgr->genSection(".param", false, 0);
    m_stack_sect = m_sect_mgr->genStackSection();
}


//Return true if given ir indicates an intrinsic operation.
bool CGMgr::isIntrinsic(IR const* ir) const
{
    if (!ir->is_call()) { return false; }

    xoc::Var const* var = CALL_idinfo(ir);
    ASSERT0(var);
    return getIntrinMgr()->isIntrinsic(var->get_name());
}


//Return true if given ir indicates the intrinsic operation
//that name is 'code'.
bool CGMgr::isIntrinsic(IR const* ir, UINT code) const
{
    if (!ir->is_call() || code == INTRIN_UNDEF) { return false; }

    xoc::Var const* var = CALL_idinfo(ir);
    ASSERT0(var);
    return getIntrinMgr()->getCode(var->get_name()) == code;
}


//Free md's id and var's id back to MDSystem and VarMgr.
//The index of MD and Var is important resource if there
//are a lot of CG.
void CGMgr::destroyVAR()
{
    VarMgr * varmgr = m_rm->getVarMgr();
    MDSystem * mdsys = m_rm->getMDSystem();
    ConstMDIter mdit;
    Bltin2VarIter it;
    Var * v;
    for (m_builtin_var.get_first(it, &v); v != nullptr;
         m_builtin_var.get_next(it, &v)) {
        mdsys->removeMDforVAR(v, mdit);
        varmgr->destroyVar(v);
    }
    m_builtin_var.clean();
}


//Allocate VarMgr.
AsmPrinter * CGMgr::allocAsmPrinter(CG const* cg)
{
    return new AsmPrinter(cg, getAsmPrtMgr());
}


//Print result of CG to asm file.
void CGMgr::prtCGResult(CG const* cg)
{
    if (getAsmFileHandler() == nullptr) { return; }
    AsmPrinter * ap = allocAsmPrinter(cg);
    ap->printData(getAsmFileHandler());
    ap->printCode(getAsmFileHandler());
    delete ap;
}


bool CGMgr::genAndPrtGlobalVariable(Region * rg)
{
    if (getAsmFileHandler() == nullptr || !has_section()) { return false; }
    START_TIMER(t, "Generate and Print Global Variable");
    ASSERT0(rg->is_program());
    CG * cg = allocCG(rg);
    VarVec * varvec = rg->getVarMgr()->getVarVec();
    for (INT i = 0; i <= varvec->get_last_idx(); i++) {
        Var * v = varvec->get(i);
        if (v == nullptr ||
            !v->is_global() ||
            v->is_fake() ||
            v->is_unallocable() ||
            v->is_func_decl()) {
            continue;
        }
        cg->computeAndUpdateGlobalVarLayout(v, nullptr, nullptr);
    }
    prtCGResult(cg);
    delete cg;
    END_TIMER(t, "Generate and Print Global Variable");
    return true;
}


static void generateORAndOutput(Region * rg, CGMgr * cgmgr)
{
    CG * cg = cgmgr->allocCG(rg);
    cg->initFuncUnit();
    cg->initDedicatedSR();
    //cg->initGlobalVar(m_rg->getVarMgr());
    cg->perform();
    if (rg->is_function()) {
        cgmgr->prtCGResult(cg);
    }
    delete cg;
}


//Generate code and perform target machine dependent operations.
//Basis step to do:
//  1. Generate target dependent micro operation representation(named as OR).
//  2. Print micro operation into ASM file.
//  3. Machine resource allocation.
//
//Optimizations to be performed:
//  1. Instruction scheduling.
//  2. Loop optimization.
//  3. Software pipelining.
//  4. Control flow optimization(predicational).
//  5. GRA.
//  6. LRA.
//  7. Peephole.
bool CGMgr::generate(Region * rg)
{
    ASSERT0(rg);
    START_TIMER(t, "Code Generation");
    initSRAndORMgr();
    initSectionMgrAndSections();
    generateORAndOutput(rg, this);
    destroySRAndORMgr();
    destroySectionMgr();
    END_TIMER(t, "Code Generation");
    return true;
}

} //namespace xgen
