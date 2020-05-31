/*@
Copyright (c) 2013-2014, Su Zhenyu steven.known@gmail.com

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
@*/
#include "../opt/cominc.h"
#include "feinc.h"

#undef DEBUG_XOC
#ifdef DEBUG_XOC
INT main(INT argcc, CHAR * argvc[])
{
    DUMMYUSE(argcc);
    DUMMYUSE(argvc);

    CHAR * argv[] = {
        "xocc.exe", "-gra=off", "-o","a.asm",
        //#ifdef _DEBUG_
        "-dump","tmp.log",
        //#endif
        //"-ipa", "test1.c",
        #ifndef _ON_WINDOWS_
        "../testsuite/cp.c",
        #else
        //"..\\..\\testsuite\\active\\test_for_loop.c", //fail in VRP. deadlock.
        //"..\\..\\testsuite\\active\\t4.c", //pass, failedin CG, need generate REM.
        //"..\\..\\testsuite\\active\\array.c", //failed in VRP
        //"..\\..\\testsuite\\active\\struct_field.c", //failed in VRP
        //"..\\..\\testsuite\\active\\cvt.c", //failed in VRP
        //"..\\..\\testsuite\\active\\ddot.c", //failed in VRP
        //"..\\..\\testsuite\\active\\alias_2.c", //failed in VRP
        //"..\\..\\testsuite\\active\\loop_construct.c", //failed in VRP
        //"..\\..\\testsuite\\active\\caffmark_logicatom.c",//failed in VRP
        //"..\\..\\testsuite\\active\\alias_analysis.c", //pass, failed in GCSE.
        //"..\\..\\testsuite\\active\\scalar_replace.c", //failed in VRP
        //"..\\..\\testsuite\\active\\ivr.c", //pass, failed in GCSE.
        //"..\\..\\testsuite\\active\\cond_exe.c", //fail in PRE, deadlock.
        //"..\\..\\testsuite\\active\\atol.c", //fail in PRE, deadlock.
        //"..\\..\\testsuite\\active\\icall.c", //fail in PRE, deadlock.
        //"..\\..\\testsuite\\active\\simp_typeinfer.c", //fail in PRE, deadlock.
        //"..\\..\\testsuite\\active\\array.c", //failed in VRP
        ////NOTE: !! Modify refineSwtich() to disrefine SWITCH_body to enable the extremly pressure test.
        //"..\\..\\test\\compile\\licm2.c", //pass, failed in GCSE.
        //"..\\..\\test\\exec\\big.switch.c", //Out of mem
        //"..\\..\\test\\exec\\array.assignment.c", //flow sensitive cost too much memory.
        //"..\\..\\test\\exec\\big.switch.org.c", //Out of mem
        //"..\\..\\test\\exec\\stackoverflow.c", //cfe failed!
        //"..\\..\\test\\exec\\loop_preheader.c",
        //"..\\..\\test\\compile\\cfg.c",
        //"..\\..\\test\\compile\\refine_duchain.c",
        //"..\\..\\test\\compile\\array3.c",
        //"..\\..\\test\\compile\\duref.c",
        //"..\\..\\test\\compile\\cfg2.c",
        //"..\\..\\test\\compile\\bug.c",
        //"..\\..\\test\\compile\\20020110.c",
        //"..\\..\\test\\compile\\alias10.c",
        //"..\\..\\test\\compile\\alias11_2.c",
        //"..\\..\\test\\compile\\alias11.c",        
        //"..\\..\\test\\compile\\dce.c", //fail in CG, TODO: support ::memcpy to local-tmp-var
        //"..\\..\\test\\compile\\dce3.c",
        //"..\\..\\test\\compile\\dce4.c",
        //    "..\\..\\test\\compile\\dce5.c",
        //"..\\..\\test\\compile\\dce6.c",
        //"..\\..\\test\\compile\\dce7.c",
        //"..\\..\\test\\compile\\dce8.c",
        //"..\\..\\test\\compile\\20020116-1.c",        
        //"..\\..\\test\\compile\\20020120-1.c",
        //"..\\..\\test2\\compile\\pr29250.c",
        //"..\\..\\test2\\compile\\20020304-1.c",
        //"..\\..\\test2\\compile\\du_chain2.c",
        //"..\\..\\test2\\compile\\linpack.c",
        //"..\\..\\test2\\compile\\linpack.c",
        //"..\\..\\test\\bigmethod\\array_and_compute_sensitive_code.c", //Disable CG's LRA.
        //"..\\..\\test2\\compile\\block.c",
        //"..\\..\\test2\\compile\\lib_headers.c",        
        //"..\\..\\test\\compile\\20020309-2.c",
        //"..\\..\\test\\compile\\20020304-1.c",
        //"..\\..\\test\\compile\\alias11.c",
        //"..\\..\\test\\compile\\ansic.c",
        "..\\..\\test\\compile\\cvt.c",
        "..\\..\\test\\compile\\alias10.c",
        "..\\..\\test\\compile\\20020309-2.c",
        "..\\..\\test\\compile\\array3.c",
        "..\\..\\test\\compile\\place_phi_pressure_test.c",
        "..\\..\\test\\exec\\inc_array.c",
        "..\\..\\test\\exec\\20041218-1.c",
        "..\\..\\test\\compile\\20020304-1.c",
        "..\\..\\test\\compile\\20020320-1.c",
        "..\\..\\test\\compile\\20020116-1.c",
        "..\\..\\test\\compile\\20020604-1.c",
        "..\\..\\test\\compile\\relation.c",
        "..\\..\\test\\exec\\20010915-1.c",
        "..\\..\\test\\compile\\20020116-1.c",
        //"..\\..\\test\\compile\\alias12.c",
        "..\\..\\test\\compile\\failed\\cfe\\forward_struct_decl.c",
        "..\\..\\test\\compile\\failed\\cfe\\forward_union_decl.c",
        "..\\..\\test\\compile\\failed\\cfe\\zero-strct-5.c",
        "..\\..\\test\\exec\\struct_decl.c",
        "..\\..\\test\\exec\\alias.c",        
#endif
        #ifdef _DEBUG_
        //"-dumpgr",
        
        #endif
        "-dump","tmp.log",
        "-O3",
        "-dce",
        "-prssa",
        "-mdssa",
        //"-time",
        //"-dump-aa",
        "-nocg",
        //"-dump-dumgr",
        //"-redirect_stdout_to_dump_file",
        //"-lower_to_pr_mode",
        //"-dump-cfg",
        //"-dump-dumgr",
        "-dump-all", //DUMP FrontEnd scope
        //"-dump-mdssamgr",
        //"-dump-dce",
        //"-dump-refine-duchain",
        //"-thres_opt_ir_num", "0xFFFFffff",
        //"-thres_opt_bb_num", "0xFFFFffff",
        //"-readgr", "d:\\x\\src\\reader\\test.gr",
        //"-readgr", "d:\\x\\test\\exec.gr\\inner_region.gr",
        //"-readgr", "d:\\x\\test\\exec.gr\\string.gr",
        //"-readgr", "d:\\x\\test\\exec.gr\\setelem.gr", //unsuppport on CG
        //"-readgr", "d:\\x\\test\\compile.gr\\array.gr",
        //"-readgr", "d:\\x\\test\\compile.gr\\cfg_opt.gr",
        //"-readgr", "d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.array_mode.gr",
        //"-readgr", "d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.set_key.gr",
        //"-readgr", "d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.pr_mode.gr",
        //"-readgr", "d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.encrypt.gr",
    };
    INT argc = sizeof(argv)/sizeof(argv[0]);
#else
INT main(INT argc, CHAR * argv[])
{
#endif
    if (!processCmdLine(argc, argv)) {
        return 1;
    }
    if (g_dump_file_name != NULL) {
        xoc::initdump(g_dump_file_name, true);
    }
    if (g_gr_file_name != NULL) {
        g_c_file_name = NULL;
        bool res = compileGRFile(g_gr_file_name);
        xoc::finidump();
        return res ? 0 : 2;
    } else if (g_c_file_name != NULL) {
        bool res = compileCFile();
        xoc::finidump();
        return res ? 0 : 3;
    }

    fprintf(stdout, "xocc.exe: no input files\n");
    fflush(stdout);
    return 4;
}
