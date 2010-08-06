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

static int __vrr_connect(struct sock *sk, struct sockaddr_vrr *addr,
			 int addrlen, long *timeo, int flags)
{
	struct socket *sock = sk->sk_socket;
	int err = -EISCONN;

	if (sock->state == SS_CONNECTED) {
		VRR_INFO("Got a SS_CONNECTED socket");
		goto out;
	}

	if (sock->state == SS_CONNECTING) {
		err = 0;
		VRR_INFO("Got a SS_CONNECTING socket. Write a sleep function.");
		goto out;
	}

	err = -EINVAL;
	if (addr == NULL || addrlen != sizeof(struct sockaddr_vrr))
		goto out;
	if (addr->svrr_family != AF_VRR)
		goto out;

	sock->state = SS_CONNECTED;
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

	lock_sock(sk);
	err = __vrr_connect(sk, addr, addrlen, &timeo, 0);
	release_sock(sk);

	return err;
}

static int vrr_recvmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t len, int flags)
{
	struct sock *sk = sock->sk;
	struct sockaddr_vrr *svrr = (struct sockaddr_vrr *)msg->msg_name;
	struct sk_buff *skb;
	size_t copied = 0;
	int err = -EOPNOTSUPP;

	VRR_DBG("sock %p sk %p len %zu", sock, sk, len);

	/* Pull skb from sk->sk_receive_queue */
	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &err);
	if (!skb)
		goto out;

	copied = skb->len;
	if (len < copied) {
		msg->msg_flags |= MSG_TRUNC;
		copied = len;
	}

	err = skb_copy_datagram_iovec(skb, 0, msg->msg_iov, copied);
	if (err)
		goto done;
 
	if (svrr) {
		svrr->svrr_family = AF_VRR;
		svrr->svrr_addr = vrr_hdr(skb)->dest_id;
		memset(&svrr->svrr_zero, 0, sizeof(svrr->svrr_zero));
	}
	if (flags & MSG_TRUNC)
		copied = skb->len;
 done:
	skb_free_datagram(sk, skb);
 out:
	if (err)
		return err;
	return copied;
}

void vrr_destroy_sock(struct sock *sk)
{
	if (sk->sk_socket) {
		if (sk->sk_socket->state != SS_UNCONNECTED)
			sk->sk_socket->state = SS_DISCONNECTING;
	}
}

static int vrr_sendmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t len)
{
	struct sk_buff *skb = NULL;
	struct sock *sk = sock->sk;
	struct sockaddr_vrr *dest = (struct sockaddr_vrr *)msg->msg_name;
	struct vrr_packet pkt;
	u32 nh;
	struct vrr_node *me = vrr_get_node();
	size_t sent = 0;
	int ret = -EINVAL;

	if (unlikely(!dest))
		return -EDESTADDRREQ;

	if (dest->svrr_addr != me->id) {
		nh = rt_get_next(dest->svrr_addr);
		if (!nh)
			return -EHOSTUNREACH;

		if (!pset_get_mac(nh, pkt.dest_mac))
			return -EHOSTUNREACH;
	}

	if (iocb)
		lock_sock(sk);

	pkt.src = get_vrr_id();
	pkt.dst = dest->svrr_addr;
	pkt.pkt_type = VRR_DATA;
	pkt.data_len = len;

	/* Allocate an skb for sending */
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
	vrr_output(skb, me, VRR_DATA);

 out_err:
	kfree_skb(skb);
 out:
	if (iocb)
		release_sock(sk);
	return sent ? sent : ret;
}

static int vrr_backlog_rcv(struct sock *sk, struct sk_buff *skb)
{
	return 0;
}

static int vrr_release(struct socket *sock)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;

	if (sk == NULL)
		return 0;

	lock_sock(sk);

	while ((skb = __skb_dequeue(&sk->sk_receive_queue)))
		kfree_skb(skb);

	sock->state = SS_UNCONNECTED;
	release_sock(sk);

	sock_put(sk);
	sock->sk = NULL;

	return 0;
}

struct proto vrr_proto = {
	.name = "VRR",
	.owner = THIS_MODULE,
	.max_header = VRR_MAX_HEADER,
	.obj_size = sizeof(struct sock),
};

static struct proto_ops vrr_proto_ops = {
	.family = PF_VRR,
	.owner = THIS_MODULE,
	.connect = vrr_connect,
	.recvmsg = vrr_recvmsg,
	.release = vrr_release,
	.sendmsg = vrr_sendmsg,
	/* .bind                = vrr_bind, */
	/* .getname     = vrr_getname, */
	/* .poll                = datagram_poll, */
	/* .ioctl               = vrr_ioctl, */
	/* .shutdown    = vrr_shutdown, */
	/* .setsockopt  = sock_common_setsockopt, */
	/* .getsockopt  = sock_common_getsockopt, */
	.mmap = sock_no_mmap,
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	.listen = sock_no_listen,
	/* .sendpage            = vrr_sendpage, */
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
	sock->state = SS_CONNECTING;

	sock_init_data(sock, sk);

	sk->sk_rcvtimeo = msecs_to_jiffies(VRR_FAIL_TIMEOUT * VRR_HPKT_DELAY);
	sk->sk_backlog_rcv = vrr_backlog_rcv;
	sk->sk_family = PF_VRR;
	sk->sk_protocol = protocol;

	VRR_INFO("End vrr_create");

	return 0;
}

const struct net_proto_family vrr_family_ops = {
	.family = PF_VRR,
	.create = vrr_create,
	.owner = THIS_MODULE,
};
