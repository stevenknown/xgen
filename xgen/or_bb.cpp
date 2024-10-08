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
#include "xgeninc.h"

namespace xgen {

//
//START OR2Holder
//
//Alway set new mapping even if it has done.
void OR2Holder::setAlways(OR * o, ORCt * container)
{
    OR_ct(o) = container;
}


//Establishing mapping in between 't' and 'mapped'.
void OR2Holder::set(OR * o, ORCt * container)
{
    OR_ct(o) = container;
}


ORCt * OR2Holder::get(OR * o) const
{
    return OR_ct(o);
}
//END OR2Holder


//
//START BBORList
//
ORCt * BBORList::append_tail(OR * o)
{
    ASSERT0(o);
    if (get_elem_count() == 0) {
        OR_order(o) = 0;
    } else {
        OR_order(o) = OR_order(m_tail->val()) + OR_ORDER_INTERVAL;
    }
    OR_bb(o) = m_bb;
    return xcom::EList<OR*, OR2Holder>::append_tail(o);
}


void BBORList::append_tail(List<OR*> & list)
{
    INT i = 0;
    if (get_elem_count() != 0) {
        i = OR_order(m_tail->val()) + OR_ORDER_INTERVAL;
    }

    for (OR * o = list.get_head(); o;
         o = list.get_next(), i += OR_ORDER_INTERVAL) {
        OR_order(o) = i;
        OR_bb(o) = m_bb;
    }

    xcom::EList<OR*, OR2Holder>::append_tail(list);
}


//Insert OR prior to cond_br, uncond_br, call, return.
void BBORList::append_tail_ex(List<OR*> & list)
{
    xcom::C<OR*> * ct;
    for (OR * tor = List<OR*>::get_tail(&ct);
         ct != List<OR*>::end(); tor = List<OR*>::get_prev(&ct)) {
        if (!tor->isUnconditionalBr() &&
            !tor->isConditionalBr() &&
            !tor->isMultiConditionalBr() &&
            !tor->is_return() &&
            !tor->is_call()) {
            break;
        }
    }

    if (ct == nullptr) {
        //BB is empty.
        append_tail(list);
    }

    insert_after(list, ct);
}


//Insert OR prior to cond_br, uncond_br, call, return.
C<OR*> * BBORList::append_tail_ex(OR * o)
{
    ASSERT0(o);
    xcom::C<OR*> * ct;
    for (OR * tor = List<OR*>::get_tail(&ct);
         ct != List<OR*>::end(); tor = List<OR*>::get_prev(&ct)) {
        if (!tor->isUnconditionalBr() &&
            !tor->isConditionalBr() &&
            !tor->isMultiConditionalBr() &&
            !tor->is_return() &&
            !tor->is_call()) {
            break;
        }
    }

    if (ct == nullptr) {
        //BB is empty.
        return append_tail(o);
    }
    return insert_after(o, ct);
}


ORCt * BBORList::append_head(OR * o)
{
    if (get_elem_count() == 0) {
        OR_order(o) = 0;
    } else {
        OR_order(o) = OR_order(m_head->val()) - OR_ORDER_INTERVAL;
    }

    OR_bb(o) = m_bb;
    return xcom::EList<OR*, OR2Holder>::append_head(o);
}


void BBORList::append_head(List<OR*> & list)
{
    INT i = 0;
    if (get_elem_count() != 0) {
        i = OR_order(m_head->val()) - OR_ORDER_INTERVAL;
    }

    for (OR * o = list.get_tail();
         o != nullptr; o = list.get_prev(), i -= OR_ORDER_INTERVAL) {
        OR_order(o) = i;
        OR_bb(o) = m_bb;
    }

    xcom::EList<OR*, OR2Holder>::append_head(list);
}


//Insert o before marker.
ORCt * BBORList::insert_before(OR * o, OR * marker)
{
    if (marker == m_head->val()) {
        OR_order(o) = OR_order(marker) - OR_ORDER_INTERVAL;
    } else {
        ORCt * cm = nullptr;
        bool b = xcom::EList<OR*, OR2Holder>::find(marker, &cm);
        ASSERT0_DUMMYUSE(b);
        ASSERT0(cm && C_prev(cm));

        OR * prev = C_prev(cm)->val();
        if ((OR_order(marker) - OR_order(prev)) >= 2 ) {
            OR_order(o) = OR_order(marker) - 1;
        } else {
            ASSERT0((OR_order(marker) - OR_order(prev)) == 1);
            OR_order(o) = OR_order(prev) + OR_ORDER_INTERVAL;
            INT i = OR_order(o) + OR_ORDER_INTERVAL;
            ORCt * c = cm;
            while (c) {
                OR_order(c->val()) = i;
                i += OR_ORDER_INTERVAL;
                c = C_next(c);
            }
        }
    }

    OR_bb(o) = m_bb;
    return xcom::EList<OR*, OR2Holder>::insert_before(o, marker);
}


//Insert cor before marker.
ORCt * BBORList::insert_before(OR * o, ORCt * marker)
{
    ORCt * c = newc();
    ASSERTN(c, ("newc return nullptr"));
    C_val(c) = o;
    insert_before(c, marker);
    return c;
}


//Insert cor before marker.
void BBORList::insert_before(ORCt * cor, ORCt * marker)
{
    OR * marker_or = marker->val();
    OR * o = cor->val();
    if (marker == m_head) {
        OR_order(o) = OR_order(marker_or) - OR_ORDER_INTERVAL;
    } else {
        OR * prev = C_prev(marker)->val();
        if ((OR_order(marker_or) - OR_order(prev)) >= 2 ) {
            OR_order(o) = OR_order(marker_or) - 1;
        } else {
            ASSERT0((OR_order(marker_or) - OR_order(prev)) == 1);
            OR_order(o) = OR_order(prev) + OR_ORDER_INTERVAL;
            INT i = OR_order(o) + OR_ORDER_INTERVAL;
            ORCt * c = marker;
            while (c) {
                OR_order(c->val()) = i;
                i += OR_ORDER_INTERVAL;
                c = C_next(c);
            }
        }
    }

    OR_bb(o) = m_bb;
    xcom::EList<OR*, OR2Holder>::insert_before(cor, marker);
}


void BBORList::insert_after(IN List<OR*> & list, IN ORCt * marker)
{
    for (OR * o = list.get_head(); o != nullptr; o = list.get_next()) {
        OR_bb(o) = m_bb;
        marker = insert_after(o, marker);
    }
}


void BBORList::insert_after(IN List<OR*> & list, IN OR * marker)
{
    for (OR * o = list.get_head(); o != nullptr; o = list.get_next()) {
        OR_bb(o) = m_bb;
        insert_after(o, marker);
        marker = o;
    }
}


void BBORList::insert_before(IN List<OR*> & list, IN OR * marker)
{
    for (OR * o = list.get_tail(); o != nullptr; o = list.get_prev()) {
        OR_bb(o) = m_bb;
        insert_before(o, marker);
        marker = o;
    }
}


void BBORList::insert_before(IN List<OR*> & list, IN ORCt * marker)
{
    for (OR * o = list.get_tail(); o != nullptr; o = list.get_prev()) {
        OR_bb(o) = m_bb;
        marker = insert_before(o, marker);
    }
}


//Insert 'o' after 'marker', and update OR_order for ORs in bb.
ORCt * BBORList::insert_after(OR * o, OR * marker)
{
    if (marker == m_tail->val()) {
        OR_order(o) = OR_order(marker) + OR_ORDER_INTERVAL;
    } else {
        ORCt * cm = nullptr;
        bool b = xcom::EList<OR*, OR2Holder>::find(marker, &cm);
        ASSERT0_DUMMYUSE(b);
        ASSERT0(cm && C_next(cm));

        OR * next = C_next(cm)->val();
        if ((OR_order(next) - OR_order(marker)) >= 2 ) {
            OR_order(o) = OR_order(next) - 1;
        } else {
            ASSERT0((OR_order(next) - OR_order(marker)) == 1);
            OR_order(o) = OR_order(marker) + OR_ORDER_INTERVAL;
            INT i = OR_order(o) + OR_ORDER_INTERVAL;
            ORCt * c = C_next(cm);
            while (c != nullptr) {
                OR_order(c->val()) = i;
                i += OR_ORDER_INTERVAL;
                c = C_next(c);
            }
        }
    }
    OR_bb(o) = m_bb;
    return xcom::EList<OR*, OR2Holder>::insert_after(o, marker);
}


//Insert cor before marker.
ORCt * BBORList::insert_after(OR * o, ORCt * marker)
{
    ORCt * c = newc();
    ASSERTN(c, ("newc return nullptr"));
    C_val(c) = o;
    insert_after(c, marker);
    return c;
}


OR * BBORList::remove(OR * o)
{
    OR_bb(o) = nullptr;
    return xcom::EList<OR*, OR2Holder>::remove(o);
}


OR * BBORList::remove(ORCt * holder)
{
    OR * o = xcom::EList<OR*, OR2Holder>::remove(holder);
    OR_bb(o) = nullptr;
    return o;
}


OR * BBORList::remove_tail()
{
    OR * o = xcom::EList<OR*, OR2Holder>::remove_tail();
    if (o != nullptr) {
        OR_bb(o) = nullptr;
    }
    return o;
}


OR * BBORList::remove_head()
{
    OR * o = xcom::EList<OR*, OR2Holder>::remove_head();
    if (o != nullptr) {
        OR_bb(o) = nullptr;
    }
    return o;
}


//Insert cor after marker.
void BBORList::insert_after(ORCt * cor, ORCt * marker)
{
    OR * marker_or = marker->val();
    OR * o = cor->val();
    if (marker == m_tail) {
        OR_order(o) = OR_order(marker_or) + OR_ORDER_INTERVAL;
    } else {
        OR * next = C_next(marker)->val();
        if ((OR_order(next) - OR_order(marker_or)) >= 2 ) {
            OR_order(o) = OR_order(next) - 1;
        } else {
            ASSERT0((OR_order(next) - OR_order(marker_or)) == 1);
            OR_order(o) = OR_order(marker_or) + OR_ORDER_INTERVAL;
            INT i = OR_order(o) + OR_ORDER_INTERVAL;
            ORCt * c = C_next(marker);
            while (c) {
                OR_order(c->val()) = i;
                i += OR_ORDER_INTERVAL;
                c = C_next(c);
            }
        }
    }
    OR_bb(o) = m_bb;
    xcom::EList<OR*, OR2Holder>::insert_after(cor, marker);
}


bool BBORList::is_or_precedes(OR * o1, OR * o2)
{
    ASSERT0(OR_order(o1) != OR_order(o2));
    if (OR_order(o1) < OR_order(o2)) {
        return true;
    }
    return false;
}


//Not update m_cur.
OR * BBORList::get_next(OR * marker)
{
    ORCt * holder = OR_ct(marker);
    ASSERT0(holder);
    return List<OR*>::get_next(&holder);
}


//Not update m_cur.
OR * BBORList::get_prev(OR * marker)
{
    ORCt * holder = OR_ct(marker);
    ASSERT0(holder);
    return List<OR*>::get_prev(&holder);
}


void BBORList::dump(CG * cg)
{
    if (!cg->getRegion()->isLogMgrInit()) { return; }
    xcom::StrBuf buf(64);
    ORCt * cm = nullptr;
    for (OR * o = get_head(); o != nullptr; o = get_next()) {
        ASSERT0(o->getBB() == m_bb);
        bool b = xcom::EList<OR*, OR2Holder>::find(o, &cm);
        ASSERT0_DUMMYUSE(b);
        ASSERTN(cm, ("cannot find OR in list"));

        buf.clean();
        o->dump(buf, cg);
        note(cg->getRegion(), "\n%s", buf.buf);
    }
}
//END BBORList


//
//START ORBB
//
ORBB::ORBB(CG * cg)
{
    ASSERT0(cg);
    m_id = ORBBID_UNDEF;
    m_attr = 0;
    m_cg = cg;
    m_or_list = new BBORList(this); //OR list
    m_is_exit = m_is_entry = m_is_target = false;
    ORBB_entry_spadjust(this) = nullptr;
    ORBB_exit_spadjust(this) = nullptr;
    ORBB_vex(this) = nullptr;
}


ORBB::~ORBB()
{
    delete m_or_list;
    m_or_list = nullptr;
}


//For some aggressive optimized purposes, call node is not looked as
//boundary of basic block.
//So we must bottom-up go through whole bb to find call.
bool ORBB::hasCall()
{
    for (OR * o = ORBB_orlist(this)->get_head(); o;
        o = ORBB_orlist(this)->get_next()) {
        if (OR_is_call(o)) {
            return true;
        }
    }
    return false;
}


//Is bb containing such label carried by 'lir'.
bool ORBB::hasLabel(LabelInfo const* lab) const
{
    for (LabelInfo const* li = const_cast<ORBB*>(this)->getLabelList().
            get_head();
         li != nullptr;
         li = const_cast<ORBB*>(this)->getLabelList().get_next()) {
        if (isSameLabel(li, lab)) {
            return true;
        }
    }
    return false;
}


void ORBB::setLiveOut(SR * sr)
{
    ASSERT0(sr->is_reg());
    SR_is_global(sr) = 1;
    ORBB_liveout(this).bunion(SR_sym_reg(sr));
}


void ORBB::setLiveIn(SR * sr)
{
    ASSERT0(sr->is_reg());
    SR_is_global(sr) = 1;
    ORBB_livein(this).bunion(SR_sym_reg(sr));
}


bool ORBB::isLiveIn(SR * sr)
{
    ASSERT0(sr->is_reg());
    if (sr->is_global() &&
        (ORBB_livein(this).is_contain(SR_sym_reg(sr)))) {
        return true;
    }
    return false;
}


bool ORBB::isLiveOut(SR * sr)
{
    if (sr->is_global() &&
        (ORBB_liveout(this).is_contain(SR_sym_reg(sr)))) {
        return true;
    }
    return false;
}


//OR has live out sr(global sr).
//We can get the info from LifeTimeMgr, but
//corresponding life-time of the o's results need
//to be processed respectively, that is costly operation.
bool ORBB::isLiveOutResult(OR * o)
{
    for (UINT i = 0; i < o->result_num(); i++) {
        if (isLiveOut(o->get_result(i))) {
            return true;
        }
    }
    return false;
}


//The last OR at ORBB may not be branch if current
//pass pay attention to delay slot.
OR * ORBB::getBranchOR()
{
    for (OR * o = ORBB_orlist(this)->get_tail();
         o != nullptr; o = ORBB_orlist(this)->get_prev()) {
        if (OR_is_br(o)) {
            return o;
        }
    }
    return nullptr;
}


void ORBB::mergeLabeInfoList(ORBB * from)
{
    //Note src's labellist will be moved to current BB.
    getLabelList().move_head(from->getLabelList());
}


void ORBB::dump()
{
    note(m_cg->getRegion(), "\n------ BB%d ------", ORBB_id(this));

    //Label Info list
    LabelInfo const* li = getLabelList().get_head();
    if (li != nullptr) {
        note(m_cg->getRegion(), "\nLABEL:");
    }

    StrBuf buf(32);
    for (; li != nullptr; li = getLabelList().get_next()) {
        note(m_cg->getRegion(), "%s ", m_cg->formatLabelName(li, buf));
    }

    //Attributes
    note(m_cg->getRegion(), "\nATTR:");
    if (ORBB_is_exit(this)) {
        note(m_cg->getRegion(), "exit_bb ");
    }

    //OR list
    note(m_cg->getRegion(), "\nOR NUM:%d\n", ORBB_ornum(this));
    m_cg->getRegion()->getLogMgr()->incIndent(3);
    ORBB_orlist(this)->dump(m_cg);
    m_cg->getRegion()->getLogMgr()->decIndent(3);
    note(m_cg->getRegion(), "\n");
}


bool ORBB::isLowerBoundary(OR * o)
{
    return (OR_is_call(o) ||
            OR_is_cond_br(o) ||
            OR_is_uncond_br(o) ||
            OR_is_return(o));
}
//END ORBB


void dumpORBBList(List<ORBB*> & bbl, CG * cg)
{
    if (!cg->getRegion()->isLogMgrInit()) { return; }
    note(cg->getRegion(), "\n==---- DUMP ORBB List ----==");
    if (cg->getRegion()->getDbxMgr() != nullptr) {
        cg->getRegion()->getDbxMgr()->doPrepareWorkBeforePrint();
    }
    for (ORBB * bb = bbl.get_head(); bb != nullptr; bb = bbl.get_next()) {
        bb->dump();
    }
}

} //namespace xgen
