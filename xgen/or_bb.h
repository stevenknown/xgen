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
#ifndef _OR_BB_H_
#define _OR_BB_H_

namespace xgen {

#define ORBBID_UNDEF 0
#define OR_ORDER_INTERVAL 5

typedef xcom::List<ORBB*> ORBBList;
typedef xcom::C<ORBB*> * ORBBListIter;

//Mapping from OR to HOLDER
class OR2Holder {
public:
    OR2Holder(UINT bsize = MAX_SHASH_BUCKET) { init(bsize); }
    ~OR2Holder() { destroy(); }
    void init(UINT) {}
    void destroy() {}
    void setAlways(OR * o, ORCt * container);
    void set(OR * o, ORCt * container);
    ORCt * get(OR * o) const;
};


//
//START BBORList
//
typedef xcom::C<OR*> * BBORListIter;

class BBORList : public EList<OR*, OR2Holder> {
    ORBB * m_bb;
public:
    BBORList(ORBB * bb) { m_bb = bb; }
    virtual ~BBORList() {}

    //Insert OR prior to cond_br, uncond_br, call, return.
    void append_tail_ex(List<OR*> & list);

    //Insert OR prior to cond_br, uncond_br, call, return.
    xcom::C<OR*> * append_tail_ex(OR * o);
    ORCt * append_tail(OR * o);
    void append_tail(IN List<OR*> & list);
    ORCt * append_head(IN OR * o);
    void append_head(IN List<OR*> & list);

    ORCt * insert_before(IN OR * o, IN OR * marker);
    ORCt * insert_before(IN OR * o, IN ORCt * marker);
    void insert_before(IN ORCt * cor, IN ORCt * marker);
    void insert_before(IN List<OR*> & list, IN OR * marker);
    void insert_before(IN List<OR*> & list, IN ORCt * marker);
    ORCt * insert_after(IN OR * o, IN OR * marker);
    ORCt * insert_after(IN OR * o, IN ORCt * marker);
    void insert_after(IN List<OR*> & list, IN OR * marker);
    void insert_after(IN List<OR*> & list, IN ORCt * marker);
    void insert_after(IN ORCt * cor, IN ORCt * marker);
    virtual bool is_or_precedes(IN OR * or1, IN OR * or2);

    virtual OR * remove(OR * o);
    virtual OR * remove(ORCt * holder);
    virtual OR * remove_tail();
    virtual OR * remove_head();

    virtual OR * get_next() //update 'm_cur'
    { return List<OR*>::get_next(); }

    virtual OR * get_prev() //update 'm_cur'
    { return List<OR*>::get_prev(); }

    virtual OR * get_next(IN OUT ORCt ** holder) //NOT update 'm_cur'
    { return List<OR*>::get_next(holder); }

    virtual ORCt * get_next(IN OUT ORCt * holder) //NOT update 'm_cur'
    { return List<OR*>::get_next(holder); }

    virtual OR * get_prev(IN OUT ORCt ** holder) //NOT update 'm_cur'
    { return List<OR*>::get_prev(holder); }

    virtual ORCt * get_prev(IN OUT ORCt * holder) //NOT update 'm_cur'
    { return List<OR*>::get_prev(holder); }

    virtual OR * get_next(IN OR * marker);
    virtual OR * get_prev(IN OR * marker);
    void dump(CG * cg);
};


//
//START ORBB
//
#define ORBB_livein(b) ((b)->m_live_in)
#define ORBB_liveout(b) ((b)->m_live_out)
#define ORBB_id(b) ((b)->m_id)
#define ORBB_orlist(b) ((b)->m_or_list)
#define ORBB_ornum(b) (ORBB_orlist(b)->get_elem_count())
#define ORBB_first_or(b) (ORBB_orlist(b)->get_head())
#define ORBB_next_or(b) (ORBB_orlist(b)->get_next())
#define ORBB_prev_or(b) (ORBB_orlist(b)->get_prev())
#define ORBB_last_or(b) (ORBB_orlist(b)->get_tail())
#define ORBB_is_entry(b) ((b)->m_is_entry)
#define ORBB_is_exit(b) ((b)->m_is_exit)
#define ORBB_is_target(b) ((b)->m_is_target)
#define ORBB_attr(b) ((b)->m_attr)
#define ORBB_rpo(b) ((b)->m_rpo)
#define ORBB_cg(b) ((b)->m_cg)
#define ORBB_entry_spadjust(b) ((b)->m_entry_spadjust)
#define ORBB_exit_spadjust(b) ((b)->m_exit_spadjust)
class ORBB {
    COPY_CONSTRUCTOR(ORBB);
public:
    UINT m_id:29; //BB's id
    UINT m_is_exit:1; //bb is a region exit.
    UINT m_is_entry:1; //bb a region entry.
    UINT m_is_target:1; //bb is the jumping target.
    INT m_rpo;    
    BBORList * m_or_list; //OR list
    CG * m_cg;
    OR * m_entry_spadjust; //record sp-adjust OR if bb is entry bb.
    OR * m_exit_spadjust; //record sp-adjust OR if bb is exit bb.
    ULONG m_attr; //BB attributes
    xcom::BitSet m_live_in; //record live in SR id.
    xcom::BitSet m_live_out; //record live out SR id.
    List<LabelInfo const*> m_lab_list; //Record all of labels attached on BB

public:
    explicit ORBB(CG * cg);
    ~ORBB();

