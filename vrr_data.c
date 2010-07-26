/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/
*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include "vrr.h"
#include "vrr_data.h"

//My Node
static u_int ME;

//spin locks
static DEFINE_SPINLOCK(vrr_rt_lock);
static DEFINE_SPINLOCK(vrr_vset_lock);
static DEFINE_SPINLOCK(vrr_pset_lock);

//Routing Table Setup
typedef struct routes_list {
	rt_entry		route;
	struct list_head	list;
} routes_list_t;

typedef struct rt_node {
	struct rb_node	node;
	u_int		route;
	routes_list_t	routes;
} rt_node_t;

static struct rb_root rt_root;

static int pset_size = 0;
static pset_list_t pset;

//Virtual Set Setup
typedef struct vset_list {
	struct list_head	list;
	u_int			node;
	int			diff;
} vset_list_t;

static int vset_size = 0;
static vset_list_t vset;
//static u_int vset[VRR_VSET_SIZE];


//internal functions
u_int get_diff(u_int x, u_int y);
void insert_vset_node(u_int node);
void rt_insert_helper(struct rb_root * root, rt_entry new_entry, u_int endpoint);
u_int rt_search(struct rb_root *root, u_int value);
u_int route_list_helper(routes_list_t * r_list, u_int endpoint);

void vrr_data_init()
{
	printk(KERN_ALERT "vrr_data_init enter\n");
	ME = get_vrr_id();
	rt_root = RB_ROOT;	//Initialize the routing table Tree
	INIT_LIST_HEAD(&pset.list);
	INIT_LIST_HEAD(&vset.list);
	printk(KERN_ALERT "vrr_data_init leave\n");
}

/*
 * Returns the next hop in the routing table, given the destination
 * parameter.  Returns 0 when no route exists.
 */
u_int rt_get_next(u_int dest)
{
	unsigned long flags;
	spin_lock_irqsave(&vrr_rt_lock, flags);
	u_int next = rt_search(&rt_root, dest);
	spin_unlock_irqrestore(&vrr_rt_lock, flags);

	return next;
}

/*
 * Helper function to search the Red-Black Tree routing table
 */
u_int rt_search(struct rb_root *root, u_int value)
{
	struct rb_node *node = root->rb_node;	// top of the tree

	while (node) {
		rt_node_t *curr = rb_entry(node, rt_node_t, node);

		if (curr->route > value)
			node = node->rb_left;
		else if (curr->route < value)
			node = node->rb_right;
		else {
			if (curr->route == ME)
				return 0;
			return route_list_helper(&curr->routes, value);
		}
	}
}

/* Helper function to search a list of route entries of a particular node, for the
 * next path node with the highest path_id
 */
u_int route_list_helper(routes_list_t * r_list, u_int endpoint) 
{
	routes_list_t * tmp = NULL;
	routes_list_t * max_entry = NULL;
	struct list_head * pos;
	int max_path = -1;

	list_for_each(pos, &r_list->list){	
		tmp= list_entry(pos, routes_list_t, list);
		if (tmp->route.path_id > max_path) {
			max_entry = tmp;
			max_path = tmp->route.path_id;
		}
	}
	
	if(get_diff(endpoint,max_entry->route.ea) < get_diff(endpoint,max_entry->route.eb0))
		if (max_entry->route.ea != ME)		
			return max_entry->route.na;
		else 
			return max_entry->route.nb;
	else 
		if (max_entry->route.eb != ME)		
			return max_entry->route.nb;
		else 
			return max_entry->route.na;
}


/**
 * Adds a route to the rb tree.  Might add two different entries if there
 * are both ea and eb values for the route.
 */
void rt_add_route(struct routing_table_entry new_entry)
{
	if (new_entry.ea) {
		rt_insert_helper(&rt_root, new_entry, new_entry.ea);
	}
	if (new_entry.eb) {
		rt_insert_helper(&rt_root, new_entry, new_entry.eb);
	}
}

/* Helper function to add new entries to routing table.
 */
