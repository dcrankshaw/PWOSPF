/* jkashfkdshfkj */


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

void print_nbr(struct neighbor_list* nbr)
{
	struct in_addr rid;
	rid.s_addr = nbr->id;
	fprintf(stderr, "%s,  ", inet_ntoa(rid));
}



/*this get's called from the OSPF_SUBSYSTEM thread*/
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
			fprintf(stderr, "Expired time: %lu, current time: %lu\n", (long) current->rt->expired, (long) now);
			fprintf(stderr, "TOPO before removing router\n");
			print_topo(sr);
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
			fprintf(stderr, "TOPO after removing router\n");
			print_topo(sr);
		}
		else
		{
			prev = current;
			current = current->next;
		}
	}
	
	
	/*TODO comment this out and put back the if statement*/
	/*dijkstra(sr, sr->ospf_subsys->this_router);
	fprintf(stderr, "In check_top - Ftable BEFORE updating:\n");
	print_ftable(sr);
	update_ftable(sr);
	fprintf(stderr, "In check_top - Ftable AFTER updating:\n");
	print_ftable(sr);*/

	
	
	if(changed == 1)
	{
		dijkstra(sr, sr->ospf_subsys->this_router);
		update_ftable(sr);
	}
	
	pwospf_unlock(sr->ospf_subsys);
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
    /*LOCKED IN CHECK_TOP_INVALID*/
   struct in_addr rid;
   rid.s_addr = rt->rid;
   fprintf(stderr, "Removing %s from topo\n", inet_ntoa(rid));
   
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


