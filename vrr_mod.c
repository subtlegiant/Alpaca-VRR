#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <net/sock.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>

#include "vrr.h"
#include "vrr_data.h"

#define MS_TO_NS(x)	(x * 1E6L)

/* Defined in af_vrr.c */
extern const struct net_proto_family vrr_family_ops;
extern struct proto vrr_proto;
static struct hrtimer hr_timer;

static struct packet_type vrr_packet_type __read_mostly = {
	.type = cpu_to_be16(ETH_P_VRR),
	.func = vrr_rcv,
};

//static struct vrr_node *node;
static ssize_t id_show(struct kobject *kobj, struct kobj_attribute *attr,
		       char *buf)
{
	int value = 4;
	return sprintf(buf, "%d", value);
}

static struct kobj_attribute vrr_attr = __ATTR(id, 0666, id_show, NULL);

static struct attribute *attrs[] = {
	&vrr_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *vrr_obj;

//Initialize the module
static int __init vrr_init(void)
{
	/* 1. Initialize an empty routing table
	   2. create empty pset (tree/CLL?)
	   3. create empty vset (tree/CLL?)
	   4. create vrr_node structure
	   5. Initialize sysfs hooks ??
	   6. Build hello packet and send to establish a proxy 
	   7. There is probably alot more than this */

	int err = 0;
	//node->id = 3;

	/*if(!(node = kzalloc(sizeof(struct vrr_node), GFP_KERNEL)));
	   return -ENOMEM; */

	//start hello packet timer
	ktime_t ktime;
        unsigned long delay = 1000L;

	ktime = ktime_set(0, MS_TO_NS(delay));
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
        hr_timer.function = &send_hpkt;
	hrtimer_start(&hr_timer, ktime, HRTIMER_MODE_REL); 

	VRR_INFO("Begin init");

	vrr_data_init();
	vrr_node_init();
	pset_state_init();

	err = proto_register(&vrr_proto, 1);
	if (err) {
		goto out;
	}

	/* Initialize routing/sysfs stuff here */
	/* TODO: Split these into separate functions */
	vrr_obj = kobject_create_and_add("vrr", kernel_kobj);
	if (!vrr_obj) {
		err = -ENOMEM;
		goto out;
	}

	err = sysfs_create_group(vrr_obj, &attr_group);
	if (err) {
		kobject_put(vrr_obj);
	}
	/* --- */

	/* Register our sockets protocol handler */
	err = sock_register(&vrr_family_ops);
	if (err) {
		goto out;
	}

	dev_add_pack(&vrr_packet_type);

	VRR_INFO("End init");

 out:
	return err;
}

static void __exit vrr_exit(void)
{
	sock_unregister(AF_VRR);
	dev_remove_pack(&vrr_packet_type);

	/* Cleanup routing/sysfs stuff here */
	kobject_put(vrr_obj);

	proto_unregister(&vrr_proto);
}

MODULE_AUTHOR("Cameron Kidd <cameronk@cs.pdx.edu>");
MODULE_DESCRIPTION("Core for Virtual Ring Routing Protocol");
MODULE_LICENSE("GPL");

module_init(vrr_init);
module_exit(vrr_exit);
