/*** ARP File
*/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
#include "arp.h"
#include "sr_pwospf.h"
#include "arpq.h"


#ifndef ARP_IP_LEN
#define ARP_IP_LEN 4
#endif

#ifndef ARP_HRD_ETH
#define ARP_HRD_ETH 0x0001
#endif

#ifndef ARP_PRO_IP
#define ARP_PRO_IP 0x0800
#endif

#ifndef BROADCAST_ETH
#define BROADCAST_ETH 0xFF
#endif

/*******************************************************************
*   Called when handle_packet() receives and ARP packet.
*   
*   If received a request, calls got_Request() and returns NULL. 
*   If received a reply, calls got_Reply() and returns the ARP cache entry constructed from the 
*   reply received.
*
********************************************************************/
uint8_t* handle_ARP(struct packet_state * ps, struct sr_ethernet_hdr* eth)
{
	struct sr_arphdr *arp =0;

	if(ps->len <sizeof(struct sr_arphdr))
	{
		printf("Malformed ARP Packet.");
		ps->res_len=0;
	}
	else
	{
		arp=(struct sr_arphdr *)(ps->packet);
		switch (ntohs(arp->ar_op))
		{
			case (ARP_REQUEST):
			{
	  			got_Request(ps, arp, eth);
	  			return NULL;
	  		}
	  			break;
			case (ARP_REPLY):
			{
	  			return got_Reply(ps, arp); 
	  		}
	  			break;
			default:
			{
	  			fprintf(stderr, "ARP: Not Request nor Reply\n");
	  			fprintf(stderr, "%hu", arp->ar_op);
	  		}
	  			return NULL;
		}
	}
	return NULL;
}

/*******************************************************************
*   Finds interface the ARP Request was received from and constructs ARP Reply to send back out of 
*   the received interface. 
*******************************************************************/
void got_Request(struct packet_state * ps, struct sr_arphdr * arp_hdr, const struct sr_ethernet_hdr* eth)
{
	assert(ps);
	assert(arp_hdr);
	assert(eth);
	
	struct sr_if *iface = sr_get_interface(ps->sr, ps->interface);
	assert(iface);
	construct_reply(ps, arp_hdr, iface->addr, eth);
}

/*******************************************************************
*   Adds information from received ARP Reply to ARP Cache and returns newly added ARP Cache entry.
*******************************************************************/
uint8_t *got_Reply(struct packet_state * ps, struct sr_arphdr * arp)
{
	add_cache_entry(ps, arp->ar_sip, arp->ar_sha); /*Add IP and MAC address from reply to cache */
	return search_cache(ps->sr, arp->ar_sip); /*Return the newly added entry. */	
}

/*******************************************************************
*   Adds entry to ARP Cache if not already in Cache. Also deletes any entries that are past 
*   their expiration time.
*******************************************************************/
void add_cache_entry(struct packet_state* ps,const uint32_t ip, const unsigned char* mac)
{
    unsigned char* mac_from_cache=search_cache(ps->sr, ip);
	if(mac_from_cache==NULL) /*Entry is not already in cache so add. */
	{
        lock_cache(ps->sr->arp_sub);
        struct arp_cache_entry* cache_walker=0;
        struct arp_cache_entry* prev = 0;
    
        assert(ps);
        assert(mac);
        assert(ip);
    
        if(ps->sr->arp_cache ==0)	/*If there are no entries in cache */
        {
            ps->sr->arp_cache=(struct arp_cache_entry*)malloc(sizeof(struct arp_cache_entry));
            assert(ps->sr->arp_cache);
            ps->sr->arp_cache->next=0;
            ps->sr->arp_cache->ip_add=ip;
            memmove(ps->sr->arp_cache->mac, mac,ETHER_ADDR_LEN);
            ps->sr->arp_cache->timenotvalid=time(NULL) +ARP_TIMEOUT;
        }
        else
        {
            cache_walker = ps->sr->arp_cache;
            
            while(cache_walker->next)
            {
                if(cache_walker->timenotvalid < time(NULL))
                {
                    cache_walker = delete_entry(ps->sr,cache_walker, prev);
                }
                else
                {
               		prev = cache_walker;
               		cache_walker=cache_walker->next;
               	}	
            }
            cache_walker->next=(struct arp_cache_entry*)malloc(sizeof(struct arp_cache_entry));
            assert(cache_walker->next);
            cache_walker=cache_walker->next;
            cache_walker->ip_add=ip;
            memmove(cache_walker->mac, mac,ETHER_ADDR_LEN);
            cache_walker->timenotvalid=time(NULL) +ARP_TIMEOUT;
            cache_walker->next=0;
        }
        unlock_cache(ps->sr->arp_sub);
	}
    else
        free(mac_from_cache);
}

