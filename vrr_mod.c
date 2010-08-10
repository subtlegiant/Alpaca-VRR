#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <net/sock.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/hardirq.h>

#include "vrr.h"
#include "vrr_data.h"

static void vrr_timer_tick(unsigned long arg);

/* Defined in af_vrr.c */
extern const struct net_proto_family vrr_family_ops;
extern struct proto vrr_proto;

static struct timer_list vrr_timer =
		TIMER_INITIALIZER (vrr_timer_tick, 0, 0);

static struct packet_type vrr_packet_type __read_mostly = {
	.type = cpu_to_be16(ETH_P_VRR),
	.func = vrr_rcv,
};

static ssize_t id_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf)
{
	u_int value = get_vrr_id();
	return sprintf(buf, "%08x\n", value);
}

static ssize_t pset_show_real(
	struct kobject *kobj, 
	struct kobj_attribute *attr,
	char *buf,
	u_int* (*get_pset_func)(void),
	int (*get_pset_size_func)(void),
	mac_addr* (*get_pset_mac_func)(void),
	int (*get_pset_mac_size_func)(void))
{
	int i;

	// VRR id in pset.
	u_int *vrr_id = get_pset_func();

	// Number of entries in pset.
	int pset_len = get_pset_size_func()/sizeof(u32);

	// Mac address in pset.
	mac_addr *mac = get_pset_mac_func();

	// Max length of one line in the sysfs export.
	//
	// This includes no null, and is calculated thus.
	// length(uint_32)    = 8
	// length(space)      = 1
	// length(mac_addr)   = 17
	// length(newline)    = 1
	// length(line_len)   = length(uint_32 + space + mac_addr + newline)
	//                    = 27
	//
	int line_len = 27;

	// Check precondition: In the pset, there must be a one-to-one mapping
	// between vrr id's and mac addresses.
	if ((get_pset_mac_size_func()/sizeof(mac_addr)) != pset_len) 
		goto exit;

	// Build string to be exported to sysfs.
	for (i = 0; i < pset_len; ++i) {
		// For snprint, n must include trailing null.
		snprintf(buf + line_len * i,
			line_len + 1,
			"%08x %02x:%02x:%02x:%02x:%02x:%02x\n",
			(u32)*(vrr_id),
			(*mac)[0],
			(*mac)[1],
			(*mac)[2],
			(*mac)[3],
			(*mac)[4],
			(*mac)[5]);
		++vrr_id;
		++mac;
	}
	
	return line_len * pset_len + 1; // +1 for trailing null
exit:
	VRR_DBG("Pset mac and id arrays unequal size: BAD!");
	return -1;
}

static ssize_t pset_active_show(
	struct kobject *kobj, 
	struct kobj_attribute *attr,
	char *buf)
{
	return pset_show_real(
			kobj,
			attr,
			buf,
			get_pset_active,
			get_pset_active_size,
			get_pset_active_mac,
			get_pset_active_mac_size);
}

static ssize_t pset_not_active_show(
	struct kobject *kobj, 
	struct kobj_attribute *attr,
	char *buf)
{
	return pset_show_real(
			kobj,
			attr,
			buf,
			get_pset_not_active,
			get_pset_not_active_size,
			get_pset_not_active_mac,
			get_pset_not_active_mac_size);
}

static ssize_t pset_pending_show(
	struct kobject *kobj, 
	struct kobj_attribute *attr,
	char *buf)
{
	return pset_show_real(
			kobj,
			attr,
			buf,
			get_pset_pending,
			get_pset_pending_size,
			get_pset_pending_mac,
			get_pset_pending_mac_size);
}

static ssize_t vset_show (struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	u32 vset_size, *vset;
	int i;

        // u32 = 8
        // \n  = 1
        // line_len = u32 + /n
	int line_len = 9;

	vset_size = vset_get_all(&vset);

	for (i = 0; i < vset_size; ++i) {
		snprintf(buf + line_len * i,
			line_len + 1,
			"%08x\n",
                        (u32)*(vset));
		++vset;
	}

	return line_len * vset_size + 1;
}

static struct kobj_attribute id_attr =
	 __ATTR(id, 0666, id_show, NULL);
static struct kobj_attribute pset_active_attr = 
	 __ATTR(pset_active, 0666, pset_active_show, NULL);
static struct kobj_attribute pset_not_active_attr =
	__ATTR(pset_not_active, 0666, pset_not_active_show, NULL);
static struct kobj_attribute pset_pending_attr =
	__ATTR(pset_pending, 0666, pset_pending_show, NULL);
static struct kobj_attribute vset_attr = 
	__ATTR(vset, 0666, vset_show, NULL);

static struct attribute *attrs[] = {
	&id_attr.attr,
	&pset_active_attr.attr,
	&pset_not_active_attr.attr,
	&pset_pending_attr.attr,
	&vset_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *vrr_obj;

static void vrr_workqueue_handler(struct work_struct *work)
{
        detect_failures();
        active_timeout();
        send_hpkt();
}

static DECLARE_WORK(vrr_workqueue, vrr_workqueue_handler);

/* This function runs with preemption (and possibly interrupts)
 * disabled. It can't run anything that sleeps. */
static void vrr_timer_tick(unsigned long arg)
{
	unsigned long tdelay;

	tdelay = jiffies + (VRR_HPKT_DELAY * HZ / 1000);
	schedule_work(&vrr_workqueue);
	mod_timer(&vrr_timer, tdelay);
}

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

	int err;
	unsigned long tdelay;

	WARN_ATOMIC;

	VRR_INFO("Begin init");

	vrr_node_init();
	vrr_data_init();
	pset_state_init();
	vrr_init_rcv();

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

	//start hello packet timer
	tdelay = jiffies + (VRR_HPKT_DELAY * HZ / 1000);
	mod_timer(&vrr_timer, tdelay);

	VRR_INFO("End init");

 out:
	return err;
}

static void __exit vrr_exit(void)
{
	sock_unregister(AF_VRR);
	dev_remove_pack(&vrr_packet_type);
	del_timer(&vrr_timer);
	/* Cleanup routing/sysfs stuff here */
	kobject_put(vrr_obj);

	proto_unregister(&vrr_proto);
}

MODULE_AUTHOR("Team Alpaca");
MODULE_DESCRIPTION("Core for Virtual Ring Routing Protocol");
MODULE_LICENSE("GPL");

module_init(vrr_init);
module_exit(vrr_exit);
