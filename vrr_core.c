//This is the set main core functions that implement the
//vrr algorithm

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include "vrr.h"
#include "vrr_data.h"

struct vrr_node *vrr;
struct pset_state *pstate;

int vrr_node_init()
{
	/*initialize all node
	 * members.
	 */
	int rand_id;
        struct net_device *d;
        struct vrr_interface_list *tmp;

	vrr = kmalloc(sizeof(struct vrr_node), GFP_KERNEL);
  	if(!vrr){
	    printk(KERN_CRIT "Could not allocate vrr node");
	    return -1;
	}

        rand_id = 0;

	vrr = (struct vrr_node *)kmalloc(sizeof(struct vrr_node), GFP_KERNEL);
        vrr->vset_size = 4;
        vrr->rtable_value = 0;
        vrr->version = 0x1;
	vrr->active = 0;
        vrr->timeout = 0;

	// initialize the interface list
	INIT_LIST_HEAD(&vrr->dev_list.list);
	for_each_netdev(&init_net, d) {
		if (d->name) {
			if (strcmp(d->name, "eth0") &&
			    strcmp(d->name, "lo")) {
				tmp = (struct vrr_interface_list *)
					kmalloc(sizeof(struct vrr_interface_list), GFP_KERNEL);
				sscanf(d->name, "%s", tmp->dev_name);
				list_add(&(tmp->list), &(vrr->dev_list.list));
                        }
              	}
	}

        //generate random id
        get_random_bytes(&vrr->id, VRR_ID_LEN);

	return 0;
}

/* allocate the structure and set the size fields
 * for the states
 */
int pset_state_init()
{
	pstate = kmalloc(sizeof(struct pset_state), GFP_KERNEL);
        if(!pstate)
	    return -ENOMEM;

	pstate->la_size = 0;
        pstate->lna_size = 0;
        pstate->p_size = 0;

        pstate->lam_size = 0;
        pstate->lnam_size = 0;
	pstate->pm_size = 0;

	return 0;
}

void pset_state_update()
{
        pset_list_t *p;
        struct list_head *pos;
        int i, la_i = 0, lna_i = 0, p_i = 0;


        list_for_each(pos, pset_head()) {
                p = list_entry(pos, pset_list_t, list);
                if (p->status == PSET_LINKED) {
                        if (p->active) {
                                pstate->l_active[la_i] = p->node;
                                for (i = 0; i < ETH_ALEN; i++)
                                        pstate->la_mac[la_i][i] = p->mac[i];
                                la_i++;
                        } else {
                                pstate->l_not_active[lna_i] = p->node;
                                for (i = 0; i < ETH_ALEN; i++)
                                        pstate->lna_mac[lna_i][i] = p->mac[i];
                                lna_i++;
                        }
                }
                else if (p->status == PSET_PENDING) {
                        pstate->pending[p_i] = p->node;
                        for (i = 0; i < ETH_ALEN; i++)
                                pstate->pending_mac[p_i][i] = p->mac[i];
                        p_i++;
                }
        }
        pstate->la_size = pstate->lam_size = la_i;
        pstate->lna_size = pstate->lnam_size = lna_i;
        pstate->p_size = pstate->pm_size = p_i;
}

void detect_failures() {
        pset_list_t *tmp;
        struct list_head *pos, *q;
        u_int count, status;

        list_for_each_safe(pos, q, pset_head()) {
                tmp = list_entry(pos, pset_list_t, list);
                status = tmp->status;
                count = pset_inc_fail_count(tmp);
                if (count >= VRR_FAIL_TIMEOUT && status != PSET_FAILED) {
                        VRR_DBG("Marking failed node: %x", tmp->node);
                        tmp->status = PSET_FAILED;
                        pset_state_update();
                }
                if (count >= 2 * VRR_FAIL_TIMEOUT) {
                        VRR_DBG("Deleting failed node: %x", tmp->node);
                        list_del(pos);
                        kfree(tmp);
                }
        }
}