/*******************************************************************
*   Searches cache for entry based on IP address. Deletes any entries past expiration time. Returns 
*   matching entry.
*******************************************************************/
uint8_t* search_cache(struct sr_instance* sr,const uint32_t ip)
{
     //fprintf(stderr, "trying to lock in search_cache\n");
    lock_cache(sr->arp_sub);
    // fprintf(stderr, "locked in search_cache\n");
    
    unsigned char* mac=(unsigned char *)malloc(ETHER_ADDR_LEN);
    
	struct arp_cache_entry* cache_walker=0;
	struct arp_cache_entry* prev=0;
	cache_walker=sr->arp_cache;
	while(cache_walker) 
	{
	    time_t curr_time=time(NULL);
	    assert(cache_walker);
		if(cache_walker->timenotvalid > curr_time)  /*Check if entry has expired. */
		{
			if(ip==cache_walker->ip_add)
			{
		
				memmove(mac, cache_walker->mac, ETHER_ADDR_LEN);
				unlock_cache(sr->arp_sub);
				return mac;
			}
			else
			{
			    prev=cache_walker;
			    cache_walker = cache_walker->next;
			}
		}
		else                                        /*If the ARP entry has expired, delete. */
		{
			cache_walker = delete_entry(sr, cache_walker, prev);
		}
	}
	
	/*IP Address is not in cache. */
	unlock_cache(sr->arp_sub);
	// fprintf(stderr, "unlocked in search cache\n");
	if(mac)
	    free(mac);
	return NULL;
}

/*******************************************************************
*   Deletes entry from cache.
*******************************************************************/
struct arp_cache_entry* delete_entry(struct sr_instance* sr, struct arp_cache_entry* walker, struct arp_cache_entry* prev)
{
    if(prev==0)         /* Item is first in cache. */  
    {
        if(sr->arp_cache->next)
        {
            sr->arp_cache=sr->arp_cache->next;
            free(walker);
            return sr->arp_cache;
        }	
        else
        {
            sr->arp_cache = NULL;
            free(walker);
            return NULL;
        }
    }
    else if(!walker->next) /* Item is last in cache. */
    {
        prev->next=NULL;
        free(walker);
        return NULL;
    }
    else                    /* Item is in the middle of cache. */
    {
        prev->next=walker->next;
        free(walker);
        return prev->next;
    }
	
	/* Walker is still on item to be deleted so free that item. */
	/*if(walker)
		free(walker);
		*/
	/*Return next item in cache after deleted item. */
	/*if(prev!=NULL)
		return prev->next;
	return NULL;*/
	
}

/*******************************************************************
*   Prints all of ARP Cache.
*******************************************************************/
void print_cache(struct sr_instance* sr)
{
	printf("---ARP CACHE---\n");
	struct arp_cache_entry* cache_walker=0;
	if(sr->arp_cache==0)
	{
		printf(" ARP Cache is Empty.\n");
		return;
	}
	cache_walker=sr->arp_cache;
	while(cache_walker)
	{
		print_cache_entry(cache_walker);
		cache_walker=cache_walker->next;
	}
}

/*******************************************************************
*   Prints single ARP Cache Entry.
*******************************************************************/
void print_cache_entry(struct arp_cache_entry * ent)
{
	struct in_addr ip_addr;
	assert(ent);
	ip_addr.s_addr = ent->ip_add;
	printf("IP: %s MAC: ", inet_ntoa(ip_addr));
	DebugMAC(ent->mac); 
	printf(" Time when Invalid: %lu\n",(long)ent->timenotvalid);
}

