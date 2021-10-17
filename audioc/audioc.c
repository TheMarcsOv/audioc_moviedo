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

    struct sigaction sigInfo = {
        .sa_handler = signalHandler,
        .sa_flags = 0,
    };
    sigemptyset(&sigInfo.sa_mask);
    
    if ((sigaction (SIGINT, &sigInfo, NULL)) < 0) {
        printf("Error installing signal, error: %s", strerror(errno));
        exit(1);
    }

    return 0;
}