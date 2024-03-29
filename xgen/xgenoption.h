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
#ifndef __XGEN_OPTION_H__
#define __XGEN_OPTION_H__

namespace xgen {

class DumpOpt {
public:
    //Dump all information.
    //Note is_dump_all and is_dump_nothing can not all be true.
    bool is_dump_all;
    //Do not dump anything.
    //Note is_dump_all and is_dump_nothing can not all be true.
    bool is_dump_nothing;
    bool is_dump_cg; //Dump CodeGeneration.
    bool is_dump_ra; //Dump Register Allocation.
    bool is_dump_lis; //Dump LIS.
    bool is_dump_ir2or; //Dump IR2OR convertion.
    bool is_dump_revise_spadjust; //Dump spadjust relocation.
    bool is_dump_relocate_stack; //Dump stack variable relocation.
    bool is_dump_package; //Dump instruction packaging.
    bool is_dump_localize; //Dump localization.
public:
    DumpOpt();
    DumpOpt const& operator = (DumpOpt const&); //Disable operator =.

    bool isDumpAll() const;
    bool isDumpNothing() const;
    bool isDumpCG() const;
    bool isDumpRA() const;
    bool isDumpLIS() const;
    bool isDumpIR2OR() const;
    bool isDumpReviseSpadjust() const;
    bool isDumpRelocateStack() const;
    bool isDumpPackage() const;
    bool isDumpLocalize() const;
};

//Externed Variables
extern bool g_gen_code_for_big_return_value;

//Set the flag to be true if you want OR::dump print souce file.
extern bool g_cg_dump_src_line;

//Perform instruction scheduling.
extern bool g_do_lis;

//Perform global register allocation.
extern bool g_do_gra;

//Enable access parameter via FP+ofst.
extern bool g_is_enable_fp;

//Perform Code Generation.
extern bool g_do_cg;

//Enable scheduling delay-slot of branch/call operations.
extern bool g_enable_schedule_delay_slot;

//Enable generate OR for inner region.
extern bool g_is_generate_code_for_inner_region;

//Record dump options for each Pass in XGen.
extern DumpOpt g_xgen_dump_opt;

} //namespace xgen

#endif
