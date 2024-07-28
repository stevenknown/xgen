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
#ifndef __TARG_INTERFACE_H__
#define __TARG_INTERFACE_H__

namespace xgen {

class RegSet;
class ORCodeDesc;
class SRDesc;
struct EquORCodes;
class TargMach;
class CGMgr;
typedef struct tagORScheInfo ORScheInfo;

//Return name of cluster.
CHAR const* tmGetClusterName(CLUST);

//Return name of function-unit.
CHAR const* tmGetUnitName(UNIT);

//Return name of register.
CHAR const* tmGetRegName(Reg reg);

//Return the number of registers.
UINT const tmGetRegNum();

//Return name of given register-file.
CHAR const* tmGetRegFileName(REGFILE);

//Return name of given slot.
CHAR const* tmGetSlotName(SLOT);

//Return name of OR code.
CHAR const* tmGetORCodeName(OR_CODE);

//Return a set of registers for given register-file.
RegSet const* tmMapRegFile2RegSet(REGFILE);

//Return register-file for given register.
REGFILE tmMapReg2RegFile(Reg);

//Return target-machine-word for given register.
//The tmword is always used in assembly or machine code generation.
//e.g:For ARM, the register r1's machine word is 0x1, and the value will be
//write into a 32bits binary during machine code generation.
TMWORD tmMapReg2TMWORD(Reg);

//Return alloable register set for given register-file.
RegSet const* tmMapRegFile2RegSetAllocable(REGFILE rf);

//Return alloable register set for given cluster.
RegSet const* tmMapCluster2RegSetAlloable(CLUST clust);

//Return description for given or-type.
ORCodeDesc const* tmGetORCodeDesc(OR_CODE);

//Return SR description of the operand indexed by 'idx' for given or-type.
SRDesc const* tmGetOpndSRDesc(OR_CODE ot, UINT idx);

//Return SR description of the result indexed by 'idx' for given or-type.
SRDesc const* tmGetResultSRDesc(OR_CODE ot, UINT idx);

//Return true if regfile is integer register file.
bool tmIsIntRegFile(REGFILE rf);

//Return true if regfile is float point register file.
bool tmIsFloatRegFile(REGFILE rf);

//Return true if regfile is predicate register file.
bool tmIsPredicateRegFile(REGFILE rf);

//Return a set of registers correspond to given cluster.
RegSet const* tmMapCluster2RegSet(CLUST);

//Return the number of source operands for given or-type.
UINT tmGetOpndNum(OR_CODE);

//Return the number of target operands for given or-type.
UINT tmGetResultNum(OR_CODE);

//Get equivalent or-types that has same functions.
EquORCodes const* tmGetEqualORCode(OR_CODE);

//Get the number of equivalent or-types that has same functions.
UINT tmGetNumOfEqualORCode(OR_CODE);

//Get allocable register set.
RegSet const* tmGetRegSetAllocable();

//Return allocable register set to given regfile.
RegSet const* tmMapRegFile2Allocable(REGFILE);

//Get return-value used register set.
RegSet const* tmGetRegSetOfReturnValue();

//Get argument passing used register set.
RegSet const* tmGetRegSetOfArgument();

//Get caller-saved register set.
RegSet const* tmGetRegSetOfCallerSaved();

//Get callee-saved register set.
RegSet const* tmGetRegSetOfCalleeSaved();

//Get allocable vector register set.
RegSet const* tmGetVectorRegSetAllocable();

//Get return-value used vector register set.
RegSet const* tmGetVectorRegSetOfReturnValue();

//Get argument passing used vector register set.
RegSet const* tmGetVectorRegSetOfArgument();

//Get caller-saved vector register set.
RegSet const* tmGetVectorRegSetOfCallerSaved();

//Get callee-saved vector register set.
RegSet const* tmGetVectorRegSetOfCalleeSaved();

//Get the Read and Write available cycle.
ORScheInfo const* tmGetORScheInfo(OR_CODE ot);

//Allocate CGMgr for target machine.
CGMgr * allocCGMgr(RegionMgr * rm);

} //namespace
#endif
