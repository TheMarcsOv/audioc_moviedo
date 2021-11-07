#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/soundcard.h>

#include "common.h"
#include "audiocArgs.h"
#include "../lib/circularBuffer.h"
#include "../lib/configureSndcard.h"
#include "../lib/rtp.h"

void signalHandler(int sigNum)
{
	(void) sigNum;
}

int main(int argc, char** argv)
{
    /* Obtains values from the command line - or default values otherwise */
    struct in_addr multicastIp;
    u32 ssrc;
    u16 port;
    u8 vol;
    u32 packetDuration; //in ms
    u32 bufferingTime; //in ms
    bool verbose;
    u8 payload;
    
    if (args_capture_audioc(argc, argv, &multicastIp, &ssrc,
            &port, &vol, &packetDuration, &verbose, &payload, &bufferingTime) == EXIT_FAILURE)
    { 
        exit(1);  /* there was an error parsing the arguments, error info
                   is printed by the args_capture function */
    };

    if (verbose) DEBUG_TRACES_ENABLED = true;

    trace("AudioC init...");

    if (verbose) {
        args_print_audioc(multicastIp, ssrc, port, packetDuration, payload, bufferingTime, vol, verbose);
    }

    /*
    *   Signal handler configuration
    */

    struct sigaction sigInfo = {
        .sa_handler = signalHandler,
        .sa_flags = 0,
    };
    sigemptyset(&sigInfo.sa_mask);
    
    if ((sigaction (SIGINT, &sigInfo, NULL)) < 0) {
        panic("Error installing signal.");
    }

    /*
    *   Sound card configuration
    */
    int requestedFragmentSize = 128;
    //HACK
    if (ssrc % 2 == 0) {
        int channelNumber = 1; //Only support mono
        int rate = 8000;
        int sndCardFmt;
        switch (payload)
        {
        case PCMU:
            sndCardFmt = AFMT_MU_LAW;
            break;
        case L16_1:
            sndCardFmt = AFMT_S16_BE;
            break;
        default:
            fprintf(stderr, "WARNING: No payload selected, using Mu-law by default.");
            sndCardFmt = AFMT_MU_LAW;
            break;
        }

        
        int sndCardFD = -1;
        //duplex mode is activated
        configSndcard(&sndCardFD, &sndCardFmt, &channelNumber, &rate, &requestedFragmentSize, true);
        vol = configVol(channelNumber, sndCardFD, vol);
    }

    /*
    *   Circular buffer
    */
    int numberOfBlocks = 16; /* FILL */
    void* circularBuffer = cbuf_create_buffer(numberOfBlocks, requestedFragmentSize);
    
    /*
    *   Multicast socket configuration
    */

    //MCast packet test
    #define MSG_SIZE 256
    #define GROUP "239.0.1.1"
    char recvBuffer[MSG_SIZE];
    isize result;

    /*
    *   Multicast socket configuration
    */
    struct sockaddr_in sendAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = multicastIp,
    };

    int sockId = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockId < 0) {
        panic("socket error");
    }

    int enable = 1;
    if (setsockopt(sockId, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        panic("setsockopt(SO_REUSEADDR) failed!\n");
    }

    if (bind(sockId, (struct sockaddr *)&sendAddr, sizeof(struct sockaddr_in)) < 0) {
        panic("Socket bind error!\n");
    }

    //Join multicast group
    struct ip_mreq mcRequest = {0}; 
    mcRequest.imr_multiaddr = multicastIp;
    mcRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(sockId, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcRequest, sizeof(struct ip_mreq)) < 0) {
        panic("Failed to join multicast group, setsockopt error");
    }

    //Disable loopback
    u8 loopback = 0;
    if (setsockopt(sockId, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(u8)) < 0) {
        panic("Failed to disable MC loopback, setsockopt error");
    }

    //HACK: if SSRC is odd we are the sender
    if (ssrc % 2) {
        trace("Sender!");

        /* Using sendto to send information. Since I've bound the socket, the local (source)
         port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
        char msg[MSG_SIZE] = "I am AudioC Sender!";
        

        /* Using sendto to send information. Since I've bind the socket, the local (source) port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
        if ( (result = sendto(sockId, msg, MSG_SIZE, /* flags */ 0, (struct sockaddr *) &sendAddr, sizeof(sendAddr)))<0) {
            printf("sendto error\n");
        } else {
            printf("Sender: Using sendto to send data to multicast destination\n"); 
        }

        struct sockaddr_in remoteSAddr = {0}; 
        socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
        if ((result = recvfrom(sockId, recvBuffer, MSG_SIZE, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
            printf ("recvfrom error\n");
        } else {
            //recvBuffer[result-1] = 0; /* convert to 'string' by appending a 0 value (equal to '\0') after the last received character */
            char ipBuf[64];
            const char* ip = inet_ntop(AF_INET, &((struct sockaddr_in*)&remoteSAddr)->sin_addr, ipBuf, sizeof(ipBuf));
            printf("Received message from %s. Message is: %s\n", ip, recvBuffer); fflush (stdout);
        }

    } else {
        trace("Receiver!");

        /* Using sendto to send information. Since I've bound the socket, the local (source)
         port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
        char msg[MSG_SIZE] = "I am AudioC Receiver!";

        struct sockaddr_storage remoteSAddr = {0}; 
        socklen_t sockAddrInLength = sizeof (struct sockaddr_storage); /* remember always to set the size of the rem variable in from_len */	
        if ((result = recvfrom(sockId, recvBuffer, MSG_SIZE, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
            printf ("recvfrom error\n");
        } else {
            //recvBuffer[result-1] = 0; /* convert to 'string' by appending a 0 value (equal to '\0') after the last received character */
            char ipBuf[64];
            const char* ip = inet_ntop(AF_INET, &((struct sockaddr_in*)&remoteSAddr)->sin_addr, ipBuf, sizeof(ipBuf));
            printf("Received message from %s. Message is: %s\n", ip, recvBuffer); fflush (stdout);
        }

         if ( (result = sendto(sockId, msg, MSG_SIZE, /* flags */ 0, (struct sockaddr *) &sendAddr, sizeof(sendAddr)))<0) {
            printf("sendto error\n");
        } else {
            printf("Receiver: Using sendto to send data to multicast destination\n"); 
        }

    }

     

    /*
    *   Cleanup
    */
    cbuf_destroy_buffer(circularBuffer);
    return 0;
}