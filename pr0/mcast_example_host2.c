/* Unicast_multicast communication
THIS IS CODE for host2
compile with
	gcc -Wall -o host2 mcast_example_host2.c
execute as
	./host2
	
host1 uses an unicast localSAddr address.
host2 receives data in a multicast address. It receives from ANY sender.
(If you are testing with many of your mates, you should CHANGE the multicast address to avoid collisions)

host2 should be started first, then start host1. 
	packet1: host1 (unicast) -> host2(multicast)
	packet2: host2 (unicast) -> host1(unicast)
In all cases, PORT is used both as source and destination (just as stated in RFC 4961)

host1 and host2 can be executed in any host with multicast connectivity between them (preferrably from the same link, 163.117.144/24	
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <strings.h>
#include <stdlib.h>

#define MAXBUF 256
const char message[16]= "Sent from host2";

#define PORT 5000
/* change the following multicast address if you want to avoid collisions. Note that you must change it here AND ALSO in host1 code */
#define GROUP "239.0.1.1"

int main(void) {
    int sockId;
    int result;         /* for storing results from system calls */
    struct sockaddr_in localSAddr, remoteSAddr; /* to build address/port info for local node and remote node */ 

    struct ip_mreq mreq;/* for multicast configuration */
    char buf[MAXBUF];   /* to receive data from remoteSAddrote node */
    socklen_t sockAddrInLength;/* to store the length of the address returned by recvfrom */

    /* preparing bind */
    bzero(&localSAddr, sizeof(localSAddr));
    localSAddr.sin_family = AF_INET;
    localSAddr.sin_port = htons(PORT); /* besides filtering, this assures that info is being sent with this PORT as local port */
    /* fill .sin_addr with multicast address */
    if (inet_pton(AF_INET, GROUP, &localSAddr.sin_addr) < 0) {
        printf("inet_pton error\n");
        exit(1);
    }

    /* creating socket */
    if ((sockId = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket error\n");
        exit(1);
    }

    /* configure SO_REUSEADDR, multiple instances can bind to the same multicast address/port */
    int enable = 1;
    if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed");
        exit(1);
    }

    /* binding socket - using mcast localSAddr address */
    if (bind(sockId, (struct sockaddr *)&localSAddr, sizeof(struct sockaddr_in)) < 0) {
        printf("bind error\n");
        exit(1);
    }

    /* setsockopt configuration for joining to mcast group */
    if (inet_pton(AF_INET, GROUP, &mreq.imr_multiaddr) < 0) {
        printf("inet_pton");
        exit(1);
    }
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sockId, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        printf("setsockopt error");
        exit(1);
    }


    sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
    if ((result = recvfrom(sockId, buf, MAXBUF, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
        printf ("recvfrom error\n");
    } else {
        buf[result] = 0; /* convert to 'string' by appending a 0 value (equal to '\0') after the last received character */
        printf("Host2: Received message in multicast address. Message is: %s\n", buf); fflush (stdout);
    }


    /* Note that we use the information of the remoteSAddrote node filled in by the 'recvfrom' call (the rem structure), as is, for the sendto call. This means that we are sending to the same node from which we received a packet in the recvfrom call. This is how host2 'learns' the unicast address and port of host1. */  

    /* Using sendto to send information. Since I've made a bind to the socket, the localSAddr (source) port of the packet is fixed. 
       In the remoteSAddr structure I have the address and port of the remote host, as returned by recvfrom */ 
    if ( (result = sendto(sockId, message, sizeof(message), /* flags */ 0, (struct sockaddr *) &remoteSAddr, sizeof(remoteSAddr)))<0) {
        printf ("sendto error\n");
    } else {
        printf("Host2: Sent message to UNIcast address\n"); fflush (stdout);
    }

    exit(0); /* everything ended fine */  
}   
