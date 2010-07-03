#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "vrr.h"



static int vrr_output(struct sk_buff *skb, int type)
{
	int err;
	const size_t mac_addr_len = 6;
	
	if(type == VRR_HELLO)
	{
		sockaddr_ll addr;
		memset(&addr, 0, sizeof(sockaddr_ll));
		add.sll_family = AF_PACKET;
		addr.sll_protocol = htons(ETH_P_VRR);
		addr.sll_ifindex = 2; //this should not be hard coded
		addr.sll_halen = mac_addr_len;
		memcpy(addr.sll_addr,);
	}
}