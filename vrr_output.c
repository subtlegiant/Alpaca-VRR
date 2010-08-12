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

	vh = (struct vrr_header *)skb->data;

	VRR_INFO("vrr_version: %x", vh->vrr_version);
	VRR_INFO("pkt_type: %x", vh->pkt_type);
	VRR_INFO("protocol: %x", ntohs(vh->protocol));
	VRR_INFO("data_len: %x", ntohs(vh->data_len));
	VRR_INFO("free: %x", vh->free);
	VRR_INFO("h_csum: %x", vh->h_csum);
	VRR_INFO("src_id: %x", ntohl(vh->src_id));
	VRR_INFO("dest_id: %x", ntohl(vh->dest_id));

	list_for_each(pos, &vrr->dev_list.list) {
		tmp = list_entry(pos, struct vrr_interface_list, list);
		dev = dev_get_by_name(&init_net, tmp->dev_name);
		if (!dev)
			continue;
		clone = skb_clone(skb, GFP_ATOMIC);
		if (clone) {
			skb_reset_network_header(clone);
			clone->dev = dev;
			VRR_DBG("vh->dest_mac: %x:%x:%x:%x:%x:%x",
				vh->dest_mac[0], 
				vh->dest_mac[1],
				vh->dest_mac[2], 
				vh->dest_mac[3], 
				vh->dest_mac[4], 
				vh->dest_mac[5]);
			dev_hard_header(clone, dev, ETH_P_VRR, vh->dest_mac,
					dev->dev_addr, clone->len);
                        VRR_DBG("Sending over iface %s", tmp->dev_name);
			dev_queue_xmit(clone);
			dev_put(dev);
		}
	}

	kfree_skb(skb);
	return NET_XMIT_SUCCESS;
}

/* Call the routing table to find next hop destination */
int vrr_forward(struct sk_buff *skb, const struct vrr_header *vh)
{
	u32 nh;
        u8 nh_mac[ETH_ALEN];
	struct vrr_header *myvh;

        nh = rt_get_next(ntohl(vh->dest_id));
        if (!nh)
		goto fail;

   	if (!pset_get_mac(nh, nh_mac))
		goto fail;

	myvh = (struct vrr_header *)skb_network_header(skb);
        memcpy(myvh->dest_mac, nh_mac, MAC_ADDR_LEN);
	return vrr_output(skb, vrr_get_node(), VRR_DATA);

fail:
	return NET_XMIT_DROP;
}

int vrr_forward_setup_req(struct sk_buff *skb,
			  const struct vrr_header *vh,
			  u_int nh)
{
	unsigned char dest_mac[ETH_ALEN];
	struct vrr_header *myvh;

	if (!pset_get_mac(nh, dest_mac)) {
		VRR_ERR("Forwarding setup_req to unconnected node!");
		return 0;
	}

	myvh = (struct vrr_header *)skb_network_header(skb);
        memcpy(myvh->dest_mac, dest_mac, ETH_ALEN);
	return vrr_output(skb, vrr_get_node(), VRR_SETUP_REQ);
}
