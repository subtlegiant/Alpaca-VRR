/*
rbtree to use: http://lwn.net/Articles/184495/

and linked list at each node: http://isis.poly.edu/kulesh/stuff/src/klist/
*/

#include <linux/rbtree.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sort.h>
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
	int			diff_left;
        int			diff_right;
} vset_list_t;

static int vset_size = 0;
static vset_list_t vset;

//internal functions
u_int get_diff(u_int x, u_int y);
void insert_vset_node(u_int node);
void rt_insert_helper(struct rb_root * root, rt_entry new_entry, u_int endpoint);
u_int rt_search(struct rb_root *root, u_int value);
u_int rt_search_exclude(struct rb_root *root, u_int value, u_int src);
u_int route_list_helper(routes_list_t * r_list, u_int endpoint);

int vset_bump(u32 *rem);

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
	u_int next;
	unsigned long flags;
	spin_lock_irqsave(&vrr_rt_lock, flags);
	next = rt_search(&rt_root, dest);
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
	return 0;
}

/*
 * Returns the next hop in the routing table, given the destination
 * parameter. Excludes the src from the search. Returns 0 when no route exists.
 */
u_int rt_get_next_exclude(u_int dest, u_int src)
{
	u_int next;
	unsigned long flags;
	spin_lock_irqsave(&vrr_rt_lock, flags);
	next = rt_search_exclude(&rt_root, dest, src);
	spin_unlock_irqrestore(&vrr_rt_lock, flags);

	return next;
}

/*
 * Helper function to search the Red-Black Tree routing table, while excluding the src node
 */
u_int rt_search_exclude(struct rb_root *root, u_int value, u_int src)
{
	struct rb_node *node = root->rb_node;	// top of the tree
	rt_node_t *curr = NULL;
	rt_node_t *prev = NULL;

	while (node) {
		prev = curr;
		curr = rb_entry(node, rt_node_t, node);

		if (curr->route == src && prev)
			return route_list_helper(&prev->routes, value);
		else if (curr->route > value)
			node = node->rb_left;
		else if (curr->route < value)
			node = node->rb_right;
		else {
			if (curr->route == ME)
				return 0;
			return route_list_helper(&curr->routes, value);
		}
	}
	return 0;
}

/* Helper function to search a list of route entries of a particular
 * node, for the next path node with the highest path_id
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
	
	if(get_diff(endpoint,max_entry->route.ea) < 
           get_diff(endpoint,max_entry->route.eb))
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
int rt_add_route(struct routing_table_entry new_entry)
{
        /* TODO: return 0 if there is already an entry with the same
         * ea and path_id */

	if (new_entry.ea) {
		rt_insert_helper(&rt_root, new_entry, new_entry.ea);
	}
	if (new_entry.eb) {
		rt_insert_helper(&rt_root, new_entry, new_entry.eb);
	}
        return 1;
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

	spin_lock_irqsave(&vrr_rt_lock, flags);

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

		list_add(&(tmp->list), &(curr->routes.list));
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
	spin_unlock_irqrestore(&vrr_rt_lock, flags);
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
	unsigned long flags;

	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list){	//check to see if node already exists
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
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

	spin_unlock_irqrestore(&vrr_pset_lock, flags);
	return 1;
}

int pset_remove(u_int node)
{
	pset_list_t * tmp;
	struct list_head * pos, *q;
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each_safe(pos, q, &pset.list) {
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			list_del(pos);
			kfree(tmp);
		}
	}

	spin_unlock_irqrestore(&vrr_pset_lock, flags);
	return 0;
}

u_int pset_get_status(u_int node)
{
	pset_list_t * tmp;
	struct list_head * pos;
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list){	
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
			return tmp->status;
		}
	}
	spin_unlock_irqrestore(&vrr_pset_lock, flags);
	return PSET_UNKNOWN;
}

int pset_update_status(u_int node, u_int newstatus, u_int active)
{
	pset_list_t * tmp;
	struct list_head * pos;
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list){	
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			tmp->status = newstatus;
			tmp->active = active ? 1 : 0;
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&vrr_pset_lock, flags);
	return 0;
}

int pset_lookup_mac(mac_addr mac, u32 *node)
{
        pset_list_t *tmp;
        struct list_head *pos;
        unsigned long flags;
        int ret = 0;

        spin_lock_irqsave(&vrr_pset_lock, flags);

        list_for_each(pos, &pset.list) {
                tmp = list_entry(pos, pset_list_t, list);
                if (!memcmp(mac, tmp->mac, ETH_ALEN)) {
                        *node = tmp->node;
                        ret = 1;
                        goto out;
                }
        }

out:        
        spin_unlock_irqrestore(&vrr_pset_lock, flags);
        return ret;
}

int pset_get_mac(u_int node, mac_addr mac)
{
	pset_list_t * tmp;
	struct list_head * pos;
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list){
		tmp= list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			memcpy(mac, tmp->mac, sizeof(mac_addr));
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
			return 1;
		}
	}
	spin_unlock_irqrestore(&vrr_pset_lock, flags);
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
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list) {
		tmp = list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			atomic_set(&tmp->fail_count, 0);
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
			return 0;
		}
	}
	spin_unlock_irqrestore(&vrr_pset_lock, flags);
	return -1;
}

struct list_head *pset_head()
{
	return &pset.list;
}

