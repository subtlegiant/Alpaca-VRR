#include <linux/if_ether.h>
#include <linux/netdevice.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include "vrr.h"
#include "vrr_data.h"

int vrr_output(struct sk_buff *skb, struct vrr_node *vrr, 
               int type)
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
	skb->dev = dev;

	if (type == VRR_HELLO)
                memcpy(&header.h_dest, dev->broadcast, MAC_ADDR_LEN);
	else
		memcpy(&header.h_dest, vh->dest_mac, MAC_ADDR_LEN);

	memcpy(&header.h_source, dev->dev_addr, MAC_ADDR_LEN);
	header.h_proto = htons(ETH_P_VRR);

	memcpy(skb_push(skb, sizeof(header)), &header, sizeof(header));

        /* Check destination for local delivery */
        if (!memcmp(dev->dev_addr, vh->dest_mac, ETH_ALEN)) {
                VRR_DBG("Delivering locally");
		skb_set_mac_header(skb, 0);
		skb_set_network_header(skb, sizeof(struct ethhdr));
                vrr_rcv(skb, dev, NULL, NULL);
                goto out_success;
        }

	dev_queue_xmit(skb);

        VRR_DBG("Output done");

out_success:	
	return NET_XMIT_SUCCESS;

out: 
	return err;
}

/* Call the routing table to find next hop destination */
int vrr_forward(struct sk_buff *skb, const struct vrr_header *vh)
{
	u_int next_hop;
        u8 next_hop_mac[ETH_ALEN];
        struct ethhdr *dev_header;

        next_hop = rt_get_next(vh->dest_id);
        if (next_hop == 0) 
		goto fail;
        
   	if (!pset_get_mac(next_hop, next_hop_mac))
		goto fail;

        dev_header = (struct ethhdr *)skb_network_header(skb);
        memcpy(dev_header->h_dest, next_hop_mac, MAC_ADDR_LEN);
        
	dev_queue_xmit(skb);

	return NET_XMIT_SUCCESS;

fail:
	VRR_DBG("Forward failed");
        return NET_XMIT_DROP;
       
}

int vrr_forward_setup_req(struct sk_buff *skb, 
			  const struct vrr_header *vh,
			  u_int nh)
{
	struct ethhdr *dev_header;
	unsigned char dest_mac[ETH_ALEN];

	dev_header = (struct ethhdr *)skb_network_header(skb);
	
	if (!pset_get_mac(nh, dest_mac)) {
		VRR_ERR("Forwarding setup_req to unconnected node!");
		return 0;
	}

        memcpy(dev_header->h_dest, dest_mac, MAC_ADDR_LEN);
	dev_queue_xmit(skb);
	return 1;
}
