/* LSU implementation */

void handle_lsu(struct ospfv2_hdr* pwospf, struct packet_state* ps)
{
    if(pwospf->rid==ps->sr->routerID)
    {
        //DROP
    }
    
}