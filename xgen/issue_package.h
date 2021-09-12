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
#ifndef __ISSUE_PACKAGE_H__
#define __ISSUE_PACKAGE_H__

namespace xgen {

class IssuePackageList;
class IssuePackageMgr;

typedef Vector<IssuePackageList*> IssuePackageListVector;

class IssuePackage : public
    xcom::SimpleVector<OR*, 1, MAX_OR_NUM_IN_ISSUE_PACKAGE> {
public:
    void set(UINT i, OR * elem, CG * cg);
};

typedef SC<IssuePackage*> IssuePackageListCt; //container
typedef SC<IssuePackage*> * IssuePackageListIter; //iterator

class IssuePackageList : public SListCoreEx<IssuePackage*> {
public:
    IssuePackageListCt * append_tail(IssuePackage * ip, IssuePackageMgr * mgr);
};


//This class defined IssuePackage Manager to manage the allocation and
//destroy of IssuePackage.
class IssuePackageMgr {
    COPY_CONSTRUCTOR(IssuePackageMgr);
    SMemPool * m_pool;
    CG * m_cg;
public:
    IssuePackageMgr(CG * cg);
    ~IssuePackageMgr();

    IssuePackage * allocIssuePackage();
    //num: the number of object expected to be allocated.
    //The returned objects arranged as an array.
    IssuePackageList * allocIssuePackageList(UINT num = 1);
    IssuePackageListCt * allocIssuePackageListCt();

    void destroy();

    SMemPool * get_pool() const { return m_pool; }
};

} //namespace xgen

#endif
