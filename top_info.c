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

/*NOT THREADSAFE*/
void print_nbr_list(struct sr_instance *sr)
{
	struct pwospf_iflist* iface = sr->ospf_subsys->interfaces;
	fprintf(stderr, "PRINTING NEIGHBOR LIST:\n");
	while(iface)
	{
		fprintf(stderr, "interface %s\n", iface->name);
		struct neighbor_list* nbr = iface->neighbors;
		while(nbr)
		{
			print_nbr(nbr);
			nbr = nbr->next;
		}
		fprintf(stderr, "\n");
		iface = iface->next;
	}
	fprintf(stderr, "\n");
}

/*THREADSAFE*/
void print_nbr(struct neighbor_list* nbr)
{
	fprintf(stderr, "ID: %u\n", nbr->id);
}

/*NOT THREADSAFE*/
/* This method is never used ---> Can be deleted */
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

/*THREADSAFE*/
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

/*THREADSAFE*/
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

/*THREADSAFE*/
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

/*THREADSAFE*/
int route_cmp(struct route* r1, struct route* r2)
{
	if((r1->prefix.s_addr == r2->prefix.s_addr) && (r1->mask.s_addr == r2->mask.s_addr) && (r1->r_id == r2->r_id))
	{
		return 0;
	}
	return -1;
}

/*TODO*/
/*NOT THREADSAFE*/
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
	
	
	
	/*TODO TODO
		look at all adjacencies, delete all pointers to this router, then free the router,
		DON'T FORGET TO DELETE ROUTES FROM SUBNETS IN THE ROUTERS TOO*/	
		
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	return 1;
	
}

void print_topo(struct sr_instance *sr)
{
	struct adj_list* adj_walker = sr->ospf_subsys->network;
	fprintf(stderr, "---TOPOLOGY---\n\n");
	while(adj_walker)
	{
		print_rt(adj_walker->rt);
		fprintf(stderr, "\n");
		adj_walker = adj_walker->next;
	}
	fprintf(stderr, "\n");

}

void print_rt(struct router* rt)
{
	struct in_addr this_id;
	this_id.s_addr = rt->rid;
	fprintf(stderr, "ID: %s\n", inet_ntoa(this_id));
	fprintf(stderr, "Adjacencies:");
	int i;
	for(i = 0; i < rt->adj_size; i++)
	{
		struct in_addr id;
		id.s_addr = rt->adjacencies[i]->rid;
		fprintf(stderr, " %s, ", inet_ntoa(id));
	}
	
	fprintf(stderr, "\nSubnets:");
	for(i = 0; i < rt->subnet_size; i++)
	{
		fprintf(stderr, " %s, ", inet_ntoa(rt->subnets[i]->prefix));
	}

}


