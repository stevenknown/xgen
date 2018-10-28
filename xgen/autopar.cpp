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

#define TRIP_COUNT_ALLOW_FULLY_UNROLL_BB 3

#if 0
xoc::LoopInfo * getLoopInfo(ORBB const* bb)
{
    return m_cg->mapBB2LoopInfo(bb);
}


SR * getTripCountSR(ORBB const* bb)
{
    xoc::LoopInfo * loop_info = getLoopInfo(bb);
    ASSERT0(loop_info);
    return LI_trip_count_sr(loop_info);
}


static bool FindMainIV(ORBB * bb, DataDepGraph & ddg,
                       OR ** red_or, OR ** cmp_or, SR ** iv)
{
    //If BB last OR is not branch operation, instruction scheduling
    //might have been performed.
    OR * branch = bb->getBranchOR();
    ASSERTN(branch, ("ORBB is not in loop end"));
    SR * predict_sr = branch->get_pred();
    ASSERTN(predict_sr != m_cg->genTruePred(), ("illegal control flow"));
    List<OR*> preds;

    //Find compare operation.
    OR * cmp;
    ddg.getPredsByOrder(preds, branch);
    for (cmp = preds.get_tail(); cmp; cmp = preds.get_prev()) {
        if (m_cg->isCompareOR(cmp) &&
            ORBB_cg(m_bb)->mustDef(cmp, predict_sr) &&
            !m_cg->isCondExecOR(cmp)) {
            break;
        }
    }
    ASSERTN(cmp, ("Not innermost loop"));
    *cmp_or = cmp;

    //'cmp_or' may not refer 'trip_count_sr' alwayys.
    //CASE:
    //trip_count_sr is GSR258
    //cmp_or is: [6:16]
    //[   4:  1]| SR294 , :- copy_i SR97(p0)[P1] GSR231
    //...
    //[   4: 12]| GSR258 , :- copy_i SR97(p0)[P1] SR294
    //...
    //[   6: 16]| SR270 , SR268 , SR269 , :- seq_m SR97(p0)[P1] GSR247 GSR231
    //ASSERTN(TRUE == mustReferSR(*cmp_or, trip_count_sr),
    //       ("Not innermost loop"));

    //Find reduce operation, such as : a = a + N
    preds.clean();
    ddg.getPredsByOrder(preds, *cmp_or);
    for (*red_or = preds.get_tail(); *red_or; *red_or = preds.get_prev()) {
        //Find the DEF o of induction variable.
        //Induction variable should be global register.
        bool find = false;
        for (INT i = 0; i < (*cmp_or)->opnd_num(); i++) {
            SR * opnd = (*cmp_or)->get_opnd(i);
            if (!m_cg->isIntRegSR(o, opnd, i, false) ||
                opnd == m_cg->genTruePred() ||
                !SR_is_global(opnd)) {
                continue;
            }
            if (ORBB_cg(m_bb)->mustDef(*red_or, opnd) &&
                ORBB_cg(m_bb)->mustUse(*red_or, opnd) &&
                !m_cg->isCondExecOR(*red_or)) {
                find = true;
                *iv = opnd;
                break;
            }
        }
        if (find) {
            break;
        }
    }
    return true;
}


//Generate code for low bound of IV, and target label for BB3
//'n': number of parallel part
//'iv': induction variable
//'remaing_count': rem_count_sr = trip_count % num_para_part + lb_sr
//'lb_sr': low bound of induction variable 'iv'
//'red_or': reduction o of 'iv'
static void fillBB1(OUT ORBB * bb1, //early exit from loop
                    ORBB const* orig_bb,
                    IN OUT ORBB * bb3, //early exit from loop
                    UINT num_para_part,
                    IN SR * iv,
                    OUT SR ** remaing_count,
                    IN SR * trip_count_sr,
                    IN SR * lb_sr,
                    IN OR * red_or)
{
    ORList ors;
    //Generate count sr for remainder loop
    UINT branch_cond = 0;
    if (SR_is_const(trip_count_sr)) {
        //new_upperb = lowb + trip_count % num_para_part
        INT new_trip_count_val = SR_int_imm(trip_count_sr) % num_para_part;
        ASSERTN(new_trip_count_val > 0,
               ("unroll_make_remainder_loop: trip count is negative o zero"));
        branch_cond = V_BR_I4GE;
        if (SR_is_const(lb_sr)) {
            //new_upperb = lowb + new_tr
            new_trip_count_val = new_trip_count_val + SR_int_imm(lb_sr);
            *remaing_count =
                m_cg->gen_imm(new_trip_count_val);
        } else {
            ASSERT0(SR_is_reg(lb_sr));
            *remaing_count = m_cg->dupSR(lb_sr);
            //new_upperb = lowb + new_tr
            IOC tmp;
            m_cg->buildAdd(lb_sr,
                           m_cg->gen_imm(new_trip_count_val),
                           SR_size(trip_count_sr),
                           true,
                           ors,
                           &tmp);
            remaing_count = tmp.get_reg(0);
        }
    } else {
        *remaing_count = dupSR(trip_count_sr);
        INT trip_size = SR_size(trip_count_sr);
        branch_cond = trip_size == 4 ? V_BR_I4GE : V_BR_I8GE;

        //new_tr = tr % n;
        IOC cont;
        IOC_pred(&cont) = m_cg->genTruePred();
        m_cg->buildMod(CLUST_UNDEF,
                       remaing_count,
                       trip_count_sr,
                       m_cg->gen_imm(num_para_part, trip_size),
                       trip_size,
                       false,
                       ors,
                       &cont);
        //new_upperb = lowb + new_tr
        IOC tmp;
        m_cg->buildAdd(remaing_count,
                       lb_sr,
                       trip_size,
                       true,
                       ors,
                       &tmp);
        remaing_count = tmp.get_reg(0);
    }

    //Generate condition statement to determine whether
    //if the remainder loop execute.
    xoc::LabelInfo l1 = genLabel(bb3);
    m_cg->buildOR(IR_TRUEBR,
                  NULL,
                  m_cg->genLabel(l1, 0),
                  iv,
                  *remaing_count,
                  branch_cond,
                  &ors);
    bb1->append(&ors);
    if (m_cg->isIntRegSR(NULL, *remaing_count, 0, false)) {
        ORBB_cg(m_bb)->set_sr_liveout(bb1, *remaing_count);
    }
}


