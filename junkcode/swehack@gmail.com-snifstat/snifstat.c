/* this application captures packets destined to and from 
 * a specified host, it then tries to calculate in and out 
 * traffic statistics and display it like ifstat 
 * by nocturnal [at] swehack [dot] se */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

/* gethostbyname(3) */
#include <netdb.h>

/* networking(4) */
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/if_ether.h>
#include <ifaddrs.h>

/* setitimer(2) */
#include <sys/time.h>

/* signal(3) */
#include <signal.h>

/* get_windowsize */
#include <termios.h>
#ifndef TIOCGWINSZ
#include <sys/ioctl.h>
#endif

/* duh -lpcap */
#include <pcap.h>

#define APP_NAME	"snifstat"
#define APP_VERSION	0.2

unsigned int calc_traf_io(char *, struct ether_header *);
int ethaddrsncmp(const char *, const char *, size_t);
void reset_count(int);
unsigned short get_windowsize(void);
void usage(const char *);

unsigned int reset = 0;

int main(int argc, char **argv) {
	char *ifname = NULL;
	int argch;
	unsigned int show_in_bits = 0;
	char bits_prefix[] = "Kbps";
	char bytes_prefix[] = "KB/s";
	unsigned int max_iteration = 0;

	char errbuf[PCAP_ERRBUF_SIZE];
	pcap_t *pcap = NULL;
	struct bpf_program filterd;
	bpf_u_int32 netp, netmask;
	char *filter = NULL;
	const u_char *packet = NULL;
	struct pcap_pkthdr header;
	
	unsigned char *ether_addrs = NULL;
	struct ifaddrs *ifa = NULL;
	struct sockaddr_dl *sdl = NULL;

	struct itimerval itv, oitv;
	struct itimerval *itvp = &itv;


	struct ether_header *ethernet = NULL;
	
	unsigned int iteration = 0, total_iteration = 0;
	double cur_in, cur_out;
	unsigned short cur_ws = 0, new_ws, old_ws, ws_change;
	unsigned int traf_io = 2;

	unsigned int *resetp = &reset;

	if(argc < 4) {
		usage(argv[0]);
		exit(-1);
	}

	while((argch = getopt(argc, argv, "i:bc:hv")) != -1) {
		switch(argch) {
			case 'i':
				if(strlen(optarg) < 16) {
					ifname = optarg;
					optreset = 1;
				} else {
					usage(argv[0]);
					exit(-1);
				}
				break;
			case 'b':
				show_in_bits = 1;
				optreset = 1;
				break;
			case 'c':
				max_iteration = (unsigned int)atoi(optarg);
				break;
			case 'h':
				usage(argv[0]);
				exit(-1);
			case 'v': /* LOL WUT?! */
				printf("%s v%.1f by nocturnal [at] swehack [dot] se\n", APP_NAME, APP_VERSION);
				exit(-1);
		}
	}

	if(argc - optind < 1) {
		usage(argv[0]);
		exit(-1);
	}

	filter = argv[optind];

	if(pcap_lookupnet(ifname, &netp, &netmask, errbuf) == -1) {
		fprintf(stderr, "pcap_lookupnet: %s\n", errbuf);
		exit(-1);
	}

	if((pcap = pcap_open_live(ifname, 65535, 1, 10, errbuf)) == NULL) {
		fprintf(stderr, "pcap_open_live: %s\n", errbuf);
		exit(-1);
	}

	if(pcap_compile(pcap, &filterd, filter, 1, netmask) == -1) {
		fprintf(stderr, "pcap_compile: %s\n", pcap_geterr(pcap));
		exit(-1);
	}

	if(pcap_setfilter(pcap, &filterd) == -1) {
		fprintf(stderr, "pcap_setfilter: %s\n", pcap_geterr(pcap));
		exit(-1);
	}

	if(signal(SIGALRM, reset_count) == SIG_ERR) {
		perror("signal: ");
		exit(-1);
	}

	if(getifaddrs(&ifa) == -1) {
		perror("getifaddrs: ");
		exit(-1);
	}

	for(;ifa;ifa = ifa->ifa_next) {
		if(strncmp(ifname, ifa->ifa_name, sizeof(ifa->ifa_name)) == 0) {
			sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			if((ether_addrs = malloc(sdl->sdl_alen)) == NULL) {
				perror("malloc: ");
				exit(-1);
			}
			memcpy(ether_addrs, LLADDR(sdl), sdl->sdl_alen);
			break;
		}
	}

	timerclear(&itvp->it_interval);
	itvp->it_value.tv_sec = 1;
	itvp->it_value.tv_usec = 0;

	while(1) {
		*resetp = 0;

		old_ws = cur_ws;
		new_ws = get_windowsize();
		if(new_ws != old_ws) {
			cur_ws = new_ws;
		}
		ws_change = cur_ws-2;

		if(setitimer(ITIMER_REAL, itvp, &oitv) < 0) {
			fprintf(stderr, "setitimer: \n");
			exit(-1);
		}

		cur_in = 0.0;
		cur_out = 0.0;
		while(*resetp == 0) {
			if((packet = pcap_next(pcap, &header)) != NULL) {
				ethernet = (struct ether_header *)packet;

				if(header.len == 671429858) {
					cur_in += 0.0;
					cur_out += 0.0;
				} else {
					traf_io = calc_traf_io(ether_addrs, ethernet);
					if(traf_io == 0) {
						cur_in += header.len;
					} else if(traf_io == 1) {
						cur_out += header.len;
					}
					traf_io = 2;
				}
			}
		}

		cur_in /= 1024;
		cur_out /= 1024;

		if(show_in_bits == 1) {
			cur_in *= 8;
			cur_out *= 8;
		}
		
		if(iteration >= ws_change || total_iteration == 0) {
			printf("%11s\n%5s in %5s out\n", ifname, (show_in_bits == 1) ? bits_prefix : bytes_prefix, (show_in_bits == 1) ? bits_prefix : bytes_prefix);
			if(iteration > 1) {
				iteration = 1;
			}
		}

		if(total_iteration > 0) {
			printf("%8.2lf %9.2lf\n", cur_in, cur_out);
		}

		if(max_iteration > 0 && max_iteration == total_iteration) {
			break;
		}

		iteration++;
		total_iteration++;
	}

	pcap_close(pcap);

	exit(0);
}

