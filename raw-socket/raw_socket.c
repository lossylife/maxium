#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <linux/tcp.h>
#include <time.h>
#include <stdbool.h>

int main(){
	int sock_r;
	sock_r=socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
	if(sock_r<0)
	{
		printf("error in socket\n");
		return -1;
	}

#define VLEN 1024
#define BUFSIZE 2048
    struct mmsghdr msgs[VLEN];
    struct iovec iovecs[VLEN];
    char bufs[VLEN][BUFSIZE+1];
    memset(msgs, 0, sizeof(msgs));

    int bufsize = 1024*1024;
    struct iovec iov;
    iov.iov_base = malloc(bufsize);
    iov.iov_len = bufsize;

    

    int i;
    struct sockaddr saddr;
    int saddr_len = sizeof (saddr);
    uint64_t max_read = 0;

	time_t tlast = 0;
	uint64_t bytes = 0;
	uint64_t pps = 0;
	for(i=0; true; i++){

        time_t tnow = time(NULL);
        if(tnow != tlast){
            double mbps = bytes * 8.0 / 1024 / 1024;
            printf("recv: %.2lfMbps, %dpps, max read %llu\n", mbps, pps, max_read);

			bytes = 0;
			pps = 0;
			tlast = tnow;
		}
        
        for (int i = 0; i < VLEN; i++) {
            iovecs[i].iov_base         = bufs[i];
            iovecs[i].iov_len          = BUFSIZE;
            msgs[i].msg_hdr.msg_iov    = &iovecs[i];
            msgs[i].msg_hdr.msg_iovlen = 1;
        }
        int retval = recvmmsg(sock_r, msgs, VLEN, 0, NULL);
        if (retval == -1) {
            perror("recvmmsg()");
            return -1;
        }

        pps += retval;
        for (int i = 0; i < retval; i++) {
            bytes += msgs[i].msg_len;
        }

        if(retval > max_read){
            max_read = retval;
        }
    }

	return 0;
}

