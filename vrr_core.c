//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/skbuff.h>

#include "vrr.h"

static struct vrr_node vrr = { 0, 0, 0
};

int vrr_node_init()
{
	/*initialize all node
	 * members.
	 */
	int rand_id = 0;
	int err = 0;

	err = set_vrr_id(rand_id);

	return 1;
}

/* take a packet node and build header. Add header to sk_buff for
 * transport to layer two.
 */
static int build_header(struct sk_buff *skb, struct vrr_packet *vpkt)
{
	//reserve 16 bytes for vrr header and 128 for eth device
	skb_reserve(skb, VRR_MAX_HEADER);

	/*the header consists of:
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
	 *
	 */

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
