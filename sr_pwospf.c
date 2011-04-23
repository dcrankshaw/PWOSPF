/*-----------------------------------------------------------------------------
 * file: sr_pwospf.c
 * date: Tue Nov 23 23:24:18 PST 2004
 * Author: Martin Casado
 *
 * Description:
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "sr_pwospf.h"
#include "sr_router.h"
#include "pwospf_protocol.h"
#include "lsu.h"
#include "hello.h"
#include "top_info.h"

#define FILENAME "ospf_config"

/* -- declaration of main thread function for pwospf subsystem --- */
static void* pwospf_run_thread(void* arg);

/*---------------------------------------------------------------------
 * Method: pwospf_init(..)
 *
 * Sets up the internal data structures for the pwospf subsystem
 *
 * You may assume that the interfaces have been created and initialized
 * by this point.
 *---------------------------------------------------------------------*/

int pwospf_init(struct sr_instance* sr)
{
    assert(sr);

    sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);
	
    /* -- handle subsystem initialization here! -- */
	
	
	sr->ospf_subsys->network = 0;
	sr->ospf_subsys->fwrd_table = 0;
	printf("about to create interfaces...\n");
	create_pwospf_ifaces(sr);
	printf("created interfaces\n");
	
	char *eth0_interface = "eth0";
	struct sr_if* zero = sr_get_interface(sr, eth0_interface);
	sr->ospf_subsys->this_router = add_new_router(sr, zero->ip);
	if(zero == NULL)
	{
		printf("null\n");
	}
	int i = 0;
	struct pwospf_iflist *cur_if = sr->ospf_subsys->interfaces;
	//struct route* cur_sn = NULL;
	
	while(cur_if)
	{
		
		sr->ospf_subsys->this_router->subnets[i] = (struct route*)malloc(sizeof(struct route));
		//cur_sn = sr->ospf_subsys->this_router->subnets[i];
		sr->ospf_subsys->this_router->subnets[i]->mask.s_addr = cur_if->mask.s_addr;
		//fprintf(stderr,"\nInterface mask:%s\nSubnet Mask: %s\n", inet_ntoa(cur_if->mask), inet_ntoa(sr->ospf_subsys->this_router->subnets[i]->mask));
		struct in_addr temp_add;
		struct in_addr temp_mask;
		temp_add.s_addr = cur_if->address.s_addr;
		temp_mask.s_addr = sr->ospf_subsys->this_router->subnets[i]->mask.s_addr;
		//fprintf(stderr,"\n2Interface mask:%s\nSubnet Mask: %s\n", inet_ntoa(cur_if->mask), inet_ntoa(sr->ospf_subsys->this_router->subnets[i]->mask));
		sr->ospf_subsys->this_router->subnets[i]->prefix.s_addr = (temp_add.s_addr & temp_mask.s_addr);
		
		if((cur_if->address.s_addr & cur_if->mask.s_addr) == cur_if->address.s_addr)
		{
			sr->ospf_subsys->this_router->subnets[i]->next_hop.s_addr = ntohl((htonl(cur_if->address.s_addr) | 1 ));
		}
		else
		{
			sr->ospf_subsys->this_router->subnets[i]->next_hop.s_addr = (cur_if->address.s_addr & cur_if->mask.s_addr);
		}
		
		sr->ospf_subsys->this_router->subnets[i]->r_id = 0;
		
		sr->ospf_subsys->this_router->subnet_size++;
		
		i++;
		cur_if = cur_if->next;	
	}
	print_subs(sr->ospf_subsys->this_router->subnets, sr->ospf_subsys->this_router->subnet_size);
	/*TODO TODO TODO TODO*/
	/* Will need to initialize the forwarding table the same way */
	
	
	
	sr->ospf_subsys->last_seq_sent = 0;
	sr->ospf_subsys->area_id = read_config(FILENAME); /* !!!! returns 0 if file read error !!!! */
	sr->ospf_subsys->autype = 0;

    /* -- start thread subsystem -- */
    if( pthread_create(&sr->ospf_subsys->thread, 0, pwospf_run_thread, sr)) {
        perror("pthread_create");
        assert(0);
    }

    return 0; /* success */
} /* -- pwospf_init -- */


/*---------------------------------------------------------------------
 * Method: pwospf_lock
 *
 * Lock mutex associated with pwospf_subsys
 *
 *---------------------------------------------------------------------*/

void pwospf_lock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_lock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_unlock
 *
 * Unlock mutex associated with pwospf subsystem
 *
 *---------------------------------------------------------------------*/

void pwospf_unlock(struct pwospf_subsys* subsys)
{
    if ( pthread_mutex_unlock(&subsys->lock) )
    { assert(0); }
} /* -- pwospf_subsys -- */

/*---------------------------------------------------------------------
 * Method: pwospf_run_thread
 *
 * Main thread of pwospf subsystem.
 *
 *---------------------------------------------------------------------*/

