#include "linux/kernel.h"
#include "linux/skbuff.h"
#include "linux/netdevice.h"

static int vrr_active = 0;  //Activates after first route successfully set up

//Struct for use in VRR Routing Table
typedef struct routing_table_entry
{
    u_int ea;     //endpoint A
    u_int eb;     //endpoint B
    u_int na;     //next A
    u_int nb;     //next B
    int path_id //Path ID
} rt_entry;



int vrr_rcv(struct sk_buff *skb, struct net_device *dev, struct packet_type *pt,
	    struct net_device *orig_dev)
{
	/* Do stuff! */
	printk("Received a VRR packet!");

	//int pt = get packet type out of packet
	/* skb->pkt_type should be ETH_P_VRR */

	/*
	if (pt == 0) { //Data
		u_int nh = NextHop(rt, dst)		//find next hop
		if nh == 0
			send to application layer
		else
			send packet back out to nh
	} else if (pt == 1) { //Hello
		if src is in pset
			do nothing
		else if (me == active)
			Need to understand 3.3.1, 3.3.2 and 3.3.3 text
		else	//proxy found
			send setup_req packet to proxy
	} else if (pt == 2) { //Setup Request
		send Setup packet to src from me, and include my vset
		add src to vset
	} else if (pt == 3) { //Setup
		add src to vset
		get vset' from packet
		send setup_req to all nodes in vset'
		once all setups received from further sent setup_req, then activate me
	} else if (pt == 4) { //Setup Fail
	} else if (pt == 5) { //Teardown
	} else {
		goto drop;
	}

 drop:
	kfree_skb(skb);
	return NET_RX_DROP;
	*/
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
