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
#include "xgeninc.h"
#include "emit_asm.h"

namespace xgen {

AsmPrinter::AsmPrinter(CG const* cg, AsmPrinterMgr * apmgr, FILE * asmh)
{
    ASSERT0(cg && apmgr);
    m_cg = cg;
    m_rg = cg->getRegion();
    m_cgmgr = cg->getCGMgr();
    m_tm = m_cg->getTypeMgr();
    m_vm = m_rg->getVarMgr();
    m_asmprtmgr = apmgr;
    m_asmh = asmh;
}


//Print instructions to assembly file.
void AsmPrinter::printCode()
{
    ASSERTN(FIRST_SLOT == LAST_SLOT, ("Target Dependent Code"));
    CHAR const* code_indent = "      ";
    Vector<IssuePackageList*> * ipcvec =
        const_cast<CG*>(m_cg)->getIssuePackageListVec();
    List<ORBB*> * bblst = const_cast<CG*>(m_cg)->getORBBList();
    StrBuf buf(128);
    FILE * asmh = getFile();
    for (ORBB * bb = bblst->get_head();
        bb != nullptr; bb = bblst->get_next()) {
        IssuePackageList * ipl = ipcvec->get(bb->id());
        ASSERT0(ipl != nullptr);
        for (IssuePackageListIter it = ipl->get_head();
             it != ipl->end(); it = ipl->get_next(it)) {
            IssuePackage * ip = it->val();
            fprintf(asmh, "\n { ");
            OR * o = ip->get(FIRST_SLOT);
            if (o != nullptr) {
                buf.clean();
                fprintf(asmh, "%s%s\n", code_indent, printOR(o, buf));
            }
            else {
                fprintf(asmh, "%s%s\n", code_indent, "nop");
            }
            fprintf(asmh, "}");
            fflush(asmh);
        }
    }
    fflush(asmh);
}

} //namespace xgen
