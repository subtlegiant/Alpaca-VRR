#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "vrr.h"

/* this is a hack output function based on Chad's
 * mock module code to test header code.
 */
int vrr_output(struct sk_buff *skb, struct vrr_node *vrr, int type)
{

	struct net_device *dev;
	struct eth_header header;
	struct vrr_header *vh;
        printk(KERN_ALERT "Output started");

	vh = (struct vrr_header *)skb->data;

	dev = dev_get_by_name(&init_net, "eth2");
        if (dev == 0) {
		printk(KERN_ALERT "failure in output");
	}
        printk(KERN_CRIT "dev initialized");

	skb->dev = dev;

	if (type == VRR_HELLO)
		memcpy(header.dest, dev->broadcast, MAC_ADDR_LEN);

//	else if (type == VRR_DATA)
//		memcpy(header.dest, vh->dest_mac, MAC_ADDR_LEN);

	printk(KERN_CRIT "header dest added");

	memcpy(header.source, dev->dev_addr, MAC_ADDR_LEN);
	header.protocol = htons(ETH_P_VRR);

	printk(KERN_CRIT "header done");

	memcpy(skb_push(skb, sizeof(header)), &header, sizeof(header));

	printk(KERN_CRIT "added to skb");

	dev_queue_xmit(skb);

        printk(KERN_CRIT "transmit done");
 
	return 0;
}
