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

//#define USE_GLOBAL_VARIABLE_LDA_POLICY1

namespace xgen {

//
//START ArgDescMgr
//
SR * ArgDescMgr::allocArgRegister(CG * cg)
{
    BSIdx phyreg = m_argregs.get_first();
    if (phyreg == BS_UNDEF) {
        return nullptr;
    }
    m_argregs.diff(phyreg);
    return cg->genDedicatedReg(phyreg);
}


//Abandon the first argument phy-register.
//The allocation will start at the next register.
void ArgDescMgr::dropArgRegister()
{
    BSIdx phyreg = m_argregs.get_first();
    if (phyreg != BS_UNDEF) {
        m_argregs.diff(phyreg);
    }
}


//This function should be invoked after passing one argument through register.
//Decrease tgt_ofst for each desc by the bytesize.
//e.g: pass 2 argument:
//  %r0 = arg1;
//  mgr.updatePassedArgInRegister(GNENERAL_REGISTER_SIZE);
//  %r1 = arg2;
//  mgr.updatePassedArgInRegister(arg2.bytesize);
void ArgDescMgr::updatePassedArgInRegister(UINT bytesize)
{
    m_passed_arg_in_register_byte_size += (INT)xcom::ceil_align(bytesize,
        STACK_ALIGNMENT);
    for (ArgDesc * desc = getArgList()->get_head();
         desc != nullptr; desc = getArgList()->get_next()) {
        desc->tgt_ofst -= bytesize;
    }
}
//END ArgDescMgr


//
//START CG
//
CG::CG(xoc::Region * rg, CGMgr * cgmgr): m_ip_mgr(self())
{
    ASSERTN(rg, ("Code generation requires region info."));
    ASSERT0(cgmgr);
    m_rg = rg;
    m_cgmgr = cgmgr;
    m_tm = rg->getTypeMgr();
    m_or_bb_idx = ORBBID_UNDEF + 1; //initialize to be the id of first BB.
    m_or_bb_mgr = nullptr;
    m_or_mgr = cgmgr->getORMgr();
    m_or_cfg = nullptr;
    m_ppm_vec = nullptr;
    m_max_real_arg_size = 0;
    m_pr2sr_map.init(MAX(4, xcom::getNearestPowerOf2(rg->getPRCount())));

    //TODO: To ensure alignment for performance gain.
    m_sect_count = 0;
    m_reg_count = 0;
    m_param_pointer = nullptr;
    m_pool = smpoolCreate(64, MEM_COMM);
    m_is_use_fp = false;
    m_is_compute_sect_offset = false;
    m_is_dump_or_id = true;
}


CG::~CG()
{
    m_pr2sr_map.clean();
    m_regid2sr_map.clean();
    if (m_or_cfg != nullptr) {
        delete m_or_cfg;
        m_or_cfg = nullptr;
    }

    //Free (but is not delete) all OR, SR, and ORBB.
    //m_or_bb_mgr should be deleted after freeORBBList().
    //OR will be deleted by ORMgr, and SR will be deleted by SRMgr,
    //ORBB will be deleted by ORBBMgr.
    freeORBBList();

    if (m_or_bb_mgr != nullptr) {
        delete m_or_bb_mgr;
        m_or_bb_mgr = nullptr;
    }
    smpoolDelete(m_pool);
    finiFuncUnit();
    destroyVAR();
}


//Free md's id and var's id back to MDSystem and VarMgr.
//The index of MD and Var is important resource if there
//are a lot of CG.
void CG::destroyVAR()
{
    VarMgr * varmgr = getRegion()->getVarMgr();
    MDSystem * mdsys = getRegion()->getMDSystem();
    ConstMDIter mdit;
    BBVarListIter it;
    for (m_bb_level_internal_var_list.get_head(&it); it != nullptr;
         m_bb_level_internal_var_list.get_next(&it)) {
        Var * v = it->val();
        mdsys->removeMDforVAR(v, mdit);
        varmgr->destroyVar(v);
    }

    FuncVarListIter it2;
    for (m_func_level_internal_var_list.get_head(&it2); it2 != nullptr;
         m_func_level_internal_var_list.get_next(&it2)) {
        Var * v = it2->val();
        mdsys->removeMDforVAR(v, mdit);
        varmgr->destroyVar(v);
    }
}


//Generate OR with variant number of operands and results.
//Note user should pass into the legal number of result and operand SRs
//that corresponding to 'orty'.
OR * CG::buildOR(OR_TYPE orty, ...)
{
    ASSERT0(orty != OR_UNDEF);
    UINT resnum = tmGetResultNum(orty);
    UINT opndnum = tmGetOpndNum(orty);
    va_list ptr;
    va_start(ptr, orty);
    OR * o = genOR(orty);
    //First extracting results.
    UINT i = 0;
    while (i < resnum) {
        SR * sr = va_arg(ptr, SR*);
        o->set_result(i, sr, this);
        i++;
    }
    //following are opnds
    i = 0;
    while (i < opndnum) {
        SR * sr = va_arg(ptr, SR*);
        if (i == 0 && HAS_PREDICATE_REGISTER) {
            ASSERTN(sr->is_pred(), ("first operand must be predicate SR"));
        }
        o->set_opnd(i, sr, this);
        i++;
    }
    va_end(ptr);
    return o;
}


//Generate OR with variant number of operands and results.
//Note user should pass into the legal number of result and operand SRs
//that corresponding to 'orty'.
OR * CG::buildOR(OR_TYPE orty, UINT resnum, UINT opndnum, ...)
{
    ASSERT0(orty != OR_UNDEF);
    //Unportable code, this manner worked well on ia32, but is not on x8664.
    //SR ** sr = (SR**)(((BYTE*)(&opndnum)) + sizeof(opndnum));
    ASSERT0(resnum == tmGetResultNum(orty));
    ASSERT0(opndnum == tmGetOpndNum(orty));

    va_list ptr;
    va_start(ptr, opndnum);
    OR * o = genOR(orty);
    //First extracting results.
    UINT i = 0;
    while (i < resnum) {
        SR * sr = va_arg(ptr, SR*);
        o->set_result(i, sr, this);
        i++;
    }
    //following are opnds
    i = 0;
    while (i < opndnum) {
        SR * sr = va_arg(ptr, SR*);
        if (i == 0 && HAS_PREDICATE_REGISTER) {
            ASSERTN(sr->is_pred(), ("first operand must be predicate SR"));
        }
        o->set_opnd(i, sr, this);
        i++;
    }
    va_end(ptr);
    return o;
}


//Generate OR with variant number of operands and results.
OR * CG::buildOR(OR_TYPE orty, IN SRList & result, IN SRList & opnd)
{
    OR * o = genOR(orty);
    UINT i = 0;
    for (SR * r = result.get_head(); r;
        r = result.get_next(), i++) {
        o->set_result(i, r, this);
    }

    i = 0;
    for (SR * sr = opnd.get_head(); sr != nullptr;
        sr = opnd.get_next(), i++) {
        if (i == 0 && HAS_PREDICATE_REGISTER) {
            ASSERT0(sr->is_pred());
        }
        o->set_opnd(i, sr, this);
    }
    return o;
}


//The function will allocate memory from CG's pool and build a list of Label
//according to the label info that carried by given IR.
//caselst: should be a list of IR_CASE.
LabelInfoList * CG::buildLabelInfoList(IR const* caselst)
{
    LabelInfoList * head = nullptr;
    LabelInfoList * last = nullptr;
    for (IR const* c = caselst; c != nullptr; c = c->get_next()) {
        ASSERT0(c->is_case() && CASE_lab(c));
        LabelInfoList * li = allocLabelInfoList();
        LILIST_label(li) = CASE_lab(c);
        xcom::add_next(&head, &last, li);
    }
    return head;
}


//Generate ::memcpy.
void CG::buildMemcpy(SR * tgt, SR * src, UINT bytesize, OUT ORList & ors,
                     MOD IOC * cont)
{
    ASSERT0(tgt && src && cont);
    //Push parameter on stack.
    //::memcpy(void *dest, const void *src, int n);
    SR * immreg = genReg();
    buildMove(immreg, genIntImm((HOST_INT)bytesize, false), ors, cont);
    ArgDescMgr argdescmgr;
    passArgVariant(&argdescmgr, ors, 3,
                   tgt, nullptr, tgt->getByteSize(), nullptr,
                   src, nullptr, src->getByteSize(), nullptr,
                   immreg, nullptr, immreg->getByteSize(), nullptr);

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    ASSERT0(argdescmgr.getCurrentDesc() == nullptr);

    //Copy the value from src to tgt.
    ASSERT0(m_cgmgr->getBuiltinVar(BUILTIN_MEMCPY));
    buildCall(m_cgmgr->getBuiltinVar(BUILTIN_MEMCPY), bytesize, ors, cont);
}


void CG::buildStore(SR * store_val, xoc::Var const* base, HOST_INT ofst,
                    bool is_signed, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(store_val->is_reg());
    SR * mem_base_addr = genVAR(base);
    buildStore(store_val, mem_base_addr, genIntImm(ofst, true),
               is_signed, ors, cont);
}


//Build spilling operation.
//This function generate memory store operation to the spill location.
void CG::buildSpill(IN SR * store_val, IN xoc::Var * spill_var,
                    IN ORBB *, OUT ORList & ors)
{
    ASSERT0(store_val->is_reg() && VAR_is_spill(spill_var));
    SR * mem_base_addr = genVAR(spill_var);

    IOC tc;
    IOC_mem_byte_size(&tc) = store_val->getByteSize();
    //Spill location is the same length as register.
    buildStore(store_val, mem_base_addr, genZero(), false, ors, &tc);

    for (OR * o = ors.get_head(); o != nullptr; o = ors.get_next()) {
        if (o->is_store()) {
            OR_is_spill(o) = 1;
        }
    }
}


//Build reloading operation.
//This function generate memory load operation from the spill location.
void CG::buildReload(IN SR * result_val, IN xoc::Var * reload_var,
                     IN ORBB *, OUT ORList & ors)
{
    ASSERT0(result_val->is_reg() && VAR_is_spill(reload_var));
    IOC tc;
    IOC_mem_byte_size(&tc) = result_val->getByteSize();
    bool is_signed = false; //Spill location is the same length as register.
    buildLoad(result_val, reload_var, 0, is_signed, ors, &tc);
    for (OR * o = ors.get_head(); o; o = ors.get_next()) {
        if (o->is_load()) {
            OR_is_reload(o) = true;
        }
    }
}


//Build pseduo instruction that indicate LabelInfo.
//Note OR_label must be supplied by Target.
void CG::buildLabel(LabelInfo const* li, OUT ORList & ors, IN IOC *)
{
    ASSERT0(li != nullptr);
    SR * lab = genLabel(li);
    OR * o = genOR(OR_label);
    o->setLabel(lab, this);
    ors.append_tail(o);
}


//This function build OR according to given 'code'.
//Implement the target dependent version if needed.
//'sr_size': The number of integral multiple of byte-size of single SR.
void CG::buildBinaryOR(IR_TYPE code, SR * opnd0, SR * opnd1, bool is_signed,
                       OUT ORList & ors, MOD IOC * cont)
{
    //Result's type-size might be not same as opnd. e,g: a < b,
    //result type is BOOL, opnd type is INT.
    OR_TYPE orty = mapIRType2ORType(code, opnd0->getByteSize(),
                                    opnd0, opnd1, is_signed);
    ASSERTN(orty != OR_UNDEF, ("mapIRType2ORType() should be overloaded"));
    SR * res = genReg();

    //Load immediate into register if target-machine needed.
    UINT opnd0_idx = HAS_PREDICATE_REGISTER + 0;
    UINT opnd1_idx = HAS_PREDICATE_REGISTER + 1;
    if (opnd0->is_int_imm() &&
        !isValidImmOpnd(orty, opnd0_idx, opnd0->getInt())) {
        //Bit width is not big enough to hold operand value, load it
        //into register instead.
        buildGeneralLoad(opnd0, 0, is_signed, ors, cont);
        opnd0 = cont->getResult();
        ASSERT0(opnd0);

        orty = mapIRType2ORType(code, opnd0->getByteSize(),
                                opnd0, opnd1, is_signed);
        ASSERTN(orty != OR_UNDEF, ("mapIRType2ORType() should be overloaded"));
    }

    if (opnd1->is_int_imm() &&
        !isValidImmOpnd(orty, opnd1_idx, opnd1->getInt())) {
        //Bit width is not big enough to hold operand value, load it
        //into register instead.
        buildGeneralLoad(opnd1, 0, is_signed, ors, cont);
        opnd1 = cont->getResult();
        ASSERT0(opnd1);

        orty = mapIRType2ORType(code, opnd0->getByteSize(),
                                opnd0, opnd1, is_signed);
        ASSERTN(orty != OR_UNDEF, ("mapIRType2ORType() should be overloaded"));
    }

    //Build OR.
    SRList res_list;
    SRList opnd_list;
    res_list.append_tail(res);
    if (HAS_PREDICATE_REGISTER) {
        opnd_list.append_tail(getTruePred()); //0th operand
    }
    opnd_list.append_tail(opnd0);
    opnd_list.append_tail(opnd1);
    OR * o = buildOR(orty, res_list, opnd_list);

    ASSERT0(!opnd0->is_int_imm() ||
            isValidImmOpnd(o->getCode(), opnd0_idx, opnd0->getInt()));
    ASSERT0(!opnd1->is_int_imm() ||
            isValidImmOpnd(o->getCode(), opnd1_idx, opnd1->getInt()));

    //Set result SR.
    ASSERT0(cont != nullptr);
    cont->set_reg(0, res);
    ors.append_tail(o);
}


//'sr_size': The number of byte-size of SR.
void CG::buildAdd(SR * src1, SR * src2, UINT sr_size, bool is_sign,
                  OUT ORList & ors, MOD IOC * cont)
{
    if (src1->is_int_imm() && src2->is_int_imm()) {
        SR * result = genIntImm((HOST_INT)(src1->getInt() +
                                src2->getInt()), true);
        ASSERT0(cont != nullptr);
        cont->set_reg(0, result);
        return;
    }
    if (src1->is_int_imm()) {
        buildAdd(src2, src1, sr_size, is_sign, ors, cont);
        return;
    }
    if (src1->is_reg() && (src2->is_int_imm() || src2->is_var())) {
        buildAddRegImm(src1, src2, sr_size, is_sign, ors, cont);
        return;
    }
    ASSERT0(src1->is_reg() && src2->is_reg());
    buildAddRegReg(true, src1, src2, sr_size, is_sign, ors, cont);
}


//'sr_size': The number of integral multiple of byte-size of single SR.
void CG::buildSub(SR * src1, SR * src2, UINT sr_size, bool is_sign,
                  OUT ORList & ors, MOD IOC * cont)
{
    if (src1->is_int_imm() && src2->is_int_imm()) {
        SR * result = genIntImm((HOST_INT)(src1->getInt() -
                                src2->getInt()), true);
        ASSERT0(cont != nullptr);
        cont->set_reg(0, result);
        return;
    }

    if (src1->is_int_imm()) {
        ASSERT0(sr_size <= 8);
        SR * newsrc1 = genReg();
        buildMove(newsrc1, src1, ors, cont);

        if (sr_size == 8) {
            SR * t = genReg();
            SR * src1_h = src1->getVec()->get(1);
            ASSERT0(src1_h != nullptr);
            buildMove(t, src1_h, ors, cont);
            getSRVecMgr()->genSRVec(2, newsrc1, t);
        }
        src1 = newsrc1;
    } else if (src2->is_int_imm()) {
        buildAddRegImm(src1, genIntImm(-src2->getInt(), true),
                       sr_size, is_sign, ors, cont);
        return;
    } else {
        //May be the Var is an offset that will be computed lazy.
        ASSERTN(!src2->is_var(), ("subtract Var is unsupport"));
    }
    ASSERT0(src1->is_reg() && src2->is_reg());
    buildAddRegReg(false, src1, src2, sr_size, is_sign, ors, cont);
}


//Increase 'reg' by 'val'.
void CG::buildIncReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont)
{
    ASSERT0(reg->is_reg());
    if (val == 0) { return; }

    buildAdd(reg, genIntImm((HOST_INT)val, false), GENERAL_REGISTER_SIZE,
             false, ors, cont);
    ASSERT0(ors.get_tail()->get_result(0)->is_reg());
    //Change the operation to in-situ operation.
    ors.get_tail()->set_result(0, reg, this);
}


//Decrease 'reg' by 'val'.
void CG::buildDecReg(SR * reg, UINT val, OUT ORList & ors, IOC * cont)
{
    ASSERT0(reg->is_reg());
    if (val == 0) { return; }

    buildSub(reg, genIntImm((HOST_INT)val, false), GENERAL_REGISTER_SIZE,
             false, ors, cont);
    ASSERT0(ors.get_tail()->get_result(0)->is_reg());
    //Change the operation to in-situ operation.
    ors.get_tail()->set_result(0, reg, this);
}


//'sr_size': The number of integral multiple of byte-size of single SR.
void CG::buildMod(CLUST clust, SR ** result, SR * src1, SR * src2,
                  UINT sr_size, bool is_sign, OUT ORList & ors, IN IOC *)
{
    DUMMYUSE(is_sign);
    DUMMYUSE(sr_size);
    OR_TYPE orty = OR_UNDEF;
    ASSERTN(0, ("Target Dependent Code"));
    if (src1->is_int_imm() && src2->is_int_imm()) {
        *result = genIntImm((HOST_INT)(src1->getInt() % src2->getInt()),
                            true);
        return;
    }

    ASSERTN(!SR_is_int_imm(*result), ("Not allocate result sr"));
    OR * o = buildOR(orty, 1, 3, *result, getTruePred(), src1, src2);
    OR_clust(o) = clust;
    ors.append_tail(o);
}


//Generate sp adjust operation.
void CG::buildSpadjust(OUT ORList & ors, MOD IOC * cont)
{
    OR * o = genOR(OR_spadjust_i);
    ASSERT0(o->is_fake());
    o->set_result(0, getSP(), this);
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, getSP(), this);
    ASSERT0(cont != nullptr);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1,
                genIntImm((HOST_INT)IOC_int_imm(cont), true), this);
    ors.append_tail(o);
    cont->set_reg(0, getSP());
}