static bool fullyUnrollBB(ORBB * bb, DataDepGraph & ddg)
{
    SR * trip_count_sr = getTripCountSR(bb);
    if (!SR_is_const(trip_count_sr) ||
        SR_int_imm(trip_count_sr) > TRIP_COUNT_ALLOW_FULLY_UNROLL_BB) {
        return false;
    }
    OR * red_or = NULL, * cmp_or = NULL;
    SR * iv = NULL;
    FindMainIV(bb, ddg, &red_or, &cmp_or, &iv);
    ASSERTN(red_or && iv && SR_is_global(iv), ("Cannot find iv"));
    OR * br_or = BB_xfer_op(bb);
    ORBB_orlist(bb).remove(br_or);
    ORBB_orlist(bb).remove(cmp_or);

    for (INT i = 0; i < SR_int_imm(trip_count_sr) - 1; i++) {
        ORBB * tmpbb = Dup_Bb(bb, true);
        BB_Append_All(bb, tmpbb);
    }
    return true;
}


static void modifyBB2(IN OUT ORBB * bb2, ORBB const* orig_bb,
                      SR * rem_count_sr, UINT num_para_part)
{
    //Modify ORBB info
    IR * orig_loopinfo = LOOPINFO_ir(getLoopInfo(orig_bb));
    if (SR_is_reg(rem_count_sr)) {
        bb2->setLiveIn(rem_count_sr);
    }
    //Regenerate LOOPINFO for ORBB.
    IR * ir = m_ru->dupIRTree(orig_loopinfo);
    IR_loop_trip_est(ir) = IR_loop_trip_est(orig_loopinfo) % num_para_part;

    xoc::LoopInfo * loop_info = TYPE_PU_ALLOC(xoc::LoopInfo);
    LOOPINFO_ir(loop_info) = ir;
    LOOPINFO_srcpos(loop_info) = LOOPINFO_srcpos(getLoopInfo(orig_bb));

    //trip_count = orig_trip_count - orig_trip_count % npart
    IR * loop_trip_ir = NULL;
    OR_TYPE opc_intconst =
            OR_TYPE_make_op(OPR_INTCONST,
                            IR_loop_trip(orig_loopinfo)->getType());
    if (SR_is_const(rem_count_sr)) {
        LOOPINFO_trip_count_sr(loop_info) = rem_count_sr;
        loop_trip = m_ru->buildConst(opc_intconst, SR_int_imm(rem_count_sr));
        IR_set_loop_trip(ir, loop_trip_ir);
        BB_Add_Annotation(bb2, ANNOT_LOOPINFO, loop_info);
    } else {
        LI_trip_count_sr(loop_info) = rem_count_sr;
        OR_TYPE opc_mod =
            OR_TYPE_make_op(OPR_MOD,
                IR_loop_trip(orig_loopinfo)->getType());
        loop_trip_ir = m_ru->buildBinarySimpOp(opc_mod,
            m_ru->dupIRTree(IR_loop_trip(orig_loopinfo)),
            m_ru->buildConst(opc_intconst, num_para_part));
        IR_set_loop_trip(ir, loop_trip_ir);
        BB_Add_Annotation(bb2, ANNOT_LOOPINFO, loop_info);
    }

    //ORBB's dependence graph.
    DataDepGraph ddg;
    ddg.set_param(NO_PHY_REG,
                  NO_MEM_READ,
                  INC_MEM_FLOW,
                  INC_MEM_OUT,
                  INC_CONTROL,
                  NO_REG_READ,
                  INC_REG_ANTI,
                  INC_MEM_ANTI,
                  INC_SYM_REG);
    ddg.init(bb2);
    ddg.build();

    //Try fully unrolling
    if (!fullyUnrollBB(bb2, ddg)) {
        //Modify comparation operation and attach new label.
        OR * red_or = NULL, * cmp_or = NULL;
        SR * iv = NULL;
        FindMainIV(bb2, ddg, &red_or, &cmp_or, &iv);
        ASSERTN(red_or && iv && SR_is_global(iv),
               ("Cannot find iv"));
        ASSERTN(cmp_or && m_cg->isCompareOR(cmp_or),
               ("Not innermost loop"));
        ASSERT0(cmp_or->get_opnd( 0) == m_cg->genTruePred());
        OR * br_or = BB_xfer_op(bb2);
        if (cmp_or->get_opnd( 1) == iv) { //The comparation should be iv < UB
            ASSERT0(!m_cg->is_gt(cmp_or));
            if (!m_cg->is_lt(cmp_or)) {
                //Revise comparing opcode.
                OR_TYPE lt_opc;
                if (SR_is_const(rem_count_sr)) {
                    lt_opc = m_cg->computeEquivalentORType(OR_slti_m,
                        m_cg->computeORUnit(cmp_or, NULL),
                        m_cg->computeORCluster(cmp_or, NULL));
                } else {
                    UnitSet const* us = m_cg->computeORUnit(cmp_or);
                    ASSERT0(us && us->get_elem_count() == 1);
                    lt_opc = m_cg->computeEquivalentORType(OR_slt_m,
                        us->get_first(), m_cg->computeORCluster(cmp_or));
                }
                OR_code(cmp_or) = lt_opc;
            }
            cmp_or->set_opnd(2, rem_count_sr);
        } else { //The comparation should be UB > iv
            ASSERTN(cmp_or->get_opnd( 2) == iv, ("illegal condition o"));
            ASSERT0(!m_cg->is_lt(cmp_or));
            if (!m_cg->is_gt(cmp_or)) {
                //Revise comparing opcode.
                OR_TYPE gt_ot;
                if (SR_is_const(rem_count_sr)) {
                    gt_ot = m_cg->computeEquivalentORType(OR_sgti_m,
                        m_cg->computeORUnit(cmp_or),
                        m_cg->compute_op_clust(cmp_or, NULL));
                } else {
                    gt_ot = m_cg->computeEquivalentORType(OR_sgt_m,
                        m_cg->computeORUnit(cmp_or),
                        m_cg->computeORCluster(cmp_or, NULL));
                }
                OR_code(cmp_or) = gt_ot;
            }
            cmp_or->set_opnd(1, rem_count_sr);
        }

        SR * true_res = cmp_or->get_result(1);
        replaceBranchCond(br_or, true_res);
        LabelInfo l = genLabel(bb2);
        SR * label_sr = m_cg->genLabel(l, 0);
        replaceBranchLabel(br_or, label_sr);
    }
}


