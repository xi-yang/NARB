/*
 * Copyright (c) 2007
 * DRAGON Project.
 * University of Southern California/Information Sciences Institute.
 * All rights reserved.
 *
 * Created by Xi Yang 2004-2007
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "lsa.hh"
#include "log.hh"
#include "toolbox.hh"

void LSAHandler::Run()
{
    assert(rc);

    //Update into RDB
    RDB.Update(rc);
    rc = NULL;
}

LSAHandler::~LSAHandler()
{
    if (lsa) 
        delete [](char*)lsa;
    if (rc)
        delete rc;
}

void LSAHandler::Load(api_msg* msg)
{
    //For now, we assume that each message contains only one LSA.
    int len = ntohs(msg->header.length);
    lsa = (lsa_header*) new char[len];
    memcpy(lsa, msg->body, len);
    domain_mask = msg->header.msgtag[0];
    api_msg_delete(msg);
}

Resource* LSAHandler::Parse()
{
    if (lsa == NULL)
    	return NULL;

    assert (rc == NULL);

    u_int32_t lsa_id;
    u_char lsa_type;
    struct te_tlv_header *tlvh = NULL;
    struct te_tlv_header *sub_tlvh = NULL;
    u_int32_t read_len;

    if (lsa->type != OSPF_OPAQUE_AREA_LSA &&  lsa->type != OSPF_OPAQUE_LINK_LSA)
    {
        LOG_CERR<<"LSAHandler::Parse()::NOT_TE_LSA"<<endl;
        return NULL;
    }

    lsa_type = GET_OPAQUE_TYPE(ntohl(lsa->id.s_addr));
    lsa_id = GET_OPAQUE_ID(ntohl(lsa->id.s_addr));

    if ((lsa->type == OSPF_OPAQUE_AREA_LSA && lsa_type != OPAQUE_TYPE_TE_AREA_LSA) ||
         (lsa->type == OSPF_OPAQUE_LINK_LSA && (lsa_type != OPAQUE_TYPE_TE_LINKLOCAL_LSA ||
                                                                                        lsa_id != 0)))
    {
        LOG_CERR<<"LSAHandler::Parse()::NOT_TE_LSA"<<endl;
        return NULL;
    }

    tlvh = TLV_HDR_TOP(lsa);

    // check top-level tlv
    if (lsa->type == OSPF_OPAQUE_AREA_LSA && ntohs(tlvh->type) == TE_TLV_ROUTER_ADDR)
    {
    	 rc = new RouterId (domain_mask == DOMAIN_MASK_LOCAL? RTYPE_LOC_RTID : RTYPE_GLO_RTID, 
                    0, ((struct te_tlv_router_addr *)tlvh)->value.s_addr);
    }
    else if (lsa->type == OSPF_OPAQUE_AREA_LSA && ntohs(tlvh->type) == TE_TLV_LINK)
    {
        Link *link = new Link(domain_mask == DOMAIN_MASK_LOCAL ? RTYPE_LOC_PHY_LNK : RTYPE_GLO_ABS_LNK, 0, 0, 0);
        read_len = 0;
        link->advRtId = lsa->adv_router.s_addr;
        sub_tlvh = SUBTLV_HDR_TOP(tlvh);
        ISCD * swcap;
        IACD * adcap;
        Reservation * resv;
        int opaque_length = ntohs(lsa->length) - sizeof(struct lsa_header);

        while (read_len <  opaque_length - TLV_HDR_SIZE)
        {
              int a_index, i;

             #ifdef HAVE_EXT_ATTR
              RcAttrDataType a_type;
              ResourceIndexingElement *pe;
  
                  pe =  GET_ATTR(domain_mask == DOMAIN_MASK_LOCAL ? RTYPE_LOC_PHY_LNK : RTYPE_GLO_ABS_LNK, 
                                                      ntohs(sub_tlvh->type));
                  a_index = ATTR_INDEX(domain_mask == DOMAIN_MASK_LOCAL ? RTYPE_LOC_PHY_LNK : RTYPE_GLO_ABS_LNK, 
                                                      ntohs(sub_tlvh->type));
              #endif

        	switch (ntohs(sub_tlvh->type))
        	{
        		case TE_LINK_SUBTLV_LINK_TYPE:
        			link->linkType = ((struct te_link_subtlv_link_type *)sub_tlvh)->link_type.value;
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->linkType);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_LINK_ID:
        			link->id = ((struct te_link_subtlv_link_id *)sub_tlvh)->value.s_addr;
        		       #ifdef HAVE_EXT_ATTR
                              link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->id);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_LCLIF_IPADDR:
        			link->lclIfAddr = ((struct te_link_subtlv_lclif_ipaddr *)sub_tlvh)->value.s_addr;
            		       #ifdef HAVE_EXT_ATTR
                               link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->lclIfAddr);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_RMTIF_IPADDR:
        			link->rmtIfAddr = ((struct te_link_subtlv_rmtif_ipaddr *)sub_tlvh)->value.s_addr;
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->rmtIfAddr);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_TE_METRIC:
        			link->metric = ntohl(((struct te_link_subtlv_te_metric *)sub_tlvh)->value);
                            if (domain_mask == DOMAIN_MASK_GLOBAL)
                                link->metric += METRIC_INTER_DOMAIN_NICE_INCREMENTAL;
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->metric);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_MAX_BW:
        			link->maxBandwidth = ((struct te_link_subtlv_max_bw *)sub_tlvh)->value;
                            ntohf_mbps(link->maxBandwidth);
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->maxBandwidth);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_MAX_RSV_BW:
        			link->maxReservableBandwidth =((struct te_link_subtlv_max_rsv_bw *)sub_tlvh)->value;
                            ntohf_mbps(link->maxReservableBandwidth);
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->maxReservableBandwidth);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_UNRSV_BW:
        			memcpy(link->unreservedBandwidth, ((struct te_link_subtlv_unrsv_bw *)sub_tlvh)->value, sizeof(float) * 8);
                            for (i = 0; i < 8; i++)
                                ntohf_mbps(link->unreservedBandwidth[i]);
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, link->unreservedBandwidth);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_RSC_CLSCLR:
        			link->rcClass = ntohl(((struct te_link_subtlv_rsc_clsclr *)sub_tlvh)->value);
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->rcClass);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_LINK_LCRMT_ID:
        			link->lclId = ((struct te_link_subtlv_link_lcrmt_id *)sub_tlvh)->link_local_id;
        			link->rmtId = ((struct te_link_subtlv_link_lcrmt_id *)sub_tlvh)->link_remote_id;
        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0,  link->lclRmtId);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_DOMAIN_ID:
        			link->domainId = (domain_mask & ntohl(((struct te_link_subtlv_domain_id *)sub_tlvh)->value));
                            if (domain_mask == DOMAIN_MASK_GLOBAL && link->domainId != 0)
                            {
                                list<RouterId2DomainId*>::iterator iter;
                                RouterId2DomainId* r2d_conv;
                                bool found = false;
                                for (iter = ResourceDB::routerToDomainDirectory.begin(); iter != ResourceDB::routerToDomainDirectory.end(); iter++)
                                {
                                    r2d_conv = *iter;
                                    if (link->advRtId == r2d_conv->router_id)
                                    {
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                {
                                    r2d_conv = new (struct RouterId2DomainId);
                                    r2d_conv->router_id = link->advRtId;
                                    r2d_conv->domain_id = link->domainId;
                                    ResourceDB::routerToDomainDirectory.push_back(r2d_conv);
                                }
                            }  

        		       #ifdef HAVE_EXT_ATTR
                                link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, &link->domainId);
                            #endif
        			break;
        		case TE_LINK_SUBTLV_LINK_IFSWCAP:
                            swcap = new ISCD;
                            memcpy(swcap, (char*)sub_tlvh+TLV_HDR_SIZE, ntohs(sub_tlvh->length));
                            for (i = 0; i < 8; i++)
                                ntohf_mbps(swcap->max_lsp_bw[i]); 
                            link->iscds.push_back(swcap);
        		       #ifdef HAVE_EXT_ATTR
                                assert (a_index > 0);
                                if (link->attrTable.size() < a_index +1)
                                {
                                    link->attrTable.resize(a_index+1);
                                }
                                link->attrTable[a_index].t = pe->dataType;
                                link->attrTable[a_index].l = pe->dataLen;
                                link->attrTable[a_index].p = &link->iscds;
                            #endif
        			break;                           
                    case TE_LINK_SUBTLV_LINK_IFADCAP:
                            adcap = new IACD;
                            memcpy(adcap, (char*)sub_tlvh+TLV_HDR_SIZE, sizeof(IACD));
                            link->iacds.push_back(adcap);
        		       #ifdef HAVE_EXT_ATTR
                                assert (a_index > 0);
                                if (link->attrTable.size() < a_index +1)
                                {
                                    link->attrTable.resize(a_index+1);
                                }
                                link->attrTable[a_index].t = pe->dataType;
                                link->attrTable[a_index].l = pe->dataLen;
                                link->attrTable[a_index].p = &link->iacds;
                            #endif
                            break;
                    case TE_LINK_SUBTLV_RESV_SCHEDULE:
                            resv = (Reservation *)((char*)sub_tlvh+TLV_HDR_SIZE);
                            for (i = 0; i < ntohs(sub_tlvh->length)/RESV_SIZE; i++)
                            {
                                Reservation * resv_allocated = link->resvTable.Allocate(resv->uptime, resv->duration);
                                if (resv_allocated)
                                {
                                    *resv_allocated = *(resv++);
                                }
                                else
                                {
                                    LOGF("Conflicted reservation: [%d, %d]: %f Mbps by LSP (%x/%x)\n", resv->uptime, 
                                        resv->uptime+resv->duration, resv->bandwidth, resv->domain_id, resv->lsp_id);
                                }
                            }
        		       #ifdef HAVE_EXT_ATTR
                                assert (a_index > 0);
                                if (link->attrTable.size() < a_index +1)
                                {
                                    link->attrTable.resize(a_index+1);
                                }
                                link->attrTable[a_index].t = pe->dataType;
                                link->attrTable[a_index].l = pe->dataLen;
                                link->attrTable[a_index].p = &link->resvTable.reserves;
                            #endif
                            break;
                    default:
        		       #ifdef HAVE_EXT_ATTR
                                if (pe == NULL)
                                      LOGF("The sub-tlv type %d is not supported.\n", ntohs(sub_tlvh->type));
                                else
                                {
                                        char * data = new char[ntohs(sub_tlvh->length)];
                                        memcpy(data, (char*)sub_tlvh+TLV_HDR_SIZE, ntohs(sub_tlvh->length));
                                        link->SetAttribute(a_index, pe ? pe->dataType: 0, pe ? pe->dataLen : 0, data);
                                }   
                            #else
                                LOGF("The sub-tlv type %d is not supported.\n", ntohs(sub_tlvh->type));
                            #endif
                            
        			break;
        	}
        	read_len += TLV_SIZE(sub_tlvh);
        	sub_tlvh = SUBTLV_HDR_NEXT(sub_tlvh);
        }

        // Check if mandatory sub-tlvs are included in this link tlv
        if (link->linkType == 0 || link->id == 0)
        {
            LOG_CERR<<"LSAHandler::Parse()::Madatory LINK-SUB_TLV(s) missing" <<endl;
            delete link;
        }
        else
        {
            rc = link;
        }
    }
    else if (lsa->type == OSPF_OPAQUE_LINK_LSA && ntohs(tlvh->type) == TE_TLV_LINK_LOCAL_ID)
    {
        LOG_CERR<<"NARB ignores OPAQUE LINK LOCAL ID LSA"<<endl;
    }
    else
    {
        LOG_CERR<<"LSAHandler::Parse()::Unrecognized TE-LSA due to incorrect TLV header info."<<endl;
    }

    if (rc)
        rc->domainMask = domain_mask;
    return rc;
}


