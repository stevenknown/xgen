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
#ifndef _ARM_REGION_MGR_H_
#define _ARM_REGION_MGR_H_

namespace elf {
class ELFMgr;
};
class ARMELFMgr;

class ARMRegionMgr : public RegionMgr {
    COPY_CONSTRUCTOR(ARMRegionMgr);
protected:
    FILE * m_asmfile; //assembly file handler.
    ARMELFMgr * m_em;
    AsmPrinterMgr m_asmprtmgr;
public:
    ARMRegionMgr() { m_asmfile = nullptr; m_em = nullptr; }
    virtual ~ARMRegionMgr() { m_asmfile = nullptr; m_em = nullptr; }

    virtual Region * allocRegion(REGION_TYPE rt);
    virtual VarMgr * allocVarMgr();
    virtual TargInfo * allocTargInfo();

    virtual bool checkIRSwitchCaseInterface(IR_CODE c) const override;
    bool CodeGen(Region * rg);

    void initAsmFileHandler(FILE * asmh) { m_asmfile = asmh; }

    FILE * getAsmFileHandler() const { return m_asmfile; }
    ARMELFMgr * getELFMgr() const { return m_em; }

    void setELFMgr(elf::ELFMgr * em);
};

ARMRegionMgr * allocARMRegionMgr();

#endif
