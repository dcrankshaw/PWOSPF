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
*
*   Called when handle_packet() receives a HELLO packet.
*
********************************************************************/
void handle_HELLO(struct packet_state* ps, struct sr_ethernet_hdr* eth)
{
	struct ospfv2_hdr* pwospf_hdr = 0;
	struct ospfv2_hello_hdr* hello_hdr = 0;
	struct interface_list_entry* iface = 0;

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
					iface->nghbrs->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
				}
				else
				{
					neighbor_list_walker = iface->nghbrs;
					while(neighbor_list_walker != NULL)
					{
						/* TODO -  packet matches entry, update the 'last hello packet received timer' */
						
						neighbor_list_walker = neighbor_list_walker->next;
					}
					/* no matching neigbor found - add new neighbor */
				}
			}
			iface = iface->next;
		}
	}
	return;
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