//Generate code for BB3 and target label for BB5
//TRUEBR to bb5 if iv > UB
static void fillBB3(OUT ORBB * bb3, IN OUT ORBB * bb5,
                    IN SR * ub_sr, IN SR * iv)
{
    ORList ors;
    //Generate condition statement to determine whether
    //if the main loop execute.
    LabelInfo l2 = genLabel(bb5);
    INT trip_size = SR_size(ub_sr);
    UINT branch_cond = trip_size == 4 ? V_BR_I4GT : V_BR_I8GT;
    m_cg->buildOR(IR_TRUEBR, NULL,
        m_cg->genLabel(l2, 0), iv, ub_sr, branch_cond, &ors);
    bb3->append(&ors);
}


void replaceBranchCond(OR * br_or, SR * cond)
{
    ASSERT0(SR_is_pred(cond));
    switch (OR_code(br_or)) {
    case OR_b_b:
        br_or->set_pred(cond);
        break;
    default:
        ;ASSERTN(0, ("unknown branch operation"));
    }
}


void replaceBranchLabel(OR * br_or, SR * label_sr)
{
    switch (OR_code(br_or)) {
    case OR_b_b:
        br_or->set_opnd(1, label_sr);
        break;
    default:
        ;ASSERTN(0, ("unknown branch operation"));
    }
}


//Record/clear the referrence mark of GSR.
static void markGSR(ORBB const* bb, bool is_clear)
{
    for (OR * o = ORBB_orlist(bb).get_head(); o;
         o = ORBB_orlist(bb).get_next()) {
        for (INT i = 0;  i < o->result_num(); i++) {
            SR * res = o->get_result(i);
            if (SR_is_global(res)) {
                xcom::BitSet * bs = g_sr2bbset_map.get(res);
                if (bs == NULL) {
                    bs = g_bs_mgr.create();
                    g_sr2bbset_map.set(res, bs);
                }
                if (is_clear) {
                    bs->diff(BB_id(bb));
                } else {
                    bs->bunion(BB_id(bb));
                }
            }
        }//end for
        for (INT i = 0; i < o->opnd_num(); i++) {
            SR * opnd = o->get_opnd( i);
            if (SR_is_global(opnd)) {
                xcom::BitSet * bs = g_sr2bbset_map.get(opnd);
                if (bs == NULL) {
                    bs = g_bs_mgr.create();
                    g_sr2bbset_map.set(opnd, bs);
                }
                if (is_clear) {
                    bs->diff(BB_id(bb));
                } else {
                    bs->bunion(BB_id(bb));
                }
            }
        }
    }
}


