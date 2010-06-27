/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/

*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include "vrr_data.h"


static 

void vrr_data_init() {


}


/**
  * Returns the next hop in the routing table, given the destination
  * parameter.  Returns -1 when no route exists.
  */
u_int get_rt_next(u_int dest) {

	return -1;
}
