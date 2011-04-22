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
    char* interface;        /* Interface to be sent out of */
    uint8_t* arp_req;
    uint16_t arp_len;
    struct in_addr  ip_dst;
    int num_arp_reqs;
    struct lsu_buf_ent next;

}
 
 
 
 
 #endif