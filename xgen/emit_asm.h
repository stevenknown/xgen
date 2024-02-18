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
#ifndef __EMIT_ASM_H__
#define __EMIT_ASM_H__

namespace xgen {

class AsmPrinterMgr;
class CG;

//This class prints assembly to text file.
class AsmPrinter {
    COPY_CONSTRUCTOR(AsmPrinter);
protected:
    CG const* m_cg;
    Region const* m_rg;
    CGMgr * m_cgmgr;
    xoc::TypeMgr const* m_tm;
    xoc::VarMgr const* m_vm;
    AsmPrinterMgr * m_asmprtmgr;
    FILE * m_asmh;
protected:
    FILE * getFile() const { return m_asmh; }
public:
    AsmPrinter(CG const* cg, AsmPrinterMgr * apmgr, FILE * asmh);
    virtual ~AsmPrinter() {}

    Region const* getRegion() const { return m_rg; }
    VarMgr const* getVarMgr() const { return m_vm; }
    CG const* getCG() const { return m_cg; }

    //Print OR to string buffer.
    virtual CHAR * printOR(OR * o, xcom::StrBuf & buf)
    {
        ASSERTN(0, ("Target Dependent Code"));
        DUMMYUSE(o);
        DUMMYUSE(buf);
        return nullptr;
    }

    //Print data to assembly file.
    void printData() { printData(getFile()); }
    virtual void printData(FILE *) { ASSERTN(0, ("Target Dependent Code")); }

    //Print instructions to assembly file.
    void printCode() { printCode(getFile()); }
    virtual void printCode(FILE * asmh);
};


class AsmPrinterMgr {
    COPY_CONSTRUCTOR(AsmPrinterMgr);
    //Record VARs which has been printed into ASM file.
    ConstVarTab m_prted_var_tab;
public:
    AsmPrinterMgr() {}
    size_t count_mem() const { return m_prted_var_tab.count_mem(); }
    ConstVarTab & getPrintedVARTab() { return m_prted_var_tab; }
};

} //namespace xgen
#endif
