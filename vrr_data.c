/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/

*/

//#include <stdlib.h>
#include <linux/rbtree.h>
#include <linux/list.h>
#include "vrr.h"
#include "vrr_data.h"

typedef unsigned char mac_addr[MAC_ADDR_LEN];

static u_int vset[VRR_VSET_SIZE];

struct rt_node {
	struct rb_node node;
	u_int route;
	rt_entry routes;
};

typedef struct pset_data {
	u_int node;
	u_int status;
	mac_addr mac;
}pset_data_t;

typedef struct pset_list {
	struct list_head list;
	pset_data_t * data;
} pset_list_t;


static struct rb_root rt_root;


static int pset_size = 0;
static pset_list_t pset;

void vrr_data_init()
{
	rt_root = RB_ROOT;	//Initialize the routing table Tree
	INIT_LIST_HEAD(&pset.list);
}


/*
 * Helper function to search the Red-Black Tree routing table
 */
u_int rt_search(struct rb_root *root, u_int value)
{
	struct rb_node * node = root->rb_node;  // top of the tree

	while (node) {
		struct rt_node *curr = rb_entry(node, struct rt_node, node);

		if (curr->route > value)
			node = node->rb_left;
		else if (curr->route < value)
			node = node->rb_right;
		else
			return curr->route;  // Found it
	}
	return 0;
}


/*
 * Returns the next hop in the routing table, given the destination
 * parameter.  Returns -1 when no route exists.
 */
u_int rt_get_next(u_int dest)
{
	u_int next = rt_search(&rt_root, dest);
	if (next != 0)
		return next;
	else
		return 0;
}


int rt_add_route(struct routing_table_entry new_entry)
{
	return 0;
}

int rt_remove_nexts(u_int route_hop_to_remove)
{
	return 0;
}






/*
 * Physical set functions
 */
int pset_add(u_int node, u_int status, unsigned char mac[MAC_ADDR_LEN])
{
	pset_list_t * tmp = (pset_list_t *) kmalloc(sizeof(pset_list_t), GFP_KERNEL);

	tmp->data = (pset_data_t *) kmalloc(sizeof(pset_data_t), GFP_KERNEL);
	tmp->data->node = node;
        tmp->data->status = status;
        memcpy(tmp->data->mac, mac, sizeof(mac_addr));

	list_add(&(tmp->list), &(pset.list));

	pset_size += 1;

	return 1;
}
int pset_remove(u_int node)
{
	return 0;
}
int pset_get_status(u_int node)
{
	return 0;
}
int pset_update_status(u_int node, u_int newstatus)
{
	return 0;
}






/*
 * Virtual set functions
 */
int vset_add(u_int node)
{
	return 0;
}
int vset_remove(u_int node)
{
	return 0;
}
u_int * vset_get_all()
{
	return vset;
}