void rt_insert_helper(struct rb_root * root, rt_entry new_entry, u_int endpoint)
{
	struct rb_node **link = &root->rb_node;
	struct rb_node *parent=NULL;
	rt_node_t * curr=NULL;
	int exists = 0;
	unsigned long flags;

	while (*link) //Find node to insert at
	{
		parent = *link;
		curr = rb_entry(parent, rt_node_t, node);

		if (curr->route > endpoint)
			link = &(*link)->rb_left;
		else if (curr->route < endpoint)
			link = &(*link)->rb_right;
		else {
			link=NULL;
			exists=1;
		}	
	}

	if(exists) { //add new_entry to curr->routes
		routes_list_t * tmp;

		tmp = (routes_list_t *) kmalloc(sizeof(routes_list_t), GFP_ATOMIC);
        	memcpy(&tmp->route, &new_entry, sizeof(rt_entry));

		spin_lock_irqsave(&vrr_rt_lock, flags);
		list_add(&(tmp->list), &(curr->routes.list));
		spin_unlock_irqrestore(&vrr_rt_lock, flags);
	}
	else {
		//create new rt_entry_t node
		rt_node_t * new_node = (rt_node_t *) kmalloc(sizeof(rt_node_t), GFP_ATOMIC);
		new_node->route = endpoint;
		//initialize new routes list
		INIT_LIST_HEAD( &(new_node->routes.list) );
		//copy new_entry to this new list
        	memcpy(&new_node->routes.route, &new_entry, sizeof(rt_entry));

		// Put the new node there
		rb_link_node(&(new_node->node), parent, link);
		rb_insert_color(&(new_node->node), root);
	}
}


int rt_remove_nexts(u_int route_hop_to_remove)  //TODO: code this
{
	return 0;
}


/*
 * Physical set functions
 */
int pset_add(u_int node, const unsigned char mac[MAC_ADDR_LEN], u_int status,
             u_int active)
{
	pset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &pset.list){	//check to see if node already exists
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			return 0;
		}
	}


	tmp = (pset_list_t *) kmalloc(sizeof(pset_list_t), GFP_ATOMIC);

	tmp->node = node;
        tmp->status = status;
        tmp->active = active ? 1 : 0;
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
	return PSET_UNKNOWN;
}

int pset_update_status(u_int node, u_int newstatus, u_int active)
{
	pset_list_t * tmp;
	struct list_head * pos;

	list_for_each(pos, &pset.list){	
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			tmp->status = newstatus;
                        tmp->active = active ? 1 : 0;
			return 1;
		}
	}
	return 0;
}

int pset_get_mac(u_int node, mac_addr * mac)
{
        pset_list_t * tmp;
        struct list_head * pos;

        list_for_each(pos, &pset.list){
                tmp= list_entry(pos, pset_list_t, list);
                if (tmp->node == node) {
        		memcpy(mac, tmp->mac, sizeof(mac_addr));
			return 1;
                }
        }
	return 0;
}

int pset_inc_fail_count(struct pset_list *node)
{
        atomic_inc(&node->fail_count);
        return atomic_read(&node->fail_count);
}

int pset_reset_fail_count(u_int node)
{
        pset_list_t *tmp;
        struct list_head *pos;

        list_for_each(pos, &pset.list) {
                tmp = list_entry(pos, pset_list_t, list);
                if (tmp->node == node) {
                        atomic_set(&tmp->fail_count, 0);
                        return 0;
                }
        }
        return -1;
}

struct list_head *pset_head()
{
        return &pset.list;
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
	//u_int half = UINT_MAX/2;
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
// int current_vset_size;
// u_int * vset_all = NULL;
// current_vset_size = vset_get_all(vset_all);
int vset_get_all(u_int * vset_all)
{
	vset_list_t * tmp;
	struct list_head * pos;
	int i = 0;
	vset_all = (u_int *) kmalloc(vset_size * sizeof(u_int), GFP_ATOMIC);

	list_for_each(pos, &vset.list){	
		tmp= list_entry(pos, vset_list_t, list);
		vset_all[i] = tmp->node;
		i++;
	}
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
	tmp = (vset_list_t *) kmalloc(sizeof(vset_list_t), GFP_ATOMIC);
	tmp->node = node;
	list_add(&(tmp->list), &(vset.list));
	vset_size += 1;
}

