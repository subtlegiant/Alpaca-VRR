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
	struct sockaddr_vrr dest_addr;
	unsigned int addr;
	char buf[255];

	if (argc != 3) {
		fprintf(stderr, "Usage: sendto <addr in hex> \"<message>\"\n");
		exit(1);
	}

	if (sscanf(argv[1], "%x", &addr) != 1) {
		fprintf(stderr, "Invalid addr\n");
		exit(1);
	}

	printf("addr: %x\n", addr);

	strncpy(buf, argv[2], 255);
	
	if ((sock = socket(AF_VRR, SOCK_RAW, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	dest_addr.svrr_family = AF_VRR;
	dest_addr.svrr_addr = addr;
	bzero(&(dest_addr.svrr_zero), 10);

	ret = sendto(sock, buf, strlen(buf) + 1, 0,
	       (struct sockaddr *)&dest_addr, sizeof(struct sockaddr));
	if (ret < 0) {
		perror("sendto");
		exit(1);
	}

        printf("Sent %d bytes.\n", ret);
        return 0;
}
