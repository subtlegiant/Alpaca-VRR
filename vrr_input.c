#include <linux/if_ether.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/list.h>
#include <linux/workqueue.h>
#include <net/sock.h>
#include "vrr.h"
#include "vrr_data.h"

/* State transitions for physical neighbors upon receiving hello
 * messages */

#define TRANS_LINKED	0
#define TRANS_PENDING	1
#define TRANS_MISSING	2

/* Name arrays for easy state-machine debugging */
static char *pset_states[4] = {"linked", "pending", "failed", "unknown"};
static char *pset_trans[3] = {"linked", "pending", "missing"};

static int hello_trans[4][3] = {
	/* linked	pending		missing */
	{PSET_LINKED,	PSET_LINKED,	PSET_FAILED},	/* linked */
	{PSET_LINKED,	PSET_LINKED,	PSET_PENDING},	/* pending */
	{PSET_FAILED,	PSET_PENDING,	PSET_PENDING},	/* failed */
	{PSET_FAILED,	PSET_LINKED,	PSET_PENDING}};	/* unknown */

struct pset_update {
	u32 node;
	unsigned char mac[ETH_ALEN];
	int trans;
	int active;
	struct list_head list;
};

static struct pset_update pset_updates;

static DEFINE_SPINLOCK(pset_updates_lock);

void __init vrr_init_rcv() 
{
	INIT_LIST_HEAD(&pset_updates.list);
}

void pset_update_handler(struct work_struct *work)
{
	struct list_head *pos, *q;
	struct pset_update *tmp;
	struct vrr_node *me = vrr_get_node();
	int cur_state;
	int next_state;
	int cur_active;
	
	list_for_each_safe(pos, q, &pset_updates.list) {
		tmp = list_entry(pos, struct pset_update, list);
		cur_state = pset_get_status(tmp->node);
		next_state = hello_trans[cur_state][tmp->trans];
		cur_active = pset_get_active(tmp->node);

		VRR_DBG("%s[%s] ==> %s", pset_states[cur_state],
			pset_trans[tmp->trans], pset_states[next_state]);

		if (cur_state == PSET_UNKNOWN) {
			pset_add(tmp->node, tmp->mac, next_state, tmp->active);
			pset_state_update();
		} else if (cur_state != next_state ||
			   cur_active != tmp->active) {
			pset_update_status(tmp->node, next_state, tmp->active);
			pset_state_update();
		}

		if (!me->active && tmp->active && next_state == PSET_LINKED)
			send_setup_req(me->id, me->id, tmp->node);

		list_del(pos);
		kfree(tmp);
	}

	INIT_LIST_HEAD(&pset_updates.list);
}

static DECLARE_WORK(pset_updates_wq, pset_update_handler);

static int vrr_local_rcv_setup(u32 dst, u32 pid, u32 proxy,
			       u32 vset_size, u32 *vset);
static int vrr_local_rcv_setup_fail(u32 dst, u32 proxy,
				u32 vset_size, u32 *vset);

static int vrr_rcv_data(struct sk_buff *skb, const struct vrr_header *vh)
{
	u32 src = ntohl(vh->src_id);
	u32 dst = ntohl(vh->dest_id);
	u32 me = get_vrr_id();
        int ret = 0;

	VRR_DBG("Packet type: VRR_DATA");

        if (dst == me) {
		struct sock *sk = vrr_find_sock(src);
		
		if (sk) {
			ret = sk_receive_skb(sk, skb, 0);
                        sock_put(sk);
                        return ret;
                }
		VRR_DBG("No input socket found!");
                return -1;
        } else 
		vrr_forward(skb, vh);

	return 0;
}

