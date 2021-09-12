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
#ifndef __OR_DDG_H__
#define __OR_DDG_H__

namespace xgen {

class ParallelPartMgr;
class BBSimulator;

typedef enum {
    DEP_UNDEF = 0x0,
    DEP_REG_FLOW = 0x1, //register flow dependence
    DEP_REG_ANTI = 0x2, //register anti dependence
    DEP_REG_OUT  = 0x4, //register out dependence
    DEP_REG_READ = 0x8, //register read dependence
    DEP_MEM_FLOW = 0x10, //memory flow dependence
    DEP_MEM_ANTI = 0x20, //memory anti dependence
    DEP_MEM_OUT = 0x40, //memory out dependence
    DEP_MEM_VOL = 0x80, //memory volatile dependence
    DEP_MEM_READ = 0x100, //memory read dependence
    DEP_CONTROL = 0x200, //control dependence
    DEP_PHY_REG = 0x400, //physic-register dependence.
    DEP_SYM_REG = 0x800, //symbol register dependence.
    DEP_HYB = 0x1000, //hybrid dependence
} DEP_TYPE;


#define DDGEI_deptype(c) (((DDGEdgeInfo*)(c))->deptype)
class DDGEdgeInfo {
public:
    ULONG deptype; //Dependence type
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


#define DDG_DUMP_CLUSTER_INFO 0x1
#define DDG_DUMP_EDGE_INFO 0x2
#define DDG_DUMP_OP_INFO 0x4
#define DDG_DELETE 0x8

typedef UINT DDGParam;

class DataDepGraph : public xcom::Graph {
protected:
    ORBB * m_bb;
    CG * m_cg;
    ORMgr * m_ormgr;
    ParallelPartMgr * m_ppm; //Parallel part manager
    SMemPool * m_pool;
    Stack<DDGParam> m_params_stack;
    DDGParam m_ddg_param;
    RegSet m_tmp_alias_regset; //for local tmp used.

    typedef TMap<UINT, UINT> ID2Cyc;
    ID2Cyc m_estart;
    ID2Cyc m_lstart;
    bool m_is_init;
    bool m_is_clonal;
    bool m_unique_mem_loc;
protected:
    //This function will establish dependency for memory operation one by one
    //rather than doing any analysis which is in order to speedup compilation.
    void buildMemDepWithOutAnalysis();
    void buildRegDep();
    void buildMemFlowDep(OR const* from, OR const* to);
    void buildMemOutDep(OR const* from, OR const* to, Var const* from_loc);
    void buildMemInDep(OR const* from, OR const* to, Var const* from_loc);
    void buildMemVolatileDep(OR const* from, OR const* to);
    void buildMemDep();

    void * cloneEdgeInfo(xcom::Edge * e);
    void * cloneVertexInfo(xcom::Vertex * v);
    //Collect the alias register into set.
    //Registers in alias set will be add dependence in building DDG.
    virtual void collectAliasRegSet(REG reg, OUT RegSet & alias_regset);

    void handleResultsPhyRegOut(OR const* o, REG reg,
                                OUT Reg2ORList & map_reg2defors,
                                OUT Reg2ORList & map_reg2useors);
    void handleResultsPhyRegOut(OR const* o, SR const* sr,
                                OUT Reg2ORList & map_reg2defors,
                                OUT Reg2ORList & map_reg2useors);
    void handleResultsSymRegOut(OR const* o, SR const* sr,
                                OUT SR2ORList & map_sr2defors,
                                OUT SR2ORList & map_sr2useors);
    void handleOpndsPhyRegFlow(OR const* o, REG reg,
                               OUT Reg2ORList & map_reg2defors,
                               OUT Reg2ORList & map_reg2useors);
    void handleOpndsPhyRegFlow(OR const* o, SR const* sr,
                               OUT Reg2ORList & map_reg2defors,
                               OUT Reg2ORList & map_reg2useors);
    void handleOpndsSymRegRead(OR const* o, SR const* sr,
                               OUT SR2ORList & map_sr2defors,
                               OUT SR2ORList & map_sr2useors);
    void handleResults(OR const* o,
                       OUT Reg2ORList & map_reg2defors,
                       OUT Reg2ORList & map_reg2useors,
                       OUT SR2ORList & map_sr2defors,
                       OUT SR2ORList & map_sr2useors);
    void handleOpnds(OR const* o,
                     OUT Reg2ORList & map_reg2defors,
                     OUT Reg2ORList & map_reg2useors,
                     OUT SR2ORList & map_sr2defors,
                     OUT SR2ORList & map_sr2useors);
    void handleStore(OR const* o, ULONG mem_loc_idx,
                     OUT UINT2ConstORList & map_memloc2defors,
                     OUT UINT2ConstORList & map_memloc2useors);
    void handleLoad(OR const* o, ULONG mem_loc_idx,
                    OUT UINT2ConstORList & map_memloc2defors,
                    OUT UINT2ConstORList & map_memloc2useors);

    virtual void * xmalloc(UINT size); //Given size of byte
public:
    DataDepGraph();
    virtual ~DataDepGraph();

    virtual bool appendEdge(ULONG deptype, OR const* from, OR const* to);
    virtual void appendOR(OR * o);

    virtual void build();
    void buildSerialDependence();

