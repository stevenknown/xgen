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

//Cycle-Accurate Local Instruction Schedulor
class LIS {
    COPY_CONSTRUCTOR(LIS);
public:
    //Defined the direction of scheduling and some target dependent behaviors.
    enum SchedMode {
        SCH_UNDEF = 0x0, //Undefined scheduling behavior
        SCH_TOP_DOWN = 0x1, //Scheducling Top-down
        SCH_BOTTOM_UP = 0x2, //Scheducling bottom up
        SCH_BRANCH_DELAY_SLOT = 0x4, //Scheducling branch delay
        SCH_CHANGE_SLOT = 0x8, //Allow schedulor changes Issue Slot/FunctionUnit
                               //of OR if needed during scheduling
        SCH_CHANGE_CLUSTER = 0x10, //Allow schedulor changes cluster if needed
                                   //during scheduling.

        //Allow rescheduling if needed. For now, if the strategy is scheduling
        //delay-slot, rescheduling is necessary because the default
        //scheduling-direction is top-down, the BR will be issued as soon as
        //possible which may be illegal, it need rescheduling to fixup.
        SCH_ALLOW_RESCHED = 0x20, 
    };

protected:
    UINT m_sch_mode:31;
    UINT m_or_changed:1;
    SMemPool * m_pool;
    DataDepGraph * m_ddg;
    ORBB * m_bb;
    BBSimulator * m_sim;
    CG * m_cg;
    RegFileSet const* m_is_regfile_unique;
    ORDesc * m_br_ord; //description of branch operation.
    ORTab m_ready_list;
    ORTab m_br_all_preds; //all of ors which are predecessors of branch-or.
    PreemptiveORList m_tmp_orlist; //used as tmp local variable.
protected:
    bool isScheduleDelaySlot() const { return m_br_ord != nullptr; }
    bool isBranch(OR const* o) const
    {
        ASSERT0(isScheduleDelaySlot());
        return o == ORDESC_or(m_br_ord);
    }

    inline void * xmalloc(INT size)
    {
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        ASSERTN(m_pool != 0,("need graph pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p,0,size);
        return p;
    }
    virtual OR * selectBestOR(ORTab & cand_tab, SLOT slot);
    virtual void updateIssueORs(IN OUT PreemptiveORList & cand_list,
                                SLOT slot,
                                IN OR * issue_or,
                                IN OUT OR * issue_ors[SLOT_NUM]);

    //Add more ready-OR into ready-list when removing 'or'.
    //or: OR will be removed.
    //ddg: dependent graph, it will be updated before function return.
    //visited: bitset that used to update ready-or-list. It can be
    //nullptr if you are not going to update ready-list.
    void updateReadyList(OR const* o, DataDepGraph const& ddg,
                         DefSBitSet * visited);
public:
    LIS(ORBB * bb, DataDepGraph & ddg, BBSimulator * sim,
        UINT sch_mode = SCH_TOP_DOWN | SCH_CHANGE_SLOT)
    {
        m_pool = nullptr;
        init(bb, ddg, sim, sch_mode);
    }
    virtual ~LIS() { destroy(); }

    //Return true if allowing rescheduling ORs when sch-mode is SCH_DEALY_SLOT.
    //Note rescheduling delay-slot may getting more speedup of performance,
    //but it will increase compilation time obviously also.
    virtual bool allowReschedule() const
    { return HAVE_FLAG(m_sch_mode, SCH_ALLOW_RESCHED); }
    bool allowChangeSlot() const
    { return HAVE_FLAG(m_sch_mode, SCH_CHANGE_SLOT); }
    bool allowChangeCluster() const
    { return HAVE_FLAG(m_sch_mode, SCH_CHANGE_CLUSTER); }

    //Scheduling method
    void computeReadyList(IN OUT DataDepGraph & ddg,
                          IN OUT DefSBitSet & visited,
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
    virtual INT dcache_miss_rate(OR const* o) const;
    virtual INT dcache_miss_penalty(OR const* o) const;
    virtual void dump(bool need_exec_detail = true) const;

    //Choose candidate OR from ready-list, and fill available issue slot.
    //Return true if there are ORs issued, and ready-list should
    //be recomputed.
    //stepddg: computing ready instructions step by step during scheduling.
    //visited: bitset that used to update ready-or-list. It can be nullptr if you
    //         are not going to update ready-list.
    //
    //About fill branch shadow:
    //    When GIS is calling it, don't try to schedule the operation
    //    that has moved from a different block that do not
    //    dominate/post-dominate current BB in the delay slot.
    //    Frequent observance is that this will unneccessarily
    //    restrict further code motion.
    virtual bool fillIssueSlot(DataDepGraph * stepddg, DefSBitSet * visited);

    //Return the Branch Operation.
    OR * get_br() const { return m_bb->getBranchOR(); }
    ORBB * getBB() const { return m_bb; }
    //Get simulated machine
    BBSimulator const* get_simm() const { return m_sim; }

    //Return an experimental integer that describe the maximum times to
    //rescheduling ORs by adjusting issue time of branch operation. 
    //Note this is sometime a tricky value, range between [0~N].
    virtual UINT getAddendOfMaxTryTime() const { return 0; }

    void init(ORBB * bb, DataDepGraph & ddg, BBSimulator * sim, UINT sch_mode);
    //Return true if OR can be issued at given slot.
    virtual bool isValidResourceUsage(OR const*,
                                      SLOT slot,
                                      OR * issue_ors[SLOT_NUM],
                                      OR * conflict_ors[SLOT_NUM]) const
    {
        ASSERTN(0, ("Target Dependent Code"));
        DUMMYUSE(slot);
        DUMMYUSE(issue_ors);
        DUMMYUSE(conflict_ors);
        return true;
    }

    bool isIssueCand(OR const* o) const;
    bool isFuncUnitOccupied(UnitSet const& us,
                            CLUST clst,
                            OR const* const issue_ors [SLOT_NUM]) const;

    virtual SLOT rollBackORs(bool be_changed[SLOT_NUM],
                             OR * issue_ors[SLOT_NUM],
                             SLOT to_slot);
    virtual bool selectIssueORs(IN PreemptiveORList & cand_list,
                                OUT OR * issue_ors[SLOT_NUM]);
    virtual OR * selectBestORByPriority(ORTab const& cand_hash) const;

    //Find valid OR that can be issued at slot.
    //Return true if avaiable issue ors found.
    //'cand_list': list of candidate operations which
    //             can be issue at this cycle. Element in list may be changed.
    //'slot': slot need to fill
    //'issue_or': record issued ors selected.
    //'change_slot': set to true indicate the routine allows modification
    //  of operations in other slot. Note that the modification may
    //  change the function unit of operation.
    //Note this functio will attempt to change OR's slot if possible.
    virtual bool selectIssueOR(OUT PreemptiveORList & cand_list,
                               SLOT slot,
                               OUT OR * issue_ors[SLOT_NUM],
                               bool change_slot);
    //Set simulator.
    void set_simm(BBSimulator * sim) { m_sim = sim; }
    //Set scheduling mode.
    void set_sch_mode(UINT sch_mode) { m_sch_mode = sch_mode; }
    //Set unique register file information.
    void set_unique_regfile(RegFileSet const* is_regfile_unique)
    { m_is_regfile_unique = is_regfile_unique; }
    virtual bool schedule();
    void serialize();
};

} //namespace xgen
#endif
