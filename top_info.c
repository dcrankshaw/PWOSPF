/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/29/2011
 **********************************************************************/

#define INIT_ADJLIST_SIZE	10
#define INIT_SUBNET_SIZE	10

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "sr_if.h"
#include "sr_protocol.h"
#include "sr_pwospf.h"
#include "sr_router.h"
#include "top_info.h"
#include "pwospf_protocol.h"

/*******************************************************************
*   Prints neighbor list from instance.
*******************************************************************/
void print_nbr_list(struct sr_instance *sr)
{
	struct pwospf_iflist* iface = sr->ospf_subsys->interfaces;
	int i;
	fprintf(stderr, "---NEIGHBOR LIST---\n");
	
	while(iface)
	{
		fprintf(stderr, "%s: ", iface->name);
		for(i = 0; i < iface->nbr_size; i++)
		{
			print_nbr(iface->neighbors[i]);
		}
		fprintf(stderr, "\n");
		iface = iface->next;
	}

}

/*******************************************************************
*  Prints a single neighbor.
*******************************************************************/
void print_nbr(struct neighbor_list* nbr)
{
	struct in_addr rid;
	rid.s_addr = nbr->id;
	fprintf(stderr, "%s,  ", inet_ntoa(rid));
}


/*******************************************************************
*   Checks the topology for any timed out entries. Deletes any expired. Called from 
*   pwospf_subsys thread.
*******************************************************************/
void check_top_invalid(struct sr_instance *sr)
{
	pwospf_lock(sr->ospf_subsys);
	struct adj_list *current = sr->ospf_subsys->network;
	time_t now = time(NULL);
	struct adj_list *prev = NULL;
	int changed = 0;
	while(current)
	{
		now = time(NULL);
		if((current->rt->expired <= now) && (current->rt->rid != sr->ospf_subsys->this_router->rid))
		{
			remove_from_topo(sr, current->rt);
			
			changed = 1;
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
			prev = current;
			current = current->next;
		}
	}
	
	if(changed == 1)
	{
		dijkstra(sr, sr->ospf_subsys->this_router);
		update_ftable(sr);
	}
	
	pwospf_unlock(sr->ospf_subsys);
}

/*******************************************************************
*   Removes a given subnet from a given router. Moves last entry in subnets array into deleted space.
*******************************************************************/
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

/*******************************************************************
*  Removes a subnet from a router based on the router id.
*******************************************************************/
void remove_rt_sn_using_id(struct sr_instance *sr, struct router *rt, uint32_t id)
{
	/*decrease size, remove appropriate subnet, move last subnet into empty spot in the array
	this keeps all entries packed*/
	int i;
	for(i = 0; i < rt->subnet_size; i++)
	{
		if(rt->subnets[i]->r_id == id)
		{
			free(rt->subnets[i]);
			rt->subnets[i] = NULL;
			rt->subnets[i] = rt->subnets[rt->subnet_size-1];
			rt->subnets[rt->subnet_size-1] = NULL;
			rt->subnet_size--;
			break;
		}
	}
}

/*******************************************************************
*  Compares two subnets based on their prefix, mask, and rid.
*******************************************************************/
int route_cmp(struct route* r1, struct route* r2)
{
	if((r1->prefix.s_addr == r2->prefix.s_addr) && (r1->mask.s_addr == r2->mask.s_addr) && (r1->r_id == r2->r_id))
	{
		return 0;
	}
	return -1;
}

/*******************************************************************
*   Removes a router from the topology.
*
*   Also removes router from other routers' adjacency lists. Removes any subnets from other 
*   routers that are connected to router that is being deleted.
*******************************************************************/
int remove_from_topo(struct sr_instance *sr, struct router *rt)
{
    /*LOCKED IN CHECK_TOP_INVALID*/
   struct in_addr rid;
   rid.s_addr = rt->rid;
   
   int i;
	/*free all allocated memory storing this router's routes*/
	for(i = 0; i < rt->subnet_size; i++)
	{
		free(rt->subnets[i]);
	}
	rt->subnet_size = 0;
	
	/*for(i = 0; i < rt->adj_size; i++)*/
	struct adj_list* net_walker=sr->ospf_subsys->network;
	while(net_walker)
	{
		/*Remove the connecting route from all other routers*/
		remove_rt_sn_using_id(sr, net_walker->rt, rt->rid);

		/*set to null the pointer to us in all other routers adjacency lists*/
		remove_rt_adj_using_id(sr, net_walker->rt, rt->rid);
		net_walker = net_walker->next;
	}
	free(rt->subnets);
	free(rt->adjacencies);
	free(rt);
	return 1;
}

