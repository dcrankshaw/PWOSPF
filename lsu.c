/* LSU implementation */

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

int handle_lsu(struct ospfv2_hdr* pwospf, struct packet_state* ps, struct ip* ip_hdr)
{
    
    if(pwospf->rid==ps->sr->ospf_subsys->this_router->rid)
    {
        /* Drop */
        return 0;
    }
    
    
    ps->packet=ps->packet + sizeof(struct ospfv2_hdr); 
    struct ospfv2_lsu_hdr* lsu_head = (struct ospfv2_lsu_hdr*)(ps->packet);
    
    uint16_t old_seq= get_sequence(pwospf->rid, ps->sr);
    if(old_seq==lsu_head->seq)
    {
        /* Drop */
        return 0;
    }
    uint32_t source_rid=pwospf->rid;
    struct route* advertisements=(struct route*)malloc(lsu_head->num_adv); //Can't rem if this is how to do this
    
    ps->packet=(ps->packet)+sizeof(struct ospfv2_lsu_hdr);
    for(int i=0; i<lsu_head->num_adv; i++)
    {
        
        struct ospfv2_lsu_adv* temp_adv=(struct ospfv2_lsu_adv*)(ps->packet);
        struct route temp_rt;
        temp_rt.prefix.s_addr=temp_adv->subnet;
        temp_rt.mask.s_addr=temp_adv->mask;
        temp_rt.r_id=temp_adv->rid;
        
        advertisements[i]=temp_rt;
        ps->packet+=sizeof(struct ospfv2_lsu_adv);
    }
    
    /* Adds advertisements to topology if necessary and recomputes FT if necessary.*/
    if(add_to_top(ps->sr, source_rid, &advertisements, lsu_head->num_adv)==1)
    {
        /*Need to send an LSU now*/
    }
  
    set_sequence(pwospf->rid, lsu_head->seq, ps->sr);
    
    /*Need packet to point to beginning of ospf header*/
    int i;
    for(i=0; i< lsu_head->num_adv; i++)
    {
        ps->packet-=sizeof(struct ospfv2_lsu_adv);
    }
    ps->packet-=sizeof(struct ospfv2_lsu_hdr);
    ps->packet-=sizeof(struct ospfv2_hdr);
    
    forward_lsu(ps, ps->sr, ps->packet, pwospf, ip_hdr);
    
    return 1;
  
}

