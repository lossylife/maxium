#if 0
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
			printf("recv: %.2lfMbps, %dpps\n", mbps, pps);

			bytes = 0;
			pps = 0;
			tlast = tnow;
		}
		int buflen;
		//Receive a network packet and copy in to buffer
		buflen=recvfrom(sock_r,buffer,65536,0,&saddr,(socklen_t *)&saddr_len);
		if(buflen<0)
		{
			printf("error in reading recvfrom function\n");
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

	return 0;
}
#endif

#include <pcap.h>
#include <stdio.h>

void got_packet(u_char *args, const struct pcap_pkthdr *header,
                const u_char *packet){
    printf("got a packet: length = %d\n", header->len);
}

int main(int argc, char *argv[]){
    pcap_t *handle;         /* Session handle */
    char *dev;          /* The device to sniff on */
    char errbuf[PCAP_ERRBUF_SIZE];  /* Error string */
    struct bpf_program fp;      /* The compiled filter */
    char filter_exp[] = "tcp port 1883";  /* The filter expression */
    bpf_u_int32 mask;       /* Our netmask */
    bpf_u_int32 net;        /* Our IP */
    struct pcap_pkthdr header;  /* The header that pcap gives us */
    const u_char *packet;       /* The actual packet */

#if 0
    /* Define the device */
    dev = pcap_lookupdev(errbuf);
    if (dev == NULL) {
        fprintf(stderr, "Couldn't find default device: %s\n", errbuf);
        return(2);
    }
#else
    dev = "eth0";
#endif
    /* Find the properties for the device */
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        fprintf(stderr, "Couldn't get netmask for device %s: %s\n", dev, errbuf);
        net = 0;
        mask = 0;
    }
    /* Open the session in promiscuous mode */
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return(2);
    }
    /* Compile and apply the filter */
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Couldn't parse filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return(2);
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Couldn't install filter %s: %s\n", filter_exp, pcap_geterr(handle));
        return(2);
    }

    pcap_loop(handle, 100, got_packet, NULL);

    /* And close the session */
    pcap_close(handle);
    return(0);
}

