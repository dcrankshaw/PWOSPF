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
#include "hello.h"


/*******************************************************************
*   Called when handle_packet() receives a HELLO packet.
********************************************************************/
void handle_HELLO(struct packet_state* ps, struct ip* ip_hdr)
{
	struct ospfv2_hdr* pwospf_hdr = 0;
	struct ospfv2_hello_hdr* hello_hdr = 0;
	
	pwospf_lock(ps->sr->ospf_subsys);
	struct pwospf_iflist* iface = ps->sr->ospf_subsys->interfaces;

	if(ps->len < (sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr))) /* incorrectly sized packet */
	{
		fprintf(stderr, "Malformed HELLO Packet.");
		ps->res_len=0;
	}
	else /* correctly sized packet */
	{
		pwospf_hdr = (struct ospfv2_hdr*)(ps->packet);
		hello_hdr = (struct ospfv2_hello_hdr*)(ps->packet + sizeof(struct ospfv2_hdr));

		/* check incoming packet values against ONLY the receiving interface in the interface list */
		if(iface == 0)
		{
			fprintf(stderr, "ERROR - INTERFACE LIST NOT INITIALIZED");
		}
		int found = 0;
		while(iface)
		{
			int i;
			for(i = 0; i < iface->nbr_size; i++)
			{                
                if(iface->neighbors[i]->timenotvalid < time(NULL))
                {
                    free(iface->neighbors[i]); /*delete the entry*/
                    iface->neighbors[i] = NULL;
                    iface->neighbors[i] = iface->neighbors[iface->nbr_size - 1]; /*move last entry into the now empty spot*/
                    iface->neighbors[iface->nbr_size - 1] = NULL; /*set last entry to NULL*/
                    iface->nbr_size--; /*decrease the number of neighbors in the list*/
                    i--; /*we have to recheck the new element we placed in the deleted elements spot*/
                            /*we will still exit the for loop though if the deleted element was the last one*/
                }	
			}
			
			if(strcmp(iface->name, ps->interface) == 0) /* if the current interface equals the incoming interface */
			{
				
				if((iface->mask.s_addr != ntohl(hello_hdr->nmask)) || (iface->helloint != ntohs(hello_hdr->helloint)))
				{
					/* drop packet */
					fprintf(stderr,"HELLO doesn't match any interface - packet dropped");
					pwospf_unlock(ps->sr->ospf_subsys);
					return;
				}

				/* once interface is decided: */
				
				assert((iface->nbr_buf_size > 0));
				if(iface->nbr_buf_size <= iface->nbr_size)
				{
					/*Need to resize array*/
					struct neighbor_list** temp = realloc(iface->neighbors, 2*iface->nbr_buf_size);
					assert(temp); /*Will fail if realloc fails*/
					iface->neighbors = temp;
					iface->nbr_buf_size *= 2;
				}
				
				for(i = 0; i < iface->nbr_size; i++)
				{
					if(iface->neighbors[i]->ip_address.s_addr == ip_hdr->ip_src.s_addr)
					{
						iface->neighbors[i]->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
						found = 1;
					}
				}
				if(!found)
				{
				
					iface->neighbors[iface->nbr_size] = (struct neighbor_list*) calloc(1, sizeof(struct neighbor_list));
					iface->neighbors[iface->nbr_size]->id = pwospf_hdr->rid;
					iface->neighbors[iface->nbr_size]->ip_address = ip_hdr->ip_src;
					iface->neighbors[iface->nbr_size]->timenotvalid = time(NULL) + OSPF_NEIGHBOR_TIMEOUT;
					iface->nbr_size++;
				}
				
				
				struct in_addr new_pref;
				new_pref.s_addr = (ip_hdr->ip_src.s_addr & htonl(hello_hdr->nmask));
				struct route* old_sub = router_contains_subnet(ps->sr->ospf_subsys->this_router, new_pref.s_addr);
				/*This is a check for that weird FAQ issue about initialzing subnets*/
				/*Basically, if we initialized the connection at startup, then later received an LSU*/
				if(old_sub != NULL && old_sub->r_id == 0 && old_sub->mask.s_addr == ntohl(hello_hdr->nmask))
				{
					struct in_addr rid;
                    rid.s_addr=pwospf_hdr->rid;
                    old_sub->r_id = pwospf_hdr->rid;
                    rid.s_addr=old_sub->r_id;

				}
			}
			iface = iface->next;
		}
	}
	pwospf_unlock(ps->sr->ospf_subsys);
	return;
}