/* called when LSU packets are received. Adds to the adjacency list */
/*THREADSAFE*/
int add_to_top(struct sr_instance* sr, uint32_t host_rid, struct route** advert_routes,
				int num_ads)
{
	
	pwospf_lock(sr->ospf_subsys);
	fprintf(stderr, "Locked ospf subsys\n");
	struct router* host = adj_list_contains(sr, host_rid);
	if(host != NULL)
	{
		fprintf(stderr, "1 - Found an existing router\n");
		add_to_existing_router(sr, advert_routes, host, num_ads);	
	}
	else
	{
		fprintf(stderr, "2 - Did not find a router\n");
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
	print_topo(sr);
	
	
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	pwospf_unlock(sr->ospf_subsys);
	return 1;
}

/*NOT THREADSAFE*/
struct router* adj_list_contains(struct sr_instance *sr, uint32_t id)
{
	struct adj_list *current = sr->ospf_subsys->network;
	while(current)
	{
		if(current->rt->rid == id)
		{
			return current->rt;
		}
		current = current->next;
	}
	return NULL;
}

/*THREADSAFE*/
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

/*THREADSAFE*/
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

/*THREADSAFE*/
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

/*THREADSAFE*/
void add_new_route(struct sr_instance *sr, struct route* current, struct router* host)
{
	
	/*Need to resize subnet buffer*/
	if(host->subnet_buf_size == host->subnet_size)
	{
		host->subnets = realloc(host->subnets, 2*host->subnet_buf_size); //double size of array
		host->subnet_buf_size *= 2;
	}
	
	/*add to adj_list*/
	//int invalid = 0;
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
			struct in_addr adj_r_id;
			adj_r_id.s_addr = current->r_id;
			struct in_addr host_router_id;
			host_router_id.s_addr = cur_router->rt->rid;
			if(cur_router->rt->rid == current->r_id)
			{
				added = 1;
				if(cur_router->rt->adj_buf_size == cur_router->rt->adj_size)
				{
					cur_router->rt->adjacencies = realloc(cur_router->rt->adjacencies, 2*cur_router->rt->adj_buf_size);
					cur_router->rt->adj_buf_size *= 2;
				}
				fprintf(stderr, "Found matching router\n");
				struct route* other_sub = router_contains_subnet(cur_router->rt, current->prefix.s_addr);
				if(other_sub != NULL)
				{
					
					/*TODO: decide whether this will break anything!!!!!!!*/
					other_sub->r_id = ntohl(other_sub->r_id);
					if(other_sub->mask.s_addr != current->mask.s_addr)
					{
						fprintf(stderr, "Non-matching masks - removed a subnet\n");
						remove_rt_sn_using_id(sr, cur_router->rt, other_sub->r_id);
						//invalid = 1;
					}
					else if(other_sub->r_id != host->rid)
					{
						if(other_sub->r_id == 0)
						{
							other_sub->r_id = host->rid; /*Update the router ID*/
							/*Add host to cur_router's adj_list*/
							int j;
							int exists = 0;
							for(j = 0; j < cur_router->rt->adj_size; j++)
							{
								if(cur_router->rt->adjacencies[j]->rid == host->rid)
								{
									exists = 1;
									break;
								}
							}
							if(exists == 0)
							{
								fprintf(stderr, "This should happen\n");
								fprintf(stderr, "Adding to adjacency list\n");
								cur_router->rt->adjacencies[cur_router->rt->adj_size] = host;
								cur_router->rt->adj_size++;
							}
						}
						else
						{
							fprintf(stderr, "Non-matching IDs - removed a subnet\n");
							struct in_addr subid;
							subid.s_addr = other_sub->r_id;
							struct in_addr hostid;
							hostid.s_addr = host->rid;
							fprintf(stderr, "other_sub->r_id = %s, ", inet_ntoa(subid));
							fprintf(stderr, "host->rid = %s\n", inet_ntoa(hostid));
							remove_rt_sn_using_id(sr, cur_router->rt, other_sub->r_id);
							//invalid = 1;
						}
					}
					else
					{
						
						int j;
						int exists;
						exists = 0;
						for(j = 0; j < host->adj_size; j++)
						{
							if(host->adjacencies[j]->rid == cur_router->rt->rid)
							{
								exists = 1;
								break;
							}
						}
						if(exists == 0)
						{
							host->adjacencies[host->adj_size] = cur_router->rt;
							host->adj_size++;
						}
						
						exists = 0;
						for(j = 0; j < cur_router->rt->adj_size; j++)
						{
							if(cur_router->rt->adjacencies[j]->rid == host->rid)
							{
								exists = 1;
								break;
							}
						}
						if(exists == 0)
						{
							cur_router->rt->adjacencies[cur_router->rt->adj_size] = host;
							cur_router->rt->adj_size++;
						}
						
						added = 1;
					}
				
				}
				else
				{
					fprintf(stderr, "Added new connection\n");
					fprintf(stderr, "Adding to adjacency list\n");
					cur_router->rt->adjacencies[cur_router->rt->adj_size] = host;
					cur_router->rt->adj_size++;
					host->adjacencies[host->adj_size] = cur_router->rt;
					host->adj_size++;
					
					cur_router->rt->subnets[cur_router->rt->subnet_size] = (struct route*)malloc(sizeof(struct route));
					memmove(cur_router->rt->subnets[cur_router->rt->subnet_size], current, sizeof(struct route));
					/*change neighbor ID to hosts (it was originally cur_routers)*/
					cur_router->rt->subnets[cur_router->rt->subnet_size]->r_id = host->rid;
					cur_router->rt->subnet_size++;
					added = 1;
				}
				
				host->subnets[host->subnet_size] = (struct route*)malloc(sizeof(struct route));
				memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
				host->subnet_size++;
				break;
				
			}
			else
			{
				cur_router = cur_router->next;
			}
		}
		if(!added)
		{
			/*create new router*/
			fprintf(stderr, "created new router\n");
			struct router *new_adj = add_new_router(sr, current->r_id);
			host->adjacencies[host->adj_size] = new_adj;
			host->adj_size++;
			struct route* opp_route = (struct route*) malloc(sizeof(struct route));
			memmove(opp_route, current, sizeof(struct route));
			opp_route->r_id = host->rid;
			add_new_route(sr, opp_route, new_adj);
		}
	}
	
	
	else
	{
			host->subnets[host->subnet_size] = (struct route*)malloc(sizeof(struct route));
			memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
			host->subnet_size++;
	}
}

/*NOT THREADSAFE*/
struct router* add_new_router(struct sr_instance *sr, uint32_t host_rid)
{
	fprintf(stderr,"In Adding a new router.\n");
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

/*NOT THREADSAFE*/
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

/*NOT THREADSAFE*/
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

/*NOT THREADSAFE*/
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

/*NOT THREADSAFE*/
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

/*NOT THREADSAFE*/
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



/*NOT THREADSAFE*/
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

/*THREADSAFE*/
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

/*NOT THREADSAFE*/
uint16_t get_sequence(uint32_t router_id, struct sr_instance *sr)
{
    pwospf_lock(sr->ospf_subsys);
    struct adj_list* net_walker=sr->ospf_subsys->network;
    while(net_walker)
    {
        if(net_walker->rt->rid==router_id)
        {
            pwospf_unlock(sr->ospf_subsys);
            return net_walker->rt->last_seq;
        }
        else
            net_walker=net_walker->next;
    }
    pwospf_unlock(sr->ospf_subsys);
    return 0;
}
/*NOT THREADSAFE*/
void set_sequence(uint32_t router_id, uint16_t sequence, struct sr_instance *sr)
{
   pwospf_lock(sr->ospf_subsys);
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
    pwospf_unlock(sr->ospf_subsys);
}

/*THREADSAFE*/
void print_subs(struct route** ads, int num_ads)
{
    fprintf(stderr, "----Routes----\n");
    int i=0;
    for(i=0; i< num_ads; i++)
    {
        struct in_addr sub =ads[i]->prefix;
        struct in_addr mask = ads[i]->mask;
        struct in_addr rid;
        rid.s_addr=ads[i]->r_id;
        fprintf(stderr, "Subnet: %s, ", inet_ntoa(sub)); 
        fprintf(stderr, "Mask: %s, ",inet_ntoa(mask));
        fprintf(stderr, "RID: %s ", inet_ntoa(rid));
        fprintf(stderr, "Next Hop: %s \n", inet_ntoa(ads[i]->next_hop));
    }
    fprintf(stderr, "\n");
}
