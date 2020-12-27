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
#include "../xgen/xgeninc.h"
#include "errno.h"
#include "../opt/util.h"
#include "../reader/grreader.h"
#include "../opt/comopt.h"

//#define LR0_FE

CHAR * g_c_file_name = nullptr;
CHAR * g_gr_file_name = nullptr;
CHAR * g_dump_file_name = nullptr;
static CHAR * g_output_file_name = nullptr;
bool g_is_dumpgr = false;
static CHAR const* g_xocc_version = "1.0.0";

//Record the start position of formal parameter.
//The position 0 is reserved for hidden parameter which used for
//structure return value in C/C++.
UINT const g_formal_parameter_start = 1;

//Print assembly in horizontal manner.
bool g_prt_asm_horizontal = true;

static TMap<Decl*, Var*> g_decl2var_map;
static TMap<Var*, Decl*> g_var2decl_map;

#ifdef _DEBUG_
void resetMapBetweenVARandDecl(Var * v)
{
    Decl * decl = g_var2decl_map.get(v);
    if (decl != nullptr) {
        g_decl2var_map.setAlways(decl, nullptr);
    }
    g_var2decl_map.setAlways(v, nullptr);
}
#endif


//
//START DbxMgr
//
INT report_location(CHAR const* file, INT line)
{
    printf("\n\n\n%s : %d", file, line);
    return 0;
}

#ifdef _DEBUG_
#define WARN(parmlist) (report_location(__FILE__, __LINE__), \
                        xoc::interwarn parmlist)
#else
#define WARN(parmlist)
#endif

void CLDbxMgr::printSrcLine(Dbx const* dbx, PrtCtx * ctx)
{
    ASSERT0(ctx && dbx);
    if (ctx->logmgr == nullptr) { return; }

    UINT lineno = getLineNum(dbx);
    if (lineno == m_cur_lineno) {
        //It is dispensable that print the same souce file multiple times.
        return;
    }

    m_cur_lineno = lineno;
    if (lineno == 0) {
        //No line number info recorded.
        if (ctx != nullptr && ctx->prefix != nullptr) {
            note(ctx->logmgr, "\n%s[0]\n", ctx->prefix);
        } else {
            note(ctx->logmgr, "\n[0]\n");
        }
        return;
    }

    if (g_hsrc != nullptr) {
        UINT srcline = mapRealLineToSrcLine(m_cur_lineno);
        if (srcline == 0) {
            srcline = m_cur_lineno;
        }
        ASSERTN(srcline < OFST_TAB_LINE_SIZE, ("unexpected src line"));
        fseek(g_hsrc, g_ofst_tab[srcline], SEEK_SET);
        if (fgets(g_cur_line, g_cur_line_len, g_hsrc) != nullptr) {
            if (ctx != nullptr && ctx->prefix != nullptr) {
                note(ctx->logmgr, "\n\n%s[%u]%s",
                     ctx->prefix, m_cur_lineno, g_cur_line);
            } else {
                note(ctx->logmgr, "\n\n[%u]%s", m_cur_lineno, g_cur_line);
            }
        }
    }
}


void CLDbxMgr::printSrcLine(xcom::StrBuf & output, Dbx const* dbx, PrtCtx * ctx)
{
    ASSERT0(ctx && dbx);
    UINT lineno = getLineNum(dbx);
    if (lineno == m_cur_lineno) {
        //It is dispensable that print the same souce file multiple times.
        return;
    }
    m_cur_lineno = lineno;
    if (lineno == 0) {
        //No line number info recorded.
        if (ctx != nullptr && ctx->prefix != nullptr) {
            output.strcat("\n%s[0]\n", ctx->prefix);
        } else {
            output.strcat("\n[0]\n");
        }
        return;
    }

    if (g_hsrc != nullptr) {
        UINT srcline = mapRealLineToSrcLine(m_cur_lineno);
        if (srcline == 0) {
            srcline = m_cur_lineno;
        }
        ASSERTN(srcline < OFST_TAB_LINE_SIZE, ("unexpected src line"));
        fseek(g_hsrc, g_ofst_tab[srcline], SEEK_SET);
        if (fgets(g_cur_line, g_cur_line_len, g_hsrc) != nullptr) {
            if (ctx != nullptr && ctx->prefix != nullptr) {
                output.strcat("\n\n%s[%u]%s", ctx->prefix,
                              m_cur_lineno, g_cur_line);
            } else {
                output.strcat("\n\n[%u]%s", m_cur_lineno, g_cur_line);
            }
        }
    }
}
//END DbxMgr


