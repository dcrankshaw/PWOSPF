/**********************************************************************
 * Group name: jhugroup1
 * Members: Daniel Crankshaw, Maddie Stone, Adam Gross
 * CS344
 * 4/26/2011
 **********************************************************************/
 
 struct lsu_buf_ent* add_to_lsu_buff(struct lsu_buf_ent* buff, uint8_t* pack, 
            uint16_t pack_len)
 {
    struct lsu_buf_ent* buf_walker=0;
    buf_walker=buff;
    
    if(buff==0)
    {
        buff=(struct lsu_buf_ent*)malloc(sizeof(struct lsu_buf_ent));
        buff->next=0;
        buff->lsu_packet=(struct uint8_t*)malloc(pack_len);
        memmove(buff->lsu_packet,pack, pack_len);
        buff->pack_len=pack_len;
        return sr->lsu_buffer;
    }
    else
    {
        buf_walker=buff;
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
        
		return buf_walker;
    }
 
 }
 
 void send_all_lsus(struct lsu_buf_ent* buff, uint8_t* mac, char* iface, struct sr_instance* sr)
 {
    struct lsu_buf_ent* prev=buff;
    
    while(buff)
    {
        struct sr_ethernet_hdr eth_hdr=(struct sr_ethernet_hdr*)(buff->packet);
        memmove(eth_hdr->ether_dhost, mac, ETHER_ADDR_LEN);
        sr_send_packet(sr, buff->packet, buff->pack_len, iface);
        prev=buff;
        buff=buff->next;
        free(prev);
    }
    free(buff);
    buff=NULL;
 }
 
 void delete_all_lsu(struct lsu_buf_ent* buff)
 {
    struct lsu_buf_ent* prev=buff;
    while(buff)
    {
        prev=buff;
        buff=buff->next;
        free(prev);
    }
    free(buff);
    buff=NULL;
 }