/*******************************************************************
*  Removes an adjacency from the host router by a router's id.
*******************************************************************/
void remove_rt_adj_using_id(struct sr_instance *sr, struct router* host, uint32_t id)
{
	int i;
	for(i = 0; i < host->adj_size; i++)
	{
		/*If we found matching adjacency*/
		if(host->adjacencies[i]->rid == id)
		{
			host->adjacencies[i] = NULL; /*make this pointer null in case it is the last item in the list*/
			host->adjacencies[i] = host->adjacencies[host->adj_size - 1]; /*repack list (move last item to empty spot*/
			host->adjacencies[host->adj_size - 1] = NULL; /*set last item NULL*/
			host->adj_size--;
			break; /*Done, so we don't need to keep stepping through*/
		}
	}
}

/*******************************************************************
*  Prints topology
*******************************************************************/
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

/*******************************************************************
*  Prints a single router in the topology.
*******************************************************************/
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
	
	struct in_addr rid;
	fprintf(stderr, "\nSubnets:");
	for(i = 0; i < rt->subnet_size; i++)
	{
		fprintf(stderr, "Prefix: %s, ", inet_ntoa(rt->subnets[i]->prefix));
		fprintf(stderr, "Mask: %s, ", inet_ntoa(rt->subnets[i]->mask));
		rid.s_addr=rt->subnets[i]->r_id;
		fprintf(stderr, "RID: %s\n ", inet_ntoa(rid));
	}

}


/*******************************************************************
*   Called when a LSU is received. Adds and updates the topology as necessary.
*******************************************************************/
int add_to_top(struct sr_instance* sr, uint32_t host_rid, struct route** advert_routes,
				int num_ads)
{
	pwospf_lock(sr->ospf_subsys);
	struct in_addr rid;
	rid.s_addr=host_rid;
    fprintf(stderr, "Printing Topo before add_to_top------LSU Received from %s\n", inet_ntoa(rid));
    print_topo(sr);
	struct router* host = adj_list_contains(sr, host_rid);
	if(host != NULL)
	{
		add_to_existing_router(sr, advert_routes, host, num_ads);	
	}
	else
	{
		host = add_new_router(sr, host_rid);
		add_to_existing_router(sr, advert_routes, host, num_ads);
	}
	
	/*TODO: check host's subnets. If any don't have a corresponding advertisement, delete.*/
	
	if(time(NULL) > sr->ospf_subsys->init_time)
	{
	    
        int i;
        for(i=0; i< host->subnet_size; i++)
        {
            if(sub_in_adv(sr, advert_routes, host->subnets[i], num_ads)==0)
            {
                struct router* nbor = adj_list_contains(sr, host->subnets[i]->r_id);
                remove_rt_sn_using_id(sr, host, host->subnets[i]->r_id);
                
                if(nbor!=NULL)
                {
                    remove_rt_sn_using_id(sr, nbor, host_rid);
                    remove_rt_adj_using_id(sr, nbor, host_rid);
                    remove_rt_adj_using_id(sr, host, nbor->rid);
                }
                i--;
            }
        }
    }
	fprintf(stderr, "Printing Topo after add_to_top\n");
    print_topo(sr);
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	
	pwospf_unlock(sr->ospf_subsys);
	return 1;
}

