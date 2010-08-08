#define AF_VRR 27
#define PF_VRR AF_VRR 

/* VRR sockets */
struct sockaddr_vrr {
	unsigned short	svrr_family; /* AF_VRR */
	unsigned int	svrr_addr;   /* VRR identifier */
	char		svrr_zero[10]; /* Padding */
};
