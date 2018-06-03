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
#include "../opt/cominc.h"
#include "../opt/comopt.h"
#include "../opt/cfs_opt.h"
#include "../opt/prdf.h"
#include "../xgen/xgeninc.h"
#include "../cfe/cfexport.h"
#include "../opt/util.h"

//Compute the alignment for specifical var, and return
//the power of 2.
static UINT computeAlignIsPowerOf2(VAR const* v)
{
    UINT byte_align = VAR_align(v);
    switch (byte_align) {
    case 0:
    case 1: //1 byte
        return 0; //2^0 bit
    case 2: //2 byte
        return 4; //Align is 2^4 bit
    case 4: //4 byte
        return 5; //Align is 2^5 bit
    case 8: //8 byte
        return 6; //Align is 2^6 bit
    default:;
    }
    UNREACHABLE();
    return 0;
}


CHAR * ARMAsmPrinter::printOR(OR * o, StrBuf & buf)
{
    StrBuf tbuf(8);
    buf.strcat("%s", OR_code_name(o));
    UINT i = 0;
    for (; buf.buf[i] != '_' && buf.buf[i] != 0; i++) {
    }
    if (buf.buf[i] == '_') {
        buf.buf[i] = 0;
    }
    if (o->get_pred() != m_cg->genTruePred()) {
        buf.strcat("%s", o->get_pred()->getAsmName(tbuf, m_cg));
    }
    buf.strcat(" ");
    switch (OR_code(o)) {
    case OR_orr_lsr_i:
        //orr Rd, Rs1, Rs2, lsr, #imm
        buf.strcat("%s, ", o->get_result(0)->getAsmName(tbuf, m_cg));
        buf.strcat("%s, ", o->get_opnd(1)->getAsmName(tbuf, m_cg));
        buf.strcat("%s, ", o->get_opnd(2)->getAsmName(tbuf, m_cg));
        buf.strcat("lsr, ");
        buf.strcat("%s, ", o->get_opnd(3)->getAsmName(tbuf, m_cg));
        break;
    case OR_orr_lsl_i:
        //orr Rd, Rs1, Rs2, lsr, #imm
        buf.strcat("%s, ", o->get_result(0)->getAsmName(tbuf, m_cg));
        buf.strcat("%s, ", o->get_opnd(1)->getAsmName(tbuf, m_cg));
        buf.strcat("%s, ", o->get_opnd(2)->getAsmName(tbuf, m_cg));
        buf.strcat("lsl, ");
        buf.strcat("%s, ", o->get_opnd(3)->getAsmName(tbuf, m_cg));
        break;
    default: {
        for (UINT i = 0; i < o->result_num(); i++) {
            if (i != 0) {
                buf.strcat(", ");
            }
            buf.strcat("%s", o->get_result(i)->getAsmName(tbuf, m_cg));
        }
        for (UINT i = 0; i < o->opnd_num(); i++) {
            if (i == 0 && HAS_PREDICATE_REGISTER) {
                //nothing to do
            }
            if (i != 0) {
                buf.strcat(", ");
            }
            buf.strcat("%s", o->get_opnd(i)->getAsmName(tbuf, m_cg));
        }
    }
    }
    return buf.buf;
}


void ARMAsmPrinter::printBss(FILE * asmh, Section & sect)
{
    if (SECT_var_list(&sect).get_elem_count() == 0) { return; }

    StrBuf buf(64);
    ASSERT0(SECT_var(&sect));
    for (VAR const* v = SECT_var_list(&sect).get_head();
         v != NULL; v = SECT_var_list(&sect).get_next()) {
        if (m_asmprtmgr->getPrintedVARTab().find(v) || VAR_is_func_decl(v)) {
            continue;
        }
        m_asmprtmgr->getPrintedVARTab().append(v);

        CHAR const* p = SYM_name(v->get_name());
        buf.clean();
        fprintf(asmh, "\n;;%s", v->dump(buf, m_tm));
        fprintf(asmh, "\n.common %s#, %d, %d",
                p, v->getByteSize(m_tm), VAR_align(v));
    }

    fflush(asmh);
}


