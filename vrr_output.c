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
	struct vrr_header *vh;
        struct sk_buff *clone;
	struct list_head *pos;
	struct vrr_interface_list *tmp;

        WARN_ATOMIC;

	vh = (struct vrr_header *)skb->data;

	list_for_each(pos, &vrr->dev_list.list) {
		tmp = list_entry(pos, struct vrr_interface_list, list);
		dev = dev_get_by_name(&init_net, tmp->dev_name);
		if (!dev)
			continue;
		clone = skb_clone(skb, GFP_ATOMIC);
		if (clone) {
			skb_reset_network_header(clone);
			clone->dev = dev;
			dev_hard_header(clone, dev, ETH_P_VRR, vh->dest_mac, 
					dev->dev_addr, clone->len);
			dev_queue_xmit(clone);
		}
	}

	return NET_XMIT_SUCCESS;
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
        kfree_skb(skb);
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