//The function builds stack-pointer adjustment operation.
//Note XGEN supposed that the direction of stack-pointer is always decrement.
//bytesize: bytesize that needed to adjust, it can be immediate or register.
void CG::buildAlloca(OUT ORList & ors, SR * bytesize, MOD IOC * cont)
{
    OR * o;
    if (bytesize->is_imm()) {
        o = genOR(OR_spadjust_i);
    } else {
        ASSERT0(bytesize->is_reg());
        o = genOR(OR_spadjust_r);
    }
    ASSERT0(o->is_fake());
    o->set_result(0, getSP(), this);
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, getSP(), this);
    ASSERT0(cont != nullptr);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1, bytesize, this);
    ors.append_tail(o);
    cont->set_reg(0, getSP());
}


//Convert data type from 'src' to 'tgt'.
void CG::buildTypeCvt(xoc::IR const* tgt, xoc::IR const* src,
                      OUT ORList & ors, MOD IOC * cont)
{
    ASSERTN(!tgt->is_vec() && !src->is_vec(), ("TODO"));
    UINT tgt_size = tgt->getTypeSize(m_tm);
    UINT src_size = src->getTypeSize(m_tm);
    if (tgt_size <= src_size || tgt_size <= GENERAL_REGISTER_SIZE) {
        return;
    }

    ORList tors;
    if (tgt_size <= 8) {
        if (src_size <= 4) {
            SR * tgt_low = cont->get_reg(0);
            ASSERT0(tgt_low != nullptr);
            SR * tgt_high = nullptr;

            IOC tmp;
            if (src->is_signed()) {
                //Regard src >> 31(arithmatic shift right)
                //as the sign extension.
                buildShiftRight(tgt_low, GENERAL_REGISTER_SIZE,
                                genIntImm((HOST_INT)31, false),
                                true, tors, &tmp);
                tgt_high = tmp.get_reg(0);
                ASSERT0(tgt_high != nullptr);
            } else {
                tgt_high = genReg();
                buildMove(tgt_high, genZero(), tors, &tmp);
            }
            tors.copyDbx(tgt);
            ors.move_tail(tors);
            cont->set_reg(0, tgt_low);
            getSRVecMgr()->genSRVec(2, tgt_low, tgt_high);
        } else {
            //Just do some check.
            SR * src_low = cont->get_reg(0);
            CHECK0_DUMMYUSE(src_low);
            ASSERT0(src_low->getVec() != nullptr && SR_vec_idx(src_low) == 0);
            ASSERT0(src_low->getVec()->get(1) != nullptr);
            ASSERT0(src_low->getByteSize() == 8);
        }
        return;
    }
    ASSERTN(0, ("TODO"));
}


static void buildLdaViaReg(Dbx const* dbx, SR * base, SR * ofst, CG * cg,
                           OUT ORList & ors, MOD IOC * cont)
{
    //Base address of variable is recorded in register.
    //Get variable's address by: reg = base-reg + offset.
    ORList tors;
    IOC tmp;
    cg->buildAdd(base, ofst, GENERAL_REGISTER_SIZE, false, tors, &tmp);
    ASSERT0(tors.get_elem_count() == 1);
    OR * addu = tors.get_head();
    OR_is_need_compute_var_ofst(addu) = true;
    if (dbx != nullptr) {
        OR_dbx(addu).copy(*dbx);
    }
    ors.append_tail(addu);
    SR * res = tmp.get_reg(0);
    ASSERT0(res && cont);
    cont->set_reg(0, res);
}


//For global variable, there are two SR descriptions: global section +
//byte offset in the section and global variable + byte offset in the
//variable. Use 'base' will be the first policy, and 'variable' the second
//policy.
static void buildLdaForGlobalVarPolicy1(SR * base, SR * ofst, CG * cg,
                                        OUT ORList & ors, MOD IOC * cont)
{
    //Add the constant byte offset into the base address.
    ASSERT0(ofst->is_int_imm());
    if (base->is_var()) {
        IOC tmp;
        SR * addr = cg->genReg();
        cg->buildMove(addr, base, ors, &tmp);
        base = addr;
        cont->set_reg(0, base);
    }
    if (ofst->is_int_imm() && ofst->getInt() == 0) {
        ; //Add 0, omitted.
    } else {
        cg->buildAdd(base, ofst, GENERAL_REGISTER_SIZE, false, ors, cont);
    }
    ASSERT0(cont->get_reg(0) && cont->get_reg(0)->is_reg());
}


//For global variable, there are two SR descriptions: global section +
//byte offset in the section and global variable + byte offset in the
//variable. Use 'base' will be the first policy, and 'variable' the second
//policy.
static void buildLdaForGlobalVarPolicy2(xoc::Var const* var,
                                        HOST_INT lda_ofst, CG * cg,
                                        OUT ORList & ors,
                                        MOD IOC * cont)
{
    IOC tmp;
    SR * addr = cg->genReg();
    cg->buildMove(addr, cg->genVAR(var), ors, &tmp);

    //Add the constant byte offset into the base address.
    ASSERTN(lda_ofst >= 0, ("byte offset should be positive"));
    if (lda_ofst > 0) {
        IOC tmp;
        cg->buildAdd(addr, cg->genIntImm((HOST_INT)lda_ofst, false),
                     GENERAL_REGISTER_SIZE, false, ors, &tmp);
        addr = tmp.get_reg(0);
        ASSERT0(addr);
    }

    //Set the return SR.
    cont->set_reg(0, addr);
}


static void buildLdaForGlobalVar(Dbx const* dbx, xoc::Var const* var,
                                 HOST_INT lda_ofst, SR * base, SR * ofst,
                                 CG * cg, OUT ORList & ors, MOD IOC * cont)
{
    ASSERT0(var->is_global());
    //Get variable's address: reg = _variable_symbol_address_.
    ASSERT0(base->is_var());
    ORList tors;
    //For the sake of clear dumpping, we tend to the second policy.
    #ifdef USE_GLOBAL_VARIABLE_LDA_POLICY1
    buildLdaForGlobalVarPolicy1(base, ofst, cg, tors, cont);
    #else
    buildLdaForGlobalVarPolicy2(var, lda_ofst, cg, tors, cont);
    #endif

    //Transfer the debug information.
    if (dbx != nullptr) {
        tors.copyDbx(dbx);
    }
    ors.move_tail(tors);
    ASSERT0(cont);
    ASSERT0(cont->get_reg(0) && cont->get_reg(0)->is_reg());
}


static void buildLdaForLocalVar(Dbx const* dbx, SR * base, SR * ofst, CG * cg,
                                OUT ORList & ors, MOD IOC * cont)
{
    //Get variable's address: reg = _variable_symbol_address_.
    ASSERT0(base->is_var());
    ORList tors;
    SR * addr = cg->genReg();
    //The address should have been processed and stored in 'base'.
    cg->buildMove(addr, base, tors, cont);

    //Add the constant byte offset into the address.
    ASSERT0(ofst->is_int_imm());
    if (ofst->getInt() > 0) {
        IOC tmp;
        cg->buildAdd(addr, ofst, GENERAL_REGISTER_SIZE, false, tors, &tmp);
        addr = tmp.get_reg(0);
        ASSERT0(addr);
    }

    //Transfer the debug information.
    if (dbx != nullptr) {
        tors.copyDbx(dbx);
    }
    ors.move_tail(tors);
    ASSERT0(cont);

    //Set the return SR.
    cont->set_reg(0, addr);
}


//Generate operations: reg = &var + lda_ofst
//lda_ofst: the offset based to var.
void CG::buildLda(xoc::Var const* var, HOST_INT lda_ofst, Dbx const* dbx,
                  OUT ORList & ors, MOD IOC * cont)
{
    SR * base;
    SR * ofst;
    computeVarBaseAndOffset(var, lda_ofst, &base, &ofst);
    if (base->is_reg()) {
        buildLdaViaReg(dbx, base, ofst, this, ors, cont);
        return;
    }

    if (var->is_global()) {
        //Load address from global symbol.
        buildLdaForGlobalVar(dbx, var, lda_ofst, base, ofst, this, ors, cont);
        return;
    }

    //Load address from local symbol.
    buildLdaForLocalVar(dbx, base, ofst, this, ors, cont);
}


void CG::buildGeneralLoad(IN SR * val, HOST_INT ofst, bool is_signed,
                          OUT ORList & ors, MOD IOC * cont)
{
    if (val->is_int_imm()) {
        SR * res = genReg();
        buildMove(res, val, ors, cont);
        cont->set_reg(0, res);
        return;
    }

    ASSERT0(val->is_var() || val->is_reg());
    ASSERTN(IOC_mem_byte_size(cont) > 0, ("illegal/redundant mem size"));
    if (IOC_mem_byte_size(cont) > GENERAL_REGISTER_SIZE * 2) {
        //Load too large value into register, convert the load to
        //memory copy, and return the begin address to copy.
        SR * addr = nullptr;
        if (val->is_reg()) {
            addr = val;
        } else {
            ASSERT0(val->is_var());
            buildLda(SR_var(val), SR_var_ofst(val) + ofst, nullptr, ors, cont);
            addr = cont->get_reg(0);
        }
        ASSERT0(addr);
        cont->clean_regvec();
        cont->set_addr(addr);
        return;
    }

    //Build genernal load.
    SR * load_val = nullptr;
    if (IOC_mem_byte_size(cont) <= GENERAL_REGISTER_SIZE) {
        load_val = genReg();
    } else if (IOC_mem_byte_size(cont) <= GENERAL_REGISTER_SIZE * 2) {
        load_val = genReg();
        load_val = getSRVecMgr()->genSRVec(2, load_val, genReg());
    } else { UNREACHABLE(); }
    ASSERT0(load_val);
    buildLoad(load_val, val, genIntImm(ofst, true), is_signed, ors, cont);
    ASSERT0(cont);
    cont->set_reg(0, load_val);
}


//Generate opcode of accumulating operation.
//Form as: A = A op B
void CG::buildAccumulate(OR * red_or, SR * red_var, SR * restore_val,
                         List<OR*> & ors)
{
    DUMMYUSE(red_or);
    DUMMYUSE(red_var);
    DUMMYUSE(restore_val);
    DUMMYUSE(ors);
    ASSERTN(0, ("Target Dependent Code"));
}


//Build memory store operation that store 'reg' into stack.
//NOTE: user have to assign physical register manually if there is
//new OR generated and requires register allocation.
//reg: register to be stored.
//offset: bytesize offset related to SP.
//ors: record output.
//cont: context.
void CG::buildStoreAndAssignRegister(SR * reg, UINT offset, ORList & ors,
                                     IOC * cont)
{
    SR * sr_offset = genIntImm((HOST_INT)offset, false);
    buildStore(reg, getSP(), sr_offset, false, ors, cont);
}


ORBB * CG::allocBB()
{
    if (m_or_bb_mgr == nullptr) {
        m_or_bb_mgr = new ORBBMgr();
    }

    ORBB * bb = m_or_bb_mgr->allocBB(this);
    ORBB_id(bb) = m_or_bb_idx++;
    return bb;
}


//Free OR, SR for BBs.
void CG::freeORBBList()
{
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != nullptr; bb = m_or_bb_list.get_next()) {
        for (OR * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb)) {
            //Only recycle SR, OR will be destroied during pool destruction.
            ASSERT0(m_or_mgr);
            m_or_mgr->freeSR(o);
        }
    }
    m_or_bb_list.clean();
}


bool CG::isGRAEnable() const
{
    return g_opt_level >= OPT_LEVEL2 && g_do_gra;
}


ORCFG * CG::allocORCFG()
{
    return new ORCFG(C_SEME, getORBBList(), this);
}


IssuePackageMgr * CG::allocIssuePackageMgr()
{
    return new IssuePackageMgr(this);
}


void CG::assembleSRVec(SRVec * srvec, SR * sr1, SR * sr2)
{
    ASSERT0(srvec && sr1 && sr2);
    srvec->clean();
    SR_vec(sr1) = srvec;
    SR_vec(sr2) = srvec;
    SR_vec_idx(sr1) = 0;
    SR_vec_idx(sr2) = 1;
    srvec->set(0, sr1, getSRVecMgr());
    srvec->set(1, sr2, getSRVecMgr());
}


//Calc total memory space for parameters,
//with considering the memory alignment.
UINT CG::computeTotalParameterStackSize(IR const* ir) const
{
    ASSERT0(ir->isCallStmt());
    UINT size = 0;
    for (IR * p = CALL_param_list(ir); p != nullptr; p = p->get_next()) {
        size = (UINT)ceil_align(size, STACK_ALIGNMENT);
        ASSERT0(!p->is_any());
        size += p->getTypeSize(m_tm);
    }
    return size;
}


//Calculate the section and related byte offset in section for given 'var'.
void CG::computeAndUpdateGlobalVarLayout(xoc::Var const* var, OUT SR ** base,
                                         OUT SR ** base_ofst)
{
    ASSERT0(var);
    ASSERT0(VAR_is_global(var));

    Section * section;
    if (var->is_readonly() || var->is_string()) {
        section = m_cgmgr->getRodataSection();
    } else if (var->has_init_val()) {
        section = m_cgmgr->getDataSection();
    } else {
        section = m_cgmgr->getBssSection();
    }
    ASSERT0(section);

    VarDesc * vd;
    if (!SECT_var_list(section).find(var)) {
        //Add Var into var-list of 'section'.
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));
        SECT_size(section) = ceil_align(SECT_size(section), VAR_align(var));
        VD_ofst(vd) = (ULONG)SECT_size(section);
        SECT_size(section) += var->getByteSize(m_tm);
        SECT_var2vdesc_map(section).set(var, vd);
        SECT_var_list(section).append_tail(var);
    } else {
        vd = SECT_var2vdesc_map(section).get(var);
    }

    if (base != nullptr) {
        *base = genVAR(SECT_var(section));
    }
    if (base_ofst != nullptr) {
        *base_ofst = genIntImm((HOST_INT)vd->getOfst(), false);
    }
}


//Return the combination of all of available function unit of 'o'.
UnitSet const* CG::computeORUnit(OR const* o, OUT UnitSet * us)
{
    ORTypeDesc const* otd = tmGetORTypeDesc(o->getCode());
    if (us != nullptr) {
        us->bunion(OTD_unit(otd));
        return us;
    }
    m_tmp_us.clean();
    m_tmp_us.bunion(OTD_unit(otd));
    return &m_tmp_us;
}


//Allocate 'var' on stack.
//base: can be one of stack pointer(SP) or frame pointer(FP).
//base_ofst: the byte offset related to 'base'.
void CG::computeAndUpdateStackVarLayout(xoc::Var const* var,
                                        OUT SR ** base, OUT SR ** base_ofst)
{
    ASSERT0(var && base && base_ofst);
    ASSERT0(var->is_local());
    if (var->is_formal_param()) {
        computeParamLayout(var, base, base_ofst);
        return;
    }

    Section * section = m_cgmgr->getStackSection();
    VarDesc * vd = nullptr;
    if (!section->getVarList()->find(var)) {
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));
        //Compute the start address of variable.
        //>Do not perform aligning for stack-variables.
        //>Because stack variable's address is computed by SP + OFFSET, we
        //>could not tell the value of SP until runtime. Thus we can not
        //>guarantee the value of (SP + OFFSET) is aligned as variable declared
        //>unless inserting a computation of ceil-align, which is low-
        //>performance.
        //
        //UINT align = xcom::slcm(  //align variable in lcm of
        //   STACK_ALIGNMENT,       //variable's alignment and
        //   var->get_align() != 0 ?  //STACK default value.
        //       var->get_align() : 1);
        UINT align = STACK_ALIGNMENT;
        VD_ofst(vd) = (ULONG)xcom::ceil_align(section->getSize(), align);

        //Compute the byte size of variable and padding stack with alignment.
        //Prepare the start address for next variable.
        ASSERTN(!var->is_any(), ("variable '%s' is ANY type",
                                 var->get_name()->getStr()));
        SECT_size(section) += xcom::ceil_align(var->getByteSize(m_tm),
                                               STACK_ALIGNMENT);
        section->getVar2Desc()->set(var, vd);
        section->getVarList()->append_tail(var);
    } else {
        vd = section->getVar2Desc()->get(var);
    }

    if (isUseFP()) {
        //Using frame pointer.
        if (base != nullptr) {
            *base = getFP();
        }
        if (base_ofst != nullptr) {
            if (isComputeStackOffset()) {
                *base_ofst = genIntImm((HOST_INT)-(LONG)vd->getOfst(), true);
            } else {
                *base_ofst = genVAR(var);
            }
        }
        return;
    }

    //Using stack pointer.
    if (base != nullptr) {
        *base = getSP();
    }
    if (base_ofst != nullptr) {
        if (isComputeStackOffset()) {
            *base_ofst = genIntImm((HOST_INT)vd->getOfst(), false);
        } else {
            *base_ofst = genVAR(var);
        }
    }
}


//Compute and allocate var into '.param' section.
//base: may be stack pointer(SP) or frame pointer(FP).
//base_ofst: the byte offset corresponds to 'base'.
void CG::computeParamLayout(xoc::Var const* var, OUT SR ** base, OUT SR ** ofst)
{
    ASSERT0(var && var->is_local());
    Section * section = m_cgmgr->getParamSection();
    VarDesc * vd = nullptr;
    if (section->getVarList()->find(var) == 0) {
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));

        //Do not attempt to align stack-variable.
        //Because stack variable's address is computed by SP + OFFSET, we
        //could not tell the value of SP untill runtime. So we can not
        //guarantee the value of (SP + OFFSET) is aligned in variable declared
        //unless we always insert a computation of ceil-align, and that is low-
        //performance.
        VD_ofst(vd) = (ULONG)section->getSize();

        //Align parameter always in STACK default value.
        UINT align = STACK_ALIGNMENT;

        ASSERT0(!var->is_any());
        SECT_size(section) += xcom::ceil_align(var->getByteSize(m_tm), align);
        section->getVar2Desc()->set(var, vd);
        section->getVarList()->append_tail(var);
    } else {
        vd = section->getVar2Desc()->get(var);
    }
    ASSERT0(vd);

    if (base != nullptr) {
        //*base = genParamPointer();
        if (isUseFP()) {
            *base = getFP();
        } else {
            *base = getSP();
        }
    }

    if (ofst != nullptr) {
        if (isComputeStackOffset()) {
            *ofst = genIntImm((HOST_INT)vd->getOfst(), false);
        } else {
            *ofst = genVAR(var);
        }
    }
}


