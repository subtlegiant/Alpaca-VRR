/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/
*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include "vrr.h"
#include "vrr_data.h"

//My Node
static u_int ME;

//Routing Table Setup
struct rt_node {
	struct rb_node node;
	u_int route;
	rt_entry routes;
};

static struct rb_root rt_root;

//Physical Set Setup
typedef struct pset_list {
	struct list_head list;
	u_int node;
	u_int status;
	mac_addr mac;
} pset_list_t;

static int pset_size = 0;
static pset_list_t pset;

//Virtual Set Setup
typedef struct vset_list {
	struct list_head list;
	u_int node;
	int diff;
} vset_list_t;

static int vset_size = 0;
static vset_list_t vset;
//static u_int vset[VRR_VSET_SIZE];


//internal functions
u_int get_diff(u_int x, u_int y);
void insert_vset_node(u_int node);

void vrr_data_init()
{
	ME = 10;	//TODO: Need to actually get a value from somewhere
	rt_root = RB_ROOT;	//Initialize the routing table Tree
	INIT_LIST_HEAD(&pset.list);
	INIT_LIST_HEAD(&vset.list);
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

u_int pset_get_status(u_int node)
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

int pset_update_status(u_int node, u_int newstatus)
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

void pset_get_mac(u_int node, mac_addr * mac)
{
        pset_list_t * tmp;
        struct list_head * pos;

        list_for_each(pos, &pset.list){
                tmp= list_entry(pos, pset_list_t, list);
                if (tmp->node == node) {
        		memcpy(mac, tmp->mac, sizeof(mac_addr));
                }
        }
}







/*
 * Virtual set functions
 */
int vset_add(u_int node)
{
	vset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &vset.list){	//check to see if node already exists
		tmp= list_entry(pos, vset_list_t, list);
		if (tmp->node == node) {
			return 0;
		}
	}

	if (vset_size < VRR_VSET_SIZE) {
		insert_vset_node(node);
		return 1;
	}


	//TODO: find possible node to remove to add the new node
	//u_int get_diff(u_int x, u_int y)
	u_int half = UINT_MAX/2;
	vset_size += 0;
	return 0;
}

int vset_remove(u_int node)
{
	vset_list_t * tmp;
	struct list_head * pos, *q;

	list_for_each_safe(pos, q, &vset.list) {
		tmp= list_entry(pos, vset_list_t, list);
		if (tmp->node == node) {
			list_del(pos);
			kfree(tmp);
			return 1;
		}
	}

	return 0;
}


//Good way to call this function:
// int vset_size = get_vset_size();
// u_int vset_all[vset_size];
// vset_get_all(vset_all);
void vset_get_all(u_int * vset_all)
{
	vset_list_t * tmp;
	struct list_head * pos;
	int i = 0;

	list_for_each(pos, &vset.list){	
		tmp= list_entry(pos, vset_list_t, list);
		vset_all[i] = tmp->node;
		i++;
	}
	return;
}

int get_vset_size()
{
	return vset_size;
}




//Helper Functions
u_int get_diff(u_int x, u_int y)
{
	u_int i = x - y;
	u_int j = y - x;
	if (i < j)
		return i;
	return j;
}

void insert_vset_node(u_int node)
{
	vset_list_t * tmp;
	tmp = (vset_list_t *) kmalloc(sizeof(vset_list_t), GFP_KERNEL);
	tmp->node = node;
	list_add(&(tmp->list), &(vset.list));
	vset_size += 1;
}