int pset_get_proxy(u32 *proxy) {
	u_int active[VRR_PSET_SIZE];
	u_int i = 0, r;
        pset_list_t *tmp;
        struct list_head *pos;
        unsigned long flags;

        spin_lock_irqsave(&vrr_pset_lock, flags);
	list_for_each(pos, &pset.list) {
		tmp = list_entry(pos, pset_list_t, list);
		if (tmp->status == PSET_LINKED && tmp->active) {
			active[i++] = tmp->node;
		}
	}
        spin_unlock_irqrestore(&vrr_pset_lock, flags);
	
	if (!i)
		return 0;

	get_random_bytes(&r, 4);
	r = r % i;
	*proxy = active[r];
	return 1;
}

/*
 * Virtual set functions
 */
int vset_add(u32 node, u32 *rem)
{
	vset_list_t *tmp;
	struct list_head *pos;
        unsigned long flags;

        spin_lock_irqsave(&vrr_vset_lock, flags);

	list_for_each(pos, &vset.list) {
		tmp = list_entry(pos, vset_list_t, list);
		if (tmp->node == node)
                        goto out_err;
	}

        insert_vset_node(node);
        if (!vset_bump(rem))
		goto out_nobump;

        spin_unlock_irqrestore(&vrr_vset_lock, flags);
        return 1;

out_nobump:
	spin_unlock_irqrestore(&vrr_vset_lock, flags);
	return 0;

out_err:
        spin_unlock_irqrestore(&vrr_vset_lock, flags);
        return -1;
}

int cmp_diff(const void *a, const void *b)
{
	if (a < b)
		return -1;
	if (a > b)
		return 1;
	return 0;
}

int vset_bump(u32 *rem) {
	vset_list_t *tmp;
	struct list_head *pos, *q;
        int radius = VRR_VSET_SIZE / 2;
	int i = 0;
	u32 left[vset_size], right[vset_size];

        VRR_DBG("vset_size: %x", vset_size);
	if (vset_size <= VRR_VSET_SIZE)
		return 0;

	/* Fill arrays with diffs in both directions */
        list_for_each(pos, &vset.list) {
                tmp = list_entry(pos, vset_list_t, list);
		left[i] = tmp->diff_left;
		right[i++] = tmp->diff_right;
        }
	
	/* Sort arrays */
	sort(left, vset_size, sizeof(u32), &cmp_diff, NULL);
	sort(right, vset_size, sizeof(u32), &cmp_diff, NULL);
	
	/* Bump the node with left[radius] and right[radius] */
	list_for_each_safe(pos, q, &vset.list) {
		tmp = list_entry(pos, vset_list_t, list);
		if (tmp->diff_left == left[radius] &&
		    tmp->diff_right == right[radius]) {
			*rem = tmp->node;
			list_del(pos);
			kfree(tmp);
			return 1;
		}
	}
	
	VRR_ERR("Our infallible bumping algorithm has failed!");
	return -1;
}

int vset_should_add(u32 node)
{
	vset_list_t *tmp;
	struct list_head *pos;
	u32 left[vset_size], right[vset_size];
	u32 diff_left = (node > ME) ? 
		UINT_MAX - get_diff(node, ME) : get_diff(node, ME);
	u32 diff_right = (node < ME) ?
		UINT_MAX - get_diff(node, ME) : get_diff(node, ME);
	int radius = VRR_VSET_SIZE / 2;
	int i = 0;
	unsigned long flags;

	if (vset_size < VRR_VSET_SIZE)
		return 1;
	
	spin_lock_irqsave(&vrr_vset_lock, flags);
	list_for_each(pos, &vset.list) {
		tmp = list_entry(pos, vset_list_t, list);
		left[i] = tmp->diff_left;
		right[i++] = tmp->diff_right;
	}
	spin_unlock_irqrestore(&vrr_vset_lock, flags);

	sort(left, vset_size, sizeof(u32), &cmp_diff, NULL);
	sort(right, vset_size, sizeof(u32), &cmp_diff, NULL);
	
	for (i = 0; i < radius; i++) {
		if (left[i] > diff_left)
			return 1;
		if (right[i] > diff_right)
			return 1;
	}
	return 0;
}

int vset_remove(u_int node)
{
	vset_list_t * tmp;
	struct list_head * pos, *q;
	unsigned long flags;
	spin_lock_irqsave(&vrr_vset_lock, flags);

	list_for_each_safe(pos, q, &vset.list) {
		tmp= list_entry(pos, vset_list_t, list);
		if (tmp->node == node) {
			list_del(pos);
			kfree(tmp);
			spin_unlock_irqrestore(&vrr_vset_lock, flags);
			return 1;
		}
	}

	spin_unlock_irqrestore(&vrr_vset_lock, flags);
	return 0;
}


//Good way to call this function:
// int current_vset_size;
// u_int * vset_all = NULL;
// current_vset_size = vset_get_all(vset_all);
int vset_get_all(u_int **vset_all)
{
	vset_list_t *tmp;
	struct list_head *pos;
	int i = 0;
	int l_vset_size;
	unsigned long flags;

	spin_lock_irqsave(&vrr_vset_lock, flags);
	l_vset_size = vset_size;

        VRR_DBG("l_vset_size: %x", l_vset_size);
	*vset_all = (u_int *) kmalloc(l_vset_size * sizeof(u_int), GFP_ATOMIC);

	list_for_each(pos, &vset.list){	
		tmp = list_entry(pos, vset_list_t, list);
		(*vset_all)[i++] = tmp->node;
	}
	spin_unlock_irqrestore(&vrr_vset_lock, flags);
	return l_vset_size;
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
        tmp->diff_left = (node > ME) ? 
                UINT_MAX - get_diff(node, ME) : get_diff(node, ME);
        tmp->diff_right = (node < ME) ? 
                UINT_MAX - get_diff(node, ME) : get_diff(node, ME);
	list_add(&(tmp->list), &(vset.list));
	vset_size++;
}

