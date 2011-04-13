/* This is where the network topology information defs go */
#ifndef TOP_INFO_H
#define TOP_INFO_H




/* called when hello packets are received, added to the router's list of neighbors */
void add_neighbor(struct sr_instance* sr, struct sr_if *iface, uint32_t router_id);

/* called when LSU packets are received. Adds to the adjacency list */

int add_to_top(struct st_instance* sr, uint32_t host_rid, struct **route);
		/*NOTE: will need to memmove all routes into my router struct, so that struct **route
			can be freed in the method that calls add_to_top*/
			/*Returns 0 if nothing added. Returns 1 if new information received*/

uint16_t get_sequence(uint32_t router_id, struct sr_instance *sr);
void set_sequence(uint32_t router_id, uint16_t sequence, struct sr_instance *sr);







/*searches adjacency list for a router with the given id*/
struct router *search_topo(struct sr_instance sr, uint32_t router_id);


#endif