#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "arpq.h"
#include "lsu_buf.h"
#include "buffer.h"

#define LSU 1
#define ARP_REQ_INTERVAL 5 /*seconds*/
#define MAX_ARP_REQUESTS 5 /*number of requests*/


/*type determines whether this is an LSU or non LSU packet*/
void get_mac_address(struct sr_instance *sr, struct in_addr next_hop, uint8_t *packet,
					 uint16_t len, char *iface, int type, struct sr_ethernet_hdr* hdr)
{
	lock_arp_q(sr->arp_sub);
	struct arpq* entry = get_entry(sr, next_hop);
	if(entry != NULL)
	{
		/*TODO: check if expired entry*/
		if(type == LSU)
		{
			add_to_lsu_buff(entry->lsu_buf, packet, len);
		}
		else
		{
			add_to_pack_buff(entry->pac_buf, packet, len, hdr);
		}
	}
	else
	{
		entry = create_entry(sr, sr->arp_sub, next_hop, iface);
		if(type == LSU)
		{
			add_to_lsu_buff(entry->lsu_buf, packet, len);
		}
		else
		{
			add_to_pack_buff(entry->pac_buf, packet, len, hdr);
		}
		struct thread_args* args = (struct thread_args*)malloc(sizeof(struct thread_args));
		args->sr = sr;
		args->entry = entry;
		if(pthread_create(&entry->arp_thread, 0, arp_req_init, sr))
		{
        	perror("pthread_create");
        	assert(0);
    	}
	}
	unlock_arp_q(sr->arp_sub);
}

void arp_init(struct sr_instance* sr)
{
	sr->arp_sub = (struct arp_subsys*) malloc(sizeof(struct arp_subsys));
	sr->arp_sub->arp_cache = NULL;
	sr->arp_sub->pending = NULL;
	pthread_mutex_init(&(sr->arp_sub->arp_q_lock), 0);
	pthread_mutex_init(&(sr->arp_sub->cache_lock), 0);
	
}

struct arpq* get_entry(struct sr_instance *sr, struct in_addr next_hop)
{
	/* TODO TODO TODO TODO */
	
	return NULL;

}

void* arp_req_init(void* a)
{
	struct thread_args* args = (struct thread_args*) a;
	lock_arp_q(args->sr->arp_sub);
	sr_send_packet(args->sr, args->entry->arp_request, args->entry->request_len, args->entry->iface_name);
	unlock_arp_q(args->sr->arp_sub);
	sleep(ARP_REQ_INTERVAL);
	int i;
	for(i = 1; i > 0 && i < MAX_ARP_REQUESTS; i++)
	{
		
		lock_arp_q(args->sr->arp_sub);
		uint32_t temp = args->entry->ip.s_addr;
		unlock_arp_q(args->sr->arp_sub);
		uint8_t* mac = search_cache(args->sr, temp); /*will be an array of 6 bytes*/
		if(mac != NULL)
		{
			lock_arp_q(args->sr->arp_sub);
			send_all_packs(args->entry->pac_buf, mac, args->entry->iface_name, args->sr);
			send_all_lsus(args->entry->lsu_buf, mac, args->entry->iface_name, args->sr);
			i = -1; /*to exit the loop*/
			unlock_arp_q(args->sr->arp_sub);
		}
		else
		{
			lock_arp_q(args->sr->arp_sub);
			sr_send_packet(args->sr, args->entry->arp_request, args->entry->request_len, args->entry->iface_name);
			args->entry->num_requests++;
			unlock_arp_q(args->sr->arp_sub);
			sleep(ARP_REQ_INTERVAL);
		}
		
	}
	lock_arp_q(args->sr->arp_sub);
	
	/*means that we sent 5 arp requests*/
	if(i > 0)
	{
		
		/*TODO: decide on args*/
		send_all_icmps(args->entry->pac_buf, args->sr);
		delete_all_lsu(args->entry->lsu_buf);
	}
	args->entry->num_requests = -1;
	args->entry->pac_buf = NULL;
	args->entry->lsu_buf = NULL;
	unlock_arp_q(args->sr->arp_sub);
	free(args);
	return 0; /*return from thread's calling function, terminating thread*/
}


struct arpq* create_entry(struct sr_instance *sr, struct arp_subsys* arp_sub, struct in_addr next_hop, char* iface)
{
	struct arpq* entry = (struct arpq*)malloc(sizeof(struct arpq));
	entry->ip = next_hop;
	entry->num_requests = 0;
	/*This method needs to return an arp packet*/
	entry->request_len = 0;
	entry->arp_request = construct_request(sr, iface, next_hop.s_addr);
	entry->pac_buf = NULL;
	entry->lsu_buf = NULL;
	memmove(entry->iface_name, iface, sr_IFACE_NAMELEN);
	
	
	if(arp_sub->pending == NULL)
	{
		arp_sub->pending = entry;
	}
	else
	{
		struct arpq* walker = arp_sub->pending;
		while(walker->next)
		{
			walker = walker->next;
		}
		walker->next = entry;
	}
	return entry;
}





void lock_cache(struct arp_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->cache_lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */


void unlock_cache(struct arp_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->cache_lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */



void lock_arp_q(struct arp_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->arp_q_lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */


void unlock_arp_q(struct arp_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->arp_q_lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */