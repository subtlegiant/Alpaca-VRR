/*
 * VRR		An implementation of the VRR routing protocol.
 *
 *		PF_VRR socket family handler.
 *
 * Authors:	Tad Fisher <tadfisher@gmail.com>
 *		Hunter Haugen <h.haugen@gmail.com>
 *		Cameron Kidd <cameron.kidd@gmail.com>
 *		Russell Nakamura <rhnakamu@gmail.com>
 *		Zachary Pitts <zacharyp@gmail.com>
 *		Chad Versace <chadversace@gmail.com>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/net.h>
#include <linux/skbuff.h>
#include <net/sock.h>

#include "vrr.h"
#include "vrr_data.h"

struct vrr_sock {
	struct sock sk;
	u32 rem_addr;
};

static inline struct vrr_sock *vrr_sk(const struct sock *sk)
{
        return (struct vrr_sock *)sk;
};

#define VRR_HASHSIZE	32
#define VRR_HASHMASK	(VRR_HASHSIZE-1)

static struct	{
	struct hlist_head hlist[VRR_HASHSIZE];
	spinlock_t lock;
} vrr_socks;

void __init vrr_sock_init(void)
{
	unsigned int i;

	for (i = 0; i < VRR_HASHSIZE; i++)
		INIT_HLIST_HEAD(vrr_socks.hlist + i);
	spin_lock_init(&vrr_socks.lock);
}

static struct hlist_head *vrr_hash_list(u32 addr)
{
	return vrr_socks.hlist + (addr & VRR_HASHMASK);
}

struct sock *vrr_find_sock(u32 addr)
{
	struct hlist_node *node;
	struct sock *sknode;
	struct sock *ret = NULL;
	struct hlist_head *head = vrr_hash_list(addr);

	spin_lock_bh(&vrr_socks.lock);

	sk_for_each(sknode, node, head) {
		struct vrr_sock *vsk = vrr_sk(sknode);
		if (vsk->rem_addr != addr)
			continue;
		ret = sknode;
		/* Don't forget to put me back! */
		sock_hold(sknode);
		break;
	}

	spin_unlock_bh(&vrr_socks.lock);

	if (ret)
		VRR_DBG("Found socket for %x", addr);

	return ret;
}

void vrr_hash_sock(struct sock *sk)
{
	u32 addr = vrr_sk(sk)->rem_addr;
	struct hlist_head *head = vrr_hash_list(addr);

	VRR_DBG("Adding socket for %x", addr);

	spin_lock_bh(&vrr_socks.lock);
	sk_add_node(sk, head);
	spin_unlock_bh(&vrr_socks.lock);
	sock_hold(sk);
}

void vrr_unhash_sock(struct sock *sk)
{
	VRR_DBG("Deleting socket");

	spin_lock_bh(&vrr_socks.lock);
	sk_del_node_init(sk);
	spin_unlock_bh(&vrr_socks.lock);
	sock_put(sk);
}

static void vrr_advance_rx_queue(struct sock *sk)
{
	kfree_skb(__skb_dequeue(&sk->sk_receive_queue));
}

static int __vrr_connect(struct sock *sk, struct sockaddr_vrr *addr,
			 int addrlen, long *timeo, int flags)
{
	struct socket *sock = sk->sk_socket;
	int err = -EISCONN;

	if (sock->state == SS_CONNECTED) {
		VRR_INFO("Got a SS_CONNECTED socket");
		goto out;
	}

	err = -EINVAL;
	if (addr == NULL || addrlen != sizeof(struct sockaddr_vrr))
		goto out;
	if (addr->svrr_family != AF_VRR)
		goto out;

	vrr_sk(sk)->rem_addr = addr->svrr_addr;
	sock->state = SS_CONNECTED;

	vrr_hash_sock(sk);
	err = 0;

 out:
	return err;
}

static int vrr_connect(struct socket *sock, struct sockaddr *uaddr,
		       int addrlen, int flags)
{
	struct sockaddr_vrr *addr = (struct sockaddr_vrr *)uaddr;
	struct sock *sk = sock->sk;
	int err;
	long timeo = sock_sndtimeo(sk, flags & O_NONBLOCK);

	VRR_DBG("addr: %x", addr->svrr_addr);

	lock_sock(sk);
	err = __vrr_connect(sk, addr, addrlen, &timeo, 0);
	release_sock(sk);

	return err;
}

static int vrr_recvmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t len, int flags)
{
	struct sock *sk = sock->sk;
	/* struct sockaddr_vrr *svrr = (struct sockaddr_vrr *)msg->msg_name; */
	struct sk_buff *skb;
	struct vrr_header *vh;
	int ret = 0;
	u16 sz = 0;

	VRR_DBG("sock %p sk %p len %zu", sock, sk, len);

	lock_sock(sk);

restart:
	while (skb_queue_empty(&sk->sk_receive_queue)) {
		if (sock->state != SS_CONNECTED) {
			VRR_DBG("not connected");
			ret = -EINTR;
			goto out;
		}
		if (flags & MSG_DONTWAIT) {
			ret = -EWOULDBLOCK;
			goto out;
		}
		release_sock(sk);
		ret = wait_event_interruptible(
			*sk->sk_sleep,
			(!skb_queue_empty(&sk->sk_receive_queue) ||
			 (sock->state != SS_CONNECTED)));
		lock_sock(sk);
		if (ret)
			goto out;
	}

	VRR_DBG("queue length: %u", skb_queue_len(&sk->sk_receive_queue));
	skb = skb_peek(&sk->sk_receive_queue);

	vh = vrr_hdr(skb);
	sz = ntohs(vh->data_len);
	VRR_DBG("sz: %x", sz);

	if (!sz) {
		vrr_advance_rx_queue(sk);
		goto restart;
	}

	if (unlikely(len < sz)) {
		sz = len;
		msg->msg_flags |= MSG_TRUNC;
	}

	if (unlikely(copy_to_user(msg->msg_iov->iov_base,
				  skb->data + sizeof(struct vrr_header),
				  sz))) {
		ret = -EFAULT;
		goto out;
	}
	ret = sz;

	if (likely(!(flags & MSG_PEEK))) {
		VRR_DBG("Advancing rcv queue.");
		vrr_advance_rx_queue(sk);
		VRR_DBG("Done advancing.");
	}

 out:
	release_sock(sk);
	return ret;
}

static int vrr_sendmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t len)
{
	struct sk_buff *skb = NULL;
	struct sockaddr_vrr *dest = (struct sockaddr_vrr *)msg->msg_name;
	struct vrr_packet pkt;
	u32 nh;
	struct vrr_node *me = vrr_get_node();
	size_t sent = 0;
	int ret = -EINVAL;

	if (unlikely(!dest))
		return -EDESTADDRREQ;

	VRR_DBG("dest_addr: %x", dest->svrr_addr);

	if (dest->svrr_addr != me->id) {
		nh = rt_get_next(dest->svrr_addr);
		VRR_DBG("nh: %x", nh);

		if (!nh)
			return -EHOSTUNREACH;

		if (!pset_get_mac(nh, pkt.dest_mac))
			return -EHOSTUNREACH;
	}

	pkt.src = get_vrr_id();
	pkt.dst = dest->svrr_addr;
	pkt.pkt_type = VRR_DATA;
	pkt.data_len = len;

	skb = vrr_skb_alloc(len, GFP_KERNEL);
	if (!skb) {
		VRR_ERR("vrr_skb_alloc failed");
		goto out_err;
	}

	/* Build the vrr header */
	ret = build_header(skb, &pkt);
	if (ret)
		goto out_err;

	/* Copy data from userspace */
	if (memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len)) {
		VRR_ERR("memcpy_fromiovec failed");
		ret = -EFAULT;
		goto out;
	}

	sent += len;

	/* Send packet */
	VRR_DBG("Sending data to %x", dest->svrr_addr);
	vrr_output(skb, me, VRR_DATA);

 out_err:
	kfree_skb(skb);
 out:
	return sent ? sent : ret;
}

static int vrr_backlog_rcv(struct sock *sk, struct sk_buff *skb)
{
	int err = sock_queue_rcv_skb(sk, skb);
	if (err < 0)
		kfree_skb(skb);
	return err ? NET_RX_DROP : NET_RX_SUCCESS;
}

static int vrr_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	VRR_DBG("release");
	sk_common_release(sk);
	return 0;
}

struct proto vrr_proto = {
	.name = "VRR",
	.owner = THIS_MODULE,
	.max_header = VRR_MAX_HEADER,
	.hash = vrr_hash_sock,
	.unhash = vrr_unhash_sock,
	.obj_size = sizeof(struct vrr_sock),
};

static struct proto_ops vrr_proto_ops = {
	.family = PF_VRR,
	.owner = THIS_MODULE,
	.connect = vrr_connect,
	.recvmsg = vrr_recvmsg,
	.release = vrr_release,
	.sendmsg = vrr_sendmsg,
	.mmap = sock_no_mmap,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.listen = sock_no_listen,
};

static int vrr_create(struct net *net, struct socket *sock,
		      int protocol)
{
	struct sock *sk;

	VRR_INFO("Begin vrr_create");
	VRR_DBG("proto: %u", protocol);

	if (unlikely(protocol != 0))
		return -EPROTONOSUPPORT;

        if (sock->type != SOCK_RAW)
		return -ESOCKTNOSUPPORT;

	sk = sk_alloc(net, PF_VRR, GFP_KERNEL, &vrr_proto);
	if (sk == NULL)
		return -ENOMEM;

	sock->ops = &vrr_proto_ops;
	sock->state = SS_UNCONNECTED;

	sock_init_data(sock, sk);

	sk->sk_backlog_rcv = vrr_backlog_rcv;
	sk->sk_family = PF_VRR;
	sk->sk_protocol = protocol;

	sock_hold(sk);

	VRR_INFO("End vrr_create");

	return 0;
}

const struct net_proto_family vrr_family_ops = {
	.family = PF_VRR,
	.create = vrr_create,
	.owner = THIS_MODULE,
};