static void usage()
{
    fprintf(stdout,
    "\nXOCC Version %s"
    "\nUsage: xocc [options] file"
    "\nOptions: "
    "\n  -O0,           compile without any optimization"
    "\n  -O1,-O2,-O3    compile with optimizations"
    "\n  -dump <file>   generate dump file and enable dump"
    "\n  -o <file>      output file name"
    "\n  -gra=<on|off>  switch for global register allocation"
    "\n  -ipa           enable IPA"
    "\n  -dumpgr        dump GR file"
    "\n  -prmode        simplify to the lowest IR"
    "\n  -readgr <file> read GR file"
    "\n  -nolicm        disable loop invariant code motion optimization"
    "\n  -nodce         disable dead code elimination optimization"
    "\n  -norp          disable register promotion optimization"
    "\n  -nocp          disable copy propagation optimization"
    "\n  -nogcse        disable global common subexpression elimination optimization"
    "\n  -nolcse        disable local common subexpression elimination optimization"
    "\n", g_xocc_version);
}


static bool is_c_source_file(CHAR const* fn)
{
    UINT len = (UINT)strlen(fn) + 1;
    CHAR * buf = (CHAR*)ALLOCA(len);
    upper(getfilesuffix(fn, buf, (UINT)len));
    if (strcmp(buf, "C") == 0 ||
        strcmp(buf, "I") == 0) {
        return true;
    }
    return false;
}


static bool is_gr_source_file(CHAR const* fn)
{
    UINT len = (UINT)strlen(fn) + 2;
    CHAR * buf = (CHAR*)ALLOCA(len);
    upper(getfilesuffix(fn, buf, len));
    if (strcmp(buf, "GR") == 0 ||
        strcmp(buf, "I") == 0) {
        return true;
    }
    return false;
}


static bool process_O(INT argc, CHAR * argv[], INT & i)
{
    DUMMYUSE(argc);
    CHAR * cmdstr = &argv[i][1];
    switch (cmdstr[1]) {
    case '0':
        xoc::g_opt_level = OPT_LEVEL0;
        break;
    case '1':
        xoc::g_opt_level = OPT_LEVEL1;
        xoc::g_do_pr_ssa = true;
        xoc::g_do_md_ssa = true;
        //Only do refinement.
        break;
    case '2':
        xoc::g_opt_level = OPT_LEVEL2;        
        xoc::g_do_dce = true;
        xoc::g_do_licm = true;
        xoc::g_do_rp = true;
        xoc::g_do_pr_ssa = true;
        xoc::g_do_md_ssa = true;
        break;    
    case '3':
        xoc::g_opt_level = OPT_LEVEL3;
        xoc::g_do_dce = true;
        xoc::g_do_licm = true;
        xoc::g_do_rp = true;
        xoc::g_do_pr_ssa = true;
        xoc::g_do_md_ssa = true;
        break;
    case 's':
    case 'S':
        xoc::g_opt_level = SIZE_OPT;
        xoc::g_do_dce = true;
        xoc::g_do_licm = true;
        xoc::g_do_rp = true;
        xoc::g_do_pr_ssa = true;
        xoc::g_do_md_ssa = true;
        break;
    default:
        xoc::g_opt_level = OPT_LEVEL1;
        xoc::g_do_pr_ssa = true;
        xoc::g_do_md_ssa = true;
    }
    i++;
    return true;
}


static bool process_o(INT argc, CHAR * argv[], INT & i)
{
    if (i + 1 < argc && argv[i + 1] != nullptr) {
        g_output_file_name = argv[i + 1];
    }
    i += 2;
    return true;
}



static CHAR * process_d(INT argc, CHAR * argv[], INT & i)
{
    CHAR * n = nullptr;
    if (i + 1 < argc && argv[i + 1] != nullptr) {
        n = argv[i + 1];
    }
    i += 2;
    return n;
}


