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
#ifndef _LRA_H_
#define _LRA_H_

namespace xgen {

#define LT_FIRST_POS         0
#define LT_LAST_POS          (mgr.getMaxLifeTimeLen() - 1)
#define HOLE_LENGTH          1 //Length of hole
#define HOLE_INTERF_LT_NUM   2 //Number of life times interferefering mutually .
#define UNNUMBERED_ORS       4

//Options to passes.
#define LRA_OPT_RCEL         0x1 //Redundant Code Elimination
#define LRA_OPT_RCIE         0x2 //Redundant Code Elimination Inversing
#define LRA_OPT_DDE          0x4 //Dead Def Elimination
#define LRA_OPT_SCH          0x8 //Func Unit Scheduling
#define LRA_OPT_HSL          0x10 //Hoisting Spill Location
#define LRA_OPT_CMO          0x20 //Code Motion
#define LRA_OPT_RSLE         0x40 //Redundant Store/Load Elimination
#define LRA_OPT_RCPE         0x80 //Redundant Copy Elimination
#define LRA_OPT_PG           0x100 //Partition Group of Instrutions
#define LRA_OPT_FINAL_OPT    0x200 //Finial Optimization of BB
#define LRA_OPT_CYCES        0x400 //Perform cyclic estimation of BB
#define LRA_OPT_RNSR         0x800 //Rename SR
#define LRA_OPT_LIS          0x1000 //Local Instruction Scheduling
#define LRA_VERIFY_REG       0x2000 //Verify Physical Register

//Internal Phases of LRA
#define PHASE_UNDEF               0x0
#define PHASE_INIT                0x1
#define PHASE_CA_DONE             0x2
#define PHASE_SCH_DONE            0x4
#define PHASE_RA_DONE             0x8
#define PHASE_FINIAL_FIXUP_DONE   0x10
#define PHASE_APPEND_CALLEE_SAVE  0x20

class RaMgr;
class CG;
class GRA;
class LifeTime;
class LifeTimeMgr;
class RefORBBList;

typedef xcom::TMapIter<xoc::VAR const*, RefORBBList*> VAR2ORIter;
typedef xcom::TMap<xoc::VAR const*, RefORBBList*> VAR2OR;
typedef xcom::TMap<xoc::VAR const*, RefORBBList*> VAR2OR;
typedef xcom::TMap<LifeTime*, SR*> LifeTime2SR;
typedef xcom::TMap<LifeTime*, List<LifeTime*>*> LifeTime2SibList;
typedef xcom::TMap<SR*, LifeTime*> SR2LifeTime;
typedef xcom::TMap<LifeTime const*, RegSet*> LifeTime2RegSet;
typedef xcom::TMap<LifeTime*, REG> LifeTime2Reg;
typedef xcom::List<LifeTime*> SibList;

class VAR_MAP {
    UINT m_num_or;
    Vector<UINT> m_oridx2vecidx;

    //Index refers to a variable of linear system.
    //such as, x0, x1, ...
    Vector<OR*> m_vecidx2or;
    Vector<OR*> m_oridx2or;
public:
    VAR_MAP(ORBB * bb);
    virtual ~VAR_MAP() {}
    virtual UINT map_or_cyc2varidx(UINT or_idx, UINT cyc);
    virtual void map_varidx2or_cyc(UINT var_idx, OUT OR * & o,  OUT UINT & cyc);
    virtual OR * map_vecidx2or(UINT i);
    virtual OR * map_oridx2or(UINT i);
    virtual UINT map_or2vecidx(OR * o);
    virtual UINT map_icc_varidx2coeff(UINT varidx);
    virtual UINT get_clust_num() const { return 2; }
    virtual UINT get_exec_cyc_of_bus_or() const { return 2; }
    virtual UINT get_issue_port_per_clust() const { return 3; }
};


#define POSINFO_is_def(c)    (c)->is_def
class PosInfo {
public:
    bool is_def;
};


#define LT_id(c)              (c)->id
#define LT_pos(c)             (c)->pos
#define LT_desc(c)            (c)->desc
#define LT_sr(c)              (c)->sr
#define LT_cluster(c)         (c)->cluster
#define LT_has_allocated(c)   (SR_phy_regid(LT_sr(c)) != REG_UNDEF)
#define LT_prio(c)            (c)->priority
#define LT_has_may_def(c)     (c)->has_may_def_point
#define LT_has_may_use(c)     (c)->has_may_use_point
#define LT_preferred_reg(c)   (c)->preferred_reg
class LifeTime {
public:
    UINT id;
    float priority;
    xcom::BitSet * pos;
    xcom::BSVec<PosInfo*> desc;
    SR * sr;
    CLUST cluster;
    REG preferred_reg; //TODO: enable a preferred register set

    bool has_may_def_point;
    bool has_may_use_point;

public:
    void dump(LifeTimeMgr * mgr);
};


#define ACTION_NON                   0
#define ACTION_SPILL                 1
#define ACTION_SPLIT                 2
#define ACTION_DFS_REASSIGN_REGFILE  3
#define ACTION_BFS_REASSIGN_REGFILE  4
#define ACTION_MOVE_HOUSE            5
//Finite Automata
class ACTION {
    Vector<UINT> m_lt2action;
    Vector<List<INT>*> m_lt2action_done; //Record lt actions has done.

