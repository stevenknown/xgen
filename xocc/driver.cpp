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
#include "../reader/grreader.h"

namespace xocc {

//#define LR0_FE

//Record the start position of formal parameter.
//The position 0 is reserved for hidden parameter which used for
//structure return value in C/C++.
UINT const g_formal_parameter_start = 1;
CHAR const* g_output_file_name = nullptr; //record the ASM file name.
CHAR const* g_xocc_version = "1.2.3"; //recod the xocc.exe version.
CHAR const* g_dump_file_name = nullptr;
bool g_is_dumpgr = false;
bool g_is_dump_option = false;

xcom::List<CHAR const*> g_cfile_list;
xcom::List<CHAR const*> g_grfile_list;
xcom::List<CHAR const*> g_objfile_list;
xcom::List<CHAR const*> g_static_lib_list;

//
//START DeclAndVarMap
//
void DeclAndVarMap::scanDeclList(Scope const* s, Decl * decllist)
{
    for (Decl * decl = decllist; decl != nullptr; decl = DECL_next(decl)) {
        ASSERT0(DECL_decl_scope(decl) == s);
        if (decl->is_fun_decl()) {
            if (DECL_is_fun_def(decl)) {
                //Function definition.
                ASSERT0(DECL_decl_scope(decl) == s);
                Var * v = addDecl(decl);
                ASSERT0(v); //Function definition should not be fake.
                v->setFlag(VAR_IS_FUNC);
                continue;
            }
            if (mapDecl2Var(decl) == nullptr) {
                //Function forward declaration.
                Var * v = addDecl(decl);
                if (v != nullptr) {
                    //Function declaration should not be fake, since
                    //it might be taken address.
                    //e.g: extern void foo();
                    //  void * p = &foo;
                    //v->setFlag(VAR_IS_FAKE);
                    //Note type-variable that defined by 'typedef'
                    //will NOT be mapped to XOC Variable.
                    v->setFlag((VAR_FLAG)(VAR_IS_FUNC|VAR_IS_DECL|
                               VAR_IS_REGION));
                }
                continue;
            }
            //Function/variable declaration, can not
            //override real function define.
            continue;
        }
        //General variable declaration decl.
        if (mapDecl2Var(decl) == nullptr &&
            !(decl->is_formal_param() &&
              decl->get_decl_sym() == nullptr)) {
            //No need to generate Var for parameter that does not
            //have a name.
            //e.g: parameter of foo(CHAR*)
            addDecl(decl);
        }
    }
}


//Constructing a variable table to map each of DECLARATIONs to an unique Var.
//Scanning Scope trees to construct such table.
//e.g: Scope tree
//    SCOPE0
//      --SCOPE1
//          --SCOPE2
//          --SCOPE3
//      --SCOPE4
void DeclAndVarMap::scanAndInitVar(Scope const* s)
{
    if (s == nullptr) { return; }
    do {
        scanDeclList(s, s->getDeclList());
        scanAndInitVar(SCOPE_sub(s));
        s = SCOPE_nsibling(s);
    } while (s != nullptr);
}


xoc::Type const* DeclAndVarMap::makeType(xfe::Decl const* decl)
{
    if (decl->is_fun_def() || decl->is_fun_decl()) {
        return m_tm->getSimplexTypeEx(xoc::D_ANY);
    }
    UINT data_size = 0;
    DATA_TYPE data_type = CTree2IR::get_decl_dtype(decl, &data_size, m_tm);
    if (IS_PTR(data_type)) {
        ASSERT0(decl->regardAsPointer());
        UINT basesize = decl->get_pointer_base_size();

        //Note: If pointer_base_size is 0, then the pointer can not
        //do any operation, such as pointer arithmetic.
        //ASSERTN(basesize > 0, ("meet incomplete type."));

        return m_tm->getPointerType(basesize);
    }
    if (IS_MC(data_type)) {
        //Is data_size likely to be 0?
        return m_tm->getMCType(data_size);
    }
    //data_size must definitly equal to corresponding size.
    ASSERT0(data_size == m_tm->getDTypeByteSize(data_type));
    return m_tm->getSimplexTypeEx(data_type);
}


//Transforming Decl into Var.
//decl: variable declaration in front end.
xoc::Var * DeclAndVarMap::addDecl(Decl const* decl)
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
    xoc::Type const* type = makeType(decl);
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
    Var * v = m_vm->registerVar(var_name, type, var_align, flag);
    if (v->is_global()) {
        //For conservative purpose.
        v->setFlag(VAR_ADDR_TAKEN);
    }
    if (decl->is_formal_param()) {
        VAR_formal_param_pos(v) = g_formal_parameter_start +
                                  DECL_formal_param_pos(decl);
    }
    m_decl2var_map.setAlways(decl, v);
    m_var2decl_map.setAlways(v, decl);
    return v;
}
//END DeclAndVarMap


