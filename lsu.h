/* This is where the LSU definitions go */
#ifndef LSU_H

#include "sr_router.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"

#define LSUINT 30

void handle_lsu(struct ospfv2_hdr*, struct packet_state*);
void send_lsu(struct sr_instance* , struct packet_state*);
struct ospfv2_lsu_adv* generate_adv(struct sr_instance*);

#endif LSU_H