void active_timeout() {
        if (vrr->active)
                return;
        if (++vrr->timeout == VRR_ACTIVE_TIMEOUT)
                vrr->active = 1;
}

void reset_active_timeout() {
	vrr->timeout = 0;
}

/*build and send a setup request*/
int send_setup_req(u_int src, u_int dest, u_int proxy)
{
        unsigned char proxy_mac[ETH_ALEN];
	struct sk_buff *skb;
	struct vrr_packet setup_req_pkt;
  	int vset_size;
        int data_size;
        int i, p = 0;
	u_int *vset = NULL;
        u_int *setup_req_data;

        VRR_DBG("src: %x, dest: %x, proxy: %x", src, dest, proxy);

	pset_get_mac(proxy, proxy_mac);

        VRR_DBG("proxy_mac: %x:%x:%x:%x:%x:%x",
                proxy_mac[0], 
                proxy_mac[1],
                proxy_mac[2], 
                proxy_mac[3], 
                proxy_mac[4], 
                proxy_mac[5]);

  	vset_size = vset_get_all(&vset);
        data_size = sizeof(u_int) * (vset_size + 2);

	setup_req_data = kmalloc(data_size, GFP_ATOMIC);

        VRR_DBG("proxy id: %x", proxy);
        setup_req_data[p++] = htonl(proxy);

        VRR_DBG("vset size: %x", vset_size);
        setup_req_data[p++] = htonl(vset_size);

        for (i = 0; i < vset_size; ++i) {
                VRR_DBG("vset[%x]: %x", i, vset[i]);
		setup_req_data[p++] = htonl(vset[i]);
     	}

	skb = vrr_skb_alloc(data_size, GFP_ATOMIC);
	if (skb) {
                memcpy(skb_put(skb, data_size), setup_req_data, data_size);
        }
        else {
		goto fail;
        }

	setup_req_pkt.src = src;
        setup_req_pkt.dst = dest;
        setup_req_pkt.data_len = data_size;
	setup_req_pkt.pkt_type = VRR_SETUP_REQ;
        memcpy(setup_req_pkt.dest_mac, proxy_mac, ETH_ALEN);

        build_header(skb, &setup_req_pkt);
        vrr_output(skb, vrr_get_node(), VRR_SETUP_REQ);

	kfree(setup_req_data);
        kfree(vset);
  	return 0;

fail:
	VRR_ERR("skb allocation failed");
        kfree(setup_req_data);
        return -1;

}



int send_setup(u32 src, u32 dest, u32 path_id, u32 proxy, u32 vset_size,
               u32 *vset, u32 to)
{
        struct sk_buff *skb;
        struct vrr_packet setup_pkt;
        int data_size = sizeof(u32) * (vset_size + 3);
        u32 setup_data[data_size];
        int i, p = 0;
        unsigned char dest_mac[ETH_ALEN];

        if (!pset_get_mac(to, dest_mac)) {
                VRR_ERR("Sending setup msg to unconnected node: %x", to);
                return -1;
        }

        VRR_DBG("path_id: %x", path_id);
        setup_data[p++] = htonl(path_id);

        VRR_DBG("proxy: %x", proxy);
        setup_data[p++] = htonl(proxy);

        VRR_DBG("vset_size: %x", vset_size);
        setup_data[p++] = htonl(vset_size);

        for (i = 0; i < vset_size; i++) {
                VRR_DBG("vset[%x]: %x", i, vset[i]);
		setup_data[p++] = htonl(vset[i]);
     	}

        skb = vrr_skb_alloc(data_size, GFP_ATOMIC);
        if (skb) {
                memcpy(skb_put(skb, data_size), setup_data, data_size);
        } else {
                VRR_ERR("Failed to alloc skb.");
                return -1;
        }

        setup_pkt.src = src;
        setup_pkt.dst = dest;
        setup_pkt.data_len = data_size;
        setup_pkt.pkt_type = VRR_SETUP;
        memcpy(setup_pkt.dest_mac, dest_mac, ETH_ALEN);

        build_header(skb, &setup_pkt);
        vrr_output(skb, vrr_get_node(), VRR_SETUP);

	return 0;
}

