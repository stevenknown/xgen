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

RaMgr::RaMgr(List<ORBB*> * bbs, bool is_func, CG * cg)
{
    m_cg = nullptr;
    init(bbs, is_func, cg);
}


void RaMgr::init(List<ORBB*> * bbs, bool is_func, CG * cg)
{
    if (bbs == nullptr) { return; }

    m_bb_list = bbs;
    ASSERT0(m_cg == nullptr);
    m_cg = cg;
    m_rg = cg->getRegion();
    ASSERT0(m_cg && m_rg);
    m_need_save_asm_effect = false;
    m_can_alloc_callee = true;
    m_is_func = is_func;
    m_ppm_vec = nullptr;
    m_gra = nullptr;
    m_pool = smpoolCreate(256, MEM_COMM);
}


void RaMgr::destroy()
{
    cleanVar2OR();
    m_var2or_map.destroy();
    m_bb_list = nullptr;
    m_rg = nullptr;
    m_is_func = false;
    if (m_gra != nullptr) {
        delete m_gra;
        m_gra = nullptr;
    }
    smpoolDelete(m_pool);
    m_pool = nullptr;
}


void RaMgr::cleanVar2OR()
{
    //Finializing the SYMBOL list.
    VAR2ORIter iter;
    RefORBBList * ref_bb_list = nullptr;
    for (xoc::Var const* sym = m_var2or_map.get_first(iter, &ref_bb_list);
         sym != nullptr; sym = m_var2or_map.get_next(iter, &ref_bb_list)) {
        ASSERT0(ref_bb_list);
        ref_bb_list->destroy();
    }
    m_var2or_map.clean();
}


void * RaMgr::xmalloc(INT size)
{
    ASSERTN(size > 0, ("xmalloc: size less zero!"));
    ASSERTN(m_pool != nullptr, ("need graph pool!!"));
    void * p = smpoolMalloc(size, m_pool);
    if (p == nullptr) { return nullptr; }
    ::memset(p, 0, size);
    return p;
}


void RaMgr::preProcess()
{
    for (UINT i = RF_UNDEF; i < RF_NUM; i++) {
        m_lra_used_callee_saved_reg[i].clean();
        m_lra_asmclobber_callee_saved_reg[i].clean();
    }

    if (m_cg->isGRAEnable()) {
        //TODO:Caculating live-in, live-out SR and registers,
        //and rebuilding global data flow info of GSR.
    }

    //Initializing SYMBOL referencing list.
    for (ORBB * bb = m_bb_list->get_head();
         bb != nullptr; bb = m_bb_list->get_next()) {
        for (OR * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb)) {
            xoc::Var const* spill_var = m_cg->computeSpillVar(o);
            if (spill_var != nullptr) { //o is a spilling-or.
                addVARRefList(bb, o, spill_var);
            }
        }
    }
}


void RaMgr::postProcessFunc()
{
    //Protect the registers to conform to calling convention.
    if (!m_cg->isGRAEnable()) {
        //Deduct callee-saved registers used by GRA
        //from lra register set, in order to avoid saving
        //same register at twice.
        //
        //BUG: GRA saved callee registers, but the result does not
        //be recorded in 'm_lra_used_callee_saved_reg', so it always be nullptr.
        //And phy-registers assigned just during Lra.
        //
        //RegSet * gra_used_callee_save_regs =
        //    RAMGR_gra(m_ramgr)->get_used_callee_save_regs();
        //for (REGFILE regfile = RF_UNDEF + 1; regfile < RF_NUM; regfile++) {
        //    m_lra_used_callee_saved_reg[regfile].diff(
        //                gra_used_callee_save_regs[regfile]);
        //}

        //Generate spill and reload.
        saveCallee(m_lra_used_callee_saved_reg);
    }

    if (m_need_save_asm_effect) {
        //Only process registers clobbed by asm.
        saveCallee(m_lra_asmclobber_callee_saved_reg);
    }
}


void RaMgr::postProcess()
{
    if (m_is_func) {
        postProcessFunc();
    }
    cleanVar2OR();
}


void RaMgr::updateAsmClobberCallee(REGFILE regfile, REG reg)
{
    ASSERTN(regfile != RF_UNDEF && reg != REG_UNDEF,
            ("Illegal regfile and reg"));
    if (tmGetRegSetOfCalleeSaved()->is_contain(reg)) {
        m_need_save_asm_effect = true;
        m_lra_asmclobber_callee_saved_reg[regfile].bunion(reg);
    }
}


