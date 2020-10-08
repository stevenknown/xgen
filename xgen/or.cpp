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
//START ORList
//
void ORList::append_tail(OR * o)
{
    #ifdef _DEBUG_
    for (OR * t = get_head(); t != NULL; t = get_next()) {
        ASSERTN(t != o, ("already in list."));
    }
    #endif
    List<OR*>::append_tail(o);
}


void ORList::append_tail(ORList & ors)
{
    #ifdef _DEBUG_
    for (OR * o = get_head(); o != NULL; o = get_next()) {
        for (OR * oo = ors.get_head(); oo != NULL; oo = ors.get_next()) {
            ASSERTN(o != oo, ("already in list."));
        }
    }
    #endif
    List<OR*>::append_tail(ors);
}


void ORList::dump(CG * cg)
{
    xcom::StrBuf buf(128);
    for (OR * o = get_head(); o != NULL; o = get_next()) {
        buf.clean();
        o->dump(buf, cg);
        note(cg->getRegion(), "\n%s", buf.buf);
    }
    note(cg->getRegion(), "\n");
}
//END ORList


//
//START OR
//
void OR::clean()
{
    //DO NOT MODIFY 'id'. It is the marker in or_vector.
    code = OR_UNDEF;
    clust = CLUST_UNDEF; //cluster
    container = NULL;
    order = -1;
    bb = NULL;
    OR_flag(this) = 0;
    dbx.clean();
    m_opnd.clean();
    m_result.clean();
}


void OR::copyDbx(IR const* ir)
{
    ASSERT0(ir);
    Dbx * t = ::getDbx(ir);
    if (t != NULL) {
        OR_dbx(this).copy(*t);
    }
}


void OR::clone(OR const* o)
{
    //DO NOT MODIFY 'id'.
    m_opnd.copy(o->m_opnd);
    m_result.copy(o->m_result);
    dbx.copy(o->dbx);
    code = o->code;
    order = -1; //order in BB need to recompute.
    bb = o->bb;
    OR_flag(this) = OR_flag(o);
}


//Get imm operand of instruction.
SR * OR::get_imm_sr()
{
    //Normally, operand 1 of 'mov imm to reg OR' should be literal.
    SR * sr = get_opnd(HAS_PREDICATE_REGISTER + 0);
    ASSERTN(sr->is_constant(), ("operand of movi must be literal"));
    return sr;
}


bool OR::is_equal(OR const* o) const
{
    if (OR_code(this) == o->getCode() &&
        OR_bb(this) == OR_bb(o) &&
        OR_is_call(this) == OR_is_call(o) &&
        OR_is_cond_br(this) == OR_is_call(o) &&
        OR_is_predicated(this) == OR_is_predicated(o) &&
        OR_is_uncond_br(this) == OR_is_uncond_br(o) &&
        OR_is_return(this) == OR_is_return(o) &&
        OR_flag(this) == OR_flag(o)) {
        return true;
    }
    return false;
}


void OR::dump(CG * cg) const
{
    xcom::StrBuf buf(128);
    dump(buf, cg);
    note(cg->getRegion(), "\n%s", buf.buf);
}


