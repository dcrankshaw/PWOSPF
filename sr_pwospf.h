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

/* forward declare */
struct sr_instance;


struct neighbor_list
{
	uint32_t id; /*Router ID*/
	struct in_addr ip_address;
	struct neighbor_list *next;
};

struct route
{
	uint32_t prefix;
	uint32_t mask;
	uint32_t next_hop;
	uint32_t r_id;

};


typedef struct router
{
	struct router **adjacencies;
	
	int buf_size;	/* length of the dynamically allocated array, to prevent illegal mem access */
	int list_size;	/* number of entries in the list (list_size <= buf_size at all times) */
	
	struct route **subnets;
	int subnet_buf_size;
	int subnet_size;
	uint16_t last_seq;
	
	uint32_t rid;
	
	/*for Dijkstra's algorithm */
	int known;
	int dist;
	struct router *prev;
} node;

struct adj_list
{
	struct router *rt;
	struct adj_list *next;
};


/*TODO: make sure this is the right way to define an entry in the forwarding table*/
struct ftable_entry
{
	struct ftable_entry *next;
	struct in_addr subnet; /*aka dest */
	struct in_addr mask;
	struct in_addr next_hop;
	char *interface;	/*the interface to send the packet out of*/
};

struct pwospf_iflist
{
	struct in_addr address;
	struct in_addr mask;
	uint16_t helloint;
	struct neighbor_list *neighbors;
};




struct pwospf_subsys
{
    /* -- pwospf subsystem state variables here -- */
    struct adj_list *network;
    struct ftable_entry *fwrd_table;
    struct pwospf_iflist* neighbors;
    struct router *this_router;
    uint16_t last_seq_sent;


    /* -- thread and single lock for pwospf subsystem -- */
    pthread_t thread;
    pthread_mutex_t lock;
};

int pwospf_init(struct sr_instance* sr);


#endif /* SR_PWOSPF_H */
