#include "vrr.h"

/* this is a hack output function based on Chad's
 * mock module code to test header code.
 */
int vrr_output(struct sk_buff *skb, struct vrr_node *vrr, int type)
{

	struct net_device *dev;
	struct eth_header header;
	struct vrr_header *vh;
	vh = (struct vrr_header *)skb->data;

	dev = dev_get_by_name(&init_net, vrr->dev_name);
	if (dev == 0) {
		printk(KERN_ALERT "failure in output");
	}

	if (type == VRR_HELLO)
		memcpy(header.dest, dev->broadcast, MAC_ADDR_LEN);

	else if (type == VRR_DATA)
		memcpy(header.dest, vh->dest_mac, MAC_ADDR_LEN);

	memcpy(header.src, dev->dev_addr, MAC_ADDR_LEN);
	header.protocol = ETH_P_VRR;

	skb_push(skb, sizeof(header));
	memcpy(skb_push(skb, sizeof(header)), &header, sizeof(header));

	dev_queue_xmit(skb);
 
	return 0;
}
