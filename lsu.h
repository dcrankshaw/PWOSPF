/* This is where the LSU definitions go */
#ifndef LSU_H

#define LSUINT 30



struct lsu_info
{
 struct lsu_rcd* rcd_lsus;   

}

struct lsu_rcd
{
    uint32_t source_rid;
    uint16_t seq;
    struct lsu_rcd* next;
    struct ospfv2_lsu_adv* adv_list;
}

void handle_lsu(struct ospfv2_hdr*, struct packet_state);

#endif LSU_H