static bool process_thres_opt_bb_num(INT argc, CHAR * argv[], INT & i)
{
    CHAR * n = nullptr;
    if (i + 1 < argc && argv[i + 1] != nullptr) {
        n = argv[i + 1];
    }
    if (n == nullptr) { return false; } 
    xoc::g_thres_opt_bb_num = (UINT)xcom::xatoll(n, false);
    i += 2;
    return true;
}


static bool process_thres_opt_ir_num(INT argc, CHAR * argv[], INT & i)
{
    CHAR * n = nullptr;
    if (i + 1 < argc && argv[i + 1] != nullptr) {
        n = argv[i + 1];
    }
    if (n == nullptr) { return false; } 
    xoc::g_thres_opt_ir_num = (UINT)xcom::xatoll(n, false);
    i += 2;
    return true;
}


static bool process_g(INT argc, CHAR * argv[], INT & i)
{
    DUMMYUSE(argc);
    CHAR * cmdstr = &argv[i][1];
    if (xstrcmp(cmdstr, "gra=", 4) != 0) {
        return false;
    }
    cmdstr += 4;
    if (strcmp(cmdstr, "on") == 0) {
        i++;
        g_do_gra = true;
        return true;
    } else if (strcmp(cmdstr, "off") == 0) {
        i++;
        g_do_gra = false;
        return true;
    }
    return true;
}


static bool process_i(INT argc, CHAR * argv[], INT & i)
{
    DUMMYUSE(argc);
    CHAR * cmdstr = &argv[i][1];
    if (xstrcmp(cmdstr, "ipa", 3) != 0) {
        return 1;
    }
    i++;
    g_do_ipa = true;
    return 0;
}


static bool processOneLevelCmdLine(INT argc, CHAR * argv[], INT & i)
{
    CHAR const* cmdstr = &argv[i][1];
    if (cmdstr[0] == 'O') {
        if (!process_O(argc, argv, i)) {
            return false;
        }
    } else if (cmdstr[0] == 'o') {
        if (!process_o(argc, argv, i)) {
            return false;
        }
    } else if (cmdstr[0] == 'g') {
        if (!process_g(argc, argv, i)) {
            return false;
        }
    } else if (cmdstr[0] == 'i') {
        if (!process_i(argc, argv, i)) {
            return false;
        }
    } else if (!strcmp(cmdstr, "time")) {
        g_show_time = true;
        i++;
    } else if (!strcmp(cmdstr, "dump")) {
        CHAR * n = process_d(argc, argv, i);
        if (n == nullptr) {
            return false;
        }
        g_dump_file_name = n;
    } else if (!strcmp(cmdstr, "nodce")) {
        g_do_dce = false;
        i++;
    } else if (!strcmp(cmdstr, "licm")) {
        g_do_licm = true;
        i++;
    } else if (!strcmp(cmdstr, "rp")) {
        g_do_rp = true;
        i++;
    } else if (!strcmp(cmdstr, "cp")) {
        g_do_cp = true;
        i++;
    } else if (!strcmp(cmdstr, "rce")) {
        g_do_rce = true;
        i++;            
    } else if (!strcmp(cmdstr, "dce")) {
        g_do_dce = true;
        i++;
    } else if (!strcmp(cmdstr, "nocg")) {
        g_do_cg = false;
        i++;
    } else if (!strcmp(cmdstr, "mdssa")) {
        g_do_md_ssa = true;
        i++;
    } else if (!strcmp(cmdstr, "prmode")) {
        g_is_lower_to_pr_mode = true;
        i++;
    } else if (!strcmp(cmdstr, "prdu")) {
        g_compute_pr_du_chain = true;
        i++;
    } else if (!strcmp(cmdstr, "nonprdu")) {
        g_compute_nonpr_du_chain = true;
        i++;
    } else if (!strcmp(cmdstr, "prssa")) {
        g_do_pr_ssa = true;
        i++;
    } else if (!strcmp(cmdstr, "redirect_stdout_to_dump_file")) {
        g_redirect_stdout_to_dump_file = true;
        i++;
    } else if (!strcmp(cmdstr, "lower_to_pr_mode")) {
        g_is_lower_to_pr_mode = true;
        i++;
    } else if (!strcmp(cmdstr, "schedule_delay_slot")) {
        g_enable_schedule_delay_slot = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-cfg")) {
        g_dump_opt.is_dump_cfg = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-lis")) {
        g_dump_opt.is_dump_lis = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-aa")) {
        g_dump_opt.is_dump_aa = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-dumgr")) {
        g_dump_opt.is_dump_dumgr = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-prssamgr")) {
        g_dump_opt.is_dump_prssamgr = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-mdssamgr")) {
        g_dump_opt.is_dump_mdssamgr = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-dce")) {
        g_dump_opt.is_dump_dce = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-rp")) {
        g_dump_opt.is_dump_rp = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-licm")) {
        g_dump_opt.is_dump_licm = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-rce")) {
        g_dump_opt.is_dump_rce = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-ra")) {
        g_dump_opt.is_dump_ra = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-cg")) {
        g_dump_opt.is_dump_cg = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-cdg")) {
        g_dump_opt.is_dump_cdg = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-simplification")) {
        g_dump_opt.is_dump_simplification = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-refine-duchain")) {
        g_dump_opt.is_dump_refine_duchain = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-gvn")) {
        g_dump_opt.is_dump_gvn = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-all")) {
        g_dump_opt.is_dump_all = true;
        i++;
    } else if (!strcmp(cmdstr, "dump-nothing")) {
        g_dump_opt.is_dump_nothing = true;
        i++;
    } else if (!strcmp(cmdstr, "thres_opt_ir_num")) {
        if (!process_thres_opt_ir_num(argc, argv, i)) {
            return false;
        }
    } else if (!strcmp(cmdstr, "thres_opt_bb_num")) {
        if (!process_thres_opt_bb_num(argc, argv, i)) {
            return false;
        }
    } else if (!strcmp(cmdstr, "dumpgr")) {
        g_is_dumpgr = true;
        i++;
    } else {
        return false;
    }
    return true;
}


