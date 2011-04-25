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


void handle_HELLO(struct packet_state*, struct ip*);
void delete_neighbor_list(struct pwospf_iflist*);
void print_all_neighbor_lists(struct packet_state*);
void print_neighbor_list(struct neighbor_list*);
void send_HELLO(struct sr_instance*);

#endif