/***********************************************************
*   Checks that the array of advertisements contains a specific subnet
**************************************************************/
int sub_in_adv(struct sr_instance* sr, struct route** advs, struct route* route, int num_ads)
{
    int i;
    for(i=0; i< num_ads; i++)
    {
        if(route_cmp(route, advs[i])==0)
        {
            return 1;
        }
    }
    return 0;
}
/*******************************************************************
*   Prints the forwarding table.
*******************************************************************/
void print_ftable(struct sr_instance *sr)
{
	struct ftable_entry* current = sr->ospf_subsys->fwrd_table;
	
	fprintf(stderr, "--- FORWARDING TABLE ---\n");
	while(current)
	{
		fprintf(stderr, "Prefix: %s   ", inet_ntoa(current->prefix));
		fprintf(stderr, "Mask: %s   ", inet_ntoa(current->mask));
		fprintf(stderr, "Next Hop: %s   ", inet_ntoa(current->next_hop));
		fprintf(stderr, "Interface: %s\n", current->interface);
		current = current->next;
	}
	fprintf(stderr, "\n");
}

/*******************************************************************
*  Checks if the topology contains a router based on its RID. If it does, returns the router. If
*   it doesn't, return NULL.
*******************************************************************/
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

/*******************************************************************
*  Adds subnets received in a LSU from the host router.
*   
*   As necessary for all subnets:
*       Adds subnet to host
*       Adds the reverse subnet to router identified by the rid in the subnet
*       Adds router identified by the rid in the subnet to host's adjacencies
*       Adds host to the router identified by the rid in the subnet's adjacencies
*******************************************************************/
void add_to_existing_router(struct sr_instance *sr, struct route **routes, struct router* host, int num_ads)
{
    /* Updates host's time until expired*/
    host->expired = time(NULL) + (3 * OSPF_DEFAULT_LSUINT);
    
    int i;
    for(i = 0; i < num_ads; i++)
    {
        if(router_contains(routes[i], host) == 0)
        {
            /*Adds route to host's subnets */
            struct route* old_sub = router_contains_subnet(host, routes[i]->prefix.s_addr);
            if(old_sub)
            {
                if((routes[i]->mask.s_addr != old_sub-> mask.s_addr) && ((routes[i]->r_id ==0) || (old_sub->r_id==0)))
                {
                    add_new_route(sr, routes[i], host);
                }
                else
                {
                    if(old_sub->r_id == 0)
                    {
                        old_sub->r_id = routes[i]->r_id;
                    }
                    else
                    {
                        remove_subnet_from_router(sr, host, old_sub);
                        continue;
                    }
                }
            }
            else
            {
                add_new_route(sr, routes[i], host);
            }
        }	
        
        /*Check if con_router already exists in topology*/
        if(routes[i]->r_id != 0)
        {
        
            struct router* con_router=adj_list_contains(sr, routes[i]->r_id);
            if(con_router==NULL)
            {
                con_router = add_new_router(sr, routes[i]->r_id);
            }
            
            /*If host doesn't already contain con_router in host's adjacencies*/
            if(router_has_adjacency(host, con_router) == 0)
            {
                if(host->adj_buf_size == host->adj_size)
                {
                    host->adjacencies = realloc(host->adjacencies, 2*host->adj_buf_size);
                    host->adj_buf_size *= 2;
                }
                host->adjacencies[host->adj_size]=con_router;
                host->adj_size++;
            }
            
            /*If con_router doesn't alread contain host as an adjacency */
            if(router_has_adjacency(con_router, host) == 0)
            {
                if(con_router->adj_buf_size == con_router->adj_size)
                {
                    con_router->adjacencies = realloc(con_router->adjacencies, 2*con_router->adj_buf_size);
                    con_router->adj_buf_size *= 2;
                }
                con_router->adjacencies[con_router->adj_size]=host;
                con_router->adj_size++;
            }
            
            struct route* opp_route=(struct route*)malloc(sizeof(struct route));
            memmove(opp_route, routes[i], sizeof(struct route));
            opp_route->r_id=host->rid;
            
            /*If con_router doesn't already contain the opposite route in its subnets*/
            if(router_contains(opp_route, con_router) == 0)
            {
                struct route* old_sub = router_contains_subnet(con_router, opp_route->prefix.s_addr);
                if(old_sub)
                {
                    if(old_sub->r_id == 0)
                    {
                        old_sub->r_id = opp_route->r_id;
                    }
                    else
                    {
                        remove_subnet_from_router(sr, con_router, opp_route);
                        continue;
                    }
                }
                else
                {
                     add_new_route(sr, opp_route, con_router);
                }
            }
            free(opp_route);

         }			
    }
		
}

