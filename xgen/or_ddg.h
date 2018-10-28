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

author: Su Zhenyu
@*/
#ifndef __OR_DDG_H__
#define __OR_DDG_H__

namespace xgen {

class ParallelPartMgr;
class BBSimulator;

typedef enum {
    DEP_UNDEF    = 0x0,
    DEP_REG_FLOW = 0x1,   //register flow dependence
    DEP_REG_ANTI = 0x2,   //register anti dependence
    DEP_REG_OUT  = 0x4,   //register out dependence
    DEP_REG_READ = 0x8,   //register read dependence
    DEP_MEM_FLOW = 0x10,  //memory flow dependence
    DEP_MEM_ANTI = 0x20,  //memory anti dependence
    DEP_MEM_OUT  = 0x40,  //memory out dependence
    DEP_MEM_VOL  = 0x80,  //memory volatile dependence
    DEP_MEM_READ = 0x100, //memory read dependence
    DEP_CONTROL  = 0x200, //control dependence
    DEP_HYB      = 0x400, //hybrid dependence
} DEP_TYPE;


#define DDGEI_deptype(c)    ((DDGEdgeInfo*)(c))->deptype
class DDGEdgeInfo {
public:
    ULONG deptype;  //Dependence type
};


class DDGNodeInfo {
public:
    INT degree;
};


//Record OR scheduling info
class AovInfo {
public:
    INT estart;
    INT lstart;
};


//DataDepGraph Parameters
#define NO_MEM_OUT          false
#define INC_MEM_OUT         true
#define NO_MEM_FLOW         false
#define INC_MEM_FLOW        true
#define NO_MEM_READ         false
#define INC_MEM_READ        true
#define NO_MEM_ANTI         false
#define INC_MEM_ANTI        true
#define NO_REG_FLOW         false
#define INC_REG_FLOW        true
#define NO_REG_READ         false
#define INC_REG_READ        true
#define NO_REG_OUT          false
#define INC_REG_OUT         true
#define NO_REG_ANTI         false
#define INC_REG_ANTI        true
#define NO_CONTROL          false
#define INC_CONTROL         true
#define NO_PHY_REG          false
#define INC_PHY_REG         true
#define NO_SYM_REG          false
#define INC_SYM_REG         true


class DDGParam {
public:
    bool phy_reg_dep; //Build dep caused by physic-register.
    bool mem_read_read_dep; //Build dep caused by memory RAR(read-after-read).
    bool mem_flow_dep; //Build dep caused by memory flow-dep.
    bool mem_out_dep; //Build dep caused by memory out-dep.
    bool control_dep; //Build dep caused by control.

    //Build dep caused by register RAR(read-after-read).
    //NOTE: Build read-read dependence might
    //lead to larger DataDepGraph because there
    //may be a lot of ORs read same SR.
    bool reg_read_read_dep;

    bool reg_anti_dep; //Build dep caused by register anti-ors.
    bool mem_anti_dep; //Build dep caused by memory anti-ors.
    bool sym_reg_dep; //Build dep caused by symbol register.
};


#define DDG_DUMP_CLUSTER_INFO    0x1
#define DDG_DUMP_EDGE_INFO       0x2
#define DDG_DUMP_OP_INFO         0x4
#define DDG_DELETE               0x8
class DataDepGraph : public xcom::Graph {
protected:
    ORBB * m_bb;
    CG * m_cg;
    Vector<OR*> m_mapidx2or_map;
    Stack<DDGParam*> m_params_stack;
    ParallelPartMgr * m_ppm; //Parallel part manager
    DDGParam m_ddg_param;
    bool m_is_init;
    bool m_is_clonal;
    bool m_unique_mem_loc;
    Vector<UINT> m_estart_vec;
    Vector<UINT> m_lstart_vec;
    SMemPool * m_pool;

protected:
    virtual void * xmalloc(UINT size); //Given size of byte
    virtual DDGParam * dup_param()
    {
        DDGParam * ps = (DDGParam*)xmalloc(sizeof(DDGParam));
        *ps = m_ddg_param;
        return ps;
    }