static int vrr_rcv_hello(struct sk_buff *skb, const struct vrr_header *vh)
{
	int active;
        u32 src = ntohl(vh->src_id);
	u32 la_size, lna_size, p_size, i;
	u32 la[VRR_PSET_SIZE], lna[VRR_PSET_SIZE], p[VRR_PSET_SIZE];
	size_t offset = sizeof(struct vrr_header);
        size_t step = sizeof(u32);
	int trans = TRANS_MISSING;
        unsigned char src_addr[ETH_ALEN];
        struct vrr_node *me = vrr_get_node();
	struct pset_update *update = NULL;

	VRR_DBG("Packet type: VRR_HELLO");

        eth_header_parse(skb, src_addr);

        VRR_DBG("src_addr: %x:%x:%x:%x:%x:%x",
                src_addr[0], 
                src_addr[1],
                src_addr[2], 
                src_addr[3], 
                src_addr[4], 
                src_addr[5]);

	skb_copy_bits(skb, offset, &active, step);
        active = ntohl(active);
        offset += step;
	VRR_DBG("Sender active: %x", active);

	skb_copy_bits(skb, offset, &la_size, step);
        la_size = ntohl(la_size);
        offset += step;
	VRR_DBG("la_size: %x", la_size);
        if (la_size > VRR_PSET_SIZE) {
                VRR_DBG("Invalid la_size: %x. Dropping packet.", la_size);
                return -1;
        }

	skb_copy_bits(skb, offset, &lna_size, step);
        lna_size = ntohl(lna_size);
        offset += step;
	VRR_DBG("lna_size: %x", lna_size);
        if (lna_size > VRR_PSET_SIZE) {
                VRR_DBG("Invalid lna_size: %x. Dropping packet.", lna_size);
                return -1;
        }

	skb_copy_bits(skb, offset, &p_size, step);
        p_size = ntohl(p_size);
        offset += step;
	VRR_DBG("p_size: %x", p_size);
        if (p_size > VRR_PSET_SIZE) {
                VRR_DBG("Invalid p_size: %x. Dropping packet.", p_size);
                return -1;
        }

        if (la_size) {
                skb_copy_bits(skb, offset, la, la_size * step);
                offset += la_size * step;
                for (i = 0; i < la_size; i++) {
                        VRR_DBG("la[%x]: %x", i, ntohl(la[i]));
                        la[i] = ntohl(la[i]);
                        if (la[i] == me->id)
                                trans = TRANS_LINKED;
                }
        }

        if (lna_size) {
                skb_copy_bits(skb, offset, lna, lna_size * step);
                offset += lna_size * step;
                for (i = 0; i < lna_size; i++) {
                        VRR_DBG("lna[%x]: %x", i, ntohl(lna[i]));
                        lna[i] = ntohl(lna[i]);
                        if (lna[i] == me->id)
                                trans = TRANS_LINKED;
                }
        }

        if (p_size) {
                skb_copy_bits(skb, offset, p, p_size * step);
                offset += p_size * step;
                for (i = 0; i < p_size; i++) {
                        VRR_DBG("p[%x]: %x", i, ntohl(p[i]));
                        p[i] = ntohl(p[i]);
                        if (p[i] == me->id)
                                trans = TRANS_PENDING;
                }
        }

	update = (struct pset_update *)
		kmalloc(sizeof(struct pset_update), GFP_ATOMIC);
	if (!update)
		return -1;
	
	update->node = src;
	memcpy(update->mac, src_addr, ETH_ALEN);
	update->trans = trans;
	update->active = active;
	
	list_add_tail(&update->list, &pset_updates.list);

	schedule_work(&pset_updates_wq);

	return 0;
}