bool processCmdLine(INT argc, CHAR * argv[])
{
    if (argc <= 1) { usage(); return false; }

    INT i = 1;
    while (i < argc) {
        if (argv[i][0] == '-') {
            if (!processOneLevelCmdLine(argc, argv, i)) {
                usage();
                return false; 
            }
            continue;
        }

        if (is_c_source_file(argv[i])) {
            g_c_file_name = argv[i];
            i++;
            continue;
        }

        if (is_gr_source_file(argv[i])) {
            g_gr_file_name = argv[i];
            i++;
            continue;
        }

        //Unsupport option.
        usage();
        return false;
    }

    if (g_c_file_name != nullptr) {
        g_hsrc = fopen(g_c_file_name, "rb");
        if (g_hsrc == nullptr) {
            fprintf(stdout, "xoc: cannot open %s, error information is %s\n",
                            g_c_file_name, strerror(errno));
            return false;
        }
    }
    if (g_output_file_name != nullptr) {
        UNLINK(g_output_file_name);
    }
    if (g_opt_level == OPT_LEVEL0) {
        g_do_cfg_remove_empty_bb = false;
        g_do_cfg_remove_unreach_bb = false;
        g_do_cfg_remove_trampolin_bb = false;
        g_do_cfg_remove_unreach_bb = false;
        g_do_cfg_remove_trampolin_bb = false;
        g_do_cfg_dom = false;
        g_do_cfg_pdom = false;
        g_do_loop_ana = false;
        g_do_cdg = false;
        g_do_cfg_remove_redundant_branch = false;
        g_do_cfg_invert_condition_and_remove_trampolin_bb = false;
        g_do_aa = false;
        g_do_md_du_analysis = false;
        g_is_support_dynamic_type = false;
        g_do_md_ssa = false;
        g_do_pr_ssa = false;
        g_compute_pr_du_chain = false;
        g_compute_nonpr_du_chain = false;        
        g_do_refine = false;
        g_do_refine_auto_insert_cvt = true;
        g_do_call_graph = false;
        g_do_ipa = false;
    }
    return true;
}


