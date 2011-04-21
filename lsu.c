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
    
    /*** TODO: Forward LSU -mad*/
  
}

void send_lsu(struct sr_instance* sr, struct packet_state* ps)
{
    /*
     Go through list of neighbors and generate an advertisement for each using generate_adv()
    
    Use a while loop for the sending and generating of the whole lsu packet.
        Generate ospfv2_lsu_hdr *****Is seq number the same for all packets sent and increased by 
        one everytime we send lsu packets?
        Generate ospfv2_hdr
        Generate IP header - with specific address of router
        Generate Ethernet Header
        sr_send_packet(instance, packet, length of packet, iface leaving from)
    
    Do we want to do the sending here or somewhere else??
    */
    
    struct router* my_router=sr->ospf_subsys->this_router;
    uint8_t* pack=(uint8_t*)malloc(sizeof(struct sr_ethernet_hdr)+
            sizeof(struct ip)+sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr)
            + (my_router->subnet_size)(sizeof(struct ospfv2_lsu_hdr)));
    pack=pack+sizeof(struct sr_ethernet_hdr)+sizeof(struct ip)+
            sizeof(struct ospfv2_hdr)+sizeof(struct ospfv2_lsu_hdr);
    
    /*Generate Advertisements List*/
    struct ospfv2_lsu_adv* advs=generate_adv(sr);
    struct ospfv2_lsu_adv* adv_walker=advs;
    int j;
    for(j=0; j<my_router->subnet_size; j++)
    {
        memmove(pack, adv_walker[i], sizeof(struct ospfv2_lsu_adv));
        pack+=sizeof(struct ospfv2_lsu_adv);
    } 
    
    /*Advertisements are now in pack. pack points to end of last advertisement. 
    * Move pointer to where LSU header should begin. */
    pack-=(my_router->subnet_size)*(sizeof(struct ospfv2_lsu_adv));
    pack-=(sizeof(struct ospfv2_lsu_hdr));
    
    /*Generate LSU Header */
    struct ospfv2_lsu_hdr* lsu_hdr=(struct ospfv2_lsu_hdr*)malloc(sizeof(struct ospfv2_lsu_hdr));
    lsu_hdr->seq=sr->ospf_subsys->last_seq_sent;
    lsu_hdr->ttl=INIT_TTL;
    lsu_hdr->num_adv=my_router->subnet_size;
    
    memmove(pack,lsu_hdr, sizeof(struct ospfv2_lsu_hdr));
    pack-=(sizeof(struct ospfv2_hdr));
    
    /*Generate PWOSPF Header*/
    struct ospfv2_hdr* pwospf_hdr=(struct ospfv2_hdr*) malloc(sizeof(struct ospfv2_hdr));
    pwospf_hdr->version=OSPF_V2;
    pwospf_hdr->type=OSPF_TYPE_LSU;
    /*For length do I include ip and ethernet hdrs too???? */
    pwospf_hdr->len=sizeof(struct ospfv2_hdr) + sizeof(struct ospfv2_lsu_hdr)+ (my_router->subnet_size)*(sizeof(ospfv2_lsu_adv));
    pwospf_hdr->rid=my_router->rid;
    pwospf_hdr->aid= /*Where are we keeping the aid??*/
    pwospf_hdr->autype=0;
    pwospf_hdr->audata=0;
    pwospf_hdr->csum=0;
    pwospf_hdr->csum=cksum((uint8_t*)(pwospf_hdr), sizeof(struct ospfv2_hdr));
    pwospf_hdr->csum=htons(pwospf_hdr->csum);
    
    memmove(pack, pwospf_hdr, sizeof(struct ospfv2_hdr));
    pack-=(sizeof(struct ip));
    
    /*Generate IP Header */
    struct ip* ip_hdr=(struct ip*)malloc(sizeof(struct ip));
    ip_hdr->ip_hl=(sizeof(struct ip))/4;
    ip_hdr->ip_v=IP_VERSION;
    ip_hdr->ip_tos=ROUTINE_SERVICE;
    ip_hdr->ip_len=sizeof(struct ip)/4;
    ip_hdr->ip_id=/*TODO*/
    ip_hdr->ip_off=/*TODO*/
    ip_hdr->ip_ttl= INIT_TTL;
    ip_hdr->ip_p=OSPFV2_TYPE;
  
    struct pwospf_iflist* iface_walker=sr->ospf_subsys->interfaces;
    while(iface_walker)
    {
        ip_src=neigh_walker->address;
        struct neighbor_list* neigh_walker=iface_walker->neighbors;
        while (neigh_walker)
        {
            /*Finish constructing IP Header */
            ip_dst=neigh_walker->ip_address;
            ip_hdr->ip_sum=0;
            ip_hdr->ip_sum=cksum((uint8_t*)ip_hdr, sizeof(struct ip));
            ip_hdr->ip_sum=htons(ip_hdr->ip_sum);
            
            memmove(pack, ip_hdr, sizeof(struct ip));
            pack-=sizeof(struct sr_ethernet_hdr);
            
            /*Generate Ethernet Header*/
            struct sr_ethernet_hdr* eth_hdr=(struct sr_ethernet_hdr*)malloc(sizeof(struct sr_ethernet_hdr));
            eth_hdr->ether_type=ETHERTYPE_IP;
            
            /*Find Interface to be sent out of's MAC Address*/
            struct sr_if* src_if=sr_get_interface(sr, iface_walker->name);
            memmove(eth_hdr->ether_shost, src_if->addr, ETHER_ADDR_LEN);
            
            /*Find MAC Address of source*/
            struct arp_cache_entry* arp_ent=search_cache(sr, ip_hdr->ip_dst.s_addr);
            if(arp_ent!=NULL)
            {
                memmove(eth_hdr->ether_dhost, arp_ent->mac, ETHER_ADDR_LEN);
                memmove(pack, eth_hdr, sizeof(struct sr_ethernet_hdr));
                sr_send_packet(sr, pack, sizeof(pack), src_if);
                /*Packet has been sent*/
            }
            else
            {
                /*Need to buffer and then send ARP*/
            }
            
        }

    } 
}

struct ospfv2_lsu_adv* generate_adv(struct sr_instance* sr)
{
    struct router * my_router = sr->ospf_subsys->this_router;
    if(my_router==NULL);
        return NULL;
    
    /*Do i want an array of pointers or an array of headers?*/
    struct ospfv2_lsu_adv advertisements[my_router->subnet_size]=(struct ospfv2_lsu_adv*)malloc(my_router->subnet_size);
    
    int i;
    for(i=0; i<my_router->subnet_size; i++)
    {
        struct route* temp_rt=my_router->subnets[i];
        struct ospfv2_lsu_adv new_adv;
        new_adv.subnet=temp_rt->prefix;
        new_adv.mask=temp_rt->mask;
        new_adv.rid=temp_rt->r_id;
        advertisements[i]=new_adv;
    }
    
    return advertisements;   
} 