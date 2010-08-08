#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "vrr_sock.h"

int main(int argc, char **argv)
{
	int sock, ret;
	struct sockaddr_vrr src_addr;
	unsigned int addr;	
	socklen_t addrlen = sizeof(struct sockaddr);
	char buf[255];

	if (argc != 2) {
		fprintf(stderr, "Usage: recvfrom <addr in hexadecimal>\n");
		exit(1);
	}

	if (sscanf(argv[1], "%x", &addr) != 1) {
		fprintf(stderr, "Invalid addr\n");
		exit(1);
	}

	if ((sock = socket(AF_VRR, SOCK_RAW, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	src_addr.svrr_family = AF_VRR;
	src_addr.svrr_addr = addr;
	bzero(&(src_addr.svrr_zero), 10);

	if (connect(sock, (struct sockaddr *)&src_addr, addrlen) < 0) {
		perror("connect");
		exit(1);
	}

	ret = recvfrom(sock, buf, 255, 0, 
		(struct sockaddr *)&src_addr, &addrlen);

	if (ret < 0) {
		perror("recvfrom");
		exit(1);
	}
	printf("%x said: %s\n", addr, buf);

	return 0;
}