//Update original bb loop info.
static void modifyBB(IN OUT ORBB * bb,
                     SR * trip_count_sr,
                     UINT num_para_part)
{
    xoc::LoopInfo * orig_loop_info = getLoopInfo(bb);
    IR * orig_loopinfo = LOOPINFO_ir(orig_loop_info);
    //Generate new estimate IR of trip count info for ORBB.
    IR_loop_trip_est(orig_loopinfo) =
        IR_loop_trip_est(orig_loopinfo) -
            IR_loop_trip_est(orig_loopinfo) %
            num_para_part;

    //trip_count = orig_trip_count - orig_trip_count % npart
    OR_TYPE opc_intconst =
            OR_TYPE_make_op(IR_CONST,
                            IR_loop_trip(orig_loopinfo)->getType());
    if (SR_is_const(trip_count_sr)) {
        INT new_trip_count_val =
            SR_int_imm(trip_count_sr) - SR_int_imm(trip_count_sr) % num_para_part;
        LOOPINFO_trip_count_sr(orig_loop_info) =
            m_cg->gen_imm(new_trip_count_val, SR_size(trip_count_sr));
        //Generate new IR to describe actually trip count.
        IR * new_trip_count = m_ru->buildConst(opc_intconst, new_trip_count_val);
        orig_loopinfo->setLoopTrip(new_trip_count);
    } else {
        LOOPINFO_trip_count_sr(orig_loop_info) = NULL;
        //Information to regenerate trip count IR was lost!
        //Cannot generate the IR.
        //OR_TYPE mod =
        //    OR_TYPE_make_op(IR_MOD,
        //                    IR_loop_trip(orig_loopinfo)->getType());
        //OR_TYPE sub =
        //    OR_TYPE_make_op(IR_SUB,
        //                    IR_loop_trip(orig_loopinfo)->getType());
        //IR * mod =
        //    m_ru->buildStore(mod,
        //                  m_ru->dupIRTree(IR_loop_trip(orig_loopinfo)),
        //                  m_ru->buildConst(op_intconst, num_para_part));
        ////Generate new IR to describe actually trip count.
        //IR * new_trip_count =
        //    m_ru->buildStore(sub,
        //                  m_ru->dupIRTree(IR_loop_trip(orig_loopinfo)), mod);
        //IR_set_loop_trip(orig_loopinfo, new_trip_count);
    }
}


//Looking for the sr that recording low bound of
//induction variable of DO-LOOP.
//'bb': the ors generated to compute upper bound will inserted into bb.
static SR * genUpperBound(IN SR * iv, IN OR * cmp_or, IN OUT ORBB * bb)
{
    SR * ub_sr = NULL;
    ORList ors;
    if (OR_is_eq(cmp_or)) {
        //LOOP MODE:
        //    iv = LB
        //    do {
        //        ...
        //        iv = iv + 1
        //    } while (FALSEBR(iv == VAL))
        //So ub = VAL - 1
        ub_sr = dupSR(iv);
        SR * val_sr = NULL;
        if (cmp_or->get_opnd( 1) == iv) {
            val_sr = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            val_sr = cmp_or->get_opnd( 1);
        } else {
            ASSERTN(0, ("Incomplete o"));
        }
        IOC tmp;
        m_cg->buildSub(val_sr,
                       m_cg->gen_imm(1),
                       SR_size(ub_sr),
                       true,
                       ors,
                       tmp);
        ub_sr = tmp.get_reg(0);
        bb->append(&ors);
    } else if (m_cg->is_lt(cmp_or)) {
        //LOOP MODE:
        //    iv = LB
        //    do {
        //        ...
        //        iv = iv + 1
        //    } while (iv < VAL)
        //So ub = VAL - 1
        ub_sr = dupSR(iv);
        SR * val_sr = NULL;
        if (cmp_or->get_opnd( 1) == iv) {
            val_sr = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            val_sr = cmp_or->get_opnd( 1);
        } else {
            ASSERTN(0, ("Incomplete o"));
        }
        IOC tmp;
        m_cg->buildSub(
                    val_sr,
                    m_cg->gen_imm(1),
                    SR_size(ub_sr),
                    true,
                    ors,
                    tmp);
        ub_sr = tmp.get_reg(0);
        bb->append(bb);
    } else if (m_cg->is_gt(cmp_or)) {
        //LOOP MODE:
        //    iv = LB
        //    do {
        //        ...
        //        iv = iv + 1
        //    } while (VAL > iv)
        //ub = VAL - 1

        //o

        //    iv = LB
        //    do {
        //        ...
        //        iv = iv + 1
        //    } while (FALSEBR(iv > VAL))
        //That is to say iv <= VAL
        //ub = VAL
        if (cmp_or->get_opnd( 1) == iv) {
            ub_sr = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            ub_sr = dupSR(iv);
            SR * val_sr = cmp_or->get_opnd( 1);
            I2O tmp;
            m_cg->buildSub(
                        val_sr,
                        m_cg->gen_imm(1),
                        SR_size(ub_sr),
                        true,
                        ors);
            ub_sr = tmp.get_reg(0);
            bb->append(&ors);
        } else {
            ASSERTN(0, ("Incomplete o"));
        }
    } else {
        ASSERTN(0, ("Unsupport!"));
    }
    return ub_sr;
}


