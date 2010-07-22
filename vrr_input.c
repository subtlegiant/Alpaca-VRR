#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include "vrr.h"
#include "vrr_data.h"

/* State transitions for physical neighbors upon receiving hello
 * messages */

#define TRANS_LINKED	0
#define TRANS_PENDING	1
#define TRANS_MISSING	2

int hello_trans[4][3] = {
	/* linked     pending      missing */
	{PSET_LINKED, PSET_LINKED, PSET_FAILED},   /* linked */
	{PSET_LINKED, PSET_LINKED, PSET_PENDING},  /* pending */
	{PSET_FAILED, PSET_PENDING, PSET_PENDING}, /* failed */
	{PSET_FAILED, PSET_LINKED, PSET_PENDING}}; /* unknown */

static int vrr_rcv_data(struct sk_buff *skb, const struct ethhdr *eth, 
			const struct vrr_header *vh)
{
	/* u_int nh = NextHop(rt, dst)          //find next hop */
	/* if nh == 0 */
	/* send to application layer */
	/* else */
	/* send packet back out to nh */

	VRR_DBG("Packet type: VRR_DATA");

	return 0;
}

static int vrr_rcv_hello(struct sk_buff *skb, const struct ethhdr *eth,
			 const struct vrr_header *vh)
{
	int active;
	u_int la_size, lna_size, p_size, i;
	u_int la[VRR_PSET_SIZE], lna[VRR_PSET_SIZE], p[VRR_PSET_SIZE];
	int offset = sizeof(struct vrr_header);
	int trans = TRANS_MISSING;
	int cur_state = pset_get_status(vh->src_id);
	int next_state;
	int me = get_vrr_id();
	
	VRR_DBG("Packet type: VRR_HELLO");

	/* Debug hello payload */

	/* Get the sender's "active" flag */
	skb_copy_bits(skb, offset, &active, sizeof(u_int));
	VRR_DBG("Sender active: %x", active);

	/* Linked & active set */
	offset += sizeof(u_int);
	skb_copy_bits(skb, offset, &la_size, sizeof(u_int));
	VRR_DBG("la_size: %x", la_size);

	for (i = 0; i < la_size; i++) {
		offset += sizeof(u_int);
		skb_copy_bits(skb, offset, &la[i], sizeof(u_int));
		VRR_DBG("la[%x] = %x", i, la[i]);
		if (la[i] == me)
			trans = TRANS_LINKED;
	}

	/* Linked & not active set */
	offset += sizeof(u_int);
	skb_copy_bits(skb, offset, &lna_size, sizeof(u_int));
	VRR_DBG("lna_size: %x", lna_size);

	for (i = 0; i < lna_size; i++) {
		offset += sizeof(u_int);
		skb_copy_bits(skb, offset, &lna[i], sizeof(u_int));
		VRR_DBG("lna[%x] = %x", i, lna[i]);
		if (lna[i] == me)
			trans = TRANS_LINKED;
	}

	/* Pending set */
	offset += sizeof(u_int);
	skb_copy_bits(skb, offset, &p_size, sizeof(u_int));
	VRR_DBG("p_size: %x", p_size);

	for (i = 0; i < p_size; i++) {
		offset += sizeof(u_int);
		skb_copy_bits(skb, offset, &p[i], sizeof(u_int));
		VRR_DBG("p[%x] = %x", i, p[i]);
		if (p[i] == me)
			trans = TRANS_PENDING;
	}

	/* When node x receives a hello message from node y, it
	 * compares its state in the hello message with its local
	 * state for y. Then, it updates y's local state according to
	 * the state transition diagram shown in Figure 5. */

	next_state = hello_trans[cur_state][trans];

	VRR_DBG("cur_state: %x", cur_state);
	VRR_DBG("next_state: %x", next_state);

	if (cur_state == PSET_UNKNOWN)
		pset_add(vh->src_id, next_state, eth->h_source);
	else
		pset_update_status(vh->src_id, next_state);
	
	/* if src is in pset
	   do nothing
	   else if (me == active)
	   Need to understand 3.3.1, 3.3.2 and 3.3.3 text
	   else //proxy found
	   send setup_req packet to proxy
	*/
	
	return 0;
}

static int vrr_rcv_setup_req(struct sk_buff *skb, const struct ethhdr *eth,
			     const struct vrr_header *vh)
{
	/* send Setup packet to src from me, and include my vset */
	/* 	add src to vset */

	VRR_DBG("Packet type: VRR_SETUP_REQ");

	return 0;
}

static int vrr_rcv_setup(struct sk_buff *skb, const struct ethhdr *eth,
			 const struct vrr_header *vh)
{
	/* add src to vset */
	/* get vset' from packet */
	/* send setup_req to all nodes in vset' */
	/* once all setups received from further sent setup_req, then activate me */

	VRR_DBG("Packet type: VRR_SETUP");

	return 0;
}

static int vrr_rcv_setup_fail(struct sk_buff *skb, const struct ethhdr *eth,
			      const struct vrr_header *vh) 
{
	VRR_DBG("Packet type: VRR_SETUP_FAIL");
	
	return 0;
}

static int vrr_rcv_teardown(struct sk_buff *skb, const struct ethhdr *eth,
			    const struct vrr_header *vh) 
{
	VRR_DBG("Packet type: VRR_RCV_TEARDOWN");
	return 0;
}

