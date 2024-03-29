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
#ifndef _ARM_RAMGR_H_
#define _ARM_RAMGR_H_

class ARMRaMgr : public RaMgr {
protected:
    void setUseLR();

public:
    ARMRaMgr(List<ORBB*> * bbs, bool is_func, CG * cg) :
             RaMgr(bbs, is_func, cg) {}

    virtual xgen::LifeTimeMgr * allocLifeTimeMgr();

    virtual void postProcess();

    virtual void reloadPredicateAtExit(REGFILE regfile, ORBB * exit,
                                       xgen::RegSet const used_callee_regs[],
                                       OUT ORBBList &, Reg2Var const&)
    { DUMMYUSE((regfile, exit, used_callee_regs)); }
    virtual void reloadRegFileAtExit(REGFILE regfile, ORBB * exit,
                                     xgen::RegSet const used_callee_regs[],
                                     OUT ORBBList & bblist,
                                     Reg2Var const& reg2var);

    virtual void spillRegFileAtEntry(REGFILE regfile, ORBB * entry,
                                     xgen::RegSet const used_callee_regs[],
                                     OUT ORBBList & bblist,
                                     OUT Reg2Var & reg2var);
    virtual void spillPredicateAtEntry(REGFILE regfile, ORBB * entry,
                                       xgen::RegSet const used_callee_regs[],
                                       OUT ORBBList &, OUT Reg2Var &)
    { DUMMYUSE((regfile, entry, used_callee_regs)); }
};

#endif
