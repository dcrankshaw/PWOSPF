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
#include "sr_pwospf.h"
#include "pwospf_protocol.h"
#include "top_info.h"
#include "hello.h"

/*
**************** GOES IN sr_init() IN sr_router.c *********************

pwospf_iflist* interface_list;
INTERFACE LIST NEEDS TO BE INITIALIZED WITH THE THREE INTERFACES:
Including their IP address, netmask, and MAC address

*/

/*******************************************************************
*   Called when handle_packet() receives a HELLO packet.
********************************************************************/
void handle_HELLO(struct packet_state* ps, struct sr_ethernet_hdr* eth_hdr, struct ip* ip_hdr)
{
	struct ospfv2_hdr* pwospf_hdr = 0;
	struct ospfv2_hello_hdr* hello_hdr = 0;
	struct pwospf_iflist* iface = ps->sr->ospf_subsys->interfaces;

	if(ps->len < (sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr))) /* incorrectly sized packet */
	{
		printf("Malformed HELLO Packet.");
		ps->res_len=0;
	}
	else /* correctly sized packet */
	{
		pwospf_hdr = (struct ospfv2_hdr*)(ps->packet);
		hello_hdr = (struct ospfv2_hello_hdr*)(ps->packet + sizeof(struct ospfv2_hdr));

		/* check incoming packet values against ONLY the receiving interface in the interface list */
		if(iface == 0)
		{
			printf("ERROR - INTERFACE LIST NOT INITIALIZED");
		}
		while(iface)
		{
			if(iface->name == ps->interface) /* if the current interface equals the incoming interface */
			{
				if((iface->mask.s_addr != hello_hdr->nmask) || (iface->helloint != hello_hdr->helloint))
				{
					/* drop packet */
					printf("HELLO doesn't match any interface - packet dropped");
					return;
				}

				/* once interface is decided: */
				struct neighbor_list* neighbor_list_walker = 0;
		
				if(iface->neighbors == 0) /* no neighbors known - add new neighbor */
				{
					iface->neighbors = (struct neighbor_list*) malloc(sizeof(struct neighbor_list));
					assert(iface->neighbors);
					iface->neighbors->next = 0;
					iface->neighbors->id = pwospf_hdr->rid;
					iface->neighbors->ip_address = ip_hdr->ip_src;
					iface->neighbors->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
				}
				else /* add to end of iface->neighbors (end of neighbor_list_walker) */
				{
					neighbor_list_walker = iface->neighbors;
					while(neighbor_list_walker != NULL)
					{
						if(neighbor_list_walker->ip_address.s_addr == ip_hdr->ip_src.s_addr) /* ?????????? SHOULD THIS BE COMPARING THE rid, NOT THE ip_address? */
						{
							neighbor_list_walker->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
							return;
						}
						if(neighbor_list_walker->timenotvalid < time(NULL))
						{
							neighbor_list_walker = delete_neighbor_list(iface, neighbor_list_walker);
						}
						
						neighbor_list_walker = neighbor_list_walker->next;
					}
					/* no matching neighbor found - add new neighbor */
					neighbor_list_walker->next = (struct neighbor_list*) malloc(sizeof(struct neighbor_list));
					assert(neighbor_list_walker->next);
					neighbor_list_walker = neighbor_list_walker->next;
					neighbor_list_walker->next = 0;
					neighbor_list_walker->id = pwospf_hdr->rid;
					neighbor_list_walker->ip_address = ip_hdr->ip_src;
					neighbor_list_walker->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
				}
			}
			iface = iface->next;
		}
	}
	return;
}

/*******************************************************************
*   Deletes a neigbor_list_entry from neighbors.
*******************************************************************/
struct neighbor_list* delete_neighbor_list(struct pwospf_iflist* iface, struct neighbor_list* want_deleted)
{
	struct neighbor_list* prev = 0;
	struct neighbor_list* walker = 0;
	walker = iface->neighbors;
	
	while(walker)
	{
		if(walker == want_deleted)    /* On item to be deleted in list. */
		{
			if(prev == 0)          /* Item is first in list. */  
			{
				if(iface->neighbors->next)
				{
					iface->neighbors = iface->neighbors->next;
				}	
				else
				{
					iface->neighbors = NULL;
				}
				break;
			}
			else if(!walker->next) /* Item is last in list. */
			{
				prev->next = NULL;
				break;
			}
			else                    /* Item is in the middle of list. */
			{
				prev->next = walker->next;
				break;
			}
		}
		else
		{
			prev = walker;
			walker = walker->next;
		}
	}
	
	/* Walker is still on item to be deleted so free that item. */
	if(walker)
		free(walker);
		
	/*Return next item in list after deleted item. */
	if(prev != NULL)
		return prev->next;
	return NULL;
}

