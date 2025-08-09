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
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@*/
#include "../xgen/xgeninc.h"
#include "../reader/reader.h"
#include "../arm/arm_ir_parser.h"

void ARMIRParser::initExpKeyWord()
{
    IRParser::initExpKeyWord();
    for (UINT i = X_ARM_EXP_START; i <= X_ARM_EXP_END; i++) {
        m_exp2xcode.set(getKeyWordName((X_CODE)i), (X_CODE)i);
    }
}


bool ARMIRParser::parseUserop1(MOD ParseCtx * ctx)
{
    ASSERT0(getCurrentXCode() == X_USEROP1);
    IR_CODE code = IR_ADD;

    //Fetch a token.
    m_lexer->getNextToken();

    //Type
    Type const* ty = nullptr;
    if (!parseTypeWrap(ctx, ty)) { return false; }

    //Properties
    PropertySet ps;
    if (!parsePropertyWrap(code, ctx, ps)) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd0 = ctx->returned_exp;

    //Parse comma.
    if (!parseCommaWrap()) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd1 = ctx->returned_exp;

    //Parse comma.
    if (!parseCommaWrap()) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd2 = ctx->returned_exp;
    ASSERT0_DUMMYUSE(opnd2);

    //Parse comma.
    if (!parseCommaWrap()) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd3 = ctx->returned_exp;
    ASSERT0_DUMMYUSE(opnd3);

    //Parse comma.
    if (!parseCommaWrap()) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd4 = ctx->returned_exp;
    ASSERT0_DUMMYUSE(opnd4);

    //Complete type.
    ty = reviseTypeByCode(code, ty);

    //Build exp.
    ASSERT0(ctx->current_region);
    IR * exp = ctx->current_region->getIRMgr()->buildBinaryOpSimp(
        code, ty, opnd0, opnd1);
    ctx->returned_exp = exp;
    copyProp(exp, ps, ctx);
    return true;   return false;
}


bool ARMIRParser::parseUserop0(MOD ParseCtx * ctx)
{
    ASSERT0(getCurrentXCode() == X_USEROP0);
    IR_CODE code = IR_ADD;

    //Fetch a token.
    m_lexer->getNextToken();

    //Type
    Type const* ty = nullptr;
    if (!parseTypeWrap(ctx, ty)) { return false; }

    //Properties
    PropertySet ps;
    if (!parsePropertyWrap(code, ctx, ps)) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd0 = ctx->returned_exp;

    //Parse comma.
    if (!parseCommaWrap()) { return false; }

    //Parse exp.
    if (!parseExpWrap(ctx)) { return false; }
    IR * opnd1 = ctx->returned_exp;

    //Complete type.
    ty = reviseTypeByCode(code, ty);

    //Build exp.
    ASSERT0(ctx->current_region);
    IR * exp = ctx->current_region->getIRMgr()->buildBinaryOpSimp(
        code, ty, opnd0, opnd1);
    ctx->returned_exp = exp;
    copyProp(exp, ps, ctx);
    return true;
}


bool ARMIRParser::parseExtXCode(ParseCtx * ctx)
{
    X_CODE code = getCurrentXCode();
    switch (code) {
    case X_USEROP0: return parseUserop0(ctx);
    case X_USEROP1: return parseUserop1(ctx);
    default:
        error(code, "'%s' is not an operator", getKeyWordName(code));
        return false;
    }
    return false;
}