SR * CG::computeAndUpdateOffset(SR * sr)
{
    ASSERT0(sr);
    xoc::Var const* var = SR_var(sr);
    if (var->is_formal_param()) {
        ASSERTN(m_param_sect_start_offset != -1,
                ("should have been computed at"
                 " reviseFormalParameterAndSpadjust"));
        VarDesc const* vd = SECT_var2vdesc_map(
            m_cgmgr->getParamSection()).get(var);
        HOST_UINT l = m_param_sect_start_offset + (HOST_UINT)vd->getOfst();
        //Recompute the alignment of variable.
        //>Do not perform aligning for stack-variables.
        //>Because stack variable's address is computed by SP + OFFSET, we
        //>could not tell the value of SP until runtime. Thus we can not
        //>guarantee the value of (SP + OFFSET) is aligned as variable declared
        //>unless inserting a computation of ceil-align, which is low-
        //>performance.
        //l = xcom::ceil_align(l, (HOST_UINT)var->get_align());

        //Compute byte offset inside variable.
        l += (HOST_UINT)SR_var_ofst(sr);

        return genIntImm((HOST_INT)l, false);
    }

    VarDesc * vd = SECT_var2vdesc_map(m_cgmgr->getStackSection()).get(var);
    ASSERT0(vd);
    HOST_UINT x = (HOST_UINT)getMaxArgSectionSize() + (HOST_UINT)vd->getOfst();
    //Recompute the alignment of variable.
    //>Do not perform aligning for stack-variables.
    //>Because stack variable's address is computed by SP + OFFSET, we
    //>could not tell the value of SP until runtime. Thus we can not
    //>guarantee the value of (SP + OFFSET) is aligned as variable declared
    //>unless inserting a computation of ceil-align, which is low-
    //>performance.
    //x = xcom::ceil_align(x, (HOST_UINT)var->get_align());

    //Compute byte offset inside variable.
    x += (HOST_UINT)SR_var_ofst(sr);

    return genIntImm((HOST_INT)x, false);
}


//Compute the offset for stack variable and
//supersede the symbol variable with the offset.
void CG::relocateStackVarOffset()
{
    START_TIMER(t0, "Relocate Stack Variable Offset");
    List<ORBB*> * bblist = getORBBList();
    for (ORBB * bb = bblist->get_head();
         bb != nullptr; bb = bblist->get_next()) {
        if (ORBB_ornum(bb) == 0) { continue; }

        ORCt * ct;
        for (OR * o = ORBB_orlist(bb)->get_head(&ct);
             o != nullptr; o = ORBB_orlist(bb)->get_next(&ct)) {
            if (o->is_load()) {
                SR * ofst = o->get_load_ofst();
                ASSERT0(ofst);
                if (!ofst->is_var()) { continue; }

                o->set_load_ofst(computeAndUpdateOffset(ofst), this);
                OR_is_need_compute_var_ofst(o) = false;
            } else if (o->is_store()) {
                SR * ofst = o->get_store_ofst();
                ASSERT0(ofst);
                if (!ofst->is_var()) { continue; }

                o->set_store_ofst(computeAndUpdateOffset(ofst), this);
                OR_is_need_compute_var_ofst(o) = false;
            } else if (o->needComputeVAROfst()) {
                for (UINT i = 0; i < o->result_num(); i++) {
                    SR * res = o->get_result(i);
                    if (!res->is_var()) { continue; }

                    o->set_result(i, computeAndUpdateOffset(res), this);
                }

                for (UINT i = 0; i < o->opnd_num(); i++) {
                    SR * opnd = o->get_opnd(i);
                    if (!opnd->is_var()) { continue; }

                    o->set_opnd(i, computeAndUpdateOffset(opnd), this);
                }

                OR_is_need_compute_var_ofst(o) = false;
            }
        }
    }
    END_TIMER(t0, "Relocate Stack Variable Offset");

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpRelocateStack()) {
        xoc::note(getRegion(),
                  "\n==---- DUMP AFTER RELOCATE STACK OFFSET ----==");
        m_cgmgr->getParamSection()->dump(this);
        m_cgmgr->getStackSection()->dump(this);
        dumpORBBList();
    }
}


//The function compute the base SR and offset SR to given 'var'.
//var_ofst: byte offset related to begin address of 'var'.
void CG::computeVarBaseAndOffset(xoc::Var const* var, ULONGLONG var_ofst,
                                 OUT SR ** base, OUT SR ** ofst)
{
    ASSERT0(var && base && ofst);
    *base = *ofst = nullptr;
    if (var->is_local()) {
        computeAndUpdateStackVarLayout(var, base, ofst);
        //Add constant byte offset into 'ofst'.
        if (isComputeStackOffset()) {
            ASSERTN((*ofst)->is_int_imm(), ("offset must be imm"));
            SR_int_imm(*ofst) += (HOST_INT)var_ofst;
        } else {
            ASSERTN((*ofst)->is_var(), ("offset must be var"));
            SR_var_ofst(*ofst) += (UINT)var_ofst;
        }
        return;
    }

    ASSERT0(var->is_global());
    computeAndUpdateGlobalVarLayout(var, base, ofst);

    //Add constant byte offset into 'ofst'.
    //ofst can only be one of IMM and VAR.
    if ((*ofst)->is_int_imm()) {
        SR_int_imm(*ofst) += (HOST_INT)var_ofst;
    } else {
        ASSERTN((*ofst)->is_var(), ("offset must be var"));
        SR_var_ofst(*ofst) += (UINT)var_ofst;
    }
}


xoc::Var const* CG::computeSpillVar(OR const* o) const
{
    if (o->is_load() || o->is_store()) {
        return mapOR2Var(o);
    }
    return nullptr;
}


void CG::freePackage()
{
    m_ip_mgr.destroy();
    m_ipl_vec.clean();
}


void CG::initDedicatedSR()
{
    genSP();
    genFP();
    genGP();
    genTruePred();
    genRflag();
    genParamPointer();
}


//Initialize machine info before compiling function unit.
//e.g: prepare stack info, parameter info.
void CG::initFuncUnit()
{
    ASSERT0(!m_rg->is_blackbox());

    //Initializing formal parameter section.
    ASSERT0(m_rg->getRegionVar());

    List<xoc::Var const*> param_list;
    m_rg->findFormalParam(param_list, true);

    UINT i = 0;
    for (xoc::Var const* v = param_list.get_head();
         v != nullptr; v = param_list.get_next()) {
         //Append parameter into PARAM-Section in turn.
         computeParamLayout(v, nullptr, nullptr);
         m_params.set(i, v);
    }

    m_param_sect_start_offset = -1;
}


void CG::initGlobalVar(VarMgr * vm)
{
    //Record global Var in .data section.
    VarVec * varvec = vm->getVarVec();
    SR * base, * ofst;
    for (INT i = 0; i <= varvec->get_last_idx(); i++) {
        xoc::Var * v = varvec->get(i);
        if (v == nullptr) { continue; }

        if (v->is_global() && !v->is_unallocable() && !v->is_fake() &&
            //Do not add file-region-private-var
            //into DATA section ahead of time.
            !v->is_private()) {
            computeVarBaseAndOffset(v, 0, &base, &ofst);
        }
    }
}


void CG::finiFuncUnit()
{
    //Reset target machine for next function compilation.
    m_dedicate_sr_tab.clean();
    m_param_pointer = nullptr;
    m_reg_count = 0;
    m_param_sect_start_offset = -1;
    freePackage();
}


//Duplicate OR with unique id.
OR * CG::dupOR(OR const* o)
{
    ASSERT0(o);
    OR * n = genOR(o->getCode());
    n->clone(o, this);
    return n;
}


SR * CG::dupSR(SR const* sr)
{
    SR * n = getSRMgr()->genSR();
    n->copy(sr);
    return n;
}


UINT CG::compute_pad()
{
    UINT maxpad = 0;
    for (INT bbid = 0; bbid <= m_ipl_vec.get_last_idx(); bbid++) {
        IssuePackageList * ipl = m_ipl_vec.get(bbid);
        if (ipl == nullptr) { continue; }

        IssuePackageListIter ipit;
        for (ipit = ipl->get_head(); ipit != ipl->end();
             ipit = ipl->get_next(ipit)) {
            IssuePackage * ip = ipit->val();
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                OR * o = ip->get(s);
                if (o == nullptr) { continue; }
                maxpad = MAX(maxpad, (UINT)strlen(o->getCodeName()));
            }
        }
    }
    return maxpad;
}


void CG::dumpPackage()
{
    if (!getRegion()->isLogMgrInit()) { return; }
    INT org = getRegion()->getLogMgr()->getIndent();
    getRegion()->getLogMgr()->setIndent(0);
    note(getRegion(), "\n==---- DUMP Package, Region(%d)'%s' ----==",
         getRegion()->id(),
         getRegion()->getRegionName() == nullptr ?
             "--" : getRegion()->getRegionName());

    xcom::StrBuf format(128);
    format.strcat("%%-%ds", compute_pad());

    for (INT bbid = 0; bbid <= m_ipl_vec.get_last_idx(); bbid++) {
        IssuePackageList * ipl = m_ipl_vec.get(bbid);
        if (ipl == nullptr) { continue; }

        note(getRegion(), "\n-- BB%d --", bbid);
        IssuePackageListIter ipit;
        for (ipit = ipl->get_head(); ipit != ipl->end();
             ipit = ipl->get_next(ipit)) {
            note(getRegion(), "\n{");
            IssuePackage * ip = ipit->val();
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                if (s != FIRST_SLOT) { note(getRegion(), "|"); }

                OR * o = ip->get(s);
                if (o == nullptr) {
                    prt(getRegion(), format.buf, " ");
                } else {
                    prt(getRegion(), format.buf, o->getCodeName());
                }
            }

            note(getRegion(), "}");

            //Dump instruction details.
            getRegion()->getLogMgr()->incIndent(4);
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                if (s != FIRST_SLOT) { note(getRegion(), "|"); }

                OR * o = ip->get(s);
                if (o == nullptr) {
                    note(getRegion(), "\nnop");
                } else {
                    o->dump(this);
                }
            }
            getRegion()->getLogMgr()->decIndent(4);
        }
    }
    getRegion()->getLogMgr()->setIndent(org);
}


void CG::dumpSection()
{
    if (!getRegion()->isLogMgrInit()) { return; }
    note(getRegion(), "\n==---- DUMP Section Var info ----==\n");
    m_cgmgr->getCodeSection()->dump(this);
    m_cgmgr->getDataSection()->dump(this);
    m_cgmgr->getRodataSection()->dump(this);
    m_cgmgr->getBssSection()->dump(this);
    m_cgmgr->getStackSection()->dump(this);
    m_cgmgr->getParamSection()->dump(this);
}


void CG::dumpORBBList() const
{
    xgen::dumpORBBList(*const_cast<CG*>(this)->getORBBList(),
                       const_cast<CG*>(this));
}


void CG::dumpVar() const
{
    note(getRegion(), "\n==---- DUMP Variables in CG '%s' ----==",
         m_rg->getRegionName());
    xcom::C<Var*> * it;
    if (m_bb_level_internal_var_list.get_elem_count() != 0) {
        note(getRegion(), "\n==-- BB LEVEL VAR, NUM%d --==",
             m_bb_level_internal_var_list.get_elem_count());
        for (m_bb_level_internal_var_list.get_head(&it); it != nullptr;
             m_bb_level_internal_var_list.get_next(&it)) {
            Var * v = it->val();
            v->dump(m_tm);
        }
    }

    xcom::C<Var*> * it2;
    if (m_func_level_internal_var_list.get_elem_count() != 0) {
        note(getRegion(), "\n==-- FUNC LEVEL VAR, NUM%d --==",
             m_func_level_internal_var_list.get_elem_count());
        for (m_func_level_internal_var_list.get_head(&it2); it2 != nullptr;
             m_func_level_internal_var_list.get_next(&it2)) {
            Var * v = it2->val();
            v->dump(m_tm);
        }
    }
}


void CG::setMapOR2Mem(OR * o, xoc::Var const* m)
{
    if (o->is_load() || o->is_store()) {
        m_or2memaddr_map.set(o, m);
    } else {
        UNREACHABLE();
    }
}


xoc::Var const* CG::mapOR2Var(OR const* o) const
{
    if (o->is_load() || o->is_store()) {
        return m_or2memaddr_map.get(const_cast<OR*>(o));
    }
    return nullptr;
}


//Compute regfile set that 'regs' indicated.
bool CG::mapRegSet2RegFile(MOD Vector<INT> & regfilev, RegSet const* regs)
{
    for (BSIdx reg = regs->get_first(); reg != BS_UNDEF;
         reg = regs->get_next(reg)) {
        ASSERTN(reg > 0, ("First register number starts from zero at least."));
        REGFILE regfile = tmMapReg2RegFile(reg);
        INT count = regfilev.get(regfile);
        regfilev.set(regfile, ++count);
    }
    return true;
}


UNIT CG::mapSR2Unit(OR const*, SR const* sr) const
{
    if (sr->is_reg() && sr->getRegFile() != RF_UNDEF) {
        UnitSet us;
        return mapRegFile2UnitSet(sr->getRegFile(), sr, us).checkAndGet();
    }
    return UNIT_UNDEF;
}


CLUST CG::mapSlot2Cluster(SLOT slot)
{
    DUMMYUSE(slot);

    //Default target-machine has one cluster.
    return (CLUST)(CLUST_UNDEF + 1);
}


//Change the function unit and related cluster of 'o'.
//If is_test is true, this function only check whether the given
//OR can be changed.
bool CG::changeORUnit(OR * o, UNIT to_unit, CLUST to_clust,
                      RegFileSet const* regfile_unique, bool is_test)
{
    ASSERTN(o && to_unit != 0 && to_clust != CLUST_UNDEF, ("o is nullptr"));

    //Get corresponding opcode.
    OR_TYPE new_opc = computeEquivalentORType(o->getCode(), to_unit, to_clust);
    if (new_opc == OR_UNDEF) {
        return false;
    }

    //Regfile needs to replaced amenable for instruction-constraint.
    //Thus, we perform conservative inspection for regfiles of 'o'.
    UNIT from_unit = computeORUnit(o)->checkAndGet();
    CLUST from_clust = computeORCluster(o);

    //Check regfile resource constraints.
    if (from_unit != to_unit || from_clust != to_clust) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (isBusSR(sr)) {
                continue;
            }

            if (sr->is_reg() &&
                regfile_unique != nullptr &&
                regfile_unique->is_contain(SR_sregid(sr))) {
                //Even if regfile of 'sr' has been marked as UNIQUE,
                //but some of them still have chance to change to other
                //function unit when the new function unit could also
                //use the regfile of 'sr'.
                ASSERTN(sr->getRegFile() != RF_UNDEF, ("illegal unique regfile"));
                //First handle the specical case.
                if (sr == getSP()) {
                    if (!isSPUnit(to_unit))  {
                        return false;
                    }
                } else if (isValidResultRegfile(new_opc, i, sr->getRegFile())) {
                    if (mapRegFile2Cluster(sr->getRegFile(), sr) != to_clust) {
                        //Each cluster have their own general
                        //register file, and the cluster can only access their
                        //own general register file.
                        return false;
                    }
                } else {
                    //'new_opc' can not operate the unique regfile.
                    return false;
                }
            } //end if
        } //end for each of result

        for (i = 0; i < o->opnd_num(); i++) {
            SR * sr = o->get_opnd(i);
            if (isBusSR(sr)) {
                continue;
            }
            if (sr->is_reg() &&
                regfile_unique != nullptr &&
                regfile_unique->is_contain(SR_sregid(sr))) {
                ASSERTN(sr->getRegFile() != RF_UNDEF,
                        ("Regfile unique sr should alloated regfile"));
                //First handle the specical case.
                if (sr == getSP()) {
                    if (!isSPUnit(to_unit))  {
                        return false;
                    }
                } else if (isValidOpndRegfile(new_opc, i, sr->getRegFile())) {
                    if (mapRegFile2Cluster(sr->getRegFile(), sr) != to_clust) {
                        //Each cluster have their own general
                        //register file, and the cluster can only access their
                        //own general register file.
                        return false;
                    }
                } else {
                    //'new_opc' can not operate the unique regfile.
                    return false;
                }
            }
        } //end for each of opnd
    } //end if (from_unit != to_unit || from_clust != to_clust) {

    if (is_test) {
        return true;
    }

    if (!changeORType(o, new_opc, from_clust, to_clust, regfile_unique)) {
        ASSERTN(0, ("OR_TYPE(%s) has NO alternative on the given unit!",
                o->getCodeName()));
        return false;
    }

    OR_clust(o) = to_clust;
    return true;
}


//Change the correlated cluster of 'o'
//If is_test is true, this function only check whether the given
//OR can be changed.
bool CG::changeORCluster(OR * o, CLUST to_clust,
                         RegFileSet const* regfile_unique, bool is_test)
{
    if (o->is_bus() ||
        o->is_asm() ||
        o->is_fake() ||
        to_clust == CLUST_UNDEF) {
        return false;
    }
    UNIT to_unit = computeEquivalentORUnit(o, to_clust);
    return changeORUnit(o, to_unit, to_clust, regfile_unique, is_test);
}


