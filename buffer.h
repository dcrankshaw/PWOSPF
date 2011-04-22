/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/01/2011
 **********************************************************************/

#ifndef BUFFER_H
#define BUFFER_H


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


struct packet_buffer
{
	uint8_t* packet;            /*Packet that wants to be forwarded. */
	uint16_t pack_len;
	struct sr_ethernet_hdr* old_eth; /*Original ethernet header*/
	struct packet_buffer *next;
};


struct packet_buffer * add_to_pack_buff(struct packet_buffer*, uint8_t*, uint16_t, 
                                        struct sr_ethernet_hdr *);
struct packet_buffer* delete_from_buffer(struct packet_state*, struct packet_buffer*);
struct sr_if* get_if_from_mac(struct sr_instance*, unsigned char*);

void send_all_packs(struct packet_buffer* , uint8_t*, char*, struct sr_instance*);
void delete_all_pack(struct packet_buffer* );
void send_all_icmps(struct packet_buffer* , struct sr_instance* );
void send_icmp(struct sr_instance*, uint8_t* , uint16_t , struct sr_ethernet_hdr* );
#endif