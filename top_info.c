/* jkashfkdshfkj */


#define INIT_ADJLIST_SIZE	10
#define INIT_SUBNET_SIZE	10

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#include "sr_if.h"
#include "sr_protocol.h"
#include "sr_pwospf.h"
#include "sr_router.h"
#include "top_info.h"
#include "pwospf_protocol.h"


int remove_neighbor(void)
{
	/*
	* -use if a neighbor gets removed from the interface list
	* -will need to make sure that the neighbor gets removed from the topology as well??
	*/
	
	/*-Remove from neighbor list
		-remove from ospf_subsys->this_router->subnets, adjacencies*/
		printf("Currently unimplemented");
		return 0;
	
}


/*TODO: need struct in_addr nmask as another argument*/
void add_neighbor(struct sr_instance* sr, char *name, uint32_t router_id, struct in_addr ip)
{
	struct pwospf_iflist *current_if = sr->ospf_subsys->interfaces;
	while(current_if)
	{
		if(strcmp(name, current_if->name) != 0)
		{
			current_if = current_if->next;
		}
		else
		{
			struct neighbor_list *current_nbr = current_if->neighbors;
			if(current_nbr == NULL)
			{
				current_if->neighbors = (struct neighbor_list*) malloc(sizeof(struct neighbor_list));
				current_if->neighbors->id = router_id;
				current_if->neighbors->ip_address = ip;
				current_if->neighbors->next = NULL;
				
			}
			else
			{
				while(current_nbr->next != NULL)
				{
					/*neighbor already part of list*/
					if(current_nbr->id == router_id)
					{
						break;
					}
					else
					{
						current_nbr = current_nbr->next;
					}
				}
				if(current_nbr->id == router_id)
				{
					break;
				}
				else
				{
					current_nbr->next = (struct neighbor_list*) malloc(sizeof(struct neighbor_list));
					current_nbr = current_nbr->next;
					current_nbr->id = router_id;
					current_nbr->ip_address = ip;
					current_nbr->next = NULL;
				}
			}
				
			break;
		}
	}
	
	/*Also need to add support to add neighbors to sr->ospf_subsys->this_router->subnets
		can match based on incoming mask, prefix (from ip&mask), then check if the matching
		subnet rid == 0. If it is, update it to router_id. Else add a new entry to subnets with
		the found mask, prefix, but with router_id (indicates two routers on same subnet
		with switch).*/


}

/*checks whether there are any expired entries in the topoology*/
void check_top_invalid(struct sr_instance *sr)
{
	struct adj_list *current = sr->ospf_subsys->network;
	time_t now = time(NULL);
	struct adj_list *prev = NULL;
	while(current)
	{
		if(current->rt->expired <= now && current->rt->rid != sr->ospf_subsys->this_router->rid)
		{
			remove_from_topo(sr, current->rt);
			if(prev == NULL)
			{
				sr->ospf_subsys->network = current->next;
				free(current);
				current = sr->ospf_subsys->network;
			}
			else if(current->next)
			{
				prev->next = current->next;
				free(current);
				current = prev->next;
			}
			else
			{
				prev->next = NULL;
				free(current);
				current = NULL;
			}
		}
		else
		{
			current = current->next;
		}
	}
}

/*removes from the given router's subnet list and adjacency list*/
int remove_subnet_from_router(struct sr_instance *sr, struct router *rt, struct route *subnet)
{
	/*decrease size, remove appropriate subnet, move last subnet into empty spot in the array
	this keeps all entries packed*/
	int i;
	for(i = 0; i < rt->subnet_size; i++)
	{
		if(route_cmp(rt->subnets[i], subnet) == 0)
		{
			rt->subnets[i] = NULL;
			rt->subnets[i] = rt->subnets[rt->subnet_size-1];
			i = rt->subnet_size;
			rt->subnet_size--;
		}
	}
	/*if this subnet is a router*/
	if(subnet->r_id != 0)
	{
		for(i = 0; i < rt->adj_size; i++)
		{
			if(subnet->r_id == rt->adjacencies[i]->rid)
			{
				rt->adjacencies[i] = NULL;
				rt->adjacencies[i] = rt->adjacencies[rt->adj_size-1];
				i = rt->adj_size;
				rt->adj_size--;
			}
		}
	}
	return 1;
}

int remove_rt_sn_using_id(struct sr_instance *sr, struct router *rt, uint32_t id)
{
		/*decrease size, remove appropriate subnet, move last subnet into empty spot in the array
	this keeps all entries packed*/
	int i;
	for(i = 0; i < rt->subnet_size; i++)
	{
		if(rt->subnets[i]->r_id == id)
		{
			rt->subnets[i] = NULL;
			rt->subnets[i] = rt->subnets[rt->subnet_size-1];
			i = rt->subnet_size;
			rt->subnet_size--;
		}
	}
	
	for(i = 0; i < rt->adj_size; i++)
	{
		if(id == rt->adjacencies[i]->rid)
		{
			rt->adjacencies[i] = NULL;
			rt->adjacencies[i] = rt->adjacencies[rt->adj_size-1];
			i = rt->adj_size;
			rt->adj_size--;
		}
	}
	return 1;
}