static int vrr_rcv_setup_req(struct sk_buff *skb, const struct vrr_header *vh)
{
	u32 nh, src, dst, proxy, vset_size, ovset_size, i;
	u32 *vset = NULL, *ovset = NULL;
	size_t offset = sizeof(struct vrr_header);
	size_t step = sizeof(u_int);

        src = ntohl(vh->src_id);
        dst = ntohl(vh->dest_id);

	nh = rt_get_next_exclude(dst, src);
	if (nh) {
                VRR_DBG("Forwarding to next hop: %x", nh);
		vrr_forward_setup_req(skb, vh, nh);
		return 0;
	}

	skb_copy_bits(skb, offset, &proxy, step);
	proxy = ntohl(proxy);
	offset += step;

	skb_copy_bits(skb, offset, &vset_size, step);
	vset_size = ntohl(vset_size);
	offset += step;
	VRR_DBG("Vset' size: %x", vset_size);

	if (vset_size < 0 || vset_size > VRR_VSET_SIZE) {
		VRR_ERR("Invalid vset' size: %x. Dropping packet.", vset_size);
		return -1;
	}

	vset = kmalloc(vset_size * sizeof(u32), GFP_ATOMIC);
	if (!vset) {
		VRR_ERR("No memory for vset. Dropping packet.");
		return -1;
	}

	skb_copy_bits(skb, offset, vset, vset_size * sizeof(u32));
	for (i = 0; i < vset_size; i++) {
		vset[i] = ntohl(vset[i]);
	}

	ovset_size = vset_get_all(&ovset);
	if (vrr_add(src, vset_size, vset)) {
		/* Send <setup, me, src, NewPid(), proxy, ovset> to me */
                vrr_local_rcv_setup(src, vrr_new_path_id(), proxy,
				    ovset_size, ovset);
		goto out;
	}
        else {
		vrr_local_rcv_setup_fail(src, proxy, ovset_size,
 					ovset);
		goto out;		
	}

out:
	kfree(ovset);
        kfree(vset);
	return 0;
}

static int vrr_rcv_setup_fail(struct sk_buff *skb, 
				const struct vrr_header *vh)
{
	u32 nh, src, dst, proxy;
        u32 *vset, vset_size;
    	size_t offset = sizeof(struct vrr_header);
	size_t step = sizeof(u32);

	int i;

	VRR_DBG("Packet type: VRR_SETUP_FAIL");
       
	src = ntohl(vh->src_id);
        dst = ntohl(vh->dest_id);

        skb_copy_bits(skb, offset, &proxy, step);
        proxy = ntohl(proxy);
        offset += step;

        skb_copy_bits(skb, offset, &vset_size, step);
        vset_size = ntohl(vset_size);
        offset += step;

        VRR_DBG("Vset' size: %x", vset_size);

	if (vset_size < 0 || vset_size > VRR_VSET_SIZE) {
		VRR_ERR("Invalid vset' size: %x. Dropping packet.", vset_size);
		return -1;
	}

	vset = kmalloc((vset_size + 1) * sizeof(u32), GFP_ATOMIC);
	if (!vset) {
		VRR_ERR("No memory for vset. Dropping packet.");
		return -1;
	}

	skb_copy_bits(skb, offset, vset, vset_size * sizeof(u32));
	for (i = 0; i < vset_size; i++) {
		vset[i] = ntohl(vset[i]);
	}

        if (pset_get_status(dst) == PSET_UNKNOWN)
                nh = rt_get_next(proxy);
        else
                nh = dst;

        if (nh) {
                /* Send <setup_fail, src, dst, proxy, vset'> to nh */
                send_setup_fail(src, dst, proxy, vset_size, vset, nh);
                return 0;
        }

        if (dst == get_vrr_id()) {
		vset[i+1] = ntohl(src);
 		vrr_add(-1, vset_size, vset);
	}

	return 0;
}

