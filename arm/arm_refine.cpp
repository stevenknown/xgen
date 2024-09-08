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

#include "../xgen/xgeninc.h"

static void dumpInd(StrBuf & buf, UINT ind)
{
    for (UINT i = 0; i < ind; i++) {
        buf.strcat(" ");
    }
}


static void dumpFoldConstTensorOpRes(
    IR const* ir, TenType const* res, RefineCtx const& rc)
{
    Region const* rg = rc.getRegion();
    if (!rg->isLogMgrInit()) { return; }
    rg->getLogMgr()->incIndent(2);
    ASSERT0(res);
    xcom::StrBuf buf(32);
    xoc::dumpIRToBuf(ir, rg, buf,
                     DumpFlag::combineIRID(IR_DUMP_DEF|IR_DUMP_KID));
    //Dump tensor
    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("RES:");
    res->dumpb(buf, rg->getLogMgr()->getIndent());

    //Dump brief.
    ActMgr * am = rc.getActMgr();
    if (am != nullptr) {
        am->dump("FoldConst:%s", buf.getBuf());
        rg->getLogMgr()->decIndent(2);
    }
}


static void dumpFoldConstConvWeightDer(
    IR const* ir, TenType const* input, TenType const* weight,
    TenType const* convgrad, UINT stride_h, UINT stride_w, TenType const* res,
    RefineCtx const& rc)
{
    Region const* rg = rc.getRegion();
    if (!rg->isLogMgrInit()) { return; }
    rg->getLogMgr()->incIndent(2);
    ASSERT0(input && weight && convgrad && res);
    xcom::StrBuf buf(32);
    xoc::dumpIRToBuf(ir, rg, buf,
                     DumpFlag::combineIRID(IR_DUMP_DEF|IR_DUMP_KID));
    //Dump tensor
    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("INPUT:");
    input->dumpb(buf, (UINT)rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("WEIGHT:");
    weight->dumpb(buf, rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("CONVGRAD:");
    convgrad->dumpb(buf, rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("RES:");
    res->dumpb(buf, rg->getLogMgr()->getIndent());

    //Dump brief.
    ActMgr * am = rc.getActMgr();
    if (am != nullptr) {
        am->dump("FoldConst:stride_h=%u,stride_w=%u,%s",
                 stride_h, stride_w, buf.getBuf());
        rg->getLogMgr()->decIndent(2);
    }
}


static void dumpFoldConstConv(
    IR const* ir, TenType const* input, TenType const* weight,
    UINT stride_h, UINT stride_w, TenType const* res,
    RefineCtx const& rc)
{
    Region const* rg = rc.getRegion();
    if (!rg->isLogMgrInit()) { return; }
    rg->getLogMgr()->incIndent(2);
    ASSERT0(input && weight && res);
    xcom::StrBuf buf(32);
    xoc::dumpIRToBuf(ir, rg, buf,
                     DumpFlag::combineIRID(IR_DUMP_DEF|IR_DUMP_KID));
    //Dump tensor
    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("INPUT:");
    input->dumpb(buf, (UINT)rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("WEIGHT:");
    weight->dumpb(buf, rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("RES:");
    res->dumpb(buf, rg->getLogMgr()->getIndent());

    //Dump brief.
    ActMgr * am = rc.getActMgr();
    if (am != nullptr) {
        am->dump("FoldConst:stride_h=%u,stride_w=%u,%s",
                 stride_h, stride_w, buf.getBuf());
        rg->getLogMgr()->decIndent(2);
    }
}


static void dumpFoldConstBinOp(
    IR const* ir, TenType const* op0, TenType const* op1, TenType const* res,
    RefineCtx const& rc)
{
    Region const* rg = rc.getRegion();
    if (!rg->isLogMgrInit()) { return; }
    rg->getLogMgr()->incIndent(2);
    ASSERT0(op0 && op1 && res);
    xcom::StrBuf buf(32);
    xoc::dumpIRToBuf(ir, rg, buf,
                     DumpFlag::combineIRID(IR_DUMP_DEF|IR_DUMP_KID));
    //Dump tensor
    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("OP0:");
    op0->dumpb(buf, (UINT)rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("OP1:");
    op1->dumpb(buf, rg->getLogMgr()->getIndent());

    buf.strcat("\n"); dumpInd(buf, (UINT)rg->getLogMgr()->getIndent());
    buf.strcat("RES:");
    res->dumpb(buf, rg->getLogMgr()->getIndent());

    //Dump brief.
    ActMgr * am = rc.getActMgr();
    if (am != nullptr) {
        am->dump("FoldConst:%s", buf.getBuf());
        rg->getLogMgr()->decIndent(2);
    }
}


IR * ARMRefine::refineRelu(IR * ir, bool & change, RefineCtx & rc)
{
    ASSERT0(ir->isUnaryOp());
    ASSERT0(ir->getCode() == IR_RELU);
    IR * opnd = UNA_opnd(ir);
    if (!opnd->isUInt()) { return ir; }
    IR * tmp = UNA_opnd(ir);
    UNA_opnd(ir) = nullptr;
    copyDbx(tmp, ir, m_rg);
    m_rg->freeIRTree(ir);
    return tmp;
}


IR * ARMRefine::foldConstTrigonometricTensor(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isUnaryOp());
    IR const* opnd = UNA_opnd(ir);
    if (!opnd->is_const()) { return ir; }
    if (!g_do_refine_with_host_api) { return ir; }
    ASSERT0(ir->is_tensor());
    ASSERT0(opnd->is_tensor());
    ASSERTN(ir->getType() == opnd->getType(), ("not same tensor"));
    TensorType const* tty = (TensorType const*)opnd->getType();
    if (tty->getDim() != 2 || !tty->is_int()) { return ir; }
    TenType * tensor_val = (TenType*)CONST_tensor_val(opnd);
    ASSERT0(tensor_val);
    switch (ir->getCode()) {
    case IR_ABS:
        xoc::TenType::abs(*tensor_val, *tensor_val);
        break;
    case IR_SIN:
        xoc::TenType::sin(*tensor_val, *tensor_val);
        break;
    case IR_COS:
        xoc::TenType::cos(*tensor_val, *tensor_val);
        break;
    case IR_TAN:
        xoc::TenType::tan(*tensor_val, *tensor_val);
        break;
    case IR_TANH:
        xoc::TenType::tanh(*tensor_val, *tensor_val);
        break;
    default: UNREACHABLE();
    }
    dumpFoldConstTensorOpRes(ir, tensor_val, rc);
    change = true;
    IR * res = m_rg->getIRMgr()->buildImmTensor(
        (TenVal*)tensor_val, ir->getType());
    m_rg->freeIRTree(ir);
    return res; //No need to update DU.
}


IR * ARMRefine::foldConstBinaryTensorByTensor(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isBinaryOp());
    ASSERT0(ir->is_tensor());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);
    ASSERT0(op0->is_const() && op1->is_const());
    ASSERT0(op0->is_tensor() && op1->is_tensor());
    if (!g_do_refine_with_host_api) { return ir; }
    TensorType const* restty = (TensorType const*)ir->getType();
    if (restty->getDim() != 2) { return ir; }
    TenType * op0_tval = (TenType*)CONST_tensor_val(op0);
    ASSERT0(op0_tval);
    TenType * op1_tval = (TenType*)CONST_tensor_val(op1);
    ASSERT0(op1_tval);
    TenType * t_res = getCalcDer()->genImmTensor(ir->getType());
    switch (ir->getCode()) {
    case IR_MUL:
        ASSERTN(op0_tval->is_homo_shape(*op1_tval), ("invalid Tensor"));
        op0_tval->mulElemWise(*op1_tval, *t_res);
        break;
    case IR_ADD:
        ASSERTN(op0_tval->is_homo_shape(*op1_tval), ("invalid Tensor"));
        xoc::TenType::add(*op0_tval, *op1_tval, *t_res);
        break;
    case IR_SUB:
        ASSERTN(op0_tval->is_homo_shape(*op1_tval), ("invalid Tensor"));
        xoc::TenType::sub(*op0_tval, *op1_tval, *t_res);
        break;
    default: UNREACHABLE();
    }
    dumpFoldConstBinOp(ir, op0_tval, op1_tval, t_res, rc);
    change = true;
    m_rg->freeIRTree(ir);
    IR * res = m_rg->getIRMgr()->buildImmTensor((TenVal*)t_res, restty);
    return res; //No need to update DU.
}


IR * ARMRefine::foldConstBinaryScalarByTensor(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isBinaryOp());
    ASSERT0(ir->is_tensor());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);
    ASSERT0(op0->is_const() && op1->is_const());
    ASSERT0(!op0->is_tensor() && op1->is_tensor());
    if (!g_do_refine_with_host_api) { return ir; }
    TensorType const* restty = (TensorType const*)ir->getType();
    if (restty->getDim() != 2 || !restty->is_int()) { return ir; }
    xoc::TenType * op1_tval = (TenType*)CONST_tensor_val(op1);
    ASSERT0(op1_tval);
    switch (ir->getCode()) {
    case IR_DIV:
        if (op0->is_int()) {
            HOST_FP v = (HOST_FP)CONST_int_val(op0);
            xoc::TenType::div(TenType::EType(v), *op1_tval, *op1_tval);
        } else if (op0->is_fp()) {
            xoc::TenType::div(CONST_fp_val(op0), *op1_tval, *op1_tval);
        } else { UNREACHABLE(); }
        break;
    case IR_MUL:
        if (op0->is_int()) {
            HOST_FP v = (HOST_FP)CONST_int_val(op0);
            xoc::TenType::mul(*op1_tval, TenType::EType(v), *op1_tval);
        } else if (op0->is_fp()) {
            xoc::TenType::mul(*op1_tval, CONST_fp_val(op0), *op1_tval);
        } else { UNREACHABLE(); }
        break;
    case IR_SUB:
        if (op0->is_int()) {
            HOST_FP v = (HOST_FP)CONST_int_val(op0);
            xoc::TenType::sub(TenType::EType(v), *op1_tval, *op1_tval);
        } else if (op0->is_fp()) {
            xoc::TenType::sub(CONST_fp_val(op0), *op1_tval, *op1_tval);
        } else { UNREACHABLE(); }
        break;
    default: UNREACHABLE();
    }
    dumpFoldConstTensorOpRes(ir, op1_tval, rc);
    change = true;
    m_rg->freeIRTree(ir);
    IR * res = m_rg->getIRMgr()->buildImmTensor((TenVal*)op1_tval, restty);
    return res; //No need to update DU.
}


IR * ARMRefine::foldConstBinaryTensorByScalar(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isBinaryOp());
    ASSERT0(ir->is_tensor());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);
    ASSERT0(op0->is_const() && op1->is_const());
    ASSERT0(op0->is_tensor() && !op1->is_tensor());
    if (!g_do_refine_with_host_api) { return ir; }
    TensorType const* restty = (TensorType const*)ir->getType();
    if (restty->getDim() != 2 || !restty->is_int()) { return ir; }
    TenType * op0_tval = (TenType*)CONST_tensor_val(op0);
    ASSERT0(op0_tval);
    switch (ir->getCode()) {
    case IR_SUB:
        if (op1->is_int()) {
            xoc::TenType::sub(*op0_tval, (INT)CONST_int_val(op1), *op0_tval);
        } else if (op1->is_fp()) {
            xoc::TenType::sub(*op0_tval, (INT)CONST_fp_val(op1), *op0_tval);
        } else { UNREACHABLE(); }
        break;
    case IR_POW:
        if (op1->is_int()) {
            xoc::TenType::pow(*op0_tval, (INT)CONST_int_val(op1), *op0_tval);
        } else if (op1->is_fp()) {
            xoc::TenType::pow(*op0_tval, (INT)CONST_fp_val(op1), *op0_tval);
        } else { UNREACHABLE(); }
        break;
    case IR_MUL:
        if (op1->is_int()) {
            xoc::TenType::mul(*op0_tval, (INT)CONST_int_val(op1), *op0_tval);
        } else if (op1->is_fp()) {
            xoc::TenType::mul(*op0_tval, (INT)CONST_fp_val(op1), *op0_tval);
        } else { UNREACHABLE(); }
        break;
    default: UNREACHABLE();
    }
    dumpFoldConstTensorOpRes(ir, op0_tval, rc);
    change = true;
    m_rg->freeIRTree(ir);
    IR * res = m_rg->getIRMgr()->buildImmTensor((TenVal*)op0_tval, restty);
    return res; //No need to update DU.
}


