#include "linux/kernel.h"
#include "linux/skbuff.h"
#include "linux/netdevice.h"

int vrr_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt,
	    struct net_device *orig_dev)
{
	/* Do stuff! */
	printk("Received a VRR packet!");
}
