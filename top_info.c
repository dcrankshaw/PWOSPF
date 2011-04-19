/* jkashfkdshfkj */


#define INIT_ADJLIST_SIZE	10
#define INIT_SUBNET_SIZE	10

int remove_neighbor()
{
	/*
	* -use if a neighbor gets removed from the interface list
	* -will need to make sure that the neighbor gets removed from the topology as well??
	*/
	
}




void add_neighbor(struct sr_instance* sr, char *name, uint32_t router_id, struct in_addr ip)
{
	struct pwospf_iflist *current_if = sr->pwospf_subsys->neighbors;
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


}

/*checks whether there are any expired entries in the topoology*/
void check_top_invalid(struct sr_instance *sr)
{
	struct adj_list *current = sr->pwospf_subsys->network;
	time_t now = time(NULL);
	struct adj_list *prev = NULL;
	while(current)
	{
		if(current->rt->expired <= now)
		{
			remove_from_topo(current->rt);
			if(prev == NULL)
			{
				sr->pwospf_subsys->network = current->next;
				free(current);
				current = sr->pwospf_subsys->network;
			}
			else if(current->next)
			{
				prev->next = curent->next;
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
}

int route_cmp(struct route* r1, struct route* r2)
{
	if((r1->prefix == r2->prefix) && (r1->mask == r2->mask) && (r1->r_id == r2->r_id))
	{
		return 0;
	}
	return -1;
}

/*TODO*/
int remove_from_topo(struct router *rt)
{
	int i;
	/*free all allocated memory storing this router's routes*/
	for(i = 0; i < rt->subnet_size; i++)
	{
		free(rt->subnets[i]);
	}
	/*look at all adjacencies, delete all pointers to this router, then free the router,
		DON'T FORGET TO DELETE ROUTES FROM SUBNETS IN THE ROUTERS TOO*/	
		
	dijkstra(sr, sr->pwospf_subsys->this_router);
	update_ftable(sr);	
	
	
}


/* called when LSU packets are received. Adds to the adjacency list */

/*TODO: What if we get an LSU that advertises a link connecting to a router we don't
know about?*/
int add_to_top(struct sr_instance* sr, uint32_t host_rid, struct route** advert_routes,
				int num_ads)
{
	struct router* host = adj_list_contains(host_rid);
	if(host != NULL)
	{
		add_to_existing_router(host);	
	}
	else
	{
		host = add_new_router(routes, host_rid);
		if(host != NULL)
		{
			add_to_existing_router(host);
		}
		else
		{
			/*ERROR*/
		}
		
	
	}
	/*recompute dijkstra's algorithm*/
	
	dijkstra(sr, sr->pwospf_subsys->this_router);
	update_ftable(sr);
	

}

void add_to_existing_router(struct route **routes, struct router* host)
{
	/*TODO: this is pretty inefficient, it may be alright though if these stay small enough */
		int i;
		for(i = 0; i < num_ads; i++)
		{
			if(!router_contains(routes[i], host))
			{
				add_new_route(routes[i], host);
			}
		}
}

void add_new_route(struct route* current, struct router* host)
{
	/*Need to resize subnet buffer*/
	if(host->subnet_buf_size == host->subnet_size)
	{
		host->subnets = realloc(host->subnets, 2*host->subnet_buf_size); //double size of array
		host->subnet_buf_size *= 2;
	
	}
	/*add to list of subnets*/
	memmove(host->subnets[host->subnet_size], current, sizeof(struct route));
	host->subnet_size++;
	
	/*add to adj_list*/
	if(current->r_id != 0)
	{
		if(host->adj_buf_size == host->adj_size)
		{
			host->adjacencies = realloc(host->adjacencies, 2*host->adj_buf_size);
			host->adj_buf_size *= 2;
		
		}
		/*get router pointer to add to adj list*/
		struct adj_list *cur_router = sr->pwospf_subsys->network;
		int added = 0;
		while(cur_router)
		{
			if(cur_router->rt->rid == current->r_id)
			{
				host->adjacencies[host->adj_size] = cur_router;
				host->adj_size++;
				added = 1;
				break;
			}
			else
			{
				cur_router = cur_router->next;
			}
		}
		if(!added)
		{
			/* ?????????????????????????????????????????????????
			TODO: BUFFERING MAYBE?
			???????????????????????????????????????????????????*/
		
		}
	}
}

struct router* add_new_router(struct sr_instance *sr, struct route **routes, uint32_t host_rid)
{
	struct router* new_router = (struct router*)malloc(sizeof(struct router));
	if(new_router)
	{
		struct router* current = sr->pwospf_subsys->network;
		struct router* prev = NULL;
		/*find end of list*/
		while(current)
		{
			prev = current;
			current = current->next;
		}
		if(!prev)
		{
			sr->powspf_subsys->network = new_router;
		}
		else
		{
			prev->next = new_router;
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
	}
	else
	{
		printf("Malloc error\n");
	}
}

void update_ftable(struct sr_instance *sr)
{
	reset_ftable(sr);
	struct adj_list *current = sr->pwospf_subsys->network;
	while(current)
	{
		int numhops = -1;
		struct router* next_hop = find_next_hop(sr, current->rt, &numhops);
		
		/*get interface next_hop is connected to from neighbor list*/
		
		int i;
		for(i = 0; i < current->rt->subnet_size; i++)
		{
			struct ftable_entry *cur_entry = ftable_contains(current->rt->subnets[i]->prefix,
					current->rt->subnets[i]->mask);
			
			if(cur_entry == NULL)
			{
				cur_entry = (struct ftable_entry *)malloc(sizeof(struct ftable_entry));
				cur_entry->prefix = current->rt->subnets[i]->prefix;
				cur_entry->mask = current->rt->subnets[i]->mask;
				cur_entry->next_hop = next_hop;
				cur_entry->interface = iface;
				cur_entry->num_hops = numhops;
				cur_entry->next = NULL;
				struct ftable_entry *walker = sr->pwospf_subsys->fwrd_table;
				if(walker->next == NULL)
				{
					sr->pwospf_subsys->fwrd_table = cur_entry;
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
			else if(cur_entry->num_hops > num_hops)
			{
				cur_entry->next_hop = next_hop;
				cur_entry->num_hops = numhops;
				cur_entry->interface = iface;
			}
		}
	}
	
}



struct router* find_next_hop(struct sr_instance *sr, struct router *dest, int *hops)
{
	int numhops = 0;
	while((dest->prev != NULL) && (dest->prev->rid != sr->pwospf_subsys->this_router->rid))
	{
		numhops++;
		dest = dest->prev;
	}
	if(dest->prev == NULL)
	{
		/*ERROR*/
		printf("Error finding next hop\n");
		*hops = -1;
		return NULL;
	}
	*hops = numhops;
	return dest;
}

struct ftable_entry *ftable_contains(struct sr_instance *sr, struct in_addr pfix, struct in_addr mask)
{
	struct ftable_entry *current = sr->pwospf_subsys->fwrd_table;
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
	struct ftable_entry *current = sr->pwospf_subsys->fwrd_table;
	sr->pwospf_subsys->fwrd_table = NULL;
	struct ftable_entry *prev = NULL;
	while(current)
	{
		prev = current;
		current = current->next;
		free(prev);
	}
}




void dijkstra(struct sr_instance* sr, struct router *host)
{
	struct adj_list *current = sr->pwospf_subsys->network;
	while(current != null)
	{
		current->rt->known = 0;
		current->rt->dist = -1;
		current->rt->prev = NULL;
		current = current->next;
	}
	
	
	sr->pwospf_subsys->this_router->dist = 0;
	struct router* least_unknown = get_smallest_unknown(sr->network);
	while(least_unknown != NULL)
	{
		least_unknown->known = 1; /*mark it as visited*/
		int i;
		struct router *w;
		for(i = 0; i < least_unknown->list_size; i++)
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
		least_unknown = get_smallest_unknown(sr->network);
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

