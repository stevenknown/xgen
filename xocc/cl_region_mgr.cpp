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
#include "xoccinc.h"
#include "../xgen/xgeninc.h"
#include "../reader/reader.h"
#include "../arm/arm_ir_parser.h"
#include "../arm/arm_grreader.h"

namespace xocc {

CLRegionMgr::CLRegionMgr()
{
    m_grreader = nullptr;
}


CLRegionMgr::~CLRegionMgr()
{
    if (m_grreader != nullptr) {
        delete m_grreader;
    }
}


VarMgr * CLRegionMgr::allocVarMgr()
{
    return new CLVarMgr(this);
}


Region * CLRegionMgr::allocRegion(REGION_TYPE rt)
{
    return new CLRegion(rt, this);
}


GRReader * CLRegionMgr::allocGRReader()
{
    return new ARMGRReader(this);
}


void CLRegionMgr::initGRReader()
{
    ASSERT0(m_grreader == nullptr);
    m_grreader = allocGRReader();
}


TargInfoMgr * CLRegionMgr::allocTargInfoMgr()
{
    #ifdef REF_TARGMACH_INFO
    return new ARMTargInfoMgr(this);
    #else
    return nullptr;
    #endif
}


bool CLRegionMgr::compileFuncRegion(xoc::Region * func, xoc::OptCtx * oc)
{
    ASSERT0(func && oc);
    ASSERT0(func->is_function() || func->is_program() || func->is_inner());
    START_TIMER_FMT(t, ("compileFuncRegion '%s'", func->getRegionName()));
    //Note we regard INNER region as FUNCTION region.
    if (!xoc::RegionMgr::processFuncRegion(func, oc)) {
        return false;
    }
    if (xgen::g_do_cg) {
        if (xgen::g_is_generate_code_for_inner_region) {
            if (!func->processInnerRegion(oc)) {
                return false;
            }
        }
        ASSERT0(m_cgmgr);
        m_cgmgr->generate(func);
        if (xoc::g_dump_opt.isDumpMemUsage()) {
            func->dumpMemUsage();
        }
    }
    END_TIMER_FMT(t, ("compileFuncRegion '%s'", func->getRegionName()));
    return true;
}


void CLRegionMgr::compileProgramRegion(CHAR const* fn, Region * rg)
{
    ASSERT0(m_cgmgr);
    m_cgmgr->genAndPrtGlobalVariable(rg);

    //Output GR.
    if (!xocc::g_is_dumpgr) { return; }
    ASSERT0(fn);
    xcom::StrBuf b(64);
    b.strcat(fn);
    b.strcat(".hir.gr");
    xcom::FileObj fo(b.buf, true, false);
    FILE * gr = fo.getFileHandler();
    ASSERT0(gr);
    rg->getLogMgr()->push(gr, "");
    xoc::GRDump gd(rg);
    gd.dumpRegion(true);
    rg->getLogMgr()->pop();
}


void CLRegionMgr::compileFuncRegion(Region * rg)
{
    xoc::OptCtx * oc = getAndGenOptCtx(rg);
    bool s = compileFuncRegion(rg, oc);
    ASSERT0(s);
    DUMMYUSE(s);
    //rm->deleteRegion(rg); //Local region can be deleted if processed.
}


//Processing function unit one by one.
//1. Construct Region.
//2. Generate IR of Region.
//3. Perform IR optimizaions
//4. Generate assembly code.
void CLRegionMgr::compileRegionSet(CHAR const* fn)
{
    //Test mem leak.
    //test_ru(this, m_cgmgr);
    registerGlobalMD();
    Region * program = nullptr; //There could be multiple program regions.
    for (UINT i = 0; i < getNumOfRegion(); i++) {
        Region * rg = getRegion(i);
        if (rg == nullptr) { continue; }
        if (rg->is_program()) {
            program = rg;
            ASSERT0(fn);
            compileProgramRegion(fn, rg);
            continue;
        }
        if (rg->is_blackbox()) {
            continue;
        }
        if (xoc::g_show_time) {
            xoc::prt2C("\n\n==== Start Process Region(id:%d)'%s' ====\n",
                       rg->id(), rg->getRegionName());
        }
        compileFuncRegion(rg);
    }
    ASSERT0(program);
    xoc::OptCtx * oc = getAndGenOptCtx(program);
    bool s = processProgramRegion(program, oc);
    ASSERT0(s);
    DUMMYUSE(s);
}


void CLRegionMgr::dumpProgramRegionGR(CHAR const* srcname)
{
    for (UINT i = 0; i < getNumOfRegion(); i++) {
        Region * rg = getRegion(i);
        if (rg == nullptr) { continue; }
        if (!rg->is_program()) { continue; }
        //r->dump(true);
        ASSERT0(srcname);
        xcom::StrBuf b(64);
        b.strcat(srcname);
        b.strcat(".gr");
        xcom::FileObj fo(b.buf, true, false);
        FILE * gr = fo.getFileHandler();
        ASSERT0(gr);
        rg->getLogMgr()->push(gr, b.buf);
        xoc::GRDump gd(rg);
        gd.dumpRegion(true);
        rg->getLogMgr()->pop();
    }
}

} //namespace xocc
