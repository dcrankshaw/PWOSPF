/* This is where the LSU definitions go */
#ifndef LSU_H

struct lsu_info
{
    

}

struct lsu_rcd
{
    uint32_t source_rid;
    uint16_t seq;
}

void handle_lsu(struct ospfv2_hdr*, struct packet_state);

#endif LSU_H