void RaMgr::updateCallee(REGFILE regfile, REG reg)
{
    ASSERTN(regfile != RF_UNDEF && reg != REG_UNDEF,
            ("Illegal regfile and reg"));
    if (tmGetRegSetOfCalleeSaved()->is_contain(reg)) {
        ASSERTN(canAllocCallee(), ("Callee register is forbidden."));
        m_lra_used_callee_saved_reg[regfile].bunion(reg);
    }
}


//Dump spill location var and relevant ors.
void RaMgr::dumpGlobalVAR2OR()
{
    FILE * h = m_rg->getLogMgr()->getFileHandler();
    fprintf(h,
            "\n==---- DUMP Mapping from Global Var to OR, Region:%s ----==",
            m_rg->getRegionName());
    INT i = 0;
    xcom::StrBuf buf(64);
    VAR2ORIter iter;
    RefORBBList * ref_bb_list = nullptr;
    for (xoc::Var const* var = m_var2or_map.get_first(iter, &ref_bb_list);
         var != nullptr; var = m_var2or_map.get_next(iter, &ref_bb_list)) {
        fprintf(h, "\n\tVAR%d", i++);
        fprintf(h, "\n\t\t%s", var->dump(buf, m_rg->getTypeMgr()));

        ASSERTN(ref_bb_list, ("Miss info"));
        for (ORBBUnit * bu = ref_bb_list->get_head(); bu;
             bu = ref_bb_list->get_next()) {
            ORBB * bb = OR_BBUNIT_bb(bu);
            fprintf(h, "\n\t\tBB%d:", bb->id());
            for (OR * o = OR_BBUNIT_or_list(bu)->get_head(); o != nullptr;
                 o = OR_BBUNIT_or_list(bu)->get_next()) {
                ASSERTN(o, ("Miss info"));
                fprintf(h, "\n\t\t\t");
                fprintf(h, "%s", o->dump(buf, m_cg));
            }
        }
    }

    fprintf(h, "\n");
    fflush(h);
}


void RaMgr::dumpBBList()
{
    dumpORBBList(*m_bb_list, getCG());
}


//Dump callee registers used by GRA.
void RaMgr::dumpCalleeRegUsedByGra()
{
    FILE * h = m_rg->getLogMgr()->getFileHandler();
    if (h == nullptr) { return; }

    fprintf(h, "\n==---- DUMP GRA Used Callee Regs, Region:%s: ",
            m_rg->getRegionName());
    if (RAMGR_gra(this) == nullptr) {
        fprintf(h, "No GRA\n");
        return;
    }

    for (INT regfile = RF_UNDEF + 1; regfile < RF_NUM; regfile++) {
        fprintf(h, "\nRegFile:%-10s: ", tmGetRegFileName((REGFILE)regfile));
        RAMGR_gra(this)->get_used_callee_save_regs()->dump(h);
    }

    fprintf(h, "\n");
    fflush(h);
}


void RaMgr::setParallelPartMgrVec(Vector<ParallelPartMgr*> * ppm_vec)
{
    m_ppm_vec = ppm_vec;
}


#ifdef _DEBUG_
static bool verifyORS(ORList const& ors, ORBB * bb)
{
    ORCt * ct;
    for (ors.get_head(&ct); ct != ors.end(); ct = ors.get_next(ct)) {
        CHECK0_DUMMYUSE(ct->val()->getBB() == bb);
    }
    return true;
}
#endif


//Save predicate register at entry BB
//bblist: records BBs that need to reallocate register.
void RaMgr::saveCalleePredicateAtEntry(REGFILE regfile, ORBB * entry,
                                       IN RegSet used_callee_regs[],
                                       OUT ORBBList & bblist,
                                       OUT xcom::TMap<REG, xoc::Var*> &)
{
    DUMMYUSE(bblist);
    DUMMYUSE(regfile);
    DUMMYUSE(used_callee_regs);
    //Generate code to save callee predicate registers.
    //Record in ors.
    if (HAS_PREDICATE_REGISTER) {
        ASSERTN(0, ("Target Dependent Code"));
    }
    OR * sp_adj = ORBB_entry_spadjust(entry);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());
    if (m_rg->is_function()) { ASSERT0(sp_adj); }

    ORList ors;
    if (sp_adj != nullptr) {
        ORBB_orlist(entry)->insert_after(ors, sp_adj);
    } else {
        ORBB_orlist(entry)->append_head(ors);
    }
    ASSERT0(verifyORS(ors, entry));
}


