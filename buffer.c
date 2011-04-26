#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <time.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "arp.h"
#include "buffer.h"
#include "icmp.h"
#include "sr_pwospf.h"


/*******************************************************************
*   Deletes item from buffer.
*******************************************************************/
struct packet_buffer* delete_from_buffer(struct packet_state* ps, struct packet_buffer* want_deleted)
{
	struct packet_buffer* prev=0;
	struct packet_buffer* walker=0;
	walker=ps->sr->queue;
	while(walker)
	{
		if(walker==want_deleted)
		{
			if(prev==0)
			{
			    if(ps->sr->queue->next)
				ps->sr->queue=ps->sr->queue->next;
				else
				{
				    ps->sr->queue=NULL;
				 }   
				break;
			}
			else if(!prev->next->next)
			{
                prev->next=NULL;
                break;
			}
			else
			{
				prev->next=prev->next->next;
				break;
			}
		}
		else
		{
			prev=walker;
			walker=walker->next;
		}
	}
	if(walker->packet)
	    free(walker->packet);
	/*if(walker->interface)
	    free(walker->interface);
	if(walker->arp_req)
	    free(walker->arp_req);*/
	if(walker->old_eth)
	    free(walker->old_eth);
	if(walker)
	    free(walker);
	
	if(prev!=NULL)
        return prev->next;
    else
        return NULL;
}

/*******************************************************************
*   Buffers a packet that is waiting on destination MAC address from ARP.
*******************************************************************/
struct packet_buffer * add_to_pack_buff(struct packet_buffer* buff, uint8_t* pac, uint16_t pack_len, 
                                        struct sr_ethernet_hdr *orig_eth)
{
	struct packet_buffer* buf_walker=0;
	
	if(buff==0) /* If Buffer is Empty.*/
	{
		buff=(struct packet_buffer*)malloc(sizeof(struct packet_buffer));
		assert(buff);
		
		buff->next=0;
		buff->packet=(uint8_t*)malloc(pack_len);
		memmove(buff->packet, pac, pack_len);
		buff->pack_len=pack_len;
		
		buff->old_eth=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
		memmove(buff->old_eth, orig_eth, sizeof(struct sr_ethernet_hdr));
	}
	else /* Buffer is not Empty so Add to End. */
	{
		buf_walker=buff;
		while(buf_walker->next)
		{
			buf_walker=buf_walker->next;
		}
		buf_walker->next=(struct packet_buffer*)malloc(sizeof(struct packet_buffer));
		assert(buf_walker->next);
		buf_walker=buf_walker->next;
		buf_walker->next=0;
		
		buf_walker->packet=(uint8_t*)malloc(pack_len);
		memmove(buf_walker->packet, pac, pack_len);
		buf_walker->pack_len=pack_len;
		buf_walker->old_eth=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
		memmove(buf_walker->old_eth, orig_eth, sizeof(struct sr_ethernet_hdr));
	}
	/*Need to free pack and orig header??*/
	return buff;
}

struct sr_if* get_if_from_mac(struct sr_instance* sr, unsigned char* mac)
{
	int len = ETHER_ADDR_LEN;
	struct sr_if *current = sr->if_list;
	int i;
	int match = 1;
	while(current)
	{
		for(i = 0; i < len; i++)
		{
			if(current->addr[i] != mac[i])
			{
				match = 0;
				break;
			}
		}
		if(match == 1)
		{
			return current;
		}
		else
		{
			current = current->next;
		}
	}
	return NULL;
}


void send_all_packs(struct packet_buffer* buff, uint8_t* mac, char* iface, struct sr_instance* sr)
{
    struct packet_buffer* prev=buff;
    while(buff)
    {
        struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)(buff->packet);
        memmove(eth_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
        sr_send_packet(sr, buff->packet, buff->pack_len, iface);
        prev=buff;
        buff=buff->next;
        free(prev);
    }
    free(buff);
    buff=NULL;
}