static int vrr_rcv_setup(struct sk_buff *skb, const struct vrr_header *vh)
{
        u32 nh, src, dst, pid, proxy, sender;
        u32 *vset, vset_size;
        size_t offset = sizeof(struct vrr_header);
        size_t step = sizeof(u32);
        int in_pset, i;
        unsigned char src_addr[ETH_ALEN];
	struct vrr_node *me = vrr_get_node();

	VRR_DBG("Packet type: VRR_SETUP");

        src = ntohl(vh->src_id);
        dst = ntohl(vh->dest_id);

        skb_copy_bits(skb, offset, &pid, step);
        pid = ntohl(pid);
        offset += step;

        skb_copy_bits(skb, offset, &proxy, step);
        proxy = ntohl(proxy);
        offset += step;

	VRR_DBG("src:%x dst:%x proxy:%x pid:%x", src, dst, proxy, pid);

        eth_header_parse(skb, src_addr);


        if (!memcmp(skb->dev->dev_addr, src_addr, ETH_ALEN)) {
                VRR_DBG("Received setup from myself");
                sender = get_vrr_id();
        } else {
                in_pset = pset_lookup_mac(src_addr, &sender);
                if (!in_pset) {
			tear_down_path(pid, src, null);
                        VRR_DBG("Sender is not in pset!");
                        return 0;
                }
        }

        if (pset_get_status(dst) == PSET_UNKNOWN)
		nh = (dst == me->id) ? 0 : rt_get_next(proxy);
        else
                nh = dst;

	VRR_DBG("nh: %x", nh);

        if (!rt_add_route(src, dst, sender, nh, pid)) {
                /* TearDownPath(<pid, src>, null) */
		tear_down_path(pid, src, null);
		VRR_DBG("Couldn't add route. Should tear down path to %x", src);
                return 0;
        }

	skb_copy_bits(skb, offset, &vset_size, step);
	vset_size = ntohl(vset_size);
	offset += step;
	VRR_DBG("Vset' size: %x", vset_size);

	if (vset_size < 0 || vset_size > VRR_VSET_SIZE) {
		VRR_ERR("Invalid vset' size: %x. Dropping packet.", vset_size);
		return -1;
	}

	vset = kmalloc(vset_size * sizeof(u32), GFP_ATOMIC);
	if (!vset) {
		VRR_ERR("No memory for vset. Dropping packet.");
		return -1;
	}

	skb_copy_bits(skb, offset, vset, vset_size * sizeof(u32));
	for (i = 0; i < vset_size; i++) {
		vset[i] = ntohl(vset[i]);
	}

        if (nh) {
                /* Send <setup, src, dst, pid, proxy, vset'> to nh */
                send_setup(src, dst, pid, proxy, vset_size, vset, nh);
                return 0;
        }

        if (vrr_add(src, vset_size, vset)) {
		VRR_DBG("Yay! Received multi-hop setup message from %x!", src);
		me->active = 1;
                return 0;
	} else {
		VRR_DBG("Coudn't add %x. Should tear down %x.", src, src);
		return 0;
	}

        /* TearDownPath(<pid, src>, null>) */
	tear_down_path(pid, src, null);
	return 0;
}

static int vrr_rcv_teardown(struct sk_buff *skb, const struct vrr_header *vh) 
{
	u32 src, dst, ea, endpt, pid, next, proxy;
        u32 *vset, vset_size;
        size_t offset = sizeof(struct vrr_header);
        size_t step = sizeof(u32);
        int i;
        rt_entry *route;

	VRR_DBG("Packet type: VRR_RCV_TEARDOWN");

	src = ntohl(vh->src_id);
	dst = ntohl(vh->dest_id);

        skb_copy_bits(skb, offset, &ea, step);
        ea = ntohl(ea);
        offset += step;

        skb_copy_bits(skb, offset, &pid, step);
        pid = ntohl(pid);
        offset += step;

        skb_copy_bits(skb, offset, &vset_size, step);
	vset_size = ntohl(vset_size);
	offset += step;

	route = rt_remove_route(ea, pid);
	
	if (route && route->na == src) {
		endpt = route->eb;
		next = route->nb;
	}
 	else {
		endpt = route->ea; 
		next = route->na;
	}

	if (vset_size < 0 || vset_size > VRR_VSET_SIZE) {
		VRR_ERR("Invalid vset' size: %x. Dropping packet.", vset_size);
		return -1;
	}

	vset = kmalloc(vset_size * sizeof(u32), GFP_ATOMIC);
	if (!vset) {
		VRR_ERR("No memory for vset. Dropping packet.");
		return -1;
	}

	skb_copy_bits(skb, offset, vset, vset_size * sizeof(u32));
	for (i = 0; i < vset_size; i++) {
		vset[i] = ntohl(vset[i]);
	}

	if (next) 
		send_teardown(pid, ea, vset, vset_size, next);
	else {
		vset_remove(endpt);
		if (vset)
			vrr_add(0, vset_size, vset);
		else {
			if(pset_get_proxy(&proxy)) 
				send_setup_req(get_vrr_id(), endpt, proxy);
		}
	}
	return 0;
}

