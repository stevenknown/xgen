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
@*/
#include "../xgen/xgeninc.h"
#include "ir2or.h"

namespace xgen {

IR2OR::IR2OR(CG * cg)
{
    ASSERT0(cg);
    m_cg = cg;
    m_cgmgr = cg->getCGMgr();
    m_rg = cg->getRegion();
    ASSERT0(m_rg);
    m_tm = m_rg->getTypeMgr();
}


//Store value that given by address 'src_addr' to 'tgtvar'.
//ofst: offset from base of tgtvar.
void IR2OR::convertStoreViaAddress(IN SR * src_addr,
                                   IN SR * tgtvar,
                                   HOST_INT ofst,
                                   OUT RecycORList & ors,
                                   IN IOC * cont)
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
    m_cg->buildMemcpy(tgt_addr, src_addr, IOC_mem_byte_size(cont),
                      ors.getList(), cont);
}


//Decompose 'mem_addr_sr' into the form 'base+offset'.
//'tgtvar': symbol describes memory location
void IR2OR::convertStoreDecompose(IN SR * src,
                                  IN SR * tgtvar,
                                  HOST_INT ofst,
                                  OUT RecycORList & ors,
                                  IN IOC * cont)
{
    ASSERT0(src && tgtvar && tgtvar->is_var() && SR_var(tgtvar));

    //ONLY register operand is permited to process.
    if (!src->is_reg()) {
        if (src->is_int_imm()) {
            SR * t = m_cg->genReg();
            m_cg->buildMove(t, src, ors.getList(), cont);
            src = t;
        } else {
            ASSERTN(0, ("Unsupport"));
        }
    }
    m_cg->buildStore(src, tgtvar, m_cg->genIntImm(ofst, true),
                     ors.getList(), cont);
}


void IR2OR::convertLoadConst(IR const* ir, OUT RecycORList & ors,
                             IN IOC * cont)
{
    ASSERT0(ir->is_const());
    ASSERTN(ir->getTypeSize(m_tm) <= 2 * GENERAL_REGISTER_SIZE,
            ("Target Dependent Code"));
    RecycORList tors(this);
    SR * load_val = nullptr;
    SR * load_val2 = nullptr;
    if (ir->is_int()) {
        load_val = m_cg->genReg();

        HOST_INT v = CONST_int_val(ir);
        if (ir->getTypeSize(m_tm) == 2 * GENERAL_REGISTER_SIZE) {
            HOST_UINT v2 = (HOST_UINT)(((ULONGLONG)v) <<
                                       WORD_LENGTH_OF_TARGET_MACHINE);
            v = v2 >> WORD_LENGTH_OF_TARGET_MACHINE;
        }

        //Load low part.
        m_cg->buildMove(load_val, m_cg->genIntImm(v, ir->is_signed()),
                        tors.getList(), cont);

        if (ir->getTypeSize(m_tm) == 2 * GENERAL_REGISTER_SIZE) {
            //Load high part.
            load_val2 = m_cg->genReg();

            HOST_INT v2 = 0;
            if (ir->is_signed()) {
                v2 = (HOST_INT)(((LONGLONG)CONST_int_val(ir)) >>
                                WORD_LENGTH_OF_TARGET_MACHINE);
            } else {
                v2 = (HOST_INT)(((ULONGLONG)(HOST_UINT)CONST_int_val(ir)) >>
                                WORD_LENGTH_OF_TARGET_MACHINE);
            }

            m_cg->buildMove(load_val2, m_cg->genIntImm(v2, ir->is_signed()),
                            tors.getList(), cont);
            m_cg->getSRVecMgr()->genSRVec(2, load_val, load_val2);
        }
    } else if (ir->is_fp()) {
        load_val = m_cg->genReg();
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

            m_cg->buildMove(load_val,
                m_cg->genIntImm((HOST_INT)GET_LOW_32BIT(*pb), false),
                tors.getList(), cont);

            load_val2 = m_cg->genReg();
            m_cg->buildMove(load_val2,
                m_cg->genIntImm((HOST_INT)GET_HIGH_32BIT(*pb), false),
                tors.getList(), cont);

            m_cg->getSRVecMgr()->genSRVec(2, load_val, load_val2);
        }
    } else if (ir->is_bool()) {
        load_val = m_cg->genReg();
        m_cg->buildMove(load_val, m_cg->genIntImm(CONST_int_val(ir), false),
                        tors.getList(), cont);
    } else if (ir->is_str()) {
        //Support loading const to lda(string-variable)
        Region * rg = m_cg->getRegion();
        IR * lda = rg->buildLdaString(nullptr, CONST_str_val(ir));
        //Record string-variable into program-region to facilitate asm-print.
        //This will storage string content at .data section.
        ASSERT0(lda->is_lda() && LDA_idinfo(lda)->is_global() &&
                rg->getTopRegion() != nullptr &&
                rg->getTopRegion()->is_program());
        ASSERT0(rg->getTopRegion()->getVarTab()->find(LDA_idinfo(lda)));
        //rg->getTopRegion()->addToVarTab(LDA_idinfo(lda));
        convertLda(lda, tors, cont);
        load_val = cont->get_reg(0);
        ASSERT0(load_val && load_val->is_reg());
        cont->clean_bottomup();
    } else {
        ASSERTN(0, ("unsupport immediate value DATA_TYPE:%d", ir->getDType()));
    }

    tors.copyDbx(ir);
    ors.append_tail(tors);
    cont->set_reg(0, load_val);
    //if (load_val2 != nullptr) {
    //    cont->set_reg(1, load_val2);
    //}
}


