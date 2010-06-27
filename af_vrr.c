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

static int vrr_create(struct net *net, struct socket *sock, int protocol,
		      int kern);

int vrr_recvmsg(struct kiocb *iocb, struct socket *sock,
		struct msghdr *msg, size_t size, int flags)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int rc;

	VRR_DBG("sock %p sk %p len %zu", sock, sk, size);

	skb = skb_recv_datagram(sk, flags & ~MSG_DONTWAIT,
				flags & MSG_DONTWAIT, &rc);
	if (!skb) {
		goto out;
	}

 out:
	return rc;
}

int vrr_sendmsg(struct kiocb *iocb, struct socket *sock,
		struct msghdr *msg, size_t len)
{
	struct sock *sk = sock->sk;
	struct sk_buff *skb;
	int sent = 0;

	if (sk->sk_shutdown & SEND_SHUTDOWN) {
		return -EPIPE;
	}

	/* Send packet */
	/* vrr_ouput(skb); */
	VRR_DBG("sock %p, sk %p", sock, sk);
}

struct proto vrr_prot = {
	.name 		= "VRR",
	.owner 		= THIS_MODULE,
	/* .close               = vrr_close, */
	/* .destroy     = vrr_destroy, */
	/* .connect     = vrr_connect, */
	/* .disconnect  = vrr_disconnect, */
	/* .ioctl               = vrr_ioctl, */
	/* .init                = vrr_init, */
	/* .setsockopt  = vrr_setsockopt, */
	/* .getsockopt  = vrr_getsockopt, */
	/* .sendmsg     = vrr_sendmsg, */
	/* .recvmsg     = vrr_recvmsg, */
	/* .bind                = vrr_bind, */
        .obj_size	= sizeof(struct vrr_sock),
};

static struct proto_ops vrr_sock_ops = {
	.family = PF_VRR,
	.owner = THIS_MODULE,
	/* .release     = vrr_release, */
	/* .bind                = vrr_bind, */
	/* .connect     = vrr_connect, */
	.socketpair = sock_no_socketpair,
	.accept = sock_no_accept,
	/* .getname     = vrr_getname, */
	/* .poll                = datagram_poll, */
	/* .ioctl               = vrr_ioctl, */
	.listen = sock_no_listen,
	/* .shutdown    = vrr_shutdown, */
	/* .setsockopt  = sock_common_setsockopt, */
	/* .getsockopt  = sock_common_getsockopt, */
	.sendmsg = vrr_sendmsg,
	.recvmsg = vrr_recvmsg,
	.mmap = sock_no_mmap,
	/* .sendpage    = vrr_sendpage, */
};

const struct net_proto_family vrr_family_ops = {
	.family = PF_VRR,
	.create = vrr_create,
	.owner = THIS_MODULE,
};

static int vrr_create(struct net *net, struct socket *sock, int protocol,
		      int kern)
{
	struct sock *sk;
        struct vrr_sock *vrr;
	int err;

	VRR_INFO("Begin vrr_create");
	VRR_DBG("proto: %u", protocol);

	err = -ENOBUFS;
	sk = sk_alloc(net, PF_VRR, GFP_KERNEL, &vrr_prot);
	if (!sk) {
		goto out;
	}
	err = 0;

        vrr = vrr_sk(sk);

	if (sock) {
		sock->ops = &vrr_sock_ops;
	}
	sock_init_data(sock, sk);

	/* sk->sk_destruct      = vrr_destruct; */
	sk->sk_family = PF_VRR;
	sk->sk_protocol = protocol;
	//sk->sk_allocation = gfp;
	VRR_INFO("End vrr_create");
 out:
	return err;
}
