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
#ifndef __SECTION_H__
#define __SECTION_H__

namespace xgen {

#define SECT_ID_UNDEF -1

//Describing program section.e.g. code section, data section.
#define SECT_id(s) (s)->uid //unique section id
#define SECT_var(s) (s)->sect_var //variable of section
#define SECT_size(s) (s)->size //byte size of section
#define SECT_var_list(s) (s)->var_list //list of variables in section
#define SECT_var2vdesc_map(s) (s)->var2vdesc  //map between variable and
                                              //VarDesc

typedef TMap<xoc::Var const*, VarDesc*> Var2Desc;
class Section {
public:
    INT uid;
    xoc::Var * sect_var;
    ULONGLONG size;
    VarElist var_list;
    Var2Desc var2vdesc;

public:
    Section()
    {
        uid = SECT_ID_UNDEF;
        sect_var = nullptr;
        size = 0;
    }
    virtual ~Section() {}

    void clean()
    {
        //Clean xoc::Var, VDDESC, etc func-unit related info
        //which should be initialized to new func unit.
        SECT_var_list(this).clean();
        SECT_size(this) = 0;
        SECT_var2vdesc_map(this).clean();
    }

    virtual void dump(CG const* cg);
    
    ULONGLONG getSize() const { return SECT_size(this); }
    VarElist * getVarList() { return &SECT_var_list(this); }
    Var * getVar() const { return SECT_var(this); }
    Var2Desc * getVar2Desc() { return &SECT_var2vdesc_map(this); }

    INT id() const { return uid; }
};


class StackSection : public Section {
public:
    virtual ~StackSection() {}
    virtual void dump(CG const* cg);
};

class SectionMgr {
    COPY_CONSTRUCTOR(SectionMgr);
    xcom::Vector<Section*> m_sect_vec;
    CGMgr * m_cgmgr;
    TypeMgr * m_tm;
    VarMgr * m_vm;
    MDSystem * m_mdsys;

    Section * allocStackSection();

    //Assign variable to section.
    void assignSectVar(Section * sect, CHAR const* sect_name,
                       bool allocable, UINT size);
public:
    SectionMgr(CGMgr * cgmgr);
    virtual ~SectionMgr();

    virtual Section * allocSection();

    size_t count_mem() const
    { return sizeof(*this) + m_sect_vec.count_mem() - sizeof(m_sect_vec); }

    Section * genSection(CHAR const* sect_name, bool allocable, UINT size);
    Section * genStackSection();
    UINT getSectNum() const { return m_sect_vec.get_last_idx() + 1; }
};

} //namespace xgen
#endif
