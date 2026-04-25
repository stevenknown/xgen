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

namespace xoc {

//Dump function.
void CConv::accDump(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    return dumpConv(ir, rg, ctx);
}


//Dump function.
void CConvOpndGrad::accDump(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    return dumpConvOpndGrad(ir, rg, ctx);
}

IRFieldAccTab::AccInfo CConv::accinfo[CConv::accinfo_num] = {
    IRFieldAccTab::AccInfo(IR_ACC_KID, (void*)CConv::accKid),
    IRFieldAccTab::AccInfo(IR_ACC_SS, (void*)CConv::accSS),
    IRFieldAccTab::AccInfo(IR_ACC_STRIDEW, (void*)CConv::accStrideW),
    IRFieldAccTab::AccInfo(IR_ACC_STRIDEH, (void*)CConv::accStrideH),
};


IRFieldAccTab::AccInfo CConvOpndGrad::accinfo[CConvOpndGrad::accinfo_num] = {
    IRFieldAccTab::AccInfo(IR_ACC_KID, (void*)CConvOpndGrad::accKid),
    IRFieldAccTab::AccInfo(IR_ACC_STRIDEW, (void*)CConvOpndGrad::accStrideW),
    IRFieldAccTab::AccInfo(IR_ACC_STRIDEH, (void*)CConvOpndGrad::accStrideH),
};


UINT getStrideW(IR const* ir)
{
    IRAccStrideFuncType func =
        (IRAccStrideFuncType)IRDES_accstridewfunc(ir->getCode());
    ASSERT0(func);
    return (*func)(const_cast<IR*>(ir));
}


UINT getStrideH(IR const* ir)
{
    IRAccStrideFuncType func =
        (IRAccStrideFuncType)IRDES_accstridehfunc(ir->getCode());
    ASSERT0(func);
    return (*func)(const_cast<IR*>(ir));
}

} //namespace xoc
