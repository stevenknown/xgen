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

//
//START ArgDescMgr
//
SR * ArgDescMgr::allocArgRegister(CG * cg)
{
    INT phyreg = m_argregs.get_first();
    if (phyreg == -1) {
        return NULL;
    }
    m_argregs.diff(phyreg);
    return cg->genDedicatedReg(phyreg);
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
    m_passed_arg_in_register_byte_size += (INT)xcom::ceil_align(
        bytesize, STACK_ALIGNMENT);
    for (ArgDesc * desc = getArgList()->get_head();
         desc != NULL; desc = getArgList()->get_next()) {
        desc->tgt_ofst -= bytesize;
    }
}
//END ArgDescMgr


//
//START CG
//
CG::CG(xoc::Region * rg, CGMgr * cgmgr)
{
    ASSERTN(rg, ("Code generation need region info."));
    ASSERT0(cgmgr);
    m_ru = rg;
    m_cgmgr = cgmgr;
    m_tm = rg->getTypeMgr();
    m_or_bb_idx = 1; //the id of first bb is 1.
    m_or_bb_mgr = NULL;
    m_or_mgr = cgmgr->getORMgr();
    m_or_cfg = NULL;
    m_ppm_vec = NULL;
    m_max_real_arg_size = 0;
    m_pr2sr_map.init(MAX(4, getNearestPowerOf2(rg->getPRCount())));

    //TODO: To ensure alignment for performance gain.
    m_sect_count = 0;
    m_reg_count = 0;
    m_param_pointer = NULL;
    m_pool = smpoolCreate(64, MEM_COMM);
    m_is_use_fp = false;
    m_is_compute_sect_offset = false;

    //Builtin function should be initialized in initBuiltin().
    m_builtin_memcpy = NULL;

    initSections(rg->getVarMgr());
}


CG::~CG()
{
    m_pr2sr_map.clean();
    m_regid2sr_map.clean();

    if (m_or_cfg != NULL) {
        delete m_or_cfg;
        m_or_cfg = NULL;
    }

    //Free (but is not delete) all OR, SR, and ORBB.
    //m_or_bb_mgr should be deleted after freeORBBList().
    //OR will be deleted by ORMgr, and SR will be deleted by SRMgr,
    //ORBB will be deleted by ORBBMgr.
    freeORBBList();

    if (m_or_bb_mgr != NULL) {
        delete m_or_bb_mgr;
        m_or_bb_mgr = NULL;
    }

    smpoolDelete(m_pool);

    finiFuncUnit();
}


//Generate OR with variant number of operands and results.
OR * CG::buildOR(OR_TYPE orty, UINT resnum, UINT opndnum, ...)
{
    //This manner worked well on ia32, but is not on x8664.
    //SR ** sr = (SR**)(((BYTE*)(&opndnum)) + sizeof(opndnum));

    va_list ptr;
    va_start(ptr, opndnum);
    OR * o = genOR(orty);
    //First extracting results.
    UINT i = 0;
    while (i < resnum) {
        SR * sr = va_arg(ptr, SR*);
        o->set_result(i, sr);
        i++;
    }
    //following are opnds
    i = 0;
    while (i < opndnum) {
        SR * sr = va_arg(ptr, SR*);
        if (i == 0 && HAS_PREDICATE_REGISTER) {
            ASSERTN(SR_is_pred(sr), ("first operand must be predicate SR"));
        }
        o->set_opnd(i, sr);
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
        o->set_result(i, r);
    }

    i = 0;
    for (SR * sr = opnd.get_head(); sr != NULL;
        sr = opnd.get_next(), i++) {
        if (i == 0 && HAS_PREDICATE_REGISTER) {
            ASSERT0(SR_is_pred(sr));
        }
        o->set_opnd(i, sr);
    }
    return o;
}


//Generate ::memcpy.
void CG::buildMemcpy(
        SR * tgt,
        SR * src,
        UINT bytesize,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(tgt && src && cont);
    //Push parameter on stack.
    //::memcpy(void *dest, const void *src, int n);
    SR * immreg = genReg();
    buildMove(immreg, genIntImm((HOST_INT)bytesize, false), ors, cont);

    ArgDescMgr argdescmgr;
    ArgDesc * desc = NULL;

    //The left most parameter: void *dest
    desc = argdescmgr.addValueDesc(tgt, NULL, tgt->getByteSize(), 0);
    argdescmgr.addArgInArgSection(desc->arg_size);
    ASSERT0(m_tm->get_pointer_bytesize() == tgt->getByteSize());   

    //The second right most parameter: const void *src
    desc = argdescmgr.addValueDesc(src, NULL, src->getByteSize(), 0);
    argdescmgr.addArgInArgSection(desc->arg_size);
    ASSERT0(m_tm->get_pointer_bytesize() == src->getByteSize());

    //The right most parameter: int n
    desc = argdescmgr.addValueDesc(immreg, NULL, immreg->getByteSize(), 0);
    argdescmgr.addArgInArgSection(desc->arg_size);

    //Collect the maximum parameters size during code generation.
    //And revise SP-djust operation afterwards.
    updateMaxCalleeArgSize(argdescmgr.getArgSectionSize());

    if (isPassArgumentThroughRegister()) {
        //Try to pass ir through register.
        if (!passArgThroughRegister(&argdescmgr, ors, cont)) {
            //Fail to pass all argument through registers.
            //Generate code to push remaining parameters.
            storeArgToStack(&argdescmgr, ors, cont);
        }        
    } else {
        //Generate code to push parameters.
        storeArgToStack(&argdescmgr, ors, cont);
    }

    ASSERT0(argdescmgr.getCurrentDesc() == NULL);

    //Copy the value from src to tgt.
    ASSERT0(m_builtin_memcpy);
    buildCall(m_builtin_memcpy, bytesize, ors, cont);
}


void CG::buildStore(
        SR * store_val,
        xoc::VAR const* base,
        HOST_INT ofst,
        OUT ORList & ors,
        IN IOC * cont)
{
    ASSERT0(SR_is_reg(store_val));
    SR * mem_base_addr = genVAR(base);
    buildStore(store_val, mem_base_addr, genIntImm(ofst, true), ors, cont);
}


//Build spilling operation.
//This function generate memory store operation to the spill location.
void CG::buildSpill(
        IN SR * store_val,
        IN xoc::VAR * spill_var,
        IN ORBB *,
        OUT ORList & ors)
{
    ASSERT0(SR_is_reg(store_val) && VAR_is_spill(spill_var));
    SR * mem_base_addr = genVAR(spill_var);

    IOC tc;
    IOC_mem_byte_size(&tc) = store_val->getByteSize();
    buildStore(store_val, mem_base_addr,
        genIntImm((HOST_INT)0, false), ors, &tc);

    for (OR * o = ors.get_head(); o != NULL; o = ors.get_next()) {
        if (OR_is_store(o)) {
            OR_is_spill(o) = 1;
        }
    }
}


//Build reloading operation.
//This function generate memory load operation from the spill location.
void CG::buildReload(
        IN SR * result_val,
        IN xoc::VAR * reload_var,
        IN ORBB *,
        OUT ORList & ors)
{
    ASSERT0(SR_is_reg(result_val) && VAR_is_spill(reload_var));
    IOC tc;
    IOC_mem_byte_size(&tc) = result_val->getByteSize();
    buildLoad(result_val, reload_var, 0, ors, &tc);
    for (OR * o = ors.get_head(); o; o = ors.get_next()) {
        if (OR_is_load(o)) {
            OR_is_reload(o) = true;
        }
    }
}


//Build pseduo instruction that indicate LabelInfo.
//Note OR_label must be supplied by Target.
void CG::buildLabel(LabelInfo const* li, OUT ORList & ors, IN IOC *)
{
    ASSERT0(li != NULL);
    SR * lab = genLabel(li);
    OR * o = genOR(OR_label);
    o->setLabel(lab);
    ors.append_tail(o);
}


//'sr_size': The number of byte-size of SR.
void CG::buildAdd(
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    if (SR_is_int_imm(src1) && SR_is_int_imm(src2)) {
        SR * result = genIntImm((HOST_INT)(SR_int_imm(src1) +
            SR_int_imm(src2)), true);
        ASSERT0(cont != NULL);
        cont->set_reg(0, result);
        return;
    } else if (SR_is_int_imm(src1)) {
        buildAdd(src2, src1, sr_size, is_sign, ors, cont);
        return;
    } else if (SR_is_int_imm(src2) || SR_is_var(src2)) {
        buildAddRegImm(src1, src2, sr_size, is_sign, ors, cont);
        return;
    }
    buildAddRegReg(true, src1, src2, sr_size, is_sign, ors, cont);
}


//'sr_size': The number of integral multiple of byte-size of single SR.
void CG::buildSub(
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC * cont)
{
    if (SR_is_int_imm(src1) && SR_is_int_imm(src2)) {
        SR * result = genIntImm((HOST_INT)(SR_int_imm(src1) -
            SR_int_imm(src2)), true);
        ASSERT0(cont != NULL);
        cont->set_reg(0, result);
        return;
    } else if (SR_is_int_imm(src1)) {
        ASSERT0(sr_size <= 8);
        SR * newsrc1 = genReg();
        buildMove(newsrc1, src1, ors, cont);

        if (sr_size == 8) {
            SR * t = genReg();
            SR * src1_h = SR_vec(src1)->get(1);
            ASSERT0(src1_h != NULL);
            buildMove(t, src1_h, ors, cont);
            getSRVecMgr()->genSRVec(2, newsrc1, t);
        }
        src1 = newsrc1;
    } else if (SR_is_int_imm(src2)) {
        SR_int_imm(src2) = -SR_int_imm(src2);
        buildAddRegImm(src1, src2, sr_size, is_sign, ors, cont);
        return;
    } else {
        //May be the VAR is an offset that will be computed lazy.
        ASSERTN(!SR_is_var(src2), ("subtract VAR is unsupport"));
    }
    buildAddRegReg(false, src1, src2, sr_size, is_sign, ors, cont);
}


//'sr_size': The number of integral multiple of byte-size of single SR.
void CG::buildMod(
        CLUST clust,
        SR ** result,
        SR * src1,
        SR * src2,
        UINT sr_size,
        bool is_sign,
        OUT ORList & ors,
        IN IOC *)
{
    DUMMYUSE(is_sign);
    DUMMYUSE(sr_size);
    OR_TYPE orty = OR_UNDEF;
    ASSERTN(0, ("Target Dependent Code"));
    if (SR_is_int_imm(src1) && SR_is_int_imm(src2)) {
        *result = genIntImm((HOST_INT)(SR_int_imm(src1) % SR_int_imm(src2)),
            true);
        return;
    }

    ASSERTN(!SR_is_int_imm(*result), ("Not allocate result sr"));
    OR * o = buildOR(orty, 1, 3, *result, genTruePred(), src1, src2);
    OR_clust(o) = clust;
    ors.append_tail(o);
}


//Generate sp adjust operation.
void CG::buildSpadjust(OUT ORList & ors, IN IOC * cont)
{
    OR * o = genOR(OR_spadjust);
    ASSERT0(OR_is_fake(o));
    o->set_result(0, genSP());
    o->set_opnd(HAS_PREDICATE_REGISTER + 0, genSP());
    ASSERT0(cont != NULL);
    o->set_opnd(HAS_PREDICATE_REGISTER + 1,
        genIntImm((HOST_INT)IOC_int_imm(cont), true));
    ors.append_tail(o);
}


