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
@*/

#include "../opt/cominc.h"
#include "xoccinc.h"
#include "../xgen/xgeninc.h"
#include "errno.h"
#include "../opt/util.h"
#include "../reader/grreader.h"
#include "../opt/comopt.h"

namespace xocc {

void CLDbxMgr::printSrcLine(xoc::Dbx const* dbx, PrtCtx * ctx)
{
    ASSERT0(ctx && dbx);
    if (ctx->logmgr == nullptr) { return; }

    UINT lineno = getLineNum(dbx);
    if (lineno == m_cur_lineno) {
        //It is dispensable that print the same souce file multiple times.
        return;
    }

    m_cur_lineno = lineno;
    if (lineno == 0) {
        //No line number info recorded.
        if (ctx != nullptr && ctx->prefix != nullptr) {
            note(ctx->logmgr, "\n%s[0]\n", ctx->prefix);
        } else {
            note(ctx->logmgr, "\n[0]\n");
        }
        return;
    }

    if (g_hsrc != nullptr) {
        UINT srcline = CParser::mapRealLineToSrcLine(m_cur_lineno);
        if (srcline == 0) {
            srcline = m_cur_lineno;
        }
        ASSERTN(srcline < OFST_TAB_LINE_SIZE, ("unexpected src line"));
        ::fseek(g_hsrc, g_ofst_tab[srcline], SEEK_SET);
        if (::fgets(g_cur_line, g_cur_line_len, g_hsrc) != nullptr) {
            if (ctx != nullptr && ctx->prefix != nullptr) {
                note(ctx->logmgr, "\n\n%s[%u]%s",
                     ctx->prefix, m_cur_lineno, g_cur_line);
            } else {
                note(ctx->logmgr, "\n\n[%u]%s", m_cur_lineno, g_cur_line);
            }
        }
    }
}


void CLDbxMgr::printSrcLine(xcom::StrBuf & output, Dbx const* dbx, PrtCtx * ctx)
{
    ASSERT0(ctx && dbx);
    UINT lineno = getLineNum(dbx);
    if (lineno == m_cur_lineno) {
        //It is dispensable that print the same souce file multiple times.
        return;
    }
    m_cur_lineno = lineno;
    if (lineno == 0) {
        //No line number info recorded.
        if (ctx != nullptr && ctx->prefix != nullptr) {
            output.strcat("\n%s[0]\n", ctx->prefix);
        } else {
            output.strcat("\n[0]\n");
        }
        return;
    }

    if (g_hsrc != nullptr) {
        UINT srcline = CParser::mapRealLineToSrcLine(m_cur_lineno);
        if (srcline == 0) {
            srcline = m_cur_lineno;
        }
        ASSERTN(srcline < OFST_TAB_LINE_SIZE, ("unexpected src line"));
        ::fseek(g_hsrc, g_ofst_tab[srcline], SEEK_SET);
        if (::fgets(g_cur_line, g_cur_line_len, g_hsrc) != nullptr) {
            if (ctx != nullptr && ctx->prefix != nullptr) {
                output.strcat("\n\n%s[%u]%s", ctx->prefix,
                              m_cur_lineno, g_cur_line);
            } else {
                output.strcat("\n\n[%u]%s", m_cur_lineno, g_cur_line);
            }
        }
    }
}

} //namespace xocc
