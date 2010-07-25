//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include "vrr.h"
#include "vrr_data.h"

struct vrr_node *vrr;
struct pset_state *pstate;

int vrr_node_init()
{
	/*initialize all node
	 * members.
	 */
	int rand_id;

	vrr = kmalloc(sizeof(struct vrr_node), GFP_KERNEL);
  	if(!vrr){
	    printk(KERN_CRIT "Could not allocate vrr node");
	    return -1;
	}
	 
        rand_id = 0;

	vrr = (struct vrr_node *)kmalloc(sizeof(struct vrr_node), GFP_KERNEL);
        vrr->vset_size = 4;
        vrr->rtable_value = 0;
        vrr->version = 0x1;
        vrr->dev_name = "eth1"; //hard coded for now
	vrr->active = 0;
             
        //generate random id
        get_random_bytes(&vrr->id, VRR_ID_LEN);   

	return 0;
}

/* allocate the structure and set the size fields
 * for the states
 */
int pset_state_init()
{
	pstate = kmalloc(sizeof(struct pset_state), GFP_KERNEL);
        if(!pstate)
	    return -ENOMEM;

	pstate->la_size = 0;
        pstate->lna_size = 0;
        pstate->p_size = 0;

        pstate->lam_size = 0;
        pstate->lnam_size = 0;
	pstate->pm_size = 0;

	return 0;
}

void pset_state_update()
{
        pset_list_t *p;
        struct list_head *pos;
        int i, la_i = 0, lna_i = 0, p_i = 0;
        

        list_for_each(pos, pset_head()) {
                p = list_entry(pos, pset_list_t, list);
                if (p->status == PSET_LINKED) {
                        if (p->active) {
                                pstate->l_active[la_i] = p->node;
                                for (i = 0; i < ETH_ALEN; i++)
                                        pstate->la_mac[la_i][i] = p->mac[i];
                                la_i++;
                        } else {
                                pstate->l_not_active[lna_i] = p->node;
                                for (i = 0; i < ETH_ALEN; i++)
                                        pstate->lna_mac[lna_i][i] = p->mac[i];
                                lna_i++;
                        }
                }
                else if (p->status == PSET_PENDING) {
                        pstate->pending[p_i] = p->node;
                        for (i = 0; i < ETH_ALEN; i++)
                                pstate->pending_mac[p_i][i] = p->mac[i];
                        p_i++;
                }
        }
        pstate->la_size = pstate->lam_size = la_i;
        pstate->lna_size = pstate->lnam_size = lna_i;
        pstate->p_size = pstate->pm_size = p_i;
}

void detect_failures() {
        pset_list_t *tmp;
        struct list_head *pos, *q;
        u_int count, status;
        
        list_for_each_safe(pos, q, pset_head()) {
                tmp = list_entry(pos, pset_list_t, list);
                status = tmp->status;
                count = pset_inc_fail_count(tmp);
                if (count >= VRR_FAIL_FACTOR && status != PSET_FAILED) {
                        VRR_DBG("Marking failed node: %x", tmp->node);
                        tmp->status = PSET_FAILED;
                }
                if (count >= 2 * VRR_FAIL_FACTOR) {
                        VRR_DBG("Deleting failed node: %x", tmp->node);
                        list_del(pos);
                        kfree(tmp);
                }
        }
}

int send_setup_msg()
{
	return 1;
}

/* take a packet node and build header. Add header to sk_buff for
 * transport to layer two.
 * the header consists of:
 * Version\Unused: 8 bits reserved for future
 * Packet Type: 8 bits
 *      0: regular packet with payload/data
 *      1: Hello message
 *      2: Setup request
 *      3: Setup
 *      4: Setup fail
 *      5: Teardown
 *
 * Protocol type: vrr id, 8 bits
 * Total length: length of the data 16  bits
 * Open Space: 8 bits reserved for future use
 * Header checksum: 16bits
 * Source id: 32 bits
 * Destination id: 32 bits
 * Destination Mac: 48 bits
 *
*/

int build_header(struct sk_buff *skb, struct vrr_packet *vpkt)
{
	struct vrr_header header;
	int i;

	header.vrr_version = vrr->version;
	header.pkt_type = vpkt->pkt_type;
	header.protocol = htons(PF_VRR);
	header.data_len = htons(vpkt->data_len);
	header.free = 0;
	header.h_csum = 0;
	header.src_id = htonl(vrr->id);
	header.dest_id = htonl(vpkt->dst);

	//determine what kind of header
	if (vpkt->pkt_type == VRR_HELLO) {
		for (i = 0; i < ETH_ALEN; ++i) {
			header.dest_mac[i] = 0xff;
		}
	}
	//add header
	memcpy(skb_push(skb, sizeof(struct vrr_header)), &header,
	       sizeof(struct vrr_header));

	return 0;
}