    //Return memory operations which may access same memory location.
    //Calculate the length of critical path.
    //fin_or: record the OR which at the maximum path length.
    //Record the result at Estart and Lstart table.
    UINT computeEstartAndLstart(BBSimulator const& sim, OUT OR ** fin_or);
    UINT computeCriticalPathLen(BBSimulator const& sim) const;
    virtual void clone(DataDepGraph & ddg);
    void chainPredAndSucc(OR * o);
    //Count memory usage for current object.
    size_t count_mem() const
    {
        return sizeof(DataDepGraph) + smpoolGetPoolSize(m_pool) +
               m_params_stack.count_mem() +
               m_estart.count_mem() + m_lstart.count_mem();
    }

    virtual void destroy();
    void dump(INT flag = 0xF, INT rootoridx = -1,
              CHAR const* name = nullptr) const;

    ORBB * getBB() const
    {
        ASSERTN(m_is_init, ("xcom::Graph still not yet initialize."));
        return m_bb;
    }
    //The returned OR may be changed.
    OR * getOR(UINT orid) const { return m_ormgr->getOR(orid); }
    OR * getFirstOR(INT & cur);
    OR * getNextOR(INT & cur);
    void get_neighbors(MOD ORList & nis, OR const* o);
    void getSuccsByOrder(MOD ORList & succs, IN OR * o);
    void getSuccsByOrderTraverseNode(MOD ORList & succs, OR const* o);

    //Return all dependent predecessors in lexicographicly order.
    //e.g:a->b->c->d, a->d
    //  c and a are immediate predecessors of d, whereas a,b,c are
    //  dependent predecessors of d.
    void getPredsByOrderTraverseNode(MOD ORList & succs, OR const* o);

    //Return all dependent predecessors of OR.
    //e.g:a->b->c->d, a->d
    //  c and a are immediate predecessors of d, whereas a,b,c are
    //  dependent predecessors of d.
    void getDependentPreds(OUT ORTab & preds, OR const* o);
    void get_succs(MOD ORList & succs, OR const* o);
    void getPredsByOrder(MOD ORList & preds, IN OR * o);
    void get_preds(MOD List<xcom::Vertex*> & preds, IN xcom::Vertex * v);
    void get_preds(MOD ORList & preds, OR const* o);
    void getORListWhichAccessSameMem(OUT ORList & mem_ors, OR const* o);
    void getEstartAndLstart(OUT UINT & estart, OUT UINT & lstart,
                            OR const* o) const;
    DDGParam get_param() { return m_ddg_param; }
    virtual INT get_slack(OR const* o) const;

    //Return true if 'sr' need to be processed.
    //If return true, 'sr' and 'or' will be analyzed to build
    //dependence if there exist use-OR or def-OR.
    //The decision always used to reduce compilation time and memory.
    virtual bool handleDedicatedSR(SR const* sr, OR const* o,
                                   bool is_result) const;
    bool has_mem_out_dep() const { return HAVE_FLAG(m_ddg_param, DEP_MEM_OUT); }
    bool has_mem_flow_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_MEM_FLOW); }
    bool has_mem_read_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_MEM_READ); }
    bool has_mem_anti_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_MEM_ANTI); }
    bool has_reg_flow_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_REG_FLOW); }
    bool has_reg_read_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_REG_READ); }
    bool has_reg_out_dep() const { return HAVE_FLAG(m_ddg_param, DEP_REG_OUT); }
    bool has_reg_anti_dep() const
    { return HAVE_FLAG(m_ddg_param, DEP_REG_ANTI); }
    bool has_control_dep() const { return HAVE_FLAG(m_ddg_param, DEP_CONTROL); }
    bool has_phy_reg_dep() const { return HAVE_FLAG(m_ddg_param, DEP_PHY_REG); }
    bool has_sym_reg_dep() const { return HAVE_FLAG(m_ddg_param, DEP_SYM_REG); }
    bool has_hyb() const { return HAVE_FLAG(m_ddg_param, DEP_HYB); }

    virtual void init(ORBB * bb);
    bool isGraphNode(OR const* o) const;
    //Return true if current dep graph parameters identical with inputs.
    bool is_param_equal(DDGParam param) const { return m_ddg_param == param; }
    //Return true if OR is on the critical path.
    bool isOnCriticalPath(OR const* o) const;

    //Return true if or1 is dependent with or2.
    bool is_dependent(OR const* or1, OR const* or2) const;

    //Return true if or1 is independent with or2.
    bool is_independent(OR const* or1, OR const* or2) const;

    virtual bool mustDefSP(OR const* o) const;

    virtual void removeOR(OR * o);
    virtual void removeORIdx(UINT oridx);
    virtual void removeEdge(OR * from, OR * to);
    virtual void removeEdge(UINT from, UINT to);
    void removeRedundantDep();
    virtual void reschedul();
    virtual void rebuild();

    void setParallelPartMgr(ParallelPartMgr * ppm);
    void simplifyGraph();
    void setParam(DDGParam param) { m_ddg_param = param; }
    virtual void traverse();

    virtual void pushParam() { m_params_stack.push(m_ddg_param); }
    virtual void popParam() { m_ddg_param = m_params_stack.pop(); }

    //Add edge between OR in 'orlist' and 'tgt'.
    virtual void unifyEdge(List<OR*> & orlist, OR * tgt);

    //Should be removed from the basic class.
    virtual void predigest() { ASSERTN(0, ("Target Dependent Code")); }
};

} //namespace xgen
#endif
