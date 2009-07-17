/*
 * Copyright (c) 2009
 * KDDI Path Computation Project.
 * Created by  Xi Yang (USC/ISI), 2009
 */

#include "pcen_mcbase.hh"

vector<PathM*> PCEN_MCBase::allPaths;


///////////////////  PathM /////////////////////////

PathM& PathM::operator=(const PathM& p) {
    this->source.s_addr = p.source.s_addr;
    this->destination.s_addr = p.destination.s_addr;
    this->ucid = p.ucid;
    this->seqnum = p.seqnum;
    this->path.assign(p.path.begin(), p.path.end());
    this->cost = p.cost;
    this->bandwidth = p.bandwidth;
    this->vlan_tag = p.vlan_tag;
    this->wavelength = p.wavelength;
    this->pflg = p.pflg;
    return *this;
}

PathM& PathM::operator=(const PathT& p)
{
        this->source.s_addr = p.source.s_addr;
        this->destination.s_addr = p.destination.s_addr;
        this->ucid = p.ucid;
        this->seqnum = p.seqnum;
        this->cost = p.cost;
        this->bandwidth = p.bandwidth;
        this->vlan_tag = p.vlan_tag;
        this->wavelength = p.wavelength;
        this->pflg.flag = p.pflg.flag;
        this->path.clear();
        list<PCENLink*>::const_iterator it = p.path.begin();
        for (; it != p.path.end(); it++)
            this->path.push_back((*it)->link);
        return *this;
}

//////////////////  PCEN_MCBase  ///////////////////

bool PCEN_MCBase::PostBuildTopology()
{
    bool ret = PCEN_KSP::PostBuildTopology();

    //cleanup masked-off deltas
    for (int l = 0; l < links.size(); l++)
        if (links[l]->link)
            links[l]->link->cleanupMaskoffDeltas();

    return ret;
}

#define ABS(X) ((X >= 0.0) ? (X) : -(X))

int PCEN_MCBase::PickMCPCandidates(int M)
{
    //get simple hop length of requested path. C_0 = HopLenXBW.
    PCENNode* srcNode = GetNodeByIp(routers,&source);
    PCENNode* destNode = GetNodeByIp(routers,&destination);
    assert (srcNode && destNode);
    Dijkstra(srcNode->ref_num, destNode->ref_num);
    if (destNode->path.size() == 0)
        return -1;

    thePath.path.clear();
    list<PCENLink*>::iterator it1;
    for (it1 = destNode->path.begin(); it1 != destNode->path.end(); it1++)
        thePath.path.push_back((*it1)->link);
    thePath.cost = thePath.path.size()*thePath.bandwidth;
    
    //go through allPaths to find up to (M-1) Paths that are closest to C_0 by sorting C_x (HopLenxBW) 
    MCPaths.clear();
    if (allPaths.size() > 0)
        MCPaths.push_back(allPaths.front());
    for (int i = 0; i < allPaths.size(); i++)
    {
      //assign C_x (HopLenxBW)
       allPaths[i]->cost = allPaths[i]->path.size()*allPaths[i]->bandwidth;
        for (int j = 0; j < MCPaths.size(); j++)
        {
            if (ABS(allPaths[i]->cost - thePath.cost) < ABS(MCPaths[j]->cost - thePath.cost))
            {
                MCPaths.insert(MCPaths.begin()+j, allPaths[i]);
                break;
            }
        }
    }

    if (MCPaths.size() > M-1)
        MCPaths.erase(MCPaths.begin()+M-1, MCPaths.end());

    return MCPaths.size();
}

