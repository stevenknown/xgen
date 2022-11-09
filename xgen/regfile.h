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
#ifndef __REG_FILE_H__
#define __REG_FILE_H__

namespace xgen {

//Register File set
class RegFileSet : public xcom::BitSet {
public:
    RegFileSet(UINT init_pool_size = 2) : xcom::BitSet(init_pool_size) {}
    RegFileSet(RegFileSet const& rfs) { xcom::BitSet::copy(rfs); }

    //Convert from BSIdx to REGFILE.
    static REGFILE toRegFile(BSIdx x)
    { return IS_BSUNDEF(x) ? RF_UNDEF : (REGFILE)x; }
};


//This class indicate the properties of register file.
class RegFileProp {
public:
    CHAR const* name; //name of register file.
    bool is_integer; //indicate whether if the register file is integer.
    bool is_float; //indicate whether if the register file is float.
    bool is_predicate; //indicate whether if the register file is predicate.
};

//Return true if regfile is legal to target machine.
inline bool isLegalRegFile(REGFILE rf)
{
    return rf > RF_UNDEF && rf < RF_NUM;
}

} //namespace xgen
#endif
