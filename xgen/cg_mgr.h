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
#ifndef _CG_MGR_H_
#define _CG_MGR_H_

namespace xgen {

//This class apply objects to allocate OR and SR.
class CGMgr {
    COPY_CONSTRUCTOR(CGMgr);
protected:
    ORMgr * m_or_mgr;
    SRMgr * m_sr_mgr;
    SRVecMgr m_sr_vec_mgr;
    AsmPrinterMgr m_asmprtmgr;

protected:
    virtual SRMgr * allocSRMgr() { return new SRMgr(); }
    virtual ORMgr * allocORMgr(SRMgr * srmgr) { return new ORMgr(srmgr); }

    void initSRAndORMgr()
    {
        if (m_sr_mgr != nullptr) { return; }
        m_sr_mgr = allocSRMgr();
        m_or_mgr = allocORMgr(getSRMgr());    
        m_sr_vec_mgr.init();
    }

    void finiSRAndORMgr()
    {
        if (m_sr_mgr == nullptr) { return; }
        ASSERT0(getSRMgr() && m_or_mgr);
        delete m_sr_mgr;
        delete m_or_mgr;
        m_sr_mgr = nullptr;
        m_or_mgr = nullptr;
        m_sr_vec_mgr.destroy();
    }
public:
    //You need invoke init() after CGMgr constructed.
    CGMgr() { m_sr_mgr = nullptr; m_or_mgr = nullptr; }
    virtual ~CGMgr() { finiSRAndORMgr(); }

    //Allocate CG.
    virtual CG * allocCG(Region * rg) = 0;

    //Allocate VarMgr.
    virtual AsmPrinter * allocAsmPrinter(CG * cg, AsmPrinterMgr * asmprtmgr);

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
    virtual bool CodeGen(Region * region, FILE * asmh);
    void clean()
    {
        finiSRAndORMgr();
        initSRAndORMgr();        
    }

    //Print global variable to asmfile.
    virtual bool GenAndPrtGlobalVariable(Region * rg, FILE * asmh);

    //Initialize the SRMgr and ORMgr.
    //Note this is the first function you should invoked after
    //CGMgr constructed.
    void init() { initSRAndORMgr(); }

    SRMgr * getSRMgr() const { return m_sr_mgr; }
    ORMgr * getORMgr() const { return m_or_mgr; }
    SRVecMgr * getSRVecMgr() { return &m_sr_vec_mgr; }
};

} //namespace xgen
#endif