Var * mapDecl2VAR(Decl * decl)
{
    return g_decl2var_map.get(decl);
}


Decl * mapVAR2Decl(Var * var)
{
    return g_var2decl_map.get(var);
}


//Transforming Decl into Var.
//'decl': variable declaration in front end.
static Var * addDecl(IN Decl * decl, IN OUT VarMgr * var_mgr, TypeMgr * dm)
{
    ASSERTN(DECL_dt(decl) == DCL_DECLARATION, ("unsupported"));
    UINT flag = 0;
    if (is_global_variable(decl)) {
        ASSERT0(!is_local_variable(decl));
        SET_FLAG(flag, VAR_GLOBAL);
    } else if (is_local_variable(decl)) {
        SET_FLAG(flag, VAR_LOCAL);
    } else {
        UNREACHABLE();
    }

    if (is_static(decl)) {
        SET_FLAG(flag, VAR_PRIVATE);
    }

    if (is_constant(decl)) {
        SET_FLAG(flag, VAR_READONLY);
    }

    if (is_volatile(decl)) {
        SET_FLAG(flag, VAR_VOLATILE);
    }

    if (is_restrict(decl)) {
        SET_FLAG(flag, VAR_IS_RESTRICT);
    }

    if (is_initialized(decl)) {
        //TODO: record initial value in Var at CTree2IR.
        //SET_FLAG(flag, VAR_HAS_INIT_VAL);
    }

    UINT data_size = 0;

    ASSERT0(dm);

    DATA_TYPE data_type = get_decl_dtype(decl, &data_size, dm);
    Type const* type = nullptr;
    if (IS_PTR(data_type)) {
        ASSERT0(is_pointer(decl));
        UINT basesize = get_pointer_base_size(decl);

        //Note: If pointer_base_size is 0, then the pointer can not
        //do any operation, such as pointer arithmetic.
        //ASSERTN(basesize > 0, ("meet incomplete type."));

        type = dm->getPointerType(basesize);
    } else if (IS_MC(data_type)) {
        //Is data_size likely to be 0?
        type = dm->getMCType(data_size);
    } else {
        //data_size must definitly equal to corresponding size.
        ASSERT0(data_size == dm->getDTypeByteSize(data_type));
        type = dm->getSimplexTypeEx(data_type);
    }

    if (DECL_is_formal_para(decl)) {
        //Declaration is formal parameter.
        SET_FLAG(flag, VAR_IS_FORMAL_PARAM);
    }

    if (is_array(decl)) {
        SET_FLAG(flag, VAR_IS_ARRAY);
    }

    ASSERT0(type);

    CHAR * var_name = SYM_name(get_decl_sym(decl));
    Var * v = var_mgr->registerVar(var_name, type,
                                   (UINT)xcom::ceil_align(MAX(DECL_align(decl),
                                                          STACK_ALIGNMENT),
                                   STACK_ALIGNMENT), flag);
    if (VAR_is_global(v)) {
        //For conservative purpose.
        VAR_is_addr_taken(v) = true;
    }

    if (DECL_is_formal_para(decl)) {
        VAR_formal_param_pos(v) =
            g_formal_parameter_start + DECL_formal_param_pos(decl);
    }

    g_decl2var_map.setAlways(decl, v);
    g_var2decl_map.setAlways(v, decl);
    return v;
}


