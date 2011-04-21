/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/01/2011
 **********************************************************************/
 
#ifndef HELLO_H
#define HELLO_H

#include <time.h>

#include "sr_router.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"


/*THESE NEED TO ALL TAKE struct sr_instance not packet_state*/
void handle_HELLO(struct packet_state*, struct sr_ethernet_hdr*, struct ip*);
struct neighbor_list* delete_neighbor_list(struct pwospf_iflist*, struct neighbor_list*);
void print_all_neighbor_lists(struct packet_state*);
void print_neighbor_list(struct neighbor_list*);
void send_HELLO(struct packet_state*);

#endif