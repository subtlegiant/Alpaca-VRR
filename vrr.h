#ifndef _VRR_H
#define _VRR_H

#include <linux/if_ether.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <linux/netdevice.h>
#include <linux/hrtimer.h>
#include <linux/random.h>
#include <linux/types.h>

#define WARN_ATOMIC if (in_atomic()) printk(KERN_ERR "\n%s: WARNING!!!!! THIS FUNCTION IS EXECUTED IN ATOMIC CONTEXT!!!!!\n", __func__)

/* PATCH: include/linux/socket.h */
#define AF_VRR 27
#define PF_VRR AF_VRR
/* PATCH: include/linux/if_ether.h */
#define ETH_P_VRR 0x8777

//Data packet types:
#define VRR_DATA    	0x0
#define VRR_HELLO   	1
#define VRR_SETUP_REQ   0x2
#define VRR_SETUP   	0x3
#define VRR_SETUP_FAIL  0x4
#define VRR_TEARDOWN    0x5
#define VRR_NPTYPES	6

#define VRR_INFO(fmt, arg...)	printk(KERN_INFO "VRR: " fmt "\n" , ## arg)
#define VRR_ERR(fmt, arg...)	printk(KERN_ERR "%s: " fmt "\n" , __func__ , ## arg)
#define VRR_DBG(fmt, arg...)	printk(KERN_DEBUG "%s: " fmt "\n" , __func__ , ## arg)

#define VRR_HEADER      40	
#define VRR_MAX_HEADER  VRR_HEADER + 128
#define VRR_SKB_RESERVE 80 
#define VRR_VSET_SIZE	4
#define VRR_PSET_SIZE	20

//Offsets for accessing data in the header
#define VRR_VERS        0x0
#define VRR_PKT_TYPE    0x2
#define VRR_PROTO_TYPE  0x4
#define VRR_TLEN        0x6
#define VRR_FREE        0xA
#define VRR_CSUM        0xC
#define VRR_SRC         0x10
#define VRR_DST         0x18

#define VRR_HPKT_DELAY  	10000	/* milliseconds */
#define VRR_FAIL_TIMEOUT	4	/* multiple of delay to mark
                                         * failed nodes */
#define VRR_ACTIVE_TIMEOUT	8	/* multiple of delay to activate
                                	* this node without virtual
                                	* neighbors */

#define VRR_ID_LEN	4

#define u8 unsigned char
#define u16 unsigned short
#define u_int unsigned int

//Mac address
#define MAC_ADDR_LEN 6
typedef unsigned char mac_addr[MAC_ADDR_LEN];
extern struct net *net;

struct eth_header {
    mac_addr dest;
    mac_addr source;
    u16 protocol;
};

/* We need to be explicit on the size of these fields, since 'int'
   varies depending on architecture. -tad    ***DONE*** */ 
struct pset_state {
	/* Shouldn't we be using linked lists here? We're going to be
	 * adding/removing to these arrays quite a bit, and we'll need
	 * to deal with shifting / bounds issues. -tad */

	// arrays holding pset ids
        u32 l_active[VRR_PSET_SIZE];
	u32 l_not_active[VRR_PSET_SIZE];
	u32 pending[VRR_PSET_SIZE];
 
        // arrays holding pset mac addrs mapped
        // by index to id arrays 
        mac_addr la_mac[VRR_PSET_SIZE];
        mac_addr lna_mac[VRR_PSET_SIZE];
        mac_addr pending_mac[VRR_PSET_SIZE];

        // sizes of id arrays
	int la_size;
        int lna_size;
        int p_size;
       

        // size of mac address arrays
        // used mostly for debug, should 
        // always be the same size as id
        // counterparts
        int lam_size;
        int lnam_size;
        int pm_size;  
};

/* Structure describing a VRR socket address. */
#define __SOCK_SIZE__	16      /* sizeof(struct sockaddr) */
#define svrr_zero	__pad
struct sockaddr_vrr {
	sa_family_t	svrr_family; /* AF_VRR */
	u32		svrr_addr;   /* VRR identifier */
        
