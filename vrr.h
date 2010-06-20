#ifndef _VRR_H
#define _VRR_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>

/* PATCH: include/linux/socket.h */
#define AF_VRR 27
#define PF_VRR AF_VRR
/* PATCH: include/linux/if_ether.h */
#define ETH_P_VRR 0x8777

//Data packet types:
#define VRR_DATA    1   
#define VRR_HELLO   2
#define VRR_SETUP_REQ   3       
#define VRR_SETUP   4   
#define VRR_SETUP_FAIL  5
#define VRR_TEARDOWN    6   


struct vrr_node {
	int id; //128 bit identifier to match those of IP
	int vset_size; 
	int rtable_value; //the size of the virtual neighborhood
	//*rt = //pointer to the routing table 
	//*pset = //pointer to a pset structure that maintains
	// the states of pset connections
};

struct vrr_packet {
	u_int src; //the current node id
	u_int dst; //the destination id
	u_int payload; //data being sent
	u_int pk_type; //hello message, setup, setup_req etc
};

/*
 * Functions provided by vrr_input.c
 */

extern int vrr_rcv(struct sk_buff *skb, struct net_device *dev,
		   struct packet_type *pt, struct net_device *orig_dev);

#endif	/* _VRR_H */