//Looking for the sr that recording low bound of
//induction variable of DO-LOOP.
//'bb': DO-LOOP body
//'imm_pred': immediate predecessor of 'bb'.
static SR * genLowBound(SR * iv, ORBB * bb, ORBB * imm_pred)
{
    SR * lb_sr = NULL;
    BS * dom = BB_dom_set(bb);
    ASSERT0(dom && BS_MemberP(dom, imm_pred->id));
    OR * def_iv = NULL;
    ORCt * ct;
    for (OR * o = ORBB_orlist(imm_pred)->get_tail(&ct); o;
         o = ORBB_orlist(imm_pred)->get_prev(&ct)) {
        if (ORBB_cg(m_bb)->mustDef(o, iv) &&
            !m_cg->isCondExecOR(o)) {
            def_iv = o;
            break;
        }
    }
    if (!def_iv) {
        //TODO: Try more dominate-ORBB to find the DEF o.
        return NULL;
    }
    if (m_cg->is_movi(def_iv)) {
        lb_sr = dupSR(def_iv->get_opnd( 1));
        ASSERT0(SR_is_const(lb_sr));
        if ((UINT)SR_int_imm(lb_sr) > (UINT)0x7fffFFFF) {
            ASSERTN(0, ("Low bound is too large!"));
        }
    } else if (m_cg->isCopyOR(def_iv)) {
        INT i = m_cg->computeCopyOpndIdx(def_iv);
        SR * def_iv_cp_opnd = def_iv->get_opnd( i);
        lb_sr = dupSR(def_iv_cp_opnd);
        ASSERT0(SR_is_reg(lb_sr));

        ORList ors;
        CLUST clst = m_cg->computeORCluster(def_iv);
        UNIT unit = m_cg->computeORUnit(def_iv)->checkAndGet();
        m_cg->buildCopyPred(clst, unit, def_iv_cp_opnd,
            lb_sr, m_cg->genTruePred(), ors);
        ORCt * ct;
        ORBB_orlist(imm_pred).find(def_iv, &ct);
        ASSERT0(ct);
        ORBB_orlist(imm_pred).insert_after(ors, def_iv);
        ORBB_cg(m_bb)->set_sr_liveout(imm_pred, lb_sr);
    } else {
        //TODO: Recog more operaionts
        return NULL;
    }
    return lb_sr;
}


//Computing trip_count = UpperBound - LowBound + 1.
//'bb': insert ors which computing trip count into ORBB.
static SR * genTripCount(IN SR * lb_sr, IN SR * ub_sr, IN OUT ORBB * bb)
{
    SR * trip_count_sr = NULL;
    if (SR_is_int_imm(lb_sr) && SR_is_int_imm(ub_sr)) {
        ASSERT0(SR_size(lb_sr) == SR_size(ub_sr));
        trip_count_sr =
            m_cg->gen_imm(SR_int_imm(ub_sr) - SR_int_imm(lb_sr) + 1,
                            SR_size(lb_sr));
        return trip_count_sr;
    } else if (!SR_is_const(lb_sr)) {
        trip_count_sr = dupSR(lb_sr);
    } else if (!SR_is_const(ub_sr)) {
        trip_count_sr = dupSR(ub_sr);
    } else {
        UNREACHABLE();
    }

    ORList ors;
    IOC tmp;
    m_cg->buildSub(ub_sr, lb_sr, SR_size(trip_count_sr), true, ors, &tmp);
    trip_count_sr = tmp.get_reg(0);
    tmp.clean();
    m_cg->buildAdd(
                trip_count_sr,
                m_cg->gen_imm(1),
                SR_size(trip_count_sr),
                true,
                ors,
                &tmp);
    trip_count_sr = tmp.get_reg(0);
    bb->append(&ors);
    return trip_count_sr;
}


