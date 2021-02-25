#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netinet/ip.h>
#include <linux/if_packet.h>
#include <linux/tcp.h>
#include <time.h>
#include <stdbool.h>

// 4194304 bytes
unsigned int blocksiz = 1 << 22; 
// 2048 bytes
unsigned int framesiz = 1 << 11; 
unsigned int blocknum = 64; 

uint64_t received_packets = 0;
uint64_t received_bytes = 0;

struct block_desc {
    uint32_t version;
    uint32_t offset_to_priv;
    struct tpacket_hdr_v1 h1;
};

void *speed_printer(void *arg) {
    while (true) {
        uint64_t packets_before = received_packets;
        uint64_t bytes_before = received_bytes;
        sleep(1);       
        uint64_t packets_after = received_packets;
        uint64_t bytes_after = received_bytes;
        uint64_t pps = packets_after - packets_before;
        uint64_t bytes = bytes_after - bytes_before;
        printf("We process: %llu pps at %.2f Mbps\n", pps, bytes * 8 / 1024 / 1024);
    }
}

typedef struct task {
    int sock_r;
    int id;
    int fanout_group_id;
}task_t;

void flush_block(struct block_desc *pbd) {
    pbd->h1.block_status = TP_STATUS_KERNEL;
}

void walk_block(struct block_desc *pbd, const int block_num) {
    int num_pkts = pbd->h1.num_pkts, i;
    unsigned long bytes = 0;
    struct tpacket3_hdr *ppd;

    ppd = (struct tpacket3_hdr *) ((uint8_t *) pbd +
            pbd->h1.offset_to_first_pkt);
    for (i = 0; i < num_pkts; ++i) {
        bytes += ppd->tp_snaplen;

        ppd = (struct tpacket3_hdr *) ((uint8_t *) ppd +
                ppd->tp_next_offset);
    }

    received_packets += num_pkts;
    received_bytes += bytes;
}

int get_interface_number_by_device_name(int socket_fd, char *interface_name) {
    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));

    strncpy(ifr.ifr_name, interface_name, sizeof(ifr.ifr_name));

    if (ioctl(socket_fd, SIOCGIFINDEX, &ifr) == -1) {
        return -1;
    }

    return ifr.ifr_ifindex;
}

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

    int interface_number = get_interface_number_by_device_name(task->sock_r, "eth0");
    if (interface_number == -1) {
        printf("Can't get interface number by interface name\n");
        return NULL;
    }

    struct sockaddr_ll bind_address;
    memset(&bind_address, 0, sizeof(bind_address));

    bind_address.sll_family = AF_PACKET;
    bind_address.sll_protocol = htons(ETH_P_ALL);
    bind_address.sll_ifindex = interface_number;
    int bind_result = bind(task->sock_r, (struct sockaddr *)&bind_address, sizeof(bind_address));
    if (bind_result == -1) {
        printf("Can't bind to AF_PACKET socket\n");
        return NULL;
    }

    int version = TPACKET_V3;
    int setsockopt_packet_version = setsockopt(task->sock_r, SOL_PACKET, PACKET_VERSION, &version, sizeof(version));
    if (setsockopt_packet_version < 0) {
        printf("Can't set packet v3 version\n");
        return NULL;
    }

    struct tpacket_req3 req;
    memset(&req, 0, sizeof(req));
    req.tp_block_size = blocksiz;
    req.tp_frame_size = framesiz;
    req.tp_block_nr = blocknum;
    req.tp_frame_nr = (blocksiz * blocknum) / framesiz;
    req.tp_retire_blk_tov = 60; // Timeout in msec
    req.tp_feature_req_word = TP_FT_REQ_FILL_RXHASH;
    int setsockopt_rx_ring = setsockopt(task->sock_r, SOL_PACKET , PACKET_RX_RING , (void*)&req , sizeof(req));
    if (setsockopt_rx_ring == -1) {
        printf("Can't enable RX_RING for AF_PACKET socket\n");
        return NULL;
    }

    uint8_t* mapped_buffer = NULL;
    struct iovec* rd = NULL;
    mapped_buffer = (uint8_t*)mmap(NULL, req.tp_block_size * req.tp_block_nr, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, task->sock_r, 0);
    if (mapped_buffer == MAP_FAILED) {
        printf("mmap failed!\n");
        return NULL;
    }

    rd = (struct iovec*)malloc(req.tp_block_nr * sizeof(struct iovec));
    for (int i = 0; i < req.tp_block_nr; ++i) {
        rd[i].iov_base = mapped_buffer + (i * req.tp_block_size);
        rd[i].iov_len = req.tp_block_size;
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

    unsigned int current_block_num = 0;
    struct pollfd pfd;
    memset(&pfd, 0, sizeof(pfd));

    pfd.fd = task->sock_r;
    pfd.events = POLLIN | POLLERR;
    pfd.revents = 0;

    while (true) {
        struct block_desc *pbd = (struct block_desc *) rd[current_block_num].iov_base;

        if ((pbd->h1.block_status & TP_STATUS_USER) == 0) {
            poll(&pfd, 1, -1);
            continue;
        }   

        walk_block(pbd, current_block_num);
        flush_block(pbd);
        current_block_num = (current_block_num + 1) % blocknum;
    }   
    return NULL;
}

int main(int argc, char **argv){

    if(argc < 2){
        printf("usage: %s task_number\n", argv[0]);
        return -1;
    }

    pthread_t tid_printer;
    pthread_create(&tid_printer, NULL, speed_printer, NULL);

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
