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

typedef struct task {
    int sock_r;
    int id;
}task_t;

void *receive(void *arg) {
    task_t *task = (task_t *)arg;

    unsigned char *buffer = (unsigned char *) malloc(65536); //to receive data
    memset(buffer,0,65536);
    struct sockaddr saddr;
    int saddr_len = sizeof (saddr);

    int i;
    time_t tlast = 0;
    uint64_t bytes = 0;
    uint64_t pps = 0;
    for(i=0; true; i++){
        time_t tnow = time(NULL);
        if(tnow != tlast){
            double mbps = bytes * 8.0 / 1024 / 1024;
            printf("task %d recv: %.2lfMbps, %dpps\n", task->id, mbps, pps);

            bytes = 0;
            pps = 0;
            tlast = tnow;
        }
        int buflen;
        //Receive a network packet and copy in to buffer
        buflen=recvfrom(task->sock_r,buffer,65536,0,&saddr,(socklen_t *)&saddr_len);
        if(buflen<0)
        {
            perror("error in reading recvfrom function\n");
            return -1;
        }

        // eth
        if(buflen < sizeof(struct ethhdr)){
            //printf("l: %d\n", __LINE__);
            continue;
        }
        struct ethhdr *eth = (struct ethhdr *)(buffer);
        if(eth->h_proto != htons(ETH_P_IP)){
            //printf("l: %d, eth->h_proto = %d\n", __LINE__, eth->h_proto);
            continue;
        }

        // ip
        if(buflen < sizeof(struct ethhdr) + sizeof(struct iphdr)){
            //printf("l: %d\n", __LINE__);
            continue;
        }
        struct iphdr *ip = (struct iphdr *)(buffer + sizeof(struct ethhdr));
        if(ip->protocol != 6){
            //printf("l: %d\n", __LINE__);
            continue;
        }

        // tcp
        int header_len = sizeof(struct ethhdr) + ip->ihl * 4;
        if(buflen < header_len + sizeof(struct tcphdr)){
            //printf("l: %d\n", __LINE__);
            continue;
        }
        struct tcphdr *tcp = (struct tcphdr *)(buffer + header_len);

        pps += 1;
        bytes += buflen;
        //printf("received a tcp packet: 0x%08x:%d -> 0x%08x:%d\n", \
        ip->saddr, htons(tcp->source), ip->daddr, htons(tcp->dest));
    }

    return NULL;
}

int main(int argc, char **argv){

    if(argc < 2){
        printf("usage: %s task_number\n", argv[0]);
        return -1;
    }

    int n = atoi(argv[1]);

    int sock_r;
    sock_r = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(sock_r<0){
        printf("error in socket\n");
        return -1;
    }

    int i;
    for(i=0; i<n; i++){
        pthread_t tid;
        task_t *task = malloc(sizeof(task_t));
        task->sock_r = sock_r;
        task->id = i;
        pthread_create(&tid, NULL, receive, task);
    }

    while(1){
        sleep(1);
    }
}