//Change 'o' to 'ot', modifing all operands and results.
bool CG::changeORType(OR * o, OR_TYPE ot, CLUST src, CLUST tgt,
                      RegFileSet const* regfile_unique)
{
    DUMMYUSE(src);
    ASSERTN(tgt != CLUST_UNDEF, ("requires cluster info"));
    UINT i;
    //Performing verification and substitution certainly.
    for (i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (!sr->is_reg() || sr->is_pred()) { continue; }

        //Handle dedicated SR.
        if (SR_is_dedicated(sr)) {
            ASSERTN(0, ("TODO"));
            continue;
        }

        //Handle general SR.
        if (sr->getPhyReg() != REG_UNDEF) {
            ASSERTN(sr->getRegFile() != RF_UNDEF, ("Loss regfile info"));
            ASSERTN(tgt == mapReg2Cluster(sr->getPhyReg()), ("Unmatch info"));
            continue;
        }
        if (regfile_unique == nullptr ||
            !regfile_unique->is_contain(SR_sregid(sr))) {
            //Reassign regfile.
            SR_phy_reg(sr) = REG_UNDEF;
            SR_regfile(sr) = RF_UNDEF;
        }
    }

    for (i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (!sr->is_reg() || sr->is_pred()) { continue; }

        //Handle dedicated sr.
        if (SR_is_dedicated(sr)) {
            ASSERTN(0, ("TODO"));
            continue;
        }

        //Handle general sr.
        if (sr->getPhyReg() != REG_UNDEF) {
            ASSERTN(sr->getRegFile() != RF_UNDEF, ("Loss regfile info"));

            //When 'sr' has been assigned register, the 'tgt' cluster
            //must be as same as the cluster which 'sr' correlated to.
            //ASSERTN(tgt == mapReg2Cluster(sr->getPhyReg()),
            //       ("Unmatch info"));
            if (tgt != mapReg2Cluster(sr->getPhyReg())) {
                xoc::interwarn("Unmatch info, may generate redundant copy.");
                return false;
            }
            continue;
        }
        if (regfile_unique == nullptr ||
            !regfile_unique->is_contain(SR_sregid(sr))) {
            //Reassign regfile.
            SR_phy_reg(sr) = REG_UNDEF;
            SR_regfile(sr) = RF_UNDEF;
        }
    }
    OR_code(o) = ot;
    return true;
}


//Return true if regfile can be assigned to referred operand.
bool CG::isValidRegFile(OR_TYPE ot, INT opndnum,
                        REGFILE regfile, bool is_result) const
{
    if (is_result) {
        return isValidResultRegfile(ot, opndnum, regfile);
    } else {
        return isValidOpndRegfile(ot, opndnum, regfile);
    }
}


//Return true if regfile can be assigned to referred operand.
bool CG::isValidRegFile(OR * o, SR const* opnd, REGFILE regfile,
                        bool is_result) const
{
    bool is_valid = false;

    //Note:'o' may owns a number of SR like opnd,
    //and all of them must be checked.
    //e.g: store t1, t1, 0x100  //means [t1+0x100] = t1
    if (is_result) {
        for (UINT i = 0; i < o->result_num(); i++) {
            if (isSREqual(o->get_result(i), opnd)) {
                if (isValidResultRegfile(o->getCode(), i, regfile)) {
                    is_valid = true;
                } else {
                    return false;
                }
            }
            //TODO: Should check if regfile is consistent with other operand.
        }
        return is_valid;
    }

    //operand
    for (UINT i = 0; i < o->opnd_num(); i++) {
        if (isSREqual(opnd, o->get_opnd(i))) {
            if (isValidOpndRegfile(o->getCode(), i, regfile)) {
                is_valid = true;
            } else {
                return false;
            }
        }
        //TODO: Should check if regfile is consistent with other operand.
    }
    return is_valid;
}


//Checking for the safe condition of copy-value.
bool CG::isConsistentRegFileForCopy(REGFILE rf1, REGFILE rf2)
{
    if (rf1 == rf2) { return true; }
    return false;
}


bool CG::isCompareOR(OR const* o) const
{
    return OR_is_eq(o) || OR_is_lt(o) || OR_is_gt(o);
}


//Is o predicated by TRUE condition?
bool CG::isCondExecOR(OR const* o) const
{
    if (OR_is_predicated(o)) {
        SR * sr = const_cast<OR*>(o)->get_pred();
        ASSERT0(sr);
        if (sr != getTruePred()) {
            return true;
        }
    }
    return false;
}


//TODO: support integer multiplication, logical operation, etc.
bool CG::isReductionOR(OR const* o) const
{
    ASSERTN(0, ("Target Dependent Code"));
    DUMMYUSE(o);
    return false;
}


//Which case is safe to optimize?
//If 'prev' is must-execute, 'next' is cond-execute, we say that is
//safe to optimize.
//Since must-execute operation is the dominator in execute-path.
//Otherwise is not absolutely safe.
bool CG::isSafeToOptimize(OR const* prev, OR const* next) const
{
    DUMMYUSE(prev);
    DUMMYUSE(next);
    SR * p1 = const_cast<OR*>(prev)->get_pred();
    if (p1 == nullptr || p1 == getTruePred()) {
        return true;
    }
    return false;
}


bool CG::isSREqual(SR const* sr1, SR const* sr2) const
{
    if (sr1 == sr2) {
        return true;
    }
    if (sr1->is_reg() && sr2->is_reg() &&
        SR_regfile(sr1) == SR_regfile(sr2) &&
        SR_phy_reg(sr1) != REG_UNDEF &&
        SR_phy_reg(sr2) != REG_UNDEF &&
        SR_phy_reg(sr1) == SR_phy_reg(sr2)) {
        return true;
    }
    return false;
}


//Return true if both or1 and or2 are spill operation, as well as the
//spill location.
bool CG::isSameSpillLoc(OR const* or1, OR const* or2)
{
    xoc::Var const* or1loc = computeSpillVar(or1);
    return isSameSpillLoc(or1loc, or1, or2);
}


//Return true if both or1 and or2 are spill operation, as well as the
//spill location.
bool CG::isSameSpillLoc(xoc::Var const* or1loc, OR const* or1, OR const* or2)
{
    ASSERT0(OR_is_mem(or1) && OR_is_mem(or2));

    xoc::Var const* or2loc = computeSpillVar(or2);

    if (or1loc != or2loc) { return false; }

    SR const* or1ofst = nullptr;
    if (OR_is_load(or1)) {
        or1ofst = const_cast<OR*>(or1)->get_load_ofst();
    } else {
        ASSERT0(OR_is_store(or1));
        or1ofst = const_cast<OR*>(or1)->get_store_ofst();
    }
    ASSERT0(or1ofst);

    SR const* or2ofst = nullptr;
    if (OR_is_load(or2)) {
        or2ofst = const_cast<OR*>(or2)->get_load_ofst();
    } else {
        ASSERT0(OR_is_store(or2));
        or2ofst = const_cast<OR*>(or2)->get_store_ofst();
    }
    ASSERT0(or2ofst);

    if ((or1ofst->is_var() &&
         or2ofst->is_var() &&
         SR_var(or1ofst) == SR_var(or2ofst) &&
         SR_var_ofst(or1ofst) == SR_var_ofst(or2ofst)) ||
        (or1ofst->is_int_imm() &&
         or2ofst->is_int_imm() &&
         or1ofst->getInt() == or2ofst->getInt())) {
        return true;
    }

    return false;
}


//Return true  if both 'prev' and 'next' are executive on same condtions.
//'prev' must be ordered prior to 'next' in ORBB.
//The same predicate register also should not be defined
//between 'prev' and 'next'.
bool CG::isSameCondExec(OR * prev, OR * next, BBORList const* or_list)
{
    SR * p1 = prev->get_pred();
    SR * p2 = next->get_pred();
    if (!p1 && !p2) {
        return true;
    }
    if (isSREqual(p1, getTruePred()) &&
        isSREqual(p2, getTruePred())) {
        return true;
    }

    //CASE:Cannot only compare p1==p2, e.g:
    //         = t1(p1) //prev
    //  t1(p1) =
    //         = t1(p1) //next
    //  prev and next are not same cond-exec.

    ORCt * ct = nullptr;
    BBORList * torlst = const_cast<BBORList*>(or_list);
    torlst->find(prev, &ct);
    if (isSREqual(p1, p2)) {
        SR * pd = p1;
        OR * test;
        for (test = torlst->get_next(&ct);
             test != nullptr; test = torlst->get_next(&ct)) {
            if (test == next) {
                break;
            }
            if (mayDef(test, pd)) {
                return false;
            }
        }
        ASSERTN(test, ("next-o does not placed behind prev-o"));
        return true;
    }
    return false;
}


bool CG::isSameCluster(OR const* or1, OR const* or2) const
{
    CLUST op1_clus = computeORCluster(or1);
    CLUST op2_clus = computeORCluster(or2);
    if (op1_clus != op2_clus ||
        op1_clus == CLUST_UNDEF ||
        op2_clus == CLUST_UNDEF) {
        return false;
    }
    return true;
}


//Check 'regfile' to determine whether it is correct relatived to the 'opndnum'
//operand of 'opcode'.
bool CG::isValidOpndRegfile(OR_TYPE ortype,
                            INT opndnum,
                            REGFILE regfile) const
{
    if (regfile == RF_UNDEF || opndnum < 0) {
        return false;
    }
    RegFileSet const* rfs = getValidRegfileSet(ortype, opndnum, false);
    ASSERTN(rfs != nullptr, ("miss target machine info"));
    if (rfs->is_contain(regfile)) {
        return true;
    }
    return false;
}


//Check if 'sr' has allocated valid physical register corresponding to
//SR-VEC of 'or'.
//'or':
//'sr':
//'idx': opnd/result index of 'sr'.
//'is_result': it is true if 'sr' being the result of 'o'.
bool CG::isValidRegInSRVec(OR const*, SR const* sr,
                           UINT idx, bool is_result) const
{
    DUMMYUSE(is_result);
    DUMMYUSE(idx);
    ASSERTN(0, ("Target Dependent Code"));
    if (sr->getVec() != nullptr) {
        //Do some verification.
        return true;
    }
    return true;
}


bool CG::isValidResultRegfile(OR_TYPE ortype, INT resnum, REGFILE regfile) const
{
    if (ortype == OR_asm) {
        return false;
    }
    if (regfile == RF_UNDEF || resnum < 0) {
        return false;
    }
    RegFileSet const* rfs = getValidRegfileSet(ortype, resnum, true);
    ASSERTN(rfs, ("absence of target machine info"));
    return rfs->is_contain(regfile);
}


//How to determine whether if an operation is reducible?
//1.Find the DEF of induction variable.
//2.Induction variable should be global register.
//3.Matching the reduction opcode.
bool CG::isReduction(OR const* o) const
{
    if (isCondExecOR(o)) { return false; }

    INT reduct_opnd = 0;
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (!opnd->is_reg() ||
            !opnd->is_global() ||
            opnd == getTruePred()) {
            continue;
        }

        if (mustDef(o, opnd)) {
            reduct_opnd++;
        }
    }

    if (reduct_opnd == 0) { return false; }

    //Determining the OR-type of reduction-operation.
    ASSERTN(0, ("Target Dependent Code"));
    return false;
}


//Return true if specified operand or result is Rflag register.
bool CG::isRflagRegister(OR_TYPE ot, UINT idx, bool is_result) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ot);
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    REGFILE rflag = getRflagRegfile();
    if (is_result) {
        SRDesc const* sr_desc = sdg->get_res(idx);
        if (SRD_valid_regfile_set(sr_desc)->is_contain(rflag)) {
            return true;
        }
        return false;
    }

    SRDesc const* sr_desc = sdg->get_opnd(idx);
    if (SRD_valid_regfile_set(sr_desc)->is_contain(rflag)) {
        return true;
    }
    return false;
}


//Return true if specified immediate operand is in valid range.
bool CG::isValidImmOpnd(OR const* o) const
{
    OR_TYPE ot = o->getCode();
    ORTypeDesc const* otd = tmGetORTypeDesc(ot);
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    for (UINT i = 0; i < tmGetOpndNum(ot); i++) {
        SRDesc const* sr_desc = sdg->get_opnd(i);
        ASSERT0(sr_desc);
        if (!SRD_is_imm(sr_desc)) { continue; }
        SR * imm_opnd = o->get_opnd(i);
        if (imm_opnd->is_var() || imm_opnd->is_label()) {
            continue;
        }
        ASSERT0(imm_opnd && imm_opnd->is_int_imm());
        if (!isValidImmOpnd(ot, i, imm_opnd->getInt())) {
            return false;
        }
    }
    return true;
}


//Return true if specified immediate operand is in valid range.
bool CG::isValidImmOpnd(OR_TYPE ot, HOST_INT imm) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ot);
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    for (UINT i = 0; i < tmGetOpndNum(ot); i++) {
        SRDesc const* sr_desc = sdg->get_opnd(i);
        ASSERT0(sr_desc);
        if (!SRD_is_imm(sr_desc)) { continue; }
        if (!isValidImmOpnd(ot, i, imm)) {
            return false;
        }
    }
    return true;
}


//Return true if specified immediate operand is in valid range.
bool CG::isValidImmOpnd(OR_TYPE ot, UINT idx, HOST_INT imm) const
{
    SRDesc const* sr_desc = tmGetOpndSRDesc(ot, idx);
    ASSERT0(sr_desc && SRD_is_imm(sr_desc));
    return isValidImm(SRD_bitsize(sr_desc), imm);
}


//Return true if specified immediate in
//valid range that described with bitsize.
bool CG::isValidImm(UINT bitsize, HOST_INT imm) const
{
    //Clear the high non-zero bit.
    //e.g: given imm 0xffffFFFF80000000, clean high 32bit.
    HOST_UINT pimm = xcom::xabs(imm);
    HOST_UINT mask = (((HOST_UINT)1) << bitsize) - 1;
    return (mask & pimm) == pimm;
}


//Calculate the cluster for inline-assembly operation.
CLUST CG::computeAsmCluster(IN OR * o)
{
    if (!o->is_asm()) {
        return CLUST_UNDEF;
    }

    //Get cluster information from clobber registers.
    ORAsmInfo * asm_info = getAsmInfo(o);
    if (!asm_info) {
        return CLUST_UNDEF;
    }

    //Check clobber set and record the expected cluster of result.
    SR * sr;
    UINT i;
    CLUST res_clust = CLUST_UNDEF;
    for (i = 0; i < o->result_num(); i++) {
        sr = o->get_result(i);
        if (!sr->is_reg()) { continue; }

        //Check clobber set.
        //TODO: To determine is it actually necessary?
        //RegSet clobber_set = ASM_OR_clobber_set(asm_info);
        //for (reg = clobber_set.get_first(); reg != REG_UNDEF;
        //       reg = clobber_set.get_next(reg)) {
        //    CLUST cur = mapReg2Cluster(reg);
        //    if (res_clust == CLUST_UNDEF) {
        //        res_clust = cur;
        //    } else if (cur != CLUST_UNDEF && cur != res_clust) {
        //        //Asm operation cross multi clusters.
        //        res_clust = CLUST_BUS;
        //    }
        //}

        //Compute cluster info by checking result-sr allowed register.
        SRDescGroup<> const* sdg = asm_info->get_srd_group();
        SRDesc const* sd = sdg->get_res(i);
        RegFileSet const* rfs = SRD_valid_regfile_set(sd);
        for (REGFILE rf = (REGFILE)rfs->get_first();
             rf != RF_UNDEF; rf = (REGFILE)rfs->get_next(rf)) {
            RegSet const* rs = tmMapRegFile2RegSet(rf);
            for (REG reg = rs->get_first();
                 rs != REG_UNDEF; reg = rs->get_next(reg)) {
                CLUST cur = mapReg2Cluster(reg);
                if (res_clust == CLUST_UNDEF) {
                    res_clust = cur;
                } else if (cur != CLUST_UNDEF && cur != res_clust) {
                    //Asm operation cross multiple clusters.
                    ASSERTN(0, ("Target Dependent Code"));
                }
            }
        }
    }

    CLUST opnd_clust = CLUST_UNDEF;
    for (i = 0; i < o->opnd_num(); i++) {
        sr = o->get_opnd(i);
        if (!sr->is_reg()) {
            continue;
        }

        //Compute cluster info by checking opnd-sr allowed register.
        SRDescGroup<> const* sdg = asm_info->get_srd_group();
        SRDesc const* sd = sdg->get_opnd(i);
        RegFileSet const* rfs = SRD_valid_regfile_set(sd);
        for (REGFILE rf = (REGFILE)rfs->get_first(); rf != RF_UNDEF;
            rf = (REGFILE)rfs->get_next(rf)) {

            RegSet const* rs = tmMapRegFile2RegSet(rf);
            for (REG reg = rs->get_first(); rs != REG_UNDEF;
                reg = rs->get_next(reg)) {

                CLUST cur = mapReg2Cluster(reg);
                if (opnd_clust == CLUST_UNDEF) {
                    opnd_clust = cur;
                } else if (cur != CLUST_UNDEF && cur != res_clust) {
                    //Asm operation cross multiple clusters.
                    ASSERTN(0, ("Target Dependent Code"));
                }
            }
        }
    }

    CLUST clust = CLUST_UNDEF;
    if (res_clust != CLUST_UNDEF && opnd_clust != CLUST_UNDEF) {
        if (res_clust != opnd_clust) {
            //Asm operation cross multiple clusters.
            ASSERTN(0, ("Target Dependent Code"));
        } else {
            clust = res_clust;
        }
    } else if (res_clust != CLUST_UNDEF) {
        clust = res_clust;
    } else if (opnd_clust != CLUST_UNDEF) {
        clust = opnd_clust;
    }
    return clust;
}


//Return cluster of bus operation.
CLUST CG::computeClusterOfBusOR(IN OR *)
{
    ASSERTN(0, ("Target Dependent Code"));
    return CLUST_UNDEF;
}


//Used only under single cluster.
CLUST CG::computeORCluster(OR const* o) const
{
    CLUST clust = OR_clust(o);
    if (clust != CLUST_UNDEF) { return clust; }

    //Can not find any cluster info, set default cluster.
    ASSERTN(CLUST_UNDEF + 2 == CLUST_NUM, ("Only one cluster."));
    return (CLUST)(CLUST_UNDEF + 1);
}


SLOT CG::computeORSlot(OR const*)
{
    SLOT slot = FIRST_SLOT;
    ASSERTN(0, ("Target Dependent Code"));
    return slot;
}


//Amendment the illegal BASE-SR of memory load/store.
void CG::fixCluster(MOD ORList & spill_ors, CLUST clust)
{
    DUMMYUSE(spill_ors);
    DUMMYUSE(clust);
    ASSERT0(CLUST_UNDEF + 2 == CLUST_NUM);
}


void CG::flattenInVec(SR * argval, Vector<SR*> * vec)
{
    ASSERT0(argval && vec);
    UINT vec_count = 0;

    if (argval->is_vec()) {
        ASSERTN(SR_vec_idx(argval) == 0, ("expect first element"));
        for (UINT j = 0; j < argval->getVec()->get_elem_count(); j++) {
            vec->set(vec_count, argval->getVec()->get(j));
            vec_count++;
        }
    } else {
        vec->set(vec_count, argval);
        vec_count++;
    }
}