//Save float register at entry BB
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeFPRegisterAtEntry(
    REGFILE regfile, ORBB * entry,
    IN RegSet used_callee_regs[],
    OUT ORBBList & bblist,
    OUT xcom::TMap<REG, xoc::Var*> & reg2var)
{
    ASSERT0(tmIsFloatRegFile(regfile));
    RegSet * used_regs = &used_callee_regs[regfile];

    OR * sp_adj = ORBB_entry_spadjust(entry);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());

    if (m_rg->is_function()) { ASSERT0(sp_adj != nullptr); }

    ORList ors;
    bool modified = false;
    for (INT reg = used_regs->get_first();
         reg != -1; reg = used_regs->get_next(reg)) {
        ors.clean();

        SR * sr = m_cg->getDedicatedSRForPhyReg(reg);
        if (sr == nullptr) {
            //Dedicated SR has assigned reg already.
            sr = m_cg->genReg((UINT)GENERAL_REGISTER_SIZE);
            SR_regfile(sr) = tmMapReg2RegFile(reg);
            SR_phy_reg(sr) = reg;
            SR_is_global(sr) = true; //alloc function level var.
        }

        CLUST clust = m_cg->mapReg2Cluster(reg);
        xoc::Var * loc = m_cg->genSpillVar(sr);
        reg2var.set(reg, loc);

        IOC tc;
        IOC_mem_byte_size(&tc) = sr->getByteSize();
        m_cg->buildStore(sr, loc, 0, ors, &tc);
        m_cg->setCluster(ors, clust);
        m_cg->fixCluster(ors, clust);

        //Need one/two memory operation by default.
        ASSERTN (ors.get_elem_count() == 1 || //[sp + literal(<=24bits)] = t1
                ors.get_elem_count() == 2, //t2=sp+literal(>24bits),[t2]=t1
                ("Too many spill code"));

        ORCt * orct = nullptr;
        ORCt * next = nullptr;
        for (ors.get_head(&orct); orct != ors.end(); orct = next) {
            next = ors.get_next(orct);
            OR * tmp = orct->val();
            if (OR_is_store(tmp) && m_cg->computeSpillVar(tmp) == loc) {
                addVARRefList(entry, tmp, loc);
            }
        }

        //Assign_Spill_Ops_Regfiles(pacdsp_regfile_cluster(f), &ors);
        if (sp_adj != nullptr) {
            ORBB_orlist(entry)->insert_after(ors, sp_adj);
        } else {
            ORBB_orlist(entry)->append_head(ors);
        }

        ASSERT0(verifyORS(ors, entry));
        modified = true;
    }
    if (modified && !bblist.find(entry)) {
        bblist.append_head(entry);
    }
}


//Saving region-used callee registers.
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeFPRegisterAtExit(REGFILE regfile, ORBB * exit,
                                       IN RegSet used_callee_regs[],
                                       OUT ORBBList & bblist,
                                       xcom::TMap<REG, xoc::Var*> const& reg2var)
{
    ASSERT0(tmIsFloatRegFile(regfile));
    RegSet * used_regs = &used_callee_regs[regfile];

    OR * sp_adj = ORBB_exit_spadjust(exit);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());
    if (m_rg->is_function()) { ASSERT0(sp_adj); }

    ORList ors;
    bool modified = false;
    for (INT reg = used_regs->get_first();
         reg != -1; reg = used_regs->get_next(reg)) {
        ors.clean();

        SR * sr = m_cg->getDedicatedSRForPhyReg(reg);
        if (sr == nullptr) {
            //Dedicated SR has assigned reg already.
            sr = m_cg->genReg((UINT)GENERAL_REGISTER_SIZE);
            SR_regfile(sr) = tmMapReg2RegFile(reg);
            SR_phy_reg(sr) = reg;
            SR_is_global(sr) = true; //alloc function level var.
        }

        CLUST clust = m_cg->mapReg2Cluster(reg);

        xoc::Var * v = reg2var.get(reg);
        ASSERT0(v);

        IOC tc;
        IOC_mem_byte_size(&tc) = sr->getByteSize();
        m_cg->buildLoad(sr, v, 0, ors, &tc);
        SR_spill_var(sr) = v;
        m_cg->setCluster(ors, clust);
        m_cg->fixCluster(ors, clust);

        //Need one memory operation by default.
        ASSERTN(ors.get_elem_count() == 1, ("Too many spill code"));
        addVARRefList(exit, ors.get_head(), v);

        ASSERTN (ors.get_elem_count() == 1 || // t1 = [sp + literal(<=24bits)]

                // t2 = sp + literal(>24bits), t1 = [t2]
                ors.get_elem_count() == 2,
                ("Too many spilling code"));

        ORCt * orct = nullptr;
        ORCt * next = nullptr;
        for (ors.get_head(&orct); orct != ors.end(); orct = next) {
            next = ors.get_next(orct);
            OR * tmp = orct->val();
            if (OR_is_load(tmp) && m_cg->computeSpillVar(tmp) == v) {
                addVARRefList(exit, tmp, v);
            }
        }

        if (sp_adj != nullptr) {
            ORBB_orlist(exit)->insert_before(ors, sp_adj);
        } else {
            ORBB_orlist(exit)->append_tail(ors);
        }

        ASSERT0(verifyORS(ors, exit));
        modified = true;
    }

    if (modified && !bblist.find(exit)) {
        bblist.append_head(exit);
    }
}


