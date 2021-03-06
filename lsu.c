/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/29/2011
 **********************************************************************/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "sr_pwospf.h"
#include "pwospf_protocol.h"
#include "top_info.h"
#include "lsu.h"
#include "arp.h"
#include "arpq.h"
#include "icmp.h"

/*******************************************************************
*  Handles a LSU packet. Edits topology based on advertisements in LSU packet. Calls function
*   that forwards LSU as necessary.
*******************************************************************/
int handle_lsu(struct ospfv2_hdr* pwospf, struct packet_state* ps, struct ip* ip_hdr)
{
   pwospf_lock(ps->sr->ospf_subsys);
   if(pwospf->rid==ps->sr->ospf_subsys->this_router->rid)
    {
        /* Drop */
        pwospf_unlock(ps->sr->ospf_subsys);
        fprintf(stderr, "DROPPED -- Our own LSU\n");
        return 0;
    }
    pwospf_unlock(ps->sr->ospf_subsys);
    
    ps->packet=ps->packet + sizeof(struct ospfv2_hdr); 
    struct ospfv2_lsu_hdr* lsu_head = (struct ospfv2_lsu_hdr*)(ps->packet);
    
    uint16_t old_seq= get_sequence(pwospf->rid, ps->sr); /*Locked & Unlocked in Function */
    if(old_seq==lsu_head->seq)
    {
        fprintf(stderr, "DROPPED -- Seq Number same as last Seq Number received from this router.\n");
        
        return 0;
    }
    uint32_t source_rid=pwospf->rid;
    uint32_t num_ads_rcd =ntohl(lsu_head->num_adv);
    struct route** advertisements=(struct route**)malloc(num_ads_rcd*sizeof(struct route*)); 
    
    ps->packet=(ps->packet)+sizeof(struct ospfv2_lsu_hdr);
    struct route* temp_rt;
    struct ospfv2_lsu_adv* temp_adv;
    int i;
    for(i=0; i<num_ads_rcd; i++)
    {
        
        temp_adv=(struct ospfv2_lsu_adv*)(ps->packet);
        temp_rt = (struct route*) malloc(sizeof(struct route));
        temp_rt->prefix.s_addr=temp_adv->subnet;
        temp_rt->mask.s_addr=temp_adv->mask;
        temp_rt->r_id=temp_adv->rid;
        struct in_addr new_rid;
        new_rid.s_addr=temp_rt->r_id;
        advertisements[i]=temp_rt;
        ps->packet+=sizeof(struct ospfv2_lsu_adv);
    }
    
    /* Adds advertisements to topology if necessary and recomputes FT if necessary.*/
    if(add_to_top(ps->sr, source_rid, advertisements, num_ads_rcd)==1)
    {
        /*TODO TODO TODO: Need to send an LSU now!!!*/
    }
    
    /*free advertisements*/
    for(i = 0; i<num_ads_rcd; i++)
    {
        free(advertisements[i]);
    }
    free(advertisements);
    
    set_sequence(pwospf->rid, lsu_head->seq, ps->sr);   /*Locked & Unlocked in Function */
    
    /*Need packet to point to beginning of ospf header*/
    for(i=0; i< num_ads_rcd; i++)
    {
        ps->packet-=sizeof(struct ospfv2_lsu_adv);
    }
    ps->packet-=sizeof(struct ospfv2_lsu_hdr);
    ps->packet-=sizeof(struct ospfv2_hdr);
    ps->packet-=sizeof(struct ip);
    ps->packet-=sizeof(struct sr_ethernet_hdr);
    forward_lsu(ps, ps->sr, ps->packet, ip_hdr); /*ps->packet points to beginning of packet*/
    
    return 1;
  
}