//Generate ORs to load value into a symbol register.
void IR2OR::convertGeneralLoad(IR const* ir, OUT RecycORList & ors,
                               IN IOC * cont)
{
    ASSERT0(cont != nullptr);
    switch (ir->getCode()) {
    SR * res;
    case IR_CONST:
        convertLoadConst(ir, ors, cont);
        break;
    case IR_PR:
        cont->clean_bottomup();
        convertGeneralLoadPR(ir, ors, cont);
        ASSERT0((cont->get_reg(0) && cont->get_reg(0)->is_reg()) ||
                cont->get_addr());
        break;
    default:
        cont->clean_bottomup();
        convert(ir, ors, cont);
        if (cont->get_reg(0) == nullptr) {
            ASSERT0(cont->get_addr());
            break;
        }

        res = cont->get_reg(0);
        ASSERT0(res && res->getByteSize() >= ir->getTypeSize(m_tm));
        if (!res->is_reg()) {
            SRVec * srvec = SR_vec(res);
            RecycORList tors(this);
            if (srvec == nullptr) {
                SR * r = m_cg->genReg();
                m_cg->buildMove(r, res, tors.getList(), cont);
                cont->set_reg(0, r);
            } else {
                List<SR*> regvlst;
                IOC tmp;
                for (UINT i = 0; i < srvec->get_elem_count(); i++) {
                    SR * r = m_cg->genReg();
                    tmp.clean();
                    m_cg->buildMove(r, srvec->get(i), tors.getList(), &tmp);
                    cont->set_reg(i, r);
                }
                m_cg->getSRVecMgr()->genSRVec(regvlst);
            }
            tors.copyDbx(ir);
            ors.append_tail(tors);
        }

        ASSERT0(cont->get_reg(0) && cont->get_reg(0)->is_reg());
    }
}


