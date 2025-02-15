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
@*/
#include "../xgen/xgeninc.h"

namespace xgen {

IR2OR::IR2OR(CG * cg)
{
    ASSERT0(cg);
    m_cg = cg;
    m_cgmgr = cg->getCGMgr();
    m_rg = cg->getRegion();
    ASSERT0(m_rg);
    m_dbx_mgr = m_rg->getDbxMgr();
    ASSERT0(m_dbx_mgr);
    m_tm = m_rg->getTypeMgr();
    m_irmgr = m_rg->getIRMgr();
    ASSERT0(m_irmgr);
}


//Store value that given by address 'src_addr' to 'tgtvar'.
//ofst: offset from base of tgtvar.
void IR2OR::convertStoreViaAddress(IN SR * src_addr, IN SR * tgtvar,
                                   HOST_INT ofst, OUT RecycORList & ors,
                                   MOD IOC * cont)
{
    ASSERT0(src_addr && tgtvar && tgtvar->is_var() && SR_var(tgtvar));

    //ONLY registered operand permited.
    if (!src_addr->is_reg()) {
        if (src_addr->is_int_imm()) {
            SR * t = m_cg->genReg();
            m_cg->buildMove(t, src_addr, ors.getList(), cont);
            src_addr = t;
        }
    }
    ASSERTN(src_addr->is_reg(), ("Unsupport"));

    //Generate ::memcpy.
    cont->clean_bottomup(); //clean outdated info included addr.
    m_cg->buildLda(SR_var(tgtvar), SR_var_ofst(tgtvar) + ofst, nullptr,
                   ors.getList(), cont);
    SR * tgt_addr = cont->get_reg(0); //get target memory address.
    ASSERT0(tgt_addr);
    cont->clean_bottomup(); //clean outdated info included addr.
    m_cg->buildMemcpy(tgt_addr, src_addr, cont->getMemByteSize(),
                      ors.getList(), cont);
}


//Decompose 'mem_addr_sr' into the form 'base+offset'.
//'tgtvar': symbol describes memory location
void IR2OR::convertStoreDecompose(IN SR * src, IN SR * tgtvar,
                                  HOST_INT ofst, OUT RecycORList & ors,
                                  MOD IOC * cont)
{
    ASSERT0(src && tgtvar && tgtvar->is_var() && SR_var(tgtvar));

    //ONLY support process register operand.
    if (!src->is_reg()) {
        if (src->is_int_imm()) {
            SR * t = m_cg->genReg();
            m_cg->buildMove(t, src, ors.getList(), cont);
            src = t;
        } else {
            ASSERTN(0, ("need to support more kind of operand to store"));
        }
    }
    m_cg->buildStore(src, tgtvar, m_cg->genIntImm(ofst, true), false,
                     ors.getList(), cont);
}


//Load constant float value into register.
void IR2OR::convertLoadConstFP(IR const* ir, OUT RecycORList & ors,
                               MOD IOC * cont)
{
    //Immediate can be pointer, e.g: int * p = 0;
    ASSERT0(ir->is_fp());
    SR * load_val = m_cg->genReg();
    SR * load_val2 = nullptr;
    RecycORList tors(this);
    if (ir->getTypeSize(m_tm) == BYTE_PER_FLOAT) {
        //Float
        float val = (float)CONST_fp_val(ir);
        ASSERTN(sizeof(UINT32) == BYTE_PER_FLOAT,
                ("use suitably integer type"));
        UINT32 * pb = (UINT32*)&val;
        m_cg->buildMove(load_val, m_cg->genIntImm((HOST_INT)*pb, false),
                        tors.getList(), cont);
    } else {
        //Double
        double val = CONST_fp_val(ir);
        ASSERTN(sizeof(ULONGLONG) == BYTE_PER_FLOAT * 2,
                ("use the suitably integer type to match with question"));

        ASSERTN(sizeof(ULONGLONG) == BYTE_PER_FLOAT * 2,
                ("use suitably integer type"));

        ULONGLONG * pb = (ULONGLONG*)&val;
        ASSERTN(sizeof(ULONGLONG) == 8,
                ("use suitably macro to take low part"));

        UINT64 const ival = (UINT64)*pb;
        UINT bitnum = sizeof(UINT32) * BITS_PER_BYTE;
        UINT32 ival0 = (UINT32)xcom::get64BitValueLowNBit(ival, bitnum);
        UINT32 ival1 = (UINT32)xcom::get64BitValueHighNBit(ival, bitnum);
        m_cg->buildMove(load_val, m_cg->genIntImm(
            (HOST_INT)ival0, false), tors.getList(), cont);

        load_val2 = m_cg->genReg();
        m_cg->buildMove(load_val2, m_cg->genIntImm(
            (HOST_INT)ival1, false), tors.getList(), cont);

        m_cg->getSRVecMgr()->genSRVec(2, load_val, load_val2);
    }
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
    cont->set_reg(0, load_val);
}


void IR2OR::convertLoadConstInt(IR const* ir, OUT RecycORList & ors,
                                MOD IOC * cont)
{
    //Immediate can be pointer, e.g: int * p = 0;
    ASSERT0(ir->is_const());
    ASSERT0(ir->is_int() || ir->is_ptr());
    convertLoadConstInt(CONST_int_val(ir), ir->getTypeSize(m_tm),
                        ir->is_signed(), xoc::getDbx(ir), ors, cont);
}


//Load constant integer value into register.
void IR2OR::convertLoadConstInt(HOST_INT constval, UINT constbytesize,
                                bool is_signed, Dbx const* dbx,
                                OUT RecycORList & ors, MOD IOC * cont)
{
    //For convenient purpose, the function implemented 2x machine word
    //immediate load.
    ASSERTN(constbytesize <= 2 * GENERAL_REGISTER_SIZE,
            ("Target Dependent Code"));
    SR * load_val = m_cg->genReg();
    SR * load_val2 = nullptr;
    RecycORList tors(this);
    //Load low part.
    HOST_INT low = constval;
    if (constbytesize == 2 * GENERAL_REGISTER_SIZE &&
        //Note if WORD_LENGTH_OF_TARGET_MACHINE is equal to the bitsize of
        //HOST_UINT, the shift operation will generate zero. That means the
        //maximum host immediate type can not represent twice size of
        //GENERAL_REGISTER_SIZE.
        WORD_LENGTH_OF_TARGET_MACHINE <
            sizeof(HOST_UINT) * HOST_BIT_PER_BYTE) {
        ASSERTN(sizeof(HOST_INT) >= 2 * GENERAL_REGISTER_SIZE,
                ("host machine integer can not represent target machine"
                 "integer type"));
        //Compiler may give a complaint warning about the shift size.
        HOST_UINT v2 = (HOST_UINT)(((ULONGLONG)constval) <<
                                   WORD_LENGTH_OF_TARGET_MACHINE);
        low = v2 >> WORD_LENGTH_OF_TARGET_MACHINE;
    }
    m_cg->buildMove(load_val, m_cg->genIntImm(low, is_signed),
                    tors.getList(), cont);
    if (constbytesize == 2 * GENERAL_REGISTER_SIZE) {
        ASSERTN(sizeof(HOST_INT) >= 2 * GENERAL_REGISTER_SIZE,
                ("host machine integer can not represent target machine"
                 "integer type"));
        //Load high part.
        load_val2 = m_cg->genReg();
        HOST_INT high = 0;
        if (is_signed) {
            high = (HOST_INT)(((LONGLONG)constval) >>
                              WORD_LENGTH_OF_TARGET_MACHINE);
        } else {
            high = (HOST_INT)(((ULONGLONG)(HOST_UINT)constval) >>
                              WORD_LENGTH_OF_TARGET_MACHINE);
        }
        m_cg->buildMove(load_val2, m_cg->genIntImm(high, is_signed),
                        tors.getList(), cont);
        m_cg->getSRVecMgr()->genSRVec(2, load_val, load_val2);
    }
    tors.copyDbx(dbx, getDbxMgr());
    ors.move_tail(tors);
    cont->set_reg(0, load_val);
}


//Load constant boolean value into register.
void IR2OR::convertLoadConstBool(IR const* ir, OUT RecycORList & ors,
                                 MOD IOC * cont)
{
    //Immediate can be pointer, e.g: int * p = 0;
    ASSERT0(ir->is_int() || ir->is_ptr());
    SR * load_val = m_cg->genReg();
    //SR * load_val2 = nullptr;
    RecycORList tors(this);
    m_cg->buildMove(load_val, m_cg->genIntImm(CONST_int_val(ir), false),
                    tors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
    cont->set_reg(0, load_val);
}


void IR2OR::convertLoadConstMC(IR const* ir, OUT RecycORList & ors,
                               MOD IOC * cont)
{
    ASSERTN(0, ("Target Dependent Code"));
}


//Load constant string address into register.
void IR2OR::convertLoadConstStr(IR const* ir, OUT RecycORList & ors,
                                MOD IOC * cont)
{
    //Immediate can be pointer, e.g: int * p = 0;
    ASSERT0(ir->is_str());
    RecycORList tors(this);

    //Support loading const to lda(string-variable)
    Region * rg = m_cg->getRegion();
    IR * lda = rg->getIRMgr()->buildLdaString(nullptr, CONST_str_val(ir));

    //Record string-variable into program-region to facilitate asm-print.
    //This will storage string content at .data section.
    ASSERT0(lda->is_lda() && LDA_idinfo(lda)->is_global() &&
            rg->getTopRegion() != nullptr &&
            rg->getTopRegion()->is_program());
    rg->getTopRegion()->addToVarTab(LDA_idinfo(lda));
    ASSERT0(rg->getTopRegion()->getVarTab()->find(LDA_idinfo(lda)));

    convertLda(lda, tors, cont);
    SR * load_val = cont->get_reg(0);
    ASSERT0(load_val && load_val->is_reg());
    cont->clean_bottomup();
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
    cont->set_reg(0, load_val);
}


//Load constant into register.
void IR2OR::convertLoadConst(IR const* ir, OUT RecycORList & ors,
                             MOD IOC * cont)
{
    ASSERT0(ir->is_const());
    ASSERTN(ir->getTypeSize(m_tm) <= 2 * GENERAL_REGISTER_SIZE,
            ("Target Dependent Code"));
    if (ir->is_int() ||
        ir->is_ptr()) { //Immediate can be pointer, e.g: int * p = 0;
        convertLoadConstInt(ir, ors, cont);
        return;
    }
    if (ir->is_fp()) {
        convertLoadConstFP(ir, ors, cont);
        return;
    }
    if (ir->is_bool()) {
        convertLoadConstBool(ir, ors, cont);
        return;
    }
    if (ir->is_str()) {
        convertLoadConstStr(ir, ors, cont);
        return;
    }
    if (ir->is_mc()) {
        convertLoadConstMC(ir, ors, cont);
        return;
    }
    ASSERTN(0, ("unsupport immediate value DATA_TYPE:%s",
                TypeMgr::getDTypeName(ir->getDType())));
}


void IR2OR::tryExtendLoadValByMemSize(bool is_signed, Dbx const* dbx,
                                      OUT RecycORList & ors, MOD IOC * cont)
{
    if (cont->get_addr() != nullptr) {
        //Loaded value is passed through a variable whereas the function only
        //handle register.
        return;
    }
    SR * loadval = cont->get_reg(0);
    ASSERT0(loadval && loadval->is_reg());
    if (cont->getMemByteSize() <= loadval->getByteSize()) { return; }
    m_cg->buildTypeCvt(cont->get_reg(0), cont->getMemByteSize(),
                       loadval->getByteSize(), is_signed, dbx, ors.getList(),
                       cont);

}


//Generate ORs to load value into a symbol register.
void IR2OR::convertGeneralLoad(IR const* ir, OUT RecycORList & ors,
                               MOD IOC * cont)
{
    ASSERT0(cont != nullptr);
    switch (ir->getCode()) {
    case IR_CONST:
        convertLoadConst(ir, ors, cont);
        return;
    SWITCH_CASE_READ_PR: {
        IOC tc(*cont);
        convertGeneralLoadPR(ir, ors, &tc);
        ASSERT0((tc.get_reg(0) && tc.get_reg(0)->is_reg()) ||
                tc.get_addr());
        tryExtendLoadValByMemSize(ir->is_signed(), xoc::getDbx(ir), ors, &tc);
        cont->copy_bottomup(tc);
        return;
    }
    default:;
    }

    cont->clean_bottomup();
    convert(ir, ors, cont);
    if (cont->get_reg(0) == nullptr) {
        //The return-result might be an address.
        ASSERT0(cont->get_addr());
        return;
    }

    SR * res = cont->get_reg(0);
    ASSERTN(!ir->is_any(), ("data type of '%s' can not be ANY", IRNAME(ir)));
    ASSERT0(res && res->getByteSize() >= ir->getTypeSize(m_tm));
    if (res->is_reg()) { return; }

    //The return-result is NOT register.
    SRVec * srvec = res->getVec();
    RecycORList tors(this);
    if (srvec == nullptr) {
        //Single register move.
        SR * r = m_cg->genReg();
        m_cg->buildMove(r, res, tors.getList(), cont);
        cont->set_reg(0, r);
    } else {
        //Multiple registers move.
        List<SR*> regvlst;
        IOC tmp;
        for (UINT i = 0; i < srvec->get_elem_count(); i++) {
            SR * r = m_cg->genReg();
            regvlst.append_tail(r);
            tmp.clean();
            m_cg->buildMove(r, srvec->get(i), tors.getList(), &tmp);
            cont->set_reg(i, r);
        }
        m_cg->getSRVecMgr()->genSRVec(regvlst);
    }
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
    ASSERT0(cont->get_reg(0) && cont->get_reg(0)->is_reg());
}


void IR2OR::convertIStore(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir && ir->is_ist());
    RecycORList tors(this);

    //Load mem_address into a register
    ASSERT0(IST_base(ir)->getTypeSize(m_tm) == GENERAL_REGISTER_SIZE);
    cont->clean_bottomup();
    convertGeneralLoad(IST_base(ir), tors, cont);
    SR * addr = cont->get_reg(0);
    ASSERTN(addr && addr->is_reg(),
            ("address should be recorded in a register"));
    tors.copyDbx(IST_base(ir), getDbxMgr());
    ors.move_tail(tors);

    //Load RHS into registers or get the address of memory block.
    cont->clean_bottomup();
    tors.clean();
    convertGeneralLoad(IST_rhs(ir), tors, cont);
    ASSERT0(cont->get_reg(0) || cont->get_addr());
    tors.copyDbx(IST_rhs(ir), getDbxMgr());
    ors.move_tail(tors);

    tors.clean();
    if (cont->get_reg(0) != nullptr) {
        //Generate store.
        //Note that one should use builtStore to generate STORE with offset
        //instead of generating code that adding offset to base if
        //cont->get_reg(0) is not nullptr.
        ASSERT0(cont->get_addr() == nullptr);
        ASSERT0(ir->getTypeSize(m_tm) > 0);

        IOC tc;
        IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
        m_cg->buildStore(cont->get_reg(0), addr,
                         m_cg->genIntImm((HOST_INT)IST_ofst(ir), true),
                         ir->is_signed(), tors.getList(), &tc);
    } else {
        //Generate ::memcpy.
        ASSERT0(IST_rhs(ir)->getTypeSize(m_tm) > 8);

        ASSERT0(cont->get_addr());
        SR * srcaddr = cont->get_addr(); //save srouce addr
        cont->clean_bottomup(); //clean outdated info included addr.

        if (IST_ofst(ir) != 0) {
            m_cg->buildAdd(addr,
                           m_cg->genIntImm((HOST_INT)IST_ofst(ir), true),
                           GENERAL_REGISTER_SIZE, false, tors.getList(), cont);
            addr = cont->get_reg(0);
            ASSERTN(addr && addr->is_reg(),
                    ("address should be recorded in a register"));
            cont->clean_bottomup();
        }

        m_cg->buildMemcpy(addr, srcaddr, ir->getTypeSize(m_tm),
                          tors.getList(), cont);
    }
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertILoad(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_ild());
    IOC tc;
    //Load mem_address into a register
    ASSERT0(ILD_base(ir)->is_ptr() || ILD_base(ir)->is_any());
    convertGeneralLoad(ILD_base(ir), ors, &tc);

    SR * addr = tc.get_reg(0);
    ASSERTN(addr && addr->is_reg(),
            ("address should be recorded in a register"));

    //TODO: Use symbol address directly to diminish the number of OR.

    //The value that will be returned.
    tc.clean();
    ASSERT0(!ir->is_any());
    IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);