int route_cmp(struct route* r1, struct route* r2)
{
	if((r1->prefix.s_addr == r2->prefix.s_addr) && (r1->mask.s_addr == r2->mask.s_addr) && (r1->r_id == r2->r_id))
	{
		return 0;
	}
	return -1;
}

/*TODO*/
int remove_from_topo(struct sr_instance *sr, struct router *rt)
{
	int i;
	/*free all allocated memory storing this router's routes*/
	for(i = 0; i < rt->subnet_size; i++)
	{
		free(rt->subnets[i]);
	}
	for(i = 0; i < rt->adj_size; i++)
	{
		remove_rt_sn_using_id(sr, rt->adjacencies[i], rt->rid);
	}
	free(rt->subnets);
	free(rt->adjacencies);
	
	
	
	/*look at all adjacencies, delete all pointers to this router, then free the router,
		DON'T FORGET TO DELETE ROUTES FROM SUBNETS IN THE ROUTERS TOO*/	
		
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	return 1;
	
}


/* called when LSU packets are received. Adds to the adjacency list */

/*TODO: What if we get an LSU that advertises a link connecting to a router we don't
know about?*/
int add_to_top(struct sr_instance* sr, uint32_t host_rid, struct route** advert_routes,
				int num_ads)
{
	struct router* host = adj_list_contains(sr, host_rid);
	if(host != NULL)
	{
		add_to_existing_router(sr, advert_routes, host, num_ads);	
	}
	else
	{
		host = add_new_router(sr, host_rid);
		if(host != NULL)
		{
			add_to_existing_router(sr, advert_routes, host, num_ads);
		}
		else
		{
			/*ERROR*/
		}
		
	
	}
	/*recompute dijkstra's algorithm*/
	
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	return 1;
}

struct router* adj_list_contains(struct sr_instance *sr, uint32_t id)
{
	struct adj_list *current = sr->ospf_subsys->network;
	while(current)
	{
		if(current->rt->rid == id)
		{
			return current->rt;
		}
	}
	return NULL;
}


void add_to_existing_router(struct sr_instance *sr, struct route **routes, struct router* host, int num_ads)
{
	/*TODO: this is pretty inefficient, it may be alright though if these stay small enough */
		host->expired = time(NULL) + 3*OSPF_DEFAULT_LSUINT;
		int i;
		for(i = 0; i < num_ads; i++)
		{
			/*If this does not already exist in the router's subnets*/
			if(router_contains(routes[i], host) == 0)
			{
				
				add_new_route(sr, routes[i], host);
			}
		}
}

int router_contains(struct route* rt, struct router *host)
{
	int i;
	for(i = 0; i < host->subnet_size; i++)
	{
		if(route_cmp(host->subnets[i], rt) == 0)
		{
			return 1;
		}
	}
	return 0;
}

/*May not need this method???*/
struct route* router_contains_subnet(struct router* host, uint32_t prefix)
{
	int i;
	for(i = 0; i < host->subnet_size; i++)
	{
		if(host->subnets[i]->prefix.s_addr == prefix)
		{
			return host->subnets[i];
		}
	}
	return 0;

}

