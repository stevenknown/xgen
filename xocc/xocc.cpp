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
@*/
#include "xoccinc.h"

#undef DEBUG_XOC
#ifdef DEBUG_XOC
INT main(INT argcc, CHAR const* argvc[])
{
    DUMMYUSE(argcc);
    DUMMYUSE(argvc);
    CHAR const* argv[] = {
        "xocc.exe", "-o","a.asm", "-no-gra", "-dump","tmp.log", "-time",
        //"-dump-irid",
        /////////////
        //"-O0","-dump-ir2or",
        /////////////
        //"-prdu","-nonprdu","-prssa","-mdssa",
        // "-no-prdu","-no-nonprdu","-prssa","-mdssa",
        //-O3 -no-cp -no-cp_aggr -no-dce -no-dce_aggr -no-lftr -prssa -mdssa -prdu -nonprdu -licm -rp
        /////////////
        //"-O3","-prdu","-nonprdu","-mdssa","-prssa","-no-rce","-no-cp","-no-cp_aggr","-no-dce","-no-dce_aggr","-no-licm","-no-lftr","-no-rp","-rce",
        /////////////
        "-O3",//"-dump-all",
        //"-O3","-no-cg","-nonprdu","-prdu","-mdssa","-prssa","-dump-all",
        /////////////
        //"-O3","-no-dce","-no-dce_aggr","-no-lftr","-no-cfgopt","-no-cp","-no-cp_aggr","-no-licm","-no-rp","-prssa","-mdssa","-prdu","-nonprdu",
        /////////////
        //"-O3","-no-cg","-no-lsra",
        /////////////
        //"-O3","-only-licm","-no-cg","-dump-licm",
        /////////////
        //"-O0","-dump-ir2or","-cg_for_inner_region",
        /////////////
        //"-O3","-lowest_height",
        /////////////
        //"-O3","-prdu","-nonprdu","-mdssa","-prssa","-no-rce","-no-cp","-no-cp_aggr","-no-dce","-no-dce_aggr","-no-licm","-no-lftr","-no-rp","-dce","-licm",//"-dump-all",
        /////////////
        //"-O3","-only-licm","-dce_aggr","-cp_aggr","-cfgopt","-lowest_height",
        /////////////
        //"-dump-all",
        //"-O3","-only-licm","-cfgopt",
        /////////////
        //"-dump-all",
        //"-O3","-only-licm","-dce_aggr","-cfgopt","-infer_type","-rce",
        /////////////
        //"-no-cg","-only-licm","-rce","-cp",
        /////////////
        //"-no-cg","-only-licm","-rce",
        /////////////
        //"-no-cg","-lowest_height","-prdu", "-nonprdu","-no-prssa","-only-licm","-rce","-rp","-dce","-dce_aggr","-cp_aggr","-cfgopt",
        /////////////
        //"-no-cg","-prdu", "-nonprdu","-no-prssa","-only-licm","-rce","-rp","-cp_aggr",
        /////////////
        //"-O3","-no-prssa","-no-mdssa","-prdu","-nonprdu","-only-lsra","-no-cg","-dump-all",
        /////////////
        //"-O3","-only-refine_duchain","-prdu","-nonprdu","-no-mdssa","-no-prssa","-dump-refine_duchain","-dump-dumgr","-dump-aa","-dump-irid",
        /////////////
        //"-O3","-no-prssa","-no-mdssa","-prdu","-nonprdu","-no-cg","-dump-all",
        /////////////
        //"-dump-dumgr","-dump-aa","-only-licm","-prdu", "-nonprdu",
        ////====================================================================
        "..\\..\\test\\compile.gr\\lsra_call.gr",
        "..\\..\\test\\exec\\c4.c",
        "..\\..\\test\\compile\\cdg.c",
        "..\\..\\test\\compile\\gvn1.c",
        "..\\..\\test\\compile\\ansic.c",
        "..\\..\\test\\compile.gr\\solidus.gr",
        "..\\..\\test\\compile.gr\\insert_preheader.gr",
        "..\\..\\test\\compile\\alias_stack.c",
        "..\\..\\test\\compile\\alias_local_call.c",
        "..\\..\\test\\compile\\alias_local.c",
        "..\\..\\test\\compile\\licm_undocfg2.c",
        "..\\..\\test\\compile\\lib_headers.c",
        "..\\..\\test\\compile.gr\\aa.gr",
        "..\\..\\test\\compile\\refine_duchain3.c",
        "..\\..\\test\\compile\\refine_duchain2.c",
        "..\\..\\test\\compile\\cfg_rmemptybb.c",
        "..\\..\\test\\compile\\block.c",
        "..\\..\\test\\compile\\du_local.c",
        "..\\..\\test\\compile\\alias.loop.c",
        "..\\..\\test\\compile\\alias2_1.c",
        "..\\..\\test\\compile\\alias2.c",
        "..\\..\\test\\compile\\lib_headers.c",
        "..\\..\\test\\compile.gr\\inner_func.gr",
        "..\\..\\test\\compile\\rp3.c",
        "..\\..\\test\\compile\\dce2.c",
        "..\\..\\test\\compile\\gcse.c",
        "..\\..\\test\\compile\\20020314-1.c",
        "..\\..\\test\\compile\\licm_hoist.c",
        "..\\..\\test\\compile\\licm_updatemdssa2.c",
        "..\\..\\test\\compile\\licm_updatemdssa.c",
        "..\\..\\test\\compile\\matmul.c",
        "..\\..\\test\\compile\\mdssa_renamedef.c",
        "..\\..\\test\\exec.gr\\qsort.gr",
        "..\\..\\test\\compile\\licm_undocfg.c",
        "..\\..\\test\\compile\\dce_phi_prevdef.c",
        "..\\..\\test\\compile\\rce_removeedge.c",
        "..\\..\\test\\compile\\rce_updatedom5.c",
        "..\\..\\test\\compile\\cfg_trampo.c",
        "..\\..\\test\\compile\\rce3.c",
        "..\\..\\test\\compile\\rce2.c",
        "..\\..\\test\\compile\\alias_union.c",
        "..\\..\\test\\compile\\alias_global2.c",
        "..\\..\\test\\compile\\rce_updatedom4.c",
        "..\\..\\test\\compile\\cfg_updatessa.c",
        "..\\..\\test\\compile\\rce_updatedom3.c",
        "..\\..\\test\\compile\\rce_updatemdssa.c",
        "..\\..\\test\\compile\\licm_updatepdom3.c",
        "..\\..\\test\\compile\\licm_undo.c",
        "..\\..\\test\\compile\\licm_updatedom.c",
        "..\\..\\test\\compile.gr\\mdssa_revise.gr",
        //"..\\..\\test\\compile\\rce_updatedom2.c",
        //"..\\..\\test\\compile\\20020604-1.c",
        //"..\\..\\test\\compile\\licm_updaterpo.c",
        //"..\\..\\test\\compile\\llvm_not_optimized.c",
        //"..\\..\\test\\compile\\licm_mdssa5.c",
        //"..\\..\\test\\compile\\licm4.c",
        //"..\\..\\test\\compile\\rce_updateloopinfo.c",
        //"..\\..\\test\\compile\\licm6.c",
        //"..\\..\\test\\compile\\licm_dce.c",
        //"..\\..\\test\\compile\\prssa_duset.c",
        //"..\\..\\test\\compile\\20020116-1.c",
        //"..\\..\\test\\exec\\array_array.c",
        //"..\\..\\test\\compile.gr\\encrypt.gr",
        //"..\\..\\test\\compile.gr\\ssa_common_livein.gr",
        //"..\\..\\test\\compile.gr\\label.gr",
        //"..\\..\\test\\compile\\lftr_iv.c",
        //"..\\..\\test\\exec\\a7_1.c",
        //"..\\..\\test\\compile\\preheader.c",
        //"..\\..\\test\\compile\\licm_mdssa3.c",
        //"..\\..\\test\\compile\\20020304-1.c",
        //"..\\..\\test\\compile\\cdg.c",
        //"..\\..\\test\\compile\\lftr_var_coeff.c",
        //"..\\..\\test\\compile\\param6.c",
        //"..\\..\\test\\compile\\calls.c",
        //"..\\..\\test\\compile\\20020604-1.c",
        //"..\\..\\test\\compile\\20020116-1.c",
        //"..\\..\\test\\compile\\licm_ignorephi.c",
        //"..\\..\\test\\compile\\gvn_ptr.c",
        //"..\\..\\test\\compile\\loop.c",
        //"..\\..\\test\\compile\\rce_cfgopt.c",
        //"..\\..\\test\\compile\\update_rpo2.c",
        //"..\\..\\test\\compile\\rp_exactref2.c",
        //"..\\..\\test\\compile\\rp_exactref.c",
        //"..\\..\\test\\compile\\20020110.c",
        //"..\\..\\test\\compile\\dom.c",
        //"..\\..\\test\\compile\\licm.c",
        //"..\\..\\test\\compile\\dce16.c",
        //"..\\..\\test\\compile\\licm_revert.c",
        //"..\\..\\test\\compile\\du_coalesce.c",
        //"..\\..\\test\\compile\\licm_prssa.c",
        //"..\\..\\test\\compile\\licm_mdssa.c",   //"-O3","-prdu", "-nonprdu","-only-licm","-lowest_height",
        //"..\\..\\test\\compile\\licm_mdssa3.c",  //"-O3", "-only-licm","-lowest_height","
        //"..\\..\\test\\compile\\licm_mdssa3.c",  //"-O3", "-only-licm","-lowest_height",
        //"..\\..\\test\\compile\\alias_ptr_arith.c",
        //"..\\..\\test\\compile\\refine2.c",
        //"..\\..\\test\\compile\\cp.c",
        //"..\\..\\test\\exec\\alias.c",
        //"..\\..\\test\\compile\\rp13.c",//TODO!!!!!
        //"..\\..\\test\\compile\\update_rpo3.c",
        //"..\\..\\test\\compile\\alias_global.c",
        //"..\\..\\test\\compile\\update_rpo.c",
        //"..\\..\\test\\compile\\insert_guard.c",
        //"..\\..\\test\\exec\\11200.c",
        //"..\\..\\test\\compile\\alias11.c",
        //"..\\..\\test\\compile\\20020604-1.c",
        //"..\\..\\test\\compile\\20020304-1.c",
        //"..\\..\\test\\compile\\licm2_2.c",
        //"..\\..\\test\\exec\\20000503-1.c",
        //"..\\..\\test\\compile\\alias13.c",
        //"..\\..\\test\\compile\\licm2.c",
        //"..\\..\\test\\compile\\dce_dominfo.c",
        //"..\\..\\test\\compile\\cp3.c",
        //"..\\..\\test\\compile\\lftr.c",
        //"..\\..\\test\\compile.gr\\guard.gr",
        //"..\\..\\test\\compile.gr\\rce_dom.gr",
        //"..\\..\\test\\compile.gr\\licm_emptyloophead.gr",
        //"..\\..\\test\\compile.gr\\cfg_removebb.gr",
        //"..\\..\\test\\compile.gr\\alias_ptr_arith.gr",
        //"..\\..\\test\\compile.gr\\inferty.gr",
        //"..\\..\\test\\compile.gr\\du_help.gr",
        //
        //"..\\..\\test\\compile.gr\\lsra1.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lsra_move3.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lsra_move2.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lsra_move.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lsra2.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lsra_split.gr", //-no-cg
        //"..\\..\\test\\compile\\licm_mdssa2.c",  //"-lowest_height","-no-cg","-no-prssa",
        //"..\\..\\test\\compile.gr\\lsra_move4.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\licm.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lt4.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lt3.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lt2.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\lt.gr", //-no-cg
        //"..\\..\\test\\compile.gr\\rp2_2.gr", //-no-cg
        ////====================================================================
        //"-O0",
        //"-O3",
        //"-licm",
        //"-cp_aggr",
        //"-cfgopt",
        //"-dce_aggr",
        //"-no-cg",
        //"-vrp",
        //"-dump-vrp",
        //"-no-dce",
        //"-no-dce_aggr",
        //"-O0",
        //"-licm",
        //"-dump-licm",
        //"-prdu",
        //"-nonprdu",
        //"-O3",
        //"-lowest_height",
        //"-no-cg",
        //"-no-prssa",
        //"-O3",
        //"-only-cfgopt",
        //"-licm",
        //"-infer_type",
        //"-cg",
        //"-dump-ir2or",
        //"-cg_for_inner_region",
        //"-dump-aa",
        //"-only-rp",
        //"-no-cg",
        //"-no-prssa",
        //"-dce",
        //"-dce_aggr",
        //"-only-infer_type",
        //"-rce",
        //"-rp",
        //"-only-cp",
        //"-cp_aggr",
        //"-licm",
        //"-dce",
        //"-dce_aggr",
        //"-only-rce",
        //"-O3",
        //"-rce",
        //"-cp",
        //"-cfgopt",
        //"-refine_duchain",
        //"-lftr",
        //"-refine_duchain",
        //"-dce_aggr",
        //"-dce",
        //"-rp",
        //"-no-mdssa","-no-prssa",
        //"-cp",
        //"-rp",
        //"-prdu","-nonprdu",
        //    "-ipa",
        //    "-vrp",
        //"..\\..\\test\\compile.gr\\redundant_label.gr", "-no-cg",
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
        //"..\\..\\test\\compile\\dce.c", //fail in CG, TODO: support ::memcpy to local-tmp-var
        //"..\\..\\test\\bigmethod\\array_and_compute_sensitive_code.c", //Disable CG's LRA.
        //"..\\..\\test\\compile\\20020604-1.c.i", //why it crashed? It has passed in home test.
        //"..\\..\\test\\compile\\20020604-1.c", //why it crashed? It has passed in home test.
        //"..\\..\\test\\compile\\alias10.c.i", //MDPhi info is not correct.
        //"..\\..\\test\\compile\\tree-ssa\\20030711-2.c.i", //TO CHECK
        //"..\\..\\test\\compile\\tmp\\quickjs_preprocess.c", //! TODO
        //"..\\..\\test\\exec\\array_and_pointer.c", EXEC FAILED!!
        //"..\\..\\test\\exec\\ivr.c",!
        //"..\\..\\test\\exec\\rce2.c",!
        //"..\\..\\test\\exec\\rce.c",!
        //"..\\..\\test\\compile\\load.offset.c",!
        //"-thres_opt_ir_num", "0xFFFFffff",
        //"-thres_opt_bb_num", "0xFFFFffff",
    };
    INT argc = sizeof(argv)/sizeof(argv[0]);
#else
INT main(INT argc, CHAR const* argv[])
{
#endif
    if (!xocc::processCmdLine(argc, argv)) {
        return 1;
    }
    if (xocc::g_cfile_list.get_elem_count() == 0 &&
        xocc::g_grfile_list.get_elem_count() == 0) {
        fprintf(stdout, "xocc.exe: no input files\n");
        fflush(stdout);
        return 4;
    }
    bool compile_failed = false;
    if (!xocc::compileGRFileList()) {
        compile_failed = true;
    }
    if (!xocc::compileCFileList()) {
        compile_failed = true;
    }
    return compile_failed ? 3 : 0;
}