    RecycORList tors(this);
    m_cg->buildGeneralLoad(addr, ILD_ofst(ir), ir->is_signed(),
                           tors.getList(), &tc);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);

    ASSERT0(cont);
    cont->clean_regvec();
    SR * load_val = tc.get_reg(0);
    if (load_val != nullptr) {
        //Set result reg.
        cont->set_reg(0, load_val);
        return;
    }

    ASSERT0(tc.get_addr());
    cont->set_addr(tc.get_addr());
}


void IR2OR::convertLoadVar(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir && ir->is_ld() && cont);
    ASSERT0(LD_idinfo(ir));
    RecycORList tors(this);
    IOC tc(*cont);
    IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
    m_cg->buildGeneralLoad(m_cg->genVAR(LD_idinfo(ir)),
                           LD_ofst(ir), ir->is_signed(), tors.getList(), &tc);
    cont->copy_bottomup(tc);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


//TODO: return a symbol SR.
//Load symbol's value into register.
//'ir': type must be IR_ID.
void IR2OR::convertId(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir && ir->is_id()); //ID's type is useless.
    ASSERT0(cont);
    ASSERT0(ir->is_any() || ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE);
    RecycORList tmp_ors(this);
    IOC tc(*cont);
    IOC_mem_byte_size(&tc) = ir->is_any() ? 0 : ir->getTypeSize(m_tm);
    m_cg->buildLda(ID_info(ir), 0, xoc::getDbx(ir), tmp_ors.getList(), &tc);
    SR * loaded_val = tc.get_reg(0); //get target memory address.
    tmp_ors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tmp_ors);

    //Set result SR.
    cont->set_reg(0, loaded_val);
}


