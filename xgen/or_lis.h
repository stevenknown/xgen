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
#ifndef _OR_LIS_H_
#define _OR_LIS_H_

namespace xgen {

//Local Instruction Schedulor
#define SCH_UNDEF 0
#define SCH_TOP_DOWN 1 //Scheducling Top-down
#define SCH_BOTTOM_UP 2 //Scheducling bottom up
#define SCH_BRANCH_DELAY_SLOT 3 //Scheducling branch delay
class LIS {
protected:
    SMemPool * m_pool;
    UINT m_sch_mode;
    DataDepGraph * m_ddg;
    ORBB * m_bb;
    BBSimulator * m_sim;
    CG * m_cg;
    OR_HASH m_ready_list;
    bool m_is_change_slot; //permit func unit be changed.
    bool m_is_change_cluster; //permit cluster be changed.
    bool m_or_changed; //record if resource of or changed.
    ORDesc * m_br_ord; //description of branch operation.
    OR_HASH m_br_all_preds; //all of ors which are predecessors of branch-or.
    Vector<bool> const* m_is_regfile_unique;

protected:
    inline void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        ASSERTN(m_pool != 0,("need graph pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p,0,size);
        return p;
    }
    virtual OR * selectBestOR(OR_HASH & cand_hash, SLOT slot);
    virtual void updateIssueORs(IN OUT ORList & cand_list,
                                SLOT slot,
                                IN OR * issue_or,
                                IN OUT OR * issue_ors[SLOT_NUM]);
public:
    LIS(ORBB * bb,
        DataDepGraph & ddg,
        BBSimulator * sim,
        UINT sch_mode = SCH_TOP_DOWN,
        bool change_slot = true,
        bool change_cluster = false)
    {
        m_pool = NULL;
        init(bb, ddg, sim, sch_mode, change_slot, change_cluster);
    }
    virtual ~LIS() { destroy(); }

    //Scheduling method
    void computeReadyList(IN OUT DataDepGraph & ddg,
                          IN OUT Vector<bool> & visited,
                          bool topdown);

    //Change OR to given issue slot.
    //Return true if success.
    virtual bool changeSlot(OR *, SLOT to_slot)
    {
        ASSERTN(0, ("Target Dependent Code"));
        DUMMYUSE(to_slot);
        return false;
    }

    //Verficiation of instruction hazard, and change slot of o if possible.
    //Return true if 'o' can be issued at 'to_slot'.
    //The verification includes hardware resource, instrcution hazard, etc.
    bool makeIssued(OR * o,
                     OR * issue_ors[SLOT_NUM],
                     SLOT to_slot,
                     bool is_change_slot,
                     OR * conflict_ors[SLOT_NUM]);

    void destroy();
    virtual INT dcache_miss_rate(OR * o);
    virtual INT dcache_miss_penalty(OR * o);
    virtual void dump(CHAR * name,
                      bool is_del = true,
                      bool need_exec_detail = true);

    virtual bool fillIssueSlot(DataDepGraph & stepddg);

    //Return the Branch Operation.
    OR * get_br() const { return m_bb->getBranchOR(); }
    //Get simulated machine
    BBSimulator const* get_simm() const { return m_sim; }

    void init(ORBB * bb,
              DataDepGraph & ddg,
              BBSimulator * sim,
              UINT sch_mode,
              bool change_slot,
              bool change_cluster);

    //Return true if OR can be issued at given slot.
    virtual bool isValidResourceUsage(IN OR *,
                                      SLOT slot,
                                      OR * issue_ors[SLOT_NUM],
                                      OR * conflict_ors[SLOT_NUM])
    {
        ASSERTN(0, ("Target Dependent Code"));
        DUMMYUSE(slot);
        DUMMYUSE(issue_ors);
        DUMMYUSE(conflict_ors);
        return true;
    }

    virtual bool isIssueCand(OR * o);
    bool isFuncUnitOccupied(UnitSet const& us,
                            CLUST clst,
                            OR const* const issue_ors [SLOT_NUM]) const;

    virtual SLOT rollBackORs(bool be_changed[SLOT_NUM],
                             OR * issue_ors[SLOT_NUM],
                             SLOT to_slot);
    virtual bool selectIssueORs(IN ORList & cand_list,
                                OUT OR * issue_ors[SLOT_NUM]);
    virtual OR * selectBestORByPriority(OR_HASH & cand_hash);
    virtual bool selectIssueOR(IN ORList & cand_list,
                               SLOT slot,
                               OUT OR * issue_ors[SLOT_NUM],
                               bool change_slot);
    void set_simm(BBSimulator * sim) { m_sim = sim; }
    void set_sch_mode(UINT sch_mode) { m_sch_mode = sch_mode; }

    void set_unique_regfile(Vector<bool> const& is_regfile_unique)
    { m_is_regfile_unique = &is_regfile_unique; }

    void set_change_slot(bool is_change_slot)
    { m_is_change_slot = is_change_slot; }

    void set_change_cluster(bool is_change_cluster)
    { m_is_change_cluster = is_change_cluster; }

    virtual bool schedule();
    void serialize();
};

} //namespace xgen
#endif