void IR2OR::convertIStore(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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
    tors.copyDbx(IST_base(ir));
    ors.append_tail(tors);

    //Load RHS into registers or get the address of memory block.
    cont->clean_bottomup();
    tors.clean();
    convertGeneralLoad(IST_rhs(ir), tors, cont);
    ASSERT0(cont->get_reg(0) || cont->get_addr());
    tors.copyDbx(IST_rhs(ir));
    ors.append_tail(tors);

    tors.clean();
    if (cont->get_reg(0) != nullptr) {
        //Generate store.
        //Note that one should use builtStore to generate STORE with offset
        //instead of generating code that adding offset to base if
        //cont->get_reg(0) is not nullptr.
        ASSERT0(cont->get_addr() == nullptr);
        ASSERT0(ir->getTypeSize(m_tm) > 0);

        IOC tmp_cont;
        IOC_mem_byte_size(&tmp_cont) = ir->getTypeSize(m_tm);
        m_cg->buildStore(cont->get_reg(0), addr,
                         m_cg->genIntImm((HOST_INT)IST_ofst(ir), true),
                         tors.getList(), &tmp_cont);
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

    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertILoad(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_ild());
    IOC tmp_cont;

    //Load mem_address into a register
    ASSERT0(ILD_base(ir)->is_ptr());
    convertGeneralLoad(ILD_base(ir), ors, &tmp_cont);

    SR * addr = tmp_cont.get_reg(0);
    ASSERTN(addr && addr->is_reg(),
            ("address should be recorded in a register"));

    //TODO: Use symbol address directly to diminish the number of OR.

    //The value that will be returned.
    tmp_cont.clean();
    IOC_mem_byte_size(&tmp_cont) = ir->getTypeSize(m_tm);

    RecycORList tors(this);
    m_cg->buildGeneralLoad(addr, ILD_ofst(ir), tors.getList(), &tmp_cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);

    ASSERT0(cont);
    cont->clean_regvec();
    SR * load_val = tmp_cont.get_reg(0);
    if (load_val != nullptr) {
        //Set result reg.
        cont->set_reg(0, load_val);
        return;
    }

    ASSERT0(tmp_cont.get_addr());
    cont->set_addr(tmp_cont.get_addr());
}


void IR2OR::convertLoadVar(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->is_ld() && cont);
    ASSERT0(LD_idinfo(ir));
    RecycORList tors(this);
    IOC_mem_byte_size(cont) = ir->getTypeSize(m_tm);
    cont->clean_bottomup();
    m_cg->buildGeneralLoad(m_cg->genVAR(LD_idinfo(ir)),
                           LD_ofst(ir), tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


//TODO: return a symbol SR.
//Load symbol's value into register.
//'ir': type must be IR_ID.
void IR2OR::convertId(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->is_id() && ir->getTypeSize(m_tm) > 0);
    ASSERT0(cont);
    ASSERT0(ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE);

    RecycORList tmp_ors(this);
    SR * load_val = m_cg->genReg();
    IOC_mem_byte_size(cont) = ir->getTypeSize(m_tm);
    m_cg->buildLoad(load_val, ID_info(ir), 0, tmp_ors.getList(), cont);
    tmp_ors.copyDbx(ir);
    ors.append_tail(tmp_ors);

    //Set result SR.
    cont->set_reg(0, load_val);
}


void IR2OR::convertGeneralLoadPR(IR const* ir, OUT RecycORList & ors,
                                 IN IOC * cont)
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
            IOC tc;
            RecycORList tors(this);
            IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
            m_cg->buildGeneralLoad(mm, 0, tors.getList(), &tc);
            tors.copyDbx(ir);
            ors.append_tail(tors);
            cont->clean_bottomup();
            cont->copy_result(tc);
            return;
        }

        UNREACHABLE();
        return;
    }

    if (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
        m_cg->isGRAEnable()) {
        m_cg->setMapPR2SR(PR_no(ir), m_cg->genReg());
    } else {
        Var * v = m_rg->mapPR2Var(PR_no(ir));
        ASSERT0(v != nullptr);
        //TO BE DETERMINED: Does ir size have to equal to variable's size?
        //ASSERT0(ir->getTypeSize(m_tm) == v->getByteSize(m_tm));
        m_cg->setMapPR2SR(PR_no(ir), m_cg->genVAR(v));
    }

    convertGeneralLoadPR(ir, ors, cont);
}


//Copy 'src' to 'tgt's PR'.
//'tgt': must be PR.
//'src': register or imm.
void IR2OR::convertCopyPR(IR const* tgt,
                          IN SR * src,
                          OUT RecycORList & ors,
                          IN IOC * cont)
{
    ASSERT0(tgt->isReadPR() || tgt->isWritePR() || tgt->isCallStmt());
    UINT tgtprno = tgt->getPrno();
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
        tors.copyDbx(tgt);
        ors.append_tail(tors);
        return;
    }

    if (tgt->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
        m_cg->isGRAEnable()) {
        //Store to register.
        m_cg->setMapPR2SR(tgtprno, m_cg->genReg());
    } else {
        //Note the value of GSR must be stored and loaded from
        //local variable if GRA is disabled becase the value
        //must be transferd through memory.

        //Store to local-variable(memory) instead of register.
        Var * loc = m_rg->mapPR2Var(tgtprno);
        if (loc == nullptr) {
            loc = registerLocalVar(tgt);
        } else {
            ASSERT0(m_rg->getVarTab()->find(loc));
            //m_rg->addToVarTab(loc);
        }

        ASSERT0(loc);
        ASSERT0(tgt->getTypeSize(m_tm) <= loc->getByteSize(m_tm));
        m_cg->setMapPR2SR(tgtprno, m_cg->genVAR(loc));
    }

    convertCopyPR(tgt, src, ors, cont);
}


