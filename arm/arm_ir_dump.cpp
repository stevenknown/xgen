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

#include "../xgen/xgeninc.h"

static void dumpConvStride(IR const* ir, Region const* rg)
{
    ASSERT0(ir->getCode() == IR_CONV);
    if (CONV_stride_w(ir) != 1) {
        prt(rg, ":stride_w(%u)", CONV_stride_w(ir));
    }
    if (CONV_stride_h(ir) != 1) {
        prt(rg, ":stride_h(%u)", CONV_stride_h(ir));
    }
}


void dumpConv(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    ASSERT0(ir->getCode() == IR_CONV);
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    bool dump_var_decl = ctx.dumpflag.have(IR_DUMP_VAR_DECL);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    dumpConvStride(ir, rg);
    dumpOffset(ir, rg);
    dumpStorageSpace(ir, rg);
    dumpIdinfo(ir, rg);
    if (dump_var_decl) {
        dumpVarDecl(ir, rg);
    }
    DUMPADDR(ir);
    ASSERT0(ctx.attr);
    prt(rg, "%s", ctx.attr);
    if (dump_kid) {
        rg->getLogMgr()->incIndent(ctx.dn);
        xoc::dumpIRList(CONV_input(ir), rg, " input", ctx.dumpflag);
        xoc::dumpIRList(CONV_weight(ir), rg, " weight", ctx.dumpflag);
        rg->getLogMgr()->decIndent(ctx.dn);
    }
}


void dumpConvOpndGrad(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    ASSERT0(ir->getCode() == IR_CONV_OPND_GRAD);
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    bool dump_var_decl = ctx.dumpflag.have(IR_DUMP_VAR_DECL);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));

    //Dump stride.
    if (CONVOPNDGRAD_stride_w(ir) != 1) {
        prt(rg, ":stride_w(%u)", CONVOPNDGRAD_stride_w(ir));
    }
    if (CONVOPNDGRAD_stride_h(ir) != 1) {
        prt(rg, ":stride_h(%u)", CONVOPNDGRAD_stride_h(ir));
    }
    switch (CONVOPNDGRAD_opnd_kind(ir)) {
    case CConvOpndGrad::OK_INPUT:
        prt(rg, ":opnd_kind(input)");
        break;
    case CConvOpndGrad::OK_WEIGHT:
        prt(rg, ":opnd_kind(weight)");
        break;
    default: UNREACHABLE();
    }
    dumpOffset(ir, rg);
    dumpStorageSpace(ir, rg);
    dumpIdinfo(ir, rg);
    if (dump_var_decl) {
        dumpVarDecl(ir, rg);
    }
    DUMPADDR(ir);
    ASSERT0(ctx.attr);
    prt(rg, "%s", ctx.attr);
    if (dump_kid) {
        rg->getLogMgr()->incIndent(ctx.dn);
        xoc::dumpIRList(CONVOPNDGRAD_input(ir), rg, " input", ctx.dumpflag);
        xoc::dumpIRList(CONVOPNDGRAD_weight(ir), rg, " weight", ctx.dumpflag);
        xoc::dumpIRList(CONVOPNDGRAD_grad(ir), rg, " grad", ctx.dumpflag);
        rg->getLogMgr()->decIndent(ctx.dn);
    }
}
