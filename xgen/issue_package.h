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
#ifndef __ISSUE_PACKAGE_H__
#define __ISSUE_PACKAGE_H__

namespace xgen {

class IssuePackage : public Vector<OR*, 2> {
public:
};


class IssuePackageList : public List<IssuePackage*> {
public:
    virtual ~IssuePackageList() {}
    virtual void append_tail(IssuePackage * i)
    {
        #ifdef _DEBUG_
        for (IssuePackage * ip = get_head(); ip != NULL; ip = get_next()) {
            ASSERT(ip != i, ("already in list."));
        }
        #endif
        List<IssuePackage*>::append_tail(i);
    }

    virtual void append_tail(IssuePackageList & ipl)
    {
        #ifdef _DEBUG_
        for (IssuePackage * ip = get_head(); ip != NULL; ip = get_next()) {
            for (IssuePackage * ipp = ipl.get_head();
                 ipp != NULL; ipp = ipl.get_next()) {
                ASSERT(ip != ipp, ("already in list."));
            }
        }
        #endif
        List<IssuePackage*>::append_tail(ipl);
    }
};

} //namespace xgen

#endif
