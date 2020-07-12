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
#include "../opt/liveness_mgr.h"
#include "../xgen/xgeninc.h"
#include "../cfe/cfexport.h"
#include "../opt/util.h"

//Insert CVT for float if necessary.
IR * ARMRefine::insertCvtForFloat(IR * parent, IR * kid, bool & change)
{
    ASSERT0(parent->is_fp() || kid->is_fp());
    UINT tgt_size = parent->getTypeSize(m_tm);
    UINT src_size = kid->getTypeSize(m_tm);

    bool build = false;
    if (parent->is_fp()) {
        if (kid->getType()->is_int()) {
            build = true;
        } else if (kid->is_fp()) {
            if (tgt_size != src_size) {
                build = true;
            }
        } else {
            ASSERTN(0, ("incompatible types in convertion"));
        }
    } else {
        if (parent->getType()->is_int()) {
            build = true;
        } else if (parent->is_fp()) {
            if (tgt_size != src_size) {
                build = true;
            }
        } else {
            ASSERTN(0, ("incompatible types in convertion"));
        }
    }

    if (build) {
        IR * new_kid = m_rg->buildCvt(kid, parent->getType());
        copyDbx(new_kid, kid, m_rg);
        change = true;
        return new_kid;
    }

    return kid;
}