IR * ARMRefine::foldConstBinaryTensor(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isBinaryOp());
    ASSERT0(ir->is_tensor());
    IR const* op0 = BIN_opnd0(ir);
    IR const* op1 = BIN_opnd1(ir);
    if (!op0->is_const() || !op1->is_const()) { return ir; }
    if (op0->is_tensor() && !op1->is_tensor()) {
        return foldConstBinaryTensorByScalar(ir, change, rc);
    }
    if (!op0->is_tensor() && op1->is_tensor()) {
        return foldConstBinaryScalarByTensor(ir, change, rc);
    }
    if (op0->is_tensor() && op1->is_tensor()) {
        return foldConstBinaryTensorByTensor(ir, change, rc);
    }
    return ir;
}


IR * ARMRefine::foldConstUnaryTensor(
    IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isUnaryOp());
    ASSERT0(0); //TODO:
    return nullptr; //No need to update DU.
}


IR * ARMRefine::refineTrigonometricTensor(
    IR * ir, bool & change, RefineCtx & rc)
{
    return foldConstTrigonometricTensor(ir, change, rc);
}


IR * ARMRefine::refineConvOpndGrad(IR * ir, bool & change, RefineCtx & rc)
{
    return foldConst(ir, change, rc);
}


IR * ARMRefine::refineBroadCast(IR * ir, bool & change, RefineCtx & rc)
{
    ir = refineAllKids(ir, change, rc);
    return ir;
}