void IR2OR::convertGeneralLoadPR(IR const* ir, OUT RecycORList & ors,
                                 MOD IOC * cont)
{
    ASSERT0(ir->is_pr());
    SR * mm = m_cg->mapPR2SR(PR_no(ir));
    if (mm != nullptr) {
        ASSERT0(cont != nullptr);
        if (mm->is_reg()) {
            cont->set_reg(0, mm);
            return;
        }
        if (mm->is_var()) {
            ASSERT0(!ir->is_any());
            IOC tc;
            RecycORList tors(this);
            IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
            m_cg->buildGeneralLoad(mm, 0, ir->is_signed(), tors.getList(), &tc);
            tors.copyDbx(ir, getDbxMgr());
            ors.move_tail(tors);
            cont->copy_bottomup(tc);
            return;
        }
        UNREACHABLE();
        return;
    }

    if (ir->is_any() ||
        (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
         m_cg->isGRAEnable())) {
        m_cg->setMapPR2SR(PR_no(ir), m_cg->genReg());
    } else {
        Var * v = m_rg->getVarByPRNO(PR_no(ir));
        ASSERT0(v != nullptr);
        //TO BE DETERMINED: Does ir size have to equal to variable's size?
        //ASSERT0(ir->getTypeSize(m_tm) == v->getByteSize(m_tm));
        m_cg->setMapPR2SR(PR_no(ir), m_cg->genVAR(v));
    }

    convertGeneralLoadPR(ir, ors, cont);
}