    void * cloneEdgeInfo(xcom::Edge * e);
    void * cloneVertexInfo(xcom::Vertex * v);
    void handle_results(
            OR const* o,
            OUT Reg2ORList & map_reg2defors,
            OUT Reg2ORList & map_reg2useors,
            OUT SR2ORList & map_sr2defors,
            OUT SR2ORList & map_sr2useors);
    void handle_opnds(
            OR const* o,
            OUT Reg2ORList & map_reg2defors,
            OUT Reg2ORList & map_reg2useors,
            OUT SR2ORList & map_sr2defors,
            OUT SR2ORList & map_sr2useors);
    void handle_store(
            IN OR * o,
            ULONG mem_loc_idx,
            OUT UINT2ORList & map_memloc2defors,
            OUT UINT2ORList & map_memloc2useors);
    void handle_load(
            IN OR * o,
            ULONG mem_loc_idx,
            OUT UINT2ORList & map_memloc2defors,
            OUT UINT2ORList & map_memloc2useors);
public:
    DataDepGraph();
    ~DataDepGraph();

    virtual bool appendEdge(ULONG deptype, OR const* from, OR const* to);
    virtual void appendOR(OR * o);

    ORBB * bb() const;
    virtual void build();
    void buildRegDep();
    void buildMemFlowDep(OR * from, OR * to);
    void buildMemOutDep(OR * from, OR * to, VAR const* from_loc);
    void buildMemInDep(OR * from, OR * to, VAR const* from_loc);
    void buildMemVolatileDep(OR * from, OR * to);
    void buildMemDep();

    UINT computeEstartAndLstart(IN BBSimulator & sim, OUT OR ** fin_or);
    UINT computeCriticalPathLen(BBSimulator & sim);
    virtual void clone(DataDepGraph & ddg);
    void chainPredAndSucc(OR * o);

    virtual void destroy();
    void dump(INT flag = 0xF, INT rootoridx = -1, CHAR * name = NULL);

    virtual void init(ORBB * bb);
    bool isGraphNode(OR * o);
    bool is_param_equal(bool phy_reg_dep,
                        bool memread_dep,
                        bool memflow_dep,
                        bool memout_dep,
                        bool control_dep,
                        bool regread_dep,
                        bool reganti_dep,
                        bool memanti_dep,
                        bool symreg_dep) const;

    OR * getOR(UINT mapidx) const { return m_mapidx2or_map.get(mapidx); }
    OR * getFirstOR(INT & cur);
    OR * getNextOR(INT & cur);

    void get_neighbors(IN OUT ORList & nis, OR const* o);
    void getSuccsByOrder(IN OUT ORList & succs, IN OR * o);
    void getSuccsByOrderTraverseNode(IN OUT ORList & succs, OR const* o);
    void getPredsByOrderTraverseNode(IN OUT ORList & succs, OR const* o);
    void get_succs(IN OUT ORList & succs, OR const* o);
    void getPredsByOrder(IN OUT ORList & preds, IN OR * o);
    void get_preds(IN OUT List<xcom::Vertex*> & preds, IN xcom::Vertex * v);
    void get_preds(IN OUT ORList & preds, OR const* o);
    void getORListWhichAccessSameMem(OUT ORList & mem_ors, OR const* o);
    void getEstartAndLstart(
            OUT UINT & estart,
            OUT UINT & lstart,
            OR const* o) const;
    DDGParam * get_param() { return &m_ddg_param; }
    virtual INT get_slack(OR * o);


    virtual bool handleDedicatedSR(SR const* sr, OR const* o, bool is_result);

    //Return true if OR is on the critical path.
    bool isOnCriticalPath(OR * o);

    //Return true if or1 is independent with or2.
    bool is_dependent(OR const* or1, OR const* or2);

    //Return true if or1 is independent with or2.
    bool is_independent(OR * or1, OR * or2);

    virtual bool must_def_sp_reg(OR * o);

    virtual void removeOR(OR * o);
    virtual void removeORIdx(UINT oridx);
    virtual void removeEdge(OR * from, OR * to);
    virtual void removeEdge(UINT from, UINT to);
    void removeRedundantDep();
    virtual void reschedul();
    virtual void rebuild();

    void sortInTopological(OUT Vector<UINT> & vex_vec);
    void setParallelPartMgr(ParallelPartMgr * ppm);
    void simplifyGraph();
    void set_param(bool phy_reg_dep,
                   bool memread_dep,
                   bool memflow_dep,
                   bool memout_dep,
                   bool control_dep,
                   bool regread_dep,
                   bool reganti_dep,
                   bool memanti_dep,
                   bool symreg_dep);

    virtual void traverse();

    //Preparation for building graph.
    virtual void preBuild(){}
    virtual void pushParam();
    virtual void popParam();

    virtual void union_edge(List<OR*> & orlist, OR * tgt);

    //Should be removed from the basic class.
    virtual void predigest()
    { ASSERTN(0, ("Target Dependent Code")); }
};

} //namespace xgen
#endif