//Generate ORs to store to IR_PR.
void IR2OR::convertStorePR(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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
            //ASSERTN(0, ("can not convert IR_STPR by loading from an address"));
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
        tors.copyDbx(ir);
        ors.append_tail(tors);
    } else {
        if (ir->getTypeSize(m_tm) <= GENERAL_REGISTER_SIZE &&
            m_cg->isGRAEnable()) {
            //Store to register.
            m_cg->setMapPR2SR(STPR_no(ir), m_cg->genReg());
        } else {
            //Store to local-variable(memory) instead of register.
            Var * v = m_rg->mapPR2Var(STPR_no(ir));
            if (v == nullptr) {
                v = registerLocalVar(ir);
            } else {
                ASSERT0(m_rg->getVarTab()->find(v));
            }
            ASSERT0(v != nullptr);
            ASSERT0(ir->getTypeSize(m_tm) == v->getByteSize(m_tm));
            m_cg->setMapPR2SR(STPR_no(ir), m_cg->genVAR(v));
        }
        convertStorePR(ir, ors, cont);
    }
}


//Register local variable that will be allocated in memory.
Var * IR2OR::registerLocalVar(IR const* pr)
{
    ASSERT0(pr->is_pr() || pr->is_stpr() || pr->isCallStmt());
    Var * var = m_rg->genVarForPR(pr->getPrno(), pr->getType());
    VAR_is_unallocable(var) = false; //PR variable will be allocated on stack
    return var;
}


void IR2OR::convertStoreVar(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_st());
    RecycORList tors(this);
    //Analyize memory-address expression.
    convertGeneralLoad(ST_rhs(ir), ors, cont);
    SR * store_val = cont->get_reg(0);
    ASSERT0(store_val != nullptr);
    IOC tc;
    IOC_mem_byte_size(&tc) = ir->getTypeSize(m_tm);
    ASSERT0(IOC_mem_byte_size(&tc) > 0);
    convertStoreDecompose(store_val, m_cg->genVAR(ST_idinfo(ir)),
                          ST_ofst(ir), tors, &tc);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


//Process unary operation.
void IR2OR::convertUnaryOp(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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
    //result type is BOOL, opnd type is INT.
    OR_TYPE orty = m_cg->mapIRType2ORType(ir->getCode(),
                                          UNA_opnd(ir)->getTypeSize(m_tm),
                                          opnd, nullptr, ir->is_signed());
    ASSERTN(orty != OR_UNDEF, ("mapIRType2ORType() should be overloaded"));

    OR * o;
    if (HAS_PREDICATE_REGISTER) {
        ASSERTN(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 2,
                ("not an unary op"));
        o = m_cg->buildOR(orty, 1, 2, res, m_cg->getTruePred(), opnd);
    } else {
        ASSERTN(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 1,
                ("not an unary op"));
        o = m_cg->buildOR(orty, 1, 2, res, opnd);
    }
    copyDbx(o, ir);

    //Set result SR.
    ASSERT0(cont != nullptr);
    cont->set_reg(0, res);
    ors.append_tail(o);
}