//Save integer register at entry BB
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeIntRegisterAtEntry(REGFILE regfile, ORBB * entry,
                                         IN RegSet used_callee_regs[],
                                         OUT ORBBList & bblist,
                                         OUT TMap<REG, xoc::Var*> & reg2var)
{
    ASSERT0(tmIsIntRegFile(regfile));
    RegSet * used_regs = &used_callee_regs[regfile];
    OR * sp_adj = ORBB_entry_spadjust(entry);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());
    if (m_rg->is_function()) { ASSERT0(sp_adj != nullptr); }

    ORList ors;
    bool modified = false;
    for (INT reg = used_regs->get_first();
         reg != -1; reg = used_regs->get_next(reg)) {
        ors.clean();

        SR * sr = m_cg->getDedicatedSRForPhyReg(reg);
        if (sr == nullptr) {
            //Dedicated SR has assigned reg already.
            sr = m_cg->genReg((UINT)GENERAL_REGISTER_SIZE);
            SR_regfile(sr) = tmMapReg2RegFile(reg);
            SR_phy_reg(sr) = reg;
            SR_is_global(sr) = true; //alloc function level var.
        }

        CLUST clust = m_cg->mapReg2Cluster(reg);
        xoc::Var * loc = m_cg->genSpillVar(sr);
        reg2var.set(reg, loc);

        IOC tc;
        IOC_mem_byte_size(&tc) = sr->getByteSize();
        m_cg->buildStore(sr, loc, 0, ors, &tc);
        m_cg->setCluster(ors, clust);
        m_cg->fixCluster(ors, clust);

        //Need one/two memory operation by default.
        ASSERTN(ors.get_elem_count() == 1 || //[sp + literal(<=24bits)] = t1
                ors.get_elem_count() == 2, //t2=sp+literal(>24bits),[t2]=t1
                ("Too many spill code"));
        ORCt * orct = nullptr;
        ORCt * next = nullptr;
        for (ors.get_head(&orct); orct != ors.end(); orct = next) {
            next = ors.get_next(orct);
            OR * tmp = orct->val();
            if (OR_is_store(tmp) && m_cg->computeSpillVar(tmp) == loc) {
                addVARRefList(entry, tmp, loc);
            }
        }

        if (sp_adj != nullptr) {
            ORBB_orlist(entry)->insert_after(ors, sp_adj);
        } else {
            ORBB_orlist(entry)->append_head(ors);
        }

        ASSERT0(verifyORS(ors, entry));
        modified = true;
    }
    if (modified && !bblist.find(entry)) {
        bblist.append_head(entry);
    }
}


//Saving region-used callee registers.
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeRegFileAtEntry(REGFILE regfile, ORBB * entry,
                                     IN RegSet used_callee_regs[],
                                     OUT ORBBList & bblist,
                                     xcom::TMap<REG, xoc::Var*> & reg2var)
{
    if (tmIsPredicateRegFile(regfile)) {
        //Predicate regfile always be special.
        saveCalleePredicateAtEntry(regfile, entry, used_callee_regs,
                                   bblist, reg2var);
        return;
    }
    if (tmIsIntRegFile(regfile)) {
        saveCalleeIntRegisterAtEntry(regfile, entry, used_callee_regs,
                                     bblist, reg2var);
        return;
    }
    if (tmIsFloatRegFile(regfile)) {
        saveCalleeFPRegisterAtEntry(regfile, entry, used_callee_regs,
                                    bblist, reg2var);
        return;
    }
}