static
void* pwospf_run_thread(void* arg)
{
    struct sr_instance* sr = (struct sr_instance*)arg;
	fprintf(stderr, "REACHED OSPF SUBSYSTEM THREAD\n");
    while(1)
    {
        /* -- PWOSPF subsystem functionality should start  here! -- */

        //int i;
        /*
        for(i = 0; i < OSPF_DEFAULT_LSUINT; i += OSPF_DEFAULT_HELLOINT)
        {
        	fprintf(stderr, "This is where we send Hello packets\n");
        	pwospf_lock(sr->ospf_subsys);
        	send_HELLO(sr);
        	pwospf_unlock(sr->ospf_subsys);
        	sleep(OSPF_DEFAULT_HELLOINT);
        }*/
        /*Send LSU updates*/
        
        
        fprintf(stderr, "This is where we send Hello packets\n");
        pwospf_lock(sr->ospf_subsys);
        send_HELLO(sr);
        pwospf_unlock(sr->ospf_subsys);
        sleep(OSPF_DEFAULT_HELLOINT);
        
        fprintf(stderr, "This is where we send LSU updates\n");
        pwospf_lock(sr->ospf_subsys);
        check_top_invalid(sr); /*Check for expired topo entries*/
    	print_nbr_list(sr);
    	send_lsu(sr);
        pwospf_unlock(sr->ospf_subsys);
        sleep(OSPF_DEFAULT_HELLOINT); /*****For debugging *****/
        
        
       /* pwospf_lock(sr->ospf_subsys);
        printf(" pwospf subsystem sleeping \n");
        pwospf_unlock(sr->ospf_subsys);
        sleep(2);
        printf(" pwospf subsystem awake \n");*/
    };
} /* -- run_ospf_thread -- */


int handle_pwospf(struct packet_state* ps, struct ip* ip_hdr)
{
    fprintf(stderr, "Got to handle_pwospf\n");
    ps->packet+=sizeof(struct ip);
    struct ospfv2_hdr* pwospf_hdr=(struct ospfv2_hdr*)(ps->packet);
    fprintf(stderr, "Type: %u\n", pwospf_hdr->type);
    if(pwospf_hdr->version!=2)
    {
        fprintf(stderr, "Invalid version.\n");
        return 0;
    }
    /*Verify Checksum*/
    /*
    uint16_t csum_cal;
    fprintf(stderr, "Length of ospfv2_hdr: %lu\n", sizeof(struct ospfv2_hdr));
    csum_cal=cksum((uint8_t*) pwospf_hdr, (sizeof(struct ospfv2_hdr)-8));
    csum_cal=htons(csum_cal);
    if(csum_cal!=pwospf_hdr->csum)
    {
        fprintf(stderr, "Calc Checksum: %u Pack Checksum: %u \n", csum_cal, pwospf_hdr->csum);
        fprintf(stderr, "Wrong Checksum.\n");
        return 0;
    }*/
    
    if(pwospf_hdr->aid!=ntohl(ps->sr->ospf_subsys->area_id))
    {
        fprintf(stderr, "Wrong Area ID.\n");
        return 0;
    }
    
    if(pwospf_hdr->autype!=ps->sr->ospf_subsys->autype)
    {
        fprintf(stderr, "Wrong Auth Type.\n");
        return 0;
    }
    
    /* Now need to switch to the different hello and lsu pwospf */
    if(pwospf_hdr->type==OSPF_TYPE_HELLO)
    {
        handle_HELLO(ps, ip_hdr);
    }
    else if(pwospf_hdr->type==OSPF_TYPE_LSU)
    {
        handle_lsu(pwospf_hdr, ps, ip_hdr);
    }
    else
    {
        printf("Invalid PWOSPF Type.\n");
        return 0;
    }
    
    return 1;
}


void create_pwospf_ifaces(struct sr_instance *sr)
{

	struct sr_if *cur_sr_if = sr->if_list;
	struct pwospf_iflist *cur_pw_if = sr->ospf_subsys->interfaces;
	if(cur_sr_if != NULL)
	{
		sr->ospf_subsys->interfaces = (struct pwospf_iflist *)malloc(sizeof(struct pwospf_iflist));
		cur_pw_if = sr->ospf_subsys->interfaces;
		cur_pw_if->address.s_addr = cur_sr_if->ip;
		cur_pw_if->mask.s_addr = ntohl(IF_MASK);
		memmove(cur_pw_if->name, cur_sr_if->name, sr_IFACE_NAMELEN);
		memmove(cur_pw_if->mac, cur_sr_if->addr, 6);
		cur_pw_if->helloint = OSPF_DEFAULT_HELLOINT;
		cur_pw_if->neighbors = NULL;
		cur_pw_if->next = NULL;
		cur_sr_if = cur_sr_if->next;
	}
	
	while(cur_sr_if)
	{
		cur_pw_if->next = (struct pwospf_iflist *)malloc(sizeof(struct pwospf_iflist));
		cur_pw_if = cur_pw_if->next;
		cur_pw_if->address.s_addr = cur_sr_if->ip;
		cur_pw_if->mask.s_addr = ntohl(IF_MASK);
		memmove(cur_pw_if->name, cur_sr_if->name, sr_IFACE_NAMELEN);
		memmove(cur_pw_if->mac, cur_sr_if->addr, 6);
		cur_pw_if->helloint = OSPF_DEFAULT_HELLOINT;
		cur_pw_if->neighbors = NULL;
		cur_pw_if->next = NULL;
		cur_sr_if = cur_sr_if->next;
	}

}

uint32_t read_config(const char* filename)
{
	FILE* fp = 0;
	char line[BUFSIZ];
	uint32_t aid;

	assert(filename);
	if(access(filename,R_OK) != 0)
	{
		perror("access");
		return 0;
	}
	
	fp = fopen(filename,"r");
	
	while(fgets(line,BUFSIZ,fp) != 0)
	{
		sscanf(line, "%u", &aid);
	}
	return aid;
}
