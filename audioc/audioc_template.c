/* execute
    $ cp audioc_template.c audioc.c

    Then, replace FILL comments with real code.
 */


#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <sys/soundcard.h>

#include "audiocArgs.h"
#include "../lib/circularBuffer.h"
#include "../lib/configureSndcard.h"
#include "../lib/rtp.h"

void * circularBuffer;

/* activated by Ctrl-C */
void signalHandler (int sigNum __attribute__ ((unused)))  /* __attribute__ ((unused))   -> this indicates gcc not to show an 'unused parameter' warning about sigNum: is not used, but the function must be declared with this parameter */
{
	/* FILL */
}

int main(int argc, char *argv[])
{
    struct sigaction sigInfo; 
    struct in_addr multicastIp;
    uint32_t ssrc;
    uint16_t port;
    uint8_t vol;
    uint32_t packetDuration; /* in milliseconds */
    uint32_t bufferingTime; /* in milliseconds */
    bool verbose;
    uint8_t payload;

    int channelNumber;
    int sndCardFormat;
    int rate;

    int requestedFragmentSize;
    int numberOfBlocks;
    int sndCardDesc;

    struct sockaddr_in remToSendSAddr;
    struct ip_mreq mreq;
    int socketDesc;

    /* Obtains values from the command line - or default values otherwise */
    if (args_capture_audioc(argc, argv, &multicastIp, &ssrc,
            &port, &vol, &packetDuration, &verbose, &payload, &bufferingTime) == EXIT_FAILURE)
    { 
        exit(1);  /* there was an error parsing the arguments, error info
                   is printed by the args_capture function */
    };

    /*************************/
    /* Installs signal */
    sigInfo.sa_handler = signalHandler;
    sigInfo.sa_flags = 0;
    sigemptyset(&sigInfo.sa_mask); /* clear sa_mask values */
    if ((sigaction (SIGINT, &sigInfo, NULL)) < 0) {
        printf("Error installing signal, error: %s", strerror(errno));
        exit(1);
    }

    /*************************/
    /* Configures sound card */
    channelNumber = 1;
    if (payload == PCMU) {
        rate = 8000;
        sndCardFormat = AFMT_MU_LAW;
    } else if (payload == L16_1) {
        rate = 8000;
        sndCardFormat = AFMT_S16_BE;
    }

    requestedFragmentSize = 0/* FILL */;

    /* configures sound card, sndCardDesc is filled after it returns */
    configSndcard (&sndCardDesc, &sndCardFormat, &channelNumber, &rate, &requestedFragmentSize, true);
    vol = configVol (channelNumber, sndCardDesc, vol);


    /*************************/
    /* Creates circular buffer */
    numberOfBlocks = 0; /* FILL */
    circularBuffer = cbuf_create_buffer(numberOfBlocks, requestedFragmentSize);


    /*************************/
    /* Configures socket */
    bzero(&remToSendSAddr, sizeof(remToSendSAddr));
    remToSendSAddr.sin_family = AF_INET;
    remToSendSAddr.sin_port = htons(port);
    remToSendSAddr.sin_addr = multicastIp;

    /* Creates socket */
    if ((socketDesc = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket error\n");
        exit(EXIT_FAILURE);
    }

    /* configure SO_REUSEADDR, multiple instances can bind to the same multicast address/port */
    int enable = 1;
    if (setsockopt(socketDesc, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        printf("setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    if (bind(socketDesc, (struct sockaddr *)&remToSendSAddr, sizeof(struct sockaddr_in)) < 0) {
        printf("bind error\n");
        exit(EXIT_FAILURE);
    }

    /* setsockopt configuration for joining to mcast group */
    mreq.imr_multiaddr = multicastIp;
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(socketDesc, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
        printf("setsockopt error");
        exit(EXIT_FAILURE);
    }

    /* Do not receive packets sent to the mcast address by this process */
    unsigned char loop=0;
    setsockopt(socketDesc, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(unsigned char));


    /* FILL: 
    - Record one audio packet and sent it through the socket. Generate RTP info.
    - 'while' to accumulate bufferingTime data, with select that coordinates:
        audio recording + send packet
        receive from socket + store in circular buffer
    - When we have enough data, 'while' that also plays from circular buffer.

    Then:
    - add silence processing
    - check for packets lost (only after 10 s have elapsed)
    - ...
    */
}