/*-----------------------------------------------------------------------------
 * file:  sr_pwospf.h
 * date:  Tue Nov 23 23:21:22 PST 2004 
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#ifndef SR_PWOSPF_H
#define SR_PWOSPF_H

#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>

#include "sr_router.h"
#include "sr_protocol.h"

/* forward declare */
struct sr_instance;

#define IF_MASK 0xfffffffe
#define DEF_NBR_BUF	5 /*The default size to make the array of neighbor* structs*/



struct route
{
	struct in_addr prefix;
	struct in_addr mask;
	struct in_addr next_hop; /*probably don't need*/
	uint32_t r_id;

} __attribute__ ((packed));


struct router
{
	struct router **adjacencies;
	int adj_buf_size;	/* length of the dynamically allocated array, to prevent illegal mem access */
	int adj_size;	/* number of entries in the list (list_size <= buf_size at all times) */
	
	struct route **subnets;
	int subnet_buf_size;
	int subnet_size;
	
	uint16_t last_seq;
	/*time time*/
	time_t expired;
	
	uint32_t rid;
	
	/*for Dijkstra's algorithm */
	int known;
	int dist;
	struct router *prev;
} __attribute__ ((packed));

struct adj_list
{
	struct router *rt;
	struct adj_list *next;
} __attribute__ ((packed));


/*TODO: make sure this is the right way to define an entry in the forwarding table*/
struct ftable_entry
{
	
	struct in_addr prefix; /*aka dest */
	struct in_addr mask;
	struct in_addr next_hop;
	char interface[sr_IFACE_NAMELEN];	/*the interface to send the packet out of*/
	int num_hops;
	struct ftable_entry *next;
} __attribute__ ((packed));

struct pwospf_iflist
{
	struct in_addr address;
	struct in_addr mask;
	char name[sr_IFACE_NAMELEN];
	uint16_t helloint;
	unsigned char mac[ETHER_ADDR_LEN];
	struct neighbor_list **neighbors;
	int nbr_size;
	int nbr_buf_size;
	
	
	struct pwospf_iflist *next;
} __attribute__ ((packed));

struct neighbor_list
{
	uint32_t id;
	struct in_addr ip_address;
	/*time time*/
	time_t timenotvalid;                /*The time when this entry is no longer valid*/
	//struct neighbor_list *next;
} __attribute__ ((packed));

struct pwospf_subsys
{
    /* -- pwospf subsystem state variables here -- */
    struct adj_list *network;
    struct ftable_entry *fwrd_table;
    struct pwospf_iflist* interfaces;
    struct router *this_router;
    uint16_t last_seq_sent;
    uint32_t area_id;
    uint16_t autype;
    struct lsu_buf_ent* lsu_buffer;

    /* -- thread and single lock for pwospf subsystem -- */
    pthread_t thread;
    pthread_mutex_t lock;
};

int pwospf_init(struct sr_instance* );
void create_pwospf_ifaces(struct sr_instance *);
int handle_pwospf(struct packet_state* , struct ip* );
uint32_t read_config(const char*);
void pwospf_lock(struct pwospf_subsys*);
void pwospf_unlock(struct pwospf_subsys* subsys);

#endif /* SR_PWOSPF_H */
