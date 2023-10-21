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

#ifndef _DRIVER_H_
#define _DRIVER_H_

namespace xocc {

class DeclAndVarMap {
    COPY_CONSTRUCTOR(DeclAndVarMap);
protected:
    xoc::RegionMgr * m_rm;
    xoc::TypeMgr * m_tm;
    xoc::VarMgr * m_vm;
    xcom::TMap<xfe::Decl const*, xoc::Var*> m_decl2var_map;
    xcom::TMap<xoc::Var*, xfe::Decl const*> m_var2decl_map;
protected:
    //Transforming Decl into Var.
    //decl: variable declaration in front end.
    xoc::Var * addDecl(Decl const* decl);
    xoc::Type const* makeType(xfe::Decl const* decl);
    void scanDeclList(Scope const* s, Decl * decllist);
public:
    DeclAndVarMap(xoc::RegionMgr * rm) : m_rm(rm)
    { m_tm = m_rm->getTypeMgr(); m_vm = m_rm->getVarMgr(); }

    xoc::Var * mapDecl2Var(xfe::Decl const* decl) const
    { return m_decl2var_map.get(decl); }
    xfe::Decl const* mapVar2Decl(Var * var) const
    { return m_var2decl_map.get(var); }

    //Scan frontend score info and initialize XOC variable.
    void scanAndInitVar(xfe::Scope const* s);
};

//Exported Variables, only used in FE.
//If one requires to export variables, or types to other
//module, please put them in fexp.h.
extern UINT const g_formal_parameter_start;
extern xcom::List<CHAR const*> g_cfile_list; //record list of C source files.
extern xcom::List<CHAR const*> g_grfile_list; //record list of GR source files.

//If one requires to export variables, or types to other
//module, please put them in fexp.h.
extern CHAR const* g_output_file_name; //record the ASM file name.
extern CHAR const* g_xocc_version; //recod the xocc.exe version.
extern CHAR const* g_dump_file_name;
extern bool g_is_dumpgr;
extern bool g_is_dump_option;

class Compiler {
    COPY_CONSTRUCTOR(Compiler);
protected:
    CLRegionMgr * allocCLRegionMgr();

    bool compileCFile(CHAR const* fn);
    bool compileGRFile(CHAR const* fn);
    FILE * createAsmFileHandler(CHAR const* fn);

    void destructPRSSAForAllRegion(RegionMgr * rm);

    CLRegionMgr * initRegionMgr();
    void initCompile(CHAR const* fn, OUT CLRegionMgr ** rm,
                     OUT FILE ** asmh, OUT CGMgr ** cgmgr,
                     OUT TargInfo ** ti);

    void finiCompile(CLRegionMgr * rm, FILE * asmh, CGMgr * cgmgr,
                     TargInfo * ti);

    UINT runFrontEnd(RegionMgr * rm, CParser & parser);
public:
    Compiler() {}
    bool compileGRFileList();
    bool compileCFileList();
    bool compile();
};

} //namespace xocc
#endif
