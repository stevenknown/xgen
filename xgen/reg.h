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
#ifndef __REG_H__
#define __REG_H__

namespace xgen {

typedef ULONG SymRegId; //Symbol Register Id, it must be different with REG.

//Register set
class RegSet : public xcom::BitSet {
public:
    RegSet(UINT init_pool_size = 32) : xcom::BitSet(init_pool_size) {}
    RegSet(RegSet const& rs) { xcom::BitSet::copy(rs); }
    void dump_rs(FILE * h);
};


//Register File set
class RegFileSet : public xcom::BitSet {
public:
    RegFileSet(UINT init_pool_size = 2) : xcom::BitSet(init_pool_size) {}
    RegFileSet(RegFileSet const& rfs) { xcom::BitSet::copy(rfs); }
};


class RegSetMgr {
    List<RegSet*> m_regset_list;
public:
    ~RegSetMgr()
    {
        for (RegSet * rs = m_regset_list.get_head();
             rs != NULL; rs = m_regset_list.get_next()) {
            delete rs;
        }
    }

    RegSet * allocRegSet()
    {
        RegSet * rs = new RegSet();
        m_regset_list.append_tail(rs);
        return rs;
    }
};
} //namespace xgen
#endif
