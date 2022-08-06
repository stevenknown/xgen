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

#include "../opt/cominc.h"
#include "xoccinc.h"
#include "../xgen/xgeninc.h"
#include "errno.h"
#include "../opt/util.h"
#include "../reader/grreader.h"
#include "../opt/comopt.h"

namespace xocc {

//#define LR0_FE

//Record the start position of formal parameter.
//The position 0 is reserved for hidden parameter which used for
//structure return value in C/C++.
UINT const g_formal_parameter_start = 1;

//Print assembly in horizontal manner.
bool g_prt_asm_horizontal = true;

CHAR const* g_output_file_name = nullptr; //record the ASM file name.
CHAR const* g_xocc_version = "1.2.3"; //recod the xocc.exe version.
CHAR const* g_dump_file_name = nullptr;
bool g_is_dumpgr = false;

xcom::List<CHAR const*> g_cfile_list;
xcom::List<CHAR const*> g_grfile_list;

static TMap<xfe::Decl*, Var*> g_decl2var_map;
static TMap<Var*, xfe::Decl*> g_var2decl_map;

void resetMapBetweenVARandDecl(xoc::Var * var)
{
    xfe::Decl * decl = g_var2decl_map.get(var);
    if (decl != nullptr) {
        g_decl2var_map.setAlways(decl, nullptr);
    }
    g_var2decl_map.setAlways(var, nullptr);
}


INT report_location(CHAR const* file, INT line)
{
    printf("\n\n\n%s : %d", file, line);
    return 0;
}

Var * mapDecl2VAR(xfe::Decl * decl)
{
    return g_decl2var_map.get(decl);
}


xfe::Decl * mapVAR2Decl(Var * var)
{
    return g_var2decl_map.get(var);
}


static xoc::Type const* makeType(xfe::Decl const* decl, TypeMgr * tm)
{
    ASSERT0(tm);
    if (decl->is_fun_def() || decl->is_fun_decl()) {
        return tm->getSimplexTypeEx(xoc::D_ANY);
    }
    UINT data_size = 0;
    DATA_TYPE data_type = CTree2IR::get_decl_dtype(decl, &data_size, tm);
    if (IS_PTR(data_type)) {
        ASSERT0(decl->regardAsPointer());
        UINT basesize = decl->get_pointer_base_size();

        //Note: If pointer_base_size is 0, then the pointer can not
        //do any operation, such as pointer arithmetic.
        //ASSERTN(basesize > 0, ("meet incomplete type."));

        return tm->getPointerType(basesize);
    }
    if (IS_MC(data_type)) {
        //Is data_size likely to be 0?
        return tm->getMCType(data_size);
    }
    //data_size must definitly equal to corresponding size.
    ASSERT0(data_size == tm->getDTypeByteSize(data_type));
    return tm->getSimplexTypeEx(data_type);
}


//Transforming Decl into Var.
//decl: variable declaration in front end.
static Var * addDecl(IN Decl * decl, MOD VarMgr * var_mgr, TypeMgr * tm)
{
    ASSERTN(decl->is_dt_declaration(), ("unsupported"));
    if (decl->is_user_type_decl()) { return nullptr; }
    UINT flag = 0;
    if (decl->is_global_variable()) {
        ASSERT0(!decl->is_local_variable());
        SET_FLAG(flag, VAR_GLOBAL);
    } else if (decl->is_local_variable()) {
        SET_FLAG(flag, VAR_LOCAL);
    } else {
        UNREACHABLE();
    }
    if (decl->is_static()) {
        SET_FLAG(flag, VAR_PRIVATE);
    }
    if (decl->is_constant()) {
        SET_FLAG(flag, VAR_READONLY);
    }
    if (decl->is_volatile()) {
        SET_FLAG(flag, VAR_VOLATILE);
    }
    if (decl->is_restrict()) {
        SET_FLAG(flag, VAR_IS_RESTRICT);
    }
    if (decl->is_initialized()) {
        //TODO: record initial value in Var at CTree2IR.
        //SET_FLAG(flag, VAR_HAS_INIT_VAL);
    }
    xoc::Type const* type = makeType(decl, tm);
    ASSERT0(type);
    if (decl->is_formal_param()) {
        //Declaration is formal parameter.
        SET_FLAG(flag, VAR_IS_FORMAL_PARAM);
    }
    if (decl->is_array()) {
        SET_FLAG(flag, VAR_IS_ARRAY);
    }
    CHAR const* var_name = decl->get_decl_sym()->getStr();
    UINT var_align = (UINT)xcom::ceil_align(
        MAX(DECL_align(decl), STACK_ALIGNMENT), STACK_ALIGNMENT);
    Var * v = var_mgr->registerVar(var_name, type, var_align, flag);
    if (v->is_global()) {
        //For conservative purpose.
        v->setflag(VAR_ADDR_TAKEN);
    }
    if (decl->is_formal_param()) {
        VAR_formal_param_pos(v) = g_formal_parameter_start +
                                  DECL_formal_param_pos(decl);
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
        for (Decl * decl = s->getDeclList();
             decl != nullptr; decl = DECL_next(decl)) {
            ASSERT0(DECL_decl_scope(decl) == s);
            if (decl->is_fun_decl()) {
                if (DECL_is_fun_def(decl)) {
                    //Function definition.
                    ASSERT0(DECL_decl_scope(decl) == s);
                    Var * v = addDecl(decl, vm, tm);
                    ASSERT0(v); //Function definition should not be fake.
                    v->setflag(VAR_IS_FUNC);
                    continue;
                }

                if (mapDecl2VAR(decl) == nullptr) {
                    //Function forward declaration.
                    Var * v = addDecl(decl, vm, tm);
                    if (v != nullptr) {
                        //Function declaration should not be fake, since
                        //it might be taken address.
                        //e.g: extern void foo();
                        //  void * p = &foo;
                        //v->setflag(VAR_IS_FAKE);
                        //Note type-variable that defined by 'typedef'
                        //will NOT be mapped to XOC Variable.
                        v->setflag((VAR_FLAG)(VAR_IS_FUNC|VAR_IS_DECL));
                    }
                    continue;
                }

                //Function/variable declaration, can not
                //override real function define.
                continue;
            }

            //General variable declaration decl.
            if (mapDecl2VAR(decl) == nullptr &&
                !(decl->is_formal_param() &&
                  decl->get_decl_sym() == nullptr)) {
                //No need to generate Var for parameter that does not
                //have a name.
                //e.g: parameter of foo(CHAR*)
                addDecl(decl, vm, tm);
            }
        }

        scanAndInitVar(SCOPE_sub(s), vm, tm);
        s = SCOPE_nsibling(s);
    } while (s != nullptr);
}


UINT FrontEnd(RegionMgr * rm, CParser & parser)
{
    START_TIMER(t, "CFE");
    initTypeTran();

    STATUS s = ST_SUCC;
#ifdef LR0_FE
    init_rule_info();
    reduce();
#else
    s = parser.perform();
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

    s = TreeCanonicalize();
    if (s != ST_SUCC) {
        END_TIMER(t, "CFE");
        return s;
    }

    END_TIMER(t, "CFE");
    return ST_SUCC;
}


static FILE * createAsmFileHandler(CHAR const* fn)
{
    FILE * asmh = nullptr;
    if (g_output_file_name != nullptr) {
        //Use customized output file name.
        UNLINK(g_output_file_name);
        asmh = ::fopen(g_output_file_name, "a+");
        if (asmh == nullptr) {
            xoc::prt2C("Can not create assembly file %s", g_output_file_name);
        }
        return asmh;
    }

    StrBuf buf(128);
    ASSERT0(fn);
    buf.sprint("%s.asm", fn);
    UNLINK(buf.buf);
    asmh = ::fopen(buf.buf, "a+");
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
    x->initAttachInfoMgr();
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


static void compileProgramRegion(CHAR const* fn, Region * rg, CGMgr * cgmgr)
{
    cgmgr->genAndPrtGlobalVariable(rg);
    if (!g_is_dumpgr) { return; }

    ASSERT0(fn);
    xcom::StrBuf b(64);
    b.strcat(fn);
    b.strcat(".hir.gr");
    UNLINK(b.buf);

    FILE * gr = ::fopen(b.buf, "a");
    ASSERT0(gr);
    rg->getLogMgr()->push(gr, "");
    GRDump gd(rg);
    gd.dumpRegion(true);
    rg->getLogMgr()->pop();
    ::fclose(gr);
}


static void compileFuncRegion(Region * rg, CLRegionMgr * rm, CGMgr * cgmgr)
{
    OptCtx * oc = rm->getAndGenOptCtx(rg);
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
static void compileRegionSet(CHAR const* fn, CLRegionMgr * rm, CGMgr * cgmgr)
{
    ASSERT0(fn && rm && cgmgr);
    //Test mem leak.
    //test_ru(rm, cgmgr);
    rm->registerGlobalMD();
    Region * program = nullptr; //There could be multiple program regions.
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * rg = rm->getRegion(i);
        if (rg == nullptr) { continue; }
        if (rg->is_program()) {
            program = rg;
            compileProgramRegion(fn, rg, cgmgr);
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

    OptCtx * oc = rm->getAndGenOptCtx(program);
    bool s = rm->processProgramRegion(program, oc);
    ASSERT0(s);
    DUMMYUSE(s);
    if (g_dump_opt.isDumpAll()) {
        dumpPoolUsage(rm);
    }
}


static CLRegionMgr * initRegionMgr()
{
    CLRegionMgr * rm = xocc::allocCLRegionMgr();
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


static void dumpRegionMgrGR(RegionMgr * rm, CHAR const* srcname)
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
            FILE * gr = ::fopen(b.buf, "a");
            ASSERT0(gr);
            rg->getLogMgr()->push(gr, b.buf);
            GRDump gd(rg);
            gd.dumpRegion(true);
            rg->getLogMgr()->pop();
            ::fclose(gr);
        }
    }
}


static void initCompile(CHAR const* fn, OUT CLRegionMgr ** rm, OUT FILE ** asmh,
                        OUT CGMgr ** cgmgr, OUT TargInfo ** ti)
{
    *rm = initRegionMgr();
    *cgmgr = xgen::allocCGMgr(*rm);
    *asmh = createAsmFileHandler(fn);
    *ti = (*rm)->getTargInfo();
    (*cgmgr)->setAsmFileHandler(*asmh);
    ASSERT0(*asmh);
}


static void finiCompile(CLRegionMgr * rm, FILE * asmh, CGMgr * cgmgr,
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


bool compileGRFile(CHAR const* fn)
{
    ASSERT0(fn);
    bool res = true;
    xoc::TargInfo * ti = nullptr;
    xocc::CLRegionMgr * rm = nullptr;
    xgen::CGMgr * cgmgr = nullptr;
    FILE * asmh = nullptr;
    START_TIMER_FMT(t, ("Compile GR File '%s'", fn));
    initCompile(fn, &rm, &asmh, &cgmgr, &ti);
    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }
    bool succ = xoc::readGRAndConstructRegion(rm, fn);
    if (!succ) {
        xoc::prt2C("\nFail read and parse '%s'", fn);
        res = false;
        goto FIN;
    }

    if (g_is_dumpgr) {
        dumpRegionMgrGR(rm, fn);
    }

    //Dump and clean.
    compileRegionSet(fn, rm, cgmgr);
    for (UINT i = 0; i < rm->getNumOfRegion(); i++) {
        Region * r = rm->getRegion(i);
        if (r == nullptr || r->is_blackbox()) { continue; }
        if (r->getPassMgr() != nullptr) {
            xoc::PRSSAMgr * ssamgr = r->getPRSSAMgr();
            if (ssamgr != nullptr && ssamgr->is_valid()) {
                ssamgr->destruction(*rm->getAndGenOptCtx(r));
            }
        }
    }
FIN:
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", fn));
    finiCompile(rm, asmh, cgmgr, ti);
    show_err();
    show_warn();
    fprintf(stdout, "\n%s - (%d) error(s), (%d) warnging(s)\n",
            fn, g_err_msg_list.get_elem_count(),
            g_warn_msg_list.get_elem_count());    
    g_err_msg_list.clean();
    g_warn_msg_list.clean();
    return res;
}


bool compileCFile(CHAR const* fn)
{
    START_TIMER_FMT(t, ("Compile C File '%s'", fn));
    bool succ = true;
    xoc::TargInfo * ti = nullptr;
    xocc::CLRegionMgr * rm = nullptr;
    xgen::CGMgr * cgmgr = nullptr;
    FILE * asmh = nullptr;
    initCompile(fn, &rm, &asmh, &cgmgr, &ti);

    CParser parser(rm->getLogMgr(), fn);
    if (g_err_msg_list.get_elem_count() > 0) {
        succ = false;
        goto FIN;
    }

    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }

    if (g_redirect_stdout_to_dump_file) {
        g_unique_dumpfile = rm->getLogMgr()->getFileHandler();
        ASSERT0(g_unique_dumpfile);
    }

    g_fe_sym_tab = rm->getSymTab();
    g_dbx_mgr = new CLDbxMgr();

    if (FrontEnd(rm, parser) != ST_SUCC) {
        succ = false;
        ASSERTN(g_err_msg_list.has_msg(), ("miss error msg"));
        goto FIN;
    }

    //In the file scope, generate function region.
    if (g_dump_opt.isDumpAll()) {
        get_global_scope()->dump();
    }
    scanAndInitVar(get_global_scope(), rm->getVarMgr(), rm->getTypeMgr());
    if (CTree2IR::generateRegion(rm)) {
        compileRegionSet(fn, rm, cgmgr);
    }
    if (g_is_dumpgr) {
        dumpRegionMgrGR(rm, fn);
    }
FIN:
    if (g_dbx_mgr != nullptr) {
        delete g_dbx_mgr;
        g_dbx_mgr = nullptr;
    }
    g_decl2var_map.clean();
    g_var2decl_map.clean();

    //Timer use prt2C.
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", fn));
    finiCompile(rm, asmh, cgmgr, ti);
    show_err();
    show_warn();
    fprintf(stdout, "\n%s - (%d) error(s), (%d) warnging(s)\n",
            fn, g_err_msg_list.get_elem_count(),
            g_warn_msg_list.get_elem_count());    
    g_err_msg_list.clean();
    g_warn_msg_list.clean();
    return succ;
}


bool compileCFileList()
{
    for (CHAR const* fn = g_cfile_list.get_head(); fn != nullptr;
         fn = g_cfile_list.get_next()) {
        if (!compileCFile(fn)) {
            return false;
        }
    }
    return true;
}


bool compileGRFileList()
{
    for (CHAR const* fn = g_grfile_list.get_head(); fn != nullptr;
         fn = g_grfile_list.get_next()) {
        if (!compileGRFile(fn)) {
            return false;
        }
    }
    return true;
}

} //namespace xocc