static int (*vrr_rcvfunc[6])(struct sk_buff *, const struct vrr_header *) = {
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
	const struct vrr_header *vh = vrr_hdr(skb);
	int err;

        WARN_ATOMIC;

	printk(KERN_ALERT "Received a VRR packet!");

	/* VRR_INFO("vrr_version: %x", vh->vrr_version); */
	/* VRR_INFO("pkt_type: %x", vh->pkt_type); */
	/* VRR_INFO("protocol: %x", ntohs(vh->protocol)); */
	/* VRR_INFO("data_len: %x", ntohs(vh->data_len)); */
	/* VRR_INFO("free: %x", vh->free); */
	/* VRR_INFO("h_csum: %x", vh->h_csum); */
	/* VRR_INFO("src_id: %x", ntohl(vh->src_id)); */
	/* VRR_INFO("dest_id: %x", ntohl(vh->dest_id)); */

	if (vh->pkt_type < 0 || vh->pkt_type >= VRR_NPTYPES) {
		VRR_ERR("Unknown pkt_type: %x", vh->pkt_type);
		goto drop;
	}

	reset_active_timeout();
        pset_reset_fail_count(ntohl(vh->src_id));

	err = (*vrr_rcvfunc[vh->pkt_type])(skb, vh);

	if (err) {
		VRR_ERR("Error in rcv func.");
		goto drop;
	}

	return NET_RX_SUCCESS;
drop:
        kfree_skb(skb);
	return NET_RX_DROP;
}

int vrr_local_rcv_setup(u32 dst, u32 pid, u32 proxy,
			u32 vset_size, u32 *vset)
{
	u32 src = get_vrr_id();
	u32 nh;

	if (pset_get_status(dst) == PSET_UNKNOWN)
		nh = rt_get_next(proxy);
	else
		nh = dst;

	if (!rt_add_route(src, dst, src, nh, pid)) {
		/* TearDownPath(<pid, src>, null) */
		tear_down_path(pid, src, null);
		VRR_DBG("Couldn't add route. Should tear down path to %x", src);
		return 0;
	}

	if (nh) {
		VRR_DBG("Sending setup message: "
			"src:%x dst:%x pid:%x proxy:%x vset_size:%x nh:%x",
			src, dst, pid, proxy, vset_size, nh);
		send_setup(src, dst, pid, proxy, vset_size, vset, nh);
		return 0;
	}

	return 0;
}

int vrr_local_rcv_setup_fail(u32 dst, u32 proxy, u32 vset_size,
				u32 *vset)
{
	u32 src = get_vrr_id();
	u32 nh;
        u32 ovset_size, *ovset;

	ovset_size = 1;
        ovset = kmalloc(vset_size * sizeof(u32), GFP_ATOMIC);

	VRR_DBG("Receiving setup fail from myself.");

	if (pset_get_status(dst) == PSET_UNKNOWN)
		nh = rt_get_next(proxy);
	else
		nh = dst;

        ovset[0] = dst;
	vrr_add(-1, ovset_size, ovset);

        if (nh) {
		VRR_DBG("Sending setup_fail message: "
			"src:%x dst:%x proxy:%x vset_size:%x nh:%x",
			src, dst, proxy, vset_size, nh);
 		send_setup_fail(src, dst, proxy, vset_size, vset, nh);
		return 0;
	}

	return 0;
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