//Splitting parallelizable 'bb' into two part, the second part
//is a parallel loop body whose trip count can be divide by
//the number of parallel part, and the first part is remainder loop.
//    Given original loop:
//        j = 0
//        do {
//            ...
//            j = j + 1
//        } while (j != 9)
//        ...
//    Output will be:
//        ***BB1**
//        cmp j < 1 //1 is remainder loop upper bound.
//        B_if_false L1
//        ***BB2**
//        do {
//            ...
//            j = j + 1
//        } while (j < 1) //Remainder loop
//        ***BB3**
//        L1:
//        cmp j < 9 //9 is trip_count
//        B_if_fasle L2
//        ***orig ORBB**
//        do {
//            ...
//            j = j + 1
//        } while (j != 9) //Main unroll loop
//        ***BB5**
//        L2:
//        ********
//        ...
static bool LoopPeeling(IN ORBB * bb,
                        OUT ORBB ** remainder_bb,
                        OUT ORBB ** parallel_bb,
                        OR ** red_or,
                        OR ** cmp_or,
                        SR ** iv,
                        UINT num_para_part)
{
    ASSERT0(remainder_bb && parallel_bb && red_or && cmp_or && iv);
    SR * orig_trip_count_sr = getTripCountSR(bb);
    if (SR_is_const(orig_trip_count_sr)) {
        if (SR_int_imm(orig_trip_count_sr) % num_para_part == 0) {
            *remainder_bb = NULL;
            *parallel_bb = bb;

            //Clear parallel flag lest the bb be optimized
            //accidently in other phase.
            xoc::LoopInfo * loopinfo = getLoopInfo(bb);
            REMOVE_FLAG(LI_flag(loopinfo), LI_PARALLELIZABLE);
            LI_flag(loopinfo) |= LOOP_PARALLELIZED;
            return true;
        }
        if (BB_xfer(bb) == NULL) {
            //No a loop?
            return false;
        }
    }

    //Get pred and succ in CFG of bb.
    ORBB * orig_pred = NULL, * orig_succ = NULL;
    getLoopUniquePredSucc(bb, &orig_pred, &orig_succ);
    ASSERT0(orig_pred && orig_succ);

    ORBB * bb1 = Gen_BB();
    ORBB * bb2 = Dup_Bb(bb, true);
    ORBB * bb3 = Gen_BB();
    ORBB * bb5 = Gen_BB();

    DataDepGraph ddg;
    ddg.set_param(NO_PHY_REG,
                  NO_MEM_READ,
                  INC_MEM_FLOW,
                  INC_MEM_OUT,
                  INC_CONTROL,
                  NO_REG_READ,
                  INC_REG_ANTI,
                  INC_MEM_ANTI,
                  INC_SYM_REG);
    ddg.init(bb);
    ddg.build();
    FindMainIV(bb, ddg, red_or, cmp_or, iv);
    ASSERTN(*red_or && *iv && *cmp_or && SR_is_global(*iv),
            ("Cannot find iv"));
    ddg.destroy();

    //Looking for the sr that recording low bound of
    //induction variable of DO-LOOP.
    //Inserting COPY o in immediate predecessor.
    SR * lb_sr = genLowBound(*iv, bb, orig_pred);
    if (!lb_sr) {
        return false;
    }

    //Inserting code in BB1 to compute trip count.
    //LB <= iv <= UB
    //trip_count = UB - LB + 1
    //remained_trip_count = trip_count % num_para_part.
    //Then UB of remaineder loop equals LB + remained_trip_count,
    //named as UBr.
    //In BB1, if iv >= UBr, branch to BB3.
    //And both UB and UBr lives out of BB1, that used by BB2 and BB3.
    //In BB2, if iv < UBr(o iv == UBr), goto remainder loop body.
    //In BB3, if iv > UB, then branch to BB5, o else fall through to ORBB.
    SR * ub_sr = genUpperBound(*iv, *cmp_or, bb1);
    ASSERT0(ub_sr);
    if (SR_is_reg(ub_sr)) {
        ORBB_cg(m_bb)->set_sr_liveout(bb1, ub_sr);
    }
    SR * trip_count_sr = genTripCount(lb_sr, ub_sr, bb1);
    if (SR_is_const(orig_trip_count_sr)) {
        ASSERT0(SR_is_int_imm(orig_trip_count_sr));
        ASSERT0(SR_int_imm(trip_count_sr) == SR_int_imm(orig_trip_count_sr));
    }

    //Padding BB1, BB2, BB3, BB5.
    SR * remaing_count = NULL;
    fillBB1(bb1, bb, bb3, num_para_part, *iv,
            &remaing_count, trip_count_sr, lb_sr, *red_or);
    modifyBB2(bb2, bb, remaing_count, num_para_part);
    if (SR_is_const(trip_count_sr) &&
        SR_is_const(remaing_count)) {
        ASSERT0(SR_int_imm(trip_count_sr) >= SR_int_imm(remaing_count));
        if (SR_int_imm(trip_count_sr) == SR_int_imm(remaing_count)) {
            bb.clean();
        } else {
            modifyBB(bb, trip_count_sr, num_para_part);
        }
    } else {
        if (SR_is_reg(ub_sr)) {
            bb3->setLiveIn(ub_sr);
        }
        fillBB3(bb3, bb5, ub_sr, *iv);
        modifyBB(bb, trip_count_sr, num_para_part);
    }

    //Revise control flow.
    unchainPredAndSucc(orig_pred, bb);
    unchainPredAndSucc(bb, orig_succ);

    chainPredAndSucc(orig_pred, bb1); //fall through
    chainPredAndSucc(bb1, bb2); //fall through
    chainPredAndSucc(bb2, bb3); //fall through
    chainPredAndSucc(bb3, bb); //fall through
    chainPredAndSucc(bb, bb5); //fall through
    chainPredAndSucc(bb5, orig_succ); //fall through

    chainPredAndSucc(bb1, bb3); //false branch
    chainPredAndSucc(bb2, bb2); //loop back, remainder loop
    chainPredAndSucc(bb3, bb5); //false branch

    //Add BBs into region
    insertBB(bb1, orig_pred);
    insertBB(bb2, bb1);
    insertBB(bb3, bb2);
    insertBB(bb5, bb);

    markGSR(bb1, false);
    markGSR(bb2, false);
    markGSR(bb3, false);
    markGSR(bb, false);
    markGSR(bb5, false);

    //Clear parallel flag lest the bb be optimized accidently in other phase.
    xoc::LoopInfo * li = getLoopInfo(bb);
    if (li) {
        IR * loopinfo = LOOPINFO_ir(getLoopInfo(bb));
        REMOVE_FLAG(LI_flag(loopinfo), LI_PARALLELIZABLE);
        LI_flag(loopinfo) |= LOOP_PARALLELIZED;
    }
    li = getLoopInfo(bb2);
    if (li) {
        IR * loopinfo = LOOPINFO_ir(getLoopInfo(bb2));
        REMOVE_FLAG(LI_flag(loopinfo), LI_PARALLELIZABLE);
    }

    //Output for caller.
    *remainder_bb = bb2;
    *parallel_bb = bb;
    return true;
}