//True if current argument register should be bypassed.
bool CG::skipArgRegister(UINT sz, OUT ArgDescMgr * argdescmgr)
{
    ASSERT0(argdescmgr);
    DUMMYUSE(sz && argdescmgr);
    #ifdef TO_BE_COMPATIBLE_WITH_ARM_LINUX_GNUEABI
    if (sz == CONTINUOUS_REG_NUM * GENERAL_REGISTER_SIZE) {
        ASSERT0(sz == 2 * GENERAL_REGISTER_SIZE);
        RegSet const* rs = argdescmgr->getArgRegSet();
        REG first_reg = rs->get_first();
        if (first_reg == (REG)BS_UNDEF) {
            return false;
        }
        if (!isEvenReg(first_reg) || //bypass odd register

            //There are not enough continuous arg registers.
            rs->get_elem_count() < CONTINUOUS_REG_NUM) {
            argdescmgr->dropArgRegister();
            return true;
        }
    }
    #endif
    return false;
}


//Return true if whole ir has been passed through registers, otherwise
//return false.
bool CG::passArgInMemory(SR * argaddr, UINT * argsz,
                         OUT ArgDescMgr * argdescmgr, OUT ORList & ors,
                         IOC *)
{
    UINT total_size = *argsz;
    IOC tmp_cont;

    //CG compute the ADDR for data rather than generate load operation.
    //Get the address of LoadValue.
    ASSERT0(argaddr);

    //Try to pass data through argument-register.
    //Transfer data in single register size.
    UINT transfer_size = GENERAL_REGISTER_SIZE;
    for (UINT i = 0;; i++) {
        SR * argreg = nullptr;
        if ((*argsz) > 0) {
            argreg = argdescmgr->allocArgRegister(this);
        }
        if (argreg == nullptr) {
            break;
        }
        tmp_cont.clean();
        IOC_mem_byte_size(&tmp_cont) = transfer_size;
        bool is_signed = false; //transfer size is the same length as register.
        buildLoad(argreg, argaddr, genIntImm(i * transfer_size, false),
                  is_signed, ors, &tmp_cont);
        (*argsz) -= transfer_size;
        argdescmgr->updatePassedArgInRegister(transfer_size);
    }

    //There is still remaining data have to be passed through stack.
    if (*argsz > 0) {
        argdescmgr->addAddrDesc(argaddr, nullptr, STACK_ALIGNMENT, *argsz,
                                total_size - *argsz);
        return false;
    }
    return true;
}


//Return true if whole ir has been passed through registers, otherwise
//return false.
bool CG::passArgInRegister(SR * argval, UINT * sz, ArgDescMgr * argdescmgr,
                           ORList & ors, IOC const*)
{
    //Target dependent code.
    skipArgRegister(*sz, argdescmgr);

    //Get the LoadValue and spreads them into vector.
    //It is in order to facilitate the processing of arguments passing.
    //e.g: given value is {r1, <r4, r5>, r7}
    //     spreads to: {r1, r4, r5, r7}
    UINT i = 0;
    Vector<SR*> vec;
    flattenInVec(argval, &vec);

    IOC tmp_cont;
    //Try to pass data through argument-register.
    //Transfer data in single register size.
    UINT transfer_size = GENERAL_REGISTER_SIZE;
    *sz = MAX(*sz, transfer_size);
    for (; *sz != 0; i++) {
        SR * argreg = nullptr;
        if (*sz > 0) {
            argreg = argdescmgr->allocArgRegister(this);
        }
        if (argreg == nullptr) {
            break;
        }
        SR * arg_val = vec.get(i);
        //ASSERT0(arg_val && arg_val->getByteSize() == GENERAL_REGISTER_SIZE);
        //Only move register size data for each time.
        ASSERT0(arg_val);

        tmp_cont.clean();
        buildMove(argreg, arg_val, ors, &tmp_cont);

        //Update avaiable registers info.
        argdescmgr->updatePassedArgInRegister(transfer_size);

        if (*sz >= transfer_size) {
            *sz -= transfer_size;
        } else {
            //*sz is not divisble by 'transfer_size'.
            *sz = 0;
        }
    }

    if (*sz > 0) {
        //There is still remaining data have to be passed through stack.
        for (INT remaining_irsize = *sz; remaining_irsize > 0; i++) {
            SR * arg = vec.get(i);
            ASSERT0(arg);
            argdescmgr->addValueDesc(arg, nullptr, STACK_ALIGNMENT,
                                     transfer_size, 0);
            remaining_irsize -= transfer_size;
        }
        return false;
    }
    return true;
}


//Return true if whole ir has been passed through registers, otherwise
//return false.
bool CG::tryPassArgThroughRegister(SR * argval, SR * argaddr, UINT * argsz,
                                   MOD ArgDescMgr * argdescmgr,
                                   OUT ORList & ors, IOC * cont)
{
    ASSERT0((argval != nullptr) ^ (argaddr != nullptr));
    if (argaddr != nullptr) {
        return passArgInMemory(argaddr, argsz, argdescmgr, ors, cont);
    }
    return passArgInRegister(argval, argsz, argdescmgr, ors, cont);
}


//Pass variant arguments.
//num: the number of group of arguments, a group is consist of
//     {argval, argaddr, argsz, dbx}.
//e.g: given num is 2, the stack layout will be:
// argval0,
// argaddr0,
// argsz0,
// argdb0,
// argval1,
// argaddr1,
// argsz1,
// argdb1,
void CG::passArgVariant(ArgDescMgr * argdescmgr, OUT ORList & ors, UINT num,
                        ...)
{
    va_list ptr;
    va_start(ptr, num);
    IOC tmp;
    ORList tors;
    for (UINT i = 0; i < num; i++) {
        SR * argval = va_arg(ptr, SR*);
        SR * argaddr = va_arg(ptr, SR*);
        UINT argsz = va_arg(ptr, UINT);
        Dbx * dbx = va_arg(ptr, Dbx*);
        tmp.clean();
        tors.clean();
        passArg(argval, argaddr, argsz, argdescmgr, tors, &tmp);
        if (dbx != nullptr) {
            tors.copyDbx(dbx);
        }
        ors.move_tail(tors);
    }
    va_end(ptr);
}


//This function try to pass all arguments through registers.
//Otherwise pass remaining arguments through stack memory.
//'ir': the first parameter of CALL.
void CG::passArg(SR * argval, SR * argaddr, UINT argsz,
                 OUT ArgDescMgr * argdescmgr, OUT ORList & ors,
                 MOD IOC * cont)
{
    ASSERT0((argval != nullptr) ^ (argaddr != nullptr));
    if (tmGetRegSetOfArgument() != nullptr &&
        tmGetRegSetOfArgument()->get_elem_count() != 0 &&
        argdescmgr->getNumOfAvailArgReg() > 0) {
        //Try to pass argval or data in argaddr through register.
        if (!tryPassArgThroughRegister(argval, argaddr, &argsz,
                                       argdescmgr, ors, cont)) {
            //Fail to pass argument through registers.
            ASSERT0(argsz != 0);
        }

        if (argdescmgr->getCurrentDesc() != nullptr) {
            //Pass remainging data through stack.
            storeArgToStack(argdescmgr, ors, cont);
        }
        return;
    }

    //Pass whole parameter data through stack.
    UINT align = computeArgAlign(argsz);
    if (argval != nullptr) {
        //Pass value.
        argdescmgr->addValueDesc(argval, nullptr, align, argsz, 0);
    } else {
        //Pass address.
        ASSERT0(argaddr);
        argdescmgr->addAddrDesc(argaddr, nullptr, align, argsz, 0);
    }
    storeArgToStack(argdescmgr, ors, cont);

    //ArgSectionSize may be 0 if all arguments passed through register.
    //ASSERT0(argdescmgr->getArgSectionSize() ==
    //    argdescmgr->getPassedArgByteSize());
}


//Generate code to store SR on top of stack.
//argdescmgr: record the parameter which tend to store on the stack.
void CG::storeArgToStack(ArgDescMgr * argdescmgr, OUT ORList & ors, IN IOC *)
{
    ASSERT0(argdescmgr);
    IOC tc;
    ORList tors;
    for (ArgDesc const* desc = argdescmgr->pullOutDesc();
         desc != nullptr; desc = argdescmgr->pullOutDesc()) {
        tors.clean();
        //Compute the address of parameter should be computed base on SP.
        //e.g: tgt = src + callee_param_section_byte_ofst;
        if (desc->is_record_addr) {
            //Current argument is bigger than general register bytesize.
            //We build an internal copy to copy argument value into
            //callee's parameter slot.
            tc.clean();
            SR * tgt = getSP();
            SR * src = desc->src_startaddr;
            if (desc->src_ofst != 0) {
                //Generate source address of COPY.
                //src = src_addr + src_ofst;
                if (src == getSP()) {
                    //Do not modify SP.
                    tc.clean();
                    SR * res = genReg();
                    buildMove(res, src, tors, &tc);
                    tc.clean();
                }
                buildAdd(src, genIntImm((HOST_INT)desc->src_ofst, false),
                         GENERAL_REGISTER_SIZE, false, tors, &tc);
                src = tc.get_reg(0);
            }

            tc.clean();
            if (desc->tgt_ofst != 0) {
                //Generate target address of COPY.
                //tgt = SP + tgt_ofst;
                buildAdd(tgt, genIntImm((HOST_INT)desc->tgt_ofst, false),
                         GENERAL_REGISTER_SIZE,
                         false, tors, &tc);
                tgt = tc.get_reg(0);
            } else {
                //Generate target address of COPY.
                //Do not modify SP.
                //tgt = SP;
                SR * res = genReg();
                buildMove(res, tgt, tors, &tc);
                tgt = res;
            }

            //Copy argument value into callee's parameter slot.
            //copy(x, parameter_start_addr, copy_size);
            ASSERT0(src && tgt && tgt->is_reg() && src->is_reg());
            tc.clean();
            buildMemcpyInternal(tgt, src, desc->arg_size, tors, &tc);
        } else {
            //Current argument is small enough to reside in a general register
            //We build a normal memory store to copy argument value into
            //callee's parameter slot.
            //[sp + tgt_ofst] = %rx;
            ASSERTN(desc->arg_size <= 8,
                    ("TODO: expose the value to user for custormize"));
            ASSERT0(desc->src_value);
            tc.clean();
            IOC_mem_byte_size(&tc) = desc->arg_size;
            buildStore(desc->src_value, getSP(),
                       genIntImm((HOST_INT)desc->tgt_ofst, false),
                       false, tors, &tc);
        }

        if (desc->arg_dbx != nullptr) {
            tors.copyDbx(desc->arg_dbx);
        }

        ors.move_tail(tors);
    }
}


void CG::expandSpadjust(OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o->isSpadjust());
    SR * ofst = o->get_opnd(HAS_PREDICATE_REGISTER + SPADJUST_OFFSET_INDX);
    ASSERT0(ofst->is_int_imm());
    IssuePackage * ip = m_ip_mgr.allocIssuePackage();
    ORList ors;
    IOC cont;

    //sp = sp - SIZE_OF_STACK
    buildAdd(getSP(), ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ASSERT0(ors.get_elem_count() == 1);
    OR * addi = ors.get_tail();
    addi->set_result(0, getSP(), this); //replace result-register with SP.

    ip->set(FIRST_SLOT, addi, this);
    ipl->append_tail(ip, &m_ip_mgr);
}


void CG::expandFakeOR(OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o->is_fake());
    switch (o->getCode()) {
    case OR_spadjust_i:
    case OR_spadjust_r:
        expandSpadjust(o, ipl);
        break;
    default: ASSERTN(0, ("Target Dependent Code"));
    }
}


//Perform package if target machine is multi-issue architecture.
void CG::package(Vector<BBSimulator*> & simvec)
{
    List<ORBB*> * bblst = getORBBList();
    if (bblst->get_elem_count() == 0) { return; }

    START_TIMER(t0, "Perform Packaging");
    //Assume the last bb has maximum id.
    IssuePackageList * ipl_ptr = m_ip_mgr.allocIssuePackageList(
        bblst->get_elem_count());

    m_ipl_vec.set(bblst->get_tail()->id(), nullptr);
    for (ORBB * bb = bblst->get_head();
         bb != nullptr; bb = bblst->get_next()) {
        if (bb->getORNum() == 0) { continue; }

        BBSimulator * sim = simvec.get(bb->id());
        ASSERT0(sim != nullptr);

        m_ipl_vec.set(bb->id(), ipl_ptr);

        UINT cyc = sim->getCurCycle();
        ORVec const* ss = sim->getExecSnapshot();

        ASSERT0(FIRST_SLOT == LAST_SLOT);
        for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
            for (UINT i = 0; i < cyc; i++) {
                OR * o = ss[s].get(i);
                if (o == nullptr) { continue; }
                if (o->is_fake()) {
                    expandFakeOR(o, ipl_ptr);
                } else {
                    IssuePackage * ip = m_ip_mgr.allocIssuePackage();
                    ip->set(FIRST_SLOT, o, this);
                    ipl_ptr->append_tail(ip, &m_ip_mgr);
                }
            }
        }
        ipl_ptr++;
    }
    END_TIMER(t0, "Perform Packaging");

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpPackage()) {
        /////////////////////////////////////
        //DO NOT DUMP BB LIST AFTER PACKAGE//
        /////////////////////////////////////
        xoc::note(getRegion(), "\n==---- DUMP AFTER PACKAGE ----==");
        dumpPackage();
    }
    ASSERT0(verifyPackageList());
}


//Generate TRUE predicate register.
SR * CG::genTruePred()
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


//Get TRUE predicate register.
SR * CG::getTruePred() const
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::genRflag()
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::genSP()
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::genFP()
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::genGP()
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::genParamPointer()
{
    if (m_param_pointer != nullptr) {
        return m_param_pointer;
    }
    m_param_pointer = genReg((UINT)GENERAL_REGISTER_SIZE);
    SR_is_param_pointer(m_param_pointer) = 1;
    return m_param_pointer;
}


SR * CG::getRflag() const
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::getSP() const
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::getFP() const
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::getGP() const
{
    ASSERTN(0, ("Target dependent"));
    return nullptr;
}


SR * CG::getParamPointer() const
{
    return m_param_pointer;
}


//Generate a SR if bytes_size not more than GENERAL_REGISTER_SIZE,
//otherwise generate a vector or SR.
//Return the first register if vector generated.
SR * CG::genReg(UINT bytes_size)
{
    SR * first = nullptr;
    if (bytes_size > GENERAL_REGISTER_SIZE) {
        UINT n = xceiling(bytes_size, GENERAL_REGISTER_SIZE);
        List<SR*> ls;
        for (UINT i = 0; i < n; i++) {
            SR * sr = getSRMgr()->genSR();
            if (i == 0) {
                first = sr;
            }
            SR_type(sr) = SR_REG;
            SR_sregid(sr) = ++m_reg_count;
            setMapSymbolReg2SR(SR_sregid(sr), sr);
            SR_phy_reg(sr) = REG_UNDEF;
            SR_regfile(sr) = RF_UNDEF;
            ls.append_tail(sr);
        }
        first = getSRVecMgr()->genSRVec(ls);
        return first;
    }

    first = getSRMgr()->genSR();
    SR_type(first) = SR_REG;
    SR_sregid(first) = ++m_reg_count;
    setMapSymbolReg2SR(SR_sregid(first), first);
    SR_phy_reg(first) = REG_UNDEF;
    SR_regfile(first) = RF_UNDEF;
    return first;
}


//Assign physical register manually.
//During some passes, e.g IR2OR, user expects to assign physical register
//to SR which is NOT dedicated register.
void CG::assignPhyRegister(SR * sr, REG reg, REGFILE rf)
{
    ASSERT0(sr->is_reg() && reg != REG_UNDEF && rf != RF_UNDEF);
    SR_phy_reg(sr) = reg;
    SR_regfile(sr) = rf;

    //LRA will check if the local SR has been assigned physical register.
    //If it is, LRA will demand that user have to change the local SR to
    //global SR. Because LRA will reallocate register resource as much as
    //possible.
    SR_is_global(sr) = true;
}


//Generate a global SR that bytes_size is not more than
//GENERAL_REGISTER_SIZE.
SR * CG::genRegWithPhyReg(REG reg, REGFILE rf)
{
    SR * sr = genReg();
    assignPhyRegister(sr, reg, rf);
    return sr;
}


//Generate SR by specified Symbol Register Id.
SR * CG::genReg(SymRegId regid)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_REG;
    SR_sregid(sr) = regid;
    setMapSymbolReg2SR(regid, sr);
    return sr;
}


//Get dedicated register by specified physical register.
SR * CG::getDedicatedReg(REG phy_reg) const
{
    return const_cast<CG*>(this)->m_dedicate_sr_tab.get(phy_reg);
}


//Generate dedicated register by specified physical register.
SR * CG::genDedicatedReg(REG phy_reg)
{
    SR * sr = m_dedicate_sr_tab.get(phy_reg);
    if (sr == nullptr) {
        sr = genReg();
        SR_regfile(sr) = tmMapReg2RegFile(phy_reg);
        ASSERTN(sr->getRegFile() != RF_UNDEF, ("incomplete target info"));
        SR_phy_reg(sr) = phy_reg;
        SR_is_dedicated(sr) = true;
        SR_is_global(sr) = true;
        m_dedicate_sr_tab.set(phy_reg, sr);
    }
    return sr;
}


SR * CG::genPredReg()
{
    SR * s = genReg();
    SR_is_pred(s) = true;
    return s;
}


SR * CG::genLabelList(LabelInfoList const* lilst)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_LAB_LIST;

    //label list
    SR_label_list(sr) = lilst;
    return sr;
}


SR * CG::genLabel(LabelInfo const* li)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_LAB;

    //label
    SR_label(sr) = li;
    return sr;
}


//Generate variable.
SR * CG::genVAR(xoc::Var const* var)
{
    ASSERT0(var != nullptr && getSRMgr() != nullptr);
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_VAR;

    //Symbol variable, it might be symbol, spill_loc, etc.
    SR_var(sr) = var;
    return sr;
}


