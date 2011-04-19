/* jkashfkdshfkj */



struct router* get_router(uint32_t router_id, struct sr_instance* sr)
{
    strcut adj_list* net_walker=sr->pwospf_subsys->network;
    while(net_walker)
    {
        if(net_walker->rt->r_id==router_id)
            return net_walker->rt;
        else
            net_walker=net_walker->next;
    }
    return NULL;
}


void add_neighbor(struct sr_instance* sr, struct sr_if *iface, uint32_t router_id)
{





}

/* called when LSU packets are received. Adds to the adjacency list */
void add_to_topo(struct st_instance* sr, uint32_t host_rid, struct *route)
{
	add_to_adj_list(route); /*How do we make sure it gets added to all the entries it needs to*/
	dijkstra(sr, sr->pwospf_subsys->this_router);
	for(
	

}

struct router* find_next_hop(struct router *dest)
{
	/*Trace back the dest->prev until dest->prev == sr->pwospf_subsys->this_router
	  return dest
	  */

}

/*how to actually implement the forwarding table?????????*/


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

