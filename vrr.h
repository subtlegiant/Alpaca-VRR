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

#define VRR_INFO(fmt, arg...)	printk(KERN_INFO "VRR: " fmt "\n" , ## arg)
#define VRR_ERR(fmt, arg...)	printk(KERN_ERR "%s: " fmt "\n" , __func__ , ## arg)
#define VRR_DBG(fmt, arg...)	pr_debug("%s: " fmt "\n" , __func__ , ## arg)

#define VRR_MAX_HEADER (16 + 128)

//Offsets for accessing data in the header
#define VRR_VERS        0x0
#define VRR_PKT_TYPE    0x2
#define VRR_PROTO_TYPE  0x4
#define VRR_TLEN        0x6
#define VRR_FREE        0xA
#define VRR_CSUM        0xC
#define VRR_SRC         0x10
#define VRR_DST         0x18

#define u8 unsigned char
#define u16 unsigned short

struct vrr_node {
	int id; //128 bit identifier to match those of IP
	int vset_size; 
	int rtable_value; //the size of the virtual neighborhood
	//*rt = //pointer to the routing table
	//*vset = //pointer to vset structure
	//*pset = //pointer to a pset structure that maintains
	// the states of pset connections
};

struct vrr_packet {
	u_int src; //the current node id
	u_int dst; //the destination id
	u_int payload; //data being sent
	u_int pk_type; //hello message, setup, setup_req etc
};

struct vrr_sock {
	struct sock 	sk;
	u_int 		dest_addr;
};

/*
 * Functions provided by vrr_input.c
 */

extern int vrr_rcv(struct sk_buff *skb, struct net_device *dev,
		   struct packet_type *pt, struct net_device *orig_dev);

int get_pkt_type(struct sk_buff *skb);
int set_vrr_id(u_int vrr_id); //id is a random unsigned integer

#endif	/* _VRR_H */