void add_new_route(struct sr_instance *sr, struct route* current, struct router* host)
{
	
	if(host->subnet_buf_size == host->subnet_size)
	{
		host->subnets = realloc(host->subnets, 2*host->subnet_buf_size); //double size of array
		host->subnet_buf_size *= 2;
	}
	
	/*add to adj_list*/
	int invalid = 0;
	if(current->r_id != 0)
	{
		if(host->adj_buf_size == host->adj_size)
		{
			host->adjacencies = realloc(host->adjacencies, 2*host->adj_buf_size);
			host->adj_buf_size *= 2;
		
		}
		/*get router pointer to add to adj list*/
		struct adj_list *cur_router = sr->ospf_subsys->network;
		int added = 0;
		while(cur_router)
		{
			if(cur_router->rt->rid == current->r_id)
			{
				struct route* other_sub = router_contains_subnet(cur_router->rt, current->prefix.s_addr);
				/*This is an invalid connection*/
				if((other_sub != NULL) && ((other_sub->mask.s_addr != current->mask.s_addr) ||
						(other_sub->r_id != host->rid)))
				{
					remove_rt_sn_using_id(sr, cur_router->rt, other_sub->r_id);
					invalid = 1;
				}
				else
				{
					host->adjacencies[host->adj_size] = cur_router->rt;
					host->adj_size++;
					added = 1;
				}
				
				if(!invalid)
				{
					
					/*add to list of subnets*/
					struct route* old_sub = router_contains_subnet(cur_router->rt, current->prefix.s_addr);
					/*This is a check for that weird FAQ issue about initialzing subnets*/
					/*Basically, if we initialized the connection at startup, then later received an LSU*/
					if(old_sub != NULL && old_sub->r_id == 0 && old_sub->mask.s_addr == current->mask.s_addr)
					{
						old_sub->r_id = current->r_id;
					}
					else
					{
						memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
						host->subnet_size++;
					}	
				}
				break;
				
			}
			else
			{
				cur_router = cur_router->next;
			}
		}
		if(!added)
		{
			/*create new route*/
			struct router *new_adj = add_new_router(sr, current->r_id);
			host->adjacencies[host->adj_size] = new_adj;
			host->adj_size++;
			struct route* opp_route = (struct route*) malloc(sizeof(struct route));
			memmove(opp_route, current, sizeof(struct route));
			opp_route->r_id = host->rid;
			add_new_route(sr, opp_route, new_adj);
		}
	}
	
	/*Need to resize subnet buffer*/
	else
	{
			memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
			host->subnet_size++;
	}
}

struct router* add_new_router(struct sr_instance *sr, uint32_t host_rid)
{
	struct router* new_router = (struct router*)malloc(sizeof(struct router));
	struct adj_list *new_adj_entry = (struct adj_list *)malloc(sizeof(struct adj_list));
	
	if(new_router && new_adj_entry)
	{
		new_adj_entry->rt = new_router;
		new_adj_entry->next = NULL;
		struct adj_list* current = sr->ospf_subsys->network;
		struct adj_list* prev = NULL;
		/*find end of list*/
		while(current)
		{
			prev = current;
			current = current->next;
		}
		if(!prev)
		{
			sr->ospf_subsys->network = new_adj_entry;
		}
		else
		{
			prev->next = new_adj_entry;
		}
		new_router->adjacencies = (struct router **) malloc(INIT_ADJLIST_SIZE*sizeof(struct router*));
		new_router->adj_buf_size = INIT_ADJLIST_SIZE;
		new_router->adj_size = 0;
		
		new_router->subnets = (struct route **) malloc(INIT_SUBNET_SIZE*sizeof(struct route*));
		new_router->subnet_buf_size = INIT_SUBNET_SIZE;
		new_router->subnet_size = 0;
		
		/*TODO:will need to initialize this to the value given in the LSU*/
		new_router->last_seq = 0;
		new_router->rid = host_rid;
		new_router->known = 0;
		new_router->dist  = -1;
		new_router->prev = NULL;
		return new_router;
	}
	else
	{
		printf("Malloc error\n");
	}
	return NULL;
}

void get_if_and_neighbor(struct pwospf_iflist *ifret, struct neighbor_list *nbrret,
						struct sr_instance *sr, uint32_t id)
{
	ifret = NULL;	/*these are the corresponding iface and neighbor that the caller needs */
	nbrret = NULL;
	int done = 0;
	
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	while(cur_if)
	{
		struct neighbor_list *cur_nbr = cur_if->neighbors;
		while(cur_nbr)
		{
			if(cur_nbr->id == id)
			{
				ifret = cur_if;
				nbrret = cur_nbr;
				done = 1;
				break;
			}
			else
			{
				cur_nbr = cur_nbr->next;
			}
		}
		if(done)
		{
			break;
		}
		else
		{
			cur_if = cur_if->next;
		}
	}
}