int PCEN_MCBase::PerformComputation()
{
    //initiating computation
    memset(&thePath, 0, sizeof(PathT));
    thePath.source.s_addr = this->source.s_addr;
    thePath.destination.s_addr = this->destination.s_addr;
    thePath.bandwidth = this->bandwidth_ingress;
    thePath.ucid = this->ucid;
    thePath.seqnum = this->seqnum;
    thePath.path.clear();
    MCPaths.push_back(&thePath);
    MC_KSP1.clear(); MC_KSP1.reserve(MCPaths.size());
    MC_KSP2.clear(); MC_KSP2.reserve(MCPaths.size());
    sortedMCPaths.clear(); sortedMCPaths.reserve(MCPaths.size());
    PCENNode* srcNode = GetNodeByIp(routers,&source);
    PCENNode* destNode = GetNodeByIp(routers,&destination);

    //run KSP for a single path if (allPaths.size() == 0), then return
    if (allPaths.size() == 0 || MCPaths.size() == 1)
    {
        SearchKSP(srcNode->ref_num, destNode->ref_num, SystemConfig::pce_k);
        if (KSP.size() == 0)
            return ERR_PCEN_NO_ROUTE;
        PathT* bestPath = ConstrainKSPaths(KSP);
        if (bestPath == NULL)
            return ERR_PCEN_NO_ROUTE;
        thePath = *bestPath;
        return ERR_PCEN_NO_ERROR;
    }

    //maskoff deltas for every path in MCPaths --> release MCPaths resources
    int i;
    for (i = 0; i < MCPaths.size()-1; i++)
    {
        narb_lsp_request_tlv lsp_req;
        lsp_req.type = ((MSG_LSP << 8) | ACT_MASKOFF);
        lsp_req.length = sizeof(narb_lsp_request_tlv) - TLV_HDR_SIZE;
        lsp_req.src.s_addr = MCPaths[i]->source.s_addr;
        lsp_req.dest.s_addr = MCPaths[i]->destination.s_addr;
        lsp_req.bandwidth = MCPaths[i]->bandwidth;
        lsp_req.switching_type = this->switching_type_ingress;
        lsp_req.encoding_type = this->encoding_type_ingress;
        lsp_req.gpid = 0;
        list<ero_subobj> ero;
        GetPathERO(MCPaths[i]->path, ero);
        bool is_bidir = ((this->options & LSP_OPT_BIDIRECTIONAL) != 0);
        if (ero.size() > 0)
            LSPHandler::UpdateLinkStatesByERO(lsp_req, ero, MCPaths[i]->ucid, MCPaths[i]->seqnum, is_bidir);
    }

    //M-Concurrent path computation algorithm for MCP
    //1. Run KSP for each path in MCPaths, including MCPaths.push_back(&thePath)
    //2. Pick (up to 2xM) best and second-to-best paths and place them into MC_KSP1 and MC_KSP2
    for (i = 0; i < MCPaths.size(); i++)
    {
        PCENNode* srcNode1 = GetNodeByIp(routers, &MCPaths[i]->source);
        PCENNode* destNode1 = GetNodeByIp(routers, &MCPaths[i]->destination);
        assert(srcNode1 && destNode1);
        SearchKSP(srcNode1->ref_num, destNode1->ref_num, SystemConfig::pce_k);
        MC_KSP1[i] = *MCPaths[i];
        MC_KSP2[i] = *MCPaths[i];
        if (GetBestTwoKSPaths(KSP, MC_KSP1[i], MC_KSP2[i]) == 0) //mark no_path
            return ERR_PCEN_NO_ROUTE;
    }

    //3. Sort MCPaths according to 'The Criteria' --> create sorted 'newPaths' list (identified by ucid+seqnum)
    vector<PathM*> sortedPaths;
    sortedPaths.assign(MCPaths.begin(), MCPaths.end());
    SortMPaths(sortedPaths);

    //4. Run KSP for paths in newPaths
    for (i = 0; i < sortedPaths.size(); i++)
    {
        sortedMCPaths[i] = *sortedPaths[i];

        PCENNode* srcNode2 = GetNodeByIp(routers, &MCPaths[i]->source);
        PCENNode* destNode2 = GetNodeByIp(routers, &MCPaths[i]->destination);
        SearchKSP(srcNode2->ref_num, destNode2->ref_num, SystemConfig::pce_k);
        PathT* bestPath = ConstrainKSPaths(KSP);
        if (!bestPath)
        {
            //if any MCPaths[j] fails --> maskon all paths in MCPaths except for 'thePath'
            for (int j = 0; j < MCPaths.size() - 1; j++)
            {
                narb_lsp_request_tlv lsp_req;
                lsp_req.type = ((MSG_LSP << 8) | ACT_MASKON);
                lsp_req.length = sizeof(narb_lsp_request_tlv) - TLV_HDR_SIZE;
                lsp_req.src.s_addr = MCPaths[j]->source.s_addr;
                lsp_req.dest.s_addr = MCPaths[j]->destination.s_addr;
                lsp_req.bandwidth = MCPaths[j]->bandwidth;
                lsp_req.switching_type = this->switching_type_ingress;
                lsp_req.encoding_type = this->encoding_type_ingress;
                lsp_req.gpid = 0;
                list<ero_subobj> ero;
                GetPathERO(MCPaths[j]->path, ero);
                bool is_bidir = ((this->options & LSP_OPT_BIDIRECTIONAL) != 0);
                if (ero.size() > 0)
                    LSPHandler::UpdateLinkStatesByERO(lsp_req, ero, MCPaths[j]->ucid, MCPaths[j]->seqnum, is_bidir);
            }
            thePath.path.clear();
            return ERR_PCEN_NO_ROUTE;
        }

        bestPath->source.s_addr = sortedMCPaths[i].source.s_addr;
        bestPath->destination.s_addr = sortedMCPaths[i].destination.s_addr;
        bestPath->ucid = sortedMCPaths[i].ucid;
        bestPath->seqnum = sortedMCPaths[i].seqnum;
        bestPath->cost = sortedMCPaths[i].cost;
        bestPath->bandwidth = sortedMCPaths[i].bandwidth;
        bestPath->vlan_tag = sortedMCPaths[i].vlan_tag;
        bestPath->wavelength = sortedMCPaths[i].wavelength;
        bestPath->pflg.flag = sortedMCPaths[i].pflg.flag;
        sortedMCPaths[i] = *bestPath;
    }

    //otherwise, assign paths of newPaths to MCPaths[i] (including 'thePath'), return success
    for (i = 0; i < MCPaths.size(); i++)
    {
        for (int j = 0; j < sortedMCPaths.size(); j++)
        {
            if (MCPaths[i]->ucid == sortedMCPaths[j].ucid && MCPaths[i]->seqnum == sortedMCPaths[j].seqnum)
                MCPaths[i]->path.assign(sortedMCPaths[j].path.begin(), sortedMCPaths[j].path.end());
        }
    }

    return ERR_PCEN_NO_ERROR;
}

