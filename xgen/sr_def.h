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
#ifndef __SR_DEF_H__
#define __SR_DEF_H__

typedef ULONG SymRegId; //Symbol Register Id, it must be different with Reg.

//The class represents the symbol register.
#define SR_sym_reg(sr) ((RegSR*)CK_SR_REG(sr))->symbol_reg_id
#define SR_regfile(sr) ((RegSR*)CK_SR_REG(sr))->regfile
#define SR_spill_var(sr) ((RegSR*)CK_SR_REG(sr))->spill_var
#define SR_phy_reg(sr) ((RegSR*)CK_SR_REG(sr))->phy_reg_id
class RegSR : public SR {
    void clean_self()
    {
        symbol_reg_id = SYMREG_UNDEF;
        regfile = RF_UNDEF;
        spill_var = nullptr;
        phy_reg_id = REG_UNDEF;
    }
public:
    SymRegId symbol_reg_id; //symbol register id, start at SRID_UNDEF + 1.
    REGFILE regfile; //physical register file.
    Reg phy_reg_id; //physical register id, start at REG_UNDEF + 1.
    xoc::Var * spill_var; //xoc::Var to hold spilled register.
public:
    RegSR() { SR_code(this) = SR_REG; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        symbol_reg_id = SR_sym_reg(src);
        regfile = SR_regfile(src);
        spill_var = SR_spill_var(src);
        phy_reg_id = SR_phy_reg(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_str(sr) ((StrSR*)CK_SR_STR(sr))->str
//The class represents the const string.
class StrSR : public SR {
    void clean_self() { str = nullptr; }
public:
    xoc::Sym const* str; //records a const string
public:
    StrSR() { SR_code(this) = SR_STR; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        str = SR_str(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_imm_size(sr) ((ImmSR*)CK_SR_IMM(sr))->size
class ImmSR : public SR {
    void clean_self() { size = 0; }
public:
    UINT size; //byte size of immediate.
public:
    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        size = SR_imm_size(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const
    { ASSERTN(0, ("internal used class")); DUMMYUSE((cg, buf));
      return nullptr; }
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const
    { ASSERTN(0, ("internal used class")); DUMMYUSE((cg, buf));
      return nullptr; }
};


#define SR_fp_imm(sr) ((FPSR*)CK_SR_IMM(sr))->fp_imm
//The class represents float number.
class FPSR : public ImmSR {
    void clean_self() { fp_imm = 0.0; }
public:
    HOST_FP fp_imm; //float point.
public:
    FPSR() { SR_code(this) = SR_FP_IMM; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        ImmSR::copy(src);
        fp_imm = SR_fp_imm(src);
    }
    virtual void clean()
    {
        ImmSR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_int_imm(sr) ((IntSR*)CK_SR_IMM(sr))->int_imm
//The class represents integer number.
class IntSR : public ImmSR {
    void clean_self() { int_imm = 0; }
public:
    //Note host LONGLONG should not less than HOST_INT,
    //otherwise the integer might be truncated wrongfully.
    HOST_INT int_imm;
public:
    IntSR()
    {
        SR_code(this) = SR_INT_IMM;
        clean_self();
    }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        ImmSR::copy(src);
        int_imm = SR_int_imm(src);
    }
    virtual void clean()
    {
        ImmSR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_var(sr) ((VarSR*)CK_SR_VAR(sr))->var
#define SR_var_ofst(sr) ((VarSR*)CK_SR_VAR(sr))->ofst
#define SR_var_ir(sr) ((VarSR*)CK_SR_VAR(sr))->ir
//The class represents memory variable.
class VarSR : public SR {
    void clean_self()
    {
        var = nullptr;
        ir = nullptr;
        ofst = 0;
    }
public:
    xoc::Var const* var;
    UINT ofst; //offset base on var.

    //Record the corresponding IR at middle-end if
    //the variable generated by IR.
    xoc::IR const* ir;
public:
    VarSR() { SR_code(this) = SR_VAR; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        var = SR_var(src);
        ofst = SR_var_ofst(src);
        ir = SR_var_ir(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_label(sr) ((LabSR*)CK_SR_LAB(sr))->label
//The class represents a label.
class LabSR : public SR {
    void clean_self() { label = nullptr; }
public:
    xoc::LabelInfo const* label; //records Internal-Label or Custom-Label
public:
    LabSR() { SR_code(this) = SR_LAB; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        label = SR_label(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};


#define SR_label_list(sr) ((LabListSR*)CK_SR_LAB_LIST(sr))->label_list
//The class represents a label-list.
class LabListSR : public SR {
    void clean_self() { label_list = nullptr; }
public:
    LabelInfoList const* label_list; //the list of LabelInfo.
public:
    LabListSR() { SR_code(this) = SR_LAB_LIST; clean_self(); }

    virtual void copy(SR const* src)
    {
        ASSERT0(getCode() == src->getCode());
        SR::copy(src);
        label_list = SR_label_list(src);
    }
    virtual void clean()
    {
        SR::clean();
        clean_self();
    }
    virtual CHAR const* get_name(StrBuf & buf, CG const* cg) const;
    virtual CHAR const* getAsmName(StrBuf & buf, CG const* cg) const;
};

#endif
