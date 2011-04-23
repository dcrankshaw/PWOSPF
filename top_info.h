/* This is where the network topology information defs go */
#ifndef TOP_INFO_H
#define TOP_INFO_H



uint16_t get_sequence(uint32_t router_id, struct sr_instance *sr);
void set_sequence(uint32_t router_id, uint16_t sequence, struct sr_instance *sr);
/*searches adjacency list for a router with the given id*/
struct router *search_topo(struct sr_instance, uint32_t);
void add_neighbor(struct sr_instance*, char *, uint32_t, struct in_addr);
/*checks whether there are any expired entries in the topoology*/
void check_top_invalid(struct sr_instance *);
/*removes from the given router's subnet list and adjacency list*/
int remove_subnet_from_router(struct sr_instance *, struct router *, struct route *);
int route_cmp(struct route*, struct route*);
/*TODO*/
int remove_from_topo(struct sr_instance *, struct router *);
int router_contains(struct route*, struct router*);
/* called when LSU packets are received. Adds to the adjacency list */
/*TODO: What if we get an LSU that advertises a link connecting to a router we don't
know about?*/
int add_to_top(struct sr_instance*, uint32_t, struct route**, int);
void add_to_existing_router(struct sr_instance *, struct route **, struct router*, int);
void add_new_route(struct sr_instance *, struct route*, struct router*);
struct router* add_new_router(struct sr_instance *sr, uint32_t host_rid);
void get_if_and_neighbor(struct pwospf_iflist *, struct neighbor_list *, struct sr_instance *, uint32_t);
int update_ftable(struct sr_instance *);
struct router* find_next_hop(struct sr_instance *sr, struct router *dest, int *hops);
struct ftable_entry *ftable_contains(struct sr_instance *, struct in_addr, struct in_addr);
int reset_ftable(struct sr_instance *);
void dijkstra(struct sr_instance*, struct router *);
struct router* get_smallest_unknown(struct adj_list *);
struct router* adj_list_contains(struct sr_instance *, uint32_t);
void print_nbr_list(struct sr_instance *sr);
void add_neighbor(struct sr_instance*, char *, uint32_t, struct in_addr);
void print_nbr(struct neighbor_list* nbr);
void print_subs(struct route** , int );
struct route* router_contains_subnet(struct router* host, uint32_t prefix);


#endif