//
//START Compiler
//
UINT Compiler::runFrontEnd(RegionMgr * rm, CParser & parser)
{
    START_TIMER(t, "C Front End");
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


void Compiler::createAsmFileHandler(OUT FileObj & asmfo, CHAR const* fn)
{
    if (g_output_file_name != nullptr) {
        //Use customized output file name.
        xcom::FO_STATUS st;
        asmfo.init(g_output_file_name, true, false, &st);
        if (st != xcom::FO_SUCC) {
            xoc::prt2C("can not create assembly file %s", g_output_file_name);
        }
        return;
    }
    StrBuf buf(128);
    ASSERT0(fn);
    buf.sprint("%s.asm", fn);
    xcom::FO_STATUS st;
    asmfo.init(buf.buf, true, false, &st);
    if (st != xcom::FO_SUCC) {
        xoc::prt2C("can not create assembly file %s", buf.buf);
    }
}


CLRegionMgr * Compiler::allocCLRegionMgr()
{
    return new CLRegionMgr();
}


CLRegionMgr * Compiler::initRegionMgr()
{
    CLRegionMgr * rm = allocCLRegionMgr();
    rm->initVarMgr();
    rm->initTargInfo();
    return rm;
}


void Compiler::initCompile(CHAR const* fn, OUT CLRegionMgr ** rm,
                           OUT FileObj & asmfo, OUT CGMgr ** cgmgr,
                           OUT TargInfo ** ti)
{
    *rm = initRegionMgr();
    *cgmgr = xgen::allocCGMgr(*rm);
    createAsmFileHandler(asmfo, fn);
    *ti = (*rm)->getTargInfo();
    (*cgmgr)->setAsmFileHandler(asmfo.getFileHandler());
    (*rm)->setCGMgr(*cgmgr);
}


void Compiler::finiCompile(CLRegionMgr * rm, FileObj & asmfo, CGMgr * cgmgr,
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
    asmfo.destroy();
}


void Compiler::destructPRSSAForAllRegion(RegionMgr * rm)
{
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
}


bool Compiler::compileGRFile(CHAR const* fn)
{
    START_TIMER_FMT(t, ("Compile GR File '%s'", fn));
    ASSERT0(fn);
    bool res = true;
    xoc::TargInfo * ti = nullptr;
    xocc::CLRegionMgr * rm = nullptr;
    FileObj asmfo;
    xgen::CGMgr * cgmgr = nullptr;
    initCompile(fn, &rm, asmfo, &cgmgr, &ti);
    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }
    if (g_is_dump_option) {
        xoc::Option::dump(rm);
    }
    bool succ = xoc::readGRAndConstructRegion(rm, fn);
    if (!succ) {
        xoc::prt2C("\nerror: fail read and parse '%s'", fn);
        res = false;
        goto FIN;
    }
    if (rm->getProgramRegion() == nullptr) {
        xoc::prt2C("\nerror: miss program region '%s'", fn);
        res = false;
        goto FIN;
    }
    if (g_is_dumpgr) {
        rm->dumpProgramRegionGR(fn);
    }

    //Dump and clean.
    rm->compileRegionSet(fn);
FIN:
    destructPRSSAForAllRegion(rm);
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", fn));
    finiCompile(rm, asmfo, cgmgr, ti);
    show_err();
    show_warn();
    fprintf(stdout, "\n%s - (%d) error(s), (%d) warnging(s)\n",
            fn, g_err_msg_list.get_elem_count(),
            g_warn_msg_list.get_elem_count());
    g_err_msg_list.clean();
    g_warn_msg_list.clean();
    return res;
}


