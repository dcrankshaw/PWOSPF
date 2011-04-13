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

struct interface_list_entry
{
    char* interface;
    uint32_t ip_add;
    uint32_t nmask;
    uint16_t helloint;
    unsigned char mac[ETHER_ADDR_LEN];
    struct neighbor_list_entry* nghbrs;
    struct interface_list_entry* next;
};

struct neighbor_list_entry
{
    uint32_t ip_add;
    time_t timenotvalid;                /*The time when this entry is no longer valid*/
    struct neighbor_list_entry* next;
};

void handle_HELLO(struct packet_state*, struct sr_ethernet_hdr*);
struct neighbor_list_entry* delete_neighbor_list_entry(struct interface_list_entry*, struct neighbor_list_entry*);
void print_all_neighbor_lists(struct sr_instance*);
void print_neighbor_list_entry(struct neighbor_list_entry*);
void create_HELLO();

#endif