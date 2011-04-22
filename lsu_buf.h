/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/26/2011
 **********************************************************************/
 
 #ifndef LSUBUFFER_H
 #define LSUBUFFER_H
 
 #include "sr_router.h"
#include "sr_if.h"
#include "sr_rt.h"
#include "sr_protocol.h"
#include "pwospf_protocol.h"

struct lsu_buf_ent
{
    uint8_t* lsu_packet;
    uint16_t pack_len;
    struct lsu_buf_ent* next;
};
 
struct lsu_buf_ent* add_to_lsu_buff(struct lsu_buf_ent*, uint8_t*, uint16_t);
void delete_all_lsu(struct lsu_buf_ent*);
void send_all_lsus(struct lsu_buf_ent*, uint8_t*, char*, struct sr_instance*);
 
 
 #endif