IR * ARMRefine::refineConv(IR * ir, bool & change, RefineCtx & rc)
{
    return foldConst(ir, change, rc);
}


//The function multiply tensor(or sub-tensor) by a scalar.
//Note the function does NOT support in-place operation, res can not be
//same memory with 'input'.
//Record the result in 'res'.
//e.g:given input is 2*2 tensor, scalar is N, res is 2*2 tensor.
//  input        res
//  |1,2| * N => |1*N, 2*N|
//  |3,4|        |3*N, 4*N|
//e.g:given input is 2*2 tensor, scalar is N, res is 4*3 tensor,
//    res_row_start is 1, res_col_start is 2.
//  input        res
//  |1,2| * N => |0, 0, 0,   0  |
//  |3,4|        |0, 0, 1*N, 2*N|
//               |0, 0, 3*N, 4*N|
//input: input tensor.
//scalar: the multiplier
//row_start: the start row of input.
//row_end: the end row of input.
//col_start: the start column of input.
//row_end: the end column of input.
//res_row_start: the start row of res.
//res_col_start: the start column of res.
static void calcMulOfScalarAndTensor(
    TenType const& input, TenType::EType scalar, UINT row_start, UINT row_end,
    UINT col_start, UINT col_end, OUT TenType & res,
    UINT res_row_start, UINT res_col_start)
{
    ASSERTN(&input != &res, ("not support in-place operation"));
    ASSERT0(row_start <= row_end);
    ASSERT0(col_start <= col_end);
    ASSERT0(row_end < input.getRowSize());
    ASSERT0(col_end < input.getColSize());
    ASSERT0(res_row_start + (row_end - row_start) < res.getRowSize());
    ASSERT0(res_col_start + (col_end - col_start) < res.getColSize());
    for (UINT i = row_start, resi = res_row_start;
         i <= row_end; i++, resi++) {
        for (UINT j = col_start, resj = res_col_start;
             j <= col_end; j++, resj++) {
            res.set(resi, resj,
                    res.get(resi, resj) + input.get(i, j) * scalar);
        }
    }
}


