/* Unicast_multicast communication
THIS IS CODE for host1

compile with
	gcc -Wall -o host1 mcast_example_host1.c
execute as
	./host1
	
host1 uses an unicast local address.
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
const char message[16]= "Sent from host1"; /* 16 bytes is enough space for accommodating the message */

#define PORT 5000
/* change the following multicast address if you want to avoid collisions. Note that you must change it here AND ALSO in host2 code */
#define GROUP "239.0.1.1"


int main(void) {
    int sockId;
    int result;     /* for storing results from system calls */
    struct sockaddr_in localSAddr, remToSendSAddr, remToRecvSAddr; /* to build address/port info for local node and remote node */
    char buf[MAXBUF]; /* to receive data from remote node */

    socklen_t sockAddrInLength; /* for recvfrom */   

    /* preparing bind */
    bzero(&localSAddr, sizeof(localSAddr));
    localSAddr.sin_family = AF_INET;
    localSAddr.sin_port = htons(PORT);
    localSAddr.sin_addr.s_addr = htonl (INADDR_ANY);


    if ((sockId = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket error\n");
        exit(1); /* failure */
    }

    if (bind(sockId, (struct sockaddr *)&localSAddr, sizeof(struct sockaddr_in)) < 0) {
        printf("bind error\n");
        exit(1);
    }


    /* building structure to identify address/port of the remote node in order to send data to it */
    bzero(&remToSendSAddr, sizeof(remToSendSAddr));

    remToSendSAddr.sin_family = AF_INET;
    remToSendSAddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, GROUP, &remToSendSAddr.sin_addr) < 0) {
        printf("inet_pton error\n");
        exit(1);
    }


    /* Using sendto to send information. Since I've bind the socket, the local (source) port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
    if ( (result = sendto(sockId, message, sizeof(message), /* flags */ 0, (struct sockaddr *) &remToSendSAddr, sizeof(remToSendSAddr)))<0) {
        printf("sendto error\n");
    } else {
        printf("Host1: Using sendto to send data to multicast destination\n"); 
    }


    /* we do not need to fill in the 'remToRecv' variable, but this structure appears with the address and port of the remote node from which the packet was received. 
       However, we need to provide in advance the maximum amount of memory which recvfrom can use starting from the 'remToRecv' pointer -
       to allow recvfrom to be sure that it does not exceed the available space. To do so, we need to provide the size of the 'remToRecv' variable */
    sockAddrInLength = sizeof (struct sockaddr_in); 

    /* receives from any who wishes to send to host1 in this port */  
    if ((result= recvfrom(sockId, buf, MAXBUF, 0, (struct sockaddr *) &remToRecvSAddr, &sockAddrInLength)) < 0) {
        printf("recvfrom error\n");
    } else {
        buf[result] = 0;
        printf("Host1: Message received from unicast address. The message is: %s\n", buf); 
    }

    exit(0); /* everything ended fine */  

}   