/*******************************************************************
*  Checks if con_router is in host's adjacency list
*******************************************************************/
int router_has_adjacency(struct router* host, struct router* con_router)
{
    int i=0;
    for(i=0; i< host->adj_size; i++)
    {
        if(host->adjacencies[i]->rid==con_router->rid)
        {
            return 1;
        }
    }
    return 0;
}

/*******************************************************************
*   Checks if a router contains a subnet based on prefix, mask, and rid.
*******************************************************************/
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

/*******************************************************************
*  Checks if a router contains a subnet based solely on prefix.
*******************************************************************/
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

/*******************************************************************
*  Adds a new subnet, current, to a router, host.
*******************************************************************/
void add_new_route(struct sr_instance *sr, struct route* current, struct router* host)
{
    if(host->subnet_buf_size == host->subnet_size)
    {
        host->subnets = realloc(host->subnets, 2*host->subnet_buf_size);
        host->subnet_buf_size *= 2;
    } 
    
    host->subnets[host->subnet_size]=(struct route*)malloc(sizeof(struct route));
    memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
    host->subnet_size++;
}

/*******************************************************************
*  Adds a new router to the topology based on its RID.
*******************************************************************/
struct router* add_new_router(struct sr_instance *sr, uint32_t host_rid)
{
    struct in_addr rid;
	rid.s_addr=host_rid;
	
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
		/*Initialize array of pointers to 0s*/
		new_router->adjacencies = (struct router **) calloc(INIT_ADJLIST_SIZE, sizeof(struct router*));
		new_router->adj_buf_size = INIT_ADJLIST_SIZE;
		new_router->adj_size = 0;
		
		new_router->subnets = (struct route **) calloc(INIT_SUBNET_SIZE, sizeof(struct route*));
		new_router->subnet_buf_size = INIT_SUBNET_SIZE;
		new_router->subnet_size = 0;
		