static void calcMulOfScalarAndTensor(
    TenType const& input, TenType::EType scalar, OUT TenType & res,
    UINT res_row_start, UINT res_col_start)
{
    calcMulOfScalarAndTensor(input, scalar, 0, input.getLastRow(),
                             0, input.getLastCol(),
                             res, res_row_start, res_col_start);
}


static void calcConvInputDer(
    IR const* ir, TenType const& input, TenType const& weight,
    TenType const& convgrad, OUT TenType & res, RefineCtx const& rc)
{
    ASSERT0(ir->getCode() == IR_CONV_OPND_GRAD);
    UINT row_stride = CONVOPNDGRAD_stride_h(ir);
    UINT col_stride = CONVOPNDGRAD_stride_w(ir);
    ASSERTN(&input != &res, ("not support in-place operation"));
    ASSERTN(&weight != &res, ("not support in-place operation"));
    //Given input:4x4, weight:2x2, stride_h=2, stride_w=2, the output
    //will be 2x2.
    //            col_start
    //             |       col_end
    //             |       |
    //             v       v
    //            ---------------------
    // row_start->|a11 a12 a13 a14 a15
    //            |a21 a22 a23 a24 a25
    // row_end  ->|a31 a32 a33 a34 a35
    //            |a41 a42 a43 a44 a45
    //            |a51 a52 a53 a54 a55
    //----
    //Given dl/dz:2x2, dl/dinput will be 4x4:
    //  weight:w1,w2
    //         w3,w4
    //  dl/dinput will be:
    //      dl/dz1*w1, dl/dz1*w2, dl/dz2*w1, dl/dz2*w2,
    //      dl/dz1*w3, dl/dz1*w4, dl/dz2*w3, dl/dz2*w4,
    //      dl/dz3*w1, dl/dz3*w2, dl/dz4*w1, dl/dz4*w2,
    //      dl/dz3*w3, dl/dz3*w4, dl/dz4*w3, dl/dz4*w4,
    res.reinit(input.getRowSize(), input.getColSize());
    for (UINT i = 0, row_start = 0;
         i < convgrad.getRowSize();
         i++, row_start += row_stride) {
        for (UINT j = 0, col_start = 0;
             j < convgrad.getColSize();
             j++, col_start += col_stride) {
            TenType::EType grad = convgrad.get(i, j);
            calcMulOfScalarAndTensor(
                weight, grad, res, row_start, col_start);
        }
    }
    dumpFoldConstConv(ir, &input, &weight, row_stride, col_stride,
                      &res, rc);
}