void PCEN_MCBase::Run()
{
    int ret;
    if ((ret = VerifyRequest()) != 0)
    {
        LOGF("PCEN_MCBase::VerifyRequest() failed for source[%X]-destination[%X]\n", source.s_addr, destination.s_addr);
        ReplyErrorCode(ret);
        return;
    }

    if ((ret = BuildTopology()) != 0)
    {
        LOGF("PCEN_MCBase::BuildTopology() failed for source[%X]-destination[%X]\n", source.s_addr, destination.s_addr);
        ReplyErrorCode(ret);
        return;
    }

    if (!PostBuildTopology()) //pruning links
    {
        LOGF("PCEN_MCBase::PostBuildTopology() failed for source[%X]-destination[%X]\n", source.s_addr, destination.s_addr);
        ReplyErrorCode(ERR_PCEN_NO_ROUTE);
        return;
    }

    //pick MCPaths
    if (PickMCPCandidates(SystemConfig::pce_m) < 0)
    {
        LOGF("PCEN_MCBase::PickMCPCandidates() failed to find a simple shortest path for source[%X]-destination[%X]\n", source.s_addr, destination.s_addr);
        ReplyErrorCode(ERR_PCEN_NO_ROUTE);
        return;
    }
    
    //compute Path
    if ((ret = PerformComputation()) != 0 || thePath.path.size() ==0)
    {
        LOGF("PCEN_MCBase::PerformComputation() failed to find a path for source[%X]-destination[%X]\n", source.s_addr, destination.s_addr);
        ReplyErrorCode(ERR_PCEN_NO_ROUTE);
        return;
    }

    //record the newly held path to allPaths (set ucid+seqnum)
    if (options & LSP_OPT_QUERY_HOLD)
    {
        PathM *newPath = new PathM;
        *newPath = thePath;
        allPaths.push_back(newPath);
    }

    //getPathERO
    if (has_wdm_layer)
        wavelength = thePath.wavelength;
    GetPathERO(thePath.path, this->ero);
    
    //replyERO (w/ holding logic)
    ReplyERO();
}