void forward_lsu(struct packet_state* ps,struct sr_instance* sr, uint8_t* packet, struct ospfv2_hdr* pwospf_hdr, struct ip* ip_hdr)
{
    ip_hdr->ip_ttl--;
    if(ip_hdr->ip_ttl!=0)
    {
        struct in_addr prev_src=ip_hdr->ip_src;
        struct pwospf_iflist* iface_walker=sr->ospf_subsys->interfaces;
        while(iface_walker)
        {
            ip_hdr->ip_src=iface_walker->address;
            struct neighbor_list* neigh_walker=iface_walker->neighbors;
            while (neigh_walker)
            {
                if(neigh_walker->ip_address.s_addr==prev_src.s_addr)
                    break;  /*Don't forward packet back to who we received lsu from */
                ip_hdr->ip_dst=neigh_walker->ip_address;
                ip_hdr->ip_sum=0;
                ip_hdr->ip_sum=cksum((uint8_t*)ip_hdr, sizeof(struct ip));
                ip_hdr->ip_sum=htons(ip_hdr->ip_sum);
                
                packet-=sizeof(struct ip);
                memmove(packet, ip_hdr, sizeof(struct ip));
                packet-=sizeof(struct sr_ethernet_hdr);
                
                /*Generate Ethernet Header*/
                struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
                eth_hdr->ether_type=ETHERTYPE_IP;
                
                /*Find Interface to be sent out of's MAC Address*/
                struct sr_if* src_if=sr_get_interface(sr, iface_walker->name);
                memmove(eth_hdr->ether_shost, src_if->addr, ETHER_ADDR_LEN);
                
                /*Find MAC Address of source*/
                uint8_t *mac=search_cache(ps->sr, ip_hdr->ip_dst.s_addr);
                if(mac!=NULL)
                {
                    memmove(eth_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
                    memmove(packet, eth_hdr, sizeof(struct sr_ethernet_hdr));
                    uint16_t packet_size=ip_hdr->ip_len + sizeof(struct sr_ethernet_hdr);
                    sr_send_packet(sr, packet, packet_size, iface_walker->name);
                    /*Packet has been sent*/
                }
                else
                {
                    /*Need to buffer and then send ARP*/
                }
                
            }
        
        }
    }
}


void send_lsu(struct sr_instance* sr)
{
    struct router* my_router=sr->ospf_subsys->this_router;
    
    uint8_t* pack=(uint8_t*)malloc(sizeof(struct sr_ethernet_hdr)+
            sizeof(struct ip)+sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr)
            + ((my_router->subnet_size)*(sizeof(struct ospfv2_lsu_hdr))));
    pack=pack+sizeof(struct sr_ethernet_hdr)+sizeof(struct ip)+
            sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr);
    
    /*Generate Advertisements List*/
    struct ospfv2_lsu_adv* advertisements=(struct ospfv2_lsu_adv*)malloc((my_router->subnet_size)*(sizeof(struct ospfv2_lsu_adv)));
    struct ospfv2_lsu_adv* advs=generate_adv(advertisements, sr);
    struct ospfv2_lsu_adv* adv_walker=advs;
    int j;
    for(j=0; j<my_router->subnet_size; j++)
    {
        memmove(pack, &adv_walker[j], sizeof(struct ospfv2_lsu_adv));
        pack+=sizeof(struct ospfv2_lsu_adv);
    } 
    
    /*Advertisements are now in pack. pack points to end of last advertisement. 
    * Move pointer to where LSU header should begin. */
    pack-=(my_router->subnet_size)*(sizeof(struct ospfv2_lsu_adv));
    pack-=(sizeof(struct ospfv2_lsu_hdr));
    
    /*Generate LSU Header */
    struct ospfv2_lsu_hdr* lsu_hdr=(struct ospfv2_lsu_hdr*)malloc(sizeof(struct ospfv2_lsu_hdr));
    lsu_hdr->seq=sr->ospf_subsys->last_seq_sent;
    lsu_hdr->ttl=INIT_TTL;
    lsu_hdr->num_adv=my_router->subnet_size;
    
    memmove(pack,lsu_hdr, sizeof(struct ospfv2_lsu_hdr));
    pack-=(sizeof(struct ospfv2_hdr));
    
    /*Generate PWOSPF Header*/
    struct ospfv2_hdr* pwospf_hdr=(struct ospfv2_hdr*) malloc(sizeof(struct ospfv2_hdr));
    pwospf_hdr->version=OSPF_V2;
    pwospf_hdr->type=OSPF_TYPE_LSU;
    /*For length do I include ip and ethernet hdrs too???? */
    pwospf_hdr->len=sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr)+ (my_router->subnet_size)*(sizeof(struct ospfv2_lsu_adv));
    pwospf_hdr->rid=sr->ospf_subsys->this_router->rid;
    pwospf_hdr->aid= sr->ospf_subsys->area_id;
    pwospf_hdr->autype=OSPF_DEFAULT_AUTHKEY;
    pwospf_hdr->audata=OSPF_DEFAULT_AUTHKEY;
    pwospf_hdr->csum=0;
    pwospf_hdr->csum=cksum((uint8_t*)(pwospf_hdr), sizeof(struct ospfv2_hdr));
    pwospf_hdr->csum=htons(pwospf_hdr->csum);
    
    memmove(pack, pwospf_hdr, sizeof(struct ospfv2_hdr));
    pack-=(sizeof(struct ip));
    
    /*Generate IP Header */
    struct ip* ip_hdr=(struct ip*)malloc(sizeof(struct ip));
    ip_hdr->ip_hl=(sizeof(struct ip))/4;
    ip_hdr->ip_v=IP_VERSION;
    ip_hdr->ip_tos=ROUTINE_SERVICE;
    ip_hdr->ip_len=sizeof(struct ip) + pwospf_hdr->len;
    ip_hdr->ip_id=0;
    ip_hdr->ip_off=0;
    ip_hdr->ip_ttl= INIT_TTL;
    ip_hdr->ip_p=OSPFV2_TYPE;
    
    uint16_t pack_len= sizeof(struct sr_ethernet_hdr) + ip_hdr->ip_len;
  
    struct pwospf_iflist* iface_walker=sr->ospf_subsys->interfaces;
    while(iface_walker)
    {
        ip_hdr->ip_src=iface_walker->address;
        struct neighbor_list* neigh_walker=iface_walker->neighbors;
        while (neigh_walker)
        {
            /*Finish constructing IP Header */
            ip_hdr->ip_dst=neigh_walker->ip_address;
            ip_hdr->ip_sum=0;
            ip_hdr->ip_sum=cksum((uint8_t*)ip_hdr, sizeof(struct ip));
            ip_hdr->ip_sum=htons(ip_hdr->ip_sum);
            
            memmove(pack, ip_hdr, sizeof(struct ip));
            pack-=sizeof(struct sr_ethernet_hdr);
            
            /*Generate Ethernet Header*/
            struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
            eth_hdr->ether_type=ETHERTYPE_IP;
            
            /*Find Interface to be sent out of's MAC Address*/
            struct sr_if* src_if=sr_get_interface(sr, iface_walker->name);
            memmove(eth_hdr->ether_shost, src_if->addr, ETHER_ADDR_LEN);
            
            fprintf(stderr, "Find dest Mac Address\n");
            /*Find MAC Address of dest*/
           uint8_t* mac=search_cache(sr, ip_hdr->ip_dst.s_addr);
            if(mac!=NULL)
            {
                memmove(eth_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
                memmove(pack, eth_hdr, sizeof(struct sr_ethernet_hdr));
                sr_send_packet(sr, pack, pack_len , iface_walker->name);
                /*Packet has been sent*/
            }
            else
            {
                get_mac_address(sr, ip_hdr->ip_dst, pack, pack_len, iface_walker->name, 1, NULL);
                /**** NEED TO FREE ****/
            }
            neigh_walker=neigh_walker->next;
        }
        iface_walker=iface_walker->next;
    } 
}

struct ospfv2_lsu_adv* generate_adv(struct ospfv2_lsu_adv* advs, struct sr_instance* sr)
{
    fprintf(stderr, "Generate Advs\n");
    struct router * my_router = sr->ospf_subsys->this_router;
    if(my_router==NULL);
        return NULL;
        
    int i;
    for(i=0; i<my_router->subnet_size; i++)
    {
        struct route* temp_rt=my_router->subnets[i];
        struct ospfv2_lsu_adv new_adv;
        new_adv.subnet=temp_rt->prefix.s_addr;
        new_adv.mask=temp_rt->mask.s_addr;
        new_adv.rid=temp_rt->r_id;
        advs[i]=new_adv;
    }
    
    return advs;   
} 