static void calcConvWeightDer(
    IR const* ir, TenType const& input, TenType const& weight,
    TenType const& convgrad, OUT TenType & res, RefineCtx const& rc)
{
    ASSERT0(ir->getCode() == IR_CONV_OPND_GRAD);
    ASSERTN(&input != &res, ("not support in-place operation"));
    ASSERTN(&weight != &res, ("not support in-place operation"));
    //            col_start
    //             |       col_end
    //             v       v
    //            ---------------------
    // row_start->|a11 a12 a13 a14 a15
    //            |a21 a22 a23 a24 a25
    // row_end  ->|a31 a32 a33 a34 a35
    //            |a41 a42 a43 a44 a45
    //            |a51 a52 a53 a54 a55
    UINT row_stride = CONVOPNDGRAD_stride_h(ir);
    UINT col_stride = CONVOPNDGRAD_stride_w(ir);
    res.reinit(weight.getRowSize(), weight.getColSize());
    for (UINT i = 0, row_start = 0; i < convgrad.getRowSize();
         i++, row_start += row_stride) {
        UINT row_end = row_start + weight.getLastRow();
        for (UINT j = 0, col_start = 0; j < convgrad.getColSize();
             j++, col_start += col_stride) {
            UINT col_end = col_start + weight.getLastCol();
            TenType::EType grad = convgrad.get(i, j);
            calcMulOfScalarAndTensor(input, grad, row_start, row_end,
                                     col_start, col_end, res, 0, 0);
        }
    }
    dumpFoldConstConvWeightDer(ir, &input, &weight, &convgrad,
                               row_stride, col_stride, &res, rc);
}


IR * ARMRefine::foldConstConvOpndGrad(IR * ir, bool & change, RefineCtx & rc)
{
    ir = foldConstAllKids(ir, change, rc);
    ASSERT0(ir->getCode() == IR_CONV_OPND_GRAD);
    ASSERT0(ir->is_tensor());

    //Because the first derivative|gradient may be immediate 1 in
    //reverse-mode, and it will be reinitialized to tensor if necessary.
    //ASSERT0(irder->is_tensor());
    IR const* input = CONVOPNDGRAD_input(ir);
    IR const* weight = CONVOPNDGRAD_weight(ir);
    IR const* grad = CONVOPNDGRAD_grad(ir);
    ASSERT0(input && weight && grad);
    if (!input->is_const() || !weight->is_const()) { return ir; }
    ASSERT0(grad->is_const() && grad->is_tensor());
    ASSERT0(input->is_tensor());
    ASSERT0(weight->is_tensor());
    ASSERT0(TensorInfoMgr::hasTensorInfo(ir));
    ASSERT0(TensorInfoMgr::hasTensorInfo(input));
    ASSERT0(TensorInfoMgr::hasTensorInfo(weight));
    TenType const* t_input = nullptr;
    if (input->is_const()) {
        t_input = (TenType*)CONST_tensor_val(input);
    } else {
        ASSERT0(input->is_ld());
        t_input = CalcDerivative::getTensor(input);
    }
    ASSERT0(t_input);

    TenType const* t_weight = CalcDerivative::getTensor(weight);
    if (weight->is_const()) {
        t_weight = (TenType*)CONST_tensor_val(weight);
    } else {
        ASSERT0(weight->is_ld());
        t_weight = CalcDerivative::getTensor(weight);
    }
    ASSERT0(t_weight);

    //NOTE irder may be a composite expression which type should be tensor.
    TenType * t_grad = nullptr;
    if (grad->is_const()) {
        t_grad = (TenType*)CONST_tensor_val(grad);
    } else {
        t_grad = CalcDerivative::getTensor(grad);
    }
    ASSERT0(t_grad);

    //Weight
    TenType * t_res = getCalcDer()->genImmTensor(ir->getType());
    if (CONVOPNDGRAD_is_compute_weight(ir)) {
        calcConvWeightDer(ir, *t_input, *t_weight, *t_grad, *t_res, rc);
    } else {
        ASSERT0(CONVOPNDGRAD_is_compute_input(ir));
        calcConvInputDer(ir, *t_input, *t_weight, *t_grad, *t_res, rc);
    }
    return m_irmgr->buildImmTensor((TenVal*)t_res, ir->getType());
}


