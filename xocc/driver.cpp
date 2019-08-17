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

CHAR * g_c_file_name = NULL;
CHAR * g_gr_file_name = NULL;
CHAR * g_dump_file_name = NULL;
static CHAR * g_output_file_name = NULL;
bool g_is_dumpgr = false;

//Record the start position of formal parameter.
//The position 0 is reserved for hidden parameter which used for
//structure return value in C/C++.
UINT const g_formal_parameter_start = 1;

//Print assembly in horizontal manner.
bool g_prt_asm_horizontal = true;

static TMap<Decl*, VAR*> g_decl2var_map;
static TMap<VAR*, Decl*> g_var2decl_map;

#ifdef _DEBUG_
void resetMapBetweenVARandDecl(VAR * v)
{
    Decl * decl = g_var2decl_map.get(v);
    if (decl != NULL) {
        g_decl2var_map.setAlways(decl, NULL);
    }
    g_var2decl_map.setAlways(v, NULL);
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
#define WARN(parmlist)  (report_location(__FILE__, __LINE__), \
                         interwarn parmlist)
#else
#define WARN(parmlist)
#endif

void CLDbxMgr::printSrcLine(Dbx const* dbx, PrtCtx * ctx)
{
    if (g_tfile == NULL) { return; }

    UINT lineno = getLineNum(dbx);
    if (lineno == m_cur_lineno) {
        //It is dispensable that print the same souce file multiple times.
        return;
    }

    m_cur_lineno = lineno;

    if (lineno == 0) {
        //No line number info recorded.
        if (ctx != NULL && ctx->prefix != NULL) {
            note("\n%s[0]\n", ctx->prefix);
        } else {
            note("\n[0]\n");
        }
        return;
    }

    if (g_hsrc != NULL) {
        ASSERTN(m_cur_lineno < OFST_TAB_LINE_SIZE, ("unexpected src line"));
        fseek(g_hsrc, g_ofst_tab[m_cur_lineno], SEEK_SET);
        if (fgets(g_cur_line, g_cur_line_len, g_hsrc) != NULL) {
            if (ctx != NULL && ctx->prefix != NULL) {
                note("\n\n%s[%u]%s", ctx->prefix, m_cur_lineno, g_cur_line);
            } else {
                note("\n\n[%u]%s", m_cur_lineno, g_cur_line);
            }
        }
    }
}
//END DbxMgr


static void usage()
{
    fprintf(stdout,
            "\nXOCC Version 0.9.2"
            "\nUsage: xoc [options] file"
            "\nOptions: "
            "\n  -O0,           compile without any optimization"
            "\n  -O1,-O2,-O3    compile with optimizations"
            "\n  -dump <file>   generate dump file and enable dump"
            "\n  -o <file>      output file name"
            "\n  -gra=<on|off>  switch for global register allocation"
            "\n  -ipa           enable IPA"
            "\n  -dumpgr        dump GR file"
            "\n  -readgr <file> read GR file"
            "\n");
}


static bool is_c_source_file(CHAR * fn)
{
    size_t len = strlen(fn) + 1;
    CHAR * buf = (CHAR*)ALLOCA(len);
    upper(getfilesuffix(fn, buf, (UINT)strlen(fn) + 1));
    if (strcmp(buf, "C") == 0 ||
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
    case '0': g_opt_level = OPT_LEVEL0; break;
    case '1': g_opt_level = OPT_LEVEL1; break;
    case '2': g_opt_level = OPT_LEVEL2; break;
    case '3': g_opt_level = OPT_LEVEL3; break;
    case 's': g_opt_level = SIZE_OPT; break;
    case 'S': g_opt_level = SIZE_OPT; break;
    default: g_opt_level = OPT_LEVEL1;
    }
    i++;
    return true;
}


static bool process_o(INT argc, CHAR * argv[], INT & i)
{
    if (i + 1 < argc && argv[i + 1] != NULL) {
        g_output_file_name = argv[i + 1];
    }
    i += 2;
    return true;
}