    //Map 'Status' and 'Input' to 'Action'
    //UINT m_status_trans[MAX_ST][MAX_INPUT];
    SMemPool * m_pool;
    void *xmalloc(INT size);
public:
    ACTION();
    ~ACTION();
    UINT get_action(LifeTime * lt);
    List<INT> * get_action_done(LifeTime * lt);
    void set_action(LifeTime * lt, UINT action);
};


class HashFuncForLifeTime {
public:
    UINT get_hash_value(LifeTime * t, UINT bucket_size) const
    {
        ASSERT0(bucket_size != 0);
        return LT_id(t) % bucket_size;
    }

    UINT get_hash_value(xcom::OBJTY t, UINT bucket_size) const
    {
        ASSERT0(bucket_size != 0);
        return (UINT)(size_t)t % bucket_size;
    }

    bool compare(LifeTime * t1, LifeTime * t2) const
    { return LT_id(t1) == LT_id(t2); }

    bool compare(LifeTime * t1, xcom::OBJTY t2) const
    { return (UINT)(size_t)t2 == LT_id(t1); }
};

typedef Hash<LifeTime*, HashFuncForLifeTime> LifeTimeHash;


class ORMap {
    Vector<List<OR*>*> m_or2orlist_map;
    Vector<OR*> m_idx2or_map;
    ORList m_or_list; //Only for fast accessing one by one.
    bool m_is_init;
    SMemPool * m_pool;

    void *xmalloc(INT size);
public:
    ORMap();
    ~ORMap();
    void init();
    void destroy();
    ORList * get_ors();
    List<OR*> * get_or_ors(OR * o);
    void add_or(OR * o, OR * mapped);
    void add_ors(OR * o, ORList * mapped);
};


//For global optimization.
#define ORUNIT_or(ou)        (ou)->or
class ORUnit {
public:
    OR * o;
};


#define OR_BBUNIT_bb(bu)        (bu)->bb
#define OR_BBUNIT_or_list(bu)    (bu)->or_list
class ORBBUnit {
public:
    ORBB * bb;
    List<OR*> * or_list;
};


class GroupMgr {
    Vector<List<OR*>*> m_groupidx2ors_map;
    Vector<INT> m_oridx2group_map;
    bool m_is_init;
    ORBB * m_bb;
    CG * m_cg;
    SMemPool * m_pool;

    void * xmalloc(INT size);
public:
    GroupMgr(ORBB * bb, CG * cg);
    ~GroupMgr();
    void init(ORBB * bb, CG * cg);
    void destroy();
    inline INT get_groups() const;
    inline INT get_last_group() const;
    INT get_or_group(OR * o);
    inline List<OR*> * get_orlist_in_group(INT i);
    void add_or(OR * o, INT group);
    void add_ors(ORList & ors, INT group);
    void union_group(INT tgt, INT src);
    void dump();
};


//Record the relation in between VAR, and BB, OR which referred the VAR.
class RefORBBList : public List<ORBBUnit*> {
    SMemPool * m_pool;

    ORBBUnit * allocORBBUnit()
    {
        //TODO: Should utilizing FreeList.
        return (ORBBUnit*)xmalloc(sizeof(ORBBUnit));
    }

    void freeORBBUnit(ORBBUnit * bu) { OR_BBUNIT_or_list(bu)->destroy(); }

    void * xmalloc(INT size)
    {
        ASSERTN(m_pool, ("xcom::Graph must be initialized before clone."));
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }

public:
    RefORBBList()
    {
        m_pool = NULL;
        init();
    }
    ~RefORBBList() { destroy(); }

    ORBBUnit * get_bu(ORBB * bb);
    ORBBUnit * add_bb(ORBB * bb);
    OR * add_or(ORBB * bb, OR * o);
    List<OR*> * get_or_list(ORBB * bb);
    OR * removeOR(ORBB * bb, OR * o);
    ORBB * remove_bb(ORBB * bb);
    void init();
    void destroy();
};


//Manage the sibling relationship for each lifetime pair.
//Note LifeTime which has been sibling of some LT can
//be another lifetimes' sibling meanwhile after LRA spilling.
//So the relationship between lifetime and it's sibling is not unique.
//
//The physical registers of lt and it's sibling shoud be consecutive.
//The physical register of next-lt should greater than current lt.
//The physical register of prev-lt should less than current lt.
//next sibling lifetime, e.g: lt(r1)<->next_lt(r2)
//prev sibling lifetime, e.g: prev_lt(r1)<->lt(r2)
class SibMgr {
protected:
    LifeTime2SibList m_lt2nextsiblist;
    LifeTime2SibList m_lt2prevsiblist;
    List<SibList*> m_ltlist; //record the allocated map

protected:
    UINT countNumOfNextSib(LifeTime const* lt) const;
    UINT countNumOfPrevSib(LifeTime const* lt) const;

public:
    SibMgr() {}
    ~SibMgr() { destroy(); }

    void destroy();

    SibList * getPrevSibList(LifeTime * lt)
    { return m_lt2prevsiblist.get(lt); }

    LifeTime * getFirstPrevSib(LifeTime * lt)
    {
        SibList * siblist = m_lt2prevsiblist.get(lt);
        if (siblist != NULL) {
            return siblist->get_head();
        }
        return NULL;
    }

    SibList * getNextSibList(LifeTime * lt)
    { return m_lt2nextsiblist.get(lt); }

    LifeTime * getFirstNextSib(LifeTime * lt)
    {
        SibList * siblist = m_lt2nextsiblist.get(lt);
        if (siblist != NULL) {
            return siblist->get_head();
        }
        return NULL;
    }

    //Set prev and next are sibling lifetimes.
    void setSib(LifeTime * prev, LifeTime * next);

