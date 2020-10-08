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
#ifndef _CL_REGION_MGR_H_
#define _CL_REGION_MGR_H_

namespace xgen {

class CGMgr;

//This class represents Computing Language specified region manager.
//A computing languare always consist of concepts of function and program.
class CLRegionMgr : public xoc::RegionMgr {
    COPY_CONSTRUCTOR(CLRegionMgr);
protected:
    xoc::Region * m_program;

public:
    CLRegionMgr() {}
    virtual ~CLRegionMgr() {}

    xoc::Region * get_program() { return m_program; }
    void set_program(xoc::Region * r) { m_program = r; }

    virtual bool compileFuncRegion(xoc::Region * func,
                                   CGMgr * cgmgr,
                                   FILE * asmh,
                                   xoc::OptCtx * oc);
};

CLRegionMgr * allocRegionMgr();

} //namespace xgen
#endif
