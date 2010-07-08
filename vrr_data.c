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

typedef struct pset_list {
	struct list_head list;
	u_int node;
	u_int status;
	mac_addr mac;
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
	struct rb_node *node = root->rb_node;	// top of the tree

	while (node) {
		struct rt_node *curr = rb_entry(node, struct rt_node, node);

		if (curr->route > value)
			node = node->rb_left;
		else if (curr->route < value)
			node = node->rb_right;
		else
			return curr->route;	// Found it
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

<<<<<<< HEAD
/*
 * Physical set functions
 */
int pset_add(u_int node, u_int status)
=======





/*
 * Physical set functions
 */
int pset_add(u_int node, u_int status, unsigned char mac[MAC_ADDR_LEN])
{
	pset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &pset.list){	//check to see if node already exists
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			return 0;
		}
	}


	tmp = (pset_list_t *) kmalloc(sizeof(pset_list_t), GFP_KERNEL);

	tmp->node = node;
        tmp->status = status;
        memcpy(tmp->mac, mac, sizeof(mac_addr));

	list_add(&(tmp->list), &(pset.list));

	pset_size += 1;

	return 1;
}
int pset_remove(u_int node)
>>>>>>> f6d5f28eae4dcbd109d94a283d3f8999fee6d900
{
	pset_list_t * tmp;
	struct list_head * pos, *q;

	list_for_each_safe(pos, q, &pset.list) {
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			list_del(pos);
			kfree(tmp);
		}
	}

	return 0;
}
<<<<<<< HEAD

int pset_remove(u_int node)
=======
u_int pset_get_status(u_int node)
>>>>>>> f6d5f28eae4dcbd109d94a283d3f8999fee6d900
{
	pset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &pset.list){	
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			return tmp->status;
		}
	}
	return 0;
}
<<<<<<< HEAD

int pset_get_status(u_int node)
=======
int pset_update_status(u_int node, u_int newstatus)
>>>>>>> f6d5f28eae4dcbd109d94a283d3f8999fee6d900
{
	pset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &pset.list){	
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			tmp->status = newstatus;
			return 1;
		}
	}
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

u_int *vset_get_all()
{
	return vset;
}
