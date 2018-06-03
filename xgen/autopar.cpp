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
LOOPINFO * Get_LoopInfo(ORBB const* bb)
{
    LOOPINFO * loop_info = NULL;
    ANNOTATION * annot = ANNOT_Get(BB_annotations(bb), ANNOT_LOOPINFO);
    if (annot) {
        loop_info = ANNOT_loopinfo(annot);
    }
    return loop_info;
}


SR * Get_TripCount_Tn(ORBB const* bb)
{
    ANNOTATION * annot = ANNOT_Get(BB_annotations(bb), ANNOT_LOOPINFO);
    ASSERT0(annot);
    LOOPINFO * loop_info = ANNOT_loopinfo(annot);
    ASSERT0(loop_info);
    return LOOPINFO_trip_count_tn(loop_info);
}


//Propagate live info.
void Prop_BB_LiveInfo(ORBB const* from , ORBB * to)
{
    if (BB_bbregs(to) == NULL) {
        BB_bbregs(to) = TYPE_MEM_POOL_ALLOC(BBREGS, g_orc_pu_pool);
    }
#ifdef ORC_METHOD
    GRA_LIVE_Compute_Liveness_For_BB(to);
#else
    BB_live_in(to) =
        GTN_SET_Copy(BB_live_out(from), g_orc_pu_pool);
    BB_live_out(to) =
        GTN_SET_Copy(BB_live_in(to), g_orc_pu_pool);
    BB_defreach_in(to) =
        GTN_SET_Copy(BB_defreach_out(from), g_orc_pu_pool);
    BB_defreach_out(to) =
        GTN_SET_Copy(BB_defreach_in(to), g_orc_pu_pool);

    BB_live_use(to) = GTN_SET_Create(GTN_UNIVERSE_size, g_orc_pu_pool);
    BB_live_def(to) = GTN_SET_Create(GTN_UNIVERSE_size, g_orc_pu_pool);
#endif

    //Set dominator info.
    Set_BB_dom_set(to, BS_Create_Empty(2+PU_BB_Count+1, g_orc_pu_pool));
    BS_Union(BB_dom_set(to), BB_dom_set(from), g_orc_pu_pool);
    BS_Union1(BB_dom_set(to), ORBB_id(to), g_orc_pu_pool);
    Set_BB_pdom_set(to, BS_Create_Empty(2+PU_BB_Count+1, g_orc_pu_pool));
    BS_Union(OR_BB_pdom(to), OR_BB_pdom(from), g_orc_pu_pool);
    BS_Union1(OR_BB_pdom(to), BB_id(to), g_orc_pu_pool);
}


//TODO:Should allocate the structures in 'liveness_pool' o use
//GRA_LIVE_Compute_Local_Info() instead of.
void Alloc_Local_Liveness_Info(ORBB * bb)
{
    INT size = GTN_UNIVERSE_size;
    if (BB_bbregs(bb) == NULL) {
        BB_bbregs(bb) = TYPE_MEM_POOL_ALLOC(BBREGS, g_orc_pu_pool);
        BB_live_in(bb) = NULL;
    }
    if (BB_live_in(bb) == NULL) {
        BB_live_in(bb)      = GTN_SET_Create(size, g_orc_pu_pool);
        BB_live_out(bb)     = GTN_SET_Create(size, g_orc_pu_pool);
        BB_defreach_in(bb)  = GTN_SET_Create(size, g_orc_pu_pool);
        BB_defreach_out(bb) = GTN_SET_Create(size, g_orc_pu_pool);
        BB_live_use(bb)     = GTN_SET_Create(size, g_orc_pu_pool);
        BB_live_def(bb)     = GTN_SET_Create(size, g_orc_pu_pool);
    } else {
        //BB_live_in(bb)      = GTN_SET_ClearD(BB_live_in(bb));
        //BB_live_out(bb)     = GTN_SET_ClearD(BB_live_out(bb));
        //BB_defreach_in(bb)  = GTN_SET_ClearD(BB_defreach_in(bb));
        //BB_defreach_out(bb) = GTN_SET_ClearD(BB_defreach_out(bb));
        //BB_live_use(bb)     = GTN_SET_ClearD(BB_live_use(bb));
        //BB_live_def(bb)     = GTN_SET_ClearD(BB_live_def(bb));
    }
}