//Generate SR that indicates integer.
SR * CG::genIntImm(HOST_INT val, bool is_signed)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_INT_IMM;
    if (is_signed) {
        SR_int_imm(sr) = (HOST_INT)val;
    } else {
        SR_int_imm(sr) = (HOST_INT)(HOST_UINT)val;
    }
    return sr;
}


//Generate SR that indicates float point.
SR * CG::genFpImm(HOST_FP val)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_FP_IMM;
    SR_fp_imm(sr) = val;
    return sr;
}


//Generate SR according to specified register and register file.
SR * CG::genSR(REG reg, REGFILE regfile)
{
    SR * sr = genReg((UINT)GENERAL_REGISTER_SIZE);
    SR_phy_reg(sr) = reg;
    SR_regfile(sr) = regfile;
    return sr;
}


//Generate a empty SR.
SR * CG::genSR()
{
    return getSRMgr()->genSR();
}


//Get an OR.
OR * CG::getOR(UINT id)
{
    return m_or_mgr->getOR(id);
}


RegFileSet const* CG::getValidRegfileSet(OR_TYPE ortype, UINT idx,
                                         bool is_result) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ortype);
    SRDescGroup<> const* sdg  = OTD_srd_group(otd);
    ASSERT0(sdg != nullptr);
    if (is_result) {
        SRDesc const* sd = sdg->get_res(idx);
        RegFileSet const* rfs = SRD_valid_regfile_set(sd);
        return rfs;
    }

    SRDesc const* sd = sdg->get_opnd(idx);
    RegFileSet const* rfs = SRD_valid_regfile_set(sd);
    return rfs;
}


RegSet const* CG::getValidRegSet(OR_TYPE ortype, UINT idx,
                                 bool is_result) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ortype);
    SRDescGroup<> const* sda  = OTD_srd_group(otd);
    ASSERT0(sda != nullptr);
    if (is_result) {
        SRDesc const* sd = sda->get_res(idx);
        RegSet const* rs = SRD_valid_reg_set(sd);
        return rs;
    }

    SRDesc const* sd = sda->get_opnd(idx);
    RegSet const* rs = SRD_valid_reg_set(sd);
    return rs;
}


//Map phsical register to dedicated symbol register if exist.
SR * CG::getDedicatedSRForPhyReg(REG reg)
{
    return m_dedicate_sr_tab.get(reg);
}


//Generate and return the mask code that used to reserved the lower
//'bytesize' bytes.
HOST_UINT CG::getMaskByByte(UINT bytesize)
{
    HOST_UINT mask = 0;
    for (UINT i = 0; i < bytesize * BIT_PER_BYTE; i++) {
        mask |= ((HOST_UINT)1) << i;
    }
    return mask;
}


REGFILE CG::getPredicateRegfile() const
{
    ASSERT0(HAS_PREDICATE_REGISTER);
    ASSERTN(0, ("Target Dependent Code"));
    return RF_UNDEF;
}


//Set predicate register of each operation in 'ops' same as 'o'.
void CG::setORListWithSamePredicate(MOD ORList & ops, IN OR * o)
{
    if (!HAS_PREDICATE_REGISTER) {
        return;
    }
    SR * pd = o->get_pred();
    if (pd == nullptr) {
        pd = getTruePred();
    }
    for (OR * tmpor = ops.get_head(); tmpor != nullptr; tmpor = ops.get_next()) {
        tmpor->set_pred(pd, this);
    }
}


void CG::setSpadjustOffset(OR * spadj, INT size)
{
    DUMMYUSE(spadj);
    DUMMYUSE(size);
    ASSERT0(spadj && spadj->isSpadjust());
    ASSERTN(0, ("Target Dependent Code"));
}


//Prepend spilling operation at the tail of OR list.
void CG::prependSpill(ORBB * bb, ORList & ors)
{
    //postProcess() may insert spill code at Entry-BB.
    //ASSERT0(!ORBB_is_entry(bb));
    ORBB_orlist(bb)->append_head(ors);
}


//Append reload operation at the tail of OR list.
void CG::appendReload(ORBB * bb, ORList & ors)
{
    //postProcess() may insert reload code at Exit-BB.
    //ASSERT0(!ORBB_is_exit(bb));
    ORBB_orlist(bb)->append_tail(ors);
}


//Rename result SR of 'o', from 'oldsr' to 'newsr'.
void CG::renameResult(OR * o, SR * oldsr, SR * newsr, bool match_phy_reg)
{
    if (match_phy_reg) {
        for (UINT i = 0; i < o->result_num(); i++) {
            SR * res = o->get_result(i);
            if (isSREqual(res, oldsr)) {
                o->set_result(i, newsr, this);
            }
        }
    } else {
        for (UINT i = 0; i < o->result_num(); i++) {
            if (o->get_result(i) == oldsr) {
                o->set_result(i, newsr, this);
            }
        }
    }
}


//Rename operand SR of 'o', from 'oldsr' to 'newsr'.
void CG::renameOpnd(OR * o, SR * oldsr, SR * newsr, bool match_phy_reg)
{
    if (match_phy_reg) {
        for (UINT i = 0; i < o->opnd_num(); i++) {
            SR * opnd = o->get_opnd(i);
            if (isSREqual(opnd, oldsr)) {
                o->set_opnd(i, newsr, this);
            }
        }
        return;
    }

    for (UINT i = 0; i < o->opnd_num(); i++) {
        if (o->get_opnd(i) == oldsr) {
            o->set_opnd(i, newsr, this);
        }
    }
}


//Rename opnd and result of OR from oldsr to newsr in range
//between 'start' and the end OR of BB.
//The renaming process does not consider physical register if SR assigned.
void CG::renameOpndAndResultFollowed(SR * oldsr, SR * newsr, ORCt * start,
                                     BBORList * ors)
{
    for (OR * o = start->val(); start != ors->end();
         o = ors->get_next(&start)) {
        ASSERT0(o);
        renameOpnd(o, oldsr, newsr, false);
        renameResult(o, oldsr, newsr, false);
    }
}


//Rename opnd and result of OR from oldsr to newsr in range
//between 'start' and the end OR of BB.
//The renaming process does not consider physical register if SR assigned.
void CG::renameOpndAndResultFollowed(SR * oldsr, SR * newsr, OR * start,
                                     BBORList * ors)
{
    ORCt * ct = nullptr;
    bool is = ors->find(start, &ct);
    CHECK0_DUMMYUSE(is);
    renameOpndAndResultFollowed(oldsr, newsr, ct, ors);
}


//Rename opnd and result of OR from 'oldsr' to 'newsr' in between
//'start' and 'end'.
//Note that 'start' will be renamed, but 'end' is not.
void CG::renameOpndAndResultInRange(SR * oldsr, SR * newsr, ORCt * start,
                                    ORCt * end, BBORList * orlist)
{
    ASSERTN(start && end && oldsr != newsr, ("not in list"));
    for (OR * o = start->val();
         o != nullptr && start != end;
         o = orlist->get_next(&start)) {
        renameOpnd(o, oldsr, newsr, false);
        renameResult(o, oldsr, newsr, false);
    }
    ASSERTN(start == end, ("out of given range"));
}


//Rename opnd and result of OR from 'oldsr' to 'newsr' in between
//'start' and 'end'.
//Note that 'start' will be renamed, but 'end' is not.
void CG::renameOpndAndResultInRange(SR * oldsr, SR * newsr, OR * start,
                                    OR * end, BBORList * orlist)
{
    if (start == nullptr) { return; }
    ASSERT0(oldsr != newsr);
    bool in_range = false;
    ORCt * ct = nullptr;
    orlist->find(start, &ct);

    ASSERTN(ct, ("not in list"));
    for (OR * o = start; o != nullptr; o = orlist->get_next(&ct)) {
        renameOpnd(o, oldsr, newsr, false);
        renameResult(o, oldsr, newsr, false);
        if (o == end) {
            in_range = true;
            break;
        }
    }

    DUMMYUSE(in_range);
    ASSERTN(in_range, ("out of given range"));
}


bool CG::isRegflowDep(OR const* from, OR const* to) const
{
    for (UINT i = 0; i < from->result_num(); i++) {
        SR * sr = from->get_result(i);
        if (!sr->is_reg() || sr->is_pred()) {
            continue;
        }
        if (mayUse(to, sr)) {
            return true;
        }
    }
    return false;
}


bool CG::isRegoutDep(OR const* from, OR const* to) const
{
    for (UINT i = 0; i < from->result_num(); i++) {
        SR * sr = from->get_result(i);
        if (!sr->is_reg() || sr->is_pred()) {
            continue;
        }
        if (mayDef(to, sr)) {
            return true;
        }
    }
    return false;
}


//Check asm-o clobber set.
bool CG::mustAsmDef(OR const* o, SR const* sr) const
{
    ASSERTN(o->is_asm(), ("expect asm-o"));
    ASSERTN(sr->is_reg(), ("sr is not register"));
    if (SR_phy_reg(sr) == REG_UNDEF) {
        return false;
    }

    ORAsmInfo * asm_info = getAsmInfo(o);
    if (asm_info == nullptr) {
        ASSERTN(0, ("asm info for o is nullptr?"));
        return false;
    }
    //Check DEF register set.
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr1 = o->get_result(i);
        if (!sr1->is_reg()) {
            continue;
        }

        //Check clobber set.
        //Is it actually necessary?
        //RegSet * clobber_set = asm_info->get_clobber_set();
        //for (reg = clobber_set.get_first(); reg != REG_UNDEF;
        //       reg = clobber_set.get_next(reg)) {
        //    CLUST cur = mapReg2Cluster(reg);
        //    if (res_clust == CLUST_UNDEF) {
        //        res_clust = cur;
        //    } else if (cur != CLUST_UNDEF && cur != res_clust) {
        //        //Asm operation cross multi clusters.
        //        res_clust = CLUST_BUS;
        //    }
        //}
        SRDescGroup<> const* sdg = asm_info->get_srd_group();
        SRDesc const* sd = sdg->get_res(i);
        RegFileSet const* rfs = SRD_valid_regfile_set(sd);
        for (REGFILE rf = (REGFILE)rfs->get_first();
             rf != RF_UNDEF; rf = (REGFILE)rfs->get_next(rf)) {
            RegSet const* rs = tmMapRegFile2RegSet(rf);
            if (rs->is_contain(SR_phy_reg(sr1))) {
                return true;
            }
        } //end for each regfile
    } //end for
    return false;
}


//Check result of o.
//NOTICE: asm-o also have result tns, but its clobber-set may implicitly
//override some registers. So 'mustAsmDef()' has to do more inspection.
bool CG::mustDef(OR const* o, SR const* sr) const
{
    if (!sr->is_reg()) {
        return false;
    }

    for (UINT i = 0; i < o->result_num(); i++) {
        SR * res = o->get_result(i);
        if (isSREqual(res, sr)) {
            return true;
        }
    }

    if (isSP(sr) && o->isSpadjust()) {
        return true;
    }

    if (o->is_asm() && mustAsmDef(o, sr)) {
        return true;
    }
    return false;
}


bool CG::mustUse(OR const* o, SR const* sr) const
{
    if (!sr->is_reg()) {
        return false;
    }
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (isSREqual(opnd, sr)) {
            return true;
        }
    }
    if (SR_phy_reg(sr) == REG_UNDEF) {
        return false;
    }
    return false;
}


//Return true if sr was recomputed between 'start' and 'end'.
//That processing does not include 'start','end'.
bool CG::mayDefInRange(SR const* sr, IN ORCt * start, IN ORCt * end,
                       IN ORList & ors)
{
    ASSERT0(ors.find(start->val()) && ors.find(end->val()));
    for (OR * o = start->val(); start != end; o = ors.get_next(&start)) {
        if (mayDef(o, sr)) {
            return true;
        }
    }
    return false;
}


bool CG::mayDef(OR const* o, SR const* sr) const
{
    if (mustDef(o, sr)) {
        return true;
    }
    return false;
}


bool CG::mayUse(OR const* o, SR const* sr) const
{
    if (mustUse(o, sr)) {
        return true;
    }
    return false;
}


//Compute the index of operand, return -1 if 'opnd' is not an opnd of 'o'.
INT CG::computeOpndIdx(OR * o, SR const* opnd)
{
    for (UINT i = 0; i < o->opnd_num(); i++) {
        if (isSREqual(o->get_opnd(i), opnd)) {
            return i;
        }
    }
    return -1;
}


//Compute the index of 'res', return -1 if 'res' is not a result of 'o'.
INT CG::computeResultIdx(OR * o, SR const* res)
{
    for (UINT i = 0; i < o->result_num(); i++) {
        if (isSREqual(o->get_result(i), res)) {
            return i;
        }
    }
    return -1;
}


void CG::computeEntryAndExit(IN ORCFG & cfg,
                             OUT List<ORBB*> & entry_lst,
                             OUT List<ORBB*> & exit_lst)
{
    INT c;
    for (xcom::Vertex * v = cfg.get_first_vertex(c);
         v != nullptr; v = cfg.get_next_vertex(c)) {
        ORBB * bb = cfg.getBB(v->id());
        ASSERT0(bb);
        if (v->getInDegree() == 0) {
            ORBB_is_entry(bb) = true;
            entry_lst.append_tail(bb);
        }
        if (v->getOutDegree() == 0) {
            ORBB_is_exit(bb) = true;
            exit_lst.append_tail(bb);
        }
    }
}


//Generate SP adjustment operation, and the code protecting the
//Return Address if region has a call.
//If SP adjust operations are more than one, you should construct a
//fake operation, and expand it at the phase before emitting assmbly.
void CG::generateFuncUnitDedicatedCode()
{
    bool has_call = false;
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != nullptr; bb = m_or_bb_list.get_next()) {
        if (bb->hasCall()) {
            has_call = true;
            break;
        }
    }

    ASSERT0(m_or_cfg);
    ORList ors;
    IOC cont;
    List<ORBB*> entry_lst;
    List<ORBB*> exit_lst;
    computeEntryAndExit(*m_or_cfg, entry_lst, exit_lst);
    for (ORBB * bb = entry_lst.get_head();
         bb != nullptr; bb = entry_lst.get_next()) {
        Dbx const* dbx = nullptr;
        if (ORBB_first_or(bb) != nullptr) {
            dbx = &OR_dbx(ORBB_first_or(bb));
        }

        //Save function return-address to stack.
        if (has_call) {
            ors.clean();
            SR * sr = genReturnAddr();
            xoc::Var * loc = genSpillVar(sr);
            ASSERT0(loc != nullptr);

            IOC cont1;
            IOC_mem_byte_size(&cont1) = GENERAL_REGISTER_SIZE;
            buildStore(sr, loc, 0, false, ors, &cont1);
            if (dbx != nullptr) {
                ors.copyDbx(dbx);
            }
            ORBB_orlist(bb)->append_head(ors);
        }

        //Add spadjust operations
        ors.clean();
        IOC_int_imm(&cont) = 0;
        buildSpadjust(ors, &cont);
        ASSERTN(ors.get_elem_count() == 1, ("at most one spadjust operation."));
        if (dbx != nullptr) {
            ors.copyDbx(dbx);
        }
        ORBB_orlist(bb)->append_head(ors);
        ORBB_entry_spadjust(bb) = ors.get_head();
    }

    for (ORBB * bb = exit_lst.get_head();
         bb != nullptr; bb = exit_lst.get_next()) {
        OR * last_or = ORBB_last_or(bb);
        Dbx const* dbx = nullptr;
        if (last_or != nullptr) {
            dbx = &OR_dbx(last_or);
        }

        //Protection of ret-address.
        if (has_call) {
            ors.clean();
            SR * sr = genReturnAddr();
            xoc::Var * loc = genSpillVar(sr);
            ASSERT0(loc != nullptr);

            IOC cont2;
            IOC_mem_byte_size(&cont2) = GENERAL_REGISTER_SIZE;
            buildLoad(sr, loc, 0, false, ors, &cont2);
            if (dbx != nullptr) {
                ors.copyDbx(dbx);
            }
            if (last_or != nullptr &&
                (OR_is_call(last_or) ||
                 OR_is_cond_br(last_or) ||
                 OR_is_uncond_br(last_or) ||
                 OR_is_return(last_or))) {
                 ORBB_orlist(bb)->insert_before(ors, last_or);
            } else {
                ORBB_orlist(bb)->append_tail(ors);
            }
        }

        //Add spadjust operations
        ors.clean();
        IOC_int_imm(&cont) = 0;
        buildSpadjust(ors, &cont);
        if (dbx != nullptr) {
            ors.copyDbx(dbx);
        }
        ASSERTN(ors.get_elem_count() == 1, ("at most one spadjust operation."));
        if (last_or != nullptr &&
            (OR_is_call(last_or) ||
             OR_is_cond_br(last_or) ||
             OR_is_uncond_br(last_or) ||
             OR_is_return(last_or))) {
             ORBB_orlist(bb)->insert_before(ors, last_or);
        } else {
            ORBB_orlist(bb)->append_tail(ors);
        }
        ORBB_exit_spadjust(bb) = ors.get_head();
    }
}


void CG::setCluster(ORList & ors, CLUST clust)
{
    ASSERTN(clust != CLUST_UNDEF, ("Undef cluster"));
    for (OR * o = ors.get_head(); o; o = ors.get_next()) {
        OR_clust(o) = clust;
    }
}


xoc::Var * CG::addBBLevelVar(xoc::Type const* type, UINT align)
{
    xcom::StrBuf name(64);
    xoc::Var * v = m_rg->getFuncRegion()->getVarMgr()->registerVar(
        genBBLevelNewVarName(name), type, align, VAR_LOCAL);
    m_bb_level_internal_var_list.append_tail(v);
    return v;
}


xoc::Var * CG::addFuncLevelVar(xoc::Type const* type, UINT align)
{
    xcom::StrBuf name(64);
    xoc::Var * v = m_rg->getFuncRegion()->getVarMgr()->registerVar(
        genFuncLevelNewVarName(name), type, align, VAR_LOCAL);
    m_func_level_internal_var_list.append_tail(v);
    return v;
}


//Construct a name for Var that will lived in a ORBB.
CHAR const* CG::genBBLevelNewVarName(OUT xcom::StrBuf & name)
{
    name.sprint("bb_level_var_%d",
                m_bb_level_internal_var_list.get_elem_count());
    return name.buf;
}


