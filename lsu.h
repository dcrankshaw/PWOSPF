/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/29/2011
 **********************************************************************/
 
#ifndef LSU_H
#define LSU_H

#include "sr_router.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"

#define LSUINT 30


int handle_lsu(struct ospfv2_hdr*, struct packet_state*, struct ip*);
void send_lsu(struct sr_instance*);
struct ospfv2_lsu_adv* generate_adv(struct ospfv2_lsu_adv*, struct sr_instance*, int);
void forward_lsu(struct packet_state* ps, struct sr_instance* , uint8_t*, struct ip* );
void print_ads(struct ospfv2_lsu_adv* , int );

#endif