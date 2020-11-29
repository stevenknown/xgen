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
#ifndef __IR2OR_H__
#define __IR2OR_H__

namespace xgen {

#define RESULT_REGISTER_INDEX 0
#define TRUE_PREDICATE_REGISTER_INDEX 1
#define FALSE_PREDICATE_REGISTER_INDEX 2

class ArgDescMgr;

//
//START IOC
//
//Record information during convertion in between IR and OR.
#define IOC_is_inverted(cont) ((cont)->u2.s1.is_inverted)
#define IOC_pred(cont) ((cont)->pred)
#define IOC_ortype(cont) ((cont)->ortype)
#define IOC_sr_vec(cont) ((cont)->reg_vec)
#define IOC_param_size(cont) ((cont)->u1.param_size)
#define IOC_mem_byte_size(cont) ((cont)->u1.mem_byte_size)
#define IOC_int_imm(cont) ((cont)->u1.int_imm)
class IOC {
public:
    //Used as output result, set by convertRelationOp(),
    //true if the relation operation inverted.
    union {
        struct {
            UINT is_inverted:1;
        } s1;
        UINT u2val;
    } u2;

    SR * pred; //used as input parameter, record predicate register if need.
    SR * addr; //used as input parameter, record memory address pseduo register.
    Vector<SR*> reg_vec;
    OR_TYPE ortype; //used as output, record OR_TYPE of result if exist.

    union {
        //used as input parameter, record total size of real
        //parameters before a function call.
        UINT param_size;

        //used as input parameter, record memory byte-size for operation.
        UINT mem_byte_size;

        //used as input parameter, record integer literal.
        LONGLONG int_imm;
    } u1;

public:
    IOC()
    {
        u2.u2val = 0;
        pred = nullptr;
        ortype = OR_UNDEF;
        u1.param_size = 0;
        u1.mem_byte_size = 0;
        reg_vec.init();
        addr = nullptr;
    }
    COPY_CONSTRUCTOR(IOC);
    virtual ~IOC() {}

    virtual void clean()
    {
        u2.u2val = 0;
        pred = nullptr;
        ortype = OR_UNDEF;
        u1.param_size = 0;
        u1.mem_byte_size = 0;
        reg_vec.clean();
        addr = nullptr;
    }

    virtual void set_pred(SR * p) { pred = p; }
    virtual void set_ortype(OR_TYPE ort) { ortype = ort; }
    virtual void set_addr(SR * a) { addr = a; }
    virtual void set_reg(INT i, SR * s)
    {
        ASSERT0(i >= 0);
        reg_vec.set(i, s);
    }
    virtual void copy_result(IOC const& src)
    {
        reg_vec.copy(src.reg_vec);
        set_addr(src.get_addr());
        IOC_mem_byte_size(this) = IOC_mem_byte_size(&src);
    }

    virtual SR * get_pred() const { return pred; }
    virtual SR * get_addr() const { return addr; }
    virtual SR * get_reg(INT i) const
    {
        ASSERT0(i >= 0);
        return reg_vec.get(i);
    }
    //Return the result register that context recorded.
    //The result register is either value or address.
    SR * getResult() const
    {
        SR * val = get_reg(0);
        if (val != nullptr) { return val; }
        return get_addr();        
    }
    OR_TYPE get_ortype() const { return ortype; }