int send_setup_fail(u32 src, u32 dst, u32 proxy, u32 vset_size,
			u32 *vset, u32 to)
{
	struct sk_buff *skb;
        struct vrr_packet setup_fail_pkt;
        int data_size = sizeof(u32) * (vset_size + 2);
        u32 setup_fail_data[data_size];
        int i, p = 0;
        unsigned char dest_mac[ETH_ALEN];

        if (!pset_get_mac(to, dest_mac)) {
                VRR_ERR("Sending setup_fail to unconnected node: %x", to);
                return -1;
        }

        VRR_DBG("proxy: %x", proxy);
        setup_fail_data[p++] = htonl(proxy);

        VRR_DBG("vset_size: %x", vset_size);
        setup_fail_data[p++] = htonl(vset_size);

        for (i = 0; i < vset_size; i++) {
                VRR_DBG("vset[%x]: %x", i, vset[i]);
		setup_fail_data[p++] = htonl(vset[i]);
     	}

        skb = vrr_skb_alloc(data_size, GFP_ATOMIC);
        if (skb) {
                memcpy(skb_put(skb, data_size), setup_fail_data, data_size);
        } else {
                VRR_ERR("Failed to alloc skb.");
                return -1;
        }

        setup_fail_pkt.src = src;
        setup_fail_pkt.dst = dst;
        setup_fail_pkt.data_len = data_size;
        setup_fail_pkt.pkt_type = VRR_SETUP_FAIL;
        memcpy(setup_fail_pkt.dest_mac, dest_mac, ETH_ALEN);

        build_header(skb, &setup_fail_pkt);
        vrr_output(skb, vrr_get_node(), VRR_SETUP_FAIL);

	return 0;
}

int send_teardown(u32 path_id, u32 endpoint, u32 *vset, 
			u32 vset_size, u32 to) 
{
	
	struct sk_buff *skb;
	struct vrr_packet teardown_pkt;
	int data_size = sizeof(u32) * (vset_size + 3);
	u32 teardown_data[data_size];
 	unsigned char dest_mac[ETH_ALEN];
	int i, p = 0;


	VRR_DBG("Building teardown packet");
	if(!pset_get_mac(to, dest_mac)) 
		return -1;

	VRR_DBG("ea: %x", endpoint);
	teardown_data[p++] = htonl(endpoint);
      
	VRR_DBG("path id: %x", path_id);
	teardown_data[p++] = htonl(path_id);

	VRR_DBG("vset_size: %x",  vset_size);
	teardown_data[p++] = htonl(vset_size);

	for (i = 0; i < vset_size; i++) {
		VRR_DBG("vset[%x]: %x", i, vset[i]);
		teardown_data[p++] = htonl(vset[i]);
	}

	skb = vrr_skb_alloc(data_size, GFP_ATOMIC);
        if (skb) {
                memcpy(skb_put(skb, data_size), teardown_data, data_size);
        } else {
                VRR_ERR("Failed to alloc skb.");
                return -1;
        }

        teardown_pkt.src = get_vrr_id();
        teardown_pkt.dst = to;
        teardown_pkt.data_len = data_size;
        teardown_pkt.pkt_type = VRR_TEARDOWN;
        memcpy(teardown_pkt.dest_mac, dest_mac, ETH_ALEN);
        
        build_header(skb, &teardown_pkt);
        vrr_output(skb, vrr_get_node(), VRR_TEARDOWN);

	return 0;
}
	
int tear_down_path(u32 path_id, u32 endpoint, u32 sender)
{
	rt_entry *route;
	u32 *vset, vset_size = 0;
 
	route = rt_remove_route(endpoint, path_id);

	if (route) {
		if (route->na && pset_contains(route->na)) {
			if (sender)
				vset_size = vset_get_all(&vset);
			send_teardown(path_id, endpoint, vset, vset_size, 
                                      route->na);	
		}
	        vset = 0;	
		if (route->nb && pset_contains(route->nb)) {
			if (sender)
				vset_size = vset_get_all(&vset);
			send_teardown(path_id, endpoint, vset, vset_size, 
                                      route->nb);
		}
		vset = 0;
		if (sender && pset_contains(sender)) {
			vset_size = vset_get_all(&vset);
			send_teardown(path_id, endpoint, vset, vset_size, 
                                      sender);
		}
	}

	else 
		return -1;

	return 0;
}