void delete_all_pack(struct packet_buffer* buff)
 {
    struct packet_buffer* prev=buff;
    while(buff)
    {
        prev=buff;
        buff=buff->next;
        free(prev->packet);
        free(prev->old_eth);
        free(prev);
    }
    free(buff);
    buff=NULL;
 }
 
 /*TODO: need an ICMP Port Unreachable that doesn't use ps */
 void send_icmp(struct sr_instance *sr, uint8_t* packet, uint16_t old_len, struct sr_ethernet_hdr* old_eth)
 {
    fprintf(stderr, "------Constructing ICMP------\n");
    uint8_t* icmp_pac=(uint8_t*)malloc(sizeof(uint8_t));
    uint16_t icmp_pac_len=0;
    
    uint16_t data_len=old_len-(sizeof(struct sr_ethernet_hdr) +sizeof(struct ip));
    
    packet+=sizeof(struct sr_ethernet_hdr);
    struct ip* old_ip=(struct ip*)(packet);
    
    icmp_pac+=sizeof(struct sr_ethernet_hdr);
    icmp_pac+=sizeof(struct ip);
    
    /*Create ICMP Header*/
    fprintf(stderr,"Create ICMP header.\n");
    struct icmp_hdr* icmp_head=(struct icmp_hdr*)icmp_pac;
    icmp_head->icmp_type= ICMPT_DESTUN;
    fprintf(stderr, "ICMP Type(3): %u ", icmp_head->icmp_type); 
    icmp_head->icmp_code= ICMPC_HOSTUN;
    fprintf(stderr, "ICMP Code(1): %u \n", icmp_head->icmp_code); 
    icmp_head->opt1=0;
    icmp_head->opt2=0;
    
   /*Create ICMP Data*/
   icmp_pac_len+=sizeof(struct icmp_hdr);
    if(data_len >= ICMP_DATA_RES)
    {
        memmove((icmp_pac + sizeof(struct icmp_hdr)), packet, (sizeof(struct ip)+ICMP_DATA_RES));
        icmp_pac_len= icmp_pac_len+ sizeof(struct icmp_hdr) + ICMP_DATA_RES;
    }
    else
    {
        memmove((icmp_pac + sizeof(struct icmp_hdr)), packet, sizeof(struct ip) + data_len);
        icmp_pac_len = icmp_pac_len+ sizeof(struct ip) + data_len;
    }
    
    icmp_head->icmp_sum=0;
    icmp_head->icmp_sum=ntohs(cksum((uint8_t*) icmp_pac, icmp_pac_len));
    
    
    /*Create IP Header*/
    fprintf(stderr,"Create IP header.\n");
    icmp_pac-=sizeof(struct ip);
    struct ip* new_ip=(struct ip*) icmp_pac;
    memmove(new_ip, old_ip, sizeof(struct ip));
    new_ip->ip_len=htons(sizeof(struct ip)+icmp_pac_len);
    new_ip->ip_ttl = INIT_TTL;
	new_ip->ip_p = IPPROTO_ICMP;
	
	/* Find interface to send ICMP out of*/
	struct sr_if* iface = get_if_from_mac(sr, old_eth->ether_dhost);
	
	/*Finish Constructing IP header*/
    new_ip->ip_src.s_addr = iface->ip;
    fprintf(stderr, "IP Source: %s \n", inet_ntoa(new_ip->ip_src)); 
    new_ip->ip_dst = old_ip->ip_src;
    fprintf(stderr, "IP Dest: %s \n", inet_ntoa(new_ip->ip_dst)); 
    new_ip->ip_sum = 0;
    new_ip->ip_sum = cksum((uint8_t *)new_ip, sizeof(struct ip));
    new_ip->ip_sum = htons(new_ip->ip_sum);
    
    /*Construct Ethernet Header*/
    icmp_pac-=sizeof(struct sr_ethernet_hdr);
    struct sr_ethernet_hdr* eth_resp=(struct sr_ethernet_hdr*)icmp_pac;
    memmove(eth_resp->ether_dhost,old_eth->ether_shost,ETHER_ADDR_LEN);
    assert(eth_resp->ether_dhost);
    assert(eth_resp->ether_shost);
    
    fprintf(stderr, "MAC Dest: ");
    DebugMAC(eth_resp->ether_dhost);
    memmove(eth_resp->ether_shost,iface->addr, ETHER_ADDR_LEN);
    fprintf(stderr, "MAC Source: ");
    DebugMAC(eth_resp->ether_shost);
    eth_resp->ether_type=htons(ETHERTYPE_IP);
    
    icmp_pac_len=icmp_pac_len+sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
    sr_send_packet(sr, icmp_pac, icmp_pac_len, iface->name);
    
 }
 
 void send_all_icmps(struct packet_buffer* buff, struct sr_instance* sr)
 {
    fprintf(stderr, "Trying to send ICMPS\n");
    
    struct packet_buffer* prev=buff;
    while(buff)
    {
       fprintf(stderr, "In while loop of send icmps\n");
       
       send_icmp(sr, buff->packet, buff->pack_len, buff->old_eth);
        prev=buff;
        buff=buff->next;
        free(prev->packet);
        free(prev->old_eth);
        free(prev);
    }
    buff=NULL;
 }
