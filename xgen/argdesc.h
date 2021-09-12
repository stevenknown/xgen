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
#ifndef _ARGDESC_H_
#define _ARGDESC_H_

namespace xgen {

//The class describes layout of each arguments and registers information during
//converting IR_CALL/IR_ICALL in IR2OR.
//IR2OR generator generates ORs to store arguments of CALL one by one, the
//ArgDesc informatin is updated during the generation.
class ArgDesc {
public:
    //If is_record_addr is true, src_value records
    //the start address to copy. Otherwise it records the value
    //that to be passed.
    union {
        SR * src_value;
        SR * src_startaddr;
    };

    xoc::Dbx const* arg_dbx; //describes Dbx to argument.
    UINT is_record_addr:1; //true if 
    UINT arg_size:30; //stack byte size to be passed.

    //byte offset to the base SR record in
    //src_startaddr if is_record_addr is true.
    UINT src_ofst;

    //byte offset to SP.
    UINT tgt_ofst;

public:
    ArgDesc() { init(); }

    void init()
    {
        src_value = nullptr;
        src_startaddr = nullptr;
        is_record_addr = false;
        arg_dbx = nullptr;
        arg_size = 0;
        src_ofst = 0;
        tgt_ofst = 0;
    }
};


class ArgDescMgr {
    COPY_CONSTRUCTOR(ArgDescMgr);
protected:
    Vector<ArgDesc*> m_arg_desc;
    List<ArgDesc*> m_arg_list;
    RegSet m_argregs;

protected:
    List<ArgDesc*> * getArgList() { return &m_arg_list; }

public:
    //A counter that record byte size of argument that have been passed.
    //User should update the value when one argument passed.
    UINT m_passed_arg_in_register_byte_size;

    //A counter that record byte size of argument that have been passed.
    //User should update the value when one argument passed.
    UINT m_passed_arg_in_stack_byte_size;

public:
    ArgDescMgr()
    {
        RegSet const* regs = tmGetRegSetOfArgument();
        if (regs != nullptr && regs->get_elem_count() != 0) {
            m_argregs.copy(*regs);
        }
        m_passed_arg_in_register_byte_size = 0;
        m_passed_arg_in_stack_byte_size = 0;
    }
    ~ArgDescMgr()
    {
        for (INT i = 0; i <= m_arg_desc.get_last_idx(); i++) {
            ArgDesc * desc = m_arg_desc.get(i);
            delete desc;
        }
        m_arg_desc.clean();
    }

    //Add arg-desc to the tail of argument-list.
    ArgDesc * addDesc()
    {
        ArgDesc * desc = new ArgDesc();
        m_arg_desc.set(m_arg_desc.get_last_idx() < 0 ?
            0 : m_arg_desc.get_last_idx() + 1, desc);
        m_arg_list.append_tail(desc);
        return desc;
    }

    ArgDesc * addDesc(bool is_record_addr,
                      SR * value_or_addr,
                      xoc::Dbx const* dbx,
                      UINT align,
                      UINT arg_size,
                      UINT src_ofst)
    {
        ArgDesc * desc = addDesc();
        desc->is_record_addr = is_record_addr;
        if (is_record_addr) {
            desc->src_startaddr = value_or_addr;
        } else {
            desc->src_value = value_or_addr;
        }
        desc->arg_dbx = dbx;
        desc->arg_size = arg_size;
        desc->src_ofst = src_ofst;
        desc->tgt_ofst = (UINT)xcom::ceil_align(getArgStartAddrOnStack(),
                                                align);
        updatePassedArgInStack(arg_size);
        return desc;
    }

    ArgDesc * addValueDesc(SR * src_value,
                           xoc::Dbx const* dbx,
                           UINT align,
                           UINT arg_size,
                           UINT src_ofst)
    { return addDesc(false, src_value, dbx, align, arg_size, src_ofst); }

    ArgDesc * addAddrDesc(SR * src_addr,
                          xoc::Dbx const* dbx,
                          UINT align,
                          UINT arg_size,
                          UINT src_ofst)
    { return addDesc(true, src_addr, dbx, align, arg_size, src_ofst); }

    //Allocate dedicated argument register.
    SR * allocArgRegister(CG * cg);

    ArgDesc * pullOutDesc() { return m_arg_list.remove_head(); }

    //Abandon the first argument phy-register.
    //The allocation will start at the next register.
    void dropArgRegister();

    //Get the total bytesize of current parameter section.
    UINT getArgSectionSize() const
    {
        return (UINT)xcom::ceil_align(
            getPassedRegisterArgSize() + getPassedStackArgSize(),
            STACK_ALIGNMENT);
    }
    ArgDesc * getCurrentDesc() { return m_arg_list.get_head(); }

    //Return the number of available physical reigsters.
    UINT getNumOfAvailArgReg() const
    { return getArgRegSet()->get_elem_count(); }
    RegSet const* getArgRegSet() const { return &m_argregs; }
    UINT getPassedRegisterArgSize() const
    { return m_passed_arg_in_register_byte_size; }
    UINT getPassedStackArgSize() const
    { return m_passed_arg_in_stack_byte_size; }
    UINT getArgStartAddrOnStack() const { return getPassedStackArgSize(); }

    //This function record the total bytesize that passed through registers.
    //This function should be invoked after passing one argument through
    //register.
    //Decrease tgt_ofst for each desc by the bytesize.
    //e.g: pass 2 argument:
    //  %r0 = arg1;
    //  mgr.updatePassedArgInRegister(GNENERAL_REGISTER_SIZE);
    //  %r1 = arg2;
    //  mgr.updatePassedArgInRegister(arg2.bytesize);
    void updatePassedArgInRegister(UINT bytesize);

    //This function record the total bytesize that passed via stack.
    //This function should be invoked after passing one argument to arg-
    //section. The argument may be passed through register or stack memory.
    //e.g: pass 2 argument:
    //  %r0 = arg1;
    //  mgr.updatePassedArgInRegister(GNENERAL_REGISTER_SIZE);
    //  [sp+0] = arg2;
    //  mgr.updatePassedArgInStack(arg2.bytesize);
    void updatePassedArgInStack(UINT bytesize)
    {
        m_passed_arg_in_stack_byte_size += (INT)xcom::ceil_align(
            bytesize, STACK_ALIGNMENT);
    }
};

} //namespace xgen
#endif
