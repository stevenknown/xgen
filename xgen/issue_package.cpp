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
#include "xgeninc.h"

namespace xgen {

//
//START IssuePackage
//
void IssuePackage::set(UINT i, OR * elem, CG * cg)
{
    SimpleVector<OR*, 1, MAX_OR_NUM_IN_ISSUE_PACKAGE>::set(
        i, elem, cg->getIssuePackageMgr()->get_pool());
}


//END IssuePackage
//
//START IssuePackageList
//
IssuePackageListCt * IssuePackageList::append_tail(IssuePackage * ip,
                                                   IssuePackageMgr * mgr)
{
    IssuePackageListCt * ct = mgr->allocIssuePackageListCt();
    SC_val(ct) = ip;
    SListCoreEx<IssuePackage*>::append_tail(ct); 
    return ct;
}


//
//START IssuePacketMgr
//
IssuePackageMgr::IssuePackageMgr(CG * cg)
{
    m_cg = cg;
    m_pool = smpoolCreate(64, MEM_COMM);
}


IssuePackageMgr::~IssuePackageMgr()
{
    destroy();
}


void IssuePackageMgr::destroy()
{
    smpoolDelete(m_pool);
    m_pool = nullptr;
}


static void * xmalloc(IssuePackageMgr * ipmgr, size_t size)
{
    ASSERT0(ipmgr);
    void * p = smpoolMalloc(size, ipmgr->get_pool());
    ASSERT0(p);
    ::memset(p, 0, size);
    return p;
}


IssuePackage * IssuePackageMgr::allocIssuePackage()
{
    IssuePackage * ip = (IssuePackage*)xmalloc(this, sizeof(IssuePackage));
    ip->init();
    return ip;
}


//num: the number of object expected to be allocated.
IssuePackageList * IssuePackageMgr::allocIssuePackageList(UINT num)
{
    IssuePackageList * ipl = (IssuePackageList*)xmalloc(
        this, sizeof(IssuePackageList) * num);
    for (UINT i = 0; i < num; i++) {
        ipl[i].init();
    }
    return ipl;
}


IssuePackageListCt * IssuePackageMgr::allocIssuePackageListCt()
{
    IssuePackageListCt * ct = (IssuePackageListCt*)xmalloc(
        this, sizeof(IssuePackageListCt));
    ct->init();
    return ct;
}
//END IssuePacketMgr

} //namespace xgen
