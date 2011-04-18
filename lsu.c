/* LSU implementation */

void handle_lsu(struct ospfv2_hdr* pwospf, struct packet_state* ps)
{
    
    if(pwospf->rid==ps->sr->routerID)
    {
        //DROP
    }
    
    
    ps->packet=ps->packet + sizeof(struct ospfv2_hdr); 
    struct ospfv2_lsu_hdr* lsu_head = (struct ospfv2_lsu_hdr*)(ps->packet);
    
    uint16_t old_seq= get_sequence(pwospf_rid, ps->sr);
    if(old_seq==lsu_head->seq)
    {
        //DROP
    }
    uint32t_t source_rid=pwospf->rid;
    struct route* advertisements=(struct route*)malloc(lsu_head->num_adv); //Can't rem if this is how to do this
    
   
    for(int i=0; i<lsu_head->num_adv; i++)
    {
         ps->packet=ps->packet+sizeof(struct ospfv2_lsu-hdr);
        struct ospfv2_lsu_adv* temp_adv=(struct ospfv2_lsu_adv*)(ps->packet);
        
        struct route temp_rt;
        temp_rt.prefix=temp_adv->subnet;
        temp_rt.mask=temp_adv->mask;
        temp_rt.r_id=temp_adv->rid;
        
        advertisement[i]=temp_rt;
    }
    
    /* Adds advertisements to topology if necessary and recomputes FT if necessary.*/
    add_to_top(ps->sr, source_rid, &advertisements[0], lsu_head->num_adv);
  
    set_sequence(pwospf->rid, lsu_head->seq, ps->sr);
  
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