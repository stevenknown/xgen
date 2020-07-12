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

//#undef DEBUG_XOC
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
        "..\\..\\test\\compile\\rp.c",
#endif
        #ifdef _DEBUG_
        //"-dumpgr",
        
        #endif
        "-dump","tmp.log",
        "-O3",
        //"-dce",
        "-rp",
        "-rce",
        "-dump-rp",
        "-prssa",
        "-prdu",        
        "-nonprdu",
        //"-prmode",
        "-mdssa",
        "-time",
        //"-dump-aa",
        "-nocg",
        "-dumpgr",
        //"-redirect_stdout_to_dump_file",
        //"-lower_to_pr_mode",
        //"-dump-cfg",
        //"-dump-dumgr",
        //"-dump-all", //DUMP FrontEnd scope
        "-dump-mdssamgr",
        "-dump-dce",
        //"-dump-refine-duchain",
        //"-thres_opt_ir_num", "0xFFFFffff",
        //"-thres_opt_bb_num", "0xFFFFffff",

        //"d:\\x\\src\\reader\\test.gr",
        //"d:\\x\\test\\exec.gr\\inner_region.gr",
        //"d:\\x\\test\\exec.gr\\string.gr",
        //"d:\\x\\test\\exec.gr\\setelem.gr", //unsuppport on CG
        //"d:\\x\\test\\compile.gr\\array.gr",
        //"d:\\x\\test\\compile.gr\\cfg_opt.gr",
   
        //"d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.set_key.gr",
        //"d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.pr_mode.gr",
        //"d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.encrypt.gr",
        //"d:\\x\\test\\compile.gr\\ssa.gr",
        //"d:\\x\\test\\compile.gr\\mdssa.gr",
        //"d:\\x\\test\\compile.gr\\ssa2.gr",
        //"d:\\x\\test\\compile.gr\\alias_loop_carry.gr",
        //"d:\\x\\test\\compile.gr\\rce.gr",
        //"d:\\x\\test\\compile.gr\\array_and_compute_sensitive_code.array_mode.gr",
        //"d:\\x\\test\\compile.gr\\rp.gr",
        //"d:\\x\\test\\compile.gr\\rp2.gr",
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
        //If both GR and C file are input, prefer GR file.
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
