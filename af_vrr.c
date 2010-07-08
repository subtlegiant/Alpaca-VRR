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

static int __vrr_connect(struct sock *sk, struct sockaddr_vrr *addr,
			 int addrlen, long *timeo, int flags)
{
	struct socket *sock = sk->sk_socket;
	struct vrr_sock *vrr = vrr_sk(sk);
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
<<<<<<< HEAD
	/*if (addr == NULL || addrlen != sizeof(sockaddr_vrr))
	   goto out;
	   /*if (addr->svrr_family != AF_VRR)
	   goto out; */

	/* vrr->src = get_vrr_id();
	   vrr->dest_addr = addr->svrr_addr; */
=======
	if (addr == NULL || addrlen != sizeof(struct sockaddr_vrr))
		goto out;
	if (addr->svrr_family != AF_VRR)
		goto out;

        err = 0;
	vrr->src_addr = get_vrr_id();
	vrr->dest_addr = addr->svrr_addr;
>>>>>>> f6d5f28eae4dcbd109d94a283d3f8999fee6d900

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
	struct vrr_sock *vrr = vrr_sk(sk);
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

        sock_recv_ts_and_drops(msg, sk, skb);

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
	sock_put(sk);
}

static int vrr_release(struct socket *sock)
{
	struct sock *sk = sock->sk;

	if (sk) {
		sock_orphan(sk);
		sock_hold(sk);
		lock_sock(sk);
		vrr_destroy_sock(sk);
		release_sock(sk);
		sock_put(sk);
	}

	return 0;
}

static int vrr_sendmsg(struct kiocb *iocb, struct socket *sock,
		       struct msghdr *msg, size_t len)
{
	int addr_len = msg->msg_namelen;
	int err = 0;
	int flags = msg->msg_flags;
	size_t sent = 0;
	struct sk_buff *skb = NULL;
	struct sock *sk = sock->sk;
	struct sockaddr_vrr *addr = (struct sockaddr_vrr *)msg->msg_name;
	struct vrr_sock *vrr = vrr_sk(sk);

	if (sk->sk_shutdown & SEND_SHUTDOWN) {
		return -EPIPE;
	}

	/* Allocate an skb for sending */
	/* skb = sock_alloc_send_skb(sk, len, flags, errcode); */

	/* Build the vrr header */
	/* build_header(sk, addr); */

	/* Copy data from userspace */
	/* memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len); */
	
	/* Send packet */
	/* vrr_ouput(skb); */

	VRR_DBG("sock %p, sk %p", sock, sk);
	return err;
}

static void vrr_sock_destruct(struct sock *sk)
{
        __skb_queue_purge(&sk->sk_receive_queue);
        __skb_queue_purge(&sk->sk_error_queue);

        sk_mem_reclaim(sk);
}

struct proto vrr_proto = {
	.name = "VRR",
	.owner = THIS_MODULE,
	.max_header = VRR_MAX_HEADER,
	.obj_size = sizeof(struct vrr_sock),
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
		      int protocol, int kern)
{
	struct sock *sk;
	struct vrr_sock *vrr;
	int err;

	VRR_INFO("Begin vrr_create");
	VRR_DBG("proto: %u", protocol);

	err = -ENOBUFS;
	sk = sk_alloc(net, PF_VRR, GFP_KERNEL, &vrr_proto);
	if (!sk)
		goto out;

	err = 0;
	vrr = vrr_sk(sk);

	if (sock) {
		sock->ops = &vrr_proto_ops;
	}
	sock_init_data(sock, sk);

	sk->sk_destruct 	= vrr_sock_destruct;
	sk->sk_family 		= PF_VRR;
	sk->sk_protocol 	= protocol;
	sk->sk_allocation	= GFP_KERNEL;

	VRR_INFO("End vrr_create");
 out:
	return err;
}

const struct net_proto_family vrr_family_ops = {
	.family = PF_VRR,
	.create = vrr_create,
	.owner = THIS_MODULE,
};
