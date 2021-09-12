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
    virtual UINT getNumOfAllocableIntegerRegister() const { return 16; }
    virtual UINT getNumOfAllocableFloatRegister() const { return 16; }

    //Return byte size of data cache.
    virtual UINT getDCacheSize() const { return 32 * 1024; }

    //Return byte size of instruction cache.
    virtual UINT getICacheSize() const { return 32 * 1024; }

    //Return byte size of cache line.
    virtual UINT getCacheLineSize() const { return 32; }

    //Return byte size of TLB if any.
    virtual UINT getTLBSize() const { return 4096; }

    //Approximate the cycles to execute ir operation.
    virtual UINT estimateNumOfCycles(IR const*) const { return 2; }

    //Return byte size of data cache.
    virtual UINT getDCacheLineSize() const { return 32; }

    //Return byte size of instruction cache.
    virtual UINT getICacheLineSize() const { return 32; }
};

#endif
