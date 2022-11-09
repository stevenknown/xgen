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
#ifndef _CG_MGR_H_
#define _CG_MGR_H_

namespace xgen {

class ORMgr;
class SRMgr;
class Section;
class SectionMgr;
class IntrinsicMgr;

typedef TMap<BUILTIN_TYPE, Var*> Bltin2Var;
typedef TMapIter<BUILTIN_TYPE, Var*> Bltin2VarIter;

//This class apply objects to allocate OR and SR.
class CGMgr {
    COPY_CONSTRUCTOR(CGMgr);
protected:
    ORMgr * m_or_mgr;
    SRMgr * m_sr_mgr;
    SectionMgr * m_sect_mgr;
    Section * m_code_sect;
    Section * m_data_sect;
    Section * m_rodata_sect;
    Section * m_bss_sect;
    Section * m_param_sect;
    Section * m_stack_sect;
    RegionMgr * m_rm;
    FILE * m_asm_file_handler;
    IntrinsicMgr * m_intrin_mgr;
    SRVecMgr m_sr_vec_mgr;
    AsmPrinterMgr m_asmprtmgr;

    //Builtin function should be initialized in initBuiltin().
    Bltin2Var m_builtin_var;

protected:
    virtual SRMgr * allocSRMgr() { return new SRMgr(); }
    virtual ORMgr * allocORMgr(SRMgr * srmgr) { return new ORMgr(srmgr); }
    virtual SectionMgr * allocSectionMgr() { return new SectionMgr(this); }
    inline xoc::Var * addBuiltinVar(CHAR const* buildin_name)
    {
        ASSERT0(m_rm);        
        //Set builtin variables to be LOCAL to avoid RegionMgr regarded them
        //as global variables. Because too many builtin variables will disrupt
        //the dump and analysis. Moreover it does not matter for builtin call
        //code generation whichever it is LOCAL or GLOBAL variable.
        return m_rm->getVarMgr()->registerVar(buildin_name,
                                              m_rm->getTypeMgr()->getAny(),
                                              MEMORY_ALIGNMENT,
                                              VAR_FAKE|VAR_LOCAL|
                                              VAR_IS_UNALLOCABLE);
    }

    AsmPrinterMgr * getAsmPrtMgr() { return &m_asmprtmgr; }

    //Initialize the SRMgr and ORMgr.
    //Note this is the necessary initialization function you have to call
    //before generate OR and SR.
    void initSRAndORMgr()
    {
        if (m_sr_mgr != nullptr) { return; }
        m_sr_mgr = allocSRMgr();
        m_or_mgr = allocORMgr(getSRMgr());
        m_sr_vec_mgr.init();
    }
    void initSectionMgrAndSections();
    void initBuiltin();

    void destroySRAndORMgr()
    {
        if (m_sr_mgr == nullptr) { return; }
        ASSERT0(getSRMgr() && m_or_mgr);
        delete m_sr_mgr;
        delete m_or_mgr;
        m_sr_mgr = nullptr;
        m_or_mgr = nullptr;
        m_sr_vec_mgr.destroy();
    }
    void destroySectionMgr()
    {
        if (m_sect_mgr == nullptr) { return; }
        delete m_sect_mgr;
        m_sect_mgr = nullptr;
        m_code_sect = nullptr;
        m_data_sect = nullptr;
        m_rodata_sect = nullptr;
        m_bss_sect = nullptr;
        m_param_sect = nullptr;
        m_stack_sect = nullptr;
     }
    //Free md's id and var's id back to MDSystem and VarMgr.
    //The index of MD and Var is important resource if there
    //are a lot of CG.
    void destroyVAR();

public:
    CGMgr(RegionMgr * rm);
    virtual ~CGMgr()
    {
        destroySRAndORMgr();
        destroySectionMgr();
        destroyVAR();
        delete m_intrin_mgr;
        m_intrin_mgr = nullptr;
    }

    //Allocate CG.
    virtual CG * allocCG(Region * rg) = 0;

    //Allocate VarMgr.
    virtual AsmPrinter * allocAsmPrinter(CG const* cg);
    size_t count_mem() const;

    //Generate code and perform target machine dependent operations.
    //region: this function is the main entry to generate code for given
    //        region.
    //asmh: assembly file handler. It is the output of code generation.
    //      If you expect generating code in a buffer, an embedded assembler
    //      is needed(TODO).
    //Basis step to do:
    //  1. Generate target dependent micro operation
    //     representation(named as OR).
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
    virtual bool generate(Region * rg);
    //Print global variable to asmfile.
    virtual bool genAndPrtGlobalVariable(Region * rg);
    SRMgr * getSRMgr() const { return m_sr_mgr; }
    ORMgr * getORMgr() const { return m_or_mgr; }
    SectionMgr * getSectionMgr() const { return m_sect_mgr; }
    SRVecMgr * getSRVecMgr() { return &m_sr_vec_mgr; }
    RegionMgr * getRegionMgr() const { return m_rm; }
    Section * getRodataSection() { return m_rodata_sect; }
    Section * getCodeSection() { return m_code_sect; }
    Section * getDataSection() { return m_data_sect; }
    Section * getBssSection() { return m_bss_sect; }
    Section * getStackSection() { return m_stack_sect; }
    Section * getParamSection() { return m_param_sect; }
    Var * getBuiltinVar(BUILTIN_TYPE bt) const
    { return m_builtin_var.get(bt); }
    FILE * getAsmFileHandler() const { return m_asm_file_handler; }
    IntrinsicMgr * getIntrinMgr() const { return m_intrin_mgr; }

    //Return true if there are any section generated.
    bool has_section() const
    { return m_sect_mgr == nullptr ? false : m_sect_mgr->getSectNum() != 0; }

    //Return true if given ir indicates an intrinsic operation.
    bool isIntrinsic(IR const* ir) const;
    //Return true if given ir indicates the intrinsic operation
    //that name is 'code'.
    bool isIntrinsic(IR const* ir, UINT code) const;

    //Print result of CG to asm file.
    void prtCGResult(CG const* cg);

    //The function destroy all SR and OR objects and reinitialize SRMgr
    //and ORMgr.
    void resetSRAndORMgr()
    {
        destroySRAndORMgr();
        initSRAndORMgr();
    }

    //Set handler for printing assembly into file.
    void setAsmFileHandler(FILE * asmh) { m_asm_file_handler = asmh; }
    CGMgr * self() { return this; }
};

} //namespace xgen
#endif
