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
#ifndef _INSTRUCTION_PARTITION_H_
#define _INSTRUCTION_PARTITION_H_

namespace xgen {

//
//Instructions Partition
//
template <class Mat, class T> class InstructionPartition {
    COPY_CONSTRUCTOR(InstructionPartition);

    CG * m_cg;
    ORBB * m_bb;
    RegFileSet * m_is_regfile_unique;
    DataDepGraph * m_ddg;
public:
    explicit InstructionPartition(CG * cg, ORBB * bb, DataDepGraph * ddg,
                                  RegFileSet * is_regfile_unique)
    { m_cg = cg; m_ddg = ddg; m_is_regfile_unique = is_regfile_unique; }

    bool partition();
    void formulateTargetFunction(OUT Mat & tgtf,
                                 IN VarMap & vm,
                                 IN OR * last_sr,
                                 UINT num_cycs,
                                 UINT num_ors,
                                 UINT num_vars,
                                 UINT num_cst);
    void formulateMustScheduleConstraints(OUT Mat & eq,
                                         IN VarMap & vm,
                                         UINT num_cycs,
                                         UINT num_ors,
                                         UINT num_vars,
                                         UINT num_cst);
    void formulateDependenceConstraints(OUT Mat & leq,
                                       IN VarMap & vm,
                                       IN BBSimulator & sim,
                                       UINT num_cycs,
                                       UINT num_ors,
                                       UINT num_vars,
                                       UINT num_cst);
    void formulateIssueConstraints(OUT Mat & leq,
                                  IN VarMap & vm,
                                  UINT num_cycs,
                                  UINT num_ors,
                                  UINT num_vars);
    void formulateInterClusterConstraints(OUT Mat & leq,
                                         OUT Mat & eq,
                                         OUT UINT & num_icc_vars,
                                         IN VarMap & vm,
                                         UINT num_cycs,
                                         UINT num_ors,
                                         UINT num_vars,
                                         UINT num_cst);
    void format(OUT INTMat & sched_form,
                OUT INTMat & icc_form,
                IN VarMap & vm,
                IN Mat & sol,
                UINT num_cycs,
                UINT num_icc_vars,
                UINT num_sr_vars,
                INT cst_col);
};


template <class Mat, class T>
void InstructionPartition<Mat, T>::formulateTargetFunction(OUT Mat & tgtf,
                                                           IN VarMap & vm,
                                                           IN OR * last_op,
                                                           UINT num_cycs,
                                                           UINT num_ops,
                                                           UINT num_vars,
                                                           UINT num_cst)
{
    DUMMYUSE(num_ops);

    ASSERTN(num_cst == 1, ("multi-const terms are unsupport till now"));
    INT cst_col = num_vars;
    tgtf.reinit(1, num_vars + num_cst);
    for (INT c = 0; c < (INT)num_cycs; c++) {
        tgtf.set(0, vm.map_or_cyc2varidx(last_op->id(), c), c);
    }

    //tgtf.setAllElem(1);
    tgtf.set(0, cst_col, 0);
}


//The equation constraint should be:SIGMA(o(i)) = 1, i=0..NUM_OPS,
//for each cycles.
template <class Mat, class T>
void InstructionPartition<Mat, T>::formulateMustScheduleConstraints(
    OUT Mat & eq, IN VarMap & vm, UINT num_cycs, UINT num_ops,
    UINT num_vars, UINT num_cst)
{
    DUMMYUSE(num_ops);
    ASSERTN(num_cst == 1, ("multi-const terms are unsupport till now"));
    INT cst_col = num_vars;
    for (OR * o = ORBB_first_or(m_bb);
         o != nullptr; o = ORBB_next_or(m_bb)) {
        Mat tmp_eq(1, num_vars + num_cst);
        for (INT c = 0; c < (INT)num_cycs; c++) {
            tmp_eq.set(0, vm.map_or_cyc2varidx(o->id(), c), 1);
        }
        tmp_eq.set(0, cst_col, 1);
        if (eq.size() == 0) {
            eq = tmp_eq;
        } else {
            eq.growRow(tmp_eq);
        }
    }
}


template <class Mat, class T>
void InstructionPartition<Mat, T>::formulateIssueConstraints(OUT Mat & leq,
                                                             IN VarMap & vm,
                                                             UINT num_cycs,
                                                             UINT num_ops,
                                                             UINT num_vars)
{
    UINT cst_col = num_vars;
    Mat iss_cs(num_cycs, leq.getColSize());
    for (INT c = 0; c < (INT)num_cycs; c++) {
        for (UINT i = 0; i < num_ops; i++) {
            OR * o = vm.map_vecidx2or(i);
            UINT varidx = vm.map_or_cyc2varidx(o->id(), c);
            ASSERT0(varidx < cst_col);
            iss_cs.set(c, varidx, 1);
        }
        iss_cs.set(c, cst_col, vm.get_issue_port_per_clust());
    }
    if (leq.size() == 0) {
        leq = iss_cs;
    } else {
        leq.growRow(iss_cs, 0, iss_cs.getRowSize() - 1);
    }
}


//Given a dependent edge xi->xj, the constraint is as the
//folloing form:
//    SIGMA(i * xi) + Latency(i,j) + 1 <= SIGMA (j * xj),
//    where i=(cycle0, cycle1,...)
template <class Mat, class T>
void InstructionPartition<Mat, T>::formulateDependenceConstraints(
    OUT Mat & leq, IN VarMap & vm, IN BBSimulator & sim, UINT num_cycs,
    UINT num_ops, UINT num_vars, UINT num_cst)
{
    DUMMYUSE(num_ops);
    DUMMYUSE(sim);
    INT cst_col = num_vars;
    //Dependency restrictions.
    EdgeIter c;
    for (xcom::Edge * e = m_ddg->get_first_edge(c);
         e != nullptr; e = m_ddg->get_next_edge(c)) {
        UINT opi = VERTEX_id(EDGE_from(e));
        UINT opj = VERTEX_id(EDGE_to(e));
        Mat tmp_leq(1, num_vars + num_cst);
        for (INT c2 = 0; c2 < (INT)num_cycs; c2++) {
            tmp_leq.set(0, vm.map_or_cyc2varidx(opi, c2), c2);
        }
        for (INT c2 = 0; c2 < (INT)num_cycs; c2++) {
            tmp_leq.set(0, vm.map_or_cyc2varidx(opj, c2), -c2);
        }
        OR * o = m_cg->getOR(opi);
        ASSERT0_DUMMYUSE(o);

        //tmp_leq.set(0, cst_col, -(sim.getMinLatency(o) + 1));
        tmp_leq.set(0, cst_col, -1);
        if (leq.size() == 0) {
            leq = tmp_leq;
        } else {
            leq.growRow(tmp_leq);
        }
    }

    //Estart and Lstart restrictions.
    //for (OR * o = ORBB_first_or(bb); o != nullptr;
    //     o = ORBB_next_or(bb)) {
    //    UINT estart, lstart;
    //    m_ddg->getEstartAndLstart(estart, lstart, o);
    //    ASSERT0(estart <= lstart);
    //
    //    Mat tmp_leq(1, num_vars + num_cst);
    //    //SIGMA(j*xj) >= estart
    //    for (INT c = 0; c < (INT)num_cycs; c++) {
    //        tmp_leq.set(0, vm.map_or_cyc2varidx(opi, c), -c);
    //    }
    //    tmp_leq.set(0, cst_col, -estart);
    //    if (leq.size() == 0) {
    //        leq = tmp_leq;
    //    } else {
    //        leq.growRow(tmp_leq);
    //    }
    //
    //    //SIGMA(j*xj) <= lstart
    //    tmp_leq.zero();
    //    for (INT c = 0; c < (INT)num_cycs; c++) {
    //        tmp_leq.set(0, vm.map_or_cyc2varidx(opi, c), c);
    //    }
    //    tmp_leq.set(0, cst_col, lstart);
    //    leq.growRow(tmp_leq);
    //}
}


template <class Mat, class T>
void InstructionPartition<Mat, T>::formulateInterClusterConstraints(
    OUT Mat & leq, OUT Mat & eq, OUT UINT & num_icc_vars,
    IN VarMap & vm, UINT num_cycs, UINT num_ops, UINT num_vars, UINT num_cst)
{
    INT cst_col = num_vars;
    INT start_idx_of_new_cs = cst_col;
    INT num_new_cs_of_each_edge = vm.get_clust_num() * vm.get_clust_num();

    num_icc_vars = m_ddg->getEdgeNum() * num_new_cs_of_each_edge;
    leq.insertColumnsBefore(cst_col, num_icc_vars);
    eq.insertColumnsBefore(cst_col, num_icc_vars);

    INT new_cst_col = num_vars + num_icc_vars;
    UINT ofst = start_idx_of_new_cs;
    for (UINT i = 0; i < m_ddg->getEdgeNum(); i++) {
        for (UINT j = 0; j < num_new_cs_of_each_edge; j++) {
            UINT c = vm.map_icc_varidx2coeff(j);
            leq.set(i, ofst + j, c);
        }

        Mat tmp_eq(1, eq.getColSize());
        for (UINT j = 0; j < num_new_cs_of_each_edge; j++) {
            tmp_eq.set(0, ofst + j, 1);
        }
        tmp_eq.set(0, new_cst_col, 1);
        eq.growRow(tmp_eq);

        ofst += num_new_cs_of_each_edge;
    }

}


template <class Mat, class T>
void InstructionPartition<Mat, T>::format(OUT INTMat & sched_form,
                                          OUT INTMat & icc_form,
                                          IN VarMap & vm,
                                          IN Mat & sol,
                                          UINT num_cycs,
                                          UINT num_icc_vars,
                                          UINT num_op_vars,
                                          INT cst_col)
{
    DUMMYUSE(cst_col);

    ASSERT0(sol.getRowSize() == 1);
    sched_form.reinit(num_cycs, ORBB_ornum(m_bb));
    for (UINT j = 0; j < num_op_vars; j++) {
        OR * o = nullptr;
        UINT cyc = 0;
        vm.map_varidx2or_cyc(j, o, cyc);
        UINT vecidx = vm.map_or2vecidx(o);
        if (sol.get(0, j) == 0) {
            sched_form.set(cyc, vecidx, 0);
        } else {
            sched_form.set(cyc, vecidx, o->id());
        }
    }

    Mat op_form(num_cycs, vm.get_issue_port_per_clust());
    op_form.setAllElem(-1);
    for (INT c = 0; c < (INT)num_cycs; c++) {
        UINT pos = 0;
        for (UINT j = 0; j < ORBB_ornum(m_bb); j++) {
            OR * o = vm.map_vecidx2or(j);
            ASSERT0(o);
            UINT varidx = vm.map_or_cyc2varidx(o->id(), c);
            if (sol.get(0, varidx) == 1) {
                ASSERT0(pos < vm.get_issue_port_per_clust());
                op_form.set(c, pos, o->id());
                pos++;
            }
        }
    }
    //op_form.dumpf();

    //icc_form to be formed as:
    //col0:from_vertex  col1:to_vertex col2:from_cluster col3:to_cluster
    if (num_icc_vars) {
        icc_form.reinit(m_ddg->getEdgeNum(), 4);
        UINT row = 0;
        EdgeIter c;
        for (xcom::Edge * e = m_ddg->get_first_edge(c);
             e != nullptr; e = m_ddg->get_next_edge(c), row++) {
            UINT opi = VERTEX_id(EDGE_from(e));
            UINT opj = VERTEX_id(EDGE_to(e));
            icc_form.set(row, 0, opi);
            icc_form.set(row, 1, opj);

            UINT ofst = num_op_vars;
            INT cut = -1;
            for (UINT i = 0; i < vm.get_clust_num() * vm.get_clust_num();
                 i++) {
                if (sol.get(0, ofst + i) != 0) {
                    ASSERTN(cut == -1,
                        ("more than one variable own value, in 0-1 pragram"));
                    cut = i;
                }
            }
            UINT from_clust = cut / vm.get_clust_num();
            UINT to_clust = cut % vm.get_clust_num();
            icc_form.set(row, 2, from_clust);
            icc_form.set(row, 3, to_clust);
        }
    }
}


template <class Mat, class T>
bool InstructionPartition<Mat, T>::partition()
{
    BBSimulator * sim = m_cg->allocBBSimulator(m_bb);
    LIS * lis = m_cg->allocLIS(m_bb, m_ddg, sim,
                               LIS::SCH_TOP_DOWN | LIS::SCH_CHANGE_SLOT);
    lis->set_unique_regfile(m_is_regfile_unique);
    OR * last_op = nullptr;
    UINT criti_path_len = m_ddg->computeEstartAndLstart(*sim, &last_op);
    bool optimal = false;
    for (;;) {
        sim->reset();
        lis->schedule();
        UINT real_sched_len = sim->getCurCycle();
        if (real_sched_len <= criti_path_len) {
            //We get the optimal solution.
            optimal = true;
            break;
        }

        //UINT num_cycs = real_sched_len - 1;
        UINT num_cycs = 10;
        UINT num_ops = ORBB_ornum(m_bb);
        UINT num_vars = num_cycs * num_ops;
        UINT num_cst = 1;
        UINT cst_col = num_vars;
        VarMap vm(m_bb);

        //Construct target function
        Mat tgtf;
        formulateTargetFunction(tgtf, vm, last_op, num_cycs, num_ops,
                                num_vars, num_cst);

        //Construct equality constraints.
        Mat eq;
        formulateMustScheduleConstraints(eq, vm, num_cycs, num_ops,
                                         num_vars, num_cst);

        //Construct inequality constraints.
        Mat leq;
        formulateDependenceConstraints(leq, vm, *sim, num_cycs, num_ops,
                                       num_vars, num_cst);

        //Issue constraint.
        formulateIssueConstraints(leq, vm, num_cycs, num_ops, num_vars);

        //Construct inter-cluster constraints.
        UINT num_icc_vars = 0;
        //formulateInterClusterConstraints(leq, eq, num_icc_vars, vm,
        //                                 num_cycs, num_ops, num_vars,
        //                                 num_cst);
        if (num_icc_vars != 0) {
            tgtf.insertColumnsBefore(cst_col, num_icc_vars);
            tgtf.setCols(cst_col, cst_col + num_icc_vars, 1);
        }

        //Construct variable constraints.
        Mat vc(num_vars + num_icc_vars, num_vars + num_icc_vars);
        vc.initIden(-1);
        vc.growCol(1);
        //tgtf.dumpf();
        //vc.dumpf();
        //eq.dumpf();
        //leq.dumpf();
        T minv;
        Mat res;
#if 1
        START_TIMER(t, "Instruction Partition");
        SIX<Mat, T> six;
        if (SIX_SUCC != six.minm(minv, res, tgtf, vc, eq, leq,
                                 cst_col + num_icc_vars)) {
            //There is no optimal solution.
            break;
        }
        END_TIMER(t, "Instruction Partition");
        res.dumpf();
#else
        //TODO: enable MIP partition OR into multi-parts.
        //MIP<Mat, T> ip;
        //if (IP_SUCC != ip.minm(minv, res, tgtf, vc, eq, leq,
        //                       true, nullptr, cst_col + num_icc_vars)) {
        //    //There is no optimal solution.
        //    break;
        //}
        //res.dumpf();
#endif
        INTMat sched_form;
        INTMat icc_form;
        format(sched_form,
               icc_form,
               vm,
               res,
               num_cycs,
               num_icc_vars,
               num_vars,
               cst_col + num_icc_vars);
        //sched_form.dumpf();
        //icc_form.dumpf();
    }
    delete sim;
    delete lis;
    return optimal;
}
///END InstructionPartition

} //namespace xgen
#endif