    inline void addLabel(LabelInfo const* li)
    {
        ASSERT0(li != nullptr);
        if (!m_lab_list.find(li)) {
            m_lab_list.append_tail(li);
        }
    }

    void cleanLabelInfoList() { getLabelList().clean(); }

    void dump();

    OR * getBranchOR();
    List<LabelInfo const*> & getLabelList() { return m_lab_list; }
    BBORList * getORList() const { return ORBB_orlist(this); }
    UINT getORNum() const { return ORBB_orlist(this)->get_elem_count(); }
    OR * getFirstOR() const { return ORBB_first_or(this); }
    OR * getNextOR() const { return ORBB_next_or(this); }
    OR * getPrevOR() const { return ORBB_prev_or(this); }
    OR * getLastOR() const { return ORBB_last_or(this); }

    UINT id() const { return ORBB_id(this); }
    bool isLowerBoundary(OR * o);
    bool is_bb_exit() const { return ORBB_is_exit(this); }
    bool is_entry() const { return ORBB_is_entry(this); }
    bool is_exit() const { return ORBB_is_exit(this); }
    bool is_target() const { return ORBB_is_target(this); }
    bool hasLabel(LabelInfo const* o);
    bool isBoundary(OR * o)
    { return (isUpperBoundary(o) || isLowerBoundary(o)); }
    bool isUpperBoundary(OR * o) { return o->is_label_or(); }
    bool hasCall();
    bool isLiveIn(SR * sr);
    bool isLiveOut(SR * sr);
    bool isLiveOutResult(OR * o);
    inline bool isExceptionHandler() const
    {
        ORBB * pthis = const_cast<ORBB*>(this);
        for (LabelInfo const* li = pthis->getLabelList().get_head();
             li != nullptr; li = pthis->getLabelList().get_next()) {
            if (LABELINFO_is_catch_start(li)) {
                return true;
            }
        }
        return false;
    }
    inline bool is_terminate() const
    {
        ORBB * pthis = const_cast<ORBB*>(this);
        for (LabelInfo const* li = pthis->getLabelList().get_head();
             li != nullptr; li = pthis->getLabelList().get_next()) {
            if (LABELINFO_is_terminate(li)) {
                return true;
            }
        }
        return false;
    }

    void mergeLabeInfoList(ORBB * from);

    //Return true if one of bb's successor has a phi.
    bool successorHasPhi(CFG<ORBB, OR> * cfg) { DUMMYUSE(cfg); return false; }
    void setLiveOut(SR * sr);
    void setLiveIn(SR * sr);

    INT rpo() const { return ORBB_rpo(this); }

    //Before removing bb, revising phi opnd if there are phis
    //in one of bb's successors.
    void removeAllSuccessorsPhiOpnd(CFG<ORBB, OR> * cfg) { DUMMYUSE(cfg); }    
};
//END ORBB


class ORBBMgr {
    List<ORBB*> m_bbs_list;
public:
    ~ORBBMgr()
    {
        for (ORBB * bb = m_bbs_list.get_head();
             bb != nullptr; bb = m_bbs_list.get_next()) {
            delete bb;
        }
    }

    ORBB * allocBB(CG * cg)
    {
        ORBB * bb = new ORBB(cg);
        m_bbs_list.append_tail(bb);
        return bb;
    }
};


//Exported Functions
extern void dumpORBBList(List<ORBB*> & bbl, CG * cg);

} //namespace xgen
#endif