//Constructing a variable table to map each of DECLARATIONs to an unique Var.
//Scanning Scope trees to construct such table.
//e.g: Scope tree
//    SCOPE0
//      --SCOPE1
//          --SCOPE2
//          --SCOPE3
//      --SCOPE4
static void scanAndInitVar(Scope * s, VarMgr * vm, TypeMgr * tm)
{
    if (s == nullptr) { return; }
    do {
        for (Decl * decl = SCOPE_decl_list(s);
             decl != nullptr; decl = DECL_next(decl)) {
            ASSERT0(DECL_decl_scope(decl) == s);
            if (is_fun_decl(decl)) {
                //Function declaration decl.
                if (DECL_is_fun_def(decl)) {
                    //Function definition
                    ASSERT0(DECL_decl_scope(decl) == s);
                    Var * v = addDecl(decl, vm, tm);
                    VAR_is_func_decl(v) = true;
                } else {
                    //Function declaration
                    if (mapDecl2VAR(decl) == nullptr) {
                        Var * v = addDecl(decl, vm, tm);

                        //Function declaration should not be fake, since
                        //it might be taken address.
                        //e.g: extern void foo();
                        //  void * p = &foo;
                        //VAR_is_fake(v) = true;

                        VAR_is_func_decl(v) = true;
                    } else {
                        //Function/variable declaration, can not
                        //override real function define.
                    }
                    continue;
                }
                continue;
            }

            //General variable declaration decl.
            if (mapDecl2VAR(decl) == nullptr &&
                !(DECL_is_formal_para(decl) && get_decl_sym(decl) == nullptr)) {
                //No need to generate Var for parameter that does not
                //have a name.
                //e.g: parameter of foo(char*)
                addDecl(decl, vm, tm);
            }
        }

        scanAndInitVar(SCOPE_sub(s), vm, tm);
        s = SCOPE_nsibling(s);
    } while (s != nullptr);
}


UINT FrontEnd(RegionMgr * rm)
{
    START_TIMER(t, "CFE");
    setLogMgr(rm->getLogMgr());
    initTypeTran();

    INT s = ST_SUCC;
#ifdef LR0_FE
    init_rule_info();
    reduce();
#else
    s = Parser();
    if (s != ST_SUCC) {
        END_TIMER(t, "CFE");
        return s;
    }
#endif
    s = processDeclInit();
    if (s != ST_SUCC) {
        END_TIMER(t, "CFE");
        return s;
    }

    s = TypeTransform();
    if (s != ST_SUCC) {
        END_TIMER(t, "CFE");
        return s;
    }

    s = TypeCheck();
    if (s != ST_SUCC) {
        END_TIMER(t, "CFE");
        return s;
    }

    END_TIMER(t, "CFE");
    return ST_SUCC;
}


static FILE * createAsmFileHandler()
{
    FILE * asmh = nullptr;
    if (g_output_file_name != nullptr) {
        asmh = fopen(g_output_file_name, "a+");
        if (asmh == nullptr) {
            xoc::prt2C("Can not create assembly file %s", g_output_file_name);
        }
        return asmh;
    }

    StrBuf buf(128);
    ASSERT0(g_c_file_name || g_gr_file_name);
    buf.sprint("%s.asm", g_c_file_name != nullptr ?
                         g_c_file_name : g_gr_file_name);
    asmh = fopen(buf.buf, "a+");
    if (asmh == nullptr) {
        xoc::prt2C("Can not create assembly file %s", buf.buf);
    }
    return asmh;
}


static void dumpPoolUsage(RegionMgr * rm)
{
    #ifdef _DEBUG_
    #define KB 1024
    note(rm, "\n== Situation of pool used==");
    note(rm, "\n ****** gerenal pool %lu KB ********",
             (ULONG)smpoolGetPoolSize(g_pool_general_used)/KB);
    note(rm, "\n ****** tree pool %lu KB ********",
             (ULONG)smpoolGetPoolSize(g_pool_tree_used)/KB);
    note(rm, "\n ****** st pool %lu KB ********",
             (ULONG)smpoolGetPoolSize(g_pool_st_used)/KB);
    note(rm, "\n ****** total mem query size : %lu KB\n",
             (ULONG)g_stat_mem_size/KB);
    note(rm, "\n===========================\n");
    #endif
}


//#define MEMLEAKTEST
#ifdef MEMLEAKTEST
static void test_ru(RegionMgr * rm, CGMgr * cgmgr)
{
    Region * func = nullptr;
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == nullptr || rg->is_program()) {
            continue;
        }
        func = rg;
        break;
    }

    ASSERT0(func);

    INT i = 0;
    Var * v = func->getRegionVar();
    Region * x = rm->newRegion(REGION_FUNC);
    //Note Local Vars and MDs will be freed and collected by Mgr.
    //The test region should not have global var.
    while (i < 10000) {
        OptCtx oc;
        x->init(REGION_FUNC, rm);
        x->setRegionVar(v);
        IR * irs = func->getIRList();
        x->setIRList(x->dupIRTreeList(irs));
        bool succ = x->process(&oc);
        ASSERT0(succ);
        //VarMgr * vm = x->getVarMgr();
        //vm->dump();
        //MDSystem * ms = x->getMDSystem();
        //ms->dump();
        x->destroy();
        i++;
    }
    return;
}
#endif


