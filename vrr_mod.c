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
	return sprintf(buf, "%u\n", value);
}

static ssize_t pset_active_show(struct kobject *kobj, 
				struct kobj_attribute *attr,
				char *buf)
{
	int *pset_a;
	mac_addr *pset_a_mac;
	int pset_a_size, pset_a_mac_size;
	int i;
	int buf_size;
	int pset_a_len;
	char *temp_buf;

	pset_a = get_pset_active();
	pset_a_size = get_pset_active_size();
	pset_a_mac = get_pset_active_mac();
	pset_a_mac_size = get_pset_active_mac_size();
	buf_size = pset_a_size + pset_a_mac_size + 3;
	pset_a_len = pset_a_size/sizeof(u32);
	temp_buf = kmalloc(buf_size, GFP_KERNEL);

	if (!temp_buf) 
		goto nem; //not enough memory

	// each mac address is mapped to a pset id by
	// index, therefore they should and have to be
	// the same size
	if ((pset_a_mac_size/sizeof(mac_addr)) != pset_a_len) 
		goto exit;

	for (i=0; i < pset_a_len; ++i) {
		sprintf(temp_buf, "%d %02x:%02x:%02x:%02x:%02x:%02x\n",
			(u32)*(pset_a),
			(*pset_a_mac)[0],
			(*pset_a_mac)[1],
			(*pset_a_mac)[2],
			(*pset_a_mac)[3],
			(*pset_a_mac)[4],
			(*pset_a_mac)[5]);
		++pset_a;
		++pset_a_mac;
		buf = strncat(buf, temp_buf, buf_size);
	}
	
	kfree(temp_buf);
	return pset_a_len * buf_size;
exit:
	kfree(temp_buf);
	VRR_DBG("Pset mac and id arrays unequal size: BAD!");
	return -1;

nem:
	return -ENOMEM;
}
				   
static ssize_t pset_not_active_show(struct kobject *kobj, 
				struct kobj_attribute *attr,
				char *buf)
{
	int *pset_na;
	mac_addr *pset_na_mac;
	int pset_na_size, pset_na_mac_size;
	int i;
	int buf_size;
	int pset_na_len;
	char *temp_buf;

	pset_na = get_pset_not_active();
	pset_na_size = get_pset_not_active_size();
	pset_na_mac = get_pset_not_active_mac();
	pset_na_mac_size = get_pset_not_active_mac_size();
	buf_size = pset_na_size + pset_na_mac_size + 3;
	pset_na_len = pset_na_size/sizeof(u32);
	temp_buf = kmalloc(buf_size, GFP_KERNEL);

	if (!temp_buf) 
		goto nem; //not enough memory

	// each mac address is mapped to a pset id by
	// index, therefore they should and have to be
	// the same size
	if ((pset_na_mac_size/sizeof(mac_addr)) != pset_na_len) 
		goto exit;

	for (i=0; i < pset_na_len; ++i) {
		sprintf(temp_buf, "%d %02x:%02x:%02x:%02x:%02x:%02x\n",
			(u32)*(pset_na),
			(*pset_na_mac)[0],
			(*pset_na_mac)[1],
			(*pset_na_mac)[2],
			(*pset_na_mac)[3],
			(*pset_na_mac)[4],
			(*pset_na_mac)[5]);
		++pset_na;
		++pset_na_mac;
		buf = strncat(buf, temp_buf, buf_size);
	}
	
	kfree(temp_buf);
	return pset_na_len * buf_size;
exit:
	kfree(temp_buf);
	VRR_DBG("Pset mac and id arrays unequal size: BAD!");
	return -1;

nem:
	return -ENOMEM;
}

static ssize_t pset_pending_show(struct kobject *kobj, 
				struct kobj_attribute *attr,
				char *buf)
{
	int *pset_p;
	mac_addr *pset_p_mac;
	int pset_p_size, pset_p_mac_size;
	int i;
	int buf_size;
	int pset_p_len;
	char *temp_buf;

	pset_p = get_pset_pending();
	pset_p_size = get_pset_pending_size();
	pset_p_mac = get_pset_pending_mac();
	pset_p_mac_size = get_pset_pending_mac_size();
	buf_size = pset_p_size + pset_p_mac_size + 3;
	pset_p_len = pset_p_size/sizeof(u32);
	temp_buf = kmalloc(buf_size, GFP_KERNEL);

	if (!temp_buf) 
		goto nem; //not enough memory

	// each mac address is mapped to a pset id by
	// index, therefore they should and have to be
	// the same size
	if ((pset_p_mac_size/sizeof(mac_addr)) != pset_p_len) 
		goto exit;

	for (i=0; i<pset_p_len; ++i) {
		sprintf(temp_buf, "%d %02x:%02x:%02x:%02x:%02x:%02x\n",
			(u32)*(pset_p),
			(*pset_p_mac)[0],
			(*pset_p_mac)[1],
			(*pset_p_mac)[2],
			(*pset_p_mac)[3],
			(*pset_p_mac)[4],
			(*pset_p_mac)[5]);
		++pset_p;
		++pset_p_mac;
		buf = strncat(buf, temp_buf, buf_size);
	}
	
	kfree(temp_buf);
	return pset_p_len * buf_size;
exit:
	kfree(temp_buf);
	VRR_DBG("Pset mac and id arrays unequal size: BAD!");
	return -1;

nem:
	return -ENOMEM;
}

static struct kobj_attribute id_attr =
	 __ATTR(id, 0666, id_show, NULL);
static struct kobj_attribute pset_active_attr = 
	 __ATTR(pset_active, 0666, pset_active_show, NULL);
static struct kobj_attribute pset_not_active_attr =
	__ATTR(pset_not_active, 0666, pset_not_active_show, NULL);
static struct kobj_attribute pset_pending_attr =
	__ATTR(pset_pending, 0666, pset_pending_show, NULL);



static struct attribute *attrs[] = {
	&id_attr.attr,
	&pset_active_attr.attr,
	&pset_not_active_attr.attr,
	&pset_pending_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *vrr_obj;

static void vrr_workqueue_handler(struct work_struct *work)
{
	send_hpkt();
	detect_failures();
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

	//start hello packet timer
	tdelay = jiffies + VRR_HPKT_DELAY;

	VRR_INFO("Begin init");

	vrr_node_init();
	vrr_data_init();
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
