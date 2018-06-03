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

//
//START ARMORMgr
//
OR * ARMORMgr::allocOR()
{
    return new ARMOR();
}

CG * g_cg = NULL; //FIXME
void ARMOR::set_pred(SR * v)
{
    ASSERT_DUMMYUSE(HAS_PREDICATE_REGISTER, ("target not support"));

    //FIXME
    //Set predicate register to TRUE-prediate always to avoid validataion assert.
    //This hack code intends to pass compiling ARM target.
    ASSERT0(g_cg);
    set_opnd(0, g_cg->genTruePred());
}
//END ARMORMgr


//
//START ARMOR
//
LabelInfo const* ARMOR::getLabel() const
{
    ASSERT0(OR_is_uncond_br(this) ||
            OR_is_cond_br(this) ||
            OR_is_call(this) ||
            OR_code(this) == OR_label);
    return OR::getLabel();
}
//END ARMOR