//Convert data type from 'src' to 'tgt'.
void CG::buildTypeCvt(
        xoc::IR const* tgt,
        xoc::IR const* src,
        OUT ORList & ors,
        IN OUT IOC * cont)
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
            ASSERT0(tgt_low != NULL);
            SR * tgt_high = NULL;

            IOC tmp;
            if (src->is_signed()) {
                //Regard src >> 31(arithmatic shift right)
                //as the sign extension.
                buildShiftRight(tgt_low, GENERAL_REGISTER_SIZE,
                    genIntImm((HOST_INT)31, false), true, tors, &tmp);
                tgt_high = tmp.get_reg(0);
                ASSERT0(tgt_high != NULL);
            } else {
                tgt_high = genReg();
                buildMove(tgt_high,
                    genIntImm((HOST_INT)0, false),
                    tors, &tmp);
            }
            tors.copyDbx(tgt);
            ors.append_tail(tors);
            cont->set_reg(0, tgt_low);
            getSRVecMgr()->genSRVec(2, tgt_low, tgt_high);
        } else {
            //Just do some check.
            SR * src_low = cont->get_reg(0);
            CHECK_DUMMYUSE(src_low);
            ASSERT0(SR_vec(src_low) != NULL && SR_vec_idx(src_low) == 0);
            ASSERT0(SR_vec(src_low)->get(1) != NULL);
            ASSERT0(src_low->getByteSize() == 8);
        }
    } else {
        ASSERTN(0, ("TODO"));
    }
}


//Generate operations: reg = &var + lda_ofst
//lda_ofst: the offset based to var.
void CG::buildLda(
        xoc::VAR const* var,
        HOST_INT lda_ofst,
        Dbx const* dbx,
        OUT ORList & ors,
        IN IOC * cont)
{
    SR * base, * ofst;
    computeVarBaseOffset(var, lda_ofst, &base, &ofst);

    if (SR_is_reg(base)) {
        //Get variable's address: reg = base reg + offset.
        ORList tors;
        IOC tmp;
        buildAdd(base, ofst, GENERAL_REGISTER_SIZE, false, tors, &tmp);
        ASSERT0(tors.get_elem_count() == 1);
        OR * addu = tors.get_head();
        OR_is_need_compute_var_ofst(addu) = true;
        if (dbx != NULL) {
            OR_dbx(addu).copy(*dbx);
        }
        ors.append_tail(addu);
        SR * res = tmp.get_reg(0);
        ASSERT0(res && cont);
        cont->set_reg(0, res);
        return;
    }

    //Get variable's address: reg = _variable_symbol_address_.
    ASSERT0(SR_is_var(base));
    ORList tors;
    SR * addr = genReg();
    if (VAR_is_global(var)) {
        SR * v = genVAR(var);
        if (lda_ofst != 0) {
            SR_var_ofst(v) = (UINT)lda_ofst;
        }
        buildMove(addr, v, tors, cont);
    } else {
        buildMove(addr, base, tors, cont);
        ASSERT0(SR_is_int_imm(ofst));
        if (SR_int_imm(ofst) > 0) {
            IOC tmp;
            buildAdd(addr, ofst, GENERAL_REGISTER_SIZE, false, tors, &tmp);
            addr = tmp.get_reg(0);
            ASSERT0(addr);
        }
    }

    if (dbx != NULL) {
        tors.copyDbx(dbx);
    }
    ors.append_tail(tors);
    ASSERT0(cont);
    cont->set_reg(0, addr);
}


void CG::buildGeneralLoad(
        IN SR * val,
        HOST_INT ofst,
        OUT ORList & ors,
        IN IOC * cont)
{
    if (SR_is_int_imm(val)) {
        SR * res = genReg();
        buildMove(res, val, ors, cont);
        cont->set_reg(0, res);
        return;
    }

    if (SR_is_var(val) || SR_is_reg(val)) {
        SR * load_val = NULL;
        SR * addr;
        switch (IOC_mem_byte_size(cont)) {
        case 0:
            ASSERTN(0, ("invalid mem size"));
            break;
        case 1: //1byte
        case 2:
        case 4:
            load_val = genReg();
            break;
        case 8:
            load_val = genReg();
            if (GENERAL_REGISTER_SIZE == 4) {
                load_val = getSRVecMgr()->genSRVec(2, load_val, genReg());
                break;
            } else if (GENERAL_REGISTER_SIZE == 8) {
                break;
            }
            ASSERTN(0, ("unsupport"));
            break;
        default:
            addr = NULL;
            if (SR_is_reg(val)) {
                addr = val;
            } else {
                ASSERT0(SR_is_var(val));
                buildLda(SR_var(val), SR_var_ofst(val) + ofst, NULL, ors, cont);
                addr = cont->get_reg(0);
            }

            ASSERT0(addr);
            cont->clean_regvec();
            cont->set_addr(addr);
            return;
        }

        ASSERT0(load_val);
        buildLoad(load_val, val, genIntImm(ofst, true), ors, cont);
        ASSERT0(cont);
        cont->set_reg(0, load_val);
        return;
    }

    UNREACHABLE();
}


//Generate opcode of accumulating operation.
//Form as: A = A op B
void CG::buildAccumulate(
        OR * red_or,
        SR * red_var,
        SR * restore_val,
        List<OR*> & ors)
{
    DUMMYUSE(red_or);
    DUMMYUSE(red_var);
    DUMMYUSE(restore_val);
    DUMMYUSE(ors);
    ASSERTN(0, ("Target Dependent Code"));
}


ORBB * CG::allocBB()
{
    if (m_or_bb_mgr == NULL) {
        m_or_bb_mgr = new ORBBMgr();
    }

    ORBB * bb = m_or_bb_mgr->newBB(this);
    ORBB_id(bb) = m_or_bb_idx++;
    return bb;
}


//Free OR, SR for BBs.
void CG::freeORBBList()
{
    ASSERT0(m_or_mgr);
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != NULL; bb = m_or_bb_list.get_next()) {
        for (OR * o = ORBB_first_or(bb); o != NULL; o = ORBB_next_or(bb)) {
            m_or_mgr->free_or(o);
        }
    }
    m_or_bb_list.clean();
}


bool CG::isGRAEnable()
{
    return g_opt_level >= OPT_LEVEL2 && g_do_gra;
}


OR_CFG * CG::allocORCFG()
{
    OR_CFG * cfg = new OR_CFG(C_SEME, getORBBList(), this);
    return cfg;
}


IssuePackage * CG::allocIssuePackage()
{
    return new IssuePackage();
}


void CG::assembleSRVec(SRVec * srvec, SR * sr1, SR * sr2)
{
    ASSERT0(srvec && sr1 && sr2);
    srvec->clean();
    SR_vec(sr1) = srvec;
    SR_vec(sr2) = srvec;
    SR_vec_idx(sr1) = 0;
    SR_vec_idx(sr2) = 1;
    srvec->set(0, sr1);
    srvec->set(1, sr2);
}


//Calc total memory space for parameters,
//with considering the memory alignment.
UINT CG::computeTotalParameterStackSize(xoc::IR * ir)
{
    ASSERT0(ir->isCallStmt());
    xoc::IR * param = CALL_param_list(ir);
    UINT size = 0;
    while (param != NULL) {
        size = (UINT)ceil_align(size, STACK_ALIGNMENT);
        size += param->getTypeSize(m_tm);
        param = param->get_next();
    }
    return size;
}


//Calculate section and corresponding offset for 'sym' in object space.
void CG::computeAndUpdateGlobalVarLayout(
        xoc::VAR const* var,
        OUT SR ** base,
        OUT SR ** base_ofst)
{
    ASSERT0(var);
    ASSERT0(VAR_is_global(var));

    Section * section;
    if (VAR_is_readonly(var) || var->is_string()) {
        section = &m_rodata_sect;
    } else if (VAR_has_init_val(var)) {
        section = &m_data_sect;
    } else {
        section = &m_bss_sect;
    }

    VarDesc * vd;
    if (!SECT_var_list(section).find(var)) {
        //Add VAR into var-list of 'section'.
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));
        SECT_size(section) = ceil_align(SECT_size(section), VAR_align(var));
        VD_ofst(vd) = (ULONG)SECT_size(section);
        SECT_size(section) += var->getByteSize(m_tm);
        SECT_var2vdesc_map(section).set(var, vd);
        SECT_var_list(section).append_tail(var);
    } else {
        vd = SECT_var2vdesc_map(section).get(var);
    }

    if (base != NULL) {
        *base = genVAR(SECT_var(section));
    }

    if (base_ofst != NULL) {
        *base_ofst = genIntImm((HOST_INT)VD_ofst(vd), false);
    }
}


//Return the combination of all of available function unit of 'o'.
UnitSet const* CG::computeORUnit(OR const* o, OUT UnitSet * us)
{
    ORTypeDesc const* otd = tmGetORTypeDesc(OR_code(o));
    if (us != NULL) {
        us->bunion(OTD_unit(otd));
        return us;
    }
    m_tmp_us.clean();
    m_tmp_us.bunion(OTD_unit(otd));
    return &m_tmp_us;
}


//Allocate 'sym' at stack.
//'base' may be SP or FP.
void CG::computeAndUpdateStackVarLayout(
        xoc::VAR const* var,
        OUT SR ** base, //stack pointer
        OUT SR ** base_ofst)
{
    ASSERT0(var && base && base_ofst);
    ASSERT0(VAR_is_local(var));
    if (VAR_is_formal_param(var)) {
        computeParamterLayout(var, base, base_ofst);
        return;
    }

    Section * section = &m_stack_sect;
    VarDesc * vd = NULL;
    if (SECT_var_list(section).find(var) == 0) {
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));

        //Do not perform aligning for stack-variables.
        VD_ofst(vd) = (ULONG)SECT_size(section);
        SECT_size(section) +=
            ceil_align(var->getByteSize(m_tm), STACK_ALIGNMENT);

        SECT_var2vdesc_map(section).set(var, vd);
        SECT_var_list(section).append_tail(var);
    } else {
        vd = SECT_var2vdesc_map(section).get(var);
    }

    if (m_is_use_fp) {
        if (base != NULL) {
            *base = genFP();
        }

        if (base_ofst != NULL) {
            if (m_is_compute_sect_offset) {
                *base_ofst = genIntImm((HOST_INT)-(LONG)VD_ofst(vd), true);
            } else {
                *base_ofst = genVAR(var);
            }
        }
    } else {
        if (base != NULL) {
            *base = genSP();
        }

        if (base_ofst != NULL) {
            if (m_is_compute_sect_offset) {
                *base_ofst = genIntImm((HOST_INT)VD_ofst(vd), false);
            } else {
                *base_ofst = genVAR(var);
            }
        }
    }
}


//Compute and append parameter into PARAM-Section.
void CG::computeParamterLayout(
        xoc::VAR const* var,
        OUT SR ** base,
        OUT SR ** ofst)
{
    ASSERT0(var);
    ASSERT0(VAR_is_local(var));
    Section * section = &m_param_sect;
    VarDesc * vd = NULL;
    if (SECT_var_list(section).find(var) == 0) {
        vd = (VarDesc*)xmalloc(sizeof(VarDesc));

        //Do not perform aligning for stack-variables.
        VD_ofst(vd) = (ULONG)SECT_size(section);
        SECT_size(section) += ceil_align(
            var->getByteSize(m_tm), STACK_ALIGNMENT);
        SECT_var2vdesc_map(section).set(var, vd);
        SECT_var_list(section).append_tail(var);
    } else {
        vd = SECT_var2vdesc_map(section).get(var);
    }
    ASSERT0(vd);

    if (base != NULL) {
        //*base = genParamPointer();
        if (m_is_use_fp) {
            *base = genFP();
        } else {
            *base = genSP();
        }
    }

    if (ofst != NULL) {
        if (m_is_compute_sect_offset) {
            *ofst = genIntImm((HOST_INT)VD_ofst(vd), false);
        } else {
            *ofst = genVAR(var);
        }
    }
}