//Process binary operation.
void IR2OR::convertBinaryOp(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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

    //Result
    SR * res = m_cg->genReg((UINT)ir->getTypeSize(m_tm));

    //Choose an or-type.
    ASSERTN(BIN_opnd0(ir)->getTypeSize(m_tm) ==
            BIN_opnd1(ir)->getTypeSize(m_tm), ("must be same bitsize"));

    //Result's type-size might be not same as opnd. e,g: a < b,
    //result type is BOOL, opnd type is INT.
    OR_TYPE orty = m_cg->mapIRType2ORType(ir->getCode(),
                                          BIN_opnd0(ir)->getTypeSize(m_tm),
                                          opnd0, opnd1, ir->is_signed());
    ASSERTN(orty != OR_UNDEF, ("mapIRType2ORType() should be overloaded"));

    OR * o;
    if (HAS_PREDICATE_REGISTER) {
        ASSERTN(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 3,
                ("not a binary op"));
        o = m_cg->buildOR(orty, 1, 3, res, m_cg->getTruePred(), opnd0, opnd1);
    } else {
        ASSERTN(tmGetResultNum(orty) == 1 && tmGetOpndNum(orty) == 2,
                ("not a binary op"));
        o = m_cg->buildOR(orty, 1, 2, res, opnd0, opnd1);
    }
    copyDbx(o, ir);

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
            for (UINT j = 0; j <= SR_vec(sr)->get_elem_count(); j++) {
                vec->set(vec_count, SR_vec(sr)->get(j));
                vec_count++;
            }
        } else {
            vec->set(vec_count, sr);
            vec_count++;
        }
    }
}


//This function try to pass all arguments through registers.
//Otherwise pass remaingin arguments through stack memory.
//'ir': the first parameter of CALL.
void IR2OR::processRealParamsThroughRegister(IR const* ir,
                                             IN OUT ArgDescMgr * argdescmgr,
                                             OUT RecycORList & ors,
                                             IN IOC *)
{
    RecycORList tors(this);
    //ASSERT0(tmGetRegSetOfArgument() &&
    //        tmGetRegSetOfArgument()->get_elem_count() != 0);
    for (; ir != nullptr; ir = ir->get_next()) {
        UINT irsize = ir->getTypeSize(m_tm);
        //Generate load operations.
        IOC tcont;
        convertGeneralLoad(ir, ors, &tcont);

        SR * argval = nullptr;
        SR * argaddr = nullptr;
        if (tcont.get_addr() != nullptr) {
            argaddr = tcont.get_addr();
            ASSERT0(IOC_mem_byte_size(&tcont) == irsize);
        } else {
            argval = tcont.get_reg(0);
            ASSERT0(argval->getByteSize() >= irsize);
        }

        tcont.clean();
        tors.clean();
        m_cg->passArg(argval, argaddr, irsize, argdescmgr,
                      tors.getList(), &tcont);
        tors.copyDbx(ir);
        ors.append_tail(tors);
    }
}


//'ir': the first parameter of CALL.
void IR2OR::processRealParams(IR const* ir, OUT RecycORList & ors,
                              IN IOC * cont)
{
    if (ir == nullptr) {
        ASSERT0(cont);
        IOC_param_size(cont) = 0;
        return;
    }

    ASSERTN(PUSH_PARAM_FROM_RIGHT_TO_LEFT, ("Not yet support"));
    //Find the most rightside parameter in order to coincide with
    //accessing order of the calling convention of stack varaible.
    //e.g:1
    //    f(a, b, c)
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
    //        -----
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
    if (g_is_enable_fp) {
        //FP will record the frame start address.
        //Caller is not responsible for pulling the SP down,
        //and callee should do it.
        ASSERTN(0, ("TODO"));
    }

    ArgDescMgr argdescmgr;
    processRealParamsThroughRegister(ir, &argdescmgr, ors, cont);

    //Record the size as return-value.
    IOC_param_size(cont) = argdescmgr.getArgSectionSize();
}


