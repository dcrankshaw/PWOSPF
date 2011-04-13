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
#include "pwospf_protocol.h"
#include "hello.h"

/*
**************** GOES IN sr_init() IN sr_router.c *********************

interface_list_entry* interface_list;
INTERFACE LIST NEEDS TO BE INITIALIZED WITH THE THREE INTERFACES

*/

/*******************************************************************
*   Called when handle_packet() receives a HELLO packet.
********************************************************************/
void handle_HELLO(struct packet_state* ps, struct sr_ethernet_hdr* eth)
{
	struct ospfv2_hdr* pwospf_hdr = 0;
	struct ospfv2_hello_hdr* hello_hdr = 0;
	struct interface_list_entry* iface = ps->sr->interface_list;

	if(ps->len < (sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr))) /* incorrectly sized packet */
	{
		printf("Malformed HELLO Packet.");
		ps->res_len=0;
	}
	else /* correctly sized packet */
	{
		pwospf_hdr = (struct ospfv2_hdr*)(ps->packet);
		hello_hdr = (struct ospfv2_hello_hdr*)(ps->packet + sizeof(struct ospfv2_hdr));  /* HOW TO START THIS AT MEMORY LOCATION OF THE SECOND HALF OF THE HEADER???? */

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
				struct neighbor_list_entry* neighbor_list_walker = 0;
		
				if(iface->nghbrs == 0) /* no neighbors known - add new neighbor */
				{
					iface->nghbrs = (struct neighbor_list_entry*) malloc(sizeof(struct neighbor_list_entry));
					assert(iface->nghbrs);
					iface->nghbrs->next = 0;
					iface->nghbrs->ip_add = pwospf_hdr->rid;
					iface->nghbrs->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
				}
				else /* add to end of iface->nghbrs (end of neighbor_list_walker) */
				{
					neighbor_list_walker = iface->nghbrs;
					while(neighbor_list_walker != NULL)
					{
						if(neighbor_list_walker->ip_add == pwospf_hdr->rid)
						{
							neighbor_list_walker->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
							return;
						}
						if(neighbor_list_walker->timenotvalid < time(NULL))
						{
							neighbor_list_walker = delete_neighbor_list_entry(iface, neighbor_list_walker);
						}
						
						neighbor_list_walker = neighbor_list_walker->next;
					}
					/* no matching neighbor found - add new neighbor */
					neighbor_list_walker->next = (struct neighbor_list_entry*) malloc(sizeof(struct neighbor_list_entry));
					assert(neighbor_list_walker->next);
					neighbor_list_walker = neighbor_list_walker->next;
					neighbor_list_walker->next = 0;
					neighbor_list_walker->ip_add = pwospf_hdr->rid;
					neighbor_list_walker->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
				}
			}
			iface = iface->next;
		}
	}
	return;
}

/*******************************************************************
*   Deletes a neigbor_list_entry from nghbrs.
*******************************************************************/
struct neighbor_list_entry* delete_neighbor_list_entry(struct interface_list_entry* iface, struct neighbor_list_entry* want_deleted)
{
	struct neighbor_list_entry* prev = 0;
	struct neighbor_list_entry* walker = 0;
	walker = iface->nghbrs;
	
	while(walker)
	{
		if(walker == want_deleted)    /* On item to be deleted in list. */
		{
			if(prev == 0)          /* Item is first in list. */  
			{
				if(iface->nghbrs->next)
				{
					iface->nghbrs = iface->nghbrs->next;
				}	
				else
				{
					iface->nghbrs = NULL;
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
void print_all_neighbor_lists(struct sr_instance* sr)
{
	struct interface_list_entry* interface_list_walker = 0;

	printf("--- INTERFACE LIST ---\n");
	if(sr->interface_list == 0)
	{
		printf("INTERFACE LIST IS EMPTY\n");
		return;
	}
	interface_list_walker = sr->interface_list;
	while(interface_list_walker)
	{
		printf("Interface IP: %i", interface_list_walker->ip_add); /* OLD: inet_ntoa(interface_list_walker->ip_add) */
		printf("--- NEIGHBOR LIST ---\n");
		struct neighbor_list_entry* neighbor_list_walker = 0;
		if(interface_list_walker->nghbrs == 0)
		{
			printf("NEIGHBOR LIST IS EMPTY\n");
			return;
		}
		neighbor_list_walker = interface_list_walker->nghbrs; /* WHY DOES THIS GENERATE A WARNING?? */
		while(neighbor_list_walker)
		{
			print_neighbor_list_entry(neighbor_list_walker);
			neighbor_list_walker = neighbor_list_walker->next;
		}
		interface_list_walker = interface_list_walker->next;
	}
}

/*******************************************************************
*   Prints a single Neighbor List Entry.
*******************************************************************/
void print_neighbor_list_entry(struct neighbor_list_entry* ent)
{
	struct in_addr ip_addr;
	assert(ent);
	ip_addr.s_addr = ent->ip_add;
	printf("IP: %s\t", inet_ntoa(ip_addr));
	printf("Time when Invalid: %lu\n",(long)ent->timenotvalid);
}

/*******************************************************************
*   Creates and returns a HELLO packet.
*******************************************************************/
void create_HELLO()
{

}
