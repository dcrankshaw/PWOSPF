/* LSU implementation */

void handle_lsu(struct ospfv2_hdr* pwospf, struct packet_state* ps)
{
    
    if(pwospf->rid==ps->sr->routerID)
    {
        //DROP
    }
    
    ps->packet=ps->packet + sizeof(struct ospfv2_hdr); /* Now packet points to beg of lsu hdr */
    struct ospfv2_lsu_hdr* lsu_head = (struct ospfv2_lsu_hdr*)(ps->packet);
    
    struct lsu_rcd* rcd_lsu_walker=ps->sr->lsu_info->rcd_lsus;
    while(rcd_lsu_walker)
    {
        if(rcd_lsu_walker->source_rid==pwospf->rid)
            if(rcd_lsus->seq==lsu_head->seq)
            {
                //DROP
            }
        rcd_lsu_walker=rcd_lsu_walker->next;
    }
    
    /* Check if contents match contents of previous entry*/
    /******** How do I want to do this?? -MS 
    * Possibly don't do anything special just check database if any new information 
    * needs to be added. If it does because 1) it's new or 2) it's from a new host, 
    * update DB and recomputer FT. If no new info, just update DB.
    ps->packet=ps->packet + sizeof(struct ospfv2_lsu_hdr);  
    rcd_lsu_walker=ps->sr->lsu_info->rcd_lsus;
    while(rcd_lsu_walker)
    {
        if(rcd_lsu_walker->source_rid==pwospf->rid)
        {
            struct ospfv2_lsu_adv* adv_walker=rcd_lsu_walker->adv_list;
            while(adv_walker)
            {
                
            }
        }    
        rcd_lsu_walker=rcd_lsu_walker->next;
    }
    */   
}

void send_lsu()
{
    /*  Go through list of neighbors and generate an advertisement for each using generate_adv()
    
    Use a while loop for the sending and generating of the whole lsu packet.
        Generate ospfv2_lsu_hdr *****Is seq number the same for all packets sent and increased by 
        one everytime we send lsu packets?
        Generate ospfv2_hdr
        Generate IP header - with specific address of router
        Generate Ethernet Header
        sr_send_packet(instance, packet, length of packet, iface leaving from)
    
    Do we want to do the sending here or somewhere else??
}
/*
struct ospfv2_lsu_adv generate_adv()
{
    struct ospfv2_lsu_adv new_adv;
    new_adv.subnet=
    new_adv.mask=
    if(
} */