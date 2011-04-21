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
#include "hello.h
#include "top_info.h"






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

        int i;
        for(i = 0; i < OSPF_DEFAULT_LSUINT; i += OSPF_DEFAULT_HELLOINT)
        {
        	printf("This is where we send Hello packets");
        	pwospf_lock(sr->ospf_subsys);
        	send_HELLO(sr);
        	pwospf_unlock(sr->ospf_subsys);
        	sleep(OSPF_DEFAULT_HELLOINT);
        }
        /*Send LSU updates*/
        
        
        
        
        printf("This is where we send LSU updates");
        pwospf_lock(sr->ospf_subsys);
        check_top_invalid(sr); /*Check for expired topo entries*/
    	send_lsu(sr);
        pwospf_unlock(sr->ospf_subsys);
        
        
        
       /* pwospf_lock(sr->ospf_subsys);
        printf(" pwospf subsystem sleeping \n");
        pwospf_unlock(sr->ospf_subsys);
        sleep(2);
        printf(" pwospf subsystem awake \n");*/
    };
} /* -- run_ospf_thread -- */


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