CHAR const* OR::dump(xcom::StrBuf & buf, CG * cg) const
{
    if (xoc::g_dbx_mgr != NULL && g_cg_dump_src_line) {
        DbxMgr::PrtCtx prtctx;
        xoc::g_dbx_mgr->printSrcLine(buf, &OR_dbx(this), &prtctx);
    }

    OR * pthis = const_cast<OR*>(this);
    //Order in BB.
    if (OR_bb(this) == NULL) {
        buf.strcat("[????]");
    } else {
        buf.strcat("[O:%d]", OR_order(this));
    }

    //Unique id. You can disable it for diminish the output info.
    buf.strcat("[id:%d] ", OR_id(this));
    buf.strcat(OR_code_name(this));
    buf.strcat(" ");

    if (OR_is_store(this)) {
        //[base + ofst] <- pred, store_val

        //memory addr
        buf.strcat("[");
        pthis->get_store_base()->get_name(buf, cg);

        //memory offset
        SR * ofst = pthis->get_store_ofst();
        if (cg->isComputeStackOffset() || ofst->is_int_imm()) {
            ASSERT0(ofst->is_int_imm());
            if (ofst->getInt() != 0) {
                if (ofst->getInt() > 0) {
                    buf.strcat(" + ");
                } else {
                    buf.strcat(" - ");
                }
                ofst->get_name(buf, cg);
            }
        } else {
            ASSERT0(ofst->is_var());
            buf.strcat(" + ");
            buf.strcat("'");
            buf.strcat(SYM_name(VAR_name(SR_var(ofst))));
            buf.strcat("'");
            if (SR_var_ofst(ofst) != 0) {
                buf.strcat(" + ");
                buf.strcat("%d", SR_var_ofst(ofst));
            }
        }

        buf.strcat("] <-- ");

        //predicate register if any.
        pthis->get_pred()->get_name(buf, cg);
        buf.strcat(", ");

        //The value to be stored.
        for (UINT i = 0; i < get_num_store_val(); i++) {
            ASSERTN(pthis->get_store_val(i), ("miss operand"));
            pthis->get_store_val(i)->get_name(buf, cg);
            if (i < get_num_store_val() - 1) {
                buf.strcat(", ");
            }
        }
    } else if (OR_is_load(this)) {
        //reg <- predicate_register, [base + ofst]
        //load value
        for (UINT i = 0; i < get_num_load_val(); i++) {
            ASSERTN(pthis->get_load_val(i) != NULL, ("miss result"));
            pthis->get_load_val(i)->get_name(buf, cg);
            if (i < get_num_load_val() - 1) {
                buf.strcat(", ");
            }
        }

        buf.strcat(" <-- ");

        //predicate register
        if (HAS_PREDICATE_REGISTER) {
            pthis->get_pred()->get_name(buf, cg);
            buf.strcat(", ");
        }

        //memory address
        buf.strcat("[");
        pthis->get_load_base()->get_name(buf, cg);

        //memory offset
        SR * ofst = pthis->get_load_ofst();
        if (cg->isComputeStackOffset() || ofst->is_int_imm()) {
            ASSERT0(ofst->is_int_imm());
            if (ofst->getInt() != 0) {
                if (ofst->getInt() > 0) {
                    buf.strcat(" + ");
                } else {
                    buf.strcat(" - ");
                }
                ofst->get_name(buf, cg);
            }
        } else {
            ASSERT0(ofst->is_var());
            buf.strcat(" + ");
            buf.strcat("'");
            buf.strcat(SYM_name(VAR_name(SR_var(ofst))));
            buf.strcat("'");
            if (SR_var_ofst(ofst) != 0) {
                buf.strcat(" + ");
                buf.strcat("%d", SR_var_ofst(ofst));
            }
        }

        buf.strcat("]");
    } else {
        for (UINT i = 0; i < result_num(); i++) {
            SR * res = get_result(i);
            res->get_name(buf, cg);
            if (i < result_num() - 1) {
                buf.strcat(", ");
            }
        }

        buf.strcat(" <-- ");

        for (UINT i = 0; i < opnd_num(); i++) {
            SR * opd = get_opnd(i);
            opd->get_name(buf, cg);
            if (i < opnd_num() - 1) {
                buf.strcat(", ");
            }
        }
    }

    //cluster
    if (OR_clust(this) != CLUST_UNDEF) {
        buf.strcat("  %s", tmGetClusterName(OR_clust(this)));
    }

    //assistant info
    if (OR_is_store(this)) {
        xoc::Var const* var = cg->mapOR2Var(pthis);
        if (var != NULL) {
            buf.strcat("  //store to '");
            var->dump(buf, cg->getRegion()->getTypeMgr());
            buf.strcat("'");
        }
    } else if (OR_is_load(this)) {
        xoc::Var const* var = cg->mapOR2Var(pthis);
        if (var != NULL) {
            buf.strcat("  //load from '");
            var->dump(buf, cg->getRegion()->getTypeMgr());
            buf.strcat("'");
        }
    }

    return buf.buf;
}


//Return the idx of opnd 'sr'.
INT OR::get_opnd_idx(SR * sr) const
{
    for (INT i = 0; i <= m_opnd.get_last_idx(); i++) {
        if (m_opnd.get(i) == sr) {
            return i;
        }
    }
    return -1;
}


//Return the idx of opnd 'sr'.
INT OR::get_result_idx(SR * sr) const
{
    for (INT i = 0; i <= m_result.get_last_idx(); i++) {
        if (m_result.get(i) == sr) {
            return i;
        }
    }
    return -1;
}
//END OR


//
//START ORMgr
//
ORMgr::ORMgr(SRMgr * srmgr)
{
    ASSERT0(srmgr);
    m_sr_mgr = srmgr;
}


void ORMgr::clean()
{
    for (INT i = 0; i <= get_last_idx(); i++) {
        OR * o = get(i);
        if (o != NULL) {
            //o may be NULL.
            delete o;
        }
    }

    Vector<OR*>::clean();
    m_free_or_list.clean();
}


OR * ORMgr::allocOR()
{
    return new OR();
}


OR * ORMgr::getOR(UINT id)
{
    return get(id);
}


OR * ORMgr::genOR(OR_TYPE ort, CG * cg)
{
    OR * o = m_free_or_list.remove_head();
    if (o == NULL) {
        o = allocOR();
        INT idx = get_last_idx();
        if (idx < 0) {
            OR_id(o) = 1; //Do not use 0 as index. 0 is reserved.
        } else {
            OR_id(o) = idx + 1;
        }
        set(OR_id(o), o);
    }
    OR_code(o) = ort;
    if (HAS_PREDICATE_REGISTER) {
        ASSERT0(cg);
        o->set_pred(cg->getTruePred());
    }
    return o;
}


void ORMgr::freeOR(OR * o)
{
    UINT i;
    ASSERT0(m_sr_mgr);
    for (i = 0; i < o->result_num(); i++) {
        SR * sr = o->get_result(i);
        ASSERT0(sr != NULL);
        if (!SR_is_dedicated(sr)) {
            m_sr_mgr->freeSR(sr);
        }
    }

    for (i = 0; i < o->opnd_num(); i++) {
        SR * sr = o->get_opnd(i);
        ASSERT0(sr != NULL);
        if (!SR_is_dedicated(sr)) {
            m_sr_mgr->freeSR(sr);
        }
    }

    o->clean();
    m_free_or_list.append_head(o);
}


void ORMgr::freeORList(IN ORList & ors)
{
    for (OR * o = ors.get_head(); o != NULL; o = ors.get_next()) {
        freeOR(o);
    }
}
//END ORMgr

} //namespace xgen
