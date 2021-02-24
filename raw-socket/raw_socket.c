#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <linux/tcp.h>
#include <time.h>
#include <stdbool.h>

typedef struct task {
    int sock_r;
    int id;
    int fanout_group_id;
}task_t;

void *receive(void *arg) {
    task_t *task = (task_t *)arg;

    cpu_set_t current_cpu_set;
    CPU_ZERO(&current_cpu_set);
    CPU_SET(task->id, &current_cpu_set);
    int ret = sched_setaffinity(0, sizeof(current_cpu_set), &current_cpu_set);
    if(ret != 0){
        printf("error in sched_setaffinity\n");
        return NULL;
    }

    task->sock_r = socket(AF_PACKET,SOCK_RAW,htons(ETH_P_ALL));
    if(task->sock_r<0){
        printf("error in socket\n");
        return NULL;
    }

    // PACKET_FANOUT_LB - round robin
    // PACKET_FANOUT_CPU - send packets to CPU where packet arrived
    int fanout_type = PACKET_FANOUT_CPU; 

    int fanout_arg = (task->fanout_group_id | (fanout_type << 16));

    int setsockopt_fanout = setsockopt(task->sock_r, SOL_PACKET, PACKET_FANOUT, &fanout_arg, sizeof(fanout_arg));

    if (setsockopt_fanout < 0) {
        printf("Can't configure fanout\n");
        return NULL;
    }

    int bufsize = 1024*1024;
    struct iovec iov;
    iov.iov_base = malloc(bufsize);
    iov.iov_len = bufsize;

    int i;
    uint64_t max_read = 0;

    time_t tlast = 0;
    uint64_t bytes = 0;
    uint64_t pps = 0;
    for(i=0; true; i++){

        time_t tnow = time(NULL);
        if(tnow != tlast){
            double mbps = bytes * 8.0 / 1024 / 1024;
            printf("task %d recv: %.2lfMbps, %dpps, max %llu bytes\n", task->id, mbps, pps, max_read);

            bytes = 0;
            pps = 0;
            tlast = tnow;
        }

        int buflen;
        buflen = readv(task->sock_r, &iov, 1);
        if(buflen<0){
            printf("error in reading recvfrom function\n");
            return NULL;
        }
        if(buflen > max_read){
            max_read = buflen;
        }
        bytes += buflen;
        pps += buflen / 66;
    }

    return NULL;
}

int main(int argc, char **argv){

    if(argc < 2){
        printf("usage: %s task_number\n", argv[0]);
        return -1;
    }

    int n = atoi(argv[1]);
    int fanout_group_id = getpid() & 0xffff;

    int i;
    for(i=0; i<n; i++){
        pthread_t tid;
        task_t *task = malloc(sizeof(task_t));
        task->id = i;
        task->fanout_group_id = fanout_group_id;
        pthread_create(&tid, NULL, receive, task);
    }

    while(1){
        sleep(1);
    }
}
