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
#ifndef __REGPRESSURE_H__
#define __REGPRESSURE_H__

namespace xgen {

//Describe a register file and its architectural constraints.
class RegFileLimitDesc {
public:
    REGFILE m_regfile;      //Register file identifier.
    UINT m_limit;           //Maximum number of allocatable registers.
};


//Describe the register pressure effect caused by given data type.
//A single data type may increase pressure on multiple register files.
//For example, U64 may increase pressure on both RF_R and RF_V register files.
class TypeRegPressureEffect {
public:
    REGFILE m_primary_regfile;         //Primary register file associated
                                       //with data type.
    UINT m_weight;                     //Pressure increment applied to each.
                                       //affected register file.
    xcom::ROBitSet * m_affected_sets;  //Mask of register files whose pressure
                                       //is affected by this data type. Each bit
                                       //represents one register file.
};


//Mapping from data type to its register pressure description.
class TypeRegPressureDesc {
public:
    DATA_TYPE m_dtype;                  //The data type.
    CHAR const* m_name;                 //The name of data type.
    UINT m_mask;                        //Bitmask indicating which register
                                        //files are affected by data type.
    TypeRegPressureEffect m_reg_effect; //Register pressure information
                                        //associated with this data type.
};


class TypeRegPressureModel
    : public TMap<DATA_TYPE, TypeRegPressureEffect const*> {
public:
    TypeRegPressureModel(TypeRegPressureDesc const* table, UINT len);
    ~TypeRegPressureModel() {}
};



} //namespace xgen
#endif
