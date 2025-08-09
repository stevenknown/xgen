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
#ifndef __SRDESC_H__
#define __SRDESC_H__

namespace xgen {

class RegFileSet;

//SR Descriptor
#define SRD_is_imm(sd) ((sd)->m_is_imm)
#define SRD_is_signed(sd) ((sd)->m_is_signed)
#define SRD_is_label(sd) ((sd)->m_is_label)
#define SRD_is_label_list(sd) ((sd)->m_is_label_list)
#define SRD_bitsize(sd) ((sd)->m_bitsize)
#define SRD_valid_regfile_set(sd) ((sd)->m_valid_regfile_set)
#define SRD_valid_reg_set(sd) ((sd)->m_valid_regset)
class SRDesc {
    COPY_CONSTRUCTOR(SRDesc);
public:
    ////////////////////////////////////////////////////////////////////////////
    //NOTE: DURING PRECOMPILE STAGE, THE PRINT ORDER OF FOLLOWING FIELD MUST  //
    //BE CONFORM TO THE RESPECTIVE DECLARATION ORDER.                         //
    ////////////////////////////////////////////////////////////////////////////
    BYTE m_is_signed:1; //1th printed
    BYTE m_is_imm:1; //2nd printed
    BYTE m_is_label:1; //3rd printed
    BYTE m_is_label_list:1; //4th printed
    UINT m_bitsize; //5th printed
    RegFileSet const* m_valid_regfile_set; //6th printed
    RegSet const* m_valid_regset; //7th printed
public:
    SRDesc() {}
    SRDesc(bool is_signed, bool is_imm, bool is_label, bool is_label_list,
           UINT bs, RegFileSet const* rfs, RegSet const* rs) :
        m_is_signed(is_signed), m_is_imm(is_imm), m_is_label(is_label),
        m_is_label_list(is_label_list), m_bitsize(bs), m_valid_regfile_set(rfs),
        m_valid_regset(rs) {}

    RegFileSet const* getValidRegFileSet() const
    { return SRD_valid_regfile_set(this); }
    RegSet const* getValidRegSet() const { return SRD_valid_reg_set(this); }
    UINT getBitSize() const { return SRD_bitsize(this); }

    void init()
    {
        m_is_imm = false;
        m_is_signed = false;
        m_is_label = false;
        m_is_label_list = false;
        m_bitsize = 0;
        m_valid_regfile_set = nullptr;
        m_valid_regset = nullptr;
    }
    bool is_imm() const { return SRD_is_imm(this); }
    bool is_signed() const { return SRD_is_signed(this); }
    bool is_label() const { return SRD_is_label(this); }
    bool is_label_list() const { return SRD_is_label_list(this); }
};


//Group of SRDesc
template <UINT DefaultSize = 0>
class SRDescGroup {
    COPY_CONSTRUCTOR(SRDescGroup);
    UINT opnd_num; //the number of operand.
    UINT res_num; //the number of result.
    SRDesc * sd_group[DefaultSize];

public:
    SRDescGroup(UINT resnum, UINT opndnum, ...)
    {
        opnd_num = opndnum;
        res_num = resnum;

        va_list ptr;
        va_start(ptr, opndnum);
        for (UINT i = 0; i < resnum + opndnum; i++) {
            sd_group[i] = va_arg(ptr, SRDesc*);
        }

        va_end(ptr);
    }
    ~SRDescGroup() {}

    void init(UINT resnum, UINT opndnum)
    {
        opnd_num = opndnum;
        res_num = resnum;
        if (opndnum + resnum > 0) {
            ::memset((void*)&sd_group, 0, (resnum + opndnum) * sizeof(SRDesc*));
        }
    }

    UINT get_opnd_num() const { return opnd_num; }
    UINT get_res_num() const { return res_num; }

    void set_opnd(UINT n, SRDesc * srd)
    {
        ASSERTN(n < opnd_num, ("opnd-idx %d more than opnd-num", n));
        sd_group[res_num + n] = srd;
    }

    SRDesc const* get_opnd(UINT n) const
    {
        ASSERTN(n < opnd_num, ("opnd-idx %d more than opnd-num", n));
        return sd_group[res_num + n];
    }

    void set_res(UINT n, SRDesc * srd)
    {
        ASSERTN(n < res_num, ("result-idx %d more than result-num", n));
        sd_group[n] = srd;
    }

    SRDesc const* get_res(UINT n) const
    {
        ASSERTN(n < res_num, ("result-idx %d more than result-num", n));
        return sd_group[n];
    }
};


//Compute the bytesize of SRDescGroup by given the number of operand and result.
inline UINT computeSRDescGroupSize(UINT resnum, UINT opndnum)
{
    return sizeof(SRDescGroup<0>) + sizeof(SRDesc*) * (opndnum + resnum);
}

} //namespace xgen

#endif
