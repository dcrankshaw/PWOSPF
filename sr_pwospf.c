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

    sr->ospf_subsys = (struct pwospf_subsys*)malloc(sizeof(struct
                                                      pwospf_subsys));

    assert(sr->ospf_subsys);
    pthread_mutex_init(&(sr->ospf_subsys->lock), 0);


    /* -- handle subsystem initialization here! -- */
	
	create_pwospf_ifaces(sr);


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

    while(1)
    {
        /* -- PWOSPF subsystem functionality should start  here! -- */

        pwospf_lock(sr->ospf_subsys);
        printf(" pwospf subsystem sleeping \n");
        pwospf_unlock(sr->ospf_subsys);
        sleep(2);
        printf(" pwospf subsystem awake \n");
    };
} /* -- run_ospf_thread -- */


int handle_pwospf(struct packet_state* ps, struct ip* ip_hdr)
{
    ps->packet+=sizeof(struct ip);
    struct ospfv2_hdr* pwospf_hdr=(struct ospfv2_hdr*)(ps->packet);
    if(pwospf_hdr->version!=2)
    {
        //FAIL
    }
    /*Verify Checksum*/
    uint16_t csum_cal=0;
    pwospf_hdr->audata=0;
    csum_cal=cksum((uint8_t*) pwospf_hdr, sizeof(struct ospfv2_hdr));
    if(csum_cal!=pwospf_hdr->csum)
    {
        //FAIL
    }
    
    if(pwospf_hdr->aid!=sr->pwospf_subsys->area_id)
    {
        //FAIL
    }
    
    if(pwospf_hdr->autype!=sr->pwospf_subsys->autype)
    {
        //FAIL
    }
    
    /* Now need to switch to the different hello and pwospf */
    
}

/*
struct pwospf_iflist
{
	struct in_addr address;
	struct in_addr mask;
	char name[sr_IFACE_NAMELEN];
	unsigned char addr[6];
	uint16_t helloint;
	struct neighbor_list *neighbors;
	struct pwospf_iflist *next;
};
*/


void create_pwospf_ifaces(struct sr_instance *sr)
{
	struct sr_if *cur_sr_if = sr->if_list;
	struct pwospf_iflist *cur_pw_if = sr->ospf_subsys->interfaces;
	if(cur_sr_if != NULL)
	{
		sr->ospf_subsys->interfaces = (struct pwospf_iflist *)malloc(sizeof(struct pwospf_iflist));
		cur_pw_if = sr->ospf_subsys->interfaces;
		cur_pw_if->address.s_addr = cur_sr_if->ip;
		cur_pw_if->mask.s_addr = IF_MASK;
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
		cur_pw_if->mask.s_addr = IF_MASK;
		memmove(cur_pw_if->name, cur_sr_if->name, sr_IFACE_NAMELEN);
		memmove(cur_pw_if->mac, cur_sr_if->addr, 6);
		cur_pw_if->helloint = OSPF_DEFAULT_HELLOINT;
		cur_pw_if->neighbors = NULL;
		cur_pw_if->next = NULL;
		cur_sr_if = cur_sr_if->next;
	}

}