SR * CG::computeAndUpdateImmOfst(SR * sr, HOST_UINT frame_size)
{
    ASSERT0(sr);
    xoc::VAR const* var = SR_var(sr);

    if (VAR_is_formal_param(var)) {
        VarDesc const* vd = SECT_var2vdesc_map(&m_param_sect).get(var);
        HOST_UINT l = frame_size + (HOST_UINT)VD_ofst(vd) +
            (HOST_UINT)SR_var_ofst(sr);
        return genIntImm((HOST_INT)l, false);
    }

    VarDesc * vd = SECT_var2vdesc_map(&m_stack_sect).get(var);
    ASSERT0(vd);
    HOST_UINT x = (HOST_UINT)getMaxArgSectionSize() +
        (HOST_UINT)VD_ofst(vd) + (HOST_UINT)SR_var_ofst(sr);
    return genIntImm((HOST_INT)x, false);
}


//Compute the immediate offset for stack variable and
//supersede the symbol variable with the offset.
void CG::computeStackVarImmOffset()
{
    HOST_UINT const frame_size = (HOST_UINT)SECT_size(&m_stack_sect) +
        getMaxArgSectionSize();

    List<ORBB*> * bblist = getORBBList();
    for (ORBB * bb = bblist->get_head();
         bb != NULL; bb = bblist->get_next()) {
        if (ORBB_ornum(bb) == 0) { continue; }

        ORCt * ct;
        for (OR * o = ORBB_orlist(bb)->get_head(&ct);
             o != NULL; o = ORBB_orlist(bb)->get_next(&ct)) {
            if (OR_is_load(o)) {
                SR * ofst = o->get_load_ofst();
                ASSERT0(ofst);
                if (!SR_is_var(ofst)) { continue; }

                o->set_load_ofst(computeAndUpdateImmOfst(ofst, frame_size));
                OR_is_need_compute_var_ofst(o) = false;
            } else if (OR_is_store(o)) {
                SR * ofst = o->get_store_ofst();
                ASSERT0(ofst);
                if (!SR_is_var(ofst)) { continue; }

                o->set_store_ofst(computeAndUpdateImmOfst(ofst, frame_size));
                OR_is_need_compute_var_ofst(o) = false;
            } else if (o->isNeedComputeVAROfst()) {
                for (UINT i = 0; i < o->result_num(); i++) {
                    SR * res = o->get_result(i);
                    if (!SR_is_var(res)) { continue; }

                    o->set_result(i, computeAndUpdateImmOfst(res, frame_size));
                }

                for (UINT i = 0; i < o->opnd_num(); i++) {
                    SR * opnd = o->get_opnd(i);
                    if (!SR_is_var(opnd)) { continue; }

                    o->set_opnd(i, computeAndUpdateImmOfst(opnd, frame_size));
                }

                OR_is_need_compute_var_ofst(o) = false;
            }
        }
    }
}


void CG::computeVarBaseOffset(
        xoc::VAR const* var,
        ULONGLONG var_ofst,
        OUT SR ** base,
        OUT SR ** ofst)
{
    ASSERT0(var && base && ofst);
    *base = *ofst = NULL;

    if (VAR_is_local(var)) {
        computeAndUpdateStackVarLayout(var, base, ofst);
        if (m_is_compute_sect_offset) {
            ASSERTN(SR_is_int_imm(*ofst), ("offset must be imm"));
            SR_int_imm(*ofst) += (HOST_INT)var_ofst;
        } else {
            ASSERTN(SR_is_var(*ofst), ("offset must be var"));
            SR_var_ofst(*ofst) += (UINT)var_ofst;
        }
    } else if (VAR_is_global(var)) {
        computeAndUpdateGlobalVarLayout(var, base, ofst);
    } else {
        ASSERTN(0, ("Unsupported"));
    }
}


xoc::VAR const* CG::computeSpillVar(OR * o)
{
    if (OR_is_load(o) || OR_is_store(o)) {
        return mapOR2Var(o);
    }
    return NULL;
}


void CG::freePackage()
{
    for (INT i = 0; i <= m_ipl_vec.get_last_idx(); i++) {
        IssuePackageList * ipl = m_ipl_vec.get(i);
        if (ipl != NULL) {
            for (IssuePackage * ip = ipl->get_head();
                 ip != NULL; ip = ipl->get_next()) {
                delete ip;
            }
            delete ipl;
        }
    }

    m_ipl_vec.clean();
}


void CG::initBuiltin()
{
    m_builtin_memcpy = addBuiltinVar("memcpy");
}


//Initialize machine info before compiling function unit.
//e.g: stack info, parameter info.
void CG::initFuncUnit()
{
    ASSERT0(m_ru->is_function() || m_ru->is_program());

    //Initializing formal parameter section.
    ASSERT0(m_ru->getRegionVar());

    List<xoc::VAR const*> param_list;
    m_ru->findFormalParam(param_list, true);

    UINT i = 0;
    for (xoc::VAR const* v = param_list.get_head();
         v != NULL; v = param_list.get_next()) {
         //Append parameter into PARAM-Section in turn.
         computeParamterLayout(v, NULL, NULL);
         m_params.set(i, v);
    }
}