    void clean_regvec() { reg_vec.clean(); }
    void clean_bottomup()
    {
        clean_regvec();
        set_addr(nullptr);
        set_ortype(OR_UNDEF);
    }
};
//END IOC


class IR2OR {
protected:
    Region * m_rg; //Current region.
    TypeMgr * m_tm; //Data manager.
    CG * m_cg; //Code generator.
    CGMgr * m_cgmgr; //Code generator mananger.
    RecycORListMgr m_recyc_orlist_mgr;

protected:
    void convertLoadConst(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    void flattenSRVec(IOC const* cont, Vector<SR*> * vec);

public:
    IR2OR(CG * cg);
    COPY_CONSTRUCTOR(IR2OR);
    virtual ~IR2OR() {}

    virtual void convertLabel(IRBB const* bb, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertILoad(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertIStore(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertLoadVar(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertId(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertStorePR(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertStoreVar(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertUnaryOp(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertBinaryOp(IR const* ir, OUT RecycORList & ors, IN IOC * cont);

    //Generate compare operations and return the comparation result registers.
    //The output registers in IOC are ResultSR,
    //TruePredicatedSR, FalsePredicatedSR.
    //The ResultSR record the boolean value of comparison of relation operation.
    virtual void convertRelationOp(IR const* ir,
                                   OUT RecycORList & ors,
                                   IN IOC * cont);
    virtual void convertASR(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertLSR(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertLSL(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertCvt(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertGoto(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertTruebr(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    virtual void convertFalsebr(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
    //Generate operations: reg = &var
    virtual void convertLda(Var const* var,
                            HOST_INT lda_ofst,
                            Dbx const* dbx,
                            OUT RecycORList & ors,
                            IN IOC * cont) = 0;
    virtual void convertLda(IR const* ir, OUT RecycORList & ors, IN IOC * cont) = 0;
    virtual void convertIgoto(IR const* ir,
                              OUT RecycORList & ors,
                              IN IOC * cont) = 0;
    virtual void convertReturn(IR const* ir,
                               OUT RecycORList & ors,
                               IN IOC * cont) = 0;
    virtual void convertSelect(IR const* ir,
                               OUT RecycORList & ors,
                               IN IOC * cont) = 0;
    virtual void convertCall(IR const* ir, OUT RecycORList & ors, IN IOC * cont) = 0;
    virtual void convertICall(IR const* ir,
                              OUT RecycORList & ors,
                              IN IOC * cont) = 0;
    virtual void convertGeneralLoadPR(IR const* ir,
                                      OUT RecycORList & ors,
                                      IN IOC * cont);
    virtual void convertGeneralLoad(IR const* ir,
                                    OUT RecycORList & ors,
                                    IN IOC * cont);
    //Store value that given by address 'src_addr' to 'tgtvar'.
    //ofst: offset from base of tgtvar.
    virtual void convertStoreViaAddress(IN SR * src_addr,
                                        IN SR * tgtvar,
                                        HOST_INT ofst,
                                        OUT RecycORList & ors,
                                        IN IOC * cont);
    virtual void convertStoreDecompose(IN SR * src,
                                       IN SR * tgtvar,
                                       HOST_INT ofst,
                                       OUT RecycORList & ors,
                                       IN IOC * cont);
    virtual void convertCopyPR(IR const* tgt,
                               IN SR * src,
                               OUT RecycORList & ors,
                               IN IOC * cont);
    virtual void convertAdd(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_add(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertSub(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_sub(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertDiv(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_div(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertMul(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_mul(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }


    virtual void convertRem(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_rem(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertMod(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_mod(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    //Logical AND
    virtual void convertLogicalAnd(IR const* ir,
                                   OUT RecycORList & ors,
                                   IN IOC * cont)
    {
        ASSERTN(ir->is_land(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    //Logical OR
    virtual void convertLogicalOr(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_lor(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    //Bitwise AND
    virtual void convertBitAnd(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_band(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    //Bitwise OR
    virtual void convertBitOr(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_bor(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertXOR(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_xor(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    //Bitwise NOT.
    //e.g BNOT(0x0001) = 0xFFFE
    virtual void convertBitNot(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_bnot(), ("illegal ir"));
        convertUnaryOp(ir, ors, cont);
    }

    //Boolean logical not.
    //e.g LNOT(non-zero) = 0, LNOT(0) = 1
    virtual void convertLogicalNot(IR const* ir,
                                   OUT RecycORList & ors,
                                   IN IOC * cont)
    {
        ASSERTN(ir->is_lnot(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertNeg(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_neg(), ("illegal ir"));
        convertBinaryOp(ir, ors, cont);
    }

    virtual void convertLT(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_lt(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    virtual void convertLE(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_le(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    virtual void convertGT(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_gt(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    virtual void convertGE(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_ge(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    virtual void convertEQ(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir->is_eq(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    virtual void convertNE(IR const* ir, OUT RecycORList & ors, IN IOC * cont)
    {
        ASSERTN(ir && ir->is_ne(), ("illegal ir"));
        convertRelationOp(ir, ors, cont);
    }

    void copyDbx(OR * o, IR const* ir)
    {
        Dbx * d = ::getDbx(ir);
        if (d != nullptr) { OR_dbx(o).copy(*d); }
    }

    void convertIRBBListToORList(OUT RecycORList & or_list);
    virtual void convert(IR const* ir, OUT RecycORList & ors, IN IOC * cont);

    RecycORListMgr * getRecycORListMgr() { return &m_recyc_orlist_mgr; }
    CG * getCG() const { return m_cg; }
    Var const* getBuiltinVar(BUILTIN_TYPE bt) const
    { return m_cgmgr->getBuiltinVar(bt); }

    //Register local variable that will be allocated in memory.
    Var * registerLocalVar(IR const* pr);

    //Return true if whole ir has been passed through registers, otherwise
    //return false.
    void processRealParamsThroughRegister(IR const* ir,
                                          ArgDescMgr * argdesc,
                                          OUT RecycORList & ors,
                                          IN IOC * cont);
    void processRealParams(IR const* ir, OUT RecycORList & ors, IN IOC * cont);
};

} //namespace xgen
#endif