/* called when LSU packets are received. Adds to the adjacency list */
/*THREADSAFE*/
int add_to_top(struct sr_instance* sr, uint32_t host_rid, struct route** advert_routes,
				int num_ads)
{
	pwospf_lock(sr->ospf_subsys);

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
	
	dijkstra(sr, sr->ospf_subsys->this_router);
	update_ftable(sr);
	
	
	fprintf(stderr, "Printing Topology after calling add_to_top\n");
	print_topo(sr);
	
	pwospf_unlock(sr->ospf_subsys);
	return 1;
}

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
		host->expired = time(NULL) + (3 * OSPF_DEFAULT_LSUINT);
		
		int i;
		for(i = 0; i < num_ads; i++)
		{
		
			/*If this does not already exist in the host's subnets*/
			if(router_contains(routes[i], host) == 0)
			{
				/*Adds route to host's subnets */
				struct route* old_sub = router_contains_subnet(host, routes[i]->prefix.s_addr);
				if(old_sub)
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
				else
				{
					add_new_route(sr, routes[i], host);
				}
				
			}	
				/*If the router contains subnets that aren't advertised any more, we need to delete those subnets and adjacencies*/
			
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
					/*Adds route to host's subnets */
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
			/*else
			{
			    int exists=0;
			    int i=0;
			    for(i=0; i<host->adj_size; i++)
			    {
			        if(host->adjacencies[i]->rid == routes[i]->r_id)
			        {
			            exists=1;
			            break;
			        }
			
			    }
			    if((routes[i]->r_id != 0) && (!exists))
			    {
			        fprintf(stderr, "GOT HERE NOW!!!!\n\n");
			        
			        struct router* con_router = adj_list_contains(sr, routes[i]->r_id);
			        if(con_router!=NULL)
			        {
			            if(host->adj_buf_size == host->adj_size)
                        {
                            host->adjacencies = realloc(host->adjacencies, 2*host->adj_buf_size);
                            host->adj_buf_size *= 2;
                        }
                        
                        host->adjacencies[host->adj_size]=con_router;
                        host->adj_size++;
                        
                        struct route* opp_route = (struct route*) malloc(sizeof(struct route));
                        memmove(opp_route, routes[i], sizeof(struct route));
                        opp_route->r_id = host->rid;
                        
                        free(opp_route);
                        
                        if(router_contains(opp_route, con_router) == 0)
                        {
                            if(con_router->subnet_buf_size == con_router->subnet_size)
                            {
                                con_router->subnets = realloc(con_router->subnets, 2*con_router->subnet_buf_size);
                                con_router->subnet_buf_size *= 2;
                            } 
                            
                            con_router->subnets[con_router->subnet_size]=opp_route;
                            con_router->subnet_size++;
                        }
                        
                        int has_adj=0;
                        int j;
                        for(j=0; j<con_router->adj_size; j++)
                        {
                            if(con_router->adjacencies[j]->rid==host->rid)
                            {
                                has_adj=1;
                                break;
                            }
                        }
                        
                        if(!has_adj)
                        {
                            if(con_router->adj_buf_size == con_router->adj_size)
                            {
                                con_router->adjacencies = realloc(con_router->adjacencies, 2*con_router->adj_buf_size);
                                con_router->adj_buf_size *= 2;
                            }
                            
                            con_router->adjacencies[con_router->adj_size]=host;
                            con_router->adj_size++;      
                        }
                        
			        }
			        else
			        {
			            struct router* con_router= add_new_router(sr, routes[i]->r_id);
			            
			            struct route* opp_route = (struct route*) malloc(sizeof(struct route));
                        memmove(opp_route, routes[i], sizeof(struct route));
                        opp_route->r_id = host->rid;
			            con_router->subnets[con_router->subnet_size]=opp_route;
                        con_router->subnet_size++;
                        free(opp_route);
                        
                        con_router->adjacencies[con_router->adj_size]=host;
                        con_router->adj_size++; 
                        
                        if(host->adj_buf_size == host->adj_size)
                        {
                            host->adjacencies = realloc(host->adjacencies, 2*host->adj_buf_size);
                            host->adj_buf_size *= 2;
                        }
                        host->adjacencies[host->adj_size]=con_router;
                        host->adj_size++;
			        }
			        
			        
			        /* 
			            If network contains route[i]->rid
			            {
			                Check Host's adj size against buffer size
			                Host adds connecting_rter to its adjacency list
			                If connecting_rter !contain opp_route
			                {
			                    Check connecting_rter's sub size vs buffer size
			                    add opp_route to connecting_rter subnet
			                }
			                If connecting_rter !contain host in adj list
			                {
			                    Check connecting_rter's adj size against buffer size
			                    add host to connecting_rter's adj_list
			                }
			            }
			            else
			            {
			                create connecting_rter
			                add host to connecting_rter's adj_list\
			                add opp_route to subnet
			                add connecting_rter to host's adj list
			            }
			       
			       
			       
                    
                    
                    
			    }*/
		}
		
}

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
    if(host->subnet_buf_size == host->subnet_size)
    {
        host->subnets = realloc(host->subnets, 2*host->subnet_buf_size);
        host->subnet_buf_size *= 2;
    } 
    
    host->subnets[host->subnet_size]=(struct route*)malloc(sizeof(struct route));
    host->subnets[host->subnet_size]=current;
    host->subnet_size++;
}

/*NOT THREADSAFE*/
struct router* add_new_router(struct sr_instance *sr, uint32_t host_rid)
{
    struct in_addr rid;
	rid.s_addr=host_rid;
	fprintf(stderr, "--------Adding a new Router to the topology: %s\n", inet_ntoa(rid));
	
	//fprintf(stderr,"In Adding a new router.\n");
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
		