//Saving region-used callee registers.
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeRegFileAtExit(REGFILE regfile, ORBB * exit,
                                    IN RegSet used_callee_regs[],
                                    OUT ORBBList & bblist,
                                    xcom::TMap<REG, xoc::Var*> const& reg2var)
{
    if (tmIsPredicateRegFile(regfile)) {
        //Predicated regfile always be special.
        saveCalleePredicateAtExit(regfile, exit, used_callee_regs,
                                  bblist, reg2var);
        return;
    }
    if (tmIsIntRegFile(regfile)) {
        saveCalleeIntRegisterAtExit(regfile, exit, used_callee_regs,
                                    bblist, reg2var);
        return;
    }
    if (tmIsFloatRegFile(regfile)) {
        saveCalleeFPRegisterAtExit(regfile, exit, used_callee_regs,
                                   bblist, reg2var);
        return;
    }
}


//Saving region-used callee registers.
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleeIntRegisterAtExit(REGFILE regfile, ORBB * exit,
                                        IN RegSet used_callee_regs[],
                                        OUT ORBBList & bblist,
                                        TMap<REG, xoc::Var*> const& reg2var)
{
    RegSet * used_regs = &used_callee_regs[regfile];
    OR * sp_adj = ORBB_exit_spadjust(exit);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());
    if (m_rg->is_function()) { ASSERT0(sp_adj != nullptr); }

    ORList ors;
    bool modified = false;
    for (INT reg = used_regs->get_first();
         reg != -1; reg = used_regs->get_next(reg)) {
        ors.clean();

        SR * sr = m_cg->getDedicatedSRForPhyReg(reg);
        if (sr == nullptr) {
            //Dedicated SR has assigned reg already.
            sr = m_cg->genReg((UINT)GENERAL_REGISTER_SIZE);
            SR_regfile(sr) = tmMapReg2RegFile(reg);
            SR_phy_reg(sr) = reg;
            SR_is_global(sr) = true; //alloc function level var.
        }

        CLUST clust = m_cg->mapReg2Cluster(reg);
        xoc::Var * v = reg2var.get(reg);
        ASSERT0(v);

        IOC tc;
        IOC_mem_byte_size(&tc) = sr->getByteSize();
        m_cg->buildLoad(sr, v, 0, ors, &tc);

        SR_spill_var(sr) = v;
        m_cg->setCluster(ors, clust);
        m_cg->fixCluster(ors, clust);

        //There should be just one memory operation on most target.
        ASSERTN(ors.get_elem_count() == 1, ("Too many spill code"));
        addVARRefList(exit, ors.get_head(), v);

        ASSERTN(ors.get_elem_count() == 1 || //t1=[sp + literal(<=24bits)]
                ors.get_elem_count() == 2, // t2=sp + literal(>24bits), t1=[t2]
                ("Too many spilling code"));

        ORCt * orct = nullptr;
        ORCt * next = nullptr;
        for (ors.get_head(&orct); orct != ors.end(); orct = next) {
            next = ors.get_next(orct);
            OR * tmp = orct->val();
            if (OR_is_load(tmp) && m_cg->computeSpillVar(tmp) == v) {
                addVARRefList(exit, tmp, v);
            }
        }

        if (sp_adj != nullptr) {
            ORBB_orlist(exit)->insert_before(ors, sp_adj);
        } else {
            ORBB_orlist(exit)->append_tail(ors);
        }

        ASSERT0(verifyORS(ors, exit));
        modified = true;
    }

    if (modified && !bblist.find(exit)) {
        bblist.append_head(exit);
    }
}


//Save predicate register at exit BB
//'bblist': records BBs that need to reallocate register.
void RaMgr::saveCalleePredicateAtExit(REGFILE regfile, ORBB * exit,
                                      IN RegSet used_callee_regs[],
                                      OUT ORBBList & bblist,
                                      xcom::TMap<REG, xoc::Var*> const&)
{
    DUMMYUSE(regfile);
    DUMMYUSE(used_callee_regs);
    DUMMYUSE(bblist);

    //Generate code to save callee predicate registers.
    //Record in ors.
    ASSERTN(0, ("Target Dependent Code"));

    OR * sp_adj = ORBB_exit_spadjust(exit);
    ASSERT0(sp_adj == nullptr || sp_adj->isSpadjust());
    if (m_rg->is_function()) { ASSERT0(sp_adj); }

    ORList ors;
    if (sp_adj != nullptr) {
        ORBB_orlist(exit)->insert_after(ors, sp_adj);
    } else {
        ORBB_orlist(exit)->append_tail(ors);
    }
    ASSERT0(verifyORS(ors, exit));
}


