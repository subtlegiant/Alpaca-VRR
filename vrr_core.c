//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include "vrr.h"

struct vrr_node *vrr;
struct pset_state *pstate;

int vrr_node_init()
{
	vrr = (struct vrr_node *)kmalloc(sizeof(struct vrr_node), GFP_KERNEL);
	/*initialize all node
	 * members.
	 */
	u_int rand_id;
	int err;
        rand_id = 0;
        vrr->vset_size = 4;
        vrr->rtable_value = 0;
        vrr->version = 0x1;
        vrr->dev_name = "eth1"; //hard coded for now

	//pset_state.lactive[0] = 1;

	err = set_vrr_id(rand_id);

	return err;
}

/* allocate the structure and set the size fields
 * for the states
 */
int pset_state_init()
{
	pstate = (struct pset_state *)kmalloc(sizeof(struct pset_state), GFP_KERNEL);
	pstate->la_size = (sizeof(pstate->l_active) / sizeof(int));
	pstate->lna_size = (sizeof(pstate->l_not_active) / sizeof(int));
	pstate->p_size = (sizeof(pstate->pending) / sizeof(int));
	pstate->total_size = pstate->la_size + pstate->lna_size + pstate->p_size;

	return 1;
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
	struct vrr_header *header;
	int mac_addr_len, i;

	header.vrr_version = vrr->version;
	header.pkt_type = vpkt->pkt_type;
	header.protocol = ETH_P_VRR;
	header.data_len = vpkt->data_len;
	header.free = 0;
	header.h_csum = 0;
	header.src_id = vrr->id;
	header.dest_id = vpkt->dst;

	mac_addr_len = 6;
	//determine what kind of header
	if (vpkt->pkt_type == VRR_HELLO) {
		for (i = 0; i < mac_addr_len; ++i) {
			header->dest_mac[i] = 0xff;
		}
	}
	//add header
	memcpy(skb_push(skb, sizeof(struct vrr_header)), header,
	       sizeof(struct vrr_header));

	return 1;
}

int rmv_header(struct sk_buff *skb)
{
	/*remove the header for handoff to the socket layer
	 * this is only for packets received
	 */

	return 1;
}

/*Check the header for packet type and destination.
  */
int get_pkt_type(struct sk_buff *skb)
{
	/*use the offset to access the
	 * packet type, return one of the
	 * pt macros based on type. If data
	 * packet check destination and either
	 * forward or process
	 */
	return 1;
}

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

enum hrtimer_restart send_hpkt()
{
	struct sk_buff *skb;
	struct vrr_packet *hpkt;
	int data_size, i, err;
	hpkt = (struct vrr_packet *)kmalloc(sizeof(struct vrr_packet), GFP_KERNEL);

	data_size = pstate->total_size;

	int hpkt_data[data_size + 3];

	for (i = 0; i < pstate->la_size; ++i) {
		hpkt_data[i] = pstate->l_active[i];
	}
	//add delimiter
	hpkt_data[i + 1] = 0;
	for (i = 0; i < pstate->lna_size; ++i) {
		hpkt_data[i] = pstate->l_not_active[i];
	}
	//add delimiter
	hpkt_data[i + 1] = 0;
	for (i = 0; i < pstate->p_size; ++i) {
		hpkt_data[i] = pstate->pending[i];
	}
	//add delimiter
	hpkt_data[i + 1] = 0;

	/* Creates an sk_buff. Stuffs with
	 * vrr related info...
	 * hello packet payload consists of the arrays
	 * in the hpkt structure dilineated by
	 * zeroes. Ends with a call to build
	 * header which wraps the header around
	 * packet. Pass to vrr_output
	 */
	skb = vrr_skb_alloc(sizeof(hpkt_data), GFP_KERNEL);
	if (skb) {
		memcpy(skb_put(skb, sizeof(hpkt_data)), hpkt_data,
		       sizeof(hpkt_data));
	} else
		goto fail;

	hpkt->src = vrr->id;
	hpkt->dst = 0;		//broadcast hello packet
	hpkt->data_len= sizeof(hpkt_data);	//already in sk_buff
	hpkt->pkt_type = VRR_HELLO;
	build_header(skb, hpkt);
	kfree(hpkt);
	vrr_output(skb, vrr, VRR_HELLO);

	return HRTIMER_RESTART;
 fail:
	return HRTIMER_RESTART;
}

/*build and send a setup request*/
int send_setup_req()
{
	struct sk_buff *skb;
	struct vrr_packet *set_req;
//	set_req->pkt_type = VRR_SETUP;
//	build_header(skb, set_req);
//	vrr_output(skb, VRR_SETUP_REQ);
 	
	return 1;

}

//TODO vrr exit node: release vrr node memory
//                    release pset_state memory

