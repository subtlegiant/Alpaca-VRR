//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>



struct vrr_node {
	 
	struct kobject *kobj;
	int id; //128 bit identifier to match those of IP
	int vset_size; 
	int rtable_value; //the size of the virtual neighborhood
	//*rt = //pointer to the routing table 
    //*pset = //pointer to a pset structure that maintains
		// the states of pset connections
};


struct vrr_packet {
 	
	u_int src; //the current node id
	u_int dst; //the destination id
	u_int payload; //data being sent
    u_int pk_type; //hello message, setup, setup_req etc
};

static ssize_t rtable_show(struct kobject *kobj, struct kobj_attribute *attr
						char *buf)
{
	vrr_node->rtable_value = 3;
	return sprintf(buf, "%du", vrr_node->rtable_value);
}

static struct kobj_attribute vrr_attr = 
		_ATTR(vrr, 0666, rtable_show, NULL);

static struct attribute *attrs[] = {
		&vrr_attr.attr,
		NULL,
};

static struct attribute_group attr_group = {
		.attrs = attrs,
};

//Initialize the module
static int __init vrr_init(void) 
{

	/*1. Initialize an empty routing table
	2. create vrr_node structure
	3. Initialize sysfs hooks ??
	4. Build hello packet and send to establish a proxy 
	5. There is probably alot more than this*/
	int err;
	
	vrr_node->kobj = kobject_create_and_add("vrr", kernel_kobj);
	if(!vrr_node->kobj)
		return -ENOMEM;
	
	err = sysfs_create_group(vrr_node->kobj, &attr_group);
	if(err)
		kobject_put(vrr_node->kobj);
	
	return err;
	
}	 

static void __exit vrr_exit(void)
{
	kobject_put(vrr_node->kobj);
}

MODULE_AUTHOR("Cameron Kidd <cameronk@cs.pdx.edu>");
MODULE_DESCRIPTION("Core for Virtual Ring Routing Protocol");
MODULE_LICENSE("GPL");

module_init(vrr_init);
module_exit(vrr_exit);