//Copy 'src' to 'tgt's PR'.
//tgt: must be PR.
//src: register or imm.
void IR2OR::convertCopyPR(IR const* tgt, IN SR * src,
                          OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(tgt->isPROp());
    PRNO tgtprno = tgt->getPrno();
    ASSERT0(tgtprno != PRNO_UNDEF);
    SR * tgtx = m_cg->mapPR2SR(tgtprno);
    if (tgtx != nullptr) {
        ASSERT0(src != nullptr);
        RecycORList tors(this);
        IOC tc;
        if (tgtx->is_reg()) {
            ASSERT0(tgtx->getByteSize() == src->getByteSize());
            m_cg->buildMove(tgtx, src, tors.getList(), &tc);
        } else if (tgtx->is_var()) {
            IOC_mem_byte_size(&tc) = tgt->getTypeSize(m_tm);
            convertStoreDecompose(src, tgtx, 0, tors, &tc);
        } else {
            UNREACHABLE();
        }
        tors.copyDbx(tgt, getDbxMgr());
        ors.move_tail(tors);
        return;
    }

    if (tgt->is_any() ||
        (tgt->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
         m_cg->isGRAEnable())) {
        //Store to register.
        m_cg->setMapPR2SR(tgtprno, m_cg->genReg());
    } else {
        //Note the value of GSR must be stored and loaded from
        //local variable if GRA is disabled becase the value
        //must be transferd through memory.

        //Store to local-variable(memory) instead of register.
        Var * loc = m_rg->getVarByPRNO(tgtprno);
        if (loc == nullptr) {
            loc = registerLocalVar(tgt);
        } else {
            //TODO: PR should corresponding to local memory, thus the memory
            //variable should be allocated in current region.
            ASSERT0(m_rg->getVarTab()->find(loc));
        }

        ASSERT0(loc);
        ASSERT0(tgt->getTypeSize(m_tm) <= loc->getByteSize(m_tm));
        m_cg->setMapPR2SR(tgtprno, m_cg->genVAR(loc));
    }

    convertCopyPR(tgt, src, ors, cont);
}


//Generate ORs to store to IR_PR.
void IR2OR::convertStorePR(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_stpr());
    SR * mm = m_cg->mapPR2SR(STPR_no(ir));
    if (mm != nullptr) {
        IOC tc;
        convertGeneralLoad(STPR_rhs(ir), ors, cont);
        SR * store_val = cont->get_reg(0);
        RecycORList tors(this);
        if (store_val == nullptr) {
            ASSERTN(cont->get_addr(), ("miss RHS value"));
            //ASSERTN(0,
            //        ("can not convert IR_STPR by loading from an address"));
            store_val = cont->get_addr();
            ASSERT0(mm->is_var());
            IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
            convertStoreViaAddress(cont->get_addr(), mm, 0, tors, &tc);
        } else {
            if (mm->is_reg()) {
                m_cg->buildMove(mm, store_val, tors.getList(), &tc);
            } else if (mm->is_var()) {
                IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
                convertStoreDecompose(store_val, mm, 0, tors, &tc);
            } else {
                UNREACHABLE();
            }
        }
        tors.copyDbx(ir, getDbxMgr());
        ors.move_tail(tors);
        return;
    }

    if (ir->is_any() ||
        (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
         m_cg->isGRAEnable())) {
        //Store to register.
        m_cg->setMapPR2SR(STPR_no(ir), m_cg->genReg());
    } else {
        //Store to local-variable(memory) instead of register.
        Var * v = m_rg->getVarByPRNO(STPR_no(ir));
        if (v == nullptr) {
            v = registerLocalVar(ir);
        } else {
            //TODO: PR should corresponding to local memory, thus the memory
            //variable should be allocated in current region.
            ASSERT0(m_rg->getVarTab()->find(v));
        }
        ASSERT0(v);
        //Size of PR's Var is meaningless here.
        //ASSERT0(ir->getTypeSize(m_tm) == v->getByteSize(m_tm));
        m_cg->setMapPR2SR(STPR_no(ir), m_cg->genVAR(v));
    }

    convertStorePR(ir, ors, cont);
}


//Register local variable that will be allocated in memory.
Var * IR2OR::registerLocalVar(IR const* pr)
{
    ASSERT0(pr->is_pr() || pr->is_stpr() || pr->isCallStmt());
    Var * var = m_rg->genVarForPR(pr->getPrno(), pr->getType());
    //PR variable will be allocated on stack
    var->removeFlag(VAR_IS_UNALLOCABLE);
    return var;
}


void IR2OR::convertStoreVar(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_st());
    RecycORList tors(this);
    //Analyze memory-address expression.
    convertGeneralLoad(ST_rhs(ir), ors, cont);
    SR * store_val = cont->get_reg(0);
    ASSERT0(store_val != nullptr);
    IOC tc;
    IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
    ASSERT0(tc.getMemByteSize() > 0);
    convertStoreDecompose(store_val, m_cg->genVAR(ST_idinfo(ir)),
                          ST_ofst(ir), tors, &tc);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


//Process unary operation.
void IR2OR::convertUnaryOp(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERTN(ir->isUnaryOp() && UNA_opnd(ir), ("missing operand"));

    //Operand
    IOC tmp;
    convert(UNA_opnd(ir), ors, &tmp);
    SR * opnd = tmp.get_reg(0);
    ASSERT0(opnd && opnd->is_reg());

    //Result
    SR * res = m_cg->genReg((UINT)ir->getTypeSize(m_tm));

    //Choose an or-type.

    //Result's type-size might be not same as opnd. e.g: a < b,
    //result type is bool, opnd type is INT.
    OR_CODE orty = m_cg->mapIRCode2ORCode(ir->getCode(),
        UNA_opnd(ir)->getTypeSize(m_tm), UNA_opnd(ir)->getType(),
        opnd, nullptr, ir->is_signed());
    ASSERTN(orty != OR_UNDEF, ("mapIRCode2ORCode() should be overloaded"));
    UINT resnum = res->getVec() != nullptr ?
        res->getVec()->get_elem_count() : 1;
    UINT opndnum = opnd->getVec() != nullptr ?
        opnd->getVec()->get_elem_count() : 1;
    if (HAS_PREDICATE_REGISTER) { opndnum++; }
    ASSERTN(tmGetResultNum(orty) == resnum && tmGetOpndNum(orty) == opndnum,
            ("not valid target supported unary operation"));

    //Prepare result operand.
    SRList reslst;
    if (res->getVec() != nullptr) {
        reslst.appendTailFromSRVec(*res->getVec());
    } else {
        reslst.append_tail(res);
    }

    //Prepare source operand.
    SRList opndlst;
    if (HAS_PREDICATE_REGISTER) {
        //Always set the predicate register at the first position in
        //source operand list.
        opndlst.append_tail(m_cg->getTruePred());
    }
    if (opnd->getVec() != nullptr) {
        opndlst.appendTailFromSRVec(*opnd->getVec());
    } else {
        opndlst.append_tail(opnd);
    }
    OR * o = m_cg->buildOR(orty, reslst, opndlst);
    copyDbx(o, ir, getDbxMgr());

    //Set result SR.
    ASSERT0(cont != nullptr);
    cont->set_reg(0, res);
    ors.append_tail(o);
}


