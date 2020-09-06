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
         v != NULL; v = var_list.get_next()) {
        VarDesc * vd = var2vdesc_map.get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));

        xcom::C<xoc::Var const*> * ct;
        bool find = false;
        for (xoc::Var const* v2 = layout.get_head(&ct);
             v2 != NULL; v2 = layout.get_next(&ct)) {
            VarDesc * vd2 = var2vdesc_map.get(v2);
            ASSERTN(vd2, ("No VarDesc correspond to xoc::Var"));

            if (VD_ofst(vd) < VD_ofst(vd2)) {
                layout.insert_before(v, ct);
                find = true;
                break;
            }
        }

        if (!find) { layout.append_tail(v); }
    }

    for (xoc::Var const* v = layout.get_tail();
         v != NULL; v = layout.get_prev()) {
        VarDesc * vd = var2vdesc_map.get(v);
        buf.clean();
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));
        note(cg->getRegion(), "\n  (%u)%s",
             (UINT)VD_ofst(vd), v->dump(buf, tm));
    }
}


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
         v != NULL; v = var_list.get_next()) {
        VarDesc * vd = var2vdesc_map.get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));

        xcom::C<xoc::Var const*> * ct;
        bool find = false;
        for (xoc::Var const* v2 = layout.get_head(&ct);
             v2 != NULL; v2 = layout.get_next(&ct)) {
            VarDesc * vd2 = var2vdesc_map.get(v2);
            ASSERTN(vd2, ("No VarDesc correspond to xoc::Var"));

            if (VD_ofst(vd) < VD_ofst(vd2)) {
                layout.insert_before(v, ct);
                find = true;
                break;
            }
        }

        if (!find) { layout.append_tail(v); }
    }

    for (xoc::Var const* v = layout.get_tail();
         v != NULL; v = layout.get_prev()) {
        VarDesc * vd = var2vdesc_map.get(v);
        ASSERTN(vd, ("No VarDesc correspond to xoc::Var"));
        buf.clean();
        fprintf(h, "\n  (%u)%s",
                (UINT)VD_ofst(vd) + cg->getMaxArgSectionSize(),
                v->dump(buf, tm));
    }

    if (cg->getMaxArgSectionSize() > 0) {
        fprintf(h, "\n  (%u)%s", 0, "real-param");
    }

    fflush(h);
}
//END StackSection
