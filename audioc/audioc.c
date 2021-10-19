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

    LOG("DEBUG: test!\n");
    
    if (args_capture_audioc(argc, argv, &multicastIp, &ssrc,
            &port, &vol, &packetDuration, &verbose, &payload, &bufferingTime) == EXIT_FAILURE)
    { 
        exit(1);  /* there was an error parsing the arguments, error info
                   is printed by the args_capture function */
    };

    /*
    *   Signal handler configuration
    */

    struct sigaction sigInfo = {
        .sa_handler = signalHandler,
        .sa_flags = 0,
    };
    sigemptyset(&sigInfo.sa_mask);
    
    if ((sigaction (SIGINT, &sigInfo, NULL)) < 0) {
        printf("Error installing signal, error: %s", strerror(errno));
        exit(1);
    }

    /*
    *   Sound card configuration
    */
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
        printf("WARNING: No payload selected, using Mu-law by default.");
        sndCardFmt = AFMT_MU_LAW;
        break;
    }

    int requestedFragmentSize = 0;
    int sndCardFD = -1;
    //duplex mode is activated
    configSndcard(&sndCardFD, &sndCardFmt, &channelNumber, &rate, &requestedFragmentSize, true);
    vol = configVol(channelNumber, sndCardFD, vol);

    /*
    *   Circular buffer
    */
    int numberOfBlocks = 16; /* FILL */
    void* circularBuffer = cbuf_create_buffer(numberOfBlocks, requestedFragmentSize);
    
    /*
    *   Multicast socket configuration
    */
    struct sockaddr_in sendAddr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = multicastIp,
    };

    int socketDesc;
    if ((socketDesc = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("Socket could not be openned!\n");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed!\n");
        exit(EXIT_FAILURE);
    }

    if (bind(socketDesc, (struct sockaddr *)&sendAddr, sizeof(struct sockaddr_in)) < 0) {
        printf("Socket bind error!\n");
        exit(EXIT_FAILURE);
    }

    //Join multicast group
    struct ip_mreq mcRequest; 
    mcRequest.imr_multiaddr = multicastIp;
    mcRequest.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mcRequest, sizeof(struct ip_mreq)) < 0) {
        printf("setsockopt error");
        exit(EXIT_FAILURE);
    }

    u8 loopback = 0;
    setsockopt(socketDesc, IPPROTO_IP, IP_MULTICAST_LOOP, &loopback, sizeof(u8));


    /*
    *   Cleanup
    */
    cbuf_destroy_buffer(circularBuffer);
    return 0;
}