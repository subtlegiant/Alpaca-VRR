
/*
 * Routes, Pset, and Vset initialization
 * vrr_data_init : Call once before using any of the other functions
 */
void vrr_data_init(void);


/* Routing Table functions:
 */
/*
 * rt_get_next : Get next hop given destination as parameter
 * rt_add_route : Adds a route to the Routing Table, returns 0 on failure, 1 on success
 * rt_remove_nexts : Given a 'NextA' hop, remove all entries in the table that use that node
 */
u_int rt_get_next(u_int dest);
int rt_add_route(struct routing_table_entry new_entry);
int rt_remove_nexts(u_int route_hop_to_remove);


/*
 * Functions for physical set of nodes, and also their current state (linked, active or pending)
 * pset_add : Add a node to the physical set.  Returns 1 on success, 0 on failure
 * pset_remove : Remove a node from the physical set.  Returns 1 on success, 0 on failure
 * pset_get_status: Gets current status of physical node.  Returns status
 */
int pset_add(u_int node);
int pset_remove(u_int node);
int pset_get_status(u_int node);

/*
 * Functions for virtual set of nodes
 */