void IR2OR::convertBitAnd(IR const* ir, OUT RecycORList & ors,
                          MOD IOC * cont)
{
    ASSERTN(ir->is_band(), ("illegal ir"));
    if (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE) {
        //The common implementation should be able to handle the case.
        convertBinaryOp(ir, ors, cont);
        return;
    }
    UINT bytesize = ir->getTypeSize(m_tm);
    ASSERTN(bytesize % GENERAL_REGISTER_SIZE == 0, ("Target Dependent Code"));

    //Operand0
    IOC tmp;
    convert(BIN_opnd0(ir), ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg() &&
            opnd0->getByteSize() == bytesize);
    ASSERT0(opnd0->getVec());

    //Operand1
    tmp.clean();
    convert(BIN_opnd1(ir), ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg() &&
            opnd1->getByteSize() == bytesize);
    ASSERT0(opnd1->getVec());

    //Choose an OR-type.
    ASSERTN(!BIN_opnd0(ir)->is_any() && !BIN_opnd1(ir)->is_any(),
            ("data type of operand of '%s' can not be ANY", IRNAME(ir)));
    ASSERTN(BIN_opnd0(ir)->getTypeSize(m_tm) ==
            BIN_opnd1(ir)->getTypeSize(m_tm), ("must be same bitsize"));

    //Query OR code for general register operation.
    OR_CODE orty = m_cg->mapIRCode2ORCode(IR_BAND, GENERAL_REGISTER_SIZE,
        nullptr, opnd0, opnd1, false);
    ASSERTN(orty != OR_UNDEF, ("Target Dependent Code"));
    RecycORList tors(this);
    m_cg->buildMultiBinaryORByRegSize(orty, opnd0, opnd1,
                                      tors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


//Process binary operation.
void IR2OR::convertBinaryOp(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERTN(BIN_opnd0(ir) && BIN_opnd1(ir), ("missing operand"));
    //Operand0
    IOC tmp;
    convert(BIN_opnd0(ir), ors, &tmp);
    SR * opnd0 = tmp.get_reg(0);
    ASSERT0(opnd0 != nullptr && opnd0->is_reg());

    //Operand1
    tmp.clean();
    convert(BIN_opnd1(ir), ors, &tmp);
    SR * opnd1 = tmp.get_reg(0);
    ASSERT0(opnd1 != nullptr && opnd1->is_reg());

    //Choose an OR-type.
    ASSERTN(!BIN_opnd0(ir)->is_any() && !BIN_opnd1(ir)->is_any(),
            ("data type of operand of '%s' can not be ANY", IRNAME(ir)));
    ASSERTN(BIN_opnd0(ir)->getTypeSize(m_tm) ==
            BIN_opnd1(ir)->getTypeSize(m_tm), ("must be same bitsize"));
    UINT orsize = BIN_opnd0(ir)->getTypeSize(m_tm);

    //Result
    SR * res = m_cg->genReg(orsize);

    //Result's type-size might be not same as opnd. e,g: a < b,
    //result type is bool, opnd type is INT.
    OR_CODE orty = m_cg->mapIRCode2ORCode(ir->getCode(), orsize,
        BIN_opnd0(ir)->getType(), opnd0, opnd1, ir->is_signed());
    ASSERTN(orty != OR_UNDEF, ("mapIRCode2ORCode() should be overloaded"));
    UINT resnum = res->getVec() != nullptr ?
        res->getVec()->get_elem_count() : 1;
    UINT opndnum = opnd0->getVec() != nullptr ?
        opnd0->getVec()->get_elem_count() : 1;
    opndnum += opnd1->getVec() != nullptr ?
        opnd1->getVec()->get_elem_count() : 1;

    if (HAS_PREDICATE_REGISTER) { opndnum++; }
    ASSERTN(tmGetResultNum(orty) == resnum && tmGetOpndNum(orty) == opndnum,
            ("not valid target supported binary operation"));

    //Prepare result operand.
    SRList reslst;
    if (res->getVec() != nullptr) {
        reslst.appendTailFromSRVec(*res->getVec());
    } else {
        reslst.append_tail(res);
    }

    //Prepare source operand.
    SRList opndlst;
    if (HAS_PREDICATE_REGISTER) {
        //Always set the predicate register at the first position in
        //source operand list.
        opndlst.append_tail(m_cg->getTruePred());
    }
    if (opnd0->getVec() != nullptr) {
        opndlst.appendTailFromSRVec(*opnd0->getVec());
    } else {
        opndlst.append_tail(opnd0);
    }
    if (opnd1->getVec() != nullptr) {
        opndlst.appendTailFromSRVec(*opnd1->getVec());
    } else {
        opndlst.append_tail(opnd1);
    }
    OR * o = m_cg->buildOR(orty, reslst, opndlst);
    copyDbx(o, ir, getDbxMgr());

    //Set result SR.
    ASSERT0(cont != nullptr);
    cont->set_reg(0, res);
    ors.append_tail(o);
}


void IR2OR::flattenSRVec(IOC const* cont, Vector<SR*> * vec)
{
    ASSERT0(cont && vec);
    UINT vec_count = 0;
    for (UINT i = 0;; i++) {
        SR * sr = cont->get_reg(i);
        if (sr == nullptr) { return; }
        if (sr->is_vec()) {
            ASSERTN(SR_vec_idx(sr) == 0, ("expect first element"));
            for (UINT j = 0; j <= sr->getVec()->get_elem_count(); j++) {
                vec->set(vec_count, sr->getVec()->get(j));
                vec_count++;
            }
        } else {
            vec->set(vec_count, sr);
            vec_count++;
        }
    }
}


//This function try to pass all arguments through registers.
//Otherwise pass remaining arguments through stack memory.
//ir: the first parameter of CALL.
void IR2OR::processRealParamsThroughRegister(IR const* ir,
                                             MOD ArgDescMgr * argdescmgr,
                                             OUT RecycORList & ors,
                                             MOD IOC *)
{
    RecycORList tors(this);
    //ASSERT0(tmGetRegSetOfArgument() &&
    //        tmGetRegSetOfArgument()->get_elem_count() != 0);
    for (; ir != nullptr; ir = ir->get_next()) {
        UINT irsize = ir->getTypeSize(m_tm);
        //Generate load operations.
        IOC tc;
        IOC_mem_byte_size(&tc) = irsize;
        convertGeneralLoad(ir, ors, &tc);
        SR * argval = nullptr;
        SR * argaddr = nullptr;
        if (tc.get_addr() != nullptr) {
            argaddr = tc.get_addr();
        } else {
            argval = tc.get_reg(0);
            ASSERT0(argval->getByteSize() >= irsize);
        }
        tc.clean();
        tors.clean();
        m_cg->passArg(argval, argaddr, irsize, argdescmgr,
                      tors.getList(), &tc);
        tors.copyDbx(ir, getDbxMgr());
        ors.move_tail(tors);
    }
}


//'ir': the first argument of CALL.
void IR2OR::processRealParams(IR const* ir, OUT RecycORList & ors,
                              MOD IOC * cont)
{
    if (ir == nullptr) {
        ASSERT0(cont);
        IOC_arg_size(cont) = 0;
        return;
    }

    ASSERTN(PUSH_PARAM_FROM_RIGHT_TO_LEFT, ("Not yet support"));
    //Find the most rightside argument in order to coincide with
    //accessing order of the calling convention of stack varaible.
    //e.g:f(a, b, c)
    //    {
    //      int i;
    //    }
    //        stack layout:
    //        -----
    //        | c |
    //        -----
    //        | b |
    //        -----
    //        | a |
    //        -----
    //        | i |
    //        ----- <-- stack top, sp register
    //
    //    g()
    //    {
    //        int w;
    //        f(i, j, k);
    //    }
    //
    //    There are 2 accessing methods, their stack layout are:
    //        1.
    //            -----
    //            | w |
    //            -----
    //            | k |
    //            -----
    //            | j |
    //            -----
    //            | i |
    //            -----  <-sp, stack bottom of g()
    //        2.
    //            -----
    //            | w |
    //            -----  <-sp, stack bottom of g()
    //            | k |
    //            -----
    //            | j |
    //            -----
    //            | i |
    //            -----
    if (m_cg->isUseFP()) {
        //FP will record the frame start address.
        //Caller is not responsible for pulling the SP down,
        //and callee should do it.
        //TBD: Do we have to consider real-arguments when FP enabled.
        //ASSERTN(0, ("TBD"));
    }

    ArgDescMgr argdescmgr;
    processRealParamsThroughRegister(ir, &argdescmgr, ors, cont);

    //Record the size as return-value.
    IOC_arg_size(cont) = argdescmgr.getArgSectionSize();
}


SR * IR2OR::saveToNewRegIfAssignedPhyReg(
    SR * src, IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(src && src->is_reg());
    DbxMgr * dbx_mgr = getDbxMgr();
    ASSERT0(dbx_mgr);
    if (src->is_vec()) {
        ASSERTN(src->getVecIdx() == 0, ("expect first element"));
        bool find_phy_reg = false;
        for (UINT j = 0; j <= src->getVec()->get_elem_count(); j++) {
            if (src->getPhyReg() != REG_UNDEF) {
                find_phy_reg = true;
                break;
            }
        }
        if (!find_phy_reg) { return src; }
        List<SR*> regvlst;
        IOC tmp(*cont);
        SRVec * srcvec = src->getVec();
        ASSERT0(srcvec);
        RecycORList tors(this);
        for (UINT i = 0; i < srcvec->get_elem_count(); i++) {
            tmp.clean();
            SR * srcv = srcvec->get(i);
            SR * newv = getCG()->genReg();
            regvlst.append_tail(newv);
            getCG()->buildMove(newv, srcv, tors.getList(), &tmp);
            if (i == srcvec->get_elem_count() - 1) {
                //Only copy the last context information to avoid redundant
                //copies.
                cont->copy_bottomup(tmp);
            }
        }
        SR * newsrc = m_cg->getSRVecMgr()->genSRVec(regvlst);
        tors.copyDbx(ir, dbx_mgr);
        ors.move_tail(tors);
        return newsrc;
    }
    if (src->getPhyReg() == REG_UNDEF) { return src; }
    IOC tmp(*cont);
    SR * newsrc = src;
    RecycORList tors(this);
    tmp.clean();
    newsrc = getCG()->genReg();
    getCG()->buildMove(newsrc, src, tors.getList(), &tmp);
    tors.copyDbx(ir, dbx_mgr);
    ors.move_tail(tors);
    cont->copy_bottomup(tmp);
    return newsrc;
}


void IR2OR::convertASR(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_asr());
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);
    ASSERTN(opnd0->is_signed(), ("shift should be arithmetical"));
    cont->clean_bottomup();
    convertGeneralLoad(opnd0, ors, cont);
    SR * sr1 = cont->get_reg(0);
    ASSERT0(sr1 && sr1->is_reg());

    SR * sh_ofst;
    if (opnd1->is_const()) {
        ASSERT0(opnd1->is_int());
        if (CONST_int_val(opnd1) == 0) {
            //CASE:Skip x>>0, return the latest register in context as result.
            ASSERT0(cont->get_reg(0) && cont->get_reg(0)->is_reg());
            return;
        }
        sh_ofst = m_cg->genIntImm(CONST_int_val(opnd1), false);
    } else {
        cont->clean_bottomup();
        convertGeneralLoad(opnd1, ors, cont);
        sh_ofst = cont->get_reg(0);
    }

    RecycORList tors(this);
    cont->clean_bottomup();
    m_cg->buildShiftRight(sr1, opnd0->getTypeSize(m_tm),
                          sh_ofst, opnd0->is_signed(), tors.getList(), cont);
    ASSERT0(cont->get_reg(0));
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertLSR(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_lsr());
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);
    IOC tc;
    convertGeneralLoad(opnd0, ors, &tc);
    SR * sr1 = tc.get_reg(0);

    SR * sh_ofst;
    if (opnd1->is_const() && opnd1->is_int()) {
        if (CONST_int_val(opnd1) == 0) {
            //CASE:Skip x>>0, return the latest register in context as result.
            ASSERT0(sr1->is_reg());
            cont->set_reg(RESULT_REGISTER_INDEX, sr1);
            return;
        }
        sh_ofst = m_cg->genIntImm(CONST_int_val(opnd1), false);
    } else {
        tc.clean();
        convertGeneralLoad(opnd1, ors, &tc);
        sh_ofst = tc.get_reg(0);
    }

    RecycORList tors(this);
    m_cg->buildShiftRight(sr1, opnd0->getTypeSize(m_tm),
                          sh_ofst, opnd0->is_signed(), tors.getList(), cont);
    ASSERT0(cont->get_reg(0));
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertLSL(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_lsl());
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);
    IOC tc;
    convertGeneralLoad(opnd0, ors, &tc);
    SR * sr1 = tc.get_reg(0);

    SR * sh_ofst;
    if (opnd1->is_const() && opnd1->is_int()) {
        ASSERT0(opnd1->is_int());
        if (CONST_int_val(opnd1) == 0) {
            //CASE:Skip x<<0, return the latest register in context as result.
            ASSERT0(sr1->is_reg());
            cont->set_reg(RESULT_REGISTER_INDEX, sr1);
            return;
        }
        sh_ofst = m_cg->genIntImm(CONST_int_val(opnd1), false);
    } else {
        tc.clean();
        convertGeneralLoad(opnd1, ors, &tc);
        sh_ofst = tc.get_reg(0);
    }

    RecycORList tors(this);
    m_cg->buildShiftLeft(sr1, opnd0->getTypeSize(m_tm), sh_ofst,
                         tors.getList(), cont);
    ASSERT0(cont->get_reg(0));
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertCvt(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_cvt() && CVT_exp(ir));
    ASSERTN(!ir->is_any() && !CVT_exp(ir)->is_any(), ("TODO"));
    RecycORList tors(this);
    convertGeneralLoad(CVT_exp(ir), tors, cont);
    m_cg->buildTypeCvt(ir, CVT_exp(ir), tors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertGoto(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir->is_goto());

    //Target label
    SR * tgt_lab = m_cg->genLabel(GOTO_lab(ir));
    RecycORList tors(this);
    m_cg->buildUncondBr(tgt_lab, tors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertTruebr(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_truebr());
    IR * br_det = BR_det(ir);
    ASSERT0(br_det->is_lt() || br_det->is_le() || br_det->is_gt() ||
            br_det->is_ge() || br_det->is_eq() || br_det->is_ne());
    convertRelationOp(br_det, ors, cont);
    RecycORList tors(this);
    m_cg->buildCondBr(m_cg->genLabel(BR_lab(ir)), ors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertLabel(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    m_cg->buildLabel(LAB_lab(ir), ors.getList(), cont);
}


void IR2OR::convertFalsebr(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir && ir->is_falsebr());
    IR * newir = m_rg->dupIRTree(ir);
    IR * br_det = BR_det(newir);
    ASSERT0(br_det->is_lt() || br_det->is_le() || br_det->is_gt() ||
            br_det->is_ge() || br_det->is_eq() || br_det->is_ne());
    IR_code(br_det) = IR::invertIRCode(br_det->getCode());
    IR_code(newir) = IR_TRUEBR;
    convertTruebr(newir, ors, cont);
    m_rg->freeIRTree(newir);
}


//Generate compare operations and return the comparation result registers.
//The output registers in IOC are ResultSR,
//TruePredicatedSR, FalsePredicatedSR.
//The ResultSR record the boolean value of comparison of relation operation.
//    e.g:
//        a - 1 > b + 2
//    =>
//        sr0 = a - 1
//        sr1 = b + 2
//        sr2 <- cmp.gt sr0, sr1
//        return sr2
//   e.g2:
//is_invert: true if generated inverted operation.
//  e.g: given a <= b, generate !(a > b)
void IR2OR::convertRelationOp(IR const* ir, OUT RecycORList & ors,
                              MOD IOC * cont)
{
    ASSERT0(ir->is_relation());
    IR const* opnd0 = BIN_opnd0(ir);
    IR const* opnd1 = BIN_opnd1(ir);
    ASSERTN(opnd0->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE, ("TODO"));
    ASSERTN(opnd1->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE, ("TODO"));

    RecycORList tors(this);

    IOC tmp;
    //Operands 0
    convertGeneralLoad(opnd0, tors, &tmp);
    SR * sr0 = tmp.get_reg(0);

    //Operands 1
    tmp.clean();
    convertGeneralLoad(opnd1, tors, &tmp);
    SR * sr1 = tmp.get_reg(0);

    //Compare operation
    UINT maxbytesize = MAX(opnd0->getTypeSize(m_tm), opnd1->getTypeSize(m_tm));

    bool is_signed = opnd0->is_signed() || opnd0->is_signed() ? true : false;
    OR_CODE t = m_cg->mapIRCode2ORCode(ir->getCode(), maxbytesize,
        opnd0->getType(), sr0, sr1, is_signed);

    //Generate compare operations.
    tmp.clean();
    bool is_truebr = true;
    if (t == OR_UNDEF) {
        //Query the converse or-type.
        IR_CODE rev_t = IR::invertIRCode(ir->getCode());
        t = m_cg->mapIRCode2ORCode(rev_t, maxbytesize,
            opnd0->getType(), sr0, sr1, is_signed);
        ASSERTN(t != OR_UNDEF, ("miss comparsion or-type for branch"));
        is_truebr = false;
    }

    m_cg->buildCompare(t, is_truebr, sr0, sr1, tors.getList(), cont);
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);

    cont->set_orcode(t); //record the orcode.
}


void IR2OR::convertBBLabel(IRBB const* bb, OUT RecycORList & ors,
                           MOD IOC * cont)
{
    ASSERT0(bb);
    xcom::C<LabelInfo const*> * ct;
    for (bb->getLabelListConst().get_head(&ct);
         ct != bb->getLabelListConst().end();
         ct = bb->getLabelListConst().get_next(ct)) {
        m_cg->buildLabel(ct->val(), ors.getList(), cont);
    }
}


void IR2OR::convertIntrinsic(IR const* ir, OUT RecycORList & ors,
                             MOD IOC * cont)
{
    ASSERT0(ir->is_call()); //call may be have 'INTRINSIC' flag set.
    Sym const* name = ((CCall*)ir)->getIdinfo()->get_name();
    UINT code = m_cgmgr->getIntrinMgr()->getCode(name);
    ASSERTN(code != INTRIN_UNDEF, ("Target Dependent Intrinsic Code"));
    RecycORList tors(this);
    IOC tmp;
    switch (code) {
    case INTRIN_ALLOCA: {
        IR const* addend_ir = CALL_arg_list(ir);
        ASSERT0(addend_ir);
        convert(addend_ir, tors, &tmp);
        SR * opnd0 = tmp.get_reg(0);
        ASSERT0(opnd0 && opnd0->is_reg());
        tmp.clean();
        m_cg->buildAlloca(tors.getList(), opnd0, &tmp);

        //The result register is SP.
        SR * sp = tmp.get_reg(0);
        ASSERT0(sp && sp->is_sp());

        //Generate copy operation from SP to PR.
        tmp.clean();
        convertCopyPR(ir, sp, tors, &tmp);
        break;
    }
    default: ASSERTN(0, ("Target Dependent Intrinsic Code"));
    }
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
}


void IR2OR::convertCall(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERTN(ir->isCallStmt(), ("illegal ir"));
    if (m_cgmgr->isIntrinsic(ir)) {
        convertIntrinsic(ir, ors, cont);
        return;
    }

    processRealParams(CALL_arg_list(ir), ors, cont);

    //Collect the maximum arguments size during code generation.
    //And revise spadjust operation afterwards.
    m_cg->updateMaxCalleeArgSize(IOC_arg_size(cont));

    if (IOC_arg_size(cont) > 0) {
        //DO not adjust SP here for arguments, callee will
        //do this job.
    }

    RecycORList tors(this);
    UINT retv_sz = GENERAL_REGISTER_SIZE;

    if (ir->hasReturnValue() && !ir->is_any()) {
        retv_sz = ir->getTypeSize(m_tm);
    }
    //Note if the result-type of CALL is ANY type, regard it
    //is as long as register.

    if (ir->is_call()) {
        getCG()->buildCall(CALL_idinfo(ir), retv_sz, tors.getList(), cont);
    } else {
        ASSERT0(ir->is_icall());
        IOC tc;
        convert(ICALL_callee(ir), tors, &tc);
        SR * addr = tc.get_reg(0);
        ASSERT0(addr);
        getCG()->buildICall(addr, retv_sz, tors.getList(), cont);
    }
    tors.copyDbx(ir, getDbxMgr());
    ors.move_tail(tors);
    convertReturnValue(ir, ors, cont);
}


void IR2OR::convertRegion(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    if (!g_is_generate_code_for_inner_region) { return; }
    Region * rg = REGION_ru(ir);
    ASSERT0(rg);
    if (rg->is_blackbox()) { return; }

    CGMgr * current_cgmgr = getCG()->getCGMgr();
    RegionMgr * rm = current_cgmgr->getRegionMgr();
    CGMgr * newcgmgr = xgen::allocCGMgr(rm);
    newcgmgr->setAsmFileHandler(current_cgmgr->getAsmFileHandler());

    //TODO: so far, xgen does not support generate OR for high level control
    //flow IR, user have to simplify CFS into lower level branch IR.
    OptCtx oc(rg);
    SimpCtx simp(&oc);
    simp.setSimpCFS();
    rg->initPassMgr();
    rg->initDbxMgr();
    rg->initIRMgr();
    rg->initIRBBMgr();
    rg->getIRSimp()->simplifyIRList(&simp);
    //Assign Var for PR that generated by simplification.
    rg->getMDMgr()->assignMD(true, false);
    newcgmgr->generate(rg);
    delete newcgmgr;
}


void IR2OR::convert(IR const* ir, OUT RecycORList & ors, MOD IOC * cont)
{
    ASSERT0(ir && ir->verify(m_rg));
    RecycORList tors(this);
    switch (ir->getCode()) {
    case IR_CONST:
        convertLoadConst(ir, tors, cont);
        ASSERT0(cont->get_reg(0) || cont->get_addr());
        break;
    case IR_ID:
        //TBD:Do we need to generate code for IR_ID?
        //load ID into register.
        convertId(ir, tors, cont);
        break;
    case IR_LD:
        convertLoadVar(ir, tors, cont);
        ASSERT0(cont->get_reg(0) || cont->get_addr());
        break;
    case IR_ST:
        convertStoreVar(ir, tors, cont);
        break;
    case IR_STPR:
        convertStorePR(ir, tors, cont);
        break;
    case IR_ILD:
        convertILoad(ir, tors, cont);
        ASSERT0(cont->get_reg(0) || cont->get_addr());
        break;
    case IR_IST:
        convertIStore(ir, tors, cont);
        break;
    case IR_LDA:   // &a get address of 'a'
        convertLda(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_CALL:
        convertCall(ir, tors, cont);
        break;
    case IR_ICALL:
        convertICall(ir, tors, cont);
        break;
    case IR_ASR:
        convertASR(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LSR:
        convertLSR(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LSL:
        convertLSL(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_CVT: //type convertion
        convertCvt(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    SWITCH_CASE_READ_PR:
        convertGeneralLoad(ir, tors, cont);
        break;
    case IR_ADD:
        convertAdd(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_SUB:
        convertSub(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_DIV:
        convertDiv(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_POW:
        convertPow(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_NROOT:
        convertNRoot(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LOG:
        convertLog(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_EXPONENT:
        convertExponent(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    SWITCH_CASE_UNA_TRIGONOMETRIC:
        convertTrigonometric(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_MUL:
        convertMul(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_REM:
        convertRem(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_MOD:
        convertMod(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LAND: //logical and      &&
        convertLogicalAnd(ir, tors, cont);
        break;
    case IR_LOR: //logical or        ||
        convertLogicalOr(ir, tors, cont);
        break;
    case IR_BAND: //inclusive and &
        convertBitAnd(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_BOR: //inclusive or  |
        convertBitOr(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_XOR: //exclusive or
        convertXor(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_BNOT:  //bitwise not
        convertBitNot(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LNOT:  //logical not
        convertLogicalNot(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_NEG:  // -123
        convertNeg(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_ALLOCA:  // alloca(n)
        convertAlloca(ir, tors, cont);
        ASSERT0(cont->get_reg(0) != nullptr);
        break;
    case IR_LT:
        convertLT(ir, tors, cont);
        break;
    case IR_LE:
        convertLE(ir, tors, cont);
        break;
    case IR_GT:
        convertGT(ir, tors, cont);
        break;
    case IR_GE:
        convertGE(ir, tors, cont);
        break;
    case IR_EQ: //==
        convertEQ(ir, tors, cont);
        break;
    case IR_NE: //!=
        convertNE(ir, tors, cont);
        break;
    case IR_GOTO:
        convertGoto(ir, tors, cont);
        break;
    case IR_IGOTO:
        convertIgoto(ir, tors, cont);
        break;
    case IR_TRUEBR:
        convertTruebr(ir, tors, cont);
        break;
    case IR_FALSEBR:
        convertFalsebr(ir, tors, cont);
        break;
    case IR_RETURN:
        convertReturn(ir, tors, cont);
        break;
    case IR_SELECT: //formulized determinate_exp?exp:exp
        convertSelect(ir, tors, cont);
        break;
    case IR_REGION:
        convertRegion(ir, tors, cont);
        break;
    case IR_SETELEM:
        convertSetElem(ir, tors, cont);
        break;
    case IR_GETELEM:
        convertGetElem(ir, tors, cont);
        break;
    case IR_LABEL:
        convertLabel(ir, tors, cont);
        break;
    default: convertExtStmt(ir, tors, cont);
    }
    ors.move_tail(tors);
}


void IR2OR::convertIRListToORList(OUT RecycORList & or_list)
{
    IOC cont;
    for (IR * ir = m_rg->getIRList(); ir != nullptr; ir = ir->get_next()) {
        cont.clean();
        convert(ir, or_list, &cont);
    }
}


void IR2OR::convertIRBBListToORList(OUT RecycORList & or_list)
{
    BBList * ir_bb_list = m_rg->getBBList();
    ASSERT0(ir_bb_list);
    IOC cont;
    for (IRBB const* bb = ir_bb_list->get_head();
         bb != nullptr; bb = ir_bb_list->get_next()) {
        convertBBLabel(bb, or_list, &cont);
        IRListIter ct;
        for (BB_irlist(bb).get_head(&ct);
             ct != BB_irlist(bb).end(); ct = BB_irlist(bb).get_next(ct)) {
            cont.clean();
            convert(ct->val(), or_list, &cont);
        }
    }
}


//Translate IR in IRBB to a list of OR.
void IR2OR::convertToORList(OUT RecycORList & or_list)
{
    START_TIMER(t, "Convert IR to OR");
    ASSERT0(m_rg);
    IR * ir_list = m_rg->getIRList();
    if (ir_list != nullptr) {
        ASSERT0(m_rg->getBBList() == nullptr ||
                m_rg->getBBList()->get_elem_count() == 0);
        convertIRListToORList(or_list);
        return;
    }
    convertIRBBListToORList(or_list);
    END_TIMER(t, "Convert IR to OR");
}

} //namespace xgen
