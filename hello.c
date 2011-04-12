/************************
*
*  HELLO Packet Functions
*
************************/

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sr_if.h"
#include "sr_rt.h"
#include "sr_router.h"
#include "sr_protocol.h"
/* #include "pwospf_protocol.h"   NEEDS TO BE ADDED ONCE THE FILE COMPILES */
#include "hello.h"

/*
**************** GOES IN sr_init() IN sr_router.c *********************

interface_list_entry* interface_list;
/* INTERFACE LIST NEEDS TO BE INITIALIZED WITH THE THREE INTERFACES */


*/

/*******************************************************************
*
*   Called when handle_packet() receives a HELLO packet.
*
********************************************************************/
void handle_HELLO(struct packet_state* ps, struct sr_ethernet_hdr* eth)
{
	struct ospfv2_hdr* pwospf_hdr = 0;
	struct ospfv2_hello_hdr* hello_hdr = 0;
	struct interface_list_entry* iface = 0;

	if(ps->len < (sizeof(struct ospfv2_hdr) + sizeof(ospfv2_hello_hdr)) /* incorrectly sized packet */
	{
		printf("Malformed HELLO Packet.");
		ps->res_len=0;
	}
	else /* correctly sized packet */
	{
		pwospf_hdr = (struct ospfv2_hdr*)(ps->packet);
		hello_hdr = (struct ospfv2_hello_hdr*)(ps->packet + sizeof(ospfv2_hdr));  /* HOW TO START THIS AT MEMORY LOCATION OF THE SECOND HALF OF THE HEADER???? */
		
		/* check incoming packet values against ONLY the receiving interface in the interface list */
		if(iface == 0)
		{
			printf("ERROR - INTERFACE LIST NOT INITIALIZED");
		}
		while(iface)
		{
			if(iface->interface == ps->interface) /* if the current interface equals the incoming interface */
			{
				/* WHERE IS THE INTERFACE'S helloint VARIABLE USED???? */
				if((iface->nmask != hello_hdr->nmask) || (iface->helloint != hello_hdr->helloint))
				{
					/* drop packet */
					printf("HELLO doesn't match any interface - packet dropped");
					return;
				}

				/* once interface is decided: */
				struct neighbor_list_entry* list_walker = 0;
		
				interface_list_walker = iface->nghbrs;
				if(interface_list_walker == 0)
				{
					/* no neighbors known - add new neighbor */
				}
				else
				{
					while(interface_list_walker != NULL)
					{
						/* TODO -  packet matches entry, update the 'last hello packet received timer' */
						
						interface_list_walker = interface_list_walker->next;
					}
					/* no matching neigbor found - add new neighbor */
				}
			}
			iface = iface->next;
		}
	}
	return NULL;
}


/*******************************************************************
*   Adds entry to ARP Cache if not already in Cache. Also deletes any entries that are past 
*   their expiration time.
*******************************************************************/
void add_cache_entry(struct packet_state* ps,const uint32_t ip, const unsigned char* mac)
{
	if(search_cache(ps, ip)==NULL) /*Entry is not already in cache so add. */
	{
        struct arp_cache_entry* cache_walker=0;
    
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
                    cache_walker = delete_entry(ps,cache_walker);
                }
                else
                {
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
	}

}

/*******************************************************************
*   Searches cache for entry based on IP address. Deletes any entries past expiration time. Returns 
*   matching entry.
*******************************************************************/
struct arp_cache_entry* search_cache(struct packet_state* ps,const uint32_t ip)
{
	struct arp_cache_entry* cache_walker=0;
	cache_walker=ps->sr->arp_cache;
	while(cache_walker) 
	{
	    time_t curr_time=time(NULL);
		if(cache_walker->timenotvalid > curr_time)  /*Check if entry has expired. */
		{
			if(ip==cache_walker->ip_add)
				return cache_walker;
			else
				cache_walker = cache_walker->next;
		}
		else                                        /*If the ARP entry has expired, delete. */
		{
			cache_walker = delete_entry(ps, cache_walker);
		}
	}
	
	/*IP Address is not in cache. */
	return NULL;
}

/*******************************************************************
*   Deletes entry from cache.
*******************************************************************/
struct arp_cache_entry* delete_entry(struct packet_state* ps, struct arp_cache_entry* want_deleted)
{
	struct arp_cache_entry* prev=0;
	struct arp_cache_entry* walker=0;
	walker=ps->sr->arp_cache;
	
	while(walker)
	{
		if(walker==want_deleted)    /* On item to be deleted in cache. */
		{
			if(prev==0)             /* Item is first in cache. */  
			{
				if(ps->sr->arp_cache->next)
				{
					ps->sr->arp_cache=ps->sr->arp_cache->next;
				}	
				else
				{
					ps->sr->arp_cache = NULL;
				}
				break;
			}
			else if(!walker->next) /* Item is last in cache. */
			{
                prev->next=NULL;
                break;
			}
			else                    /* Item is in the middle of cache. */
			{
				prev->next=walker->next;
				break;
			}
		}
		else
		{
			prev=walker;
			walker=walker->next;
		}
	}
	
	/* Walker is still on item to be deleted so free that item. */
	if(walker)
		free(walker);
		
	/*Return next item in cache after deleted item. */
	if(prev!=NULL)
		return prev->next;
	return NULL;
	
}

/*******************************************************************
*   Prints all of the Neighbor Lists for all Interfaces.
*******************************************************************/
void print_all_neighbor_lists(struct sr_instance* sr)
{
	struct itnerface_list_entry* interface_list_walker = 0;

	printf("--- INTERFACE LIST ---\n");
	if(sr->interface_list == 0)
	{
		printf("INTERFACE LIST IS EMPTY\n");
		return;
	}
	interface_list_walker = sr->interface_list;
	while(interface_list_walker)
	{
		printf("Interface IP: %s", inet_ntoa(interface_list_walker->ip_add));
		printf("--- NEIGHBOR LIST ---\n");
		struct neighbor_list_entry* neighbor_list_walker = 0;
		if(interface_list_walker->nghbrs == 0)
		{
			printf("NEIGHBOR LIST IS EMPTY\n");
			return;
		}
		neigbor_list_walker = interface_list_walker->nghbrs;
		while(neighbor_list_walker)
		{
			print_neigbor_list_entry(cache_walker);
			neighbor_list_walker = neigbor_list_walker->next;
		}
		interface_list_walker = interface_list_walker->next;
	}
}

/*******************************************************************
*   Prints a single Neighbor List Entry.
*******************************************************************/
void print_neigbor_list_entry(struct neigbor_list_entry* ent)
{
	struct in_addr ip_addr;
	assert(ent);
	ip_addr.s_addr = ent->ip_add;
	printf("IP: %s\t", inet_ntoa(ip_addr));
	printf("Time when Invalid: %lu\n",(long)ent->timenotvalid);
}

/*******************************************************************
*   Constructs Reply to an ARP Request.
*******************************************************************/
void construct_reply(struct packet_state* ps, const struct sr_arphdr* arp_hdr, const unsigned char* mac, const struct sr_ethernet_hdr* eth)
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