/*******************************************************************
*  Checks if LSU should be forwarded. Forwards LSU to all neighbors beside neighbor who sent LSU
*   to this router.
*******************************************************************/
void forward_lsu(struct packet_state* ps,struct sr_instance* sr, uint8_t* packet, struct ip* ip_hdr)
{
    if(ip_hdr->ip_ttl < 1)
    {
        /*packet expired*/
        icmp_response(ps, ip_hdr, ICMPT_TIMEEX, ICMPC_INTRANSIT);
    }
    ip_hdr->ip_ttl--;
    struct ospfv2_hdr* ospf_hdr = (struct ospfv2_hdr*) (packet+sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
    struct ospfv2_lsu_hdr* lsu_hdr = (struct ospfv2_lsu_hdr*) (packet+sizeof(struct sr_ethernet_hdr) + sizeof(struct ip)+sizeof(struct ospfv2_hdr));
    lsu_hdr->ttl--;
    ospf_hdr->csum = 0;
    uint16_t ospf_len = ntohs(ospf_hdr->len);
    ospf_hdr->csum = htons(cksum((uint8_t*) ospf_hdr, sizeof(struct ospfv2_hdr)-8));
    if(lsu_hdr->ttl>0)
    {
        if(ip_hdr->ip_ttl>0)
        {
            struct in_addr prev_src=ip_hdr->ip_src;
            pwospf_lock(sr->ospf_subsys);
            
            struct sr_ethernet_hdr* new_eth=(struct sr_ethernet_hdr*)(packet);
            struct ip* new_ip=(struct ip*)(packet+sizeof(struct sr_ethernet_hdr));
            
            struct pwospf_iflist* iface_walker=sr->ospf_subsys->interfaces;
            while(iface_walker)
            {
                new_ip->ip_src=iface_walker->address;
        
                int i;
                for(i = 0; i < iface_walker->nbr_size; i++)
                {
                    if(!(iface_walker->neighbors[i]->ip_address.s_addr ==prev_src.s_addr))
                    {
                    
                        new_ip->ip_dst=iface_walker->neighbors[i]->ip_address;
                        new_ip->ip_sum=0;
                        new_ip->ip_sum=cksum((uint8_t*)ip_hdr, sizeof(struct ip));
                        new_ip->ip_sum=htons(ip_hdr->ip_sum);
                        
                        
                        /*Generate Ethernet Header*/
                        new_eth->ether_type=htons(ETHERTYPE_IP);
                        
                        /*Find Interface to be sent out of's MAC Address*/
                        memmove(new_eth->ether_shost, iface_walker->mac, ETHER_ADDR_LEN);
                        
                        /*Find MAC Address of source*/
                        uint8_t * mac=search_cache(ps->sr, ip_hdr->ip_dst.s_addr);
                        uint16_t packet_size=sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + ospf_len;

                        if(mac!=NULL)
                        {
                            memmove(new_eth->ether_dhost, mac, ETHER_ADDR_LEN);
                            sr_send_packet(sr, packet, packet_size, iface_walker->name);
                            /*Packet has been sent*/
                        }
                        else
                        {
                            get_mac_address(sr, ip_hdr->ip_dst, packet, packet_size, iface_walker->name, 1, NULL);
                        }    
                     }
                }
                
                iface_walker=iface_walker->next;
            }
            pwospf_unlock(sr->ospf_subsys);
         }
    }
}

/*******************************************************************
*  Constructs and sends and LSU
*******************************************************************/
void send_lsu(struct sr_instance* sr)
{
	pwospf_lock(sr->ospf_subsys);
    struct router* my_router=sr->ospf_subsys->this_router;
    
    struct sr_rt* rt_table_walker=sr->routing_table;
    int num_entries_rt=0;
    while(rt_table_walker)
    {
        num_entries_rt++;
        rt_table_walker=rt_table_walker->next;
    }
    
    uint8_t* pack=(uint8_t*)malloc(sizeof(struct sr_ethernet_hdr)+
                    sizeof(struct ip)+sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr)
                    + ((my_router->subnet_size + num_entries_rt)*(sizeof(struct ospfv2_lsu_adv))));
    pack=pack+sizeof(struct sr_ethernet_hdr)+sizeof(struct ip)+
            sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr);
    
    /*Generate Advertisements List*/
    struct ospfv2_lsu_adv* advertisements=(struct ospfv2_lsu_adv*)malloc
                    ((my_router->subnet_size + num_entries_rt)*(sizeof(struct ospfv2_lsu_adv)));
    assert(advertisements);
    struct ospfv2_lsu_adv* advs=generate_adv(advertisements, sr, num_entries_rt);
    struct ospfv2_lsu_adv* adv_walker=advs;
    int j;
    for(j=0; j<(my_router->subnet_size + num_entries_rt); j++)
    {
        memmove(pack, &adv_walker[j], sizeof(struct ospfv2_lsu_adv));
        pack+=sizeof(struct ospfv2_lsu_adv);
    } 
    
    /*Advertisements are now in pack. pack points to end of last advertisement. 
    * Move pointer to where LSU header should begin. */
    pack-=(my_router->subnet_size + num_entries_rt)*(sizeof(struct ospfv2_lsu_adv));
    pack-=(sizeof(struct ospfv2_lsu_hdr));
    
    /*Generate LSU Header */
    struct ospfv2_lsu_hdr* lsu_hdr=(struct ospfv2_lsu_hdr*)pack;
    sr->ospf_subsys->last_seq_sent++;
    lsu_hdr->seq=htons(sr->ospf_subsys->last_seq_sent);
    lsu_hdr->ttl=INIT_TTL;
    lsu_hdr->num_adv=htonl(my_router->subnet_size + num_entries_rt);
    pack-=(sizeof(struct ospfv2_hdr));
    
    /*Generate PWOSPF Header*/
    struct ospfv2_hdr* pwospf_hdr=(struct ospfv2_hdr*) pack;
    pwospf_hdr->version=OSPF_V2;
    pwospf_hdr->type=OSPF_TYPE_LSU;
    pwospf_hdr->len=htons(sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr) + 
                        (my_router->subnet_size + num_entries_rt)*(sizeof(struct ospfv2_lsu_adv)));

    pwospf_hdr->rid=sr->ospf_subsys->this_router->rid;
    pwospf_hdr->aid= htonl(sr->ospf_subsys->area_id);
    pwospf_hdr->autype=OSPF_DEFAULT_AUTHKEY;
    pwospf_hdr->audata=OSPF_DEFAULT_AUTHKEY;
    pwospf_hdr->csum=0;
    pwospf_hdr->csum=cksum((uint8_t*)(pwospf_hdr), sizeof(struct ospfv2_hdr)-8);
    pwospf_hdr->csum=htons(pwospf_hdr->csum);
    pack-=(sizeof(struct ip));
    
    /*Generate IP Header */
    struct ip* ip_hdr=(struct ip*)pack;
    ip_hdr->ip_hl=(sizeof(struct ip))/4;
    ip_hdr->ip_v=IP_VERSION;
    ip_hdr->ip_tos=ROUTINE_SERVICE;
    ip_hdr->ip_len=htons(sizeof(struct ip) + ntohs(pwospf_hdr->len));
    ip_hdr->ip_id=0;
    ip_hdr->ip_off=0;
    ip_hdr->ip_ttl= INIT_TTL;
    ip_hdr->ip_p=OSPFV2_TYPE;
    
    uint16_t pack_len= sizeof(struct sr_ethernet_hdr)+
                    sizeof(struct ip)+sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr)
                    + ((my_router->subnet_size + num_entries_rt)*(sizeof(struct ospfv2_lsu_adv)));
    
    struct pwospf_iflist* iface_walker=sr->ospf_subsys->interfaces;
    pack-=sizeof(struct sr_ethernet_hdr);
    while(iface_walker)
    {
        ip_hdr->ip_src=iface_walker->address;
       int i;
       for(i = 0; i < iface_walker->nbr_size; i++)
       {
       		ip_hdr->ip_dst=iface_walker->neighbors[i]->ip_address;
            ip_hdr->ip_sum=0;
            ip_hdr->ip_sum=cksum((uint8_t*)ip_hdr, sizeof(struct ip));
            ip_hdr->ip_sum=htons(ip_hdr->ip_sum);
            
            /*Generate Ethernet Header*/
            struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)pack;
            eth_hdr->ether_type=htons(ETHERTYPE_IP);
            
            /*Find Interface to be sent out of's MAC Address*/
            struct sr_if* src_if=sr_get_interface(sr, iface_walker->name);
            memmove(eth_hdr->ether_shost, src_if->addr, ETHER_ADDR_LEN);

            /*Find MAC Address of dest*/
            uint8_t* mac=search_cache(sr, ip_hdr->ip_dst.s_addr);
            if(mac!=NULL)
            {
                memmove(eth_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
                sr_send_packet(sr, pack, pack_len , iface_walker->name);
            }
            else
            {
                get_mac_address(sr, ip_hdr->ip_dst, pack, pack_len, iface_walker->name, 1, NULL);
            }
		}
		iface_walker=iface_walker->next;
    } 
    
    pwospf_unlock(sr->ospf_subsys);
    
    free(pack);
    free(advs);    
}