bool Compiler::compileCFile(CHAR const* fn)
{
    START_TIMER_FMT(t, ("Compile C File '%s'", fn));
    bool succ = true;
    xoc::TargInfo * ti = nullptr;
    xocc::CLRegionMgr * rm = nullptr;
    FileObj asmfo;
    xgen::CGMgr * cgmgr = nullptr;
    initCompile(fn, &rm, asmfo, &cgmgr, &ti);
    if (g_dump_file_name != nullptr) {
        rm->getLogMgr()->init(g_dump_file_name, true);
    }
    if (g_is_dump_option) {
        xoc::Option::dump(rm);
    }
    DeclAndVarMap dvmap(rm);
    CParser parser(rm->getLogMgr(), fn);
    if (g_err_msg_list.get_elem_count() > 0) {
        succ = false;
        goto FIN;
    }
    if (g_redirect_stdout_to_dump_file) {
        g_unique_dumpfile = rm->getLogMgr()->getFileHandler();
        ASSERT0(g_unique_dumpfile);
        g_unique_dumpfile_name = g_dump_file_name;
        ASSERT0(g_unique_dumpfile_name);
    }
    g_fe_sym_tab = rm->getSymTab();
    g_dbx_mgr = new CLDbxMgr();
    if (runFrontEnd(rm, parser) != ST_SUCC) {
        succ = false;
        ASSERTN(g_err_msg_list.has_msg(), ("miss error msg"));
        goto FIN;
    }

    //In the file scope, generate function region.
    if (g_dump_opt.isDumpAll()) {
        xfe::get_global_scope()->dump();
    }
    {
        dvmap.scanAndInitVar(xfe::get_global_scope());
        ((CLVarMgr*)rm->getVarMgr())->setDVMap(&dvmap);
        ASSERT0(rm->getVarMgr()->verifyAllVar());
        CScope2IR s2ir(rm, dvmap);
        if (s2ir.generateScope(xfe::get_global_scope())) {
            rm->compileRegionSet(fn);
        }
    }
    if (g_is_dumpgr) {
        rm->dumpProgramRegionGR(fn);
    }
FIN:
    if (g_dbx_mgr != nullptr) {
        delete g_dbx_mgr;
        g_dbx_mgr = nullptr;
    }
    //Timer use prt2C.
    END_TIMER_FMT(t, ("Total Time To Compile '%s'", fn));
    finiCompile(rm, asmfo, cgmgr, ti);
    show_err();
    show_warn();
    fprintf(stdout, "\n%s - (%d) error(s), (%d) warnging(s)\n",
            fn, g_err_msg_list.get_elem_count(),
            g_warn_msg_list.get_elem_count());
    g_err_msg_list.clean();
    g_warn_msg_list.clean();
    return succ;
}


bool Compiler::compileCFileList()
{
    for (CHAR const* fn = g_cfile_list.get_head(); fn != nullptr;
         fn = g_cfile_list.get_next()) {
        if (!compileCFile(fn)) {
            return false;
        }
    }
    return true;
}


bool Compiler::compileGRFileList()
{
    for (CHAR const* fn = g_grfile_list.get_head(); fn != nullptr;
         fn = g_grfile_list.get_next()) {
        if (!compileGRFile(fn)) {
            return false;
        }
    }
    return true;
}


bool Compiler::compile()
{
    bool succ = true;
    if (!compileGRFileList()) {
        succ = false;
    }
    if (!compileCFileList()) {
        succ = false;
    }
    return succ;
}
//END Compiler

} //namespace xocc
