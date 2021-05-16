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
#ifndef _OR_GRA_H_
#define _OR_GRA_H_

namespace xgen {

class OR_DF_MGR {
    CG * m_cg;
    xcom::BitSetMgr m_bs_mgr;
    Vector<xcom::BitSet*> m_def_var_set;
    Vector<xcom::BitSet*> m_use_var_set;

public:
    OR_DF_MGR(CG * cg) { m_cg = cg; }

    void computeLocalLiveness(ORBB * bb);
    void computeGlobalLiveness();
    void computeLiveness();

    void dump();

    xcom::BitSet * get_use_var(ORBB * bb);
    xcom::BitSet * get_def_var(ORBB * bb);
    Region * getRegion() const;
};


//
//START G_LIFE_TIME
//
#define GLT_id(g) ((g)->id)
#define GLT_sr(g) ((g)->sr)
#define GLT_livebbs(g) ((g)->livebbs)
class G_LIFE_TIME {
public:
    UINT id;
    SR * sr;
    xcom::BitSet * livebbs;
};
//END G_LIFE_TIME


typedef HMap<SR*, G_LIFE_TIME*> SR2GLT_MAP;

//GLT_MGR
#define GLT_MGR_ra_mgr(gm) ((gm).m_ramgr)
#define GLT_MGR_ru(gm) ((gm).rg)
#define GLT_MGR_sr2glt_map(gm) ((gm).m_sr2glt_map)
class GLT_MGR {
protected:
    SMemPool * m_pool;
    Vector<xcom::BitSet*> m_map_sr2livebb; //Map from SR to its liveness-bbs.
    Vector<LifeTimeMgr*> m_map_bb2ltmgr; //Map from BB id to LifeTimeMgr.
    Vector<G_LIFE_TIME*> m_id2glt_map; //Map from id to G_LIFE_TIME.
    xcom::BitSetMgr m_bs_mgr;
    UINT m_glt_count;
    CG * m_cg;

protected:
    void * xmalloc(UINT size)
    {
        ASSERTN(m_pool != nullptr,("need graph pool!!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p != nullptr);
        ::memset(p, 0, size);
        return p;
    }
public:
    RaMgr * m_ramgr;
    SR2GLT_MAP m_sr2glt_map;

public:
    GLT_MGR(RaMgr * ra_mgr, CG * cg);
    ~GLT_MGR();

    void build(IN OR_DF_MGR & df_mgr);

    void dump();

    G_LIFE_TIME * get_glt(UINT id);
    Region * getRegion() const;

    xcom::BitSet * map_sr2livebbs(SR * sr);
    LifeTimeMgr * map_bb2ltmgr(ORBB * bb);

    G_LIFE_TIME * new_glt(SR * sr);
};


//GInterfGraph
#define GIG_ru(g) ((g)->m_rg)
#define GIG_ra_mgr(g) ((g)->m_ramgr)
#define GIG_glt_mgr(g) ((g)->m_glt_mgr)

class GInterfGraph : public xcom::Graph {
public:
    CG * m_cg;
    RaMgr * m_ramgr;
    GLT_MGR * m_glt_mgr;

public:
    GInterfGraph(RaMgr * ra_mgr, GLT_MGR * glt_mgr, CG * cg);

    void dump(CHAR const* name = nullptr) const;
    bool isInterferred(IN G_LIFE_TIME * glt1, IN G_LIFE_TIME * glt2);
    void rebuild();
    void build();
};


//G_ACTION
#define G_ACTION_NON 0
#define G_ACTION_SPILL 1
#define G_ACTION_SPLIT 2
#define G_ACTION_DFS_REASSIGN_REGFILE 3
#define G_ACTION_BFS_REASSIGN_REGFILE 4
#define G_ACTION_MOVE_HOUSE 5
class G_ACTION {
    xcom::Vector<UINT> m_lt2action;
public:
    G_ACTION() {}
    ~G_ACTION() {}
    UINT get_action(G_LIFE_TIME * lt) { return m_lt2action.get(GLT_id(lt)); }
    void set_action(G_LIFE_TIME * lt, UINT action)
    { m_lt2action.set(GLT_id(lt), action); }
};


//GRA
#define GRA_ru(g) ((g)->m_rg)
#define GRA_ra_mgr(g) ((g)->m_ramgr)
class GRA {
protected:
    CG * m_cg;
    RaMgr * m_ramgr;
    //Supply callee-saved regs usage information for GRA followed phase.
    RegSet m_used_callee_save_regs[RF_NUM];
public:
    GRA(RaMgr * ra_mgr, CG * cg)
    {
        ASSERT0(ra_mgr != nullptr);
        m_ramgr = ra_mgr;
        m_cg = cg;
    }
    void assignRegFile(IN GLT_MGR & glt_mgr);
    void assignCluster();
    bool allocatePrioList(OUT List<G_LIFE_TIME*> & prio_list,
                          OUT List<G_LIFE_TIME*> & uncolored_list,
                          IN GInterfGraph & ig);
    void buildPriorityList(OUT List<G_LIFE_TIME*> & prio_list,
                           IN GInterfGraph & ig);
    RegSet * get_used_callee_save_regs() { return m_used_callee_save_regs; }
    void solveConflict(OUT List<G_LIFE_TIME*> & uncolored_list,
                       OUT List<G_LIFE_TIME*> & prio_list,
                       IN GInterfGraph & ig,
                       G_ACTION & action);
    void perform();
};

} //namespace xgen
#endif
