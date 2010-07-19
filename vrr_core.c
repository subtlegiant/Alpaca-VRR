//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/mm.h>

#include "vrr.h"

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
	int mac_addr_len, i;

	header.vrr_version = vrr->version;
	header.pkt_type = vpkt->pkt_type;
	header.protocol = 27;
	header.data_len = vpkt->data_len;
	header.free = 0;
	header.h_csum = 0;
	header.src_id = vrr->id;
	header.dest_id = vpkt->dst;

	mac_addr_len = 6;
	//determine what kind of header
	if (vpkt->pkt_type == VRR_HELLO) {
		for (i = 0; i < mac_addr_len; ++i) {
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
	struct vrr_packet *hpkt;
	int data_size, i;
  	int *hpkt_data;

        data_size = pstate->la_size + 
                    pstate->lna_size + 
                    pstate->p_size;

	hpkt = kmalloc(sizeof(struct vrr_packet), GFP_KERNEL);

	//total bytes of all arrays + 3 ints
        hpkt_data = (int*)
		kmalloc((data_size + 3) * sizeof(int), GFP_KERNEL);
       
	for (i = 1; i < pstate->la_size; ++i) {
		hpkt_data[i] = pstate->l_active[i];
	}
	//add size of link active set
	hpkt_data[0] = pstate->la_size;

	for (i = 1; i < pstate->lna_size; ++i) {
		hpkt_data[i] = pstate->l_not_active[i];
	}
	//add size of link not active set
	hpkt_data[0] = pstate->lna_size;

	for (i = 1; i < pstate->p_size; ++i) {
		hpkt_data[i] = pstate->pending[i];
	}
	//add size of pstate pending set
	hpkt_data[0] = pstate->p_size;

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
        printk(KERN_ALERT "Made it here");
	vrr_output(skb, vrr, VRR_HELLO);

     	printk(KERN_ALERT "packet sent");	
	return 0;
 fail:
	printk(KERN_DEBUG "hello skb buff failed");
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