xoc::LoopInfo * findLoopInfo(ORBB const* bb)
{
    for (xoc::LoopInfo * loop_info = g_loopdesc_list.get_head();
         loop_info != NULL;
         loop_info = g_loopdesc_list.get_next()) {
        if (bb == LI_loop_head(loop_info)) {
            return loop_info;
        }
    }
    return NULL;
}


static bool isParaMultiBB(LoopDesc * loop_desc)
{
    return false;
}


static bool isParaBB(ORBB * bb)
{
    return true;
}


//Perform parallelism optimization for one ORBB.
//Control flow will change if perform parallelism successfully.
static bool parallelBB(ORBB * bb, ParallelPartMgrVec & ppm_vec)
{
    xoc::LoopInfo * loopinfo = bb->getLoopInfo();
    if (loopinfo != NULL) {
        if (!LI_innermost(loopinfo) ||
            //!LI_nz_trip(loopinfo) ||
            !LI_parallelizable(loopinfo)) {
            return false;
        }
    } else {
        return false;
    }

    #ifdef _DEBUG_
    note("\n*************************************\n");
    note("\nRegion:%s:ORBB%d:", Cur_PU_Name, bb->id);
    #endif

    if (!isParaBB(bb)) {
        //return false;
    }
    LoopDesc * loop_desc = findLoopDesc(bb);
    if (loop_desc != NNLL) {
        xcom::BitSet * bbset = LI_bb_set(loop_desc);
        ASSERT0(bbset);
        bool multi_bb = bbset->get_elem_count() > 1;
        if (multi_bb) {
            if (!isParaMultiBB(loop_desc)) {
                #ifdef _DEBUG_
                note("\nMulti-ORBB do not allowed currently!!!"
                                 " Loop Header:ORBB%d\n", bb->id);
                #endif
                return false;
            }
            ORBB * new_single_bb = forceIfConvert(loop_desc, false);
            if (new_single_bb == NULL) {
                REMOVE_FLAG(LI_flag(loopinfo), LI_PARALLELIZABLE);
                #ifdef _DEBUG_
                note("\nIf-Conversion for Multi-ORBB failed!!!"
                                 " Loop Header:ORBB%d\n", bb->id);
                #endif
                return false;
            }
            bb = new_single_bb;
        }
    }

    ParallelPartMgr * ppm = new ParallelPartMgr(bb); //delete it after Lra()
    ppm_vec.set(BB_id(bb), ppm);
    ppm->computeNumOfParallelPart();
    if (ppm->numOfParallelPart() == 0) {
        return false;
    }

    ORBB * rem_bb = NULL, * para_bb = NULL;
    OR * red_or = NULL;
    OR * cmp_or = NULL;
    SR * iv = NULL;
    if (!LoopPeeling(bb, &rem_bb, &para_bb,
                     &red_or, &cmp_or, &iv,
                     ppm->numOfParallelPart())) {
        #ifdef _DEBUG_
        note("\nLoop_Peeling() failed!!! ORBB%d\n", bb->id);
        #endif
        return false;
    }

    //Reanalysis GSR info
    getRegionEntryBB()->initLiveness();
    if (para_bb == NULL) {
        #ifdef _DEBUG_
        note("\nAfter LoopPeeling(ORBB%d), para_bb is NULL?!\n", bb->id);
        #endif
        return false;
    }
    m_cfg->computeDom2(); //used by genPrologAndEpilog()
    if (!isParaBB(bb)) {
        return true;
    }
    ppm->setBB(para_bb);
    if (ppm->prepare_distribute(red_or, cmp_or, iv)) {
        if (!ppm->distribute()) {
            UNREACHABLE();
        }
    }
    #ifdef _DEBUG_
    note("\nParallellizing Success!! Dump all of OR:\n");
    for (OR * o = ORBB_orlist(para_bb).get_head(); o != NULL;
         o = ORBB_orlist(para_bb).get_next()) {
        prt("\t");
        Print_OP_No_SrcLine(o);
    }
    note("\n\n\n");
    #endif
    return true;
}


