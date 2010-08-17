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
/* typedef struct routes_list { */
/*	rt_entry		route; */
/*	struct list_head	list; */
/* } routes_list_t; */

typedef struct rt_node {
	struct rb_node	node;
	u32		endpoint;
	rt_entry	routes;
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
static rt_node_t *rt_find_insert_node(struct rb_root *root, u32 endpoint);
u_int rt_search(struct rb_root *root, u32 endpoint);
rt_entry *rt_search_rmv(struct rb_root *root, u32 endpoint, u32 path_id);
u_int rt_search_exclude(struct rb_root *root, u32 endpoint, u32 src);
u_int route_list_search(rt_entry* r_list, u32 endpoint);
rt_entry *route_list_search_rmv(rt_entry *r_list, u32 endpoint, u32 path_id);

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
u_int rt_get_next(u32 dest)
{
	u32 next;
	unsigned long flags;

	spin_lock_irqsave(&vrr_rt_lock, flags);
	next = rt_search(&rt_root, dest);
	spin_unlock_irqrestore(&vrr_rt_lock, flags);

	return next;
}

/*
 * Helper function to search the Red-Black Tree routing table
 */
u32 rt_search(struct rb_root *root, u32 endpoint)
{
	struct rb_node *node = root->rb_node;	// top of the tree

	while (node) {
		rt_node_t *this = rb_entry(node, rt_node_t, node);

		if (endpoint < this->endpoint)
			node = node->rb_left;
		else if (endpoint > this->endpoint)
			node = node->rb_right;
		else {
			if (this->endpoint == ME)
				return 0;
			return route_list_search(&this->routes, endpoint);
		}
	}
	return 0;
}

rt_entry *rt_search_rmv(struct rb_root *root, u32 endpoint, u32 path_id)
{
	struct rb_node *node = root->rb_node;	// top of the tree

	while (node) {
		rt_node_t *this = rb_entry(node, rt_node_t, node);

		if (endpoint < this->endpoint)
			node = node->rb_left;
		else if (endpoint > this->endpoint)
			node = node->rb_right;
		else
			return route_list_search_rmv(&this->routes, endpoint, path_id);

	}
	return NULL;
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
u_int rt_search_exclude(struct rb_root *root, u32 endpoint, u32 src)
{
	struct rb_node *node = root->rb_node;	// top of the tree
	rt_node_t *this = NULL;
	rt_node_t *prev = NULL;

	while (node) {
		prev = this;
		this = rb_entry(node, rt_node_t, node);

		if (this->endpoint == src && prev)
			return route_list_search(&prev->routes, endpoint);
		else if (endpoint < this->endpoint)
			node = node->rb_left;
		else if (endpoint > this->endpoint)
			node = node->rb_right;
		else {
			if (this->endpoint == ME)
				return 0;
			return route_list_search(&this->routes, endpoint);
		}
	}
	return 0;
}

/* Helper function to search a list of route entries of a particular
 * node, for the next path node with the highest path_id
 */
u_int route_list_search(rt_entry *r_list, u32 endpoint)
{
	rt_entry *tmp = NULL;
	rt_entry *max_entry = NULL;
	struct list_head *pos;
	u32 max_path = 0;

	list_for_each(pos, &r_list->list){
		tmp = list_entry(pos, rt_entry, list);
		if (tmp->path_id > max_path) {
			max_entry = tmp;
			max_path = tmp->path_id;
		}
	}

	if (!max_entry) {
		/* TODO: Should take the node as a pointer argument,
		 * since 0 is a valid ID. */
		return 0;
	}

	if(get_diff(endpoint, max_entry->ea) <
	   get_diff(endpoint, max_entry->eb))
		if (max_entry->ea != ME)
			return max_entry->na;
		else
			return max_entry->nb;
	else
		if (max_entry->eb != ME)
			return max_entry->nb;
		else
			return max_entry->na;
	return 0;
}

rt_entry* route_list_search_rmv(rt_entry *r_list, u32 endpoint, u32 path_id)
{
	rt_entry *tmp = NULL;
	struct list_head *pos, *q;
	unsigned long flags;

	spin_lock_irqsave(&vrr_rt_lock, flags);

	list_for_each_safe(pos, q, &r_list->list){
		tmp = list_entry(pos, rt_entry, list);
		if (tmp->path_id == path_id) {
			list_del(&tmp->list);
			spin_unlock_irqrestore(&vrr_rt_lock, flags);
			return tmp;
		}
	}

	spin_unlock_irqrestore(&vrr_rt_lock, flags);
	return NULL;
}

/**
 * Adds a route to the rb tree.	 Might add two different entries if there
 * are both ea and eb values for the route.
 */
int rt_add_route(u32 ea, u32 eb, u32 na, u32 nb, u32 path_id)
{
	rt_node_t *insert = NULL;
	rt_entry *route = NULL;
	int ret = 1;
	unsigned long flags;

	/* TODO: return 0 if there is already an entry with the same
	 * ea and pid */


	spin_lock_irqsave(&vrr_rt_lock, flags);

	if (ea) {
		/* rt_insert_helper(&rt_root, new_entry, new_entry->ea); */
		insert = rt_find_insert_node(&rt_root, ea);
		if (!insert)
			goto out_err;
		route = (rt_entry *) kmalloc(sizeof(rt_entry), GFP_ATOMIC);
		if (!route)
			goto out_err;
		route->ea = ea;
		route->eb = eb;
		route->na = na;
		route->nb = nb;
		route->path_id = path_id;
		list_add(&(route->list), &(insert->routes.list));
	}
	if (eb) {
		insert = rt_find_insert_node(&rt_root, eb);
		if (!insert)
			goto out_err;
		route = (rt_entry *) kmalloc(sizeof(rt_entry), GFP_ATOMIC);
		if (!route)
			goto out_err;
		route->ea = ea;
		route->eb = eb;
		route->na = na;
		route->nb = nb;
		route->path_id = path_id;
		list_add(&(route->list), &(insert->routes.list));
	}
	goto out;

out_err:
	ret = 0;
out:
	spin_unlock_irqrestore(&vrr_rt_lock, flags);
	return ret;
}

/**
 * Find the node to insert the route. Returns node to add route
 * to. Must hold vrr_rt_lock prior to calling.
 */
static rt_node_t *rt_find_insert_node(struct rb_root *root, u32 endpoint)
{
	struct rb_node **new = &(root->rb_node);
	struct rb_node *parent = NULL;
	rt_node_t *this = NULL;
	rt_node_t *new_node = NULL;
	int cmp;

	while (*new)
	{
		this = rb_entry(*new, rt_node_t, node);
		cmp = endpoint < this->endpoint ? -1 :
			endpoint > this->endpoint ? 1 : 0;
		parent = *new;

		if (cmp < 0)
			new = &((*new)->rb_left);
		else if (cmp > 0)
			new = &((*new)->rb_right);

else {
			return this;
		}
	}

	/* Node doesn't exist, let's create one. */
	new_node = (rt_node_t *)kmalloc(sizeof(rt_node_t), GFP_ATOMIC);
	if (!new_node)
		return NULL;
	new_node->endpoint = endpoint;
	INIT_LIST_HEAD(&(new_node->routes.list));

	/* Insert the node */
	rb_link_node(&new_node->node, parent, new);
	rb_insert_color(&new_node->node, root);
	return new_node;
}


int rt_remove_nexts(u_int route_hop_to_remove)	//TODO: code this
{
	return 0;
}

rt_entry* rt_remove_route(u32 ea, u32 path_id)
{
	return(rt_search_rmv(&rt_root, ea, path_id));
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

	list_for_each(pos, &pset.list){
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
	atomic_set(&tmp->fail_count, 0);
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

int pset_get_active(u32 node)
{
	pset_list_t *tmp;
	struct list_head *pos;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&vrr_pset_lock, flags);
	list_for_each(pos, &pset.list) {
		tmp = list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			ret = tmp->active;
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
	VRR_DBG("Incrementing fail count for %x", node->node);
	atomic_inc(&node->fail_count);
	return atomic_read(&node->fail_count);
}

int pset_reset_fail_count(u32 node)
{
	pset_list_t *tmp;
	struct list_head *pos;
	unsigned long flags;
	spin_lock_irqsave(&vrr_pset_lock, flags);

	list_for_each(pos, &pset.list) {
		tmp = list_entry(pos, pset_list_t, list);
		if (tmp->node == node) {
			VRR_DBG("Resetting fail count for %x", node);
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

int pset_contains(u32 id)
{
	pset_list_t *tmp;
	unsigned long flags;
	struct list_head *pos;

	spin_lock_irqsave(&vrr_pset_lock, flags);
	list_for_each(pos, &pset.list) {
		tmp = list_entry(pos, pset_list_t, list);
		if (tmp->node == id) {
			spin_unlock_irqrestore(&vrr_pset_lock, flags);
			return 1;
		}
	}

	spin_unlock_irqrestore(&vrr_pset_lock, flags);

	return 0;
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

	spin_lock_irqsave(&vrr_vset_lock, flags);

	list_for_each(pos, &vset.list) {
		tmp = list_entry(pos, vset_list_t, list);
		if (tmp->node == node) {
			spin_unlock_irqrestore(&vrr_vset_lock, flags);
			return 0;
		}
	}

	if (vset_size < VRR_VSET_SIZE) {
		spin_unlock_irqrestore(&vrr_vset_lock, flags);
		return 1;
	}

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