void IR2OR::convertASR(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_asr());
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);

    ASSERTN(opnd0->is_signed(), ("shift should be arithmetical"));

    cont->clean_bottomup();
    convertGeneralLoad(opnd0, ors, cont);
    SR * sr1 = cont->get_reg(0);

    SR * sh_ofst;
    if (opnd1->is_const()) {
        ASSERT0(opnd1->is_int());
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
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertLSR(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_lsr());
    ASSERT0(!BIN_opnd0(ir)->is_vec() && !BIN_opnd1(ir)->is_vec());
    IR * opnd0 = BIN_opnd0(ir);
    IR * opnd1 = BIN_opnd1(ir);

    ASSERT0(!opnd0->is_signed());

    IOC tc;
    convertGeneralLoad(opnd0, ors, &tc);
    SR * sr1 = tc.get_reg(0);

    SR * sh_ofst;
    if (opnd1->is_const() && opnd1->is_int()) {
        if (CONST_int_val(opnd1) == 0) {
            //Redundant operation, do nothing.
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
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertLSL(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
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
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertCvt(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_cvt() && CVT_exp(ir));
    ASSERTN(!ir->is_any() && !CVT_exp(ir)->is_any(), ("TODO"));
    RecycORList tors(this);
    convertGeneralLoad(CVT_exp(ir), tors, cont);
    m_cg->buildTypeCvt(ir, CVT_exp(ir), tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertGoto(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir->is_goto());

    //Target label
    SR * tgt_lab = m_cg->genLabel(GOTO_lab(ir));
    RecycORList tors(this);
    m_cg->buildUncondBr(tgt_lab, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertTruebr(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir != nullptr && ir->is_truebr());
    IR * br_det = BR_det(ir);
    ASSERT0(br_det->is_lt() || br_det->is_le() || br_det->is_gt() ||
            br_det->is_ge() || br_det->is_eq() || br_det->is_ne());

    convertRelationOp(br_det, ors, cont);

    RecycORList tors(this);
    m_cg->buildCondBr(m_cg->genLabel(BR_lab(ir)), ors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);
}


void IR2OR::convertFalsebr(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->is_falsebr());
    IR * newir = m_rg->dupIRTree(ir);
    IR * br_det = BR_det(newir);
    ASSERT0(br_det->is_lt() || br_det->is_le() || br_det->is_gt() ||
            br_det->is_ge() || br_det->is_eq() || br_det->is_ne());

    IR_code(br_det) = invertIRType(br_det->getCode());
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
                              IN IOC * cont)
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
    OR_TYPE t = m_cg->mapIRType2ORType(ir->getCode(), maxbytesize,
                                       sr0, sr1, is_signed);

    //Generate compare operations.
    tmp.clean();
    bool is_truebr = true;
    if (t == OR_UNDEF) {
        //Query the converse or-type.
        IR_TYPE rev_t = invertIRType(ir->getCode());
        t = m_cg->mapIRType2ORType(rev_t, maxbytesize, sr0, sr1, is_signed);
        ASSERTN(t != OR_UNDEF, ("miss comparsion or-type for branch"));
        is_truebr = false;
    }

    m_cg->buildCompare(t, is_truebr, sr0, sr1, tors.getList(), cont);
    tors.copyDbx(ir);
    ors.append_tail(tors);

    cont->set_ortype(t); //record the ortype.
}


void IR2OR::convertLabel(IRBB const* bb, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(bb);
    xcom::C<LabelInfo const*> * ct;
    for (bb->getLabelListConst().get_head(&ct);
         ct != bb->getLabelListConst().end();
         ct = bb->getLabelListConst().get_next(ct)) {
        LabelInfo const* li = ct->val();
        m_cg->buildLabel(li, ors.getList(), cont);
    }
}


void IR2OR::convert(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
{
    ASSERT0(ir && ir->verify(m_rg));

    RecycORList tors(this);
    switch (ir->getCode()) {
    case IR_CONST:
        convertLoadConst(ir, tors, cont);
        ASSERT0(cont->get_reg(0) || cont->get_addr());
        break;
    case IR_ID:
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
    case IR_PR:
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
        convertXOR(ir, tors, cont);
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
        break;
    default: ASSERTN(0, ("unknown IR type:%s", IRNAME(ir)));
    }
    ors.append_tail(tors);
}


//Translate IR in IRBB to a list of OR.
void IR2OR::convertIRBBListToORList(OUT RecycORList & or_list)
{
    START_TIMER(t, "Convert IR to OR");
    ASSERT0(m_rg);
    BBList * ir_bb_list = m_rg->getBBList();
    ASSERT0(ir_bb_list);
    IOC cont;
    for (IRBB const* bb = ir_bb_list->get_head();
         bb != nullptr; bb = ir_bb_list->get_next()) {
        convertLabel(bb, or_list, &cont);
        IRListIter ct;
        for (BB_irlist(bb).get_head(&ct);
             ct != BB_irlist(bb).end(); ct = BB_irlist(bb).get_next(ct)) {
            cont.clean();
            convert(ct->val(), or_list, &cont);
        }
    }
    END_TIMER(t, "Convert IR to OR");
}

} //namespace xgen
