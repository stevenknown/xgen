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

//Kid is float.
IR * ARMRefine::insertCvtForFloatCase2(IR * parent, IR * kid, bool & change)
{
    ASSERT0(kid->is_fp());
    UINT tgt_size = parent->getTypeSize(m_tm);
    UINT src_size = kid->getTypeSize(m_tm);
    bool build = false;
    if (parent->getType()->is_int() || parent->getType()->is_pointer()) {
        //Build the conversion between integer and float.
        build = true;
    } else if (parent->is_fp()) {
        if (tgt_size != src_size) {
            //Build the conversion between different precision float.
            build = true;
        } else {
            //No need to convert.
        }
    } else {
        ASSERTN(0, ("incompatible types in convertion"));
    }
    if (build) {
        Type const* cvtty = parent->getType();
        if (parent->is_add() &&
            parent->is_ptr() &&
            kid == ((CBin*)parent)->getOpnd1() &&
            BIN_opnd0(parent)->is_ptr()) {
            //IR_ADD could not add two pointer type. The addend type should be
            //integer.
            cvtty = m_tm->getPointerSizeType();
        }
        ASSERT0(cvtty);
        IR * new_kid = m_rg->getIRMgr()->buildCvt(kid, cvtty);
        copyDbx(new_kid, kid, m_rg);
        change = true;
        return new_kid;
    }
    return kid;
}


//Parent is float.
IR * ARMRefine::insertCvtForFloatCase1(IR * parent, IR * kid, bool & change)
{
    ASSERT0(parent->is_fp());
    UINT tgt_size = parent->getTypeSize(m_tm);
    UINT src_size = kid->getTypeSize(m_tm);
    bool build = false;
    if (kid->getType()->is_int() || kid->getType()->is_pointer()) {
        //Build the conversion between float and integer.
        build = true;
    } else if (kid->is_fp()) {
        if (tgt_size != src_size) {
            //Build the conversion between different precision float.
            build = true;
        } else {
            //No need to convert.
        }
    } else {
        ASSERTN(0, ("incompatible types in convertion"));
    }
    if (build) {
        IR * new_kid = m_rg->getIRMgr()->buildCvt(kid, parent->getType());
        copyDbx(new_kid, kid, m_rg);
        change = true;
        return new_kid;
    }
    return kid;
}


//Insert CVT for float if necessary.
IR * ARMRefine::insertCvtForFloat(IR * parent, IR * kid, bool & change)
{
    if (parent->is_fp()) {
        return insertCvtForFloatCase1(parent, kid, change);
    }
    ASSERT0(kid->is_fp());
    return insertCvtForFloatCase2(parent, kid, change);
}