/*******************************************************************
*   Creates and sends a HELLO packet with Ethernet, IP, OSPF, and OSPF_HELLO headers.
*******************************************************************/
void send_HELLO(struct sr_instance* sr)
{
	uint16_t packet_size = sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + 
	                                    sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr);
	uint8_t* outgoing_packet_buffer = (uint8_t*)malloc(packet_size);
	
	struct sr_ethernet_hdr* eth_hdr = (struct sr_ethernet_hdr*) outgoing_packet_buffer;
	struct ip* ip_hdr = (struct ip*) (outgoing_packet_buffer + sizeof(struct sr_ethernet_hdr));
	struct ospfv2_hdr* pwospf_hdr = (struct ospfv2_hdr*) (outgoing_packet_buffer + 
	                                    sizeof(struct sr_ethernet_hdr) + sizeof(struct ip));
	struct ospfv2_hello_hdr* hello_hdr = (struct ospfv2_hello_hdr*) (outgoing_packet_buffer + 
	                sizeof(struct sr_ethernet_hdr) + sizeof(struct ip) + sizeof(struct ospfv2_hdr));
	
	/* Set Ethernet destination MAC address to ff:ff:ff:ff:ff:ff (Broadcast) */
	int i = 0;
	for(i=0; i<ETHER_ADDR_LEN; i++)
	{
		eth_hdr->ether_dhost[i] = 0xff;
	}
	eth_hdr->ether_type=htons(ETHERTYPE_IP);

	/* Set IP destination IP address to 224.0.0.5 (0xe0000005) (Broadcast) */
	
	ip_hdr->ip_hl = (sizeof(struct ip))/4;
	ip_hdr->ip_v = IP_VERSION;
	ip_hdr->ip_tos=ROUTINE_SERVICE;
	ip_hdr->ip_len = htons(packet_size - sizeof(struct sr_ethernet_hdr));
	ip_hdr->ip_id = 0; 
	ip_hdr->ip_off = 0; 
	ip_hdr->ip_ttl = INIT_TTL;
	ip_hdr->ip_p = OSPFV2_TYPE;
	ip_hdr->ip_dst.s_addr = htonl(OSPF_AllSPFRouters); 

    /* Set up HELLO header. */
	hello_hdr->helloint = htons(OSPF_DEFAULT_HELLOINT);
	hello_hdr->padding = 0;

	/* Set up PWOSPF header. */
	pwospf_hdr->version = OSPF_V2;
	pwospf_hdr->type = OSPF_TYPE_HELLO;
	pwospf_hdr->len = htons(sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_hello_hdr));
	pwospf_lock(sr->ospf_subsys);
	pwospf_hdr->rid = sr->ospf_subsys->this_router->rid;
	pwospf_hdr->aid = htonl(sr->ospf_subsys->area_id);
	pwospf_hdr->autype = OSPF_DEFAULT_AUTHKEY;
	pwospf_hdr->audata = OSPF_DEFAULT_AUTHKEY;

	/* Send the packet out on each interface. */
	struct pwospf_iflist* iface = sr->ospf_subsys->interfaces;
	assert(iface);
	while(iface)
	{
		
		hello_hdr->nmask = htonl(iface->mask.s_addr);
		pwospf_hdr->csum = 0;
	    pwospf_hdr->csum = cksum((uint8_t *)pwospf_hdr, sizeof(struct ospfv2_hdr)-8); 
	    pwospf_hdr->csum = htons(pwospf_hdr->csum);
		
		
		ip_hdr->ip_src = iface->address;
		ip_hdr->ip_sum = 0;
        ip_hdr->ip_sum = cksum((uint8_t *)ip_hdr, sizeof(struct ip));
        ip_hdr->ip_sum = htons(ip_hdr->ip_sum);
	
	    memmove(eth_hdr->ether_shost, iface->mac, ETHER_ADDR_LEN);
	
		sr_send_packet(sr, outgoing_packet_buffer, packet_size, iface->name);
	    iface = iface->next;
	}

	if(outgoing_packet_buffer)
		free(outgoing_packet_buffer);
	pwospf_unlock(sr->ospf_subsys);
}