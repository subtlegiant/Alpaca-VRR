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
#include <net/sock.h>

#include "vrr.h"

static int vrr_create(struct net *net, struct socket *sock, int protocol,
		      int kern);

static const struct proto vrr_prot = {
	.name = "VRR",
	.owner = THIS_MODULE,
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
};

static const struct proto_ops vrr_sock_ops = {
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
	/* .sendmsg     = vrr_sendmsg, */
	.recvmsg = sock_common_recvmsg,
	.mmap = sock_no_mmap,
	/* .sendpage    = vrr_sendpage, */
};

static const struct net_proto_family vrr_family_ops = {
	.family = PF_VRR,
	.create = vrr_create,
	.owner = THIS_MODULE,
};

static int vrr_create(struct net *net, struct socket *sock, int protocol,
		      int kern)
{
	struct sock *sk;
	int err;

	sock->ops = &vrr_sock_ops;

	err = -ENOBUFS;
	sk = sk_alloc(net, PF_VRR, GFP_KERNEL, &vrr_prot);
	if (sk == NULL)
		goto out;

	err = 0;

	sock_init_data(sock, sk);

	/* sk->sk_destruct      = vrr_sock_destruct; */
	sk->sk_protocol = protocol;

	sk_refcnt_debug_inc(sk);

	if (sk->sk_prot->init) {
		err = sk->sk_prot->init(sk);
		if (err)
			sk_common_release(sk);
	}
 out:
	return err;
}
