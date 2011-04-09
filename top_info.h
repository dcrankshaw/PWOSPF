/* This is where the network topology information defs go */

struct route
{
	uint32_t subnet;
	uint32_t mask;
	uint32_t r_id;

};


/* called when hello packets are received, added to the router's list of neighbors */
void add_neighbor(struct sr_instance* sr, struct sr_if *iface, uint32_t router_id);

/* called when LSU packets are received. Adds to the adjacency list */
void add_to_top(struct st_instance* sr, uint32_t host_rid, struct *route);