int rmv_vrr_header(struct sk_buff *skb)
{
	/*remove the header for handoff to the socket layer
	 * this is only for packets received
	 */

	return 1;
}



/*Check the header for packet type and destination.
  */
/* int get_pkt_type(struct sk_buff *skb) */
/* { */
/* 	/\*use the offset to access the */
/* 	 * packet type, return one of the */
/* 	 * pt macros based on type. If data */
/* 	 * packet check destination and either */
/* 	 * forward or process */
/* 	 *\/ */

/* 	u8 pkt_type; */
	

/* 	return pkt_type; */
/* } */

int set_vrr_id(u_int vrr_id)
{
	if (vrr_id == 0) {
		/*generate random unsigned int
		 *arg is always zero at boot time
		 *but id may be changed/set via
		 *user space/sysfs which would pass
		 *a non-zero arg
		 */
	}

	else {
		/*set the id in the vrr node
		 */
	}
        vrr->id = 1;

	return 1;
}

/*called by sysfs show function or
 * sockets module
 */
u_int get_vrr_id()
{

	return vrr->id;
}

/*build and send a hello packet */

	/* Creates an sk_buff. Stuffs with
	 * vrr related info...
	 * hello packet payload consists of the arrays
	 * in the hpkt structure dilineated by
	 * zeroes. Ends with a call to build
	 * header which wraps the header around
	 * packet. Pass to vrr_output
	 */
int send_hpkt()
{
	struct sk_buff *skb;
	struct vrr_packet hpkt;
	int data_size, i = 0, p = 0;
  	u_int *hpkt_data;

        WARN_ATOMIC;

        VRR_DBG("My ID: %x", vrr->id);

        data_size = sizeof(u_int) * (pstate->la_size + 
                                     pstate->lna_size + 
                                     pstate->p_size + 4);

        hpkt_data = (u_int *) kmalloc(data_size, GFP_KERNEL);

        VRR_DBG("vrr->active: %x", vrr->active);
	hpkt_data[p++] = htonl(vrr->active);

        VRR_DBG("pstate->la_size: %x", pstate->la_size);
	hpkt_data[p++] = htonl(pstate->la_size);

        VRR_DBG("pstate->lna_size: %x", pstate->lna_size);
	hpkt_data[p++] = htonl(pstate->lna_size);

        VRR_DBG("pstate->p_size: %x", pstate->p_size);
	hpkt_data[p++] = htonl(pstate->p_size);

	for (i = 0; i < pstate->la_size; ++i) {
                VRR_DBG("pstate->l_active[%x]: %x", i, pstate->l_active[i]);
                hpkt_data[p++] = htonl(pstate->l_active[i]);
	}

	for (i = 0; i < pstate->lna_size; ++i) {
                VRR_DBG("pstate->l_not_active[%x]: %x", i, pstate->l_not_active[i]);
                hpkt_data[p++] = htonl(pstate->l_not_active[i]);
	}

	for (i = 0; i < pstate->p_size; ++i) {
                VRR_DBG("pstate->pending[%x]: %x", i, pstate->pending[i]);
                hpkt_data[p++] = htonl(pstate->pending[i]);
	}

	skb = vrr_skb_alloc(data_size, GFP_KERNEL);
	if (skb)
                memcpy(skb_put(skb, data_size), hpkt_data, data_size);
        else
		goto fail;

	hpkt.src = vrr->id;
	hpkt.dst = 0;                      /* broadcast addr */
	hpkt.data_len = data_size;
	hpkt.pkt_type = VRR_HELLO;
	build_header(skb, &hpkt);
	vrr_output(skb, vrr_get_node(), VRR_HELLO);

        kfree(hpkt_data);
	return 0;
 fail:
	VRR_ERR("hello skb buff failed");
        kfree(hpkt_data);
	return -1;
}

int *get_pset_active()
{
	return pstate->l_active;
}


int get_pset_active_size()
{
	return pstate->la_size * sizeof(int); 
}

mac_addr *get_pset_active_mac()
{
	return pstate->la_mac;
}

int get_pset_active_mac_size()
{
	return pstate->lam_size * sizeof(mac_addr);
}


struct vrr_node* vrr_get_node()
{
 	return vrr;
}

/*build and send a setup request*/
int send_setup_req()
{
	//struct sk_buff *skb;
	//struct vrr_packet *set_req;
	//set_req->pkt_type = VRR_SETUP;
	//build_header(skb, set_req);
	//vrr_output(skb, VRR_SETUP_REQ);

	return 1;

}

//TODO vrr exit node: release vrr node memory
//                    release pset_state memory
//		      stop timer