/*******************************************************************
*   Constructs Reply to an ARP Request.
*******************************************************************/
void construct_reply(struct packet_state* ps, const struct sr_arphdr* arp_hdr, 
                        const unsigned char* mac, const struct sr_ethernet_hdr* eth)
{
    /* Construct ARP Reply Header*/
	struct sr_arphdr *reply;
	reply = (struct sr_arphdr*)malloc(sizeof(struct sr_arphdr));
	reply->ar_hrd = arp_hdr->ar_hrd;
	reply->ar_pro = arp_hdr->ar_pro;
	reply->ar_hln= ETHER_ADDR_LEN;
	reply->ar_pln = arp_hdr->ar_pln;
	reply->ar_op = htons(ARP_REPLY);
	memmove(reply->ar_sha, mac,ETHER_ADDR_LEN);
	reply->ar_sip=arp_hdr->ar_tip;
	memmove(reply->ar_tha, arp_hdr->ar_sha,ETHER_ADDR_LEN);
	reply->ar_tip=arp_hdr->ar_sip;
	
	/*ARP Constructed, Now Add Ethernet Header */
	struct sr_ethernet_hdr* new_eth;
	new_eth=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
	memmove(new_eth->ether_dhost, eth->ether_shost,ETHER_ADDR_LEN); /*Moving original ether_shost to new ether_dhost);*/
	memmove(new_eth->ether_shost, mac,ETHER_ADDR_LEN); /* Set new ether_shost to mac */
	new_eth->ether_type=htons(ETHERTYPE_ARP);
	
	int eth_offset=sizeof(struct sr_ethernet_hdr);
	memmove(ps->response, reply, sizeof(struct sr_arphdr)); /*Put ARP Header in Response */
	ps->response-=eth_offset;
	memmove(ps->response, new_eth, eth_offset); /*Put Ethernet Header in Response */
	
	/* Free arp header and eth header we constructed. */
	if(reply)
		free(reply);
	if(new_eth)
		free(new_eth);

	ps->res_len=eth_offset + sizeof(struct sr_arphdr);
}

/*******************************************************************
*   Constructs appropriate ARP Request based on a packet to be forwarded.
*******************************************************************/
uint8_t* construct_request(struct sr_instance* sr, const char* interface,const uint32_t ip_addr)
{
    uint8_t* request=(uint8_t*)malloc(sizeof(struct sr_ethernet_hdr)+sizeof(struct sr_arphdr));
    //uint16_t pack_len= sizeof(struct sr_ethernet_hdr)+sizeof(struct sr_arphdr);
    
    struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)(request);
    /*Set Ethernet dest MAC address to ff:ff:ff:ff:ff:ff (Broadcast) */
	int i;
	for(i=0; i<ETHER_ADDR_LEN; i++)
	{
		eth_hdr->ether_dhost[i]=0xff;
	}
	
	eth_hdr->ether_type=htons(ETHERTYPE_ARP);
	struct sr_if* iface=sr_get_interface(sr, interface);
	memmove(eth_hdr->ether_shost, iface->addr, ETHER_ADDR_LEN);
	
	struct sr_arphdr* arp_hdr=(struct sr_arphdr*)(request+sizeof(struct sr_ethernet_hdr));
	arp_hdr->ar_hrd=htons(ARP_HRD_ETH);
	arp_hdr->ar_pro=htons(ARP_PRO_IP);
	arp_hdr->ar_hln=ETHER_ADDR_LEN;
	arp_hdr->ar_pln=ARP_IP_LEN;
	arp_hdr->ar_op=htons(ARP_REQUEST);
	memmove(arp_hdr->ar_sha, iface->addr, ETHER_ADDR_LEN);
	arp_hdr->ar_sip=iface->ip;
	for(i=0; i<ETHER_ADDR_LEN; i++)
	{
		arp_hdr->ar_tha[i]=0x00;
	}
	arp_hdr->ar_tip=ip_addr;
	/*int happyfuntime = sizeof(struct sr_ethernet_hdr) + sizeof(struct sr_arphdr);
	for(i = 0; i < happyfuntime; i++)
	{
		
		if(i > 0 && ((i % 2) == 0))
		{
			fprintf(stderr, " ");
		}
		fprintf(stderr, "%x", request[i]);
		if(i > 0 && ((i % 8) == 0))
		{
			fprintf(stderr, "\n");
		}
	}*/
	
	//fprintf(stderr, "\n\n");
	
	/*fprintf(stderr,"Ether dhost: ");
	for(i=0; i< ETHER_ADDR_LEN; i++)
	{
	    fprintf(stderr, "%x ", eth_hdr->ether_dhost[i]);
	}
	fprintf(stderr,"\n");*/
	
	return request;
	
}


