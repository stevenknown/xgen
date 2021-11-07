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

#include "xgeninc.h"

namespace xgen {

//
//START DumpOpt
//
DumpOpt::DumpOpt()
{
    is_dump_all = false;
    is_dump_nothing = false;
    is_dump_cg = false;
    is_dump_ra = false;
    is_dump_lis = false;
    is_dump_ir2or = false;
}


bool DumpOpt::isDumpAll() const
{
    //is_dump_all and is_dump_nothing can not all be true.
    ASSERT0(!(is_dump_nothing & is_dump_all));
    return is_dump_all;
}


bool DumpOpt::isDumpNothing() const
{
    //is_dump_all and is_dump_nothing can not all be true.
    ASSERT0(!(is_dump_nothing & is_dump_all));
    return is_dump_nothing;
}


bool DumpOpt::isDumpCG() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_cg);
}


bool DumpOpt::isDumpRA() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_ra);
}


bool DumpOpt::isDumpLIS() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_lis);
}


bool DumpOpt::isDumpIR2OR() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_ir2or);
}


bool DumpOpt::isDumpReviseSpadjust() const
{
    return is_dump_all || is_dump_cg ||
           (!is_dump_nothing && is_dump_revise_spadjust);
}


bool DumpOpt::isDumpRelocateStack() const
{
    return is_dump_all || is_dump_cg ||
           (!is_dump_nothing && is_dump_relocate_stack);
}


bool DumpOpt::isDumpPackage() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_package);
}


bool DumpOpt::isDumpLocalize() const
{
    return is_dump_all || is_dump_cg || (!is_dump_nothing && is_dump_localize);
}
//END DumpOpt


//Generate code for big return value that can not be accommdated in
//return-value registers.
//e.g:
// typedef struct _S{int a,b,c,d;}S;
// extern S foo();
// void bar() {
//   s = foo();
// }
bool g_gen_code_for_big_return_value = true;

//Set the flag to be true if you want OR::dump print souce file.
bool g_cg_dump_src_line = true;

//Perform instruction scheduling.
bool g_do_lis = true;

//Perform global register allocation.
bool g_do_gra = false;

//Accessing parameter via FP+ofst
bool g_is_enable_fp = true;

//Perform Code Generation.
bool g_do_cg = true;

//Enable scheduling delay-slot of branch/call operations.
bool g_enable_schedule_delay_slot = false;

//Enable generate OR for inner region.
//The option is false in default, because user intends to generate code by
//iterating RegionTab, instead of generating code for inner region recursively.
bool g_is_generate_code_for_inner_region = false;

//Record dump options for each Pass in XGen.
DumpOpt g_xgen_dump_opt;

} //namespace xgen
