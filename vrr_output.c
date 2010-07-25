#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "vrr.h"

int vrr_output(struct sk_buff *skb, struct vrr_node *vrr, int type)
{
	struct net_device *dev;
	struct ethhdr header;
	struct vrr_header *vh;
	int err = 0;

        WARN_ATOMIC;

        VRR_DBG("Output started");

	vh = (struct vrr_header *)skb->data;

	dev = dev_get_by_name(&init_net, vrr->dev_name);
        if (dev == 0) {
                VRR_ERR("Device %s does not exist", vrr->dev_name);
		err = -1;
		goto out;
	}
        VRR_DBG("dev initialized");

	skb->dev = dev;

	if (type == VRR_HELLO)
                memcpy(&header.h_dest, dev->broadcast, MAC_ADDR_LEN);
	else if (type == VRR_DATA)
		memcpy(&header.h_dest, vh->dest_mac, MAC_ADDR_LEN);

	VRR_DBG("header dest added");

	memcpy(&header.h_source, dev->dev_addr, MAC_ADDR_LEN);
	header.h_proto = htons(ETH_P_VRR);

	VRR_DBG("header done");

	memcpy(skb_push(skb, sizeof(header)), &header, sizeof(header));

	VRR_DBG("added to skb");

	dev_queue_xmit(skb);

        VRR_DBG("transmit done");

 out: 
	return err;
}