void ARMAsmPrinter::printData(FILE * asmh, Section & sect)
{
    if (SECT_var_list(&sect).get_elem_count() == 0) { return; }

    VAR * sect_var = SECT_var(&sect);
    ASSERT0(sect_var);

    StrBuf buf(128);
    bool first = true;
    for (VAR const* v = SECT_var_list(&sect).get_head();
         v != NULL; v = SECT_var_list(&sect).get_next()) {
        if (m_asmprtmgr->getPrintedVARTab().find(v)) {
            continue;
        }
        m_asmprtmgr->getPrintedVARTab().append(v);

        if (first) {
            first = false;
            fprintf(asmh, "\n\n.section %s\n", SYM_name(sect_var->get_name()));
        }

        CHAR const* name = SYM_name(v->get_name());
        if (v->is_string()) {
            CHAR const* p = SYM_name(VAR_string(v));
            ASSERT0(v->getByteSize(m_tm) == xstrlen(p) + 1);
            fprintf(asmh, "\n%s:", name);

            //Always align string in 1 byte.
            //fprintf(asmh, "\n.align %d", computeAlignIsPowerOf2(v));
            fprintf(asmh, "\n.align 0 ;2^0 bit");

            fprintf(asmh, "\n.byte ");
            while (*p != 0) {
                if (*p == 0xa) {
                    if (*(p+1) == 0xd) {
                        p += 2;
                    } else {
                        p += 1;
                    }

                    fprintf(asmh, "0xa, ");
                } else {
                    fprintf(asmh, "'%c', ", *p);
                    p++;
                }
            }
            fprintf(asmh, "0");
            fprintf(asmh, "\n");
        } else {
            buf.clean();
            fprintf(asmh, "\n;;%s", v->dump(buf, m_tm));
            fprintf(asmh, "\n%s:", name);
            fprintf(asmh, "\n.align %d", computeAlignIsPowerOf2(v));
            fprintf(asmh, "\n.byte ");
            for (UINT i = 0; i < v->getByteSize(m_tm); i++) {
                fprintf(asmh, "0 ");
                if (i + 1 < v->getByteSize(m_tm)) {
                    fprintf(asmh, ", ");
                }
            }
            fprintf(asmh, "\n");
        }
    }

    fprintf(asmh, "\n");
    fflush(asmh);
}


void ARMAsmPrinter::printData(FILE * asmh)
{
    printBss(asmh, *m_cg->getBssSection());
    printData(asmh, *m_cg->getDataSection());
    printData(asmh, *m_cg->getRodataSection());
}


void ARMAsmPrinter::printCodeSequentially(
        IssuePackageList * ipl,
        StrBuf & buf,
        FILE * asmh)
{
    ASSERT0(ipl);

    CHAR const* format = "%s%19s";
    CHAR const* code_indent = "";

    for (IssuePackage const* ip = ipl->get_head();
         ip != NULL; ip = ipl->get_next()) {
        fprintf(asmh, "\n");
        OR * o = ip->get(SLOT_G);
        if (o != NULL) {
            buf.clean();
            fprintf(asmh, format, code_indent, printOR(o, buf));
        } else {
            fprintf(asmh, format, code_indent, "nop");
        }
        fprintf(asmh, "\n ");
    }
}




void ARMAsmPrinter::printCode(FILE * asmh)
{
    StrBuf buf(128);
    //Print function name, type, global property.
    CHAR const* func_name = SYM_name(
        m_cg->getRegion()->getRegionVar()->get_name());
    ASSERT0(func_name);
    fprintf(asmh, "\n\n\n\n.section .text, \"ax\", \"progbits\"");
    fprintf(asmh, "\n.align 4");
    fprintf(asmh, "\n.proc %s#", func_name);
    fprintf(asmh, "\n.type %s#, @function", func_name);
    fprintf(asmh, "\n.global %s#", func_name);
    fprintf(asmh, "\n%s#:", func_name);
    fprintf(asmh, "\n");
    fflush(asmh);

    Vector<IssuePackageList*> const* iplvec =
        m_cg->getIssuePackageListVec();
    List<ORBB*> * bblst = m_cg->getORBBList();
    ASSERT0(bblst);
    for (ORBB * bb = bblst->get_head(); bb != NULL; bb = bblst->get_next()) {
        //Print BB info
        fprintf(asmh, "\n;START BB(%d)", ORBB_id(bb));
        if (ORBB_is_entry(bb)) {
            fprintf(asmh, ", entry");
        }
        if (ORBB_is_exit(bb)) {
            fprintf(asmh, ", exit");
        }

        fprintf(asmh, "\n");

        //Print label
        for (LabelInfo const* li = bb->getLabelList().get_head();
             li != NULL; li = bb->getLabelList().get_next()) {
            buf.clean();
            fprintf(asmh, "%s:\n", m_cg->formatLabelName(li, buf));
        }

        //Print instructions
        if (ORBB_ornum(bb) == 0) { continue; }

        buf.clean();
        printCodeSequentially(iplvec->get(ORBB_id(bb)), buf, asmh);
    }
    fprintf(asmh, "\n.endp %s#\n", func_name);
}