//Generate spill location that same like 'sr'.
//Or return the spill location if exist.
//'sr': the referrence SR.
xoc::Var * CG::genSpillVar(SR * sr)
{
    ASSERT0(sr->is_reg());
    if (SR_spill_var(sr) != nullptr) {
        return SR_spill_var(sr);
    }

    xoc::Var * v;
    xoc::Type const* type = m_tm->getSimplexTypeEx(
        m_tm->getDType(WORD_BITSIZE, false));

    //Generate spill-loc for life time that belonged to whole func unit if v is
    //global variable, or else generate spill-loc for life time that only
    //lived in a single BB.
    v = genTempVar(type, STACK_ALIGNMENT, sr->is_global());
    ASSERT0(v);

    VAR_is_spill(v) = true;
    SR_spill_var(sr) = v;
    return v;
}


//Amend byte offset of load/store of parameters according to
//frame bytesize.
//'lv_size': bytesize of (local variable + temporary variable +
//    callee parameter size).
void CG::reviseFormalParamAccess(UINT lv_size)
{
    ASSERT0(!isUseFP());
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != nullptr; bb = m_or_bb_list.get_next()) {
        for (OR * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb)) {
            if (o->is_load()) {
                if (o->get_load_base() == genParamPointer()) {
                    o->set_load_base(getSP(), this);
                    SR * offset = o->get_load_ofst();
                    ASSERT0(offset->is_int_imm() && offset->getInt() >= 0);
                    SR_int_imm(offset) += lv_size;
                }
                continue;
            }

            if (o->is_store()) {
                if (o->get_store_base() == genParamPointer()) {
                    o->set_store_base(getSP(), this);
                    SR * offset = o->get_store_ofst();
                    ASSERT0(offset->is_int_imm() && offset->getInt() >= 0);
                    SR_int_imm(offset) += lv_size;
                }
                continue;
            }
        }
    }
}


UINT CG::calcSizeOfParameterPassedViaRegister(
    List<Var const*> const* param_lst) const
{
    RegSet const* phyregset = tmGetRegSetOfArgument();
    ASSERT0(phyregset);
    C<Var const*> * ct = nullptr;
    param_lst->get_head(&ct);
    if (ct == nullptr) {
        return 0;
    }
    UINT passed_paramsz = 0; //record the bytesize that has been passed.
    UINT size = 0;
    for (INT phyreg = phyregset->get_first();
         phyreg >= 0 && ct != nullptr;
         phyreg = phyregset->get_next(phyreg)) {
        if (passed_paramsz == 0 && //only check if whole
                                   //value can be passed via reg
            skipArgRegister(ct->val(), phyregset, phyreg)) {
            continue;
        }
        ASSERT0(phyreg >= 0);
        size += GENERAL_REGISTER_SIZE;
        passed_paramsz += GENERAL_REGISTER_SIZE;
        if (passed_paramsz == ct->val()->getByteSize(m_tm)) {
            ct = param_lst->get_next(ct);
            passed_paramsz = 0;
        }
    }
    return size;
}


//Insert store of register value of parameters
//after spadjust at each entry BB.
//param_start: bytesize offset of total parameter section, related to SP.
//entry_lst: list of entry BB
//param_lst: parameter variable which sorted in declaration order
void CG::storeRegisterParameterBackToStack(List<ORBB*> * entry_lst,
                                           UINT param_start)
{
    if (SECT_size(m_cgmgr->getParamSection()) == 0) {
        return;
    }
    ASSERT0(tmGetRegSetOfArgument());
    List<Var const*> param_lst;
    m_rg->findFormalParam(param_lst, true);

    ASSERT0(entry_lst);
    RegSet const* phyregset = tmGetRegSetOfArgument();
    ASSERT0(phyregset);
    ORList ors;
    IOC tc;
    for (ORBB * bb = entry_lst->get_head();
         bb != nullptr; bb = entry_lst->get_next()) {
        OR * spadj = ORBB_entry_spadjust(bb);
        if (spadj == nullptr) { continue; }
        ORCt * orct = nullptr;
        ORBB_orlist(bb)->find(spadj, &orct);
        ASSERTN(orct, ("not find spadjust in BB"));
        Var const* param = param_lst.get_head();
        ASSERT0(param);

        ors.clean();
        tc.clean();
        //Record the bytesize that has been passed for each parameter.
        UINT passed_paramsz = 0;

        //Record bytesize of total parameters that has been passed.
        UINT passed_total_paramsz = 0;
        for (BSIdx phyreg = phyregset->get_first();
             phyreg != BS_UNDEF && param != nullptr;
             phyreg = phyregset->get_next(phyreg)) {
            if (passed_paramsz == 0 && //only check if whole
                                       //value can be passed via reg
                skipArgRegister(param, phyregset, phyreg)) {
                //This physical register does not need to
                //store into stack.
                continue;
            }
            ASSERT0(phyreg >= 0);
            SR * sr_phyreg = genDedicatedReg(phyreg);
            ASSERT0(sr_phyreg);
            IOC_mem_byte_size(&tc) = GENERAL_REGISTER_SIZE;
            buildStoreAndAssignRegister(sr_phyreg,
                                        param_start + passed_total_paramsz,
                                        ors, &tc);
            passed_total_paramsz += GENERAL_REGISTER_SIZE;
            passed_paramsz += GENERAL_REGISTER_SIZE;
            if (passed_paramsz == param->getByteSize(m_tm)) {
                param = param_lst.get_next();
                passed_paramsz = 0;
            }
        }

        ORBB_orlist(bb)->insert_after(ors, orct);
    }
}


//The function inserts code to initialize and finialize frame pointer register.
//e.g: foo() { int x; x = 20; }
//    --- frame start ---
//    sp <- sp - 4 #spadjust operation.
//    [sp + 0] = 20
//    sp <- sp + 4 #spadjust operation.
//    --- frame end ---
// If user invoke alloca, spadjust operation will be generated. Thus local
// variable should be accessed through FP.
//e.g: foo() { int x; x = 20; void * p = alloca(64); ... }
//    --- frame start ---
//    sp <- sp - 4 #spadjust operation.
//    fp <- sp # sp store operation.
//    [fp + 0] = 20
//    sp <- sp - 64 #alloca
//    ...
//    sp <- fp # sp reload operation.
//    sp <- sp + 4 #spadjust operation.
//    --- frame end ---
void CG::insertCodeToUseFP(List<ORBB*> & entry_lst, List<ORBB*> & exit_lst)
{
    ORList ors;
    IOC tc;
    for (ORBB * bb = entry_lst.get_head();
         bb != nullptr; bb = entry_lst.get_next()) {
        OR * spadj = ORBB_entry_spadjust(bb);
        if (spadj == nullptr) { continue; }

        //Insert save-or before spadjust-or.
        ors.clean();
        //storeRegToStack(getFP(), ors);
        tc.clean();
        buildMove(getFP(), getSP(), ors, &tc);
        ORBB_orlist(bb)->insert_after(ors, spadj);
    }

    for (ORBB * bb = exit_lst.get_head(); bb != nullptr;
         bb = exit_lst.get_next()) {
        OR * spadj = ORBB_exit_spadjust(bb);
        if (spadj == nullptr) { continue; }

        //Insert reload-or after spadjust-or.
        ors.clean();
        tc.clean();
        //reloadRegFromStack(getFP(), ors);
        buildMove(getSP(), getFP(), ors, &tc);
        ORBB_orlist(bb)->insert_before(ors, spadj);
    }

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpCG()) {
        xoc::note(getRegion(),
                  "\n==---- DUMP AFTER INSERT CODE TO USER FP ----==");
        dumpORBBList();
    }
}


//Insert store of parameter register after spadjust at each entry BB
//if target machine support pass argument through register.
void CG::reviseFormalParameterAndSpadjust(List<ORBB*> & entry_lst,
                                          List<ORBB*> & exit_lst)
{
    //framesize = (register caller parameter size + local variable +
    //             temporary variable + callee parameter size).
    INT framesize = (INT)SECT_size(m_cgmgr->getStackSection()) +
                    getMaxArgSectionSize();

    //Adjust size by stack alignment.
    ASSERTN(xcom::isPowerOf2(STACK_ALIGNMENT),
            ("For the convenience of generating double load/store"));
    ASSERTN(xcom::isPowerOf2(SPADJUST_ALIGNMENT),
            ("For the convenience of generating double load/store"));
    ASSERTN(SPADJUST_ALIGNMENT - 1 <= 0xFFFF, ("Stack alignment is too big"));

    framesize = (INT)xcom::ceil_align(framesize, SPADJUST_ALIGNMENT);

    if (!isUseFP() &&
        (getMaxArgSectionSize() > 0 &&
         SECT_size(m_cgmgr->getParamSection()) > 0)) {
        reviseFormalParamAccess(framesize);
    }

    //Update the parameter section start.
    ASSERTN(m_param_sect_start_offset == -1, ("already set"));
    m_param_sect_start_offset = framesize;

    if (isPassArgumentThroughRegister()) {
        ASSERT0(tmGetRegSetOfArgument()); //phy regset should exist.
        xcom::List<xoc::Var const*> param_list;
        m_rg->findFormalParam(param_list, true);
        UINT bytesize_of_arg_passed_via_reg =
            calcSizeOfParameterPassedViaRegister(&param_list);
        if (bytesize_of_arg_passed_via_reg % SPADJUST_ALIGNMENT != 0) {
            UINT alignsize = (UINT)xcom::ceil_align(
                bytesize_of_arg_passed_via_reg, SPADJUST_ALIGNMENT);
            ASSERT0(alignsize > bytesize_of_arg_passed_via_reg);
            framesize += alignsize - bytesize_of_arg_passed_via_reg;
        }

        //Update the parameter section start.
        m_param_sect_start_offset = framesize;
        storeRegisterParameterBackToStack(&entry_lst, framesize);

        //Amend framesize.
        framesize += bytesize_of_arg_passed_via_reg;
    }

    //size must align in SPADJUST_ALIGNMENT.
    ASSERT0(framesize == xcom::ceil_align(framesize, SPADJUST_ALIGNMENT));

    for (ORBB * bb = entry_lst.get_head();
         bb != nullptr; bb = entry_lst.get_next()) {
        OR * spadj = ORBB_entry_spadjust(bb);
        if (spadj == nullptr) { continue; }
        if (framesize == 0) {
            //Spadjust OR should be removed if the frame length is zero.
            //Do not free o, its SR may be used by other OR.
            ORBB_orlist(bb)->remove(spadj);
            continue;
        }

        setSpadjustOffset(spadj, -framesize);
    }

    for (ORBB * bb = exit_lst.get_head(); bb != nullptr;
         bb = exit_lst.get_next()) {
        OR * spadj = ORBB_exit_spadjust(bb);
        if (spadj == nullptr) { continue; }
        if (framesize == 0) {
            //Spadjust OR should be removed if the frame length is zero.
            //Do not free o, its SR may be used by other OR.
            ORBB_orlist(bb)->remove(spadj);
            continue;
        }

        setSpadjustOffset(spadj, framesize);
    }

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpReviseSpadjust()) {
        xoc::note(getRegion(), "\n==---- DUMP AFTER REVISE SPADJUST ----==");
        dumpORBBList();
    }
}


//Return true if 'test_sr' is the one of operands of 'o' ,
//it is also the results.
//'test_sr': can be nullptr. If it is nullptr, we only try to
//           get the index-info of the same opnd and result.
bool CG::isOpndSameWithResult(SR const* test_sr, OR const* o,
                              OUT INT * opndnum, OUT INT * resnum) const
{
    INT o1, o2;
    if (opndnum == nullptr) { opndnum = &o1;}
    if (resnum == nullptr) { resnum = &o2;}
    *opndnum = -1;
    *resnum = -1;

    //Find the sr with 'same_res' property.
    SR * the_sr = nullptr;
    for (UINT resn = 0; resn < o->result_num(); resn++) {
        for (UINT opndn = 0; opndn < o->opnd_num(); opndn++) {
            SR * res = o->get_result(resn);
            if (res == o->get_opnd(opndn)) {
                ASSERT0(res);
                //Find the same opnd and result.
                *opndnum = opndn;
                *resnum = resn;
                the_sr = res;
                goto FIN;
            }
        }
    }
FIN:
    if (the_sr != nullptr) {
        if (test_sr == nullptr) {
            return true;
        }
        if (test_sr == the_sr) {
            return true;
        }
    }
    return false;
}


//Construct a name for Var that will lived in Region.
CHAR const* CG::genFuncLevelNewVarName(OUT xcom::StrBuf & name)
{
    name.sprint("func_level_var_%d",
                m_func_level_internal_var_list.get_elem_count());
    return name.buf;
}


xoc::Var * CG::genTempVar(xoc::Type const* type, UINT align, bool func_level)
{
    if (func_level) {
        return addFuncLevelVar(type, align);
    }

    xoc::Var * v = getBBLevelVarList()->getFreeVar();
    if (v == nullptr) {
        v = addBBLevelVar(type, align);
    }
    return v;
}


//Split OR list into ORBB.
void CG::constructORBBList(IN ORList & or_list)
{
    if (or_list.get_elem_count() == 0) { return; }

    START_TIMER(t, "Construct ORBB list");
    ORBB * cur_bb = nullptr;
    for (OR * o = or_list.get_head(); o != nullptr; o = or_list.get_next()) {
        //Insert xoc::IR into individual BB.
        if (cur_bb == nullptr) {
            cur_bb = allocBB();
        }

        if (cur_bb->isLowerBoundary(o)) {
            ORBB_orlist(cur_bb)->append_tail(o);
            if (OR_is_uncond_br(o)) {
                //We have no knowledge about whether target BB of GOTO will be
                //followed with current BB.
                //So leave this problem till CFG is built, and the acutally
                //attribution when should be attached.
                ;
            } else if (OR_is_return(o)) {
                //Succeed stmt of 'ir' may be DEAD code
                ORBB_is_exit(cur_bb) = true;
            }

            //Generate new BB
            m_or_bb_list.append_tail(cur_bb);
            cur_bb = allocBB();
            continue;
        }

        if (cur_bb->isUpperBoundary(o)) {
            ORBB_is_target(cur_bb) = true;
            m_or_bb_list.append_tail(cur_bb);

            //New empty BB
            cur_bb = allocBB();

            //Regard label info as add-on info attached on bb, and
            //'o' be dropped off.
            if (o->is_label_or()) {
                cur_bb->addLabel(o->getLabel());
            } else {
                ORBB_orlist(cur_bb)->append_tail(o);
            }
            continue;
        }

        ORBB_orlist(cur_bb)->append_tail(o);
    }
    ASSERT0(cur_bb);

    m_or_bb_list.append_tail(cur_bb);

    END_TIMER(t, "Construct ORBB list");

    #ifdef _DEBUG_
    //Do some verifications.
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != nullptr; bb = m_or_bb_list.get_next()) {
        INT cur_order = -1;
        for (OR  * o = ORBB_first_or(bb); o != nullptr; o = ORBB_next_or(bb)) {
            ASSERT0(OR_order(o) != -1);
            if (cur_order == -1) {
                cur_order = OR_order(o);
            } else {
                ASSERT0(OR_order(o) > cur_order);
            }
        }
    }
    #endif

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpIR2OR()) {
        xoc::note(getRegion(), "\n==---- DUMP AFTER IR2OR CONVERT %s ----==",
                  m_rg->getRegionName());
        dumpORBBList();
    }
}


//Perform global and local register allocation.
RaMgr * CG::performRA()
{
    if (xoc::g_dump_opt.isDumpBeforePass() && g_xgen_dump_opt.isDumpRA()) {
        xoc::note(getRegion(),
                  "\n==---- DUMP BEFORE REGISTER ALLOCATION of '%s' ----==",
                  m_rg->getRegionName());
        dumpORBBList();
    }

    START_TIMER(t, "Register Allocation");
    RaMgr * rm = allocRaMgr(getORBBList(), m_rg->is_function());
    rm->setParallelPartMgrVec(getParallelPartMgrVec());
    #ifdef _DEBUG_
    if (tmGetRegSetOfCallerSaved()->is_empty()) {
        xoc::interwarn("Caller Register Set is empty!"
                       "There might lack of allocable registers "
                       "if RAMGR_can_alloc_callee is disabled");
    }
    #endif
    if (isGRAEnable()) {
        rm->performGRA();
    }

    rm->performLRA();
    END_TIMER(t, "Register Allocation");

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpRA()) {
        xoc::note(getRegion(),
                  "\n==---- DUMP AFTER REGISTER ALLOCATION %s ----==",
                  m_rg->getRegionName());
        dumpORBBList();
    }
    return rm;
}


//Local instruction scheduling.
//simvec: record instruction layout that will be used to package.
//        Note BBSimulators in simvec must be freed by caller.
void CG::performIS(MOD Vector<BBSimulator*> & simvec, IN RaMgr * ra_mgr)
{
    START_TIMER(t, "Instruction Schedule");
    List<ORBB*> * bblist = getORBBList();
    simvec.set(bblist->get_elem_count(), nullptr);
    ORBBListIter it;
    for (ORBB * bb = bblist->get_head(&it);
         bb != nullptr; bb = bblist->get_next(&it)) {
        if (bb->getORNum() == 0) { continue; }

        DataDepGraph * ddg = nullptr;
        BBSimulator * sim = nullptr;
        LIS * lis = nullptr;
        preLS(bb, ra_mgr, &ddg, &sim, &lis);

        ASSERT0(ddg && sim && lis);

        simvec.set(bb->id(), sim); //record sim that required by package().

        if (g_do_lis) {
            ddg->build();
            lis->schedule();
        } else {
            bool cycle_accurate = false;
            if (cycle_accurate) {
                //Build DDG if one requires cycle-accurated scheduling
                //and output the execution table in simulator as result of IS.
                //Cycle-accurated scheduling will consider delay-solt
                //and branch-latency and insert nop if needed.
                ddg->buildSerialDependence();
            }
            lis->serialize();
        }

        postLS(lis, ddg);
    }
    END_TIMER(t, "Instruction Schedule");
}


RaMgr * CG::allocRaMgr(List<ORBB*> * bblist, bool is_func)
{
    return new RaMgr(bblist, is_func, this);
}


void CG::evaluateCallArgSize(IR const* ir)
{
    ASSERT0(ir->isCallStmt());
    updateMaxCalleeArgSize(computeTotalParameterStackSize(ir));
    if (m_cgmgr->isIntrinsic(ir, INTRIN_ALLOCA)) {
        m_is_use_fp = true;
    }
}