/* calculate if the packet is going in or out */
unsigned int calc_traf_io(char *ether_addrs, struct ether_header *ethernet) {
	/* 0 = in
	 * 1 = out
	 * 2 = error
	 * GET IT!? GET IT?!?!??!!? :/ */
	if(ethaddrsncmp(ether_addrs, ethernet->ether_shost, sizeof(ether_addrs)) == 0) {
		return(1);
	}
	if(ethaddrsncmp(ether_addrs, ethernet->ether_dhost, sizeof(ether_addrs)) == 0) {
		return(0);
	}
	return(2);
}

/* compare ethernet addresses */
int ethaddrsncmp(const char *s1, const char *s2, size_t len) {
	if(len == 0) {
		return(0);
	}

	do {
		if(*s1++ != *s2++) {
			return(*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
		}
	} while(--len != 0);

	return(0);
}

/* resets a global value */
void reset_count(int signal) {
	int *resetp = NULL;
	resetp = &reset;

	*resetp = 1;

	return;
}

/* get windowsize */
unsigned short get_windowsize(void) {
	struct winsize ws;

	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) < 0) {
		return(-1);
	}

	return(ws.ws_row);
}

void usage(const char *appname) {
	printf("Usage: %s -i <interface> [-bhv] <filter>\n", appname);
	printf("\t-i <interface>\t Specify interface to capture from\n"
			"\t-b\t\t Show values in bits instead of bytes\n"
			"\t-h\t\t Show this help text\n"
			"\t-v\t\t Show version\n");

	return;
}