/* take a packet node and build header. Add header to sk_buff for
 * transport to layer two.
 * the header consists of:
 * Version\Unused: 8 bits reserved for future
 * Packet Type: 8 bits
 *      0: regular packet with payload/data
 *      1: Hello message
 *      2: Setup request
 *      3: Setup
 *      4: Setup fail
 *      5: Teardown
 *
 * Protocol type: vrr id, 8 bits
 * Total length: length of the data 16  bits
 * Open Space: 8 bits reserved for future use
 * Header checksum: 16bits
 * Source id: 32 bits
 * Destination id: 32 bits
 * Destination Mac: 48 bits
 *
*/

int build_header(struct sk_buff *skb, struct vrr_packet *vpkt)
{
	struct vrr_header header;

	header.vrr_version = vrr->version;
	header.pkt_type = vpkt->pkt_type;
	header.protocol = htons(PF_VRR);
	header.data_len = htons(vpkt->data_len);
	header.free = 0;
	header.h_csum = 0;
	header.src_id = htonl(vpkt->src);
	header.dest_id = htonl(vpkt->dst);

	//determine what kind of header
	if (vpkt->pkt_type == VRR_HELLO)
                memset(header.dest_mac, -1, ETH_ALEN);
        else
		memcpy(header.dest_mac, vpkt->dest_mac, ETH_ALEN);

	//add header
	memcpy(skb_push(skb, sizeof(struct vrr_header)), &header,
	       sizeof(struct vrr_header));

	return 0;
}

int rmv_vrr_header(struct sk_buff *skb)
{
	/*remove the header for handoff to the socket layer
	 * this is only for packets received
	 */

	return 1;
}



/*Check the header for packet type and destination.
  */
/* int get_pkt_type(struct sk_buff *skb) */
/* { */
/* 	/\*use the offset to access the */
/* 	 * packet type, return one of the */
/* 	 * pt macros based on type. If data */
/* 	 * packet check destination and either */
/* 	 * forward or process */
/* 	 *\/ */

/* 	u8 pkt_type; */


/* 	return pkt_type; */
/* } */



/*build and send a hello packet */

	/* Creates an sk_buff. Stuffs with
	 * vrr related info...
	 * hello packet payload consists of the arrays
	 * in the hpkt structure dilineated by
	 * zeroes. Ends with a call to build
	 * header which wraps the header around
	 * packet. Pass to vrr_output
	 */