static void compileProgramRegion(Region * rg, CGMgr * cgmgr)
{
    cgmgr->genAndPrtGlobalVariable(rg);
    if (!g_is_dumpgr) { return; }
    
    ASSERT0(g_c_file_name || g_gr_file_name);
    xcom::StrBuf b(64);
    b.strcat(g_c_file_name != nullptr ? g_c_file_name : g_gr_file_name);
    b.strcat(".hir.gr");
    UNLINK(b.buf);

    FILE * gr = ::fopen(b.buf, "a");
    ASSERT0(gr);
    rg->getLogMgr()->push(gr, "");
    rg->dumpGR(true);
    rg->getLogMgr()->pop();
    ::fclose(gr);
}


static void compileFuncRegion(Region * rg, CLRegionMgr * rm, CGMgr * cgmgr)
{
    OptCtx * oc = rm->getAndGenOptCtx(rg->id());
    bool s = rm->compileFuncRegion(rg, cgmgr, oc);
    ASSERT0(s);
    DUMMYUSE(s);
    //rm->deleteRegion(rg); //Local region can be deleted if processed.
}


//Processing function unit one by one.
//1. Construct Region.
//2. Generate IR of Region.
//3. Perform IR optimizaions
//4. Generate assembly code.
static void compileRegionSet(CLRegionMgr * rm, CGMgr * cgmgr)
{
    ASSERT0(rm && cgmgr);
    //Test mem leak.
    //test_ru(rm, cgmgr);
    rm->registerGlobalMD();
    Region * program = nullptr; //There could be multiple program regions.
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == nullptr) { continue; }
        if (rg->is_program()) {
            program = rg;
            compileProgramRegion(rg, cgmgr);
            continue;
        }
        if (rg->is_blackbox()) {
            continue;
        }
        if (g_show_time) {
            xoc::prt2C("\n\n==== Start Process Region(id:%d)'%s' ====\n",
                       rg->id(), rg->getRegionName());
        }
        compileFuncRegion(rg, rm, cgmgr);
    }
    ASSERT0(program);

    OptCtx * oc = rm->getAndGenOptCtx(program->id());
    bool s = rm->processProgramRegion(program, oc);
    ASSERT0(s);
    DUMMYUSE(s);
    if (g_dump_opt.isDumpALL()) {
        dumpPoolUsage(rm);
    }
}


static CLRegionMgr * initRegionMgr()
{
    CLRegionMgr * rm = allocRegionMgr();
    rm->initVarMgr();
    rm->initTargInfo();

    //For C language, inserting CVT is expected.
    g_do_refine_auto_insert_cvt = true;
    g_do_refine = true;

    //Retain CFG, DU info for IPA used.
    g_compute_region_imported_defuse_md = true;
    g_retain_pass_mgr_for_region = false;
    //g_compute_pr_du_chain = false;
    //g_compute_nonpr_du_chain = false;
    //g_do_call_graph = true;
    //g_do_ipa = true;
    g_is_support_dynamic_type = true;
    g_is_opt_float = true;
    g_prt_asm_horizontal = true;
    return rm;
}


static void dumpRegionMgrGR(RegionMgr * rm, CHAR * srcname)
{
    ASSERT0(rm);
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == nullptr) { continue; }
        if (rg->is_program()) {
            //r->dump(true);
            ASSERT0(srcname);
            xcom::StrBuf b(64);
            b.strcat(srcname);
            b.strcat(".gr");
            UNLINK(b.buf);
            FILE * gr = fopen(b.buf, "a");
            ASSERT0(gr);
            rg->getLogMgr()->push(gr, b.buf);
            rg->dumpGR(true);
            rg->getLogMgr()->pop();
            ::fclose(gr);
        }
    }
}


