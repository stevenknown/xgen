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
#ifndef _ARM_TARG_INFO_H_
#define _ARM_TARG_INFO_H_

class ARMTargInfo : public TargInfo {
public:
    // Return the alignment parameter in memory.
    virtual UINT getAlignMem() const override { return STACK_ALIGNMENT; }

    //Return the alignment parameter for matrix.
    virtual UINT getMemAlignOfMatrix() const
    {
        ASSERTN(0, ("ARM does not support matrix operations"));
        return 0;
    }
    virtual UINT getNumOfReturnValueRegister() const override;
    virtual UINT getNumOfAllocableIntegerRegister() const override;
    virtual UINT getNumOfAllocableFloatRegister() const override;
    virtual UINT getNumOfGroup() const override { return 1; }
    virtual UINT getNumOfThread() const override { return 1; }

    //Return byte size of data cache.
    virtual UINT getDCacheSize() const override { return 32 * 1024; }

    //Return byte size of instruction cache.
    virtual UINT getICacheSize() const override { return 32 * 1024; }

    //Return byte size of TLB if any.
    virtual UINT getTLBSize() const override { return 4096; }

    //Approximate the cycles to execute ir operation.
    virtual UINT estimateNumOfCycles(IR const*) const override { return 2; }

    //Return byte size of data cache.
    virtual UINT getDCacheLineSize() const override { return 32; }

    //Return byte size of instruction cache.
    virtual UINT getICacheLineSize() const override { return 32; }

    //Returns the minimum alignment for instructions in the debug section.
    virtual UINT getMinDebugInstAlign() const override { return 1; }

    //Get the callee-saved register stack slot size in bytes.
    virtual UINT getCalleeSaveStackSlotSize() const override
    { return CALLEE_SAVE_STACK_SLOT_SIZE; }

    //Return true if the stack grows downward, false means growing upward.
    virtual bool isStackGrowthDownward() const override { return true; }
};

#endif