//Initialize program-section information, which might include
//but not limited CODE, DATA, RODATA, BSS.
//Generate new symbol for each of sections in order to
//assign concise name of them.
void CG::initSections(VarMgr * vm)
{
    ASSERT0(vm);

    xoc::Type const* type = m_tm->getMCType(4);

    //.code
    SECT_var(&m_code_sect) = vm->registerVar(".code",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    VAR_is_unallocable(SECT_var(&m_code_sect)) = true;
    SECT_size(&m_code_sect) = 0;
    SECT_id(&m_code_sect) = m_sect_count++;

    //.data
    SECT_var(&m_data_sect) = vm->registerVar(".data",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    VAR_is_unallocable(SECT_var(&m_data_sect)) = true;
    SECT_size(&m_data_sect) = 0;
    SECT_id(&m_data_sect) = m_sect_count++;

    //.rodata
    SECT_var(&m_rodata_sect) = vm->registerVar(".rodata",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    SECT_size(&m_rodata_sect) = 0;
    SECT_id(&m_rodata_sect) = m_sect_count++;
    VAR_is_unallocable(SECT_var(&m_rodata_sect)) = true;

    //.bss
    SECT_var(&m_bss_sect) = vm->registerVar(".bss",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    SECT_size(&m_bss_sect) = 0;
    SECT_id(&m_bss_sect) = m_sect_count++;
    VAR_is_unallocable(SECT_var(&m_bss_sect)) = true;

    //.stack
    SECT_var(&m_stack_sect) = vm->registerVar(".stack",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    SECT_size(&m_stack_sect) = 0;
    SECT_id(&m_stack_sect) = m_sect_count++;
    VAR_is_unallocable(SECT_var(&m_stack_sect)) = true;

    //.param section
    SECT_var(&m_param_sect) = vm->registerVar(".param_sect",
        type, 1, VAR_GLOBAL|VAR_FAKE);
    SECT_size(&m_param_sect) = 0;
    SECT_id(&m_param_sect) = m_sect_count++;
    VAR_is_unallocable(SECT_var(&m_param_sect)) = true;
}


void CG::initGlobalVar(VarMgr * vm)
{
    //Record global VAR in .data section.
    VarVec * varvec = vm->get_var_vec();
    SR * base, * ofst;
    for (INT i = 0; i <= varvec->get_last_idx(); i++) {
        xoc::VAR * v = varvec->get(i);
        if (v == NULL) { continue; }

        if (v->is_global() &&
            !v->is_unallocable() &&
            !v->is_fake() &&

            //Do not add file-region-private-var
            //into DATA section ahead of time.
            !v->is_private()) {
            computeVarBaseOffset(v, 0, &base, &ofst);
        }
    }
}


void CG::finiFuncUnit()
{
    //Reset target machine for next function compilation.
    m_dedicate_sr_tab.clean();
    m_param_pointer = NULL;
    m_reg_count = 0;
    m_stack_sect.clean();
    m_param_sect.clean();
    freePackage();
}


//Duplicate OR with unique id.
OR * CG::dupOR(OR const* o)
{
    ASSERT0(o);
    OR * n = genOR(OR_code(o));
    n->clone(o);
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
        if (ipl == NULL) { continue; }

        xcom::C<IssuePackage*> * ipct;
        for (ipl->get_head(&ipct); ipct != ipl->end();
             ipct = ipl->get_next(ipct)) {
            IssuePackage * ip = ipct->val();
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                OR * o = ip->get(s);
                if (o == NULL) { continue; }
                maxpad = MAX(maxpad, strlen(OR_code_name(o)));
            }
        }
    }
    return maxpad;
}


void CG::dumpPackage()
{
    if (xoc::g_tfile == NULL) { return; }

    note("\n==---- DUMP Package, Region(%d)'%s' ----==",
         REGION_id(getRegion()),
         getRegion()->getRegionName() == NULL ?
             "--" : getRegion()->getRegionName());

    xcom::StrBuf format(128);
    format.strcat("%%-%ds", compute_pad());

    for (INT bbid = 0; bbid <= m_ipl_vec.get_last_idx(); bbid++) {
        IssuePackageList * ipl = m_ipl_vec.get(bbid);
        if (ipl == NULL) { continue; }

        note("\n-- BB%d --", bbid);
        xcom::C<IssuePackage*> * ipct;
        for (ipl->get_head(&ipct); ipct != ipl->end();
             ipct = ipl->get_next(ipct)) {
            note("\n{");
            IssuePackage * ip = ipct->val();
            for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
                if (s != FIRST_SLOT) { fprintf(xoc::g_tfile, "|"); }

                OR * o = ip->get(s);
                if (o == NULL) {
                    fprintf(xoc::g_tfile, format.buf, " ");
                } else {
                    fprintf(xoc::g_tfile, format.buf, OR_code_name(o));
                }
            }
            fprintf(xoc::g_tfile, "}");
        }
    }
    fflush(xoc::g_tfile);
}


void CG::dumpSection()
{
    if (xoc::g_tfile == NULL) { return; }
    fprintf(xoc::g_tfile, "\n==---- DUMP Section VAR info ----==\n");
    m_code_sect.dump(this);
    m_data_sect.dump(this);
    m_rodata_sect.dump(this);
    m_bss_sect.dump(this);
    m_stack_sect.dump(this);
    m_param_sect.dump(this);
    fflush(xoc::g_tfile);
}


void CG::setMapOR2Mem(OR * o, xoc::VAR const* m)
{
    if (OR_is_load(o) || OR_is_store(o)) {
        CG_or2memaddr_map(this).set(o, m);
    } else {
        UNREACHABLE();
    }
}


xoc::VAR const* CG::mapOR2Var(OR * o)
{
    if (OR_is_load(o) || OR_is_store(o)) {
        return CG_or2memaddr_map(this).get(o);
    }
    return NULL;
}


//Compute regfile set that 'regs' indicated.
bool CG::mapRegSet2RegFile(
        IN OUT Vector<INT> & regfilev,
        RegSet const* regs)
{
    for (INT reg = regs->get_first(); reg != -1; reg = regs->get_next(reg)) {
        ASSERTN(reg > 0, ("First register number starts from zero at least."));
        REGFILE regfile = tmMapReg2RegFile(reg);
        INT count = regfilev.get(regfile);
        regfilev.set(regfile, ++count);
    }
    return true;
}


UNIT CG::mapSR2Unit(IN OR const*, SR const* sr)
{
    if (SR_is_reg(sr) && SR_regfile(sr) != RF_UNDEF) {
        UnitSet us;
        return mapRegFile2UnitSet(SR_regfile(sr), sr, us).checkAndGet();
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
bool CG::changeORUnit(
        OR * o,
        UNIT to_unit,
        CLUST to_clust,
        Vector<bool> const& regfile_unique,
        bool is_test)
{
    ASSERTN(o && to_unit != 0 && to_clust != CLUST_UNDEF, ("o is NULL"));

    //Get corresponding opcode.
    OR_TYPE new_opc = computeEquivalentORType(OR_code(o), to_unit, to_clust);
    if (new_opc == OR_UNDEF) {
        return false;
    }

    //Regfile needs to replaced amenable for instruction-constrain.
    //Thus, we perform conservative inspection for regfiles of 'o'.
    UNIT from_unit = computeORUnit(o)->checkAndGet();
    CLUST from_clust = computeORCluster(o);

    //Check regfile resource constrains.
    if (from_unit != to_unit || from_clust != to_clust) {
        UINT i;
        for (i = 0; i < o->result_num(); i++) {
            SR * sr = o->get_result(i);
            if (isBusSR(sr)) {
                continue;
            }

            if (SR_is_reg(sr) && regfile_unique.get(SR_sregid(sr))) {
                //Even if regfile of 'sr' has been marked as UNIQUE,
                //but some of them still have chance to change to other
                //function unit when the new function unit could also
                //use the regfile of 'sr'.
                ASSERTN(SR_regfile(sr) != RF_UNDEF, ("illegal unique regfile"));
                //First handle the specical case.
                if (sr == genSP()) {
                    if (!isSPUnit(to_unit))  {
                        return false;
                    }
                } else if (isValidResultRegfile(new_opc, i, SR_regfile(sr))) {
                    if (mapRegFile2Cluster(SR_regfile(sr), sr) != to_clust) {
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
            if (SR_is_reg(sr) && regfile_unique.get(SR_sregid(sr))) {
                ASSERTN(SR_regfile(sr) != RF_UNDEF,
                        ("Regfile unique sr should alloated regfile"));
                //First handle the specical case.
                if (sr == genSP()) {
                    if (!isSPUnit(to_unit))  {
                        return false;
                    }
                } else if (isValidOpndRegfile(new_opc, i, SR_regfile(sr))) {
                    if (mapRegFile2Cluster(SR_regfile(sr), sr) != to_clust) {
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
        } //end for each of opnd
    } //end if (from_unit != to_unit || from_clust != to_clust) {

    if (is_test) {
        return true;
    }

    if (!changeORType(o, new_opc, from_clust, to_clust, regfile_unique)) {
        ASSERTN(0, ("OR_TYPE(%s) has NO alternative on the given unit!",
                OR_code_name(o)));
        return false;
    }

    OR_clust(o) = to_clust;
    return true;
}


//Change the correlated cluster of 'o'
//If is_test is true, this function only check whether the given
//OR can be changed.
bool CG::changeORCluster(
        OR * o,
        CLUST to_clust,
        Vector<bool> const& regfile_unique,
        bool is_test)
{
    if (OR_is_bus(o) ||
        OR_is_asm(o) ||
        OR_is_fake(o) ||
        to_clust == CLUST_UNDEF) {
        return false;
    }
    UNIT to_unit = computeEquivalentORUnit(o, to_clust);
    return changeORUnit(o, to_unit, to_clust, regfile_unique, is_test);
}


//Change 'o' to 'ot', modifing all operands and results.
bool CG::changeORType(
        OR * o,
        OR_TYPE ot,
        CLUST src,
        CLUST tgt,
        Vector<bool> const& regfile_unique)
{
    DUMMYUSE(src);
    ASSERTN(tgt != CLUST_UNDEF, ("need cluster info"));
    UINT i;
    //Performing verification and substitution certainly.
    for (i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        if (SR_is_reg(sr)) {
            if (SR_is_pred(sr)) {
                continue;
            }

            //Handle dedicated SR.
            if (SR_is_dedicated(sr)) {
                ASSERTN(0, ("TODO"));
                continue;
            }

            //Handle general SR.
            if (SR_phy_regid(sr) != REG_UNDEF) {
                ASSERTN(SR_regfile(sr) != RF_UNDEF, ("Loss regfile info"));
                ASSERTN(tgt == mapReg2Cluster(SR_phy_regid(sr)), ("Unmatch info"));
            } else {
                if (!regfile_unique.get(SR_sregid(sr))) {
                    SR_phy_regid(sr) = REG_UNDEF;
                    SR_regfile(sr) = RF_UNDEF;
                }
            }
        }
    }

    for (i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        if (SR_is_reg(sr)) {
            if (SR_is_pred(sr)) {
                continue;
            }

            //Handle dedicated sr.
            if (SR_is_dedicated(sr)) {
                ASSERTN(0, ("TODO"));
                continue;
            }

            //Handle general sr.
            if (SR_phy_regid(sr) != REG_UNDEF) {
                ASSERTN(SR_regfile(sr) != RF_UNDEF, ("Loss regfile info"));

                //When 'sr' has been assigned register, the 'tgt' cluster
                //must be as same as the cluster which 'sr' correlated to.
                //ASSERTN(tgt == mapReg2Cluster(SR_phy_regid(sr)),
                //       ("Unmatch info"));
                if (tgt != mapReg2Cluster(SR_phy_regid(sr))) {
                    interwarn("Unmatch info, may generate redundant copy.");
                    return false;
                }
            } else {
                if (!regfile_unique.get(SR_sregid(sr))) {
                    SR_phy_regid(sr) = REG_UNDEF;
                    SR_regfile(sr) = RF_UNDEF;
                }
            }
        }
    }
    OR_code(o) = ot;
    return true;
}


//Return true if regfile can be assigned to referred operand.
bool CG::isValidRegFile(
        OR_TYPE ot,
        INT opndnum,
        REGFILE regfile,
        bool is_result) const
{
    if (is_result) {
        return isValidResultRegfile(ot, opndnum, regfile);
    } else {
        return isValidOpndRegfile(ot, opndnum, regfile);
    }
}


//Return true if regfile can be assigned to referred operand.
bool CG::isValidRegFile(
        OR * o,
        SR const* opnd,
        REGFILE regfile,
        bool is_result) const
{
    bool is_valid = false;

    //Note:'o' may owns a number of SR like opnd,
    //and all of them must be checked.
    //e.g: store t1, t1, 0x100  //means [t1+0x100] = t1
    if (is_result) {
        for (UINT i = 0; i < o->result_num(); i++) {
            if (isSREqual(o->get_result(i), opnd)) {
                if (isValidResultRegfile(OR_code(o), i, regfile)) {
                    is_valid = true;
                } else {
                    return false;
                }
            }
            //TODO: Should check if regfile is consistent with other operand.
        }
    } else {
        for (UINT i = 0; i < o->opnd_num(); i++) {
            if (isSREqual(opnd, o->get_opnd(i))) {
                if (isValidOpndRegfile(OR_code(o), i, regfile)) {
                    is_valid = true;
                } else {
                    return false;
                }
            }
            //TODO: Should check if regfile is consistent with other operand.
        }
    }
    return is_valid;
}


//Checking for the safe condition of copy-value.
bool CG::isConsistentRegFileForCopy(REGFILE rf1, REGFILE rf2)
{
    if (rf1 == rf2) { return true; }
    return false;
}


bool CG::isCompareOR(OR const* o)
{
    return OR_is_eq(o) || OR_is_lt(o) || OR_is_gt(o);
}


//Is o predicated by TRUE condition?
bool CG::isCondExecOR(OR * o)
{
    if (OR_is_predicated(o)) {
        SR * sr = o->get_pred();
        ASSERT0(sr);
        if (sr != genTruePred()) {
            return true;
        }
    }
    return false;
}


//TODO: support integer multiplication, logical operation, etc.
bool CG::isReductionOR(OR * o)
{
    ASSERTN(0, ("Target Dependent Code"));
    DUMMYUSE(o);
    return false;
}


//Which case is safe to optimize?
//If 'prev' is must-execute, 'next' is cond-execute, we say that is
//safe to optimize.
//Since must-execute operation is the dominator in execute-path.
bool CG::isSafeToOptimize(OR * prev, OR * next)
{
    DUMMYUSE(prev);
    DUMMYUSE(next);
    SR * p1 = prev->get_pred();
    if (p1 == NULL || p1 == genTruePred()) {
        return true;
    }
    return false;
}


bool CG::isSREqual(SR const* sr1, SR const* sr2) const
{
    if (sr1 == sr2) {
        return true;
    }

    if (SR_is_reg(sr1) && SR_is_reg(sr2) &&
        SR_regfile(sr1) == SR_regfile(sr2) &&
        SR_phy_regid(sr1) != REG_UNDEF &&
        SR_phy_regid(sr2) != REG_UNDEF &&
        SR_phy_regid(sr1) == SR_phy_regid(sr2)) {
        return true;
    }
    return false;
}


//Return true if both or1 and or2 are spill operation, as well as the
//spill location.
bool CG::isSameSpillLoc(OR const* or1, OR const* or2)
{
    xoc::VAR const* or1loc = computeSpillVar(const_cast<OR*>(or1));
    return isSameSpillLoc(or1loc, or1, or2);
}


//Return true if both or1 and or2 are spill operation, as well as the
//spill location.
bool CG::isSameSpillLoc(xoc::VAR const* or1loc, OR const* or1, OR const* or2)
{
    ASSERT0(OR_is_mem(or1) && OR_is_mem(or2));

    xoc::VAR const* or2loc = computeSpillVar(const_cast<OR*>(or2));

    if (or1loc != or2loc) { return false; }

    SR const* or1ofst = NULL;
    if (OR_is_load(or1)) {
        or1ofst = const_cast<OR*>(or1)->get_load_ofst();
    } else {
        ASSERT0(OR_is_store(or1));
        or1ofst = const_cast<OR*>(or1)->get_store_ofst();
    }
    ASSERT0(or1ofst);

    SR const* or2ofst = NULL;
    if (OR_is_load(or2)) {
        or2ofst = const_cast<OR*>(or2)->get_load_ofst();
    } else {
        ASSERT0(OR_is_store(or2));
        or2ofst = const_cast<OR*>(or2)->get_store_ofst();
    }
    ASSERT0(or2ofst);

    if ((SR_is_var(or1ofst) &&
         SR_is_var(or2ofst) &&
         SR_var(or1ofst) == SR_var(or2ofst) &&
         SR_var_ofst(or1ofst) == SR_var_ofst(or2ofst)) ||
        (SR_is_int_imm(or1ofst) &&
         SR_is_int_imm(or2ofst) &&
         SR_int_imm(or1ofst) == SR_int_imm(or2ofst))) {
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
    if (isSREqual(p1, genTruePred()) &&
        isSREqual(p2, genTruePred())) {
        return true;
    }

    //CASE:Cannot only compare p1==p2, e.g:
    //         = t1(p1) //prev
    //  t1(p1) =
    //         = t1(p1) //next
    //  prev and next are not same cond-exec.

    ORCt * ct = NULL;
    BBORList * torlst = const_cast<BBORList*>(or_list);
    torlst->find(prev, &ct);
    if (isSREqual(p1, p2)) {
        SR * pd = p1;
        OR * test;
        for (test = torlst->get_next(&ct);
             test != NULL; test = torlst->get_next(&ct)) {
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
bool CG::isValidOpndRegfile(
        OR_TYPE ortype,
        INT opndnum,
        REGFILE regfile) const
{
    if (regfile == RF_UNDEF || opndnum < 0) {
        return false;
    }
    RegFileSet const* rfs = getValidRegfileSet(ortype, opndnum, false);
    ASSERTN(rfs != NULL, ("miss target machine info"));
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
bool CG::isValidRegInSRVec(OR *, SR * sr, UINT idx, bool is_result)
{
    DUMMYUSE(is_result);
    DUMMYUSE(idx);
    ASSERTN(0, ("Target Dependent Code"));
    if (SR_vec(sr) != NULL) {
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
bool CG::isReduction(OR * o)
{
    if (isCondExecOR(o)) { return false; }

    INT reduct_opnd = 0;
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (!SR_is_reg(opnd) ||
            !SR_is_global(opnd) ||
            opnd == genTruePred()) {
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
    ORTypeDesc const* otd = tmGetORTypeDesc(ot);
    SRDescGroup<> const* sdg = OTD_srd_group(otd);
    SRDesc const* sr_desc = sdg->get_opnd(idx);
    ASSERT0(sr_desc && SRD_is_imm(sr_desc));
    HOST_UINT pimm = abs(imm);
    HOST_UINT mask = (((HOST_UINT)1) << SRD_bitsize(sr_desc)) - 1;
    return (mask & pimm) == pimm;
}


//Calculate the cluster for inline-assembly operation.
CLUST CG::computeAsmCluster(IN OR * o)
{
    if (!OR_is_asm(o)) {
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
        if (!SR_is_reg(sr)) { continue; }

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
        if (!SR_is_reg(sr)) {
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
    ASSERT_DUMMYUSE(CLUST_UNDEF + 2 == CLUST_NUM, ("Only one cluster."));
    return (CLUST)(CLUST_UNDEF + 1);
}


SLOT CG::computeORSlot(OR const*)
{
    SLOT slot = FIRST_SLOT;
    ASSERTN(0, ("Target Dependent Code"));
    return slot;
}


//Amendment the illegal BASE-SR of memory load/store.
void CG::fixCluster(IN OUT ORList & spill_ors, CLUST clust)
{
    DUMMYUSE(spill_ors);
    DUMMYUSE(clust);
    ASSERT0_DUMMYUSE(CLUST_UNDEF + 2 == CLUST_NUM);
}


//Return true if all args has been passed through registers, otherwise
//return false.
bool CG::passArgThroughRegister(
        OUT ArgDescMgr * argdescmgr,
        OUT ORList & ors,
        IOC *)
{
    IOC tcont;
    for (; argdescmgr->getCurrentDesc() != NULL;) {
        SR * argreg = argdescmgr->allocArgRegister(this);
        if (argreg == NULL) {
            return false;
        }
        ArgDesc const* desc = argdescmgr->pullOutDesc();
        ASSERT0(!desc->is_record_addr);
        SR * arg_val = desc->src_value;
        //ASSERT0(arg_val && arg_val->getByteSize() == GENERAL_REGISTER_SIZE);
        //Only move register size data for each time.
        ASSERT0(arg_val);   

        tcont.clean();
        buildMove(argreg, arg_val, ors, &tcont);
        ASSERT0(arg_val->getByteSize() == GENERAL_REGISTER_SIZE);
        argdescmgr->updatePassedArgInRegister(GENERAL_REGISTER_SIZE);
    }
    return true;
}


//Generate code to store SR on top of stack.
//argdescmgr: record the parameter which tend to store on the stack.
void CG::storeArgToStack(
        ArgDescMgr * argdescmgr,
        OUT ORList & ors,
        IN IOC *)
{
    ASSERT0(argdescmgr);
    IOC tc;
    ORList tors;
    for (ArgDesc const* desc = argdescmgr->pullOutDesc();
         desc != NULL; desc = argdescmgr->pullOutDesc()) {
        tors.clean();
        //Compute the address of parameter should be computed base
        //on SP.
        //e.g: tgt = src + callee_param_section_byte_ofst;
        if (desc->is_record_addr) {
            tc.clean();
            SR * tgt = genSP();
            SR * src = desc->src_startaddr;
            if (desc->src_ofst != 0) {
                //src = src_addr + src_ofst;
                if (src == genSP()) {
                    //Do not modify SP.
                    tc.clean();
                    SR * res = genReg();
                    buildMove(res, src, tors, &tc);
                    tc.clean();
                }
                buildAdd(src,
                    genIntImm((HOST_INT)desc->src_ofst, false),
                    GENERAL_REGISTER_SIZE,
                    false, tors, &tc);
                src = tc.get_reg(0);
            }
            
            tc.clean();
            if (desc->tgt_ofst != 0) {            
                //tgt = SP + tgt_ofst;
                buildAdd(tgt,
                    genIntImm((HOST_INT)desc->tgt_ofst, false),
                    GENERAL_REGISTER_SIZE,
                    false, tors, &tc);
                tgt = tc.get_reg(0);
            } else {
                //Do not modify SP.
                //tgt = SP;
                SR * res = genReg();
                buildMove(res, tgt, tors, &tc);
                tgt = res;
            }

            //copy(x, parameter_start_addr, copy_size);            
            ASSERT0(src && tgt && SR_is_reg(tgt) && SR_is_reg(src));
            tc.clean();
            buildMemcpyInternal(tgt, src,
                desc->arg_size, tors, &tc);
        } else {
            //[sp + tgt_ofst] = %rx;
            ASSERTN(desc->arg_size <= 8, ("TODO"));
            ASSERT0(desc->src_value);
            tc.clean();
            IOC_mem_byte_size(&tc) = desc->arg_size;
            buildStore(desc->src_value, genSP(),
                genIntImm((HOST_INT)desc->tgt_ofst, false),
                tors, &tc);
        }
        if (desc->arg_dbx != NULL) {
            tors.copyDbx(desc->arg_dbx);
        }
        ors.append_tail(tors);
    }
}


void CG::expandSpadjust(OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(o->isSpadjust());
    SR * ofst = o->get_opnd(HAS_PREDICATE_REGISTER +
                            SPADJUST_OFFSET_INDX);
    ASSERT0(SR_is_int_imm(ofst));
    IssuePackage * ip = allocIssuePackage();
    ORList ors;
    IOC cont;

    //sp = sp - SIZE_OF_STACK
    buildAdd(genSP(), ofst, GENERAL_REGISTER_SIZE, true, ors, &cont);
    ASSERT0(ors.get_elem_count() == 1);
    OR * addi = ors.get_tail();
    addi->set_result(0, genSP()); //replace result-register with SP.

    ip->set(FIRST_SLOT, addi);
    ipl->append_tail(ip);
}


void CG::expandFakeOR(OR * o, OUT IssuePackageList * ipl)
{
    ASSERT0(OR_is_fake(o));
    switch (OR_code(o)) {
    case OR_spadjust:
        expandSpadjust(o, ipl);
        break;
    default: ASSERTN(0, ("Target Dependent Code"));
    }
}


void CG::package(Vector<BBSimulator*> & simvec)
{
    List<ORBB*> * bblst = getORBBList();
    for (ORBB * bb = bblst->get_head(); bb != NULL; bb = bblst->get_next()) {
        BBSimulator * sim = simvec.get(ORBB_id(bb));
        ASSERT0(sim != NULL);

        IssuePackageList * ipl = new IssuePackageList();
        m_ipl_vec.set(ORBB_id(bb), ipl);

        UINT cyc = sim->getCurCycle();
        ORVec const* ss = sim->getExecSnapshot();

        ASSERT0_DUMMYUSE(FIRST_SLOT == LAST_SLOT);
        for (SLOT s = FIRST_SLOT; s <= LAST_SLOT; s = (SLOT)(s + 1)) {
            for (UINT i = 0; i < cyc; i++) {
                OR * o = ss[s].get(i);
                if (o == NULL) { continue; }
                if (OR_is_fake(o)) {
                    expandFakeOR(o, ipl);
                } else {
                    IssuePackage * ip = allocIssuePackage();
                    ip->set(FIRST_SLOT, o);
                    ipl->append_tail(ip);
                }
            }
        }
    }
}


//Generate TRUE predicate register.
SR * CG::genTruePred()
{
    ASSERTN(0, ("Target dependent"));
    return NULL;
}


SR * CG::genRflag()
{
    ASSERTN(0, ("Target dependent"));
    return NULL;
}


SR * CG::genSP()
{
    ASSERTN(0, ("Target dependent"));
    return NULL;
}


SR * CG::genFP()
{
    ASSERTN(0, ("Target dependent"));
    return NULL;
}


SR * CG::genGP()
{
    ASSERTN(0, ("Target dependent"));
    return NULL;
}


SR * CG::genParamPointer()
{
    if (m_param_pointer != NULL) {
        return m_param_pointer;
    }
    m_param_pointer = genReg((UINT)GENERAL_REGISTER_SIZE);
    SR_is_param_pointer(m_param_pointer) = 1;
    return m_param_pointer;
}


//Generate a SR if bytes_size not more than GENERAL_REGISTER_SIZE,
//otherwise generate a vector or SR.
//Return the first register if vector generated.
SR * CG::genReg(UINT bytes_size)
{
    SR * first = NULL;
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
            SR_phy_regid(sr) = REG_UNDEF;
            SR_regfile(sr) = RF_UNDEF;
            ls.append_tail(sr);
        }
        first = getSRVecMgr()->genSRVec(ls);
    } else {
        first = getSRMgr()->genSR();
        SR_type(first) = SR_REG;
        SR_sregid(first) = ++m_reg_count;
        setMapSymbolReg2SR(SR_sregid(first), first);
        SR_phy_regid(first) = REG_UNDEF;
        SR_regfile(first) = RF_UNDEF;
    }
    return first;
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


//Generate dedicated register by specified physical register.
SR * CG::genDedicatedReg(REG phy_reg)
{
    SR * sr = m_dedicate_sr_tab.get(phy_reg);
    if (sr == NULL) {
        sr = genReg();
        SR_regfile(sr) = tmMapReg2RegFile(phy_reg);
        ASSERTN(SR_regfile(sr) != RF_UNDEF, ("incomplete target info"));
        SR_phy_regid(sr) = phy_reg;
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


SR * CG::genLabel(LabelInfo const* li)
{
    SR * sr = getSRMgr()->genSR();
    SR_type(sr) = SR_LAB;

    //label
    SR_label(sr) = li;
    return sr;
}


//Generate variable.
SR * CG::genVAR(xoc::VAR const* var)
{
    ASSERT0(var != NULL && getSRMgr() != NULL);
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
    SR_phy_regid(sr) = reg;
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


RegFileSet const* CG::getValidRegfileSet(
        OR_TYPE ortype,
        UINT idx,
        bool is_result) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ortype);
    SRDescGroup<> const* sdg  = OTD_srd_group(otd);
    ASSERT0(sdg != NULL);
    if (is_result) {
        SRDesc const* sd = sdg->get_res(idx);
        RegFileSet const* rfs = SRD_valid_regfile_set(sd);
        return rfs;
    }

    SRDesc const* sd = sdg->get_opnd(idx);
    RegFileSet const* rfs = SRD_valid_regfile_set(sd);
    return rfs;
}


RegSet const* CG::getValidRegSet(
        OR_TYPE ortype,
        UINT idx,
        bool is_result) const
{
    ORTypeDesc const* otd = tmGetORTypeDesc(ortype);
    SRDescGroup<> const* sda  = OTD_srd_group(otd);
    ASSERT0(sda != NULL);
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


REGFILE CG::getPredicateRegfile() const
{
    ASSERT0_DUMMYUSE(HAS_PREDICATE_REGISTER);
    ASSERTN(0, ("Target Dependent Code"));
    return RF_UNDEF;
}


//Set predicate register of each operation in 'ops' same as 'o'.
void CG::setORListWithSamePredicate(IN OUT ORList & ops, IN OR * o)
{
    if (!HAS_PREDICATE_REGISTER) {
        return;
    }
    SR * pd = o->get_pred();
    if (pd == NULL) {
        pd = genTruePred();
    }
    for (OR * tmpor = ops.get_head(); tmpor != NULL; tmpor = ops.get_next()) {
        tmpor->set_pred(pd);
    }
}


void CG::setSpadjustOffset(OR * spadj, INT size)
{
    DUMMYUSE(spadj);
    DUMMYUSE(size);
    ASSERT0(spadj && OR_code(spadj) == OR_spadjust);
    ASSERTN(0, ("Target Dependent Code"));
}


//Prepend spilling operation at the tail of OR list.
void CG::prependSpill(ORBB * bb, ORList & ors)
{
    //postBuild() may insert spill code at Entry-BB.
    //ASSERT0(!ORBB_is_entry(bb));
    ORBB_orlist(bb)->append_head(ors);
}


//Append reload operation at the tail of OR list.
void CG::appendReload(ORBB * bb, ORList & ors)
{
    //postBuild() may insert reload code at Exit-BB.
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
                o->set_result(i, newsr);
            }
        }
    } else {
        for (UINT i = 0; i < o->result_num(); i++) {
            if (o->get_result(i) == oldsr) {
                o->set_result(i, newsr);
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
                o->set_opnd(i, newsr);
            }
        }
    } else {
        for (UINT i = 0; i < o->opnd_num(); i++) {
            if (o->get_opnd(i) == oldsr) {
                o->set_opnd(i, newsr);
            }
        }
    }
}


//Rename opnd and result of OR from oldsr to newsr in range
//between 'start' and the end OR of BB.
//The renaming process does not consider physical register if SR assigned.
void CG::renameOpndAndResultFollowed(
        SR * oldsr,
        SR * newsr,
        ORCt * start,
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
void CG::renameOpndAndResultFollowed(
        SR * oldsr,
        SR * newsr,
        OR * start,
        BBORList * ors)
{
    ORCt * ct = NULL;
    bool is = ors->find(start, &ct);
    CHECK_DUMMYUSE(is);
    renameOpndAndResultFollowed(oldsr, newsr, ct, ors);
}


//Rename opnd and result of OR from 'oldsr' to 'newsr' in between
//'start' and 'end'.
//Note that 'start' will be renamed, but 'end' is not.
void CG::renameOpndAndResultInRange(
        SR * oldsr,
        SR * newsr,
        ORCt * start,
        ORCt * end,
        BBORList * orlist)
{
    ASSERTN(start && end && oldsr != newsr, ("not in list"));
    for (OR * o = start->val();
         o != NULL && start != end;
         o = orlist->get_next(&start)) {
        renameOpnd(o, oldsr, newsr, false);
        renameResult(o, oldsr, newsr, false);
    }
    ASSERTN(start == end, ("out of given range"));
}


//Rename opnd and result of OR from 'oldsr' to 'newsr' in between
//'start' and 'end'.
//Note that 'start' will be renamed, but 'end' is not.
void CG::renameOpndAndResultInRange(
        SR * oldsr,
        SR * newsr,
        OR * start,
        OR * end,
        BBORList * orlist)
{
    if (start == NULL) { return; }
    ASSERT0(oldsr != newsr);
    bool in_range = false;
    ORCt * ct = NULL;
    orlist->find(start, &ct);

    ASSERTN(ct, ("not in list"));
    for (OR * o = start; o != NULL; o = orlist->get_next(&ct)) {
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


bool CG::isRegflowDep(OR * from, OR * to)
{
    for (UINT i = 0; i < from->result_num(); i++) {
        SR * sr = from->get_result(i);
        if (!SR_is_reg(sr) || SR_is_pred(sr)) {
            continue;
        }
        if (mayUse(to, sr)) {
            return true;
        }
    }
    return false;
}


bool CG::isRegoutDep(OR * from, OR * to)
{
    for (UINT i = 0; i < from->result_num(); i++) {
        SR * sr = from->get_result(i);
        if (!SR_is_reg(sr) || SR_is_pred(sr)) {
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
    ASSERTN(OR_is_asm(o), ("expect asm-o"));
    ASSERTN(SR_is_reg(sr), ("sr is not register"));
    if (SR_phy_regid(sr) == REG_UNDEF) {
        return false;
    }

    ORAsmInfo * asm_info = getAsmInfo(o);
    if (asm_info == NULL) {
        ASSERTN(0, ("asm info for o is NULL?"));
        return false;
    }
    //Check DEF register set.
    for (UINT i = 0; i < o->result_num(); i++) {
        SR * sr1 = o->get_result(i);
        if (!SR_is_reg(sr1)) {
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
            if (rs->is_contain(SR_phy_regid(sr1))) {
                return true;
            }
        } //end for each regfile
    } //end for
    return false;
}


//Check result of o.
//NOTICE: asm-o also have result tns, but its clobber-set may implicitly
//override some registers. So 'mustAsmDef()' need to do more inspection.
bool CG::mustDef(OR const* o, SR const* sr) const
{
    if (!SR_is_reg(sr)) {
        return false;
    }

    for (UINT i = 0; i < o->result_num(); i++) {
        SR * res = o->get_result(i);
        if (isSREqual(res, sr)) {
            return true;
        }
    }

    if (isSP(sr) && OR_code(o) == OR_spadjust) {
        return true;
    }

    if (OR_is_asm(o) && mustAsmDef(o, sr)) {
        return true;
    }
    return false;
}


bool CG::mustUse(OR const* o, SR const* sr) const
{
    if (!SR_is_reg(sr)) {
        return false;
    }
    for (UINT i = 0; i < o->opnd_num(); i++) {
        SR * opnd = o->get_opnd(i);
        if (isSREqual(opnd, sr)) {
            return true;
        }
    }
    if (SR_phy_regid(sr) == REG_UNDEF) {
        return false;
    }
    return false;
}


//Return true if sr was recomputed between 'start' and 'end'.
//That processing does not include 'start','end'.
bool CG::mayDefInRange(
        SR const* sr,
        IN ORCt * start,
        IN ORCt * end,
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


bool CG::mayDef(OR * o, SR const* sr)
{
    if (mustDef(o, sr)) {
        return true;
    }
    return false;
}


bool CG::mayUse(IN OR * o, SR const* sr)
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


void CG::computeEntryAndExit(
        IN OR_CFG & cfg,
        OUT List<ORBB*> & entry_lst,
        OUT List<ORBB*> & exit_lst)
{
    INT c;
    for (xcom::Vertex * v = cfg.get_first_vertex(c);
         v != NULL; v = cfg.get_next_vertex(c)) {
        ORBB * bb = cfg.getBB(VERTEX_id(v));
        ASSERT0(bb);
        if (cfg.get_in_degree(v) == 0) {
            ORBB_is_entry(bb) = true;
            entry_lst.append_tail(bb);
        }
        if (cfg.get_out_degree(v) == 0) {
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
         bb != NULL; bb = m_or_bb_list.get_next()) {
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
         bb != NULL; bb = entry_lst.get_next()) {

        //Protection of ret-address.
        if (has_call) {
            ors.clean();
            SR * sr = genReturnAddr();
            xoc::VAR * loc = genSpillVar(sr);
            ASSERT0(loc != NULL);

            IOC cont1;
            IOC_mem_byte_size(&cont1) = GENERAL_REGISTER_SIZE;
            buildStore(sr, loc, 0, ors, &cont1);
            ORBB_orlist(bb)->append_head(ors);
        }

        //Spadjust operations
        ors.clean();
        IOC_int_imm(&cont) = 0;
        buildSpadjust(ors, &cont);
        ASSERTN(ors.get_elem_count() == 1, ("at most one spadjust operation."));
        ORBB_orlist(bb)->append_head(ors);
        ORBB_entry_spadjust(bb) = ors.get_head();
    }

    for (ORBB * bb = exit_lst.get_head();
         bb != NULL; bb = exit_lst.get_next()) {
        OR * last_or = ORBB_last_or(bb);

        //Protection of ret-address.
        if (has_call) {
            ors.clean();
            SR * sr = genReturnAddr();
            xoc::VAR * loc = genSpillVar(sr);
            ASSERT0(loc != NULL);

            IOC cont2;
            IOC_mem_byte_size(&cont2) = GENERAL_REGISTER_SIZE;
            buildLoad(sr, loc, 0, ors, &cont2);
            if (last_or != NULL &&
                (OR_is_call(last_or) ||
                 OR_is_cond_br(last_or) ||
                 OR_is_uncond_br(last_or) ||
                 OR_is_return(last_or))) {
                 ORBB_orlist(bb)->insert_before(ors, last_or);
            } else {
                ORBB_orlist(bb)->append_tail(ors);
            }
        }

        //Spadjust operations
        ors.clean();
        IOC_int_imm(&cont) = 0;
        buildSpadjust(ors, &cont);
        ASSERTN(ors.get_elem_count() == 1, ("at most one spadjust operation."));
        if (last_or != NULL &&
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


void CG::addBBLevelNewVar(IN xoc::VAR * var)
{
    m_bb_level_internal_var_list.append_tail(var);
}


void CG::addFuncLevelNewVar(IN xoc::VAR * var)
{
    m_func_level_internal_var_list.append_tail(var);
}


//Construct a name for VAR that will lived in a ORBB.
CHAR const* CG::genBBLevelNewVarName(OUT xcom::StrBuf & name)
{
    name.sprint("bb_level_var_%d",
        m_bb_level_internal_var_list.get_elem_count());
    return name.buf;
}


//Generate spill location that same like 'sr'.
//Or return the spill location if exist.
//'sr': the referrence SR.
xoc::VAR * CG::genSpillVar(SR * sr)
{
    ASSERT0(SR_is_reg(sr));
    if (SR_spill_var(sr) != NULL) {
        return SR_spill_var(sr);
    }

    xoc::VAR * v;
    xoc::Type const* type = m_tm->getSimplexTypeEx(
        m_tm->getDType(WORD_BITSIZE, false));

    //Generate spill-loc for life time that belonged to whole func unit if v is
    //global variable, or else generate spill-loc for life time that only
    //lived in a single BB.
    v = genTempVar(type, STACK_ALIGNMENT, SR_is_global(sr));
    ASSERT0(v);

    VAR_is_spill(v) = true;
    SR_spill_var(sr) = v;
    return v;
}


//Adjust offset of parameter of current region.
//'lv_size': size of (local variable + temporary variable +
//    callee parameter size).
void CG::reviseFormalParamAccess(UINT lv_size)
{
    ASSERT0(!g_is_enable_fp);
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != NULL; bb = m_or_bb_list.get_next()) {
        for (OR * o = ORBB_first_or(bb); o != NULL; o = ORBB_next_or(bb)) {
            if (OR_is_load(o)) {
                if (o->get_load_base() == genParamPointer()) {
                    o->set_load_base(genSP());
                    SR * offset = o->get_load_ofst();
                    ASSERT0(SR_is_int_imm(offset) && SR_int_imm(offset) >= 0);
                    SR_int_imm(offset) += lv_size;
                }
            } else if (OR_is_store(o)) {
                if (o->get_store_base() == genParamPointer()) {
                    o->set_store_base(genSP());
                    SR * offset = o->get_store_ofst();
                    ASSERT0(SR_is_int_imm(offset) && SR_int_imm(offset) >= 0);
                    SR_int_imm(offset) += lv_size;
                }
            }
        }
    }
}


void CG::storeParameterAfterSpadjust(
        UINT regparamoffset,
        UINT regparamsize,
        List<ORBB*> * entry_lst)
{
    ASSERT0(entry_lst);
    RegSet const* phyregset = tmGetRegSetOfArgument();
    ASSERT0(phyregset);
    INT phyreg = -1;
    ORList ors;
    IOC tc;

    for (ORBB * bb = entry_lst->get_head();
        bb != NULL; bb = entry_lst->get_next()) {
        OR * spadj = ORBB_entry_spadjust(bb);
        if (spadj == NULL) { continue; }
        ORCt * orct = NULL;
        ORBB_orlist(bb)->find(spadj, &orct);
        ASSERTN(orct, ("not find spadjust in BB"));
 
        ors.clean();
        tc.clean();
        for (UINT regsz = 0; regsz < regparamsize;
            regsz += GENERAL_REGISTER_SIZE) {
            if (regsz == 0) {
                phyreg = phyregset->get_first();
            } else {
                phyreg = phyregset->get_next(phyreg);
            }
            ASSERT0(phyreg >= 0);
            SR * store_val = genDedicatedReg(phyreg);
            ASSERT0(store_val);

            IOC_mem_byte_size(&tc) = GENERAL_REGISTER_SIZE;
            buildStore(store_val, genSP(),
                genIntImm((HOST_INT)(regparamoffset + regsz), false),
                ors, &tc);
        }

        ORBB_orlist(bb)->insert_after(ors, orct);
    }
}


void CG::reviseFormalParameterAndSpadjust()
{
    //size of (register caller parameter size + local variable + 
    //         temporary variable + callee parameter size).
    INT size = (INT)SECT_size(getStackSection()) + getMaxArgSectionSize();

    //Adjust size by stack alignment.
    ASSERTN(isPowerOf2(STACK_ALIGNMENT),
            ("For the convenience of generating double load/store"));
    UINT mask = STACK_ALIGNMENT - 1; //get the mask.
    ASSERTN(mask <= 0xFFFF, ("Stack alignment is too big"));
    size = (size + mask) & ~mask; //size is not less than stack alignment.

    if (!g_is_enable_fp &&
        (getMaxArgSectionSize() > 0 &&
         SECT_size(getParamSection()) > 0)) {
        reviseFormalParamAccess(size);
    }

    List<ORBB*> entry_lst;
    List<ORBB*> exit_lst;
    computeEntryAndExit(*m_or_cfg, entry_lst, exit_lst);

    if (isPassArgumentThroughRegister()) {
        ASSERT0(tmGetRegSetOfArgument());
        UINT regsize = GENERAL_REGISTER_SIZE *
            tmGetRegSetOfArgument()->get_elem_count();
        UINT regparamsize = MIN((UINT)SECT_size(getParamSection()), regsize);
        storeParameterAfterSpadjust(size, regparamsize, &entry_lst);
        size += (INT)regparamsize;
    }
        
    for (ORBB * bb = entry_lst.get_head();
         bb != NULL; bb = entry_lst.get_next()) {
        OR * spadj = ORBB_entry_spadjust(bb);
        if (spadj == NULL) { continue; }
        if (size == 0) {
            //Spadjust OR should be removed if the frame length is zero.
            //Do not free o, its SR may be used by other OR.
            ORBB_orlist(bb)->remove(spadj);
            continue;
        }

        setSpadjustOffset(spadj, -size);
    }

    for (ORBB * bb = exit_lst.get_head(); bb != NULL;
         bb = exit_lst.get_next()) {
        OR * spadj = ORBB_exit_spadjust(bb);
        if (spadj == NULL) { continue; }
        if (size == 0) {
            //Spadjust OR should be removed if the frame length is zero.
            //Do not free o, its SR may be used by other OR.
            ORBB_orlist(bb)->remove(spadj);
            continue;
        }

        setSpadjustOffset(spadj, size);
    }
}


//Return true if 'test_tn' is the one of operands of 'o' ,
//it is also the results.
//'test_tn': can be NULL. And if it is NULL, we only try to
//get the index-info of the same opnd and result.
bool CG::isOpndSameWithResult(
        SR const* test_tn,
        IN OR * o,
        OUT INT * opndnum,
        OUT INT * resnum)
{
    if (opndnum == NULL) {INT o1; opndnum = &o1;}
    if (resnum == NULL) {INT o1; resnum = &o1;}
    *opndnum = -1;
    *resnum = -1;
    //Find the sr with 'same_res' property.
    SR * the_tn = NULL;
    for (UINT resn = 0; resn < o->result_num(); resn++) {
        for (UINT opndn = 0; opndn < o->opnd_num(); opndn++) {
            SR * res = o->get_result(resn);
            if (res == o->get_opnd(opndn)) {
                *opndnum = opndn;
                *resnum = resn;
                the_tn = res;
                goto FIN;
            }
        }
    }
FIN:
    if (*opndnum >= 0) {
        if (test_tn == NULL) {
            return true;
        } else if (test_tn == the_tn) {
            return true;
        }
    }
    return false;
}


//Construct a name for VAR that will lived in Region.
CHAR const* CG::genFuncLevelNewVarName(OUT xcom::StrBuf & name)
{
    name.sprint("func_level_var_%d",
        m_func_level_internal_var_list.get_elem_count());
    return name.buf;
}


xoc::VAR * CG::genTempVar(xoc::Type const* type, UINT align, bool func_level)
{
    xcom::StrBuf name(64);
    xoc::Region * fu = m_ru->getFuncRegion();
    if (func_level) {
        xoc::VAR * v = fu->getVarMgr()->registerVar(
            genFuncLevelNewVarName(name),
            type, align, VAR_LOCAL);
        addFuncLevelNewVar(v);
        fu->addToVarTab(v); //v is local-used in current func-region.
        return v;
    }
    xoc::VAR * v = m_bb_level_internal_var_list.get_free();
    if (v == NULL) {
        v = fu->getVarMgr()->registerVar(
            genBBLevelNewVarName(name),
            type, align, VAR_LOCAL);
        addBBLevelNewVar(v);
        fu->addToVarTab(v); //v is local-used in current func-region.
    }
    return v;
}


void CG::constructORBBList(IN ORList & or_list)
{
    if (or_list.get_elem_count() == 0) {
        return;
    }

    ORBB * cur_bb = NULL;
    for (OR * o = or_list.get_head(); o != NULL; o = or_list.get_next()) {
        //Insert xoc::IR into individual BB.
        if (cur_bb == NULL) {
            cur_bb = allocBB();
        }

        if (cur_bb->isDownBoundary(o)) {
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
        } else if (cur_bb->isUpperBoundary(o)) {
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
        } else {
            ORBB_orlist(cur_bb)->append_tail(o);
        }
    } //end for each of ORs
    ASSERT0(cur_bb != NULL);
    m_or_bb_list.append_tail(cur_bb);

    #ifdef _DEBUG_
    //Do some verifications.
    for (ORBB * bb = m_or_bb_list.get_head();
         bb != NULL; bb = m_or_bb_list.get_next()) {
        INT cur_order = -1;
        for (OR  * o = ORBB_first_or(bb); o != NULL; o = ORBB_next_or(bb)) {
            ASSERT0(OR_order(o) != -1);
            if (cur_order == -1) {
                cur_order = OR_order(o);
            } else {
                ASSERT0(OR_order(o) > cur_order);
            }
        }
    }
    #endif
}


//Perform global and local register allocation.
RaMgr * CG::performRA()
{
    RaMgr * lm = allocRaMgr(getORBBList(), m_ru->is_function());
    lm->setParallelPartMgrVec(getParallelPartMgrVec());
    #ifdef _DEBUG_
    if (tmGetRegSetOfCallerSaved()->is_empty()) {
        xoc::interwarn("Caller Register Set is empty!"
                       "There might lack of allocable registers "
                       "if RAMGR_can_alloc_callee is disabled");
    }
    #endif
    if (isGRAEnable()) {
        lm->performGRA();
    }
    lm->performLRA();
    return lm;
}


void CG::addSerialDependence(ORBB * bb, DataDepGraph * ddg)
{
    ASSERT0(bb && ddg);
    xcom::C<OR*> * ct = NULL;
    OR * prev = NULL;
    for (ORBB_orlist(bb)->get_head(&ct);
         ct != ORBB_orlist(bb)->end(); ct = ORBB_orlist(bb)->get_next(ct)) {
        if (prev != NULL) {
            ddg->appendEdge(DEP_HYB, prev, ct->val());
        }
        //Make sure the first OR is on the graph.
        ddg->appendOR(ct->val());
        prev = ct->val();
    }
}


//Local instruction scheduling.
void CG::performIS(IN OUT Vector<BBSimulator*> & simvec, IN RaMgr * ra_mgr)
{
    START_TIMER(t, "List Schedule");

    List<ORBB*> * bblist = getORBBList();
    for (ORBB * bb = bblist->get_head();
         bb != NULL; bb = bblist->get_next()) {
        if (ORBB_ornum(bb) <= 0) { continue; }

        DataDepGraph * ddg = NULL;
        BBSimulator * sim = NULL;
        LIS * lis = NULL;
        preLS(bb, ra_mgr, &ddg, &sim, &lis);
        ASSERT0(ddg && sim && lis);

        simvec.set(ORBB_id(bb), sim); //record sim that need by package().

        if (g_do_lis) {
            ddg->build();
            lis->schedule();
        } else {
            addSerialDependence(bb, ddg);
            lis->serialize();
        }
        lis->dump(NULL, false, true);
    }

    END_TIMER(t, "List Schedule");
}


RaMgr * CG::allocRaMgr(List<ORBB*> * bblist, bool is_func)
{
    return new RaMgr(bblist, is_func, this);
}


//Return true if ir is ALLOCA.
bool CG::isAlloca(xoc::IR const* ir)
{
    if (!ir->is_call()) { return false; }

    xoc::VAR const* var = CALL_idinfo(ir);
    ASSERT0(var);
    if (strcmp(SYM_name(var->get_name()), "alloca") == 0) {
        return true;
    }
    return false;
}


//Reserving space for real parameters.
void CG::computeMaxRealParamSpace()
{
    BBList * ir_bb_list = m_ru->getBBList();
    for (IRBB * bb = ir_bb_list->get_head();
         bb != NULL; bb = ir_bb_list->get_next()) {
        xcom::C<xoc::IR*> * ct;
        for (xoc::IR * ir = BB_irlist(bb).get_head(&ct); ir != NULL;
             ir = BB_irlist(bb).get_next(&ct)) {
            if (!ir->isCallStmt()) { continue; }
            updateMaxCalleeArgSize(computeTotalParameterStackSize(ir));
            if (isAlloca(ir)) {
                m_is_use_fp = true;
            }
        }
    }
    if (getMaxArgSectionSize() > 0) {
        SECT_size(getStackSection()) += getMaxArgSectionSize();
    }
}


//'is_log': false value means that Caller will delete
//    the object allocated utilmately.
DataDepGraph * CG::allocDDG(bool is_log)
{
    DataDepGraph * p = new DataDepGraph();
    if (is_log) {
        m_ddg_list.append_tail(p);
    }
    return (DataDepGraph*)p;
}


//'is_log': false value means that Caller will delete
//    the object allocated utilmately.
LIS * CG::allocLIS(
        ORBB * bb,
        DataDepGraph * ddg,
        BBSimulator * sim,
        UINT sch_mode,
        bool change_slot,
        bool change_cluster,
        bool is_log)
{
    LIS * p = new LIS(bb, *ddg, sim, sch_mode, change_slot, change_cluster);
    if (is_log) {
        m_lis_list.append_tail(p);
    }
    return p;
}


//'is_log': false value means that Caller will delete
//    the object allocated utilmately.
BBSimulator * CG::allocBBSimulator(ORBB * bb, bool is_log)
{
    BBSimulator * p = new BBSimulator(bb);
    if (is_log) {
        m_simm_list.append_tail(p);
    }
    return (BBSimulator*)p;
}


void CG::freeSimmList()
{
    for (BBSimulator * p = m_simm_list.get_head();
         p != NULL; p = m_simm_list.get_next()) {
        delete p;
    }
    m_simm_list.clean();
}


void CG::freeDdgList()
{
    for (DataDepGraph * p = m_ddg_list.get_head();
         p != NULL; p = m_ddg_list.get_next()) {
        delete p;
    }
    m_ddg_list.clean();
}


void CG::freeLisList()
{
    for (LIS * p = m_lis_list.get_head();
         p != NULL; p = m_lis_list.get_next()) {
        delete p;
    }
    m_lis_list.clean();
}


//Create components that schedulor dependents on.
void CG::preLS(IN ORBB * bb,
               IN RaMgr * ra_mgr,
               OUT DataDepGraph ** ddg,
               OUT BBSimulator ** sim, OUT LIS ** lis)
{
    //Init DDG
    DataDepGraph * tddg = allocDDG(true);
    tddg->set_param(INC_PHY_REG, NO_MEM_READ,
        INC_MEM_FLOW, INC_MEM_OUT, NO_CONTROL, NO_REG_READ, INC_REG_ANTI,
        INC_MEM_ANTI, INC_SYM_REG);
    Vector<ParallelPartMgr*> * ppm_vec = ra_mgr->getParallelPartMgrVec();
    ParallelPartMgr * ppm = ppm_vec == NULL ? NULL : ppm_vec->get(ORBB_id(bb));
    tddg->init(bb);
    tddg->setParallelPartMgr(ppm);

    //Mark all symbol registers into the unique to prevent
    //schedulor schedules it to separate cluster.
    LRA * tlra = ra_mgr->allocLRA(bb, ppm, ra_mgr);
    Vector<bool> is_regfile_unique;
    tlra->markRegFileUnique(is_regfile_unique);
    tlra->assignCluster(*tddg, is_regfile_unique, false);
    delete tlra;

    //Init BBSimulator
    BBSimulator * tsim = allocBBSimulator(bb, true);
    UINT mode;
    mode = SCH_BRANCH_DELAY_SLOT;

    //Init LIS
    LIS * tlis = allocLIS(bb, tddg, tsim, mode, false, false, true);
    tlis->set_unique_regfile(is_regfile_unique);
    *ddg = tddg;
    *sim = tsim;
    *lis = tlis;
}


//This function does not handling SRs in bewteen stmt1 and stmt2,
//which include stmt1, not include stmt2.
//e.g:
// ... <- first_use
// ...
// first_def <- ... //stmt1
// ... <- not_first_use
// last_def <- ... //stmt2
// ...
// ... <- use
void CG::localizeBB(SR * sr, ORBB * bb)
{
    ASSERT0(SR_is_reg(sr));
    ASSERTN(!SR_is_dedicated(sr),
        ("rename dedicated register may incur illegal instruction format"));
    ASSERT0(sr && bb);
    xcom::C<OR*> * orct = NULL;
    xcom::C<OR*> * first_usestmt_ct = NULL;
    xcom::C<OR*> * first_defstmt_ct = NULL;
    xcom::C<OR*> * last_defstmt_ct = NULL;

    for (ORBB_orlist(bb)->get_head(&orct);
         orct != ORBB_orlist(bb)->end();
         ORBB_orlist(bb)->get_next(&orct)) {
        OR * o = orct->val();
        ASSERT0(o);

        if (first_usestmt_ct == NULL && first_defstmt_ct == NULL) {
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
            if (first_defstmt_ct == NULL) {
                first_defstmt_ct = orct;
            }
            last_defstmt_ct = orct;
        }
    }

    ASSERT0(first_defstmt_ct || first_usestmt_ct);
    if (SR_spill_var(sr) == NULL) {
        genSpillVar(sr);
    }
    IOC toc;
    ASSERT0(SR_spill_var(sr));
    IOC_mem_byte_size(&toc) = GENERAL_REGISTER_SIZE; // sr->getByteSize();
    ORList ors;
    if (first_usestmt_ct != NULL) {
        //Handle upward exposed use.
        toc.clean_bottomup();
        SR * newsr = genReg();
        buildLoad(newsr, SR_spill_var(sr), 0, ors, &toc);
        ASSERT0(first_usestmt_ct != ORBB_orlist(bb)->end());
        ORBB_orlist(bb)->insert_before(ors, first_usestmt_ct);
                
        if (first_defstmt_ct != NULL) {
            ASSERT0(first_usestmt_ct == first_defstmt_ct ||
                ORBB_orlist(bb)->is_or_precedes(
                    first_usestmt_ct->val(), first_defstmt_ct->val()));
        }

        if (first_usestmt_ct == first_defstmt_ct) {
            renameOpnd(first_usestmt_ct->val(), sr, newsr, false);
        } else if (first_defstmt_ct != NULL &&
                   ORBB_orlist(bb)->is_or_precedes(
                       first_usestmt_ct->val(), first_defstmt_ct->val())) {
            renameOpndAndResultInRange(sr, newsr,
                first_usestmt_ct, first_defstmt_ct, ORBB_orlist(bb));
        } else {
            //USE live through BB.
            // <-use
            // ...
            // <-use
            renameOpndAndResultFollowed(sr, newsr,
                first_usestmt_ct, ORBB_orlist(bb));
        }
    }

    if (first_defstmt_ct != NULL) {
        //Handle downward exposed use.
        toc.clean_bottomup();
        ors.clean();

        SR * newsr = genReg();
        buildStore(newsr, SR_spill_var(sr), 0, ors, &toc);
        if (HAS_PREDICATE_REGISTER) {
            SR * pd = last_defstmt_ct->val()->get_pred();
            if (pd != NULL) {
                ors.set_pred(pd);
            }
        }

        ORBB_orlist(bb)->append_tail_ex(ors);
        renameOpndAndResultFollowed(sr, newsr,
            last_defstmt_ct, ORBB_orlist(bb));
    }
}


void CG::localizeBBTab(SR * sr, TTab<ORBB*> * orbbtab)
{
    ASSERT0(sr && orbbtab);

    TabIter<ORBB*> iter;
    for (ORBB * bb = orbbtab->get_first(iter);
         bb != NULL; bb = orbbtab->get_next(iter)) {
        localizeBB(sr, bb);
    }
}


void CG::localize()
{
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
                if (!SR_is_reg(sr) || SR_is_dedicated(sr)) { continue; }

                ORBB * tgtbb = NULL;
                if ((tgtbb = sr2bb.get(sr)) == NULL) {
                    sr2bb.set(sr, bb);
                    continue;
                }
                if (tgtbb == bb) { continue; }

                TTab<ORBB*> * orbbtab = sr2orbbtab.get(sr);
                if (orbbtab == NULL) {
                    orbbtab = new TTab<ORBB*>();
                    sr2orbbtab.set(sr, orbbtab);
                    orbbtab->append(tgtbb); //Add the first BB own SR.
                }

                orbbtab->append_and_retrieve(bb); //Add new BB.
            }

            for (UINT i = 0; i < o->opnd_num(); i++) {
                SR * sr = o->get_opnd(i);
                ASSERT0(sr);
                if (!SR_is_reg(sr) || SR_is_dedicated(sr)) { continue; }

                ORBB * tgtbb = NULL;
                if ((tgtbb = sr2bb.get(sr)) == NULL) {
                    sr2bb.set(sr, bb);
                    continue;
                }
                if (tgtbb == bb) { continue; }

                TTab<ORBB*> * orbbtab = sr2orbbtab.get(sr);
                if (orbbtab == NULL) {
                    orbbtab = new TTab<ORBB*>();
                    sr2orbbtab.set(sr, orbbtab);
                    orbbtab->append(tgtbb);
                }

                orbbtab->append_and_retrieve(bb);
            }
        }
    }

    if (sr2orbbtab.get_elem_count() == 0) { return; }

    TMapIter<SR*, TTab<ORBB*>*> iter;
    TTab<ORBB*> * orbbtab;
    for (SR * sr = sr2orbbtab.get_first(iter, &orbbtab);
         sr != NULL; sr = sr2orbbtab.get_next(iter, &orbbtab)) {
        localizeBBTab(sr, orbbtab);
        delete orbbtab;
    }
}


//Check is SR assigned physical register and register file.
bool CG::verify()
{
    List<ORBB*> * bbl = getORBBList();
    for (ORBB * bb = bbl->get_head(); bb != NULL; bb = bbl->get_next()) {
        for (OR * o = ORBB_first_or(bb); o != NULL; o = ORBB_next_or(bb)) {
            for (UINT i = 0; i < o->result_num(); i++) {
                SR * sr = o->get_result(i);
                ASSERT0(sr);
                if (SR_is_reg(sr)) {
                    ASSERTN(SR_phy_regid(sr) != REG_UNDEF,
                        ("SR is not assigned physical register"));
                    ASSERTN(SR_regfile(sr) != RF_UNDEF,
                        ("SR is not assigned register file"));
                }
                if (SR_is_int_imm(sr)) {
                    ASSERT0(isValidImmOpnd(OR_code(o), SR_int_imm(sr)));
                }
            }
            for (UINT i = 0; i < o->opnd_num(); i++) {
                SR * sr = o->get_opnd(i);
                ASSERT0(sr);
                if (SR_is_reg(sr)) {
                    ASSERTN(SR_phy_regid(sr) != REG_UNDEF,
                        ("SR is not assigned physical register"));
                    ASSERTN(SR_regfile(sr) != RF_UNDEF,
                        ("SR is not assigned register file"));
                }
                if (SR_is_int_imm(sr)) {
                    ASSERT0(isValidImmOpnd(OR_code(o), i, SR_int_imm(sr)));
                }
            }
        }
    }

    return true;
}


bool CG::perform()
{
    ASSERTN(isPowerOf2(STACK_ALIGNMENT),
           ("Stack alignment should be power of 2"));

    if (m_ru->getIRList() == NULL &&
        m_ru->getBBList()->get_elem_count() == 0) {
        return true;
    }
    if (xoc::g_tfile != NULL) {
        fprintf(xoc::g_tfile,
            "\n==---- Code Generation for region(%d)'%s' ----==\n",
            REGION_id(m_ru), m_ru->getRegionName());
    }

    ORList or_list; //record OR list after converting xoc::IR to OR.
    computeMaxRealParamSpace();

    IR2OR * ir2or = allocIR2OR();
    ASSERT0(ir2or);

    //FILE * h = fopen("cg.log","a");
    //FILE * x = xoc::g_tfile;
    //xoc::g_tfile = h;
    if (m_ru->isRegionName("main"))
    {
        m_ru->dump(false);
        //dumpBBList(m_ru->getBBList(), m_ru);
    }
    //xoc::g_tfile = x;
    //fclose(h);

    CHAR const* varname = SYM_name(m_ru->getRegionVar()->get_name());
    DUMMYUSE(varname);
    ir2or->convertIRBBListToORList(or_list);

    constructORBBList(or_list);

    if (m_ru->isRegionName("main"))
    {
        dumpBBList(m_ru->getBBList(), m_ru);
        dumpORBBList(m_or_bb_list);
    }

    //Build CFG
    ASSERT0(m_or_cfg == NULL);
    m_or_cfg = allocORCFG();

    OptCtx oc;
    m_or_cfg->removeEmptyBB(oc);
    m_or_cfg->build(oc);

    m_or_cfg->dump_vcg("graph_or_cfg.vcg");
    dumpORBBList(m_or_bb_list);
    m_or_cfg->removeEmptyBB(oc);
    dumpORBBList(m_or_bb_list);
    m_or_cfg->computeExitList();
    if (m_or_cfg->removeUnreachBB()) {
        m_or_cfg->computeExitList();
    }
    dumpORBBList(m_or_bb_list);

    if (m_ru->is_function()) {
        generateFuncUnitDedicatedCode();
    }

    dumpORBBList(m_or_bb_list);
    if (!isGRAEnable()) { localize(); }

    dumpORBBList(m_or_bb_list);
    RaMgr * ra_mgr = performRA();

    bool change;
    do {
        change = false;
        if (m_or_cfg->removeEmptyBB(oc)) {
            m_or_cfg->computeExitList();
            change = true;
        }
        if (m_or_cfg->removeUnreachBB()) {
            m_or_cfg->computeExitList();
            change = true;
        }
        if (m_or_cfg->removeRedundantBranch()) {
            m_or_cfg->computeExitList();
            change = true;
        }
    } while (change);

    if (m_ru->is_function()) {
        reviseFormalParameterAndSpadjust();
    }

    Vector<BBSimulator*> simvec;
    performIS(simvec, ra_mgr);

    computeStackVarImmOffset();

    //if (m_ru->isRegionName("bar")) {
    //    getParamSection()->dump(this);
    //    getStackSection()->dump(this);
    //    dumpORBBList(m_or_bb_list);
    //}
    dumpORBBList(m_or_bb_list);
    ASSERT0(verify());

    delete ra_mgr;
    freeDdgList();
    freeLisList();

    package(simvec);
    dumpPackage();

    freeSimmList(); //Free BBSimulator list here.
    return true;
}

} //namespace xgen