IR * ARMRefine::foldConstConv(IR * ir, bool & change, RefineCtx const& rc)
{
    if (!g_do_refine_with_host_api) { return ir; }
    ASSERT0(ir->getCode() == IR_CONV);
    ASSERT0(ir->is_tensor());
    IR const* input = CONV_input(ir);
    IR const* weight = CONV_weight(ir);
    if (!input->is_const() || !weight->is_const()) { return ir; }
    ASSERT0(input->is_tensor());
    ASSERT0(weight->is_tensor());
    TensorType const* ity = (TensorType const*)input->getType();
    TensorType const* wty = (TensorType const*)weight->getType();
    if (ity->getDim() != 2 || !ity->is_int()) { return ir; }
    if (wty->getDim() != 2 || !wty->is_int()) { return ir; }
    TenType * t_input = (TenType*)CONST_tensor_val(input);
    ASSERT0(t_input);
    TenType * t_weight = (TenType*)CONST_tensor_val(weight);
    ASSERT0(t_weight);
    UINT input_stride_h = CONV_stride_h(ir);
    UINT input_stride_w = CONV_stride_w(ir);
    TenType * tensor_val = (TenType*)getCalcDer()->genImmTensor(ir->getType());
    ASSERT0(tensor_val);
    xoc::TenType::conv(*t_input, *t_weight, input_stride_h,
                       input_stride_w, *tensor_val);
    dumpFoldConstConv(ir, t_input, t_weight, input_stride_h, input_stride_w,
                      tensor_val, rc);
    change = true;
    IR * res = m_rg->getIRMgr()->buildImmTensor(
        (TenVal*)tensor_val, ir->getType());
    m_rg->freeIRTree(ir);
    return res; //No need to update DU.
}


IR * ARMRefine::foldConstUnary(IR * ir, bool & change, RefineCtx & rc)
{
    IR * newir = Refine::foldConstUnary(ir, change, rc);
    if (newir->is_tensor()) {
        return foldConstUnaryTensor(newir, change, rc);
    }
    return newir;
}


IR * ARMRefine::foldConstBinary(IR * ir, bool & change, RefineCtx & rc)
{
    IR * newir = Refine::foldConstBinary(ir, change, rc);
    if (newir->is_tensor()) {
        return foldConstBinaryTensor(newir, change, rc);
    }
    return newir;
}


IR * ARMRefine::foldConstTrigonometric(
    IR * ir, bool & change, RefineCtx & rc)
{
    if (ir->is_tensor()) {
        return foldConstTrigonometricTensor(ir, change, rc);
    }
    return foldConstTrigonometricScalar(ir, change, rc);
}


