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

//
//START Section
//
void Section::dump(CG const* cg)
{
    if (!cg->getRegion()->isLogMgrInit()) { return; }
    xoc::TypeMgr const* tm = cg->getTypeMgr();
    xcom::StrBuf buf(64);
    note(cg->getRegion(), "\nSection:size:%d,", (UINT)SECT_size(this));
    note(cg->getRegion(), "%s", sect_var->dump(buf, tm));
    note(cg->getRegion(), "\n  VarLayOut:");
    List<xoc::Var const*> layout;
    for (xoc::Var const* v = var_list.get_head();
         v != nullptr; v = var_list.get_next()) {
        VarDesc * vd = getVar2Desc()->get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));

        xcom::C<xoc::Var const*> * ct;
        bool find = false;
        for (xoc::Var const* v2 = layout.get_head(&ct);
             v2 != nullptr; v2 = layout.get_next(&ct)) {
            VarDesc * vd2 = getVar2Desc()->get(v2);
            ASSERTN(vd2, ("No VarDesc correspond to xoc::Var"));

            if (vd->getOfst() < vd2->getOfst()) {
                layout.insert_before(v, ct);
                find = true;
                break;
            }
        }

        if (!find) { layout.append_tail(v); }
    }

    for (xoc::Var const* v = layout.get_tail();
         v != nullptr; v = layout.get_prev()) {
        VarDesc * vd = getVar2Desc()->get(v);
        buf.clean();
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));
        note(cg->getRegion(), "\n  (%u)%s",
             (UINT)vd->getOfst(), v->dump(buf, tm));
    }
}
//END Section


//
//START StackSection
//
void StackSection::dump(CG const* cg)
{
    if (!cg->getRegion()->isLogMgrInit()) { return; }
    xoc::TypeMgr const* tm = cg->getTypeMgr();
    xcom::StrBuf buf(64);
    FILE * h = cg->getRegion()->getLogMgr()->getFileHandler();
    fprintf(h, "\nSection:size:%d,",
            (UINT)SECT_size(this) + cg->getMaxArgSectionSize());
    fprintf(h, "%s", sect_var->dump(buf, tm));

    fprintf(h, "\n  VarLayOut:");
    List<xoc::Var const*> layout;
    for (xoc::Var const* v = var_list.get_head();
         v != nullptr; v = var_list.get_next()) {
        VarDesc * vd = getVar2Desc()->get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));

        xcom::C<xoc::Var const*> * ct;
        bool find = false;
        for (xoc::Var const* v2 = layout.get_head(&ct);
             v2 != nullptr; v2 = layout.get_next(&ct)) {
            VarDesc * vd2 = getVar2Desc()->get(v2);
            ASSERTN(vd2, ("No VarDesc correspond to xoc::Var"));

            if (vd->getOfst() < vd2->getOfst()) {
                layout.insert_before(v, ct);
                find = true;
                break;
            }
        }

        if (!find) { layout.append_tail(v); }
    }

    for (xoc::Var const* v = layout.get_tail();
         v != nullptr; v = layout.get_prev()) {
        VarDesc * vd = getVar2Desc()->get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));
        buf.clean();
        fprintf(h, "\n  (%u)%s",
                (UINT)vd->getOfst() + cg->getMaxArgSectionSize(),
                v->dump(buf, tm));
    }

    if (cg->getMaxArgSectionSize() > 0) {
        fprintf(h, "\n  (%u)%s", 0, "real-param");
    }

    fflush(h);
}
//END StackSection


//
//START SectionMgr
//
SectionMgr::SectionMgr(CGMgr * cgmgr)
{
    m_cgmgr = cgmgr;
    m_tm = cgmgr->getRegionMgr()->getTypeMgr();
    m_vm = cgmgr->getRegionMgr()->getVarMgr();
    m_mdsys = cgmgr->getRegionMgr()->getMDSystem();
}
 

SectionMgr::~SectionMgr()
{
    ConstMDIter mdit;
    for (INT i = 0; i <= m_sect_vec.get_last_idx(); i++) {
        Section * sect = m_sect_vec[i]; 
        ASSERT0(sect);

        //Free md's id and var's id back to MDSystem and VarMgr.
        //The index of MD and Var is important resource if there
        //are a lot ones in CG.
        Var * v = sect->getVar();
        m_mdsys->removeMDforVAR(v, mdit);
        m_vm->destroyVar(v);
        
        //NOTE: the variable in section's varlist do not need to be destoried.
        //Because they are not registered in CG.
        //FIXME: spill-var need to be destoried
        delete sect;
    }
}


Section * SectionMgr::allocSection()
{
    return new Section();
}


Section * SectionMgr::allocStackSection()
{
    return new StackSection();
}


//Assign variable to section.
void SectionMgr::assignSectVar(Section * sect, CHAR const* sect_name,
                               bool allocable, UINT size)
{
    UINT id = getSectNum();
    xoc::Type const* type = m_tm->getMCType(4);
    SECT_var(sect) = m_vm->registerVar(sect_name, type, 1,
                                       VAR_GLOBAL|VAR_FAKE);
    VAR_is_unallocable(SECT_var(sect)) = !allocable;
    SECT_size(sect) = size;
    SECT_id(sect) = id;
    m_sect_vec.set(id, sect);
}


Section * SectionMgr::genStackSection()
{
    Section * sect = allocStackSection();
    assignSectVar(sect, ".stack", false, 0);
    return sect;
}


Section * SectionMgr::genSection(CHAR const* sect_name, bool allocable,
                                 UINT size)
{
    Section * sect = allocSection();
    assignSectVar(sect, sect_name, allocable, size);
    return sect;
}
//END SectionMgr

} //namespace xgen
