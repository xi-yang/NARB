#ifndef __PCEN_MRN_CG_HH__
#define __PCEN_MRN_CG_HH__

#include "rce_types.hh"
#include <vector>
#include <list>
#include <algorithm>
#include <functional>
#include "rce_pcen.hh"
#include "pcen_ksp.hh"
using namespace std;

class PCENCGLink;
class PCEN_MRN_CG;
class PCENCGNode : public PCENNode
{
public:
    int lcl_endID;
    int rmt_endID;
    u_char swType;
    u_char encoding;//changed 07/14
    float bandwidth;//changed 07/15
    double LinkMetric;
    list<PCENCGLink*> out_CGlinks;
    list<PCENCGLink*> in_CGlinks;
    list<PCENCGLink*> path_mrn;
    friend class PCEN_MRN_CG;
public:
    PCENCGNode(): PCENNode() {}
    PCENCGNode(int id) : PCENNode(id) {}

    void FilterCGNode();
    void RestoreCGNodeFilter();

    void MaskCGNode();
    void RestoreCGNodeMask();
    bool operator< (const PCENCGNode& node) const { return minCost<node.minCost; }
};

class PCENCGLink : public PCENLink
{
public:
    PCENCGNode* lcl_end;
    PCENCGNode* rmt_end;

    PCENCGLink(int id, int localNodeId, int remoteNodeId, vector<PCENCGNode *> &CGnodes);
    PCENCGNode* search_PCENCGNode(vector<PCENCGNode*> &CGnodes, int NodeId);
};

class PathT_MRN : public PathT
{
public:
    list<PCENCGLink*> path_mrn;
    list<PCENCGLink*> MaskedLinkList_mrn;

    PathT_MRN();
    void CalculatePathCost_MRN();
    void DisplayPath_MRN();
};

class PCEN_MRN_CG: public PCEN_KSP
{
protected:
    vector<PCENCGNode*> CGnodes;//added by qian
    vector<PCENCGLink*> CGlinks;//added by qian
    vector<PathT_MRN*> KSP_MRN;//added by qian

public:
    PCEN_MRN_CG(in_addr src, in_addr dest, u_int8_t sw_type, u_int8_t enc_type, float bw, u_int32_t opts, u_int32_t lspq_id, u_int32_t msg_seqnum);
    virtual ~PCEN_MRN_CG();
    void AddCGLink(int linkid, int localNodeId, int remoteNodeId, double metric, int passNode);//added by qian
    void AddLink(int linkid, int localNodeId, int remoteNodeId, double metric, u_char swtype1);//added by qian
    void AddLink(int linkid, int localNodeId, int remoteNodeId, double metric, u_char swtype1,u_char encoding,float bandwidth);//changed 07/14
    void AddLink(int linkid, int localNodeId, int remoteNodeId, double metric, u_char swtype1,u_char encoding1,float bandwidth1,u_char swtype2,u_char encoding2,float bandwidth2, bool adapt);//added by qian
    void AddCGNode (int nodeid,int lclID,int rmtID,u_char swtype,u_char encoding,float bandwidth,double vMetric);    
    bool BuildEndNodes(int source, int end, u_char swtype,u_char encoding,float bandwidth);//changed 07/14
    void DeleteVirtualNodes();//added by qian
    void RestoreCGGraph();//added by qian
    void ResetCGCost();//added by qian
    void ResetCGVisitedFlag();//added by qian
    void ClearCGPath();//added by qian
    void DisplayCGNodes();//added by qian
    void DisplayCGLinks();//added by qian
    void ShowMarkedLinks();//added by qian
    void DijkstraMRN(int source, int destination);//added by qian
    void SearchMRNKSP(int source, int destination, u_char swtype, u_char encoding, float bandwidth, int K);
    PCENCGNode* search_PCENCGNode(int nodeid);//added by qian
    PCENCGNode* search_PCENCGNode(int lclID, int rmtID, u_char swtype);//added by qian
    void MaskParentPath(PathT* ParentPath); // mark the link list of the parent path
    void MaskParentPath_MRN(PathT_MRN* ParentPath);//added by qian
    void CreateChannelGraph(float bandwidth);//changed 07/14    
    void OutputKSP_MRN();
    virtual void Run();
};

#endif