		new_router->last_seq = 0;
		new_router->rid = host_rid;
		new_router->expired = time(NULL) + (3 * OSPF_DEFAULT_LSUINT);
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

/*******************************************************************
*   Gets the neighbor and interface for a given subnet.
*******************************************************************/
int get_if_and_neighbor(struct pwospf_iflist *ifret, struct neighbor_list *nbrret,
						struct sr_instance *sr, uint32_t id)
{
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	
	while(cur_if)
	{
	    int i;
		for(i = 0; i < cur_if->nbr_size; i++)
		{
			if(cur_if->neighbors[i]->id == id)
			{
				memmove(ifret, cur_if, sizeof(struct pwospf_iflist));
				memmove(nbrret, cur_if->neighbors[i], sizeof(struct neighbor_list));
				return 1;
			}
		
		}
		cur_if = cur_if->next;
	}
	return 0;
}

/*******************************************************************
*   Gets the interface associated with a given subnet.
*******************************************************************/
struct pwospf_iflist* get_subnet_if(struct sr_instance *sr, struct route* subnet)
{
	assert(subnet);
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	while(cur_if)
	{
		if((cur_if->mask.s_addr & cur_if->address.s_addr) == subnet->prefix.s_addr)
		{
			return cur_if;
		}
		else
		{
			cur_if = cur_if->next;
		}
	}
	return NULL;
}

/*******************************************************************
*  Updates the forwarding table based on the recomputed dijkstra's information.
*******************************************************************/
int update_ftable(struct sr_instance *sr)
{
	reset_ftable(sr);
	assert((!sr->ospf_subsys->fwrd_table));
	struct adj_list *current_dest_rt = sr->ospf_subsys->network;
	while(current_dest_rt)
	{
		int numhops = -1;
		
		struct router* next_hop = find_next_hop(sr, current_dest_rt->rt, &numhops);
		
		if(next_hop == NULL) /*Indicates current_dest_rt is this_router*/
		{
			if(numhops == 0)
			{
				struct ftable_entry* cur_ft_entry = NULL;
				int i;
				for(i = 0; i < sr->ospf_subsys->this_router->subnet_size; i++)
				{
					int replaced = 0;
					if(sr->ospf_subsys->fwrd_table == 0)
					{
						sr->ospf_subsys->fwrd_table = (struct ftable_entry*)calloc(1, sizeof(struct ftable_entry));
						cur_ft_entry = sr->ospf_subsys->fwrd_table;
					}
					else
					{
						struct ftable_entry *cur_entry = ftable_contains(sr, current_dest_rt->rt->subnets[i]->prefix,
										current_dest_rt->rt->subnets[i]->mask);
						if(cur_entry)
						{
							assert((numhops == 0));
							cur_entry->next_hop.s_addr = sr->ospf_subsys->this_router->subnets[i]->next_hop.s_addr;
							struct pwospf_iflist* iface = get_subnet_if(sr, sr->ospf_subsys->this_router->subnets[i]);
							assert(iface);
							memmove(cur_entry->interface, iface->name, sr_IFACE_NAMELEN);
							cur_entry->num_hops = 0;
							replaced = 1;
							
						}
						else
						{
							cur_ft_entry = sr->ospf_subsys->fwrd_table;
							while(cur_ft_entry->next)
							{
								cur_ft_entry = cur_ft_entry->next;
							}
							cur_ft_entry->next = (struct ftable_entry*)calloc(1, sizeof(struct ftable_entry));
							cur_ft_entry = cur_ft_entry->next;
						}
						
						
					}
					if(!replaced)
					{
						cur_ft_entry->prefix = sr->ospf_subsys->this_router->subnets[i]->prefix;
						cur_ft_entry->mask = sr->ospf_subsys->this_router->subnets[i]->mask;
						cur_ft_entry->next_hop = sr->ospf_subsys->this_router->subnets[i]->next_hop;
						cur_ft_entry->num_hops = 0;
						cur_ft_entry->next = 0;
						struct pwospf_iflist* iface = get_subnet_if(sr, sr->ospf_subsys->this_router->subnets[i]);
						assert(iface);
						memmove(cur_ft_entry->interface, iface->name, sr_IFACE_NAMELEN);
					}
				}
				
			}
			else
			{
				return 0;
			}
		}
		else
		{
			
			struct pwospf_iflist *iface = (struct pwospf_iflist *)calloc(1, sizeof(struct pwospf_iflist));
			struct neighbor_list *nbr = (struct neighbor_list *)calloc(1, sizeof(struct neighbor_list));
			get_if_and_neighbor(iface, nbr, sr, next_hop->rid);
			if(iface == NULL || nbr == NULL)
			{
				return 0;
			}
			int i;
			for(i = 0; i < current_dest_rt->rt->subnet_size; i++)
			{
				struct ftable_entry *cur_entry = ftable_contains(sr, current_dest_rt->rt->subnets[i]->prefix,
						current_dest_rt->rt->subnets[i]->mask);
				
				if(cur_entry == NULL)
				{
					/*create entry*/
					cur_entry = (struct ftable_entry *)calloc(1, sizeof(struct ftable_entry));
					cur_entry->prefix = current_dest_rt->rt->subnets[i]->prefix;
					cur_entry->mask = current_dest_rt->rt->subnets[i]->mask;
					
					assert((numhops != 0));
					cur_entry->next_hop.s_addr = nbr->ip_address.s_addr;
					memmove(cur_entry->interface, iface->name, sr_IFACE_NAMELEN);
					cur_entry->num_hops = numhops;
					cur_entry->next = NULL;
					
					/*add to back of ftable*/
					
					if(sr->ospf_subsys->fwrd_table == NULL)
					{
						sr->ospf_subsys->fwrd_table = cur_entry;
					}
					else
					{
						struct ftable_entry *walker = sr->ospf_subsys->fwrd_table;
						while(walker->next)
						{
							walker = walker->next;
						}
						walker->next = cur_entry;
						cur_entry->next = NULL;
					}
				}
				else if(cur_entry->num_hops > numhops)
				{
					
					assert((numhops != 0));
					cur_entry->next_hop.s_addr = nbr->ip_address.s_addr;
					memmove(cur_entry->interface, iface->name, sr_IFACE_NAMELEN);
					cur_entry->num_hops = numhops;
				}
			}
			if(iface)
			{
				free(iface);
				iface = NULL;
			}
			if(nbr)
			{
				free(nbr);
				nbr = NULL;
			}
		}
		current_dest_rt = current_dest_rt->next;
	}
	return 1;
}

/*******************************************************************
*  Finds the next hop to ultimately get to the router, dest. Also records the number of hops to get 
*   to the destination.
*******************************************************************/
struct router* find_next_hop(struct sr_instance *sr, struct router *dest, int *hops)
{
	int numhops = 0;
	struct router *cur = dest;
	if(dest->rid == sr->ospf_subsys->this_router->rid)
	{
		*hops = 0;
		return NULL;
	}
	numhops = 1;
	while((cur->prev != NULL) && (cur->prev->rid != sr->ospf_subsys->this_router->rid))
	{
		numhops++;
		cur = cur->prev;
	}
	if(cur->prev == NULL)
	{	
		*hops = -1;
		return NULL;
	}
	*hops = numhops;
	return cur;
}

/*******************************************************************
*  Checks if the forwarding table contains an entry based on a prefix and mask
*******************************************************************/
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

/*******************************************************************
*  Resets the forwarding table
*******************************************************************/
void reset_ftable(struct sr_instance *sr)
{
	struct ftable_entry *current = sr->ospf_subsys->fwrd_table;
	
	struct ftable_entry *prev = NULL;
	while(current)
	{
		prev = current;
		current = current->next;
		free(prev);
		prev = NULL;
	}
	sr->ospf_subsys->fwrd_table = NULL;
}

/*******************************************************************
*  Computes the dijkstra's algorithm on the topology.
*******************************************************************/
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
	struct in_addr thisid;
	thisid.s_addr = sr->ospf_subsys->this_router->rid;
	struct router* least_unknown = get_smallest_unknown(sr->ospf_subsys->network);
	while(least_unknown != NULL)
	{
		struct in_addr lu_id;
		lu_id.s_addr = least_unknown->rid;
		
		least_unknown->known = 1; /*mark it as visited*/
		int i;
		
		for(i = 0; i < least_unknown->adj_size; i++)
		{
			struct router *w;
			w = least_unknown->adjacencies[i];
			struct in_addr w_id;
			w_id.s_addr = w->rid;
			if(w->known == 0)
			{
				
				if(((least_unknown->dist + 1) < w->dist) || (w->dist < 0))
				{
					w->dist = least_unknown->dist + 1; /* update the cost */
					w->prev = least_unknown;
					w_id.s_addr = w->rid;
					struct in_addr prev_id;
					prev_id.s_addr = w->prev->rid;
				}
			}
		}
		least_unknown = get_smallest_unknown(sr->ospf_subsys->network);
	}
}

