#ifndef _VRR_DATA_H
#define _VRR_DATA_H

#include <linux/types.h>

#define PSET_LINKED	0
#define PSET_PENDING	1
#define PSET_FAILED	2
#define PSET_UNKNOWN	3

//Struct for use in VRR Routing Table
typedef struct routing_table_entry {
	u32 ea;		//endpoint A
	u32 eb;		//endpoint B
	u32 na;		//next A
	u32 nb;		//next B
	int path_id;		//Path ID
	struct list_head list;
} rt_entry;

//Physical Set Setup
typedef struct pset_list {
	struct list_head	list;
	u_int			node;
	u_int			status;
        u_int			active;
	mac_addr		mac;
        atomic_t		fail_count;
} pset_list_t;

/*
 * Routes, Pset, and Vset initialization
 * vrr_data_init : Call once before using any of the other functions
 */
void vrr_data_init(void);


/* Routing Table functions:
 * rt_get_next : Get next closest hop given destination as parameter.
 * rt_add_route : Adds a route to the Routing Table.
 * rt_remove_nexts : Given a 'NextA' hop, remove all entries in the table
 *	that use that node
 * rt_remove_route : deletes a route form the Routing Table.
 */
u_int rt_get_next(u_int dest);
u_int rt_get_next_exclude(u_int dest, u_int src);
int rt_add_route(u32 ea, u32 eb, u32 na, u32 nb, u32 path_id);
int rt_remove_nexts(u_int route_hop_to_remove);
rt_entry* rt_remove_route(u32 ea, u32 path_id);

/* Functions for physical set of nodes, and also their current state (linked, active or pending)
 * pset_add : Add a node to the physical set.  Returns 1 on success,
 *	0 on failure
 * pset_remove : Remove a node from the physical set.  Returns 1 on success,
 *	0 on failure
 * pset_get_status : Gets current status of physical node.  Returns status or
 *	PSET_UNKNOWN
 * pset_get_mac: Gets current status of physical node.  Uses the passed pointer
 *	to mac for the data.  Returns 1 on success, 0 on failure.
 * pset_update_status : Updates node with a new status.
 */
int pset_add(u_int node, const mac_addr mac, u_int status, u_int active);
int pset_remove(u_int node);
u_int pset_get_status(u_int node);
int pset_lookup_mac(mac_addr mac, u32 *node);
int pset_get_mac(u_int node, mac_addr mac);
int pset_get_active(u32 node);
int pset_update_status(u_int node, u_int new_status, u_int active);
int pset_inc_fail_count(struct pset_list *node);
int pset_reset_fail_count(u_int node);
struct list_head *pset_head(void);
int pset_get_proxy(u32 *proxy);
int pset_contains(u32 id);

/* Functions for virtual set of nodes
 * vset_add : Adds a node to the virtual set.  Returns 0 on failure
 * vset_remove : Remove a node from the virtual set.  Returns 1 on success,
 *	0 on failure
 * vset_get_all : pass in an array of size of the vset, and this function will
 *	populate it with all the vset nodes.  Returns the size of the array.
 */
int vset_add(u32 node, u32 *rem);
int vset_should_add(u32 node);
int vset_remove(u_int node);
int vset_get_all(u_int **vset_all);

#endif	/* _VRR_DATA_H */