int send_hpkt()
{
	struct sk_buff *skb;
	struct vrr_packet hpkt;
	int data_size, i = 0, p = 0;
  	u32 *hpkt_data;

        WARN_ATOMIC;

        VRR_DBG("My ID: %x", vrr->id);

        data_size = sizeof(u32) * (pstate->la_size +
                                     pstate->lna_size +
                                     pstate->p_size + 4);

        hpkt_data = (u32 *) kmalloc(data_size, GFP_ATOMIC);

        VRR_DBG("vrr->active: %x", vrr->active);
	hpkt_data[p++] = htonl(vrr->active);

        VRR_DBG("pstate->la_size: %x", pstate->la_size);
	hpkt_data[p++] = htonl(pstate->la_size);

        VRR_DBG("pstate->lna_size: %x", pstate->lna_size);
	hpkt_data[p++] = htonl(pstate->lna_size);

        VRR_DBG("pstate->p_size: %x", pstate->p_size);
	hpkt_data[p++] = htonl(pstate->p_size);

	for (i = 0; i < pstate->la_size; ++i) {
                VRR_DBG("pstate->l_active[%x]: %x", i, pstate->l_active[i]);
                hpkt_data[p++] = htonl(pstate->l_active[i]);
	}

	for (i = 0; i < pstate->lna_size; ++i) {
                VRR_DBG("pstate->l_not_active[%x]: %x", i, pstate->l_not_active[i]);
                hpkt_data[p++] = htonl(pstate->l_not_active[i]);
	}

	for (i = 0; i < pstate->p_size; ++i) {
                VRR_DBG("pstate->pending[%x]: %x", i, pstate->pending[i]);
                hpkt_data[p++] = htonl(pstate->pending[i]);
	}

	skb = vrr_skb_alloc(data_size, GFP_ATOMIC);
	if (skb)
                memcpy(skb_put(skb, data_size), hpkt_data, data_size);
        else
		goto fail;

	hpkt.src = vrr->id;
	hpkt.dst = 0;                      /* broadcast addr */
	hpkt.data_len = data_size;
	hpkt.pkt_type = VRR_HELLO;
	build_header(skb, &hpkt);
	vrr_output(skb, vrr_get_node(), VRR_HELLO);

        kfree(hpkt_data);
	return 0;
 fail:
	VRR_ERR("hello skb buff failed");
        kfree(hpkt_data);
	return -1;
}

int set_vrr_id(u_int vrr_id)
{
	if (vrr_id == 0) {
		/*generate random unsigned int
		 *arg is always zero at boot time
		 *but id may be changed/set via
		 *user space/sysfs which would pass
		 *a non-zero arg
		 */
	}

	else {
		/*set the id in the vrr node
		 */
	}
        vrr->id = 1;

	return 1;
}


/*called by sysfs show function or
 * sockets module
 */
u_int get_vrr_id()
{

	return vrr->id;
}


u32 *get_pset_active()
{
	return pstate->l_active;
}

u32 *get_pset_not_active()
{
	return pstate->l_not_active;
}

u32 *get_pset_pending()
{

	return pstate->pending;
}

int get_pset_active_size()
{
	return pstate->la_size * sizeof(u32);
}

int get_pset_not_active_size()
{
	return pstate->lna_size * sizeof(u32);
}

int get_pset_pending_size()
{
	return pstate->p_size * sizeof(u32);
}

mac_addr *get_pset_active_mac()
{
	return pstate->la_mac;
}

mac_addr *get_pset_not_active_mac()
{
	return pstate->lna_mac;
}

mac_addr *get_pset_pending_mac()
{
	return pstate->pending_mac;
}

int get_pset_active_mac_size()
{
	return pstate->lam_size * sizeof(mac_addr);
}

int get_pset_not_active_mac_size()
{
	return pstate->lnam_size * sizeof(mac_addr);
}

int get_pset_pending_mac_size()
{
	return pstate->pm_size * sizeof(mac_addr);
}

struct vrr_node* vrr_get_node()
{
 	return vrr;
}


int vrr_add(u32 src, u32 vset_size, u32 *vset)
{
	u32 i, proxy, rem = 0, ret;
        u32 me = get_vrr_id();

	for (i = 0; i < vset_size; i++)
		if (vset_should_add(vset[i])) {
			ret = pset_get_proxy(&proxy);
                        if (ret) {
                                VRR_DBG("Sending setup_req: me=%x, vset[%x]=%x, proxy=%x", me, i, vset[i], proxy);
                                send_setup_req(me, vset[i], proxy);
                        }
		}
        if (src != -1 && vset_should_add(src)) {
                VRR_DBG("Adding src: %x", src);
                ret = vset_add(src, &rem);
		if (ret > 0) {
			/* TearDownPathTo(rem) */
                        VRR_DBG("Should tear down path to %x", rem);
		}
                return 1;
        }
        return 0;
}

u32 vrr_new_path_id()
{
        u32 id;

        get_random_bytes(&id, sizeof(u32));
        /* TODO: Check for existence in our vset */
        return id;
}

//TODO vrr exit node: release vrr node memory
//                    release pset_state memory
//		      stop timer