/*******************************************************************
*  Finds closest unknown router.
*******************************************************************/
struct router* get_smallest_unknown(struct adj_list *current)
{
	struct router* least_unknown = NULL;
	int min_dist = -1;
	while(current != NULL)
	{
		if(current->rt->known == 1)
		{
			current = current->next;
		}
		else if(current->rt->dist < 0)
		{
			current = current->next;
		}
		else if(min_dist > current->rt->dist)
		{
			min_dist = current->rt->dist;
			least_unknown = current->rt;
		}
		else if(min_dist < 0)
		{
			min_dist = current->rt->dist;
			least_unknown = current->rt;
		}
		else
		{
			current = current->next;
		}
	}
	return least_unknown;
}

/*******************************************************************
*  Gets the sequence of the last LSU sent by a router.
*******************************************************************/
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

/*******************************************************************
*   Sets the sequence of a router based on the last sequence received.
*******************************************************************/
void set_sequence(uint32_t router_id, uint16_t sequence, struct sr_instance *sr)
{
   pwospf_lock(sr->ospf_subsys);
   struct adj_list* net_walker=sr->ospf_subsys->network;
	while(net_walker)
    {
        if(net_walker->rt->rid==router_id)
        {
            net_walker->rt->last_seq=sequence;
            break;
        }
        else
            net_walker=net_walker->next;
    }
    pwospf_unlock(sr->ospf_subsys);
}

/*******************************************************************
*  Prints all of the routes in an array of pointers to routes.
*******************************************************************/
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