static int (*vrr_rcvfunc[6])(struct sk_buff *, const struct ethhdr *eth,
			     const struct vrr_header *) = {
	&vrr_rcv_data,
	&vrr_rcv_hello,
	&vrr_rcv_setup_req,
	&vrr_rcv_setup,
	&vrr_rcv_setup_fail,
	&vrr_rcv_teardown
};

int vrr_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt,
	    struct net_device *orig_dev)
{
	const struct ethhdr *eth = eth_hdr(skb);
	const struct vrr_header *vh = vrr_hdr(skb);
	int err;

	/* Do stuff! */
	printk(KERN_ALERT "Received a VRR packet!");
	/* skb->pkt_type should be ETH_P_VRR */

	/* Debug vrr header */
	VRR_INFO("vrr_version: %x", vh->vrr_version);
	VRR_INFO("pkt_type: %x", vh->pkt_type);
	VRR_INFO("protocol: %x", vh->protocol);
	VRR_INFO("data_len: %x", vh->data_len);
	VRR_INFO("free: %x", vh->free);
	VRR_INFO("h_csum: %x", vh->h_csum);
	VRR_INFO("src_id: %x", vh->src_id);
	VRR_INFO("dest_id: %x", vh->dest_id);

	if (vh->pkt_type < 0 || vh->pkt_type >= VRR_NPTYPES) {
		VRR_ERR("Unknown pkt_type: %x", vh->pkt_type);
		goto drop;
	}

	err = (*vrr_rcvfunc[vh->pkt_type])(skb, eth, vh);
	kfree_skb(skb);

	if (err) {
		VRR_ERR("Error in rcv func.");
		goto drop;
	}

	return 0;
drop:
	return NET_RX_DROP;
}

/*DEFINITIONS
 * me = local node
 * vset = virtual neighbor set
 * vset' = contains the identifiers of the nodes in the vset of the source
 * pset = physical neighbor set
 * rt = routing table
 */

//Psuedo Code for more functions
/*
NextHop (rt, dst)
    endpoint := closest id to dst from Endpoints(rt)
    if (endpoint == me)
        return null
    return next hop towards endpoint in rt

Receive (<setup_req,src,dst,proxy,vset'>, sender)
    nh := NextHopExclude(rt, dst, src)
    if (nh != null)
        Send setup req, src, dst, proxy, vset’ to nh
    else
        ovset := vset; added := Add(vset, src, vset’)
        if (added)
            Send setup, me, src, NewPid(),proxy, ovset to me
        else
            Send setup fail, me, src, proxy, ovset to me

Receive (<setup,src,dst,proxy,vset'>, sender)
    nh := (dst in pset) ? dst : NextHop(rt, proxy)
    added := Add(rt, src, dst, sender, nh, pid )
    if (¬added ∨ sender ∈ pset)
        TearDownPath( pid, src , sender)
    else if (nh = null)
        Send setup, src, dst, pid, proxy, vset’ to nh
    else if (dst = me)
        added := Add(vset, src, vset’)
        if (¬added)
            TearDownPath( pid, src , null)
    else
        TearDownPath( pid, src , null)

Receive (<setup_fail,src,dst,proxy,vset'>, sender)
    nh := (dst ∈ pset) ? dst : NextHop(rt, proxy)
    if (nh = null)
        Send setup fail, src, dst, proxy, vset’ to nh
    else if (dst = me)
        Add(vset, null, vset’ Union {src} )

Receive (<teardown, <pid,ea>, vset'>, sender)
    < ea , eb , na , nb , pid := Remove(rt, pid, ea )
    next := (sender = na ) ? nb : na
    if (next = null)
        Send teardown, pid, ea , vset’ to next
    else
        e := (sender = na ) ? eb : ea
        Remove(vset, e)
        if (vset’ = null)
            Add(vset, null, vset’)
        else
            proxy := PickRandomActive(pset)
            Send <setup_req, me, e, proxy, vset> to proxy

Add (vset, src, vset')
    for each (id ∈ vset’)
        if (ShouldAdd(vset, id))
            proxy := PickRandomActive(pset)
            Send setup req, me, id, proxy, vset to proxy
    if (src = null ∧ ShouldAdd(vset,src))
        add src to vset and any nodes removed to rem
        for each (id ∈ rem) TearDownPathTo(id)
        return true;
    return false;

TearDownPath(<pid,ea>, sender)
    < ea , eb , na , nb , pid := Remove(rt, pid, ea )
    for each (n ∈ {na , nb , sender})
        if (n = null ∧ n ∈ pset)
            vset’ := (sender = null) ? vset : null
            Send teardown, pid, ea , vset’ to n

NextHopExclude (rt, dst, src)
    endpoint := closest id to dst from Endpoints(rt) that is not src
    if (endpoint == me)
        return null
    return next hop towards endpoint in rt

NewPid()
	returns a new path identifier that is not in use by the local node

Add(rt, <ea , eb , na , nb , pid> )
	adds the entry to the routing table unless there is already an
	entry with the same pid, ea 

Remove(rt, <pid, ea> )
	removes and returns the entry identified by pid, ea from the routing table

PickRandomActive(pset)
	returns a random physical neighbor that is active

Remove(vset, id)
	removes node id from the vset

ShouldAdd(vset, id)
	sorts the identifiers in vset union {id, me} and returns true if id
	should be in the vset;

TearDownPathTo(id)
	similar to TearDownPath but it tears down all vset-paths that have
	id as an endpoint
*/