//Estimate and reserve stack memory space for real parameters.
//The function also collecting the information to determine whether enable
//the using of FP register.
void CG::computeMaxRealParamSpace()
{
    START_TIMER(t, "Compute Max Real Parameter Space");
    if (m_rg->getIRList() != nullptr) {
        IRIter it;
        for (IR const* ir = xoc::iterInit(m_rg->getIRList(), it);
             ir != nullptr; ir = xoc::iterNext(it)) {
            if (ir->isCallStmt()) {
                evaluateCallArgSize(ir);
            }
        }
    } else {
        BBList * ir_bb_list = m_rg->getBBList();
        for (IRBB * bb = ir_bb_list->get_head();
             bb != nullptr; bb = ir_bb_list->get_next()) {
            IRListIter ct;
            for (xoc::IR const* ir = BB_irlist(bb).get_head(&ct);
                 ir != nullptr; ir = BB_irlist(bb).get_next(&ct)) {
                if (ir->isCallStmt()) {
                    evaluateCallArgSize(ir);
                }
            }
        }
    }
    if (getMaxArgSectionSize() > 0) {
        SECT_size(m_cgmgr->getStackSection()) += getMaxArgSectionSize();
    }
    END_TIMER(t, "Compute Max Real Parameter Space");
}


DataDepGraph * CG::allocDDG()
{
    return new DataDepGraph();
}


//'is_log': false value means that Caller will delete
//    the object allocated utilmately.
LIS * CG::allocLIS(ORBB * bb, DataDepGraph * ddg, BBSimulator * sim,
                   UINT sch_mode)
{
    return new LIS(bb, *ddg, sim, sch_mode);
}


//is_log: false value means that Caller will delete
//        the object allocated utilmately.
BBSimulator * CG::allocBBSimulator(ORBB * bb)
{
    return new BBSimulator(bb);
}


//Destroy useless resource.
void CG::postLS(LIS * lis, DataDepGraph * ddg)
{
    delete lis;
    delete ddg;
}


//Create components that schedulor dependents on.
void CG::preLS(IN ORBB * bb, IN RaMgr * ra_mgr, OUT DataDepGraph ** ddg,
               OUT BBSimulator ** sim, OUT LIS ** lis)
{
    //Init DDG
    DataDepGraph * tddg = allocDDG();
    tddg->setParam(DEP_PHY_REG|DEP_MEM_FLOW|DEP_MEM_OUT|DEP_REG_ANTI|
                   DEP_MEM_ANTI|DEP_SYM_REG);
    Vector<ParallelPartMgr*> * ppm_vec = ra_mgr->getParallelPartMgrVec();
    ParallelPartMgr * ppm = ppm_vec == nullptr ?
        nullptr : ppm_vec->get(bb->id());
    tddg->init(bb);
    tddg->setParallelPartMgr(ppm);

    //Mark all symbol registers into the unique to prevent
    //schedulor schedules it to separate cluster.
    LRA * tlra = ra_mgr->allocLRA(bb, ppm, ra_mgr);
    RegFileSet is_regfile_unique;
    tlra->markRegFileUnique(is_regfile_unique);
    tlra->assignCluster(*tddg, is_regfile_unique, false);
    delete tlra;

    //Init BBSimulator
    BBSimulator * tsim = allocBBSimulator(bb);
    UINT mode = LIS::SCH_TOP_DOWN;
    if (g_enable_schedule_delay_slot) {
        mode |= LIS::SCH_BRANCH_DELAY_SLOT;
        mode |= LIS::SCH_ALLOW_RESCHED;
    }

    //Init LIS
    LIS * tlis = allocLIS(bb, tddg, tsim, mode);
    //Post LRA scheduling does NOT require regfile info.
    tlis->set_unique_regfile(nullptr);
    *ddg = tddg;
    *sim = tsim;
    *lis = tlis;
}


//This function does not handle SRs bewteen stmt1 and stmt2,
//which include stmt1, not include stmt2.
//e.g:   ... <- sr1 //first_use
//       ...
// first_def <- ... //stmt1
//       ... <- not_first_use1
//       ... <- not_first_use2
// last_def  <- ... //stmt2
//       ...
//       ... <- use
void CG::localizeBB(SR * sr, ORBB * bb)
{
    ASSERT0(sr->is_reg());
    ASSERTN(!SR_is_dedicated(sr),
        ("rename dedicated register may incur illegal instruction format"));
    ASSERT0(sr && bb);
    xcom::C<OR*> * orct = nullptr;
    xcom::C<OR*> * first_usestmt_ct = nullptr;
    xcom::C<OR*> * first_defstmt_ct = nullptr;
    xcom::C<OR*> * last_defstmt_ct = nullptr;

    for (ORBB_orlist(bb)->get_head(&orct);
         orct != ORBB_orlist(bb)->end();
         ORBB_orlist(bb)->get_next(&orct)) {
        OR * o = orct->val();
        ASSERT0(o);

        if (first_usestmt_ct == nullptr && first_defstmt_ct == nullptr) {
            for (UINT i = 0; i < o->opnd_num(); i++) {
                SR const* tsr = o->get_opnd(i);
                if (tsr != sr) { continue; }
                first_usestmt_ct = orct;
                break;
            }
        }

        for (UINT i = 0; i < o->result_num(); i++) {
            SR const* tsr = o->get_result(i);
            if (tsr != sr) { continue; }
            if (first_defstmt_ct == nullptr) {
                first_defstmt_ct = orct;
            }
            last_defstmt_ct = orct;
        }
    }

    ASSERT0(first_defstmt_ct || first_usestmt_ct);
    if (SR_spill_var(sr) == nullptr) {
        genSpillVar(sr);
    }
    IOC toc;
    ASSERT0(SR_spill_var(sr));
    IOC_mem_byte_size(&toc) = GENERAL_REGISTER_SIZE; // sr->getByteSize();
    ORList ors;
    if (first_usestmt_ct != nullptr) {
        //Handle upward exposed use.
        toc.clean_bottomup();
        SR * newsr = genReg();
        buildLoad(newsr, SR_spill_var(sr), 0, false, ors, &toc);
        ASSERT0(first_usestmt_ct != ORBB_orlist(bb)->end());
        ORBB_orlist(bb)->insert_before(ors, first_usestmt_ct);

        if (first_defstmt_ct != nullptr) {
            ASSERT0(first_usestmt_ct == first_defstmt_ct ||
                    ORBB_orlist(bb)->is_or_precedes(first_usestmt_ct->val(),
                                                    first_defstmt_ct->val()));
        }

        if (first_usestmt_ct == first_defstmt_ct) {
            renameOpnd(first_usestmt_ct->val(), sr, newsr, false);
        } else if (first_defstmt_ct != nullptr &&
                   ORBB_orlist(bb)->is_or_precedes(first_usestmt_ct->val(),
                                                   first_defstmt_ct->val())) {
            renameOpndAndResultInRange(sr, newsr, first_usestmt_ct,
                                       first_defstmt_ct, ORBB_orlist(bb));
        } else {
            //USE live through BB.
            // <-use
            // ...
            // <-use
            renameOpndAndResultFollowed(sr, newsr, first_usestmt_ct,
                                        ORBB_orlist(bb));
        }
    }

    if (first_defstmt_ct != nullptr) {
        //Handle downward exposed use.
        toc.clean_bottomup();
        ors.clean();

        SR * newsr = genReg();
        buildStore(newsr, SR_spill_var(sr), 0, false, ors, &toc);
        if (HAS_PREDICATE_REGISTER) {
            SR * pd = last_defstmt_ct->val()->get_pred();
            if (pd != nullptr) {
                ors.set_pred(pd, this);
            }
        }

        ORBB_orlist(bb)->append_tail_ex(ors);
        renameOpndAndResultFollowed(sr, newsr, last_defstmt_ct,
                                    ORBB_orlist(bb));
    }
}


void CG::localizeBBTab(SR * sr, TTab<ORBB*> * orbbtab)
{
    ASSERT0(sr && orbbtab);

    TTabIter<ORBB*> iter;
    for (ORBB * bb = orbbtab->get_first(iter);
         bb != nullptr; bb = orbbtab->get_next(iter)) {
        localizeBB(sr, bb);
    }
}


//Perform localization to global life-time PR.
void CG::localize()
{
    START_TIMER(t0, "CG Localization");
    TMap<SR*, ORBB*> sr2bb;
    TMap<SR*, TTab<ORBB*>*> sr2orbbtab;
    List<ORBB*> * bbl = getORBBList();
    xcom::C<ORBB*> * ct;
    for (bbl->get_head(&ct); ct != bbl->end(); ct = bbl->get_next(ct)) {
        ORBB * bb = ct->val();
        ASSERT0(bb);

        xcom::C<OR*> * orct;
        for (ORBB_orlist(bb)->get_head(&orct);
             orct != ORBB_orlist(bb)->end();
             ORBB_orlist(bb)->get_next(&orct)) {
            OR * o = orct->val();
            ASSERT0(o);

            for (UINT i = 0; i < o->result_num(); i++) {
                SR * sr = o->get_result(i);
                ASSERT0(sr);
                if (!sr->is_reg() || SR_is_dedicated(sr)) { continue; }

                ORBB * tgtbb = nullptr;
                if ((tgtbb = sr2bb.get(sr)) == nullptr) {
                    sr2bb.set(sr, bb);
                    continue;
                }
                if (tgtbb == bb) { continue; }

                TTab<ORBB*> * orbbtab = sr2orbbtab.get(sr);
                if (orbbtab == nullptr) {
                    orbbtab = new TTab<ORBB*>();
                    sr2orbbtab.set(sr, orbbtab);
                    orbbtab->append(tgtbb); //Add the first BB own SR.
                }

                orbbtab->append_and_retrieve(bb); //Add new BB.
            }

            for (UINT i = 0; i < o->opnd_num(); i++) {
                SR * sr = o->get_opnd(i);
                ASSERT0(sr);
                if (!sr->is_reg() || SR_is_dedicated(sr)) { continue; }

                ORBB * tgtbb = nullptr;
                if ((tgtbb = sr2bb.get(sr)) == nullptr) {
                    sr2bb.set(sr, bb);
                    continue;
                }
                if (tgtbb == bb) { continue; }

                TTab<ORBB*> * orbbtab = sr2orbbtab.get(sr);
                if (orbbtab == nullptr) {
                    orbbtab = new TTab<ORBB*>();
                    sr2orbbtab.set(sr, orbbtab);
                    orbbtab->append(tgtbb);
                }

                orbbtab->append_and_retrieve(bb);
            }
        }
    }

    TMapIter<SR*, TTab<ORBB*>*> iter;
    TTab<ORBB*> * orbbtab = nullptr;
    for (SR * sr = sr2orbbtab.get_first(iter, &orbbtab);
         sr != nullptr; sr = sr2orbbtab.get_next(iter, &orbbtab)) {
        localizeBBTab(sr, orbbtab);
        delete orbbtab;
    }
    END_TIMER(t0, "CG Localization");

    if (g_dump_opt.isDumpAfterPass() && g_xgen_dump_opt.isDumpLocalize()) {
        note(getRegion(), "\n==---- DUMP AFTER LOCALIZE ----==");
        dumpORBBList();
    }
}


//Scan package list and verify OR.
//1. Check is SR assigned physical register and register file.
//2. Check if immediate operand is in valid value range.
bool CG::verifyPackageList()
{
    START_TIMER(t0, "Verify Package List");
    for (INT bbid = 0; bbid <= m_ipl_vec.get_last_idx(); bbid++) {
        IssuePackageList * ipl = m_ipl_vec.get(bbid);
        if (ipl == nullptr) { continue; }

        IssuePackageListIter it;
        for (it = ipl->get_head();
             it != ipl->end(); it = ipl->get_next(it)) {
            IssuePackage * ip = it->val();
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                OR const* o = ip->get(s);
                if (o != nullptr) {
                    verifyOR(o);
                }
            }
        }
    }
    END_TIMER(t0, "Verify Package List");
    return true;
}


bool CG::verifyOR(OR const* o)
{
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        ASSERT0(sr);
        if (sr->is_reg()) {
            ASSERTN(sr->getPhyReg() != REG_UNDEF,
                    ("SR is not assigned physical register"));
            ASSERTN(sr->getRegFile() != RF_UNDEF,
                    ("SR is not assigned register file"));
        }
        if (sr->is_int_imm()) {
            ASSERT0(isValidImmOpnd(o->getCode(), sr->getInt()));
        }
    }

    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        ASSERT0(sr);
        if (sr->is_reg()) {
            ASSERTN(sr->getPhyReg() != REG_UNDEF,
                    ("SR is not assigned physical register"));
            ASSERTN(sr->getRegFile() != RF_UNDEF,
                    ("SR is not assigned register file"));
        }
        if (sr->is_int_imm()) {
            ASSERT0(isValidImmOpnd(o->getCode(), i, sr->getInt()));
        }
    }

    ASSERT0(isValidImmOpnd(o));
    return true;
}


static void convertORBBList(CG * cg)
{
    //allocate convertor that dependend on target
    IR2OR * ir2or = cg->allocIR2OR();
    ASSERT0(ir2or);

    //Translate IR in IRBB to a list of OR.
    //Record OR list after converting xoc::IR to OR.
    {
        RecycORList or_list(ir2or);
        ir2or->convertToORList(or_list);

        //Split OR list into ORBB.
        cg->constructORBBList(or_list.getList());

        //destruct or_list.
    }

    //Object in ir2or is refered by or_list, thus destroy it till
    //constructORBBList is completed.
    delete ir2or;
}


void CG::createORCFG(OptCtx & oc)
{
    //Allocate CFG.
    ASSERTN(m_or_cfg == nullptr, ("CFG already exist"));
    m_or_cfg = allocORCFG();

    //Build CFG.
    START_TIMER(t0, "OR Control Flow Optimizations");
    xoc::CfgOptCtx ctx(oc);
    m_or_cfg->removeEmptyBB(ctx);
    m_or_cfg->build(oc);
    m_or_cfg->removeEmptyBB(ctx);
    m_or_cfg->computeExitList();
    if (m_or_cfg->removeUnreachBB(ctx)) {
        m_or_cfg->computeExitList();
    }
    END_TIMER(t0, "OR Control Flow Optimizations");
}


static void performCFGOptimization(CG * cg, OptCtx & oc)
{
    //Perform control flow optimizations.
    START_TIMER(t1, "OR Control Flow Optimizations");
    bool change;
    ORCFG * cfg = cg->getORCFG();
    xoc::CfgOptCtx ctx(oc);
    do {
        change = false;
        if (cfg->removeEmptyBB(ctx)) {
            cfg->computeExitList();
            change = true;
        }
        if (cfg->removeUnreachBB(ctx)) {
            cfg->computeExitList();
            change = true;
        }
        if (cfg->removeRedundantBranch()) {
            cfg->computeExitList();
            change = true;
        }
    } while (change);
    END_TIMER(t1, "OR Control Flow Optimizations");
}


//Guarantee IR_RETURN at the end of function.
void CG::addReturnForEmptyRegion()
{
    IR * ret = m_rg->buildReturn(nullptr);
    IRBB * bb = m_rg->getBBList()->get_head();
    if (bb != nullptr) {
        ASSERTN(m_rg->getIRList() == nullptr,
                ("IR list should have been split into BB list"));
        ASSERT0(bb->getNumOfIR() == 0);
        bb->getIRList().append_tail(ret);
        return;
    }
    m_rg->addToIRList(ret);
}


//This function generate target dependent information.
bool CG::perform()
{
    ASSERTN(xcom::isPowerOf2(STACK_ALIGNMENT),
            ("Stack alignment should be power of 2"));

    if ((xoc::g_dump_opt.isDumpAfterPass() ||
         xoc::g_dump_opt.isDumpBeforePass()) &&
        g_xgen_dump_opt.isDumpCG()) {
        xoc::note(getRegion(),
                  "\n==---- DUMP START CODE GENERATION (%d)'%s' ----==\n",
                  m_rg->id(), m_rg->getRegionName());
        m_rg->dump(false);
    }

    if (m_rg->getBBList()->get_elem_count() == 0) {
        addReturnForEmptyRegion();
    }

    START_TIMER_FMT(tcg, ("Code Generation Perform '%s'",
                          m_rg->getRegionName()));
    //Estimate and reserve stack memory space for real parameters.
    //The function also collecting the information to determine whether enable
    //the using of FP register.
    computeMaxRealParamSpace();

    //Generate OR.
    convertORBBList(this);

    //Build CFG.
    OptCtx oc;
    createORCFG(oc);

    if (m_rg->is_function()) {
        //Generate SP adjustment operation, and the code protecting the
        //Return Address if region has a call.
        //If SP adjust operations are more than one, you should construct a
        //fake operation, and expand it at the phase before emitting assmbly.
        generateFuncUnitDedicatedCode();
    }

    if (!isGRAEnable()) {
        //Perform localization to global life-time PR.
        localize();
    }

    //Perform global and local register allocation.
    RaMgr * ra_mgr = performRA();
    performCFGOptimization(this, oc);

    List<ORBB*> entry_lst;
    List<ORBB*> exit_lst;
    computeEntryAndExit(*m_or_cfg, entry_lst, exit_lst);

    if (m_rg->is_function()) {
        //Insert store of parameter register after spadjust at each entry BB
        //if target machine support pass argument through register.
        reviseFormalParameterAndSpadjust(entry_lst, exit_lst);
    }

    if (isUseFP()) {
        insertCodeToUseFP(entry_lst, exit_lst);
    }

    //Perform Local instruction scheduling.
    SimVec * simvec = new SimVec();
    performIS(*simvec, ra_mgr);

    //Compute the offset for stack variable and
    //supersede the symbol variable with the offset.
    relocateStackVarOffset();

    delete ra_mgr;

    //Packaging instruction bundle if target machine is
    //multi-issue architecture.
    package(*simvec);

    delete simvec;

    /////////////////////////////////////////
    //DO NOT DUMP ORBB LIST AFTER THIS LINE//
    /////////////////////////////////////////

    END_TIMER_FMT(tcg, ("Code Generation Perform '%s'", m_rg->getRegionName()));
    return true;
}

} //namespace xgen
