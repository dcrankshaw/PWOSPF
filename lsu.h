/* This is where the LSU definitions go */
#ifndef LSU_H
#define LSU_H

#define LSUINT 30


/*THESE NEED TO TAKE struct sr_instance not packet_state*/
void handle_lsu(struct ospfv2_hdr*, struct packet_state*);
void send_lsu(struct sr_instance*, struct packet_state*);
struct ospfv2_lsu_adv* generate_adv(struct sr_instance*);

#endif LSU_H