static bool FindMainIV(ORBB * bb, DataDepGraph & ddg,
                    OR ** red_or, OR ** cmp_or, SR ** iv)
{
    //If BB_last_op is not branch operation, instruction scheduling
    //has performed.
    OR * branch = bb->get_branch_or();
    ASSERT(branch, ("ORBB is not in loop end"));
    SR * predict_tn = branch->get_pred();
    ASSERT(predict_tn != m_cg->genTruePred(),
            ("illegal control flow"));
    List<OR*> preds;

    //Find compare or
    OR * cmp;
    ddg.getPredsByOrder(preds, branch);
    for (cmp = preds.get_tail(); cmp; cmp = preds.get_prev()) {
        if (m_cg->isCompareOR(cmp) &&
            ORBB_cg(m_bb)->mustDef(cmp, predict_tn) &&
            !m_cg->isCondExecOR(cmp)) {
            break;
        }
    }
    ASSERT(cmp, ("Not innermost loop"));
    *cmp_or = cmp;

    /*
    'cmp_or' may not refer 'trip_count_tn' alwayys.
    CASE:
    trip_count_tn is GTN258
    cmp_or is: [6:16]
    [   4:  1]| TN294 , :- copy_i TN97(p0)[P1] GTN231
    ...
    [   4: 12]| GTN258 , :- copy_i TN97(p0)[P1] TN294
    ...
    [   6: 16]| TN270 , TN268 , TN269 , :- seq_m TN97(p0)[P1] GTN247 GTN231
    */
    //ASSERT(TRUE == OP_Refs_TN(*cmp_or, trip_count_tn),
    //          ("Not innermost loop"));

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


/*
Generate code for low bound of IV, and target label for BB3

'n': number of parallel part
'iv': induction variable
'remaining_count_tn': rem_count_tn = trip_count % num_para_part + lb_tn
'lb_tn': low bound of induction variable 'iv'
'red_or': reduction o of 'iv'
*/
static void Fill_BB1(OUT ORBB * bb1, //early exit from loop
                     ORBB const* orig_bb,
                     IN OUT ORBB * bb3, //early exit from loop
                     UINT num_para_part,
                     IN SR * iv,
                     OUT SR ** remaining_count_tn,
                     IN SR * trip_count_tn,
                     IN SR * lb_tn,
                     IN OR * red_or)
{
    ORList ors;
    //Generate count sr for remainder loop
    UINT branch_cond = 0;
    if (TN_is_constant(trip_count_tn)) {
        //new_upperb = lowb + trip_count % num_para_part
        INT new_trip_count_val = SR_int_imm(trip_count_tn) % num_para_part;
        ASSERT(new_trip_count_val > 0,
                ("unroll_make_remainder_loop: trip count is negative o zero"));
        branch_cond = V_BR_I4GE;
        if (TN_is_constant(lb_tn)) {
            //new_upperb = lowb + new_tr
            new_trip_count_val = new_trip_count_val + SR_int_imm(lb_tn);
            *remaining_count_tn =
                m_cg->gen_imm(new_trip_count_val);
        } else {
            ASSERT0(SR_is_reg(lb_tn));
            *remaining_count_tn = m_cg->dupSR(lb_tn);
            //new_upperb = lowb + new_tr
            IOC tmp;
            m_cg->buildAdd(lb_tn,
                                m_cg->gen_imm(new_trip_count_val),
                                TN_size(trip_count_tn),
                                true,
                                ors,
                                &tmp);
            remaining_count_tn = tmp.get_reg(0);
        }
    } else {
        *remaining_count_tn = Dup_TN(trip_count_tn);
        INT trip_size = TN_size(trip_count_tn);
        branch_cond = trip_size == 4 ? V_BR_I4GE : V_BR_I8GE;

        //new_tr = tr % n;
        IOC cont;
        IOC_pred(&cont) = m_cg->genTruePred();
        m_cg->buildMod(CLUST_UNDEF,
                remaining_count_tn,
                trip_count_tn,
                m_cg->gen_imm(num_para_part, trip_size),
                trip_size,
                false,
                ors,
                &cont);
        //new_upperb = lowb + new_tr
        IOC tmp;
        m_cg->buildAdd(remaining_count_tn,
                            lb_tn,
                            trip_size,
                            true,
                            ors,
                            &tmp);
        remaining_count_tn = tmp.get_reg(0);
    }

    //Generate condition statement to determine whether
    //if the remainder loop execute.
    LABEL_IDX l1 = Gen_Label_For_BB(bb3);
    Exp_OP3v(OPC_TRUEBR,
                NULL,
                Gen_Label_TN(l1, 0),
                iv,
                *remaining_count_tn,
                branch_cond,
                &ors);
    BB_Append_Ops(bb1, &ors);
    if (m_cg->isIntRegSR(NULL, *remaining_count_tn, 0, false)) {
        Alloc_Local_Liveness_Info(bb1);
        ORBB_cg(m_bb)->set_sr_liveout(bb1, *remaining_count_tn);
    }
}


static bool Fully_Unroll_BB(ORBB * bb, DataDepGraph & ddg)
{
    SR * trip_count_tn = Get_TripCount_Tn(bb);
    if (!TN_is_constant(trip_count_tn) ||
        SR_int_imm(trip_count_tn) > TRIP_COUNT_ALLOW_FULLY_UNROLL_BB) {
        return false;
    }
    OR * red_or = NULL, * cmp_or = NULL;
    SR * iv = NULL;
    FindMainIV(bb, ddg, &red_or, &cmp_or, &iv);
    ASSERT(red_or && iv && SR_is_global(iv), ("Cannot find iv"));
    OR * br_or = BB_xfer_op(bb);
    ORBB_orlist(bb).remove(br_or);
    ORBB_orlist(bb).remove(cmp_or);

    for (INT i = 0; i < SR_int_imm(trip_count_tn) - 1; i++) {
        ORBB * tmpbb = Dup_Bb(bb, true);
        BB_Append_All(bb, tmpbb);
    }
    return true;
}


static void Modify_BB2(IN OUT ORBB * bb2, ORBB const* orig_bb,
                       SR * rem_count_tn, UINT num_para_part)
{
    //Modify ORBB info
    WN * orig_loop_info_wn = LOOPINFO_wn(Get_LoopInfo(orig_bb));
    if (SR_is_reg(rem_count_tn)) {
        Alloc_Local_Liveness_Info(bb2);
        bb2->set_livein(rem_count_tn);
    }
    //Regenerate LOOPINFO for ORBB.
    WN * wn = WN_COPY_Tree(orig_loop_info_wn);
    WN_loop_trip_est(wn) = WN_loop_trip_est(orig_loop_info_wn) % num_para_part;

    LOOPINFO * loop_info = TYPE_PU_ALLOC(LOOPINFO);
    LOOPINFO_wn(loop_info) = wn;
    LOOPINFO_srcpos(loop_info) = LOOPINFO_srcpos(Get_LoopInfo(orig_bb));

    //trip_count = orig_trip_count - orig_trip_count % npart
    WN * loop_trip_wn = NULL;
    OPCODE opc_intconst =
            OPCODE_make_op(OPR_INTCONST,
                           WN_rtype(WN_loop_trip(orig_loop_info_wn)), MTYPE_V);
    if (TN_is_constant(rem_count_tn)) {
        LOOPINFO_trip_count_tn(loop_info) = rem_count_tn;
        loop_trip_wn = WN_CreateIntconst(opc_intconst, SR_int_imm(rem_count_tn));
        WN_set_loop_trip(wn, loop_trip_wn);
        BB_Add_Annotation(bb2, ANNOT_LOOPINFO, loop_info);
    } else {
        LOOPINFO_trip_count_tn(loop_info) = rem_count_tn;
        OPCODE opc_mod =
            OPCODE_make_op(OPR_MOD,
                           WN_rtype(WN_loop_trip(orig_loop_info_wn)), MTYPE_V);
        loop_trip_wn = WN_CreateExp2(opc_mod,
                             WN_COPY_Tree(WN_loop_trip(orig_loop_info_wn)),
                             WN_CreateIntconst(opc_intconst, num_para_part));
        WN_set_loop_trip(wn, loop_trip_wn);
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
    if (!Fully_Unroll_BB(bb2, ddg)) {
        //Modify comparation operation and attach new label.
        OR * red_or = NULL, * cmp_or = NULL;
        SR * iv = NULL;
        FindMainIV(bb2, ddg, &red_or, &cmp_or, &iv);
        ASSERT(red_or && iv && SR_is_global(iv),
                ("Cannot find iv"));
        ASSERT(cmp_or && m_cg->isCompareOR(cmp_or),
                ("Not innermost loop"));
        ASSERT0(cmp_or->get_opnd( 0) == m_cg->genTruePred());
        OR * br_or = BB_xfer_op(bb2);
        if (cmp_or->get_opnd( 1) == iv) { //The comparation should be iv < UB
            ASSERT0(!m_cg->is_gt(cmp_or));
            if (!m_cg->is_lt(cmp_or)) {
                //Revise comparing opcode.
                OR_TYPE lt_opc;
                if (TN_is_constant(rem_count_tn)) {
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
            cmp_or->set_opnd(2, rem_count_tn);
        } else { //The comparation should be UB > iv
            ASSERT(cmp_or->get_opnd( 2) == iv, ("illegal condition o"));
            ASSERT0(!m_cg->is_lt(cmp_or));
            if (!m_cg->is_gt(cmp_or)) {
                //Revise comparing opcode.
                OR_TYPE gt_ot;
                if (TN_is_constant(rem_count_tn)) {
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
            cmp_or->set_opnd(1, rem_count_tn);
        }

        SR * true_res = cmp_or->get_result(1);
        Replace_Branch_Cond(br_or, true_res);
        LABEL_IDX l = Gen_Label_For_BB(bb2);
        SR * label_tn = Gen_Label_TN(l, 0);
        Replace_Branch_Label(br_or, label_tn);
    }
}


/*
Generate code for BB3 and target label for BB5
TRUEBR to bb5 if iv > UB
*/
static void Fill_BB3(OUT ORBB * bb3, IN OUT ORBB * bb5,
                     IN SR * ub_tn, IN SR * iv)
{
    ORList ors;

    //Generate condition statement to determine whether
    //if the main loop execute.
    LABEL_IDX l2 = Gen_Label_For_BB(bb5);
    INT trip_size = TN_size(ub_tn);
    UINT branch_cond = trip_size == 4 ? V_BR_I4GT : V_BR_I8GT;
    Exp_OP3v(OPC_TRUEBR, NULL,
             Gen_Label_TN(l2, 0), iv, ub_tn, branch_cond, &ors);
    BB_Append_Ops(bb3, &ors);
}


void Replace_Branch_Cond(OR * br_or, SR * cond)
{
    ASSERT0(SR_is_pred(cond));
    switch (OR_code(br_or)) {
    case OR_b_b:
        br_or->set_pred(cond);
        break;
    default:
        ;ASSERT(0, ("unknown branch operation"));
    }
}


void Replace_Branch_Label(OR * br_or, SR * label_tn)
{
    switch (OR_code(br_or)) {
    case OR_b_b:
        br_or->set_opnd(1, label_tn);
        break;
    default:
        ;ASSERT(0, ("unknown branch operation"));
    }
}


//Record/clear the referrence mark of GSR.
static void Mark_Gtn(ORBB const* bb, bool is_clear)
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
        }//end for OPND
    }//end for OR
}


//Update original bb loop info.
static void Modify_BB(IN OUT ORBB * bb,
                    SR * trip_count_tn,
                    UINT num_para_part)
{
    LOOPINFO * orig_loop_info = Get_LoopInfo(bb);
    WN * orig_loop_info_wn = LOOPINFO_wn(orig_loop_info);
    //Generate new estimate WN of trip count info for ORBB.
    WN_loop_trip_est(orig_loop_info_wn) =
            WN_loop_trip_est(orig_loop_info_wn) -
                WN_loop_trip_est(orig_loop_info_wn) %
                num_para_part;

    //trip_count = orig_trip_count - orig_trip_count % npart
    OPCODE opc_intconst =
            OPCODE_make_op(OPR_INTCONST,
                           WN_rtype(WN_loop_trip(orig_loop_info_wn)), MTYPE_V);
    if (TN_is_constant(trip_count_tn)) {
        INT new_trip_count_val =
            SR_int_imm(trip_count_tn) - SR_int_imm(trip_count_tn) % num_para_part;
        LOOPINFO_trip_count_tn(orig_loop_info) =
            m_cg->gen_imm(new_trip_count_val, TN_size(trip_count_tn));
        //Generate new WN to describe actually trip count.
        WN * new_trip_count_wn = WN_CreateIntconst(opc_intconst, new_trip_count_val);
        WN_set_loop_trip(orig_loop_info_wn, new_trip_count_wn);
    } else {
        LOOPINFO_trip_count_tn(orig_loop_info) = NULL;
        /*
        //Information to regenerate trip count WN was lost!
        //Cannot generate the WN.
        OPCODE opc_mod =
            OPCODE_make_op(OPR_MOD,
                           WN_rtype(WN_loop_trip(orig_loop_info_wn)), MTYPE_V);
        OPCODE opc_sub =
            OPCODE_make_op(OPR_SUB,
                           WN_rtype(WN_loop_trip(orig_loop_info_wn)), MTYPE_V);
        WN * mod_wn =
            WN_CreateExp2(opc_mod,
                          WN_COPY_Tree(WN_loop_trip(orig_loop_info_wn)),
                          WN_CreateIntconst(opc_intconst, num_para_part));
        //Generate new WN to describe actually trip count.
        WN * new_trip_count_wn =
            WN_CreateExp2(opc_sub,
                          WN_COPY_Tree(WN_loop_trip(orig_loop_info_wn)), mod_wn);
        WN_set_loop_trip(orig_loop_info_wn, new_trip_count_wn);
        */
    }
}


/*
Looking for the sr that recording low bound of
induction variable of DO-LOOP.
'bb': the ors generated to compute upper bound will inserted into bb.
*/
static SR * Gen_UpperBound(IN SR * iv, IN OR * cmp_or, IN OUT ORBB * bb)
{
    SR * ub_tn = NULL;
    ORList ors;
    if (OR_is_eq(cmp_or)) {
        /*
        LOOP MODE:
            iv = LB
            do {
                ...
                iv = iv + 1
            } while (FALSEBR(iv == VAL))
        So ub = VAL - 1
        */
        ub_tn = Dup_TN(iv);
        SR * val_tn = NULL;
        if (cmp_or->get_opnd( 1) == iv) {
            val_tn = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            val_tn = cmp_or->get_opnd( 1);
        } else {
            ASSERT(0, ("Incomplete o"));
        }
        IOC tmp;
        m_cg->buildSub(
                        val_tn,
                        m_cg->gen_imm(1),
                        TN_size(ub_tn),
                        true,
                        ors,
                        tmp);
        ub_tn = tmp.get_reg(0);
        BB_Append_Ops(bb, &ors);
    } else if (m_cg->is_lt(cmp_or)) {
        /*
        LOOP MODE:
            iv = LB
            do {
                ...
                iv = iv + 1
            } while (iv < VAL)
        So ub = VAL - 1
        */
        ub_tn = Dup_TN(iv);
        SR * val_tn = NULL;
        if (cmp_or->get_opnd( 1) == iv) {
            val_tn = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            val_tn = cmp_or->get_opnd( 1);
        } else {
            ASSERT(0, ("Incomplete o"));
        }
        IOC tmp;
        m_cg->buildSub(
                    val_tn,
                    m_cg->gen_imm(1),
                    TN_size(ub_tn),
                    true,
                    ors,
                    tmp);
        ub_tn = tmp.get_reg(0);
        BB_Append_Ops(bb, &ors);
    } else if (m_cg->is_gt(cmp_or)) {
        /*
        LOOP MODE:
            iv = LB
            do {
                ...
                iv = iv + 1
            } while (VAL > iv)
        ub = VAL - 1

        o

            iv = LB
            do {
                ...
                iv = iv + 1
            } while (FALSEBR(iv > VAL))
        That is to say iv <= VAL
        ub = VAL
        */
        if (cmp_or->get_opnd( 1) == iv) {
            ub_tn = cmp_or->get_opnd( 2);
        } else if (cmp_or->get_opnd( 2) == iv) {
            ub_tn = Dup_TN(iv);
            SR * val_tn = cmp_or->get_opnd( 1);
            I2O tmp;
            m_cg->buildSub(
                        val_tn,
                        m_cg->gen_imm(1),
                        TN_size(ub_tn),
                        true,
                        ors);
            ub_tn = tmp.get_reg(0);
            BB_Append_Ops(bb, &ors);
        } else {
            ASSERT(0, ("Incomplete o"));
        }
    } else {
        ASSERT(0, ("Unsupport!"));
    }
    return ub_tn;
}


/*
Looking for the sr that recording low bound of
induction variable of DO-LOOP.
'bb': DO-LOOP body
'imm_pred': immediate predecessor of 'bb'.
*/
static SR * Gen_LowBound(SR * iv, ORBB * bb, ORBB * imm_pred)
{
    SR * lb_tn = NULL;
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
        lb_tn = Dup_TN(def_iv->get_opnd( 1));
        ASSERT0(TN_is_constant(lb_tn));
        if ((UINT)SR_int_imm(lb_tn) > (UINT)0x7fffFFFF) {
            ASSERT(0, ("Low bound is too large!"));
        }
    } else if (m_cg->isCopyOR(def_iv)) {
        INT i = m_cg->computeCopyOpndIdx(def_iv);
        SR * def_iv_cp_opnd = def_iv->get_opnd( i);
        lb_tn = Dup_TN(def_iv_cp_opnd);
        ASSERT0(SR_is_reg(lb_tn));

        ORList ors;
        CLUST clst = m_cg->computeORCluster(def_iv);
        UNIT unit = m_cg->computeORUnit(def_iv)->checkAndGet();
        m_cg->buildCopyPred(clst, unit, def_iv_cp_opnd,
            lb_tn, m_cg->genTruePred(), ors);
        ORCt * ct;
        ORBB_orlist(imm_pred).find(def_iv, &ct);
        ASSERT0(ct);
        ORBB_orlist(imm_pred).insert_after(ors, def_iv);
        ORBB_cg(m_bb)->set_sr_liveout(imm_pred, lb_tn);
    } else {
        //TODO: Recog more operaionts
        return NULL;
    }
    return lb_tn;
}


/*
Computing trip_count = UpperBound - LowBound + 1.
'bb': insert ors which computing trip count into ORBB.
*/
static SR * Gen_Trip_Count(IN SR * lb_tn, IN SR * ub_tn, IN OUT ORBB * bb)
{
    SR * trip_count_tn = NULL;
    if (SR_is_int_imm(lb_tn) && SR_is_int_imm(ub_tn)) {
        ASSERT0(TN_size(lb_tn) == TN_size(ub_tn));
        trip_count_tn =
            m_cg->gen_imm(SR_int_imm(ub_tn) - SR_int_imm(lb_tn) + 1,
                            TN_size(lb_tn));
        return trip_count_tn;
    } else if (!TN_is_constant(lb_tn)) {
        trip_count_tn = Dup_TN(lb_tn);
    } else if (!TN_is_constant(ub_tn)) {
        trip_count_tn = Dup_TN(ub_tn);
    } else {
        UNREACHABLE();
    }

    ORList ors;
    IOC tmp;
    m_cg->buildSub(
                ub_tn, lb_tn,
                TN_size(trip_count_tn), true, ors, &tmp);
    trip_count_tn = tmp.get_reg(0);
    tmp.clean();
    m_cg->buildAdd(
                trip_count_tn,
                m_cg->gen_imm(1),
                TN_size(trip_count_tn),
                true,
                ors,
                &tmp);
    trip_count_tn = tmp.get_reg(0);
    BB_Append_Ops(bb, &ors);
    return trip_count_tn;
}


/*
Splitting parallelizable 'bb' into two part, the second part
is a parallel loop body whose trip count can be divide by
the number of parallel part, and the first part is remainder loop.
    Given original loop:
        j = 0
        do {
            ...
            j = j + 1
        } while (j != 9)
        ...
    Output will be:
        ***BB1**
        cmp j < 1 //1 is remainder loop upper bound.
        B_if_false L1
        ***BB2**
        do {
            ...
            j = j + 1
        } while (j < 1) //Remainder loop
        ***BB3**
        L1:
        cmp j < 9 //9 is trip_count
        B_if_fasle L2
        ***orig ORBB**
        do {
            ...
            j = j + 1
        } while (j != 9) //Main unroll loop
        ***BB5**
        L2:
        ********
        ...
*/
static bool Loop_Peeling(IN ORBB * bb,
                        OUT ORBB ** remainder_bb,
                        OUT ORBB ** parallel_bb,
                        OR ** red_or,
                        OR ** cmp_or,
                        SR ** iv,
                        UINT num_para_part)
{
    ASSERT0(remainder_bb && parallel_bb && red_or && cmp_or && iv);
    SR * orig_trip_count_tn = Get_TripCount_Tn(bb);
    if (TN_is_constant(orig_trip_count_tn)) {
        if (SR_int_imm(orig_trip_count_tn) % num_para_part == 0) {
            *remainder_bb = NULL;
            *parallel_bb = bb;

            //Clear parallel flag lest the bb be optimized
            //accidently in other phase.
            WN * loop_info_wn = LOOPINFO_wn(Get_LoopInfo(bb));
            REMOVE_FLAG(WN_loop_flag(loop_info_wn), WN_LOOP_PARALLELIZABLE);
            WN_loop_flag(loop_info_wn) |= WN_LOOP_PARALLELIZED;
            return true;
        }
        if (BB_xfer_op(bb) == NULL) {
            //No a loop?
            return false;
        }
    }

    //Get pred and succ in CFG of bb.
    ORBB * orig_pred = NULL, * orig_succ = NULL;
    Get_Loop_Unique_PredSucc(bb, &orig_pred, &orig_succ);
    ASSERT0(orig_pred && orig_succ);

    ORBB * bb1 = Gen_BB();
    ORBB * bb2 = Dup_Bb(bb, true);
    ORBB * bb3 = Gen_BB();
    ORBB * bb5 = Gen_BB();

    PACDDG ddg;
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
    ASSERT(*red_or && *iv && *cmp_or && SR_is_global(*iv),
            ("Cannot find iv"));
    ddg.destroy();

    //Looking for the sr that recording low bound of
    //induction variable of DO-LOOP.
    //Inserting COPY o in immediate predecessor.
    SR * lb_tn = Gen_LowBound(*iv, bb, orig_pred);
    if (!lb_tn) {
        return false;
    }

    /*
    Inserting code in BB1 to compute trip count.
    LB <= iv <= UB
    trip_count = UB - LB + 1
    remained_trip_count = trip_count % num_para_part.
    Then UB of remaineder loop equals LB + remained_trip_count,
    named as UBr.
    In BB1, if iv >= UBr, branch to BB3.
    And both UB and UBr lives out of BB1, that used by BB2 and BB3.
    In BB2, if iv < UBr(o iv == UBr), goto remainder loop body.
    In BB3, if iv > UB, then branch to BB5, o else fall through to ORBB.
    */
    SR * ub_tn = Gen_UpperBound(*iv, *cmp_or, bb1);
    ASSERT0(ub_tn);
    if (SR_is_reg(ub_tn)) {
        Alloc_Local_Liveness_Info(bb1);
        ORBB_cg(m_bb)->set_sr_liveout(bb1, ub_tn);
    }
    SR * trip_count_tn = Gen_Trip_Count(lb_tn, ub_tn, bb1);
    if (TN_is_constant(orig_trip_count_tn)) {
        ASSERT0(SR_is_int_imm(orig_trip_count_tn));
        ASSERT0(SR_int_imm(trip_count_tn) == SR_int_imm(orig_trip_count_tn));
    }

    /*
    Padding BB1, BB2, BB3, BB5.
    */
    SR * remaining_count_tn = NULL;
    Fill_BB1(bb1, bb, bb3, num_para_part, *iv,
             &remaining_count_tn, trip_count_tn, lb_tn, *red_or);
    Modify_BB2(bb2, bb, remaining_count_tn, num_para_part);
    if (TN_is_constant(trip_count_tn) &&
        TN_is_constant(remaining_count_tn)) {
        ASSERT0(SR_int_imm(trip_count_tn) >= SR_int_imm(remaining_count_tn));
        if (SR_int_imm(trip_count_tn) == SR_int_imm(remaining_count_tn)) {
            Clear_BB(bb);
        } else {
            Modify_BB(bb, trip_count_tn, num_para_part);
        }
    } else {
        if (SR_is_reg(ub_tn)) {
            Alloc_Local_Liveness_Info(bb3);
            bb3->set_livein(ub_tn);
        }
        Fill_BB3(bb3, bb5, ub_tn, *iv);
        Modify_BB(bb, trip_count_tn, num_para_part);
    }

    //Revise control flow.
    Unlink_Pred_Succ(orig_pred, bb);
    Unlink_Pred_Succ(bb, orig_succ);

    Link_Pred_Succ(orig_pred, bb1); //fall through
    Link_Pred_Succ(bb1, bb2); //fall through
    Link_Pred_Succ(bb2, bb3); //fall through
    Link_Pred_Succ(bb3, bb); //fall through
    Link_Pred_Succ(bb, bb5); //fall through
    Link_Pred_Succ(bb5, orig_succ); //fall through

    Link_Pred_Succ(bb1, bb3); //false branch
    Link_Pred_Succ(bb2, bb2); //loop back, remainder loop
    Link_Pred_Succ(bb3, bb5); //false branch

    //Add BBs into region
    Insert_BB(bb1, orig_pred);
    Insert_BB(bb2, bb1);
    Insert_BB(bb3, bb2);
    Insert_BB(bb5, bb);

    Mark_Gtn(bb1, false);
    Mark_Gtn(bb2, false);
    Mark_Gtn(bb3, false);
    Mark_Gtn(bb, false);
    Mark_Gtn(bb5, false);

    //Clear parallel flag lest the bb be optimized accidently in other phase.
    LOOPINFO * li = Get_LoopInfo(bb);
    if (li) {
        WN * loop_info_wn = LOOPINFO_wn(Get_LoopInfo(bb));
        REMOVE_FLAG(WN_loop_flag(loop_info_wn), WN_LOOP_PARALLELIZABLE);
        WN_loop_flag(loop_info_wn) |= WN_LOOP_PARALLELIZED;
    }
    li = Get_LoopInfo(bb2);
    if (li) {
        WN * loop_info_wn = LOOPINFO_wn(Get_LoopInfo(bb2));
        REMOVE_FLAG(WN_loop_flag(loop_info_wn), WN_LOOP_PARALLELIZABLE);
    }

    //Output for caller.
    *remainder_bb = bb2;
    *parallel_bb = bb;
    return true;
}


LOOP_DESCR * Find_LoopDesc(ORBB const* bb)
{
    for (LOOP_DESCR * loop_desc = g_loopdesc_list.get_head(); loop_desc;
         loop_desc = g_loopdesc_list.get_next()) {
        if (bb == LOOP_DESCR_loophead(loop_desc)) {
            return loop_desc;
        }
    }
    return NULL;
}


static bool Is_Para_MultiBB(LOOP_DESCR * loop_desc)
{
    return false;
}


static bool Is_Para_BB(ORBB * bb)
{
    return true;
}


/*
Perform parallelism optimization for one ORBB.
Control flow will change if perform parallelism successfully.
*/
static bool Parallel_BB(ORBB * bb, ParallelPartMgrVec & ppm_vec)
{
    WN * loop_info_wn = NULL;
    ANNOTATION * annot = ANNOT_Get(BB_annotations(bb), ANNOT_LOOPINFO);
    if (annot) {
        LOOPINFO * loop_info = ANNOT_loopinfo(annot);
        ASSERT0(loop_info);
        loop_info_wn = LOOPINFO_wn(loop_info);
        ASSERT0(loop_info_wn);
        if (!WN_Loop_Innermost(loop_info_wn) ||
            //!WN_Loop_Nz_Trip(loop_info_wn) ||
            !WN_Loop_Parallelizable(loop_info_wn)) {
            return false;
        }
    } else {
        return false;
    }

    #ifdef _DEBUG_
    fprintf(g_tfile, "\n*************************************\n");
    fprintf(g_tfile, "\nPU:%s:ORBB%d:", Cur_PU_Name, bb->id);
    #endif

    if (!Is_Para_BB(bb)) {
        //return false;
    }
    LOOP_DESCR * loop_desc = Find_LoopDesc(bb);
    if (loop_desc) {
        BS * bbset = LOOP_DESCR_bbset(loop_desc);
        ASSERT0(bbset);
        bool multi_bb = (BB_SET_Size(bbset) > 1);
        if (multi_bb) {
            if (!Is_Para_MultiBB(loop_desc)) {
                #ifdef _DEBUG_
                fprintf(g_tfile, "\nMulti-ORBB do not allowed currently!!! Loop Header:ORBB%d\n", bb->id);
                #endif
                return false;
            }
            ORBB * new_single_bb = Force_If_Convert(loop_desc, false);
            if (new_single_bb == NULL) {
                REMOVE_FLAG(WN_loop_flag(loop_info_wn), WN_LOOP_PARALLELIZABLE);
                #ifdef _DEBUG_
                fprintf(g_tfile, "\nIf-Conversion for Multi-ORBB failed!!! Loop Header:ORBB%d\n", bb->id);
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
    if (!Loop_Peeling(bb, &rem_bb, &para_bb,
                    &red_or, &cmp_or, &iv,
                    ppm->numOfParallelPart())) {
        #ifdef _DEBUG_
        fprintf(g_tfile, "\nLoop_Peeling() failed!!! ORBB%d\n", bb->id);
        #endif
        return false;
    }

    //Reanalysis gsr info
    GRA_LIVE_Init(BB_rid(REGION_First_BB));
    if (para_bb == NULL) {
        #ifdef _DEBUG_
        fprintf(g_tfile, "\nAfter Loop_Peeling(ORBB%d), para_bb is NULL?!\n", bb->id);
        #endif
        return false;
    }
    Free_Dominators_Memory();
    Calculate_Dominators(); //used by genPrologAndEpilog()
    if (!Is_Para_BB(bb)) {
        return true;
    }
    ppm->setBB(para_bb);
    if (ppm->prepare_distribute(red_or, cmp_or, iv)) {
        if (!ppm->distribute()) {
            UNREACHABLE();
        }
    }
    #ifdef _DEBUG_
    fprintf(g_tfile, "\nParallellizing Success!! Dump all of OR:\n");
    for (OR * o = ORBB_orlist(para_bb).get_head(); o;
         o = ORBB_orlist(para_bb).get_next()) {
        fprintf(g_tfile, "\t");
        Print_OP_No_SrcLine(o);
    }
    fprintf(g_tfile, "\n\n\n");
    #endif
    return true;
}


//Return the unique predecessor and successor for natural loop with single ORBB.
bool Get_Loop_Unique_PredSucc(ORBB * bb, ORBB ** pred, ORBB ** succ)
{
    ASSERT(BB_loop_head_bb(bb) == bb, ("bb must be loop with single ORBB"));
    INT count = 0;
    ORBB * orig_pred = NULL, * orig_succ = NULL;
    for (BBLIST * bblist = BB_preds(bb); bblist;
         bblist = BBLIST_next(bblist)) {
        count++;
        ORBB * tmp = BBLIST_item(bblist);;
        if (tmp != bb) {
            orig_pred = tmp;
        }
    }
    if (NULL == orig_pred || count != 2) {
        return false;
    }
    count = 0;
    for (BBLIST * bblist = BB_succs(bb); bblist;
         bblist = BBLIST_next(bblist)) {
        count++;
        ORBB * tmp = BBLIST_item(bblist);;
        if (tmp != bb) {
            orig_succ = tmp;
        }
    }
    if (NULL == orig_succ || count != 2) {
        return false;
    }
    *pred = orig_pred;
    *succ = orig_succ;
    return true;
}


bool Change_OPS_FuncUnit(ORList & ors,
                        CLUST to_clust,
                        Vector<bool> & regfile_unique)
{
    for (OR * o = ors.get_head(); o; o = ors.get_next()) {
        if (!m_cg->changeORCluster(o,
                                            to_clust,
                                            regfile_unique,
                                            false)) {
            return false;
        }
    }//end for
    return true;
}


OR * Dup_Op(OR * o)
{
    OR * new_or = m_cg->gen_or();
    new_or->clone(o);
    //Which MEM_POOL utilized during the duplication?
    //May be there is a memleak.
    Copy_WN_For_Memory_OP(new_or, o);
    return new_or;
}


void Clear_BB(ORBB * bb)
{
    BB_Remove_All(bb);
}


ORBB * Dup_Bb(ORBB * bb, bool rename_lcl_tn)
{
    SR2SR_DMAP dmap;
    ORBB * newBB = Gen_BB();

    for (OR * o = ORBB_orlist(bb).get_head(); o;
         o = ORBB_orlist(bb).get_next()) {
        OR * new_or = Dup_OP(o);
        Copy_WN_For_Memory_OP(new_or, o);
        if (rename_lcl_tn) {
            for (INT i = 0;  i < new_or->result_num(); i++) {
                SR * res = new_or->get_result(i);
                if (SR_is_reg(res) &&
                    !SR_is_dedicated(res) &&
                    !SR_is_global(res)) {
                    SR * newtn = Dup_TN(res);
                    dmap.setAlways(res, newtn);

                    //Rename result sr of o for parallel part.
                    new_or->set_result(i, newtn);
                }
            }//end for
            for (INT i = 0; i < new_or->opnd_num(); i++) {
                SR * opnd = new_or->get_opnd( i);
                if (SR_is_reg(opnd) &&
                    !SR_is_dedicated(opnd) &&
                    !SR_is_global(opnd)) {
                    SR * newtn = dmap.get(opnd);
                    ASSERT(newtn, ("local sr must have DEF"));
                    //Rename operand sr of o for parallel part.
                    new_or->set_opnd(i, newtn);
                }
            }//end for
        }//end if
        BB_Append_Op(newBB, new_or);
    }
    return newBB;
}
#endif


#if 0
//Parallel BBs which marked by LNO with flag of 'PARALLELIZABLE'.
void parallel_inner_loop(ParallelPartMgrVec & ppm_vec)
{
    g_loopdesc_list.init();
    g_sr2bbset_map.init();
    GRA_LIVE_Init(BB_rid(REGION_First_BB));
    //MEM_POOL_Push(&MEM_local_pool);

    Calculate_Dominators(); //needed for loop recognition
    for (LOOP_DESCR * loop = LOOP_DESCR_Detect_Loops(&m_pool);
        loop;
        loop = LOOP_DESCR_next(loop)) {
        g_loopdesc_list.append_tail(loop);
    }

    //Pool initializing at 'CG_PU_Initialize(WN *wn_pu)'

    //Record occurrence of GSR.
    //TODO: Should build dataflow graph.
    Show_Phase("---\tMark_Gtn() in CG_PARA_PART", NULL);
    for (ORBB * bb = REGION_First_BB; bb != NULL; bb = BB_next(bb)) {
        Mark_Gtn(bb, false);
    }//end for ORBB

    ORBB * next;
    for (ORBB * bb = REGION_First_BB; bb != NULL; bb = next) {
        next = BB_next(bb);
        Show_Phase("---\tParallel_BB() in CG_PARA_PART", bb);
        if (Parallel_BB(bb, ppm_vec)) {
            next = REGION_First_BB;
            continue;
        }
    }

    #ifdef _DEBUG_
    fprintf(g_tfile, "Finish CG Parallelizing of PU");
    fprintf(g_tfile, "\n*************************************\n\n\n\n");
    #endif

    Show_Phase("---\tEnd of CG_Parallel_Inner_Loop()", NULL);
    g_loopdesc_list.destroy();
    g_sr2bbset_map.destroy();

    Free_Dominators_Memory();
    //MEM_POOL_Pop(&MEM_local_pool);
    MEM_POOL_Pop(&g_orc_lcl_pool);
    MEM_POOL_Delete(&g_orc_lcl_pool);
    //GRA_LIVE_Finish_PU();
}
#endif

} //namespace xgen