//Return the unique predecessor and successor for natural loop with single ORBB.
bool getLoopUniquePredSucc(ORBB * bb, ORBB ** pred, ORBB ** succ)
{
    ASSERTN(BB_loop_head(bb) == bb, ("bb must be loop with single ORBB"));
    INT count = 0;
    ORBB * orig_pred = NULL, * orig_succ = NULL;
    for (ORBB * pred = bb->getPreds()->get_head();
         pred != NULL; pred = bb->gtPreds()->get_next()) {
        count++;
        if (pred != bb) {
            orig_pred = pred;
        }
    }
    if (NULL == orig_pred || count != 2) {
        return false;
    }
    count = 0;
    for (ORBB * succ = bb->get_succs()->get_head();
         succ != NULL; succ = bb->get_succs()->get_next()) {
        count++;
        if (succ != bb) {
            orig_succ = succ;
        }
    }
    if (NULL == orig_succ || count != 2) {
        return false;
    }
    *pred = orig_pred;
    *succ = orig_succ;
    return true;
}


OR * dupOR(OR * o)
{
    OR * new_or = m_cg->gen_or();
    new_or->clone(o);
    //TODO: copy IR for memory OR
    //copyIR(new_or, o);
    return new_or;
}


ORBB * dupBB(ORBB * bb, bool rename_lcl_sr)
{
    SR2SRDmap dmap;
    ORBB * newBB = Gen_BB();

    for (OR * o = ORBB_orlist(bb).get_head(); o;
         o = ORBB_orlist(bb).get_next()) {
        OR * new_or = Dup_OP(o);

        //TODO:Copy IR for memory operation (new_or, o);
        ASSERT0(0);

        if (rename_lcl_sr) {
            for (INT i = 0;  i < new_or->result_num(); i++) {
                SR * res = new_or->get_result(i);
                if (SR_is_reg(res) &&
                    !SR_is_dedicated(res) &&
                    !SR_is_global(res)) {
                    SR * newsr = dupSR(res);
                    dmap.setAlways(res, newsr);

                    //Rename result sr of o for parallel part.
                    new_or->set_result(i, newsr);
                }
            }
            for (INT i = 0; i < new_or->opnd_num(); i++) {
                SR * opnd = new_or->get_opnd( i);
                if (SR_is_reg(opnd) &&
                    !SR_is_dedicated(opnd) &&
                    !SR_is_global(opnd)) {
                    SR * newsr = dmap.get(opnd);
                    ASSERTN(newsr, ("local sr must have DEF"));
                    //Rename operand sr of o for parallel part.
                    new_or->set_opnd(i, newsr);
                }
            }
        }
        newBB->appendOR(new_or);
    }
    return newBB;
}
#endif


#if 0
//Parallel BBs which marked with PARALLELIZABLE.
void parallelInnerLoop(ParallelPartMgrVec & ppm_vec)
{
    g_loopdesc_list.init();
    g_sr2bbset_map.init();

    m_cfg->computeDom2(); //needed for loop recognition
    for (loopDesc * loop = LOOP_DESCR_Detect_Loops(&m_pool);
        loop != NULL; loop = LOOP_DESCR_next(loop)) {
        g_loopdesc_list.append_tail(loop);
    }

    //Record occurrence of GSR.
    //TODO: Should build dataflow graph.
    Show_Phase("---\tmarkGSR() in CG_PARA_PART", NULL);
    for (ORBB * bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
        markGSR(bb, false);
    }

    ORBB * next;
    for (ORBB * bb = REGION_First_BB; bb != NULL; bb = next) {
        next = BB_next(bb);
        Show_Phase("---\tParallel_BB() in CG_PARA_PART", bb);
        if (parallelBB(bb, ppm_vec)) {
            next = REGION_First_BB;
            continue;
        }
    }

    #ifdef _DEBUG_
    prt("Finish CG Parallelizing");
    note("\n*************************************\n\n\n\n");
    #endif

    Show_Phase("---\tEnd of ParallelInnerLoop()", NULL);
    g_loopdesc_list.destroy();
    g_sr2bbset_map.destroy();
}
#endif

} //namespace xgen