    UINT countNumOfSib(LifeTime const* lt) const;
};


//Life Time Manager
//Function Order:
//    1. construtor()
//    2. init();
//    3. allocLifeTime() for evey lifetime
#define DUMP_LT_FUNC_UNIT        1
#define DUMP_LT_CLUST            2
#define DUMP_LT_USABLE_REG       4
class LifeTimeMgr {
protected:
    bool m_is_init;
    bool m_is_verify;

    //True if cluster information must be checked during processing.
    bool m_clustering;
    ORBB * m_bb;
    UINT m_max_lt_len;
    UINT m_lt_count; //a counter for life time.
    LifeTimeHash m_lt_tab;
    SR2LifeTime m_sr2lt_map;
    Vector<OR*> m_pos2or_map;
    Vector<INT> m_or2pos_map;
    Vector<OR*> m_oridx2or_map;
    CG * m_cg;
    xoc::Region * m_ru;

    //Record the first(or named 'Prepend Op' in ORC),
    //spill/reload operation for livein/liveout gsr.
    //e.g: 1.t1 <- sp + Large_Num
    //     2.[t1] <- gsr2 //It's the focus.
    //Operation 2. is the spill operation for gsr2.
    //And similar for reload.
    BSVec<SR*> m_oridx2sr_livein_gsr_spill_pos;
    BSVec<SR*> m_oridx2sr_liveout_gsr_reload_pos;

