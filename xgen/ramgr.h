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
#ifndef _RA_MGR_H_
#define _RA_MGR_H_

namespace xgen {

#define RAMGR_var2or_map(lm)        (lm)->m_var2or_map
#define RAMGR_can_alloc_callee(lm)  (lm)->m_can_alloc_callee
#define RAMGR_gra(lm)               (lm)->m_gra
class RaMgr {
friend class LRA;
protected:
    bool m_is_func;
    bool m_need_save_asm_effect;
    Vector<ParallelPartMgr*> * m_ppm_vec;
    //Supply callee-saved regs usage information for LRA.
    RegSet m_lra_used_callee_saved_reg[RF_NUM];
    List<ORBB*> * m_bb_list;
    GRA * m_gra;

    //Supply callee-saved regs usage information for Asm effect.
    RegSet m_lra_asmclobber_callee_saved_reg[RF_NUM];
    SMemPool * m_pool;
    CG * m_cg;
    Region * m_ru;

protected:
    void * xmalloc(INT size);

public:
    VAR2OR m_var2or_map;

    //Set true if callee-register is allocable.
    bool m_can_alloc_callee;

public:
    RaMgr(List<ORBB*> * bbs, bool is_func, CG * cg);
    virtual ~RaMgr() { destroy(); }
    virtual void addVARRefList(ORBB * bb, OR * o, VAR const* loc);
    virtual GRA * allocGRA();
    virtual LRA * allocLRA(
            ORBB * bb,
            ParallelPartMgr * ppm,
            RaMgr * lm);
    virtual LifeTimeMgr * allocLifeTimeMgr();

    virtual void destroy();
    virtual void dumpGlobalVAR2OR();
    virtual void dumpCalleeRegUsedByGra();
    void dumpBBList();

    List<ORBB*> * getBBList() { return m_bb_list; }
    Vector<ParallelPartMgr*> * getParallelPartMgrVec() { return m_ppm_vec; }
    Region * getRegion() { return m_ru; }
    CG * getCG() { return m_cg; }
    RegSet * getLRAUsedCalleeSavedRegSet(REGFILE rf)
    {
        ASSERT0(rf < RF_NUM);
        return &m_lra_used_callee_saved_reg[rf];
    }

    virtual void init(List<ORBB*> * bbs, bool is_func, CG * cg);

    void performLRA();
    void performGRA();
    virtual void postBuild();
    virtual void preBuild();

    virtual void setParallelPartMgrVec(Vector<ParallelPartMgr*> * ppm_vec);
    virtual void saveCalleePredicateAtEntry(
                    REGFILE regfile,
                    IN ORBB * entry,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    OUT TMap<REG, VAR*> & reg2var);
    virtual void saveCalleePredicateAtExit(
                    REGFILE regfile,
                    IN ORBB * exit,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    TMap<REG, VAR*> const& reg2var);
    virtual void saveCalleeRegFileAtEntry(
                    REGFILE regfile,
                    IN ORBB * entry,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    OUT TMap<REG, VAR*> & reg2var);
    virtual void saveCalleeRegFileAtExit(
                    REGFILE regfile,
                    IN ORBB * exit,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    TMap<REG, VAR*> const& reg2var);
    virtual void saveCalleeIntRegisterAtEntry(
                    REGFILE regfile,
                    IN ORBB * entry,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    OUT TMap<REG, VAR*> & reg2var);
    virtual void saveCalleeIntRegisterAtExit(
                    REGFILE regfile,
                    IN ORBB * exit,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    TMap<REG, VAR*> const& reg2var);
    virtual void saveCalleeFPRegisterAtEntry(
                    REGFILE regfile,
                    IN ORBB * entry,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    OUT TMap<REG, VAR*> & reg2var);
    virtual void saveCalleeFPRegisterAtExit(
                    REGFILE regfile,
                    IN ORBB * exit,
                    IN RegSet used_callee_regs[],
                    OUT List<ORBB*> & bblist,
                    TMap<REG, VAR*> const& reg2var);
    virtual void saveCallee(IN RegSet used_callee_regs[]);
    virtual void updateAsmClobberCallee(REGFILE regfile, REG reg);
    virtual void updateCallee(REGFILE regfile, REG reg);
};

} //namespace xgen
#endif