int PCEN_MCBase::GetBestTwoKSPaths(vector<PathT*>& KSP, PathM &path1, PathM &path2)
{
    double minCost = PCEN_INFINITE_COST;
    PathT* bestPath = NULL;
    PathT* secondPath = NULL;
    PathT* P;
    vector<PathT*>::iterator iterP;

    iterP = KSP.begin();
    while (iterP != KSP.end())
    {
        P = (*iterP);
        if (VerifyPathConstraints(P->path, P->vlan_tag, P->wavelength))
        {
            if (P->cost < minCost)
            {
                minCost = P->cost;
                if (bestPath)
                    secondPath = bestPath;
                bestPath = P;
            }
            iterP++;
        }
        else
        {
            delete (*iterP); //$$ release PathT memory
            iterP = KSP.erase(iterP);
        }
    }

    int ret = 0;
    if (bestPath)
    {
        path1 = *bestPath;
        ret++;
    }
    else
    {
        path1.pflg.pfg.filteroff = 1;
    }

    if (secondPath)
    {
        path2 = *secondPath;
        ret++;
    }
    else
    {
        path2.pflg.pfg.filteroff = 1;
    }

    return ret;
}

inline void SwapPaths(PathM* &path1, PathM* &path2)
{
    PathM* p;
    p= path1;
    path1 = path2;
    path2 = p;
}

inline double SumOfBandwidthWeightedCommonLinks(PathM* &P, vector<PathM>& Paths)
{
    if (P->path.size() == 0)
        return 0;
    int i, numPaths = Paths.size();

    for (i = 0; i < numPaths; i++)
    {
        if (P->ucid == Paths[i].ucid && P->seqnum == Paths[i].seqnum)
            break;
    }
    assert (i < numPaths);

    PathM* path1 = &Paths[i];

    if (path1->pflg.pfg.filteroff != 0)
        return PCEN_INFINITE_COST;

    double sum = 0;
    list<Link*>::iterator iter1, iter2;
    for (iter1 = path1->path.begin(); iter1 != path1->path.end(); iter1++)
    {
        for (i = 0; i < numPaths; i++)
        {
            if (path1->ucid == Paths[i].ucid && path1->seqnum == Paths[i].seqnum)
                continue;
            for (iter2 = Paths[i].path.begin(); iter2 != Paths[i].path.end(); iter2++)
            {
                if ((*iter1) == (*iter2))
                {
                    sum += (path1->bandwidth < Paths[i].bandwidth ? path1->bandwidth : Paths[i].bandwidth);
                }
            }
        }
    }
    return sum;
}

// swap if (path1 < path2) according to porivsioning priority criteria
void PCEN_MCBase::SortTwoPaths(PathM* &path1, PathM* &path2)
{

    // criterion 1: bandwidth-weighted hop length (created-time?)
    if (path1->bandwidth*path1->path.size() < path2->bandwidth*path2->path.size())
        SwapPaths(path1, path2);
    // criterion 2: sum of bandwidth-weighted common links for best-path
    else if (SumOfBandwidthWeightedCommonLinks(path1, MC_KSP1) < SumOfBandwidthWeightedCommonLinks(path2, MC_KSP1))
        SwapPaths(path1, path2);
    // criterion 3: sum of bandwidth-weighted common links for second-path --> no_path first ...
    else if (SumOfBandwidthWeightedCommonLinks(path1, MC_KSP2) < SumOfBandwidthWeightedCommonLinks(path2, MC_KSP2))
        SwapPaths(path1, path2);
}

// decreasing order according to porivsioning priority criteria
void PCEN_MCBase::SortMPaths(vector<PathM*>& Paths)
{
    int numPaths = Paths.size();

    for (int i = 0; i < numPaths; i++)
        for (int j = 0; j < numPaths; j++)
        {
            if (j > i)
                SortTwoPaths(Paths[i], Paths[j]);
        }
}

int PCEN_MCBase::GetPathERO(list<Link*>& path, list<ero_subobj>& ero)
{
    ero.clear();
    PCENLink* pcen_link;
    list<Link*>::iterator it;
    for (it = path.begin(); it != path.end(); it++)
    {
        pcen_link = GetPCENLinkByLink(*it);
        if (pcen_link == NULL)
            continue;
        AddLinkToEROTrack(ero, pcen_link);
    }
}

PCENLink* PCEN_MCBase::GetPCENLinkByLink(Link* link)
{
    for (int i = 0; i < links.size(); i++)
    {
        if (links[i]->link == link)
            return links[i];
    }
    return NULL;
}