/*******************************************************************
*  Generates the advertisements to go in a LSU
*******************************************************************/
struct ospfv2_lsu_adv* generate_adv(struct ospfv2_lsu_adv* advs, struct sr_instance* sr, int num_entries_rt)
{
    /***Precondition: pwospf_subsystem already lock*/
    print_nbr_list(sr);
    if(sr->ospf_subsys->this_router==NULL)
        return NULL;
    
    int num_subs= sr->ospf_subsys->this_router->subnet_size;
    
    int i;
    for(i=0; i<num_subs; i++)
    {
        struct route* temp_rt=sr->ospf_subsys->this_router->subnets[i];
        struct ospfv2_lsu_adv new_adv;
        new_adv.subnet=temp_rt->prefix.s_addr;
        new_adv.mask=temp_rt->mask.s_addr;
        struct in_addr rid;
        rid.s_addr=temp_rt->r_id;
        new_adv.rid=temp_rt->r_id;
        rid.s_addr=new_adv.rid;
        advs[i]=new_adv;
    }
    
    struct sr_rt* rt_table_walker=sr->routing_table;

    for(i=num_subs; i<(num_subs + num_entries_rt); i++)
    {
        
        struct sr_rt* temp_rt=rt_table_walker;
        struct ospfv2_lsu_adv new_adv;
        new_adv.subnet=temp_rt->dest.s_addr;
        new_adv.mask=temp_rt->mask.s_addr;
        new_adv.rid=0;
        advs[i]=new_adv;
        rt_table_walker=rt_table_walker->next;
    }
    return advs;   
} 

/*******************************************************************
*  Prints advertisements.
*******************************************************************/
void print_ads(struct ospfv2_lsu_adv* ads, int num_ads)
{
    fprintf(stderr, "----ADVERTISEMENTS----\n");
    int i=0;
    for(i=0; i< num_ads; i++)
    {
        struct in_addr sub;
        sub.s_addr=ads[i].subnet;
        struct in_addr mask;
        mask.s_addr=ads[i].mask;
        struct in_addr rid;
        rid.s_addr=ads[i].rid;
        fprintf(stderr, "Subnet: %s, ", inet_ntoa(sub)); 
        fprintf(stderr, "Mask: %s, ",inet_ntoa(mask));
        fprintf(stderr, "RID: %s \n", inet_ntoa(rid));
    }
    fprintf(stderr, "\n");
}