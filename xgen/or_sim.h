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
#ifndef _OR_SIM_H_
#define _OR_SIM_H_

namespace xgen {

#define ORDESC_or(ord) ((ord)->o)
#define ORDESC_start_cyc(ord) ((ord)->start_cyc)
#define ORDESC_sche_info(ord) ((ord)->or_info)
#define ORDESC_next(ord) ((ord)->next)
#define ORDESC_prev(ord) ((ord)->prev)
class ORDesc {
public:
    ORDesc * next;
    ORDesc * prev;
    OR * o;
    INT start_cyc; //cycles which start executing.
    ORScheInfo const* or_info;

public:
    INT getStartCycle() const { return ORDESC_start_cyc(this); }
    ORScheInfo const* getScheInfo() const { return ORDESC_sche_info(this); }
    //The OR may be changed.
    OR * getOR() const { return ORDESC_or(this); }
};


//Simulator for BB.
class BBSimulator {
protected:
    INT m_cyc_counter;
    ORBB * m_bb;
    xoc::Region * m_rg;
    CG * m_cg;
    List<ORDesc*> m_slot_lst[SLOT_NUM]; //Record ors who are executing.
    ORVec m_exec_tab[SLOT_NUM]; //Record total ors issued.
    SMemPool * m_pool;
    xcom::FreeList<ORDesc> m_free_list; //Hold for availablze containers

protected:
    ORDesc * allocORDesc();

    INT computeDependentResultIndex(OR const* def, OR const* use) const;

    //Allocate ordesc container
    void freeordesc(ORDesc * ord)
    {
        ASSERTN(m_pool, ("not yet initialized."));
        ORDESC_next(ord) = ORDESC_prev(ord) = nullptr;
        m_free_list.add_free_elem(ord);
    }

    void * xmalloc(INT size)
    {
        ASSERTN(m_pool, ("not yet initialized."));
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }
public:
    BBSimulator(ORBB * bb);
    virtual ~BBSimulator() { destroy(); }

    bool canBeIssued(OR const* o, SLOT slot, DataDepGraph const& ddg) const;
    //Count memory usage for current object.
    size_t count_mem() const
    {
        size_t size = sizeof(BBSimulator) + smpoolGetPoolSize(m_pool) +
                      m_free_list.count_mem();
        for (UINT i = 0; i < SLOT_NUM; i++) {
            size += m_slot_lst[i].count_mem();
            size += m_exec_tab[i].count_mem();
        }
        return size;
    }

    void destroy();
    bool done();
    void dump(FILE * h, bool dump_exec_detail) const;
    void dump(CHAR * name = nullptr,
              bool is_del = true,
              bool dump_exec_detail = true) const;

    //Return the simmulation of OR execution.
    ORVec const* getExecSnapshot() const { return m_exec_tab; }
    virtual void getOccupiedSlot(OR const* o, OUT bool occ_slot[SLOT_NUM]);
    //Return the last cycle that simm executed.
    UINT getCurCycle() const { return m_cyc_counter; }

    void init();
    virtual bool isMemResourceConflict(DEP_TYPE deptype,
                                       ORDesc const* ck_ord,
                                       OR const* cand_or) const;
    virtual bool isRegResourceConflict(DEP_TYPE deptype,
                                       ORDesc const* ck_ord,
                                       OR const* cand_or) const;
    virtual bool isResourceConflict(ORDesc const* ck_ord,
                                    OR const* cand_or,
                                    DataDepGraph const& ddg) const;
    virtual bool isInShadow(ORDesc const* ord) const;
    virtual bool issue(OR * o, SLOT slot);

    virtual UINT getShadow(OR const* o) const;
    virtual UINT getExecCycle(OR const* o) const;
    virtual UINT getMinLatency(OR const* o) const;

    virtual UINT numOfMemResult(OR const*) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        return 0;
    }

    virtual void runOneCycle(IN OUT ORList * finished_ors);
    void reset();

    void setCurCycle(UINT cur_cyc) { m_cyc_counter = cur_cyc; }
};

} //namespace xgen
#endif

