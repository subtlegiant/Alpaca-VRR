/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/

*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include "vrr.h"
#include "vrr_data.h"

static struct rb_root rt_root;

struct rt_node {
	struct rb_node node;
	u_int route;
	rt_entry routes;
};

void vrr_data_init()
{
	rt_root = RB_ROOT;	//Initialize the routing table Tree
}


/**
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


/**
  * Returns the next hop in the routing table, given the destination
  * parameter.  Returns -1 when no route exists.
  */
u_int get_rt_next(u_int dest)
{
	u_int next = rt_search(&rt_root, dest);
	if (next != 0)
		return next;
	else
		return -1;
}