		/*TODO:will need to initialize this to the value given in the LSU*/
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

/*NOT THREADSAFE*/
int get_if_and_neighbor(struct pwospf_iflist *ifret, struct neighbor_list *nbrret,
						struct sr_instance *sr, uint32_t id)
{
	struct in_addr nh;
	nh.s_addr = id;
	//fprintf(stderr, "Next hop ID: %s\n", inet_ntoa(nh));
	
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	/*while(cur_if)
	{
		fprintf(stderr, "Interface %s exists\n", cur_if->name);
		cur_if = cur_if->next;
	}
	cur_if = sr->ospf_subsys->interfaces;*/
	while(cur_if)
	{
		//struct neighbor_list *cur_nbr = cur_if->neighbors;
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


struct pwospf_iflist* get_subnet_if(struct sr_instance *sr, struct route* subnet)
{
	//fprintf(stderr, "Got here\n");
	assert(subnet);
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	while(cur_if)
	{
		if((cur_if->mask.s_addr & cur_if->address.s_addr) == subnet->prefix.s_addr)
		{
			//fprintf(stderr, "Found matching interface\n");
			return cur_if;
		}
		else
		{
			cur_if = cur_if->next;
		}
	}
	//fprintf(stderr, "Got thru while loop\n");
	return NULL;
}

/*NOT THREADSAFE*/
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
				//fprintf(stderr, "Subnet Size: %d\n\n", sr->ospf_subsys->this_router->subnet_size);
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
				printf("Error -- Next hop equals NULL but numhops != 0");
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
				
				assert(iface);
				assert(nbr);
				printf("Error - Next hop doesn't match any neighbors");
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

/*NOT THREADSAFE*/
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


/*NOT THREADSAFE*/
void dijkstra(struct sr_instance* sr, struct router *host)
{
	//fprintf(stderr, "Starting Dijkstra's\n");
	
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
	//fprintf(stderr, "This router: %s, ", inet_ntoa(thisid));
	//sr->ospf_subsys->this_router->known = 1;
	struct router* least_unknown = get_smallest_unknown(sr->ospf_subsys->network);
	while(least_unknown != NULL)
	{
		struct in_addr lu_id;
		lu_id.s_addr = least_unknown->rid;
		/*fprintf(stderr, "Least unknown: %s ", inet_ntoa(lu_id));
		fprintf(stderr, "has %d adjacencies\n", least_unknown->adj_size);*/
		
		least_unknown->known = 1; /*mark it as visited*/
		int i;
		
		for(i = 0; i < least_unknown->adj_size; i++)
		{
			struct router *w;
			w = least_unknown->adjacencies[i];
			struct in_addr w_id;
			w_id.s_addr = w->rid;
			//fprintf(stderr, "Before while loop, w: %s\n", inet_ntoa(w_id));
			if(w->known == 0)
			{
				/*fprintf(stderr, "w->known == 0\n");
				fprintf(stderr, "%d\n", least_unknown->dist + 1);
				fprintf(stderr, "w->dist = %d\n", w->dist);*/
				
				
				if(((least_unknown->dist + 1) < w->dist) || (w->dist < 0))
				{
					//fprintf(stderr, "YAY, entered the right loop!!!!!!!!!!!!!!!!!!!!!!\n");
					w->dist = least_unknown->dist + 1; /* update the cost */
					w->prev = least_unknown;
					w_id.s_addr = w->rid;
					//fprintf(stderr, "w: %s, ", inet_ntoa(w_id));
					struct in_addr prev_id;
					prev_id.s_addr = w->prev->rid;
					//fprintf(stderr, "prev: %s\n", inet_ntoa(prev_id));
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

/*NOT THREADSAFE*/
uint16_t get_sequence(uint32_t router_id, struct sr_instance *sr)
{
    //fprintf(stderr, "Locking in get_sequence()\n");
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
   //fprintf(stderr, "Locking in set_sequence()\n");
   pwospf_lock(sr->ospf_subsys);
   struct adj_list* net_walker=sr->ospf_subsys->network;
	while(net_walker)
    {
        //fprintf(stderr, "In while loop\n");
        if(net_walker->rt->rid==router_id)
        {
            net_walker->rt->last_seq=sequence;
            break;
        }
        else
            net_walker=net_walker->next;
    }
    pwospf_unlock(sr->ospf_subsys);
    //fprintf(stderr, "Unlocked in set_sequence()\n");
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
