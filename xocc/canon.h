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

#ifndef __CANON_H__
#define __CANON_H__

namespace xocc {

//The class canonicalize IR tree into well-formed layout that conform to
//the guidelines of XOC IR pass preferred.

class CanonCtx {
public:
    IR * new_stmts;

    CanonCtx() { new_stmts = nullptr; }
};


class Canon {
    Region * m_rg;
public:
    Canon(Region * rg) { m_rg = rg; }
    COPY_CONSTRUCTOR(Canon);
    ~Canon() {}

    IR * only_left_last(IR * head);

    void handle_call(IN IR * ir, OUT bool & change, CanonCtx * cc);
    IR * handle_lda(IR * ir, bool & change, CanonCtx * cc);
    IR * handle_det_list(IN IR * ir_list, OUT bool & change, CanonCtx * cc);
    IR * handle_stmt(IN IR * ir, OUT bool & change, CanonCtx * cc);
    IR * handle_exp(IN IR * ir, OUT bool & change, CanonCtx * cc);
    IR * handle_select(IN IR * ir, OUT bool & change, CanonCtx * cc);
    IR * handle_stmt_list(IR * ir_list, bool & change);
};

} //namespace xocc
#endif
