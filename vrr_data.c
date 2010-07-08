/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/

*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include "vrr.h"
#include "vrr_data.h"

static struct rb_root rt_root;

static u_int vset[VRR_VSET_SIZE];

struct rt_node {
	struct rb_node node;
	u_int route;
	rt_entry routes;
};

void vrr_data_init()
{
	rt_root = RB_ROOT;	//Initialize the routing table Tree
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

/*
 * Physical set functions
 */
int pset_add(u_int node, u_int status)
{
	return 0;
}

int pset_remove(u_int node)
{
	return 0;
}

int pset_get_status(u_int node)
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

u_int *vset_get_all()
{
	return vset;
}