    LifeTime2RegSet m_lt2usable_reg_set_map;
    LifeTime2RegSet m_lt2antici_reg_set_map;
    SMemPool * m_pool;
    RegSet m_gra_used;
    SibMgr m_sibmgr;

protected:
    void processFuncExitBB(
            IN OUT List<LifeTime*> & liveout_exitbb_lts,
            IN OUT LifeTimeHash & live_lt_list,
            INT pos);
    void processLiveOutSR(IN OUT LifeTimeHash & live_lt_list, INT pos);
    void reviseLTCase1(LifeTime * lt);
    void processLiveInSR(IN OUT LifeTimeHash & live_lt_list);
    void appendPosition(IN OUT LifeTimeHash & live_lt_list, INT pos);
    void recordPhysicalRegOcc(
            IN SR * sr,
            IN LifeTimeHash & live_lt_list,
            INT pos,
            IN PosInfo * pi);
    void * xmalloc(INT size)
    {
        ASSERTN(m_is_init, ("Life time manager should initialized first."));
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        ASSERTN(m_pool != NULL,("need graph pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }
public:
    explicit LifeTimeMgr(CG * cg);
    COPY_CONSTRUCTOR(LifeTimeMgr);
    virtual ~LifeTimeMgr() { destroy(); }

    void addAnticiReg(LifeTime const* lt, REG reg);
    LifeTime * allocLifeTime(SR * sr, OR * o);

    ORBB * bb() const { return m_bb; }

    void computeLifeTimeCluster(LifeTime * lt, OR * o);
    void computeUsableRegs();
    void computeLifeTimeUsableRegs(LifeTime * lt, RegSet * usable_rs);
    virtual void considerSpecialConstrains(
            OR * o,
            SR const* sr,
            RegSet & usable_regs);
    LifeTime * clone_lifetime(LifeTime * lt);
    bool clone(LifeTimeMgr & mgr);
    INT create();

    void destroy();
    void dump(UINT flag = DUMP_LT_FUNC_UNIT|DUMP_LT_CLUST|DUMP_LT_USABLE_REG);

    //Enumerate and collect function units that 'lt' traversed.
    virtual void enumTraversedUnits(LifeTime const* lt, OUT UnitSet & us);

    void freeLifeTime(LifeTime * lt);
    void freeAllLifeTime();

    CG * getCG() { return m_cg; }
    SibMgr * getSibMgr() { return &m_sibmgr; }
    OR * getOR(UINT pos);
    OR * getORByIdx(INT oridx);
    INT getPos(OR * o, bool is_result);
    UINT getOccCount(LifeTime * lt);
    UINT getMaxLifeTimeLen() const { return m_max_lt_len; }
    RegSet * getUsableRegSet(LifeTime * lt) const;
    RegSet * getAnticiRegs(LifeTime * lt);
    LifeTime * getLifeTime(UINT id);
    LifeTime * getLifeTime(SR * sr);
    UINT getLiftTimeCount() const;
    LifeTime * getNextLifeTime(INT & cur);
    LifeTime * getFirstLifeTime(INT & cur);
    void getOccInRange(
            INT start,
            INT end,
            IN LifeTime * lt,
            IN OUT List<INT> & occs);
    INT getBackwardOcc(INT pos, IN LifeTime * lt, IN OUT bool * is_def);
    INT getForwardOcc(INT pos, IN LifeTime * lt, IN OUT bool * is_def);
    INT get_backward_use_occ(INT pos, IN LifeTime * lt);
    INT getForwardOccForUSE(INT pos, IN LifeTime * lt);
    INT getBackwardOccForDEF(INT pos, IN LifeTime * lt);
    INT getForwardOccForDEF(INT pos, IN LifeTime * lt);
    virtual xcom::BSVec<SR*> * getGSRLiveinSpill();
    virtual xcom::BSVec<SR*> * getGSRLiveoutReload();
    virtual RegSet * getGRAUsedReg();

    //Record the preference register information at neighbour life time.
    virtual void handlePreferredReg(OR const*)
    {
        //Use assert instead of abstract-interface to enable allocating
        //the object of LifeTimeMgr.
        ASSERTN(0, ("Target Dependent Code"));
    }

    void init(ORBB * bb,
              bool is_verify = false,
              bool clustering = true); //Only avaible for multi-cluster machine

    //Check all occurrence of SR's lifetime, and retrue if all of them
    //are recalculable.
    virtual bool isRecalcSR(SR * sr);
    bool isContainOR(IN LifeTime * lt,
                     OR_TYPE ortype,
                     OUT List<OR*> * orlst);

    void setAnticiReg(LifeTime const* lt, RegSet * rset);
    void setUsableReg(LifeTime * lt, RegSet * rset);
    virtual void setGRAUsedReg(SR * sr);
    virtual void setGRALiveoutReload(OR * o, SR * sr);
    virtual void setGRALiveinSpill(OR * o, SR * sr);

    void removeLifeTime(LifeTime * lt);
    virtual void recomputeLTUsableRegs(LifeTime const* lt, RegSet * usable_rs);
    void reviseLifeTime(List<LifeTime*> & lts);
    INT recreate(ORBB * bb,
                 bool is_verify = false,
                 bool clustering = true); //Only avaible for multi-cluster machine

    //Pick out registers which should not be used as allocable register
    //This is always depend on individual target machine.
    virtual void pickOutUnusableRegs(LifeTime const*, RegSet &)
    {
        //Target Dependent Code.
        //Do nothing by default.
    }

    //Verfication to LifeTime and cluster information.
    virtual bool verifyLifeTime();
};


//REG file dependence information
#define RDGEI_exp_val(c)  ((RDG_EDGE_INFO*)(c))->expected_value
class RDG_EDGE_INFO {
public:
    INT expected_value;
};


#define RDGVI_val(c)      ((RDG_VERTEX_INFO*)(c))->value
class RDG_VERTEX_INFO {
public:
    INT value;
};


//Register File Affinity xcom::Graph.
class RegFileAffinityGraph : public xcom::Graph {
protected:
    ORBB * m_bb;
    CG * m_cg;
    SMemPool * m_pool;
    Vector<LifeTime*> m_id2lt; //map from id to life time.
    bool m_is_enable_far_edge; //Enable build edge for far distance.
    bool m_is_init;

    virtual void * cloneEdgeInfo(xcom::Edge * e);
    virtual void * cloneVertexInfo(xcom::Vertex * v);

    void * xmalloc(UINT size)
    {
        ASSERTN(m_is_init, ("xcom::Graph must be initialized before clone."));
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p, 0, size);
        return p;
    }
public:
    RegFileAffinityGraph();
    COPY_CONSTRUCTOR(RegFileAffinityGraph);
    virtual ~RegFileAffinityGraph();
    void init(ORBB * bb, bool is_enable_far = false);
    void destroy();
    virtual void build(LifeTimeMgr & mgr, DataDepGraph & ddg);
    void clone(RegFileAffinityGraph & rdg);
    RDG_EDGE_INFO * getEdgeInfo(UINT start, UINT end);
    ORBB * bb();
    xcom::Edge * clustering(SR * sr1, SR * sr2, LifeTimeMgr & mgr);
    bool isEnableFarEdge() const { return m_is_enable_far_edge; }
    void dump();

};


//Register Interference xcom::Graph.
//For the sake of convenience of life time analysis, interference graph
//will incorporate with life time manager intact synchronously.
class InterfGraph : public xcom::Graph {
private:
    ORBB * m_bb;
    bool m_is_init;

    //True if cluster information must be checked during processing.
    bool m_clustering;
    bool m_is_estimate;
    LifeTimeMgr * m_lt_mgr;
    List<LifeTime*> m_lt_rf_group[RF_NUM];
    List<LifeTime*> m_lt_cl_group[CLUST_NUM];
public:
    InterfGraph();
    COPY_CONSTRUCTOR(InterfGraph);
    virtual ~InterfGraph();
    virtual void clone(InterfGraph & ig);

    //Note clustering is only useful for multi-cluster machine.
    virtual void init(ORBB * bb, bool clustering = true, bool estimate = false);
    virtual void destroy();
    virtual void build(LifeTimeMgr & mgr);
    virtual void updateLifeTimeInterf(LifeTime * lt, INT prio_regfile);
    virtual void moveLifeTime(LifeTime * lt, CLUST from_clust, INT from_regfile,
             CLUST to_clust, INT to_regfile);
    virtual void rebuild();
    virtual void getLifeTimeList(List<LifeTime*> & lt_group, CLUST clust);
    virtual void getLifeTimeList(
            List<LifeTime*> & lt_group,
            REGFILE regfile);
    virtual void getLifeTimeList(
            List<LifeTime*> & lt_group,
            REGFILE regfile,
            INT start,
            INT end);
    virtual ORBB * bb();
    virtual bool isGraphNode(LifeTime * lt);
    virtual bool isInterferred(LifeTime * lt1, LifeTime * lt2);
    virtual void getNeighborList(
            IN OUT List<LifeTime*> & ni_list,
            LifeTime * lt);
    virtual UINT getInterfDegree(LifeTime * lt);
    virtual void dump();
};


//
//Instructions Partition
//
template <class Mat, class T> class InstructionPartition {
public:
    bool partition(ORBB * bb,
            DataDepGraph & ddg,
            Vector<bool> & is_regfile_unique);
    void formulateTargetFunction(
            OUT Mat & tgtf,
            IN VAR_MAP & vm,
            IN OR * last_sr,
            UINT num_cycs,
            UINT num_ors,
            UINT num_vars,
            UINT num_cst);
    void formulateMustScheduleConstrains(
            OUT Mat & eq,
            IN ORBB * bb,
            IN VAR_MAP & vm,
            UINT num_cycs,
            UINT num_ors,
            UINT num_vars,
            UINT num_cst);
    void formulateDependenceConstrains(
            OUT Mat & leq,
            IN ORBB * bb,
            IN DataDepGraph & ddg,
            IN VAR_MAP & vm,
            IN BBSimulator & sim,
            UINT num_cycs,
            UINT num_ors,
            UINT num_vars,
            UINT num_cst);
    void formulateIssueConstrains(
            OUT Mat & leq,
            IN VAR_MAP & vm,
            UINT num_cycs,
            UINT num_ors,
            UINT num_vars);
    void formulateInterClusterConstrains(
            OUT Mat & leq,
            OUT Mat & eq,
            OUT UINT & num_icc_vars,
            IN ORBB * bb,
            IN DataDepGraph & ddg,
            IN VAR_MAP & vm,
            UINT num_cycs,
            UINT num_ors,
            UINT num_vars,
            UINT num_cst);
    void format(OUT INTMat & sched_form,
                OUT INTMat & icc_form,
                IN ORBB * bb,
                IN DataDepGraph & ddg,
                IN VAR_MAP & vm,
                IN Mat & sol,
                UINT num_cycs,
                UINT num_icc_vars,
                UINT num_sr_vars,
                INT rhs_idx);
};


//This class record the number of REGFILE that each cluster have.
//Assign REGFILE to life time, and after assingnment,
//pruning interference graph due to that
//two life time may not interfered mutually with different
//REGFILE assigned.
#define CRI_regfile_count(cri, rf) ((cri).count[(rf)])
class ClustRegInfo {
public:
    INT count[RF_NUM];
};


//
//Local Resource Allocator
//
#define OR_BB_LEN_AFTER_RFA 1000
#define MAX_OPT_OR_BB_LEN 1000
#define MAX_SCH_OR_BB_LEN 1000
class LRA {
protected:
    ORBB * m_bb;
    SMemPool * m_mem_pool;
    INT m_cur_phase; //record current LRA's phase
    ParallelPartMgr * m_ppm; //parallel-partition mananger.
    RaMgr * m_ramgr;
    xoc::Region * m_ru;
    CG * m_cg;
    UINT m_opt_phase; //record optimizing option.
    List<DataDepGraph*> m_ddg_list;
    List<RegFileGroup*> m_rfg_list;
    List<BBSimulator*> m_simm_list;
    List<LIS*> m_lis_list;
    List<RegFileAffinityGraph*> m_rf_affi_list;
    Vector<bool> m_spilled_live_in_gsr;
    Vector<bool> m_spilled_live_out_gsr;
    List<SR*> m_spilled_gsr;

protected:
    bool checkSpillCanBeRemoved(xoc::VAR const* spill_loc);

    void genSRWith2opnds(
            OR_TYPE src,
            CLUST clust,
            SR * result,
            SR * src1,
            SR * src2,
            SR * pd,
            ORList & ors);

    void findFollowedLoad(
            OUT ORList & followed_lds,
            IN OR * o,
            xoc::VAR const* spill_loc,
            IN OUT bool & spill_can_be_removed,
            IN DataDepGraph & ddg);
    SR * findAvailPhyRegFromLoadList(
            IN OR * o,
            IN OUT bool & spill_can_be_removed,
            IN ORList & followed_lds,
            IN LifeTimeMgr & mgr,
            IN InterfGraph & ig);

    bool hoistSpillLocForStore(
            IN OR * o,
            IN InterfGraph & ig,
            IN LifeTimeMgr & mgr,
            IN OUT DataDepGraph & ddg,
            IN ORCt * orct,
            IN OUT ORCt ** next_orct);
public:
    LRA(IN ORBB * bb, IN RaMgr * ra_mgr);
    COPY_CONSTRUCTOR(LRA);
    virtual ~LRA() { smpoolDelete(m_mem_pool); }

    bool assignRegister(
            LifeTime * lt,
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            RegFileGroup * rfg);
    virtual void assignRegFileInResourceView(
            IN LifeTime * lt,
            IN List<REGFILE> & regfile_cand,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            IN LifeTimeMgr & mgr,
            IN RegFileAffinityGraph & rdg);
    virtual void assignRegFile(
            IN OUT ClustRegInfo cri[CLUST_NUM],
            Vector<bool> const& is_regfile_unique,
            IN LifeTimeMgr & mgr,
            IN DataDepGraph & ddg,
            IN RegFileAffinityGraph & rdg);
    virtual void assignPreferRegFilePressure(
            IN LifeTime * lt,
            IN ClustRegInfo cri[CLUST_NUM],
            IN List<REGFILE> & regfile_cand);
    void assignDesignatedRegFile(
            IN LifeTime * lt,
            REGFILE regfile,
            IN OUT ClustRegInfo cri[CLUST_NUM]);
    void assignPreferAnticipation(
            IN OUT LifeTime * lt,
            IN RegFileAffinityGraph & rdg,
            IN ClustRegInfo expvalue[CLUST_NUM],
            IN OUT ClustRegInfo cri[CLUST_NUM],
            REGFILE best_rf,
            REGFILE better_rf);
    virtual void assignDedicatedCluster() {}
    virtual void assignCluster(DataDepGraph & ddg,
            Vector<bool> & is_regfile_unique,
            bool partitioning);
    virtual bool allocatePrioList(
            List<LifeTime*> & prio_list,
            List<LifeTime*> & uncolored_list,
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            RegFileGroup * rfg);

    void buildPriorityList(
            IN OUT List<LifeTime*> & prio_list,
            IN InterfGraph & ig,
            IN LifeTimeMgr & mgr,
            DataDepGraph & ddg);

    //Return true if registers of all sibling of lt are continuous and valid.
    bool checkAndAssignPrevSiblingLT(
            REG treg,
            LifeTime const* lt,
            LifeTimeMgr * mgr,
            RegSet const* usable_rs);
    bool checkAndAssignNextSiblingLT(
            REG treg,
            LifeTime const* lt,
            LifeTimeMgr * mgr,
            RegSet const* usable_rs);
    virtual void chooseRegFileCandInLifeTime(
            IN UnitSet & us,
            IN LifeTime * lt,
            IN LifeTimeMgr & mgr,
            IN DataDepGraph & ddg,
            OUT List<REGFILE> & regfile_cand);
    void choose_regfile(IN LifeTime * lt,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            IN LifeTimeMgr & mgr,
            IN DataDepGraph & ddg,
            IN RegFileAffinityGraph & rdg);
    bool cse(IN OUT DataDepGraph & ddg, IN OUT Vector<bool> & handled);
    void coalesceCopy(OR * o, DataDepGraph & ddg, bool * is_resch);
    void coalesceMovi(
            OR * o,
            DataDepGraph & ddg,
            bool * is_resch,
            ORCt * orct,
            ORCt ** next_orct);
    bool coalescing(DataDepGraph & ddg, bool cp_any);
    virtual bool canOpndCrossCluster(OR * o);
    virtual bool canResultCrossCluster(OR * o);
    bool canBeSpillCandidate(LifeTime * lt, LifeTime * cand);
    bool canBeSpilled(LifeTime * lt, LifeTimeMgr & mgr);
    REG chooseByRegFileGroup(
            RegSet & regs,
            LifeTime * lt,
            LifeTimeMgr & mgr,
            RegFileGroup * rfg);
    virtual bool CodeMotion(DataDepGraph & ddg);
    void computeLTResideInHole(
            IN OUT List<LifeTime*> & reside_in_lts,
            IN LifeTime * lt,
            IN InterfGraph & ig,
            IN LifeTimeMgr & mgr);
    virtual LifeTime * computeBestSpillCand(
            LifeTime * lt,
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            bool try_self,
            bool * has_hole);
    void computeUniqueRegFile(IN OUT Vector<bool> & is_regfile_unique);

    //Compute the cost for copying SR from
    virtual float computeCopyCost(OR const*) const { return 1.0f; }

    //Compute the spill cost for spilling given SR of OR.
    virtual float computeSpillCost(SR const*, OR const*) const { return 1.0f; }

    //Compute the reload cost for reloading given SR of OR.
    virtual float computeReloadCost(SR const*, OR const*) const { return 1.0f; }
    virtual float computeRematCost(SR * sr, OR * o);
    virtual float computePrority(
            REG spill_location,
            LifeTime * lt,
            LifeTimeMgr & mgr,
            DataDepGraph & ddg);
    virtual RegSet & computeUnusedRegSet(
            REGFILE regfile,
            INT start,
            INT end,
            InterfGraph & ig,
            RegSet & rs);

    //Return true if given hardware resource can be used as spill location.
    //This is up to the hardware constrains.
    //This function always invoked by register hoisting which can spilling
    //a value into another register.
    virtual bool canBeSpillLoc(CLUST, REGFILE) { return true; }
    virtual REG chooseAvailSpillLoc(
            CLUST clust,
            INT start,
            INT end,
            InterfGraph & ig);
    virtual CLUST chooseDefaultCluster(OR * o);
    virtual void chooseExpectClust(
            ORList const ors[CLUST_NUM],
            IN OUT List<CLUST> & expcls);
    virtual bool cyc_estimate(
            IN DataDepGraph & ddg,
            IN OUT BBSimulator * sim,
            IN OUT Vector<bool> & is_regfile_unique);

    void dumpPrioList(List<LifeTime*> & prio_list);
    void deductORCrossBus(DataDepGraph & ddg);

    bool elimRedundantStoreLoad(DataDepGraph & ddg);
    bool elimRedundantCopy(bool gra_enable);
    bool elimRedundantUse(DataDepGraph & ddg);
    bool elimRedundantUse(LifeTimeMgr & mgr);
    bool elimRedundantRegDef(DataDepGraph & ddg);

    //Do some instruction amendments. This is target dependent.
    virtual bool fixup() { return false; }
    virtual CLUST findOpndMustBeCluster(IN OR * o, bool reassign_regfile);
    virtual CLUST findOpndExpectCluster(IN OR * o, IN DataDepGraph & ddg);
    virtual CLUST findResultMustBeCluster(IN OR * o, bool reassign_regfile);
    virtual CLUST findResultExpectCluster(IN OR * o, IN DataDepGraph & ddg);
    void freeRfgList();
    void freeDdgList();
    void freeSimmList();
    void freeLisList();
    void freeRFAffineList();
    void finalLRAOpt(LifeTimeMgr * mgr, InterfGraph * ig, DataDepGraph * ddg);

    bool getResideinHole(
            OUT INT * startpos,
            OUT INT * endpos,
            IN LifeTime * owner,
            IN LifeTime * inner,
            IN LifeTimeMgr & mgr);
    bool getMaxHole(
            OUT INT * startpos,
            OUT INT * endpos,
            IN LifeTime * lt,
            InterfGraph & ig,
            IN LifeTimeMgr & mgr,
            INT info_type);
    void genSpill(
            IN LifeTime * lt,
            IN SR * oldsr,
            INT pos,
            IN xoc::VAR * spill_var,
            IN LifeTimeMgr & mgr,
            bool is_rename,
            OUT ORList * sors);
    void genCopyOR(
            CLUST clust,
            UNIT unit,
            SR * src,
            SR * tgt,
            SR * pd,
            ORList & ors);
    SR * genNewReloadSR(SR * oldsr, xoc::VAR * spill_var);
    SR * genReload(
            IN LifeTime * lt,
            IN SR * oldsr,
            INT pos,
            IN xoc::VAR * spill_var,
            IN LifeTimeMgr &mgr,
            OUT ORList * ors);

    bool hasMemSideEffect(OR * o);
    bool hasSideEffect(OR * o);
    bool hasSideEffectResult(OR * o);
    virtual bool assignUniqueRegFile(SR * sr, OR * o, bool is_result);
    virtual bool hoistSpillLoc(
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            DataDepGraph & ddg);

    virtual bool isAllAllocated(IN OUT OR ** wor);
    bool isSRAffectClusterAssign(SR * sr);
    bool isMultiCluster();
    bool isAlwaysColored(
            LifeTime * lt,
            InterfGraph & ig,
            LifeTimeMgr & mgr);
    virtual bool isReasonableCluster(
            CLUST clust,
            List<OR*> & es_or_list,
            DataDepGraph & ddg,
            Vector<bool> const& is_regfile_unique);
    bool isOpt() const
    {
        return (m_opt_phase &
           (LRA_OPT_RCEL |
            LRA_OPT_RCIE |
            LRA_OPT_DDE |
            LRA_OPT_SCH |
            LRA_OPT_HSL |
            LRA_OPT_CMO |
            LRA_OPT_RSLE |
            LRA_OPT_RCPE |
            LRA_OPT_PG |
            LRA_OPT_FINAL_OPT |
            LRA_OPT_CYCES |
            LRA_OPT_RNSR |
            LRA_OPT_LIS)) != 0;
    }


    virtual void markRegFileUnique(Vector<bool> & is_regfile_unique);
    virtual void middleLRAOpt(
            IN OUT DataDepGraph & ddg,
            IN OUT LifeTimeMgr & mgr,
            IN OUT BBSimulator & sim,
            IN Vector<bool> & is_regfile_unique,
            IN OUT ClustRegInfo cri[CLUST_NUM]);
    bool mergeRedundantStoreLoad(
            OR * o,
            OR * succ,
            ORList & remainder_succs,
            ORBB * bb,
            xoc::VAR const* spill_var,
            DataDepGraph & ddg);
    void chooseBestRegFileFromMultipleCand(
            IN LifeTime * lt,
            IN List<REGFILE> & regfile_cand,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            IN LifeTimeMgr & mgr,
            IN RegFileAffinityGraph & rdg);
    bool moving_house(
            LifeTime * lt,
            List<LifeTime*> & prio_list,
            List<LifeTime*> & uncolored_list,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            Vector<bool> const& is_regfile_unique,
            LifeTimeMgr & mgr,
            InterfGraph & ig,
            ACTION & action,
            RegFileGroup * rfg);

    virtual RegFileAffinityGraph * allocRegFileAffinityGraph();
    virtual BBSimulator * allocBBSimulator();
    virtual LIS * allocLIS(
            DataDepGraph * ddg,
            BBSimulator * sim,
            UINT sch_mode,
            bool change_slot,
            bool change_cluster);
    virtual DataDepGraph * allocDDG();
    virtual RegFileGroup * allocRegFileGroup();
    virtual InterfGraph * allocInterfGraph();

    virtual bool optimal_partition(
            DataDepGraph & ddg,
            Vector<bool> & is_regfile_unique);

    virtual bool PureAssignCluster(
            IN OR * o,
            IN OUT ORCt ** next_orct,
            IN DataDepGraph & ddg,
            Vector<bool> const& is_regfile_unique);
    virtual bool partitionGroup(
            DataDepGraph & ddg,
            Vector<bool> & is_regfile_unique);
    virtual bool preOpt(IN OUT DataDepGraph & ddg);
    bool processORSpill(
            OR * sw,
            LifeTimeMgr & mgr,
            List<LifeTime*> & uncolored_list);
    void pure_spill(LifeTime * lt, LifeTimeMgr & mgr);
    virtual void preLRA()
    {
        if (HAVE_FLAG(m_opt_phase, LRA_VERIFY_REG)) {
            ASSERT0(verifyRegSR());
        }
    }
    virtual void postLRA();
    virtual bool perform();

    virtual void refineAssignedRegFile(
            IN LifeTimeMgr & mgr,
            Vector<bool> const& is_regfile_unique,
            IN OUT ClustRegInfo cri[CLUST_NUM]);
    bool reviseORBase(LifeTimeMgr & mgr, List<LifeTime*> & uncolored_list);
    virtual void renameOpndsFollowedLT(
            SR * oldsr,
            SR * newsr,
            INT start,
            LifeTime * lt,
            LifeTimeMgr & mgr);
    virtual void renameOpndInRange(
            SR * oldsr,
            SR * newsr,
            INT start,
            INT end,
            LifeTime * lt,
            LifeTimeMgr & mgr);
    void reassignRegFileForNewSR(
            IN OUT ClustRegInfo cri[CLUST_NUM],
            IN LifeTimeMgr & mgr,
            IN DataDepGraph & ddg);
    void reallocateLifeTime(
            List<LifeTime*> & prio_list,
            List<LifeTime*> & uncolored_list,
            LifeTimeMgr & mgr,
            DataDepGraph & ddg,
            RegFileGroup * rfg,
            InterfGraph & ig,
            IN OUT ClustRegInfo cri[CLUST_NUM]);
    virtual bool removeRedundantStoreLoadAfterLoad(
            OR * o,
            ORList & succs,
            ORBB * bb,
            xoc::VAR const* spill_var,
            DataDepGraph & ddg);
    virtual void resetLifeTimeAllocated(LifeTimeMgr & mgr);
    virtual bool reassignRegFile(
            LifeTime * lt,
            List<LifeTime*> & prio_list,
            List<LifeTime*> & uncolored_list,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            Vector<bool> const& is_regfile_unique,
            LifeTimeMgr & mgr,
            InterfGraph & ig,
            RegFileGroup * rfg);

    //Revise inter-cluster data transfer operation(bus OR) if necessary.
    //Return true if 'ddg' need to be update.
    virtual bool reviseInterClusterOR(
            DataDepGraph &,
            IN Vector<bool> & is_regfile_unique)
    {
        if (!isMultiCluster()) { return false; }
        DUMMYUSE(is_regfile_unique);
        return false;
    }

    //Refining scheduling result, return true if DDG need to be updated.
    virtual bool refineScheduling(
            CLUST,
            IN OUT ORList &,
            IN LifeTimeMgr &,
            IN DataDepGraph &,
            IN Vector<bool> & regfile_unique,
            IN BBSimulator *)
       {
        DUMMYUSE(regfile_unique);
        ASSERTN(0, ("Target Dependent Code"));
        return false;
    }
    virtual void resetClustRegInfo(ClustRegInfo * cri, UINT num);
    virtual void reviseMiscOR();
    virtual bool updateInfoEffectedByInlineASM();
    virtual void resetGSRSpillLocation();
    virtual void renameSR();

    virtual bool schedulFuncUnit(
            IN LifeTimeMgr & mgr,
            IN DataDepGraph & ddg,
            IN OUT BBSimulator * sim,
            IN OUT Vector<bool> & is_regfile_unique,
            ClustRegInfo cri[CLUST_NUM]);
    bool solveConflictRecursive(
            LifeTime * lt,
            List<LifeTime*> & uncolored_list,
            List<LifeTime*> & prio_list,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            Vector<bool> const& is_regfile_unique,
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            DataDepGraph & ddg,
            RegFileGroup * rfg,
            ACTION & action);
    bool solveConflict(
            List<LifeTime*> & uncolored_list,
            List<LifeTime*> & prio_list,
            IN OUT ClustRegInfo cri[CLUST_NUM],
            Vector<bool> const& is_regfile_unique,
            InterfGraph & ig,
            LifeTimeMgr & mgr,
            DataDepGraph & ddg,
            RegFileGroup * rfg,
            ACTION & action);
    void setOptPhase(UINT opt_phase) { m_opt_phase = opt_phase; }
    void setParallelPartMgr(ParallelPartMgr * ppm) { m_ppm = ppm; }
    void show_phase(CHAR * phase_name);
    void spillPassthroughGSR(LifeTime * lt, LifeTimeMgr & mgr);
    void spillGSR(LifeTime * lt, LifeTimeMgr & mgr);
    void spillLSR(LifeTime * lt, LifeTimeMgr & mgr);
    void spill(LifeTime * lt,
               List<LifeTime*> & prio_list,
               List<LifeTime*> & uncolored_list,
               LifeTimeMgr & mgr,
               DataDepGraph & ddg,
               RegFileGroup * rfg,
               InterfGraph & ig,
               REG spill_location,
               ACTION & action,
               IN OUT ClustRegInfo cri[CLUST_NUM]);
    bool split(LifeTime * lt,
               List<LifeTime*> & prio_list,
               List<LifeTime*> & uncolored_list,
               LifeTimeMgr & mgr,
               DataDepGraph & ddg,
               RegFileGroup * rfg,
               InterfGraph & ig,
               REG spill_location,
               ACTION & action,
               IN OUT ClustRegInfo cri[CLUST_NUM]);
    void splitLTAt(
            INT start,
            INT end,
            bool is_start_spill,
            bool is_end_spill,
            LifeTime * lt,
            LifeTimeMgr & mgr);
    void splitOneLT(
            LifeTime * lt,
            List<LifeTime*> & prio_list,
            List<LifeTime*> & uncolored_list,
            LifeTimeMgr & mgr,
            InterfGraph & ig,
            REG spill_location,
            ACTION & action);
    bool splitTwoLTContained(
            LifeTime * lt1,
            LifeTime * lt2,
            LifeTimeMgr & mgr);
    bool splitTwoLTCross(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr);
    bool splitTwoLT(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr);
    bool spillFirstDef(LifeTime * lt1, LifeTime * lt2, LifeTimeMgr & mgr);
    void selectReasonableSplitPos(
            IN OUT INT * pos1,
            IN OUT INT * pos2,
            IN OUT bool * is_pos1_spill,
            IN OUT bool * is_pos2_spill,
            IN LifeTime * lt,
            IN LifeTimeMgr & mgr);

    virtual bool tryAssignCluster(
            CLUST exp_clust,
            List<OR*> * orlist,
            Vector<bool> & is_regfile_unique,
            ORList ors[CLUST_NUM]);

    virtual bool verifyUsableRegSet(LifeTimeMgr & mgr);
    virtual void verifyRegFileForOpnd(OR * o, INT opnd, bool is_result);
    virtual void verifyRegFile();
    virtual bool verifyCluster();
    bool verifyRegSR();

    bool verify(LifeTimeMgr & mgr, InterfGraph & ig);
};
//END LRA

} //namespace xgen
#endif