/*******************************************************************
*   Prints all of the Neighbor Lists for all Interfaces.
*******************************************************************/
void print_all_neighbor_lists(struct packet_state* ps)
{
	struct pwospf_iflist* interface_list_walker = 0;

	printf("--- INTERFACE LIST ---\n");
	if(ps->sr->ospf_subsys->interfaces == 0)
	{
		printf("INTERFACE LIST IS EMPTY\n");
		return;
	}
	interface_list_walker = ps->sr->ospf_subsys->interfaces;
	while(interface_list_walker)
	{
		printf("Interface IP: %i", interface_list_walker->address.s_addr); /* OLD: inet_ntoa(interface_list_walker->ip_add) */
		printf("--- NEIGHBOR LIST ---\n");
		struct neighbor_list* neighbor_list_walker = 0;
		if(interface_list_walker->neighbors == 0)
		{
			printf("NEIGHBOR LIST IS EMPTY\n");
			return;
		}
		neighbor_list_walker = interface_list_walker->neighbors; /* WHY DOES THIS GENERATE A WARNING?? */
		while(neighbor_list_walker)
		{
			print_neighbor_list(neighbor_list_walker);
			neighbor_list_walker = neighbor_list_walker->next;
		}
		interface_list_walker = interface_list_walker->next;
	}
}

/*******************************************************************
*   Prints a single Neighbor List Entry.
*******************************************************************/
void print_neighbor_list(struct neighbor_list* ent)
{
	struct in_addr ip_addr;
	assert(ent);
	ip_addr = ent->ip_address;
	printf("IP: %s\t", inet_ntoa(ip_addr));
	printf("Time when Invalid: %lu\n",(long)ent->timenotvalid);
}

/*******************************************************************
*   Creates and sends a HELLO packet with Ethernet, IP, OSPF, and OSPF_HELLO headers.
*******************************************************************/
void send_HELLO(struct packet_state* ps)
{
	unsigned int packet_size = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);
	uint8_t* outgoing_packet_buffer = malloc(packet_size);
	struct sr_ethernet_hdr* eth_hdr = (struct sr_ethernet_hdr*) outgoing_packet_buffer;
	struct ip* ip_hdr = (struct ip*) outgoing_packet_buffer + sizeof(struct sr_ethernet_hdr);
	struct ospfv2_hdr* pwospf_hdr = (struct ospfv2_hdr*) outgoing_packet_buffer + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip);
	struct ospfv2_hello_hdr* hello_hdr = (struct ospfv2_hello_hdr*) outgoing_packet_buffer + sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr);
	
	/* Set Ethernet destination MAC address to ff:ff:ff:ff:ff:ff (Broadcast) */
	int i = 0;
	for(i=0; i<ETHER_ADDR_LEN; i++)
	{
		eth_hdr->ether_dhost[i] = 0xff;
	}
	for(i=0; i<ETHER_ADDR_LEN; i++)
	{
		eth_hdr->ether_shost[i] = 0x00; /* ??????????????? */
	}
	eth_hdr->ether_type=htons(ETHERTYPE_IP);

	/* Set IP destination IP address to 224.0.0.5 (0xe0000005) (Broadcast) */
	ip_hdr->ip_hl = sizeof(struct ip)/4; /* CHECK THIS ?????????? */
	ip_hdr->ip_v = IP_VERSION;
	ip_hdr->ip_tos = 0; /* ??????????????????????? */
	ip_hdr->ip_len = packet_size - sizeof(struct sr_ethernet_hdr);
	ip_hdr->ip_id = 0; /* ?????????????????????? */
	ip_hdr->ip_off = 0; /* ?????????????????????*/
	ip_hdr->ip_ttl = INIT_TTL;
	ip_hdr->ip_p = IPPROTO_TCP; /* ???????????? */
	ip_hdr->ip_sum = 0;
	ip_hdr->ip_sum = cksum((uint8_t *)ip_hdr, sizeof(struct ip));
	ip_hdr->ip_sum = htons(ip_hdr->ip_sum);
	ip_hdr->ip_src.s_addr = 0x00000000; /* ~~~~~~ interface specific: set in while loop below ~~~~~~~~ */
	ip_hdr->ip_dst.s_addr = OSPF_AllSPFRouters; /* CHECK THIS ????????????? */

	/* Set up PWOSPF header. */
	pwospf_hdr->version = OSPF_V2;
	pwospf_hdr->type = OSPF_TYPE_HELLO;
	pwospf_hdr->len = sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);
	pwospf_hdr->rid = 0; /* ????????????? */
	pwospf_hdr->aid = 0; /* ????????????? */
	pwospf_hdr->csum = 0; /* ????????????? */
	pwospf_hdr->csum = cksum((uint8_t *)pwospf_hdr, sizeof(struct ospfv2_hdr)-64); /* Minus 64 to exclude the authentication field. */
	pwospf_hdr->csum = htons(pwospf_hdr->csum);
	pwospf_hdr->autype = OSPF_DEFAULT_AUTHKEY;
	pwospf_hdr->audata = OSPF_DEFAULT_AUTHKEY;

	/* Set up HELLO header. */
	hello_hdr->nmask = 0; /* ~~~~~~ inteface specific: set in while loop below ~~~~~~~ */
	hello_hdr->helloint = OSPF_DEFAULT_HELLOINT;
	hello_hdr->padding = 0; /* ?????????? */

	/* Send the packet out on each interface. */
	struct pwospf_iflist* iface = ps->sr->ospf_subsys->interfaces;
	assert(iface);
	while(iface)
	{
		ip_hdr->ip_src = iface->address;
		hello_hdr->nmask = iface->mask.s_addr;
		sr_send_packet(ps->sr, outgoing_packet_buffer, packet_size, iface->name);
		iface = iface->next;
	}

	if(outgoing_packet_buffer)
		free(outgoing_packet_buffer);
}
