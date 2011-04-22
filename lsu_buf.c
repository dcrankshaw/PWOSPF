/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/26/2011
 **********************************************************************/
 
 struct lsu_buf_ent* buf_packet(struct sr_instance* sr, uint8_t* pack, uint16_t pack_len, char* iface, struct in_addr ip_dst)
 {
    //Construct ARP Request
    
    struct lsu_buf_ent* buf_walker=0;
    
    if(sr->lsu_buffer==0)
    {
        sr->lsu_buffer=(struct lsu_buf_ent*)malloc(sizeof(struct lsu_buf_ent));
        sr->lsu_buffer->next=0;
        sr->lsu_buffer->lsu_packet=(struct uint8_t*)malloc(pack_len);
        memmove(sr->lsu_buffer->lsu_packet,pack, pack_len);
        sr->lsu_buffer->pack_len=pack_len;
        sr->lsu_buffer->interface=(char*) malloc(sr_IFACE_NAMELEN);
        memmove(sr->lsu_buffer->interface, iface, sr_IFACE_NAMELEN);
        sr->lsu_buffer->arp_req=/*ARP REQUEST*/
        sr->lsu_buffer->arp_len=/*ARP LENGTH*/
        sr->lsu_buffer->ip_dst=ip_dst;
        sr->lsu_buffer->num_arp_reqs=0;
        
        return sr->lsu_buffer;
        
    }
    else
    {
        buf_walker=sr->lsu_buffer;
        while(buf_walker->next)
        {
            buf_walker=buf_walker->next;
        }
        buf_walker=buf_walker->next=(struct lsu_buf_ent*) malloc(sizeof(struct lsu_buf_ent));
        buf_walker=buf_walker->next;
        buf_walker->next=0;
        
        buf_walker->lsu_packet=(uint8_t*)malloc(pack_len);
        memmove(buf_walker->lsu_packet, pack,pack_len);
        buf_walker->pack_len=pack_len;
        buf_walker->interface=(char *)malloc(sr_IFACE_NAMELEN);
		memmove(buf_walker->interface, iface->name, sr_IFACE_NAMELEN); 
		sr->lsu_buffer->arp_req=/*ARP REQUEST*/
        sr->lsu_buffer->arp_len=/*ARP LENGTH*/
		buf_walker->ip_dst=ip_dst;
		buf_walker->num_arp_reqs=0;
		
		return buf_walker;
        
    }
 
 }