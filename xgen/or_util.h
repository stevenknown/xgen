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
#ifndef __OR_UTIL_H__
#define __OR_UTIL_H__

namespace xgen {

#define MAX_ST 5
#define MAX_INPUT 10
#define MIN_IOR_NUM 0.85 //Defined minimal factor of percent to assign regfile

//Defined base factor that must be added to estimation in cyc-estimate
#define SL_BASE_FACTOR 0.5
#define INFINITE 0x7FFFFFFF //Defined Pseudo finitesimal I supposed
#define EPSILON 0.000001 //Defined Pseudo infinitesimal I supposed
#define UNBALANCE_RATE 9999999.5 //Used in register bank partitioning

typedef List<OR const*> ConstORList;
typedef xcom::TMap<SR*, SR*> SR2SR;
typedef xcom::DMap<SR*, SR*, SR2SR, SR2SR> SR2SR_DMAP;

//EList of Var
typedef xcom::EList<xoc::Var const*,
                    xcom::TMap<xoc::Var const*,
                               xcom::C<xoc::Var const*>*> > VarElist;

#define VD_ofst(m) ((m)->m_ofst)
class VarDesc {
public:
    size_t m_ofst;

    size_t getOfst() const { return m_ofst; }
};


//Mapping from SR to OR list.
//Managing program memory automatically.
class SR2ORList : public xcom::TMap<SR*, ConstORList*> {
    List<ConstORList*> m_orlst_lst;
public:
    virtual ~SR2ORList()
    {
        for (ConstORList * ol = m_orlst_lst.get_head();
             ol != nullptr; ol = m_orlst_lst.get_next()) {
            delete ol;
        }
    }

    //'sr' corresponds to single 'or'.
    virtual void set(SR * sr, OR const* o)
    {
        ASSERT0(sr && o);
        ConstORList * orlst = xcom::TMap<SR*, ConstORList*>::get(sr);
        if (orlst == nullptr) {
            orlst = new ConstORList();
            m_orlst_lst.append_tail(orlst);
            xcom::TMap<SR*, ConstORList*>::set(sr, orlst);
        } else {
            //In single ORBB, only record the immediate-dominate DEF or.
            ASSERT0(orlst->get_elem_count() <= 1);
            orlst->remove_head();
        }
        orlst->append_tail(o);
    }

    //'sr' corresponds to multiple 'or'.
    void append(SR * sr, OR * o)
    {
        ASSERT0(sr && o);
        ConstORList * orlst = xcom::TMap<SR*, ConstORList*>::get(sr);
        if (orlst == nullptr) {
            set(sr, o);
        } else {
            orlst->append_tail(o);
        }
    }

    void clean(SR * sr)
    {
        ASSERT0(sr);
        ConstORList * orlst = xcom::TMap<SR*, ConstORList*>::get(sr);
        if (orlst != nullptr) {
            orlst->clean();
        }
    }
};


//Mapping between an unsigned long integer and a list of OR.
//Manage object memory automatically.
class UINT2ConstORList : public xcom::TMap<UINT, ConstORList*> {
    List<ConstORList*> m_orlst_lst; //Collect all allocated objects.
public:
    virtual ~UINT2ConstORList()
    {
        for (ConstORList * ol = m_orlst_lst.get_head();
             ol != nullptr; ol = m_orlst_lst.get_next()) {
            delete ol;
        }
    }

    virtual void set(UINT u, OR const* o)
    {
        ASSERT0(u != 0 && o); //'0' is set as default nullptr
        ConstORList * orlst = xcom::TMap<UINT, ConstORList*>::get(u);
        if (orlst == nullptr) {
            orlst = new ConstORList();
            m_orlst_lst.append_tail(orlst);
            xcom::TMap<UINT, ConstORList*>::set(u, orlst);
        }
        orlst->append_tail(o);
    }

    virtual void clean(UINT u)
    {
        ASSERT0(u != 0); //'0' is set as default nullptr
        ConstORList * orlst = xcom::TMap<UINT, ConstORList*>::get(u);
        if (orlst != nullptr) {
            orlst->clean();
        }
    }
};


//Mapping between register number and a list of OR.
class Reg2ORList : public UINT2ConstORList {
public:
    virtual ~Reg2ORList() {}

    //'u' corresponds to single 'or'.
    virtual void set(UINT u, OR const* o)
    {
        ASSERT0(u != 0 && o); //'0' is set as default nullptr
        ConstORList * orlst = get(u);
        if (orlst == nullptr) {
            UINT2ConstORList::set(u, o);
        } else {
            //In single ORBB, only record the immediate-dominate DEF or.
            ASSERT0(orlst->get_elem_count() <= 1);
            orlst->remove_head();
            orlst->append_tail(o);
        }
    }

    //'u' corresponds to multiple 'or'.
    void append(UINT u, OR const* o)
    {
        ASSERT0(u != 0 && o); //'0' is set as default nullptr
        ConstORList * orlst = get(u);
        if (orlst == nullptr) {
            UINT2ConstORList::set(u, o);
        } else {
            orlst->append_tail(o);
        }
    }
};


//Function Unit set
class UnitSet : public xcom::BitSet {
public:
    UnitSet() {}
    UnitSet(UnitSet const& src);

    //This function check that the unit set only contains single element.
    //Return the single element.
    UNIT checkAndGet() const;
};


typedef xcom::HMap<REG, SR*, HashFuncBase<REG> > Reg2SR;

} //namespace xgen
#endif