static CHAR * process_d(INT argc, CHAR * argv[], INT & i)
{
    CHAR * n = NULL;
    if (i + 1 < argc && argv[i + 1] != NULL) {
        n = argv[i + 1];
    }
    i += 2;
    return n;
}


static CHAR * process_readgr(INT argc, CHAR * argv[], INT & i)
{
    CHAR * n = NULL;
    if (i + 1 < argc && argv[i + 1] != NULL) {
        n = argv[i + 1];
    }
    g_gr_file_name = n;
    i += 2;
    return n;
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


static bool process_t(INT argc, CHAR * argv[], INT & i)
{
    DUMMYUSE(argc);
    CHAR * cmdstr = &argv[i][1];
    if (xstrcmp(cmdstr, "time", 4) != 0) {
        return false;
    }
    i++;
    g_show_time = true;
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


bool processCmdLine(INT argc, CHAR * argv[])
{
    INT i = 1;
    if (argc <= 1) { usage(); return false; }
    while (i < argc) {
        if (argv[i][0] == '-') {
            CHAR const* cmdstr = &argv[i][1];
            if (cmdstr[0] == 'O') {
                if (!process_O(argc, argv, i)) {
                    usage();
                    return false;
                }
            } else if (cmdstr[0] == 'o') {
                if (!process_o(argc, argv, i)) {
                    usage();
                    return false;
                }
            } else if (cmdstr[0] == 'g') {
                if (!process_g(argc, argv, i)) {
                    usage();
                    return false;
                }
            } else if (cmdstr[0] == 'i') {
                if (!process_i(argc, argv, i)) {
                    usage();
                    return false;
                }
            } else if (cmdstr[0] == 't') {
                if (!process_t(argc, argv, i)) {
                    usage();
                    return false;
                }
            } else if (!strcmp(cmdstr, "dump")) {
                CHAR * n = process_d(argc, argv, i);
                if (n == NULL) {
                    usage();
                    return false;
                }
                g_dump_file_name = n;
            } else if (!strcmp(cmdstr, "readgr")) {
                CHAR * n = process_readgr(argc, argv, i);
                if (n == NULL) {
                    usage();
                    return false;
                }
            } else if (!strcmp(cmdstr, "dumpgr")) {
                g_is_dumpgr = true;
                i++;
            } else {
                usage();
                return false;
            }
        } else if (is_c_source_file(argv[i])) {
            g_c_file_name = argv[i];
            i++;
        } else {
            //Unsupport option.
            usage();
            return false;
        }
    }
    if (g_c_file_name != NULL) {
        g_hsrc = fopen(g_c_file_name, "rb");
        if (g_hsrc == NULL) {
            fprintf(stdout, "xoc: cannot open %s, error information is %s\n",
                            g_c_file_name, strerror(errno));
            return false;
        }
    }
    if (g_output_file_name != NULL) {
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
        g_compute_classic_du_chain = false;
        g_do_refine = false;
        g_do_refine_auto_insert_cvt = false;
        g_do_call_graph = false;
        g_do_ipa = false;
    }
    return true;
}


VAR * mapDecl2VAR(Decl * decl)
{
    return g_decl2var_map.get(decl);
}


Decl * mapVAR2Decl(VAR * var)
{
    return g_var2decl_map.get(var);
}


//Transforming Decl into VAR.
//'decl': variable declaration in front end.
static VAR * addDecl(IN Decl * decl, IN OUT VarMgr * var_mgr, TypeMgr * dm)
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
        //TODO: record initial value in VAR at CTree2IR.
        //SET_FLAG(flag, VAR_HAS_INIT_VAL);
    }

    UINT data_size = 0;

    ASSERT0(dm);

    DATA_TYPE data_type = get_decl_dtype(decl, &data_size, dm);
    Type const* type = NULL;
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
        ASSERT0(data_size == dm->get_dtype_bytesize(data_type));
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
    VAR * v = var_mgr->registerVar(var_name, type,
        (UINT)xcom::ceil_align(MAX(DECL_align(decl), STACK_ALIGNMENT),
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


//Constructing a variable table to map each of DECLARATIONs to an unique VAR.
//Scanning SCOPE trees to construct such table.
//e.g: SCOPE tree
//    SCOPE0
//      --SCOPE1
//          --SCOPE2
//          --SCOPE3
//      --SCOPE4
static void scanAndInitVar(SCOPE * s, VarMgr * vm, TypeMgr * tm)
{
    if (s == NULL) { return; }
    do {
        for (Decl * decl = SCOPE_decl_list(s);
             decl != NULL; decl = DECL_next(decl)) {
            ASSERT0(DECL_decl_scope(decl) == s);
            if (is_fun_decl(decl)) {
                //Function declaration decl.
                if (DECL_is_fun_def(decl)) {
                    //Function definition
                    ASSERT0(DECL_decl_scope(decl) == s);
                    VAR * v = addDecl(decl, vm, tm);
                    VAR_is_func_decl(v) = true;
                } else {
                    //Function declaration
                    if (mapDecl2VAR(decl) == NULL) {
                        VAR * v = addDecl(decl, vm, tm);

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
            if (mapDecl2VAR(decl) == NULL &&
                !(DECL_is_formal_para(decl) && get_decl_sym(decl) == NULL)) {
                addDecl(decl, vm, tm);
            }
        }

        scanAndInitVar(SCOPE_sub(s), vm, tm);
        s = SCOPE_nsibling(s);
    } while (s != NULL);
}


UINT FrontEnd()
{
    START_TIMER(t, "CFE");

//#define LR0_FE
#ifdef LR0_FE
    init_rule_info();
    reduce();
#else
    INT s = Parser();
    if (s != ST_SUCC) {
        goto FIN;
    }
#endif
    s = TypeTransform();
    if (s != ST_SUCC) {
        goto FIN;
    }

    s = TypeCheck();
    if (s != ST_SUCC) {
        goto FIN;
    }

FIN:
    END_TIMER(t, "CFE");
    return s;
}


static FILE * createAsmFileHandler()
{
    FILE * asmh = NULL;
    if (g_output_file_name != NULL) {
        asmh = fopen(g_output_file_name, "a+");
        if (asmh == NULL) {
            prt2C("Can not create assembly file %s", g_output_file_name);
        }
        return asmh;
    }

    StrBuf buf(128);
    ASSERT0(g_c_file_name || g_gr_file_name);
    buf.sprint("%s.asm", g_c_file_name != NULL ?
        g_c_file_name : g_gr_file_name);
    asmh = fopen(buf.buf, "a+");
    if (asmh == NULL) {
        prt2C("Can not create assembly file %s", buf.buf);
    }
    return asmh;
}


static void dumpPoolUsage()
{
    #define KB 1024
    #ifdef _DEBUG_
    if (g_tfile == NULL) { return; }

    note("\n== Situation of pool used==");
    note("\n ****** gerenal pool %lu KB ********",
                    (ULONG)smpoolGetPoolSize(g_pool_general_used)/KB);
    note("\n ****** tree pool %lu KB ********",
                    (ULONG)smpoolGetPoolSize(g_pool_tree_used)/KB);
    note("\n ****** st pool %lu KB ********",
                    (ULONG)smpoolGetPoolSize(g_pool_st_used)/KB);
    note("\n ****** tmp pool %lu KB ********",
                    (ULONG)smpoolGetPoolSize(xoc::get_tmp_pool())/KB);
    note("\n ****** total mem query size : %lu KB\n",
                    (ULONG)g_stat_mem_size/KB);

    note("\n===========================\n");
    fflush(g_tfile);
    #endif
}


//#define MEMLEAKTEST
#ifdef MEMLEAKTEST
static void test_ru(RegionMgr * rm)
{
    Region * func = NULL;
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == NULL) { continue; }
        if (rg->is_program()) {
            continue;
        }
        func = rg;
        break;
    }

    ASSERT0(func);

    INT i = 0;
    VAR * v = func->getRegionVar();
    Region * x = new Region(REGION_FUNC, rm);
    while (i < 10000) {
        x->init(REGION_FUNC, rm);
        x->setRegionVar(v);
        IR * irs = func->getIRList();
        x->setIRList(x->dupIRTreeList(irs));
        bool succ = x->process();
        ASSERT0(succ);
        //VarMgr * vm = x->getVarMgr();
        //vm->dump();
        //MDSystem * ms = x->getMDSystem();
        //ms->dump();
        tfree();
        x->destroy();
        i++;
    }
    return;
}
#endif


//Processing function unit one by one.
//1. Construct Region.
//2. Generate IR of Region.
//3. Perform IR optimizaions
//4. Generate assembly code.
static void compileRegionSet(CLRegionMgr * rm, CGMgr * cgmgr, FILE * asmh)
{
    ASSERT0(rm && cgmgr && asmh);
    //Test mem leak.
    //test_ru(this);
    rm->registerGlobalMD();
    Region * program = NULL; //There could be multiple program regions.
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == NULL) { continue; }
        if (rg->is_program()) {
            program = rg;
            cgmgr->GenAndPrtGlobalVariable(rg, asmh);
            if (g_is_dumpgr) {
                g_indent = 0;
                //rg->dump(true);
                ASSERT0(g_c_file_name || g_gr_file_name);
                xcom::StrBuf b(64);
                b.strcat(g_c_file_name != NULL ?
                    g_c_file_name : g_gr_file_name);
                b.strcat(".hir.gr");
                UNLINK(b.buf);
                FILE * gr = fopen(b.buf, "a");
                FILE * oldvalue = g_tfile;
                g_tfile = gr;
                rg->dumpGR(true);
                fclose(gr);
                g_tfile = oldvalue;
            }
            continue;
        }

        if (g_show_time) {
            printf("\n====Start Process Region(id:%d)'%s' ====\n",
               REGION_id(rg), rg->getRegionName());
        }

        OptCtx * oc = rm->getAndGenOptCtx(rg->id());
        bool s = rm->compileFuncRegion(rg, cgmgr, asmh, oc);
        ASSERT0(s);
        DUMMYUSE(s);
        //rm->deleteRegion(rg); //Local region can be deleted if processed.
    }

    ASSERT0(program);
    //if (g_is_dumpgr) {
    //    g_indent = 0;
    //    //rg->dump(true);
    //    ASSERT0(g_c_file_name || g_gr_file_name);
    //    xcom::StrBuf b(64);
    //    b.strcat(g_c_file_name != NULL ?
    //        g_c_file_name : g_gr_file_name);
    //    b.strcat(".cg.gr");
    //    UNLINK(b.buf);
    //    FILE * gr = fopen(b.buf, "a");
    //    FILE * oldvalue = g_tfile;
    //    g_tfile = gr;
    //    program->dumpGR(true);
    //    fclose(gr);
    //    g_tfile = oldvalue;
    //}

    OptCtx * oc = rm->getAndGenOptCtx(program->id());
    bool s = rm->processProgramRegion(program, oc);
    ASSERT0(s);
    DUMMYUSE(s);
    dumpPoolUsage();
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
    g_retain_pass_mgr_for_region = true;
    g_compute_classic_du_chain = false;
    //g_do_call_graph = true;
    //g_do_ipa = true;
    g_is_support_dynamic_type = true; 
    g_is_opt_float = true;
    g_prt_asm_horizontal = true;
    g_do_refine_auto_insert_cvt = false;
    g_is_dump_mdset_hash = true;
    g_is_dump_du_chain = true;
    return rm;
}


static void dumpRegionMgrGR(RegionMgr * rm, CHAR * srcname)
{
    ASSERT0(rm);
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * r = rm->getRegion(i);
        if (r == NULL) { continue; }
        if (r->is_program()) {
            g_indent = 0;
            //r->dump(true);
            ASSERT0(srcname);
            xcom::StrBuf b(64);
            b.strcat(srcname);
            b.strcat(".gr");
            UNLINK(b.buf);
            FILE * gr = fopen(b.buf, "a");
            FILE * oldvalue = g_tfile;
            g_tfile = gr;
            r->dumpGR(true);
            fclose(gr);
            g_tfile = oldvalue;
        }
    }
}


static void initCompile(
        CLRegionMgr ** rm,
        FILE ** asmh,
        CGMgr ** cgmgr,
        TargInfo ** ti)
{
    *rm = initRegionMgr();
    *cgmgr = xgen::allocCGMgr();
    *asmh = createAsmFileHandler();
    *ti = (*rm)->allocTargInfo();
    (*rm)->setTargetInfo(*ti);
    ASSERT0(*asmh);
}


static void finiCompile(
        CLRegionMgr * rm,
        FILE * asmh,
        CGMgr * cgmgr,
        TargInfo * ti)
{
    if (rm != NULL) {
        delete rm;
    }
    if (cgmgr != NULL) {
        delete cgmgr;
    }
    if (ti != NULL) {
        delete ti;
    }
    if (asmh != NULL) {
        fclose(asmh);
    }
}


bool compileGRFile(CHAR * gr_file_name)
{
    bool res = true;
    ASSERT0(gr_file_name);
    TargInfo * ti = NULL;
    CLRegionMgr * rm = NULL;
    CGMgr * cgmgr = NULL;
    FILE * asmh = NULL;
    START_TIMER(t, "Compile GR File");
    initCompile(&rm, &asmh, &cgmgr, &ti);
    xoc::initdump("tmp.log", true);
    bool succ = xoc::readGRAndConstructRegion(rm, gr_file_name);
    if (!succ) {
        res = false;
        goto FIN;
    }

    if (g_is_dumpgr) {
    	dumpRegionMgrGR(rm, gr_file_name);
    }

    //Dump and clean
    compileRegionSet(rm, cgmgr, asmh);
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
    	Region * r = rm->getRegion(i);
    	if (r == NULL) { continue; }
    	if (r->getPassMgr() != NULL) {
            xoc::PRSSAMgr * ssamgr = (PRSSAMgr*)r->
            	getPassMgr()->queryPass(PASS_PR_SSA_MGR);
            if (ssamgr != NULL && ssamgr->isSSAConstructed()) {
                OptCtx * oc = rm->getAndGenOptCtx(r->id());
            	ssamgr->destruction(oc);
            }
    	}
    }
FIN:
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", gr_file_name));
    finiCompile(rm, asmh, cgmgr, ti);
    xoc::finidump();
    return res;
}


bool compileCFile()
{
    bool res = true;
    TargInfo * ti = NULL;
    CLRegionMgr * rm = NULL;
    CGMgr * cgmgr = NULL;
    FILE * asmh = NULL;
    START_TIMER(t, "Compile C File");
    initParser();
    initCompile(&rm, &asmh, &cgmgr, &ti);
    g_fe_sym_tab = rm->getSymTab();
    g_dbx_mgr = new CLDbxMgr();
    if (FrontEnd() != ST_SUCC) {
        res = false;
        goto FIN;
    }
    scanAndInitVar(get_global_scope(), rm->getVarMgr(), rm->getTypeMgr());
    if (generateRegion(rm)) {
        compileRegionSet(rm, cgmgr, asmh);
    }
    if (g_is_dumpgr) {
        dumpRegionMgrGR(rm, g_c_file_name);
    }
FIN:
    if (g_dbx_mgr != NULL) {
        delete g_dbx_mgr;
        g_dbx_mgr = NULL;
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
    //xoc::finidump will be invoked after the function return.
    return res;
}