static void initCompile(CLRegionMgr ** rm,
                        FILE ** asmh,
                        CGMgr ** cgmgr,
                        TargInfo ** ti)
{
    *rm = initRegionMgr();
    *cgmgr = xgen::allocCGMgr(*rm);    
    *asmh = createAsmFileHandler();
    *ti = (*rm)->getTargInfo();
    (*cgmgr)->setAsmFileHandler(*asmh);
    ASSERT0(*asmh);
}


static void finiCompile(CLRegionMgr * rm,
                        FILE * asmh,
                        CGMgr * cgmgr,
                        TargInfo * ti)
{
    if (cgmgr != nullptr) {
        delete cgmgr;
    }
    if (rm != nullptr) {
        delete rm;
    }
    if (ti != nullptr) {
        delete ti;
    }
    if (asmh != nullptr) {
        ::fclose(asmh);
    }
}


bool compileGRFile(CHAR * gr_file_name)
{
    bool res = true;
    ASSERT0(gr_file_name);
    TargInfo * ti = nullptr;
    CLRegionMgr * rm = nullptr;
    CGMgr * cgmgr = nullptr;
    FILE * asmh = nullptr;
    START_TIMER_FMT(t, ("Compile GR File"));
    initCompile(&rm, &asmh, &cgmgr, &ti);
    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }
    bool succ = xoc::readGRAndConstructRegion(rm, gr_file_name);
    if (!succ) {
        prt2C("\nFail read and parse '%s'", gr_file_name);
        res = false;
        goto FIN;
    }

    if (g_is_dumpgr) {
        dumpRegionMgrGR(rm, gr_file_name);
    }

    //Dump and clean
    compileRegionSet(rm, cgmgr);
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * r = rm->getRegion(i);
        if (r == nullptr || r->is_blackbox()) { continue; }
        if (r->getPassMgr() != nullptr) {
            xoc::PRSSAMgr * ssamgr = (PRSSAMgr*)r->
                getPassMgr()->queryPass(PASS_PR_SSA_MGR);
            if (ssamgr != nullptr && ssamgr->is_valid()) {
                OptCtx * oc = rm->getAndGenOptCtx(r->id());
                ssamgr->destruction(oc);
            }
        }
    }
FIN:
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", gr_file_name));
    finiCompile(rm, asmh, cgmgr, ti);
    return res;
}


bool compileCFile()
{
    bool res = true;
    TargInfo * ti = nullptr;
    CLRegionMgr * rm = nullptr;
    CGMgr * cgmgr = nullptr;
    FILE * asmh = nullptr;
    START_TIMER(t, "Compile C File");
    initParser();
    initCompile(&rm, &asmh, &cgmgr, &ti);    
    g_fe_sym_tab = rm->getSymTab();
    g_dbx_mgr = new CLDbxMgr();
    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }
    if (g_redirect_stdout_to_dump_file) {
        g_unique_dumpfile = rm->getLogMgr()->getFileHandler();
        ASSERT0(g_unique_dumpfile);
    }

    if (FrontEnd(rm) != ST_SUCC) {
        res = false;
        ASSERTN(g_err_msg_list.get_elem_count() > 0, ("miss error msg"));
        goto FIN;
    }
    //In the file scope, generate function region.
    if (g_dump_opt.isDumpALL()) {
        dump_scope(get_global_scope(), 0xffffffff);
    }
    scanAndInitVar(get_global_scope(), rm->getVarMgr(), rm->getTypeMgr());
    if (generateRegion(rm)) {
        compileRegionSet(rm, cgmgr);
    }
    if (g_is_dumpgr) {
        dumpRegionMgrGR(rm, g_c_file_name);
    }
FIN:
    if (g_dbx_mgr != nullptr) {
        delete g_dbx_mgr;
        g_dbx_mgr = nullptr;
    }
    g_decl2var_map.clean();
    g_var2decl_map.clean();
    destroy_scope_list();
    finiCompile(rm, asmh, cgmgr, ti);
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", g_c_file_name));
    show_err();
    show_warn();
    fprintf(stdout, "\n%s - (%d) error(s), (%d) warnging(s)\n",
            g_c_file_name, g_err_msg_list.get_elem_count(),
            g_warn_msg_list.get_elem_count());
    finiParser();
    return res;
}
