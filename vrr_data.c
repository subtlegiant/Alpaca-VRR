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
  * Returns the next hop in the routing table, given the destination
  * parameter.  Returns -1 when no route exists.
  */
u_int get_rt_next(u_int dest)
{
	return -1;
}

/**
  * Helper function to search the Red-Black Tree routing table
  */
struct rt_node *my_rb_search(struct rb_root *root, u_int value)
{
	/*
	   struct rb_node *node = rt_root->rb_node;  // top of the tree

	   while (node) {
	   struct rt_node *current= rb_entry(node, struct rt_node, node);

	   if (current->route > value)
	   node = node->rb_left;
	   else if (current->route < value)
	   node = node->rb_right;
	   else
	   return stuff;  // Found it
	   }
	   return NULL;
	 */
	return NULL;
}
