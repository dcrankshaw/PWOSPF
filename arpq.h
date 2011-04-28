/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/29/2011
 **********************************************************************/

#ifndef ARPQ_H
#define ARPQ_H


#include <pthread.h>
#include <arpa/inet.h>

#include "arp.h"
#include "sr_if.h"


struct arp_subsys
{
	struct arpq *pending;
	struct arp_cache_entry* arp_cache;
	pthread_mutex_t cache_lock;
	pthread_mutex_t arp_q_lock;
} __attribute__ ((packed));

struct arpq
{
	struct in_addr ip; /* ip address of the next hop, used to identify matching ARP requests */
	int num_requests; /* the number of requests already sent by this arp_thread */
	pthread_t arp_thread;
	uint8_t* arp_request;
	uint16_t request_len;
	struct packet_buffer* pac_buf; /*the linked list buffering tcp/udp/icmp packets*/
	struct lsu_buf_ent* lsu_buf; /*the linked list buffering lsu packets*/
	char iface_name[sr_IFACE_NAMELEN];
	struct arpq* next;
};

struct thread_args
{
	struct sr_instance* sr;
	struct arpq* entry;
} __attribute__ ((packed));



void arp_subsys_init(struct sr_instance* sr);
void get_mac_address(struct sr_instance *, struct in_addr, uint8_t *,
					uint16_t , char *, int, struct sr_ethernet_hdr*);
void* arp_req_init(void*);
struct arpq* get_entry(struct sr_instance *, struct in_addr);
struct arpq* create_entry(struct sr_instance *sr, struct arp_subsys*, struct in_addr, char*);
void lock_cache(struct arp_subsys*);
void unlock_cache(struct arp_subsys*);
void lock_arp_q(struct arp_subsys*);
void unlock_arp_q(struct arp_subsys*);

#endif