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

static struct vrr_node *vrr;
static struct pset_state *pstate;

static int vrr_node_init()
{

	vrr = (struct vrr_node*)kmalloc(sizeof(vrr_node), GFP_KERNEL);
	/*initialize all node
	 * members.
	 */
	int rand_id = 0;
	int err = 0;
	//pset_state.lactive[0] = 1;

	err = set_vrr_id(rand_id);

	return err;
}

static int set_pset_state_size()
{
	pstate->la_size = (sizeof(pstate->l_active)/sizeof(int));
	pstate->lna_size = (sizeof(pstate->l_not_active)/sizeof(int));
	pstate->p_size = (sizeof(pstate->pending)/sizeof(int));
	pstate->total_size = pstate->la_size + pstate->lna_size + pstate->psize;
}

static int send_setup_msg()
{
}

/* take a packet node and build header. Add header to sk_buff for
 * transport to layer two.
 * /*the header consists of:
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

static int build_header(struct sk_buff *skb, struct vrr_packet *vpkt)
{
	struct vrr_header *header;
	header = (struct vrr_header*)kmalloc(sizeof(vrr_header), GFP_KERNEL);
	int mac_addr_len = 6;
	int i;

	header->vrr_version = vrr->version;
	header->pkt_type = vpkt->pkt_type;
	header->protocol = 0;
	header->data_len = sizeof(skb->data);
	header->free = 0;;
	header->h_csum = 0;
	header->src_id = vrr->id;
	header->dest_id = vpkt->dst;

	//determine what kind of header
	if(vpkt->pkt_type == VRR_HELLO) {

	    for(i=0; i<mac_addr_len, ++i){
		    header->dest_mac[i] = 0xff;s
	    }
	}

	//add header
	memcpy(skb_push(skb, sizeof(vrr_header), header, sizeof(vrr_header));
	kfree(header);

	return 1;
}

static int rmv_header(struct sk_buff *skb)
{
	/*remove the header for handoff to the socket layer
	 * this is only for packets received
	 */

	return 1;
}

/*Check the header for packet type and destination.
  */
static int get_pkt_type(struct sk_buff *skb)
{
	/*use the offset to access the
	 * packet type, return one of the
	 * pt macros based on type. If data
	 * packet check destination and either
	 * forward or process
	 */
	return 1;

}

static int set_vrr_id(u_int vrr_id)
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
static u_int get_vrr_id()
{
	return vrr->id;
}

/*build and send a hello packet */

static int send_hpkt()
{
	struct sk_buff *skb;
	struct vrr_packet *hpkt;
	hpkt = (struct vrr_packet*)kmalloc(sizeof(vrr_packet), GFP_KERNEL)
	int data_size = pstate->total_size;
	int i;
	int err = 0;

	int hpkt_data[data_size + 3];

	for(i=0; i<pstate->la_size, ++i) {
            hpkt_data[i] = pstate->l_active[i];
	}
	//add delimiter
	hpkt_data[i + 1] = 0;

	for(i=0; i<pstate->lna_size, ++i) {
	    hpkt_data[i] = pstate->l_not_active[i];
	}
	//add delimiter
	hpkt_data[i + 1] = 0;

	for(i=0; i<pstate->p_size, ++i) {
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

	skb = vrr_skb_alloc(buf_size, GFP_KERNEL);
	if (skb) {
		memcpy(skb_put(skb, sizeof(hpkt_data)), hpkt_data, sizeof(hpkt_data));
	}

	else
	    goto fail;


	hpkt->src = vrr->id;
	hpkt->dest = 0 ; //broadcast hello packet
	hpkt->payload = null; //already in sk_buff
	hpkt->pkt_type = VRR_HELLO;

	build_header(skb, hpkt);
	kfree(hpkt);
	vrr_output(skb, VRR_HELLO);

	return 1;
fail:
	return err;
}

/*build and send a setup request*/
static int send_setup_req()
{
	struct sk_buff *skb;
	struct vrr_packet *set_req;

	set_req->pkt_type = VRR_SETUP;
	build_header(skb, set_req);

	//vrr_output(skb);

}

//TODO vrr exit node: release vrr node memory