        /* Pad to sizeof(struct sockaddr). */
        unsigned char	__pad[__SOCK_SIZE__ - 
                              sizeof(sa_family_t) -
                              sizeof(unsigned long)];
};

struct vrr_interface_list {
	char dev_name[IFNAMSIZ];
        struct list_head list;
};

struct vrr_node {
	u_int id; //128 bit identifier to match those of IP
	int vset_size; 
	int rtable_value; //the size of the virtual neighborhood
	u8 version;
	int active;
        int timeout;

	struct vrr_interface_list dev_list;
};

struct vrr_packet {
	u_int src; //the current node id
	u_int dst; //the destination id
	u16 data_len; //data being sent
	u8 pkt_type; //hello message, setup, setup_req etc
     	mac_addr dest_mac;  
};

struct vrr_header {
	u8 vrr_version;
	u8 pkt_type;
	u16 protocol;
	u16 data_len;
        u8 free;
        u16 h_csum;
        u_int src_id;
        u_int dest_id;
        u8 dest_mac[6];
};

static inline struct vrr_header *vrr_hdr(const struct sk_buff *skb)
{
        /* skb_network_header returns skb->head + skb->network_header */
        return (struct vrr_header *)skb_network_header(skb);
}

static inline struct sk_buff *vrr_skb_alloc(unsigned int len, gfp_t how)
{
	struct sk_buff *skb = NULL;

	if ((skb = alloc_skb(len + VRR_SKB_RESERVE, how))) {
		skb_reserve(skb, VRR_SKB_RESERVE);
	}
	return skb;
}

/*
 * Functions provided by vrr_input.c
 */

int vrr_rcv(struct sk_buff *skb, struct net_device *dev,
            struct packet_type *pt, struct net_device *orig_dev);

// forward packet to id closest to dest in rt
int vrr_forward(struct sk_buff *skb, const struct vrr_header *vh);
int vrr_forward_setup_req(struct sk_buff *skb, 
			  const struct vrr_header *vh,
			  u_int next_hop);

struct sock *vrr_find_sock(u32 addr);

/*
 * Functions provided by vrr_core.c
 */
  
// Vrr node functionality
int get_pkt_type(struct sk_buff *skb);
int set_vrr_id(u_int vrr_id); //id is a random unsigned integer
unsigned int get_vrr_id(void);
int vrr_node_init(void);
struct vrr_node *vrr_get_node(void);
void reset_active_timeout(void);

// Vrr packet handling
int send_hpkt(void);
int send_setup_req(u_int src, u_int dest, u_int proxy);
int send_setup(u32 src, u32 dest, u32 path_id, u32 proxy, u32 vset_size,
               u32 *vset, u32 to);
int send_setup_fail(u32 src, u32 dest, u32 proxy, u32 vset_size,
		u32 *vset, u32 to);
int send_teardown(u32 path_id, u32 endpoint, u32 *vset, 
		u32 vset_size, u32 to);
int tear_down_path(u32 path_id, u32 endpoint, u32 sender);
int build_header(struct sk_buff *skb, struct vrr_packet *vpkt);
int vrr_output(struct sk_buff *skb, struct vrr_node *node, int type);
int vrr_add(u32 src, u_int vset_size, u_int *vset);

void __init vrr_init_rcv(void);
/* Various utilities */
u32 vrr_new_path_id(void);

/* 
 * Functions to handle pset state
 */

int pset_state_init(void);

//return a pointer any of the pset state arrays
u32 *get_pset_active(void);
u32 *get_pset_not_active(void);
u32 *get_pset_pending(void);

//return the number of bytes in the pset state arrays
int get_pset_active_size(void);
int get_pset_not_active_size(void);
int get_pset_pending_size(void);

//return a pointer to any of the the pset mac arrays
mac_addr *get_pset_active_mac(void);
mac_addr *get_pset_not_active_mac(void);
mac_addr *get_pset_pending_mac(void);

//return the number of bytes in the pset mac arrays
int get_pset_active_mac_size(void);
int get_pset_not_active_mac_size(void);
int get_pset_pending_mac_size(void);

void pset_state_update(void);

void detect_failures(void);
void active_timeout(void);


#endif	/* _VRR_H */