IR * ARMRefine::foldConstTrigonometricScalar(
    IR * ir, bool & change, RefineCtx & rc)
{
    ASSERT0(ir->isUnaryOp());
    IR const* op = UNA_opnd(ir);
    if (!op->is_const()) { return ir; }
    if (!g_do_refine_with_host_api) { return ir; }
    if (!op->isInt() && !op->is_fp()) { return ir; }
    if (!ir->isInt() && !ir->is_fp()) { return ir; }
    Type const* ty = ir->getType();
    switch (ir->getCode()) {
    case IR_TANH: {
        HOST_FP v;
        if (op->isInt()) { v = ::tanh(HOST_FP(CONST_int_val(op))); }
        else { v = ::tanh(CONST_fp_val(op)); }
        m_rg->freeIRTree(ir);
        if (ty->is_int()) {
            return m_irmgr->buildImmInt(HOST_INT(v), ty);
        }
        return m_irmgr->buildImmFP(HOST_FP(v), ty);
    }
    default: ASSERTN(0, ("TODO"));
    }
    return ir;
}


IR * ARMRefine::refineTrigonometric(IR * ir, bool & change, RefineCtx & rc)
{
    //NOTE:the trigonometric op should be common IR, otherwise handle it
    //in individual function.
    IR * newir = Refine::refineTrigonometric(ir, change, rc);
    if (newir->is_tensor()) {
        return refineTrigonometricTensor(newir, change, rc);
    }
    return newir;
}


IR * ARMRefine::foldConstRelu(IR * ir, bool & change, RefineCtx const& rc)
{
    ASSERT0(ir->isUnaryOp());
    ASSERT0(ir->getCode() == IR_RELU);
    IR * opnd = UNA_opnd(ir);
    if (!opnd->is_const()) { return ir; }
    Type const* resty = ir->getType();
    if (g_is_opt_float && resty->is_fp()) {
        IR * tmp = nullptr;
        if (CONST_fp_val(opnd) <= HOST_FP(0)) {
            tmp = m_irmgr->buildImmFP((HOST_FP)0, resty);
        } else {
            ASSERT0(CONST_fp_val(opnd) > HOST_FP(0));
            tmp = m_rg->dupIRTree(opnd);
        }
        copyDbx(tmp, ir, m_rg);
        m_rg->freeIRTree(ir);
        change = true;
        return tmp;
    }
    if (resty->is_int()) {
        IR * tmp = nullptr;
        if (CONST_int_val(opnd) <= HOST_INT(0)) {
            tmp = m_irmgr->buildImmInt((HOST_INT)0, resty);
        } else {
            ASSERT0(CONST_int_val(opnd) > HOST_INT(0));
            tmp = m_rg->dupIRTree(opnd);
        }
        copyDbx(tmp, ir, m_rg);
        m_rg->freeIRTree(ir);
        change = true;
        return tmp;
    }
    return ir;
}


IR * ARMRefine::foldConstExtExp(IR * ir, bool & change, RefineCtx & rc)
{
    switch (ir->getCode()) {
    case IR_RELU: return foldConstRelu(ir, change, rc);
    case IR_SOFTMAX:
    case IR_SIGMOID:
        return ir;
    case IR_TANH: return foldConstTrigonometricTensor(ir, change, rc);
    case IR_CONV_OPND_GRAD: return foldConstConvOpndGrad(ir, change, rc);
    case IR_CONV: return foldConstConv(ir, change, rc);
    default: UNREACHABLE(); return ir;
    }
    return nullptr;
}



IR * ARMRefine::refineExtOp(IR * ir, bool & change, RefineCtx & rc)
{
    ir = refineAllKids(ir, change, rc);
    switch (ir->getCode()) {
    case IR_RELU: return refineRelu(ir, change, rc);
    case IR_SOFTMAX: ASSERT0(0);
    case IR_SIGMOID: ASSERT0(0);
    case IR_CONV: return refineConv(ir, change, rc);
    case IR_TANH: return foldConstTrigonometric(ir, change, rc);
    case IR_CONV_OPND_GRAD: return refineConvOpndGrad(ir, change, rc);
    case IR_BROADCAST: return refineBroadCast(ir, change, rc);
    default: UNREACHABLE(); return nullptr;
    }
    return nullptr;
}


void ARMRefine::initCalcDer(MOD OptCtx & oc)
{
    m_rg->getPassMgr()->checkValidAndRecompute(
        &oc, PASS_CALC_DERIVATIVE, PASS_UNDEF);
    m_cd = (CalcDerivative*)getRegion()->getPassMgr()->
            queryPass(PASS_CALC_DERIVATIVE);
    ASSERT0(m_cd && m_cd->is_valid());
}


bool ARMRefine::perform(OptCtx & oc)
{
    initCalcDer(oc);
    return Refine::perform(oc);
}
