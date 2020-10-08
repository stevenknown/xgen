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
#ifndef _PARALLEL_PART_MGR_
#define _PARALLEL_PART_MGR_

namespace xgen {

class ReductionMgr {
protected:
    xcom::TMap<SR*, OR*> m_sr2redor_map; //Mapping from SR to OR
    xcom::TMap<SR*, OR*> m_sr2defor_map; //Mapping from SR to OR
public:
    void set(SR * sr, OR * red_or) { m_sr2redor_map.setAlways(sr, red_or); }
    OR * get(SR * sr) { return m_sr2redor_map.get(sr); }

    void set_live_def(SR * sr, OR * def_or)
    { m_sr2defor_map.setAlways(sr, def_or); }
    OR * get_live_def(SR * sr) { return m_sr2defor_map.get(sr); }
};


class ParallelPartMgr {
protected:
    List<List<OR*>*> m_para_part_orlst;
    List<xcom::BitSet*> m_para_part_oridx_lst;
    List<List<OR*>*> m_para_part_redor_lst;
    //Map original sr to sr in each of parallel part.
    List<SR2SR_DMAP*> m_sr2sr_dmap_lst;
    Vector<bool> m_regfile_unique;
    SRHash m_gsr;
    ReductionMgr m_red_mgr;
    ORBB * m_bb;
    ORBB * m_prolog;
    ORBB * m_epilog;
    OR * m_main_red_or;
    OR * m_cmp_or;
    SR * m_iv;
    SMemPool * m_pool;
    UINT m_num_cluster; //number of cluster that is distributable.
    CG * m_cg;

    void * xmalloc(INT size)
    {
        ASSERTN(m_pool, ("Graph must be initialized before clone."));
        ASSERTN(size > 0, ("xmalloc: size less zero!"));
        void * p = smpoolMalloc(size, m_pool);
        ASSERT0(p);
        ::memset(p,0,size);
        return p;
    }
public:
    ParallelPartMgr(ORBB * bb);
    COPY_CONSTRUCTOR(ParallelPartMgr);
    virtual ~ParallelPartMgr();
    void init(ORBB * bb);

    //Determine the number of clusters in which code can be distributed.
    virtual void computeNumOfParallelPart();
    virtual void computeUniqueRegFile(
            OR * o,
            Vector<bool> & is_regfile_unique);

    void destroy();
    void dupORForParallelPart();
    bool distributeToCluster(UINT n);
    bool distribute();

    void genReductionRestore(SR * red_var);
    List<OR*> * getClusterParallelPart(
            UINT n,
            OUT xcom::BitSet ** oridx_lst = NULL);
    List<OR*> * getClusterReductionOR(UINT n);
    SR2SR_DMAP * getClusterDMap(UINT n);
    CLUST getCluster(UINT n) const;
    virtual void genBusCopy(
                    ORBB * bb,
                    CLUST from_clust,
                    List<CLUST> & to_clust_lst,
                    SR * from_sr,
                    SRList & to_sr_lst);
    virtual void genPrologAndEpilog();
    virtual INT getClusterIdx(CLUST clt) const;
    virtual UINT getFirstClusterIdx() const;

    bool isReductionOR(OR const* o) const;

    SR * findIV(OR * o);
    virtual bool findMainIV(ORBB * bb,
                            DataDepGraph & ddg,
                            OR ** red_or,
                            OR ** cmp_or,
                            SR ** iv);

    bool hasPDomOcc(ORBB * bb, SR * gsr);
    bool hasParallelPart(CLUST clust) const;

    bool modifyReductionOR(OR * o, INT mul);
    bool modifyReductionOR();

    inline UINT numOfParallelPart() const
    {
        ASSERTN(m_pool, ("not yet initialize."));
        return m_num_cluster;
    }

    bool prepare_distribute(OR * red_or, OR * cmp_or, SR * iv);

    void renameSR(IN OUT OR * o, IN SR2SR_DMAP & dmap);

    void setBB(ORBB * bb) { m_bb = bb; }

    //Set cluster property of o.
    //'n': index of cluster, CLUS1_PART_IDX indicate the first cluster,
    //    next one is CLUS2_PART_IDX, and so on.
    void setCluster(OR * o, CLUST clst)
    {
        ASSERTN(m_pool, ("not yet initialize."));
        OR_clust(o) = clst;
    }

    virtual bool verifyReductionOR();
};


class ParallelPartMgrVec : public Vector<ParallelPartMgr*> {
public:
    ParallelPartMgrVec(){ init(); }
    ~ParallelPartMgrVec(){ destroy(); }
    void init()
    {
        if (Vector<ParallelPartMgr*>::is_init()) { return; }
        Vector<ParallelPartMgr*>::init();
    }
    void destroy()
    {
        if (!Vector<ParallelPartMgr*>::is_init()) { return; }
        for (INT i = 0; i <= get_last_idx(); i++) {
            ParallelPartMgr * ppm = get(i);
            if (ppm != NULL) {
                ppm->destroy();
            }
        }
        Vector<ParallelPartMgr*>::destroy();
    }
};

} //namespace xgen
#endif
