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

IR * ARMIRSimp::simplifyToPR(IR * ir, SimpCtx * ctx)
{
    if (SIMP_to_pr_mode(ctx)) {
        if (!xgen::g_do_cg || !ir->is_mc()) { goto NEXT; }
        ASSERT0(ir->is_exp());
        IR * stmt = ir->getStmt();
        ASSERT0(stmt);
        if (!stmt->hasRHS()) { goto NEXT; }
        IR * rhs = stmt->getRHS();
        if (rhs != ir) { goto NEXT; }
        if (ir->is_mc() && stmt->is_mc()) {
            //ARM cg does not handle the code generation to MC typed PR->Mem.
            return ir;
        }
    }
NEXT:
    return IRSimp::simplifyToPR(ir, ctx);
}


IR * ARMIRSimp::simplifyRHSInPRMode(IR * ir, SimpCtx * ctx)
{
    if (SIMP_to_pr_mode(ctx)) {
        ASSERT0(ir->is_stmt());
        ASSERT0(ir->hasRHS());
        if (ir->is_mc() && ir->getRHS()->is_mc() && xgen::g_do_cg) {
            //ARM cg does not handle the code generation to MC typed PR->Mem.
            return nullptr;
        }
    }
    return IRSimp::simplifyRHSInPRMode(ir, ctx);
}
