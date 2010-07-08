#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "vrr.h"


/* this is a hack output function based on Chad's
 * mock module code to test header code.
 */
static int vrr_output(struct sk_buff *skb, int type)
{
	int err, i;
	size_t mac_addr_len = 6;
	vrr_header *h = (vrr_header*)skb->data;

	if (type == VRR_HELLO) {
		sockaddr_ll addr;
		memset(&addr, 0, sizeof(sockaddr_ll));
		add.sll_family = AF_PACKET;
		addr.sll_protocol = htons(ETH_P_VRR);
		addr.sll_ifindex = 2;
		addr.sll_halen = mac_addr_len;
		memcpy(addr.sll_addr, h->dest_mac, sizeof(h->dest_mac));
	}

	int fd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_VRR);
	if (fd == -1)
		return 0;

	err = bind(fd, (struct sockaddr *) &addr,
		    sizeof(struct sockaddr_ll));
	if(err == -1)
		return 0;

	err = sendto(fd, skb, sizeof(skb), 0, (struct sockaddr *) &addr, sizeof(struct sockaddr_ll));
	if (err == -1)
		return 0;

	return 1;


}