//Generate spilling at entry ORBB and reloading at exit ORBB.
void RaMgr::saveCallee(RegSet used_callee_regs[])
{
    List<ORBB*> bblist;
    bool orig_val = canAllocCallee();
    m_can_alloc_callee = false;
    List<ORBB*> entry_bbs;
    List<ORBB*> exit_bbs;
    m_cg->computeEntryAndExit(*m_cg->getORCFG(), entry_bbs, exit_bbs);

    xcom::TMap<REG, xoc::Var*> reg2var;
    for (ORBB * bb = entry_bbs.get_head();
         bb != nullptr; bb = entry_bbs.get_next()) {
        ASSERTN(ORBB_is_entry(bb) , ("not an entry BB"));
        for(INT regfile = RF_UNDEF + 1; regfile < RF_NUM; regfile++) {
            saveCalleeRegFileAtEntry((REGFILE)regfile,
                                     bb, used_callee_regs, bblist, reg2var);
        }
    }

    for (ORBB * bb = exit_bbs.get_head();
         bb != nullptr; bb = exit_bbs.get_next()) {
        ASSERTN(ORBB_is_exit(bb), ("not an exit BB"));
        for(INT regfile = RF_UNDEF + 1; regfile < RF_NUM; regfile++) {
            saveCalleeRegFileAtExit((REGFILE)regfile,
                                    bb, used_callee_regs, bblist, reg2var);
        }
    }

    //Do LRA for BB if new memory operations generated.
    for (ORBB * bb = bblist.get_head(); bb != nullptr; bb = bblist.get_next()) {
        ParallelPartMgr * ppm = nullptr;
        if (m_ppm_vec != nullptr) {
            ppm = m_ppm_vec->get(bb->id());
        }
        LRA * lra = allocLRA(bb, ppm, this);
        lra->perform();
        delete lra;
    }

    m_can_alloc_callee = orig_val;
}


//Record that 'loc' is referred in 'bb'.
void RaMgr::addVARRefList(ORBB * bb, OR * o, xoc::Var const* loc)
{
    RefORBBList * ref_bb_list = m_var2or_map.get(loc);
    if (ref_bb_list == nullptr) {
        ref_bb_list = (RefORBBList*)xmalloc(sizeof(RefORBBList));
        ref_bb_list->init();
        m_var2or_map.set(loc, ref_bb_list);
    }
    ref_bb_list->addOR(bb, o);
}


//Keep LRA info of each BB for followed optimizations.
//Free logged LRA during destroy().
void RaMgr::performLRA()
{
    START_TIMER(t, "Perform Local Register Allocation");
    xcom::C<ORBB*> * ct = nullptr;
    for (m_bb_list->get_head(&ct);
         ct != m_bb_list->end(); ct = m_bb_list->get_next(ct)) {
        ORBB * bb = ct->val();
        ParallelPartMgr * ppm = nullptr;
        if (m_ppm_vec != nullptr) {
            ppm = m_ppm_vec->get(bb->id());
        }
        LRA * lra = allocLRA(bb, ppm, this);
        lra->setOptPhase(LRA_VERIFY_REG);
        lra->perform();
        m_cg->getBBLevelVarList()->freeAll();
        delete lra;
    }
    postProcess();
    END_TIMER(t, "Perform Local Register Allocation");
}


void RaMgr::performGRA()
{
    if (m_gra == nullptr) {
        m_gra = allocGRA();
    }
    m_gra->perform();
}


//'is_log': false value means that Caller will delete
//          the object allocated utilmately.
LifeTimeMgr * RaMgr::allocLifeTimeMgr()
{
    return (LifeTimeMgr*)new LifeTimeMgr(m_cg);
}


GRA * RaMgr::allocGRA()
{
    return new GRA(this, m_cg);
}


//'is_log': false value means that Caller will delete
//          the object allocated utilmately.
LRA * RaMgr::allocLRA(IN ORBB * bb, IN ParallelPartMgr * ppm, IN RaMgr * ra_mgr)
{
    LRA * p = new LRA(bb, ra_mgr);
    p->setParallelPartMgr(ppm);
    return p;
}

} //namespace xgen