int update_ftable(struct sr_instance *sr)
{
	reset_ftable(sr);
	struct adj_list *current = sr->ospf_subsys->network;
	while(current)
	{
		int numhops = -1;
		struct router* next_hop = find_next_hop(sr, current->rt, &numhops);
		if(next_hop == NULL)
		{
			printf("Error");
			return 0;
		}
		struct pwospf_iflist *iface = NULL;
		struct neighbor_list *nbr = NULL;
		get_if_and_neighbor(iface, nbr, sr, next_hop->rid);
		if(iface == NULL || nbr == NULL)
		{
			printf("Error");
			return 0;
		}
		
		int i;
		for(i = 0; i < current->rt->subnet_size; i++)
		{
			struct ftable_entry *cur_entry = ftable_contains(sr, current->rt->subnets[i]->prefix,
					current->rt->subnets[i]->mask);
			
			if(cur_entry == NULL)
			{
				cur_entry = (struct ftable_entry *)malloc(sizeof(struct ftable_entry));
				cur_entry->prefix = current->rt->subnets[i]->prefix;
				cur_entry->mask = current->rt->subnets[i]->mask;
				
				
				
				if(numhops == 0)
				{
					cur_entry->next_hop.s_addr = 0;
				}
				else
				{
					cur_entry->next_hop.s_addr = nbr->ip_address.s_addr;
				}
				memmove(cur_entry->interface, iface->name, sr_IFACE_NAMELEN);
				
				
				cur_entry->num_hops = numhops;
				cur_entry->next = NULL;
				struct ftable_entry *walker = sr->ospf_subsys->fwrd_table;
				if(walker == NULL)
				{
					sr->ospf_subsys->fwrd_table = cur_entry;
				}
				else
				{
					while(walker->next)
					{
						walker = walker->next;
					}
					walker->next = cur_entry;
				}
			}
			else if(cur_entry->num_hops > numhops)
			{
				
				if(numhops == 0)
				{
					cur_entry->next_hop.s_addr = 0;
				}
				else
				{
					cur_entry->next_hop.s_addr = nbr->ip_address.s_addr;
				}
				memmove(cur_entry->interface, iface->name, sr_IFACE_NAMELEN);
				cur_entry->num_hops = numhops;
			}
		}
	}
	return 1;
	
}

struct router* find_next_hop(struct sr_instance *sr, struct router *dest, int *hops)
{
	int numhops = 0;
	struct router *cur = dest;
	while((cur->prev != NULL) && (cur->prev->rid != sr->ospf_subsys->this_router->rid))
	{
		numhops++;
		cur = cur->prev;
	}
	if(cur->prev == NULL)
	{
		/*ERROR*/
		printf("Error finding next hop\n");
		*hops = -1;
		return NULL;
	}
	*hops = numhops;
	return cur;
}

struct ftable_entry *ftable_contains(struct sr_instance *sr, struct in_addr pfix, struct in_addr mask)
{
	struct ftable_entry *current = sr->ospf_subsys->fwrd_table;
	while(current)
	{
		if((current->prefix.s_addr == pfix.s_addr) && (current->mask.s_addr == mask.s_addr))
		{
			return current;
		}
		else
		{
			current = current->next;
		}
	}
	return NULL;
}

int reset_ftable(struct sr_instance *sr)
{
	struct ftable_entry *current = sr->ospf_subsys->fwrd_table;
	sr->ospf_subsys->fwrd_table = NULL;
	struct ftable_entry *prev = NULL;
	while(current)
	{
		prev = current;
		current = current->next;
		free(prev);
	}
	return 1;
}




void dijkstra(struct sr_instance* sr, struct router *host)
{
	struct adj_list *current = sr->ospf_subsys->network;
	while(current != NULL)
	{
		current->rt->known = 0;
		current->rt->dist = -1;
		current->rt->prev = NULL;
		current = current->next;
	}
	
	
	sr->ospf_subsys->this_router->dist = 0;
	struct router* least_unknown = get_smallest_unknown(sr->ospf_subsys->network);
	while(least_unknown != NULL)
	{
		least_unknown->known = 1; /*mark it as visited*/
		int i;
		struct router *w;
		for(i = 0; i < least_unknown->adj_size; i++)
		{
			w = least_unknown->adjacencies[i];
			if(w->known == 0)
			{
				if((least_unknown->dist + 1) < w->dist || w->dist < 0)
				{
					w->dist = least_unknown->dist + 1; /* update the cost */
					w->prev = least_unknown;
				}
			}
		}
		least_unknown = get_smallest_unknown(sr->ospf_subsys->network);
	}
}

struct router* get_smallest_unknown(struct adj_list *current)
{
	struct router* least_unknown = NULL;
	int min_dist = -1;
	while(current != NULL)
	{
		if(current->rt->known)
		{
			current = current->next;
		}
		else
		{
			if((min_dist < 0 && current->rt->dist >= 0) || (min_dist > current->rt->dist))
			{
				min_dist = current->rt->dist;
				least_unknown = current->rt;
			}
			current = current->next;
		}
	}
	return least_unknown;
}

uint16_t get_sequence(uint32_t router_id, struct sr_instance *sr)
{
    struct adj_list* net_walker=sr->ospf_subsys->network;
    while(net_walker)
    {
        if(net_walker->rt->rid==router_id)
        {
            return net_walker->rt->last_seq;
        }
        else
            net_walker=net_walker->next;
    }
    return 0;
}
void set_sequence(uint32_t router_id, uint16_t sequence, struct sr_instance *sr)
{
    struct adj_list* net_walker=sr->ospf_subsys->network;
    while(net_walker)
    {
        if(net_walker->rt->rid==router_id)
        {
            net_walker->rt->last_seq=sequence;
        }
        else
            net_walker=net_walker->next;
    }
}
