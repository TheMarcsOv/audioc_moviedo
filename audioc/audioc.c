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

typedef enum payload payload_t;
typedef struct {
    u32 ssrc;
    u8 pt;
    u32 packetBytes;
    u32 bytesPerSample;
} session_params_t;

static void signalHandler(int sigNum)
{
	(void) sigNum;
}

static void execSender(int sockId, struct sockaddr_in* sendAddr, session_params_t session);
static void execReceiver(int sockId, struct sockaddr_in* sendAddr, session_params_t session);

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
    //Calculate from parameters
    // secs * Hz

    int rate = 8000;
    int channelNumber = 1; //Only support mono
    
    int sndCardFmt;
    int bytesPerSample;
    switch (payload)
    {
    case L16_1:
        sndCardFmt = AFMT_S16_BE;
        bytesPerSample = 2;
        break;
    case PCMU:
        sndCardFmt = AFMT_MU_LAW;
        bytesPerSample = 1;
        break;
    default:
        fprintf(stderr, "WARNING: No payload selected, using Mu-law by default.");
        sndCardFmt = AFMT_MU_LAW;
        bytesPerSample = 1;
        break;
    }
    trace("BPSample: %d bytes, rate=%d Hz", bytesPerSample, rate);
    // In bytes
    int requestedFragmentSize = packetDuration * rate * channelNumber * bytesPerSample / 1000;
    
    trace("Requested fragment size: %d bytes.", requestedFragmentSize);
    
    int sndCardFD = -1;
    //duplex mode is activated
    configSndcard(&sndCardFD, &sndCardFmt, &channelNumber, &rate, &requestedFragmentSize, true);
    vol = configVol(channelNumber, sndCardFD, vol);

    float fragmentDuration = requestedFragmentSize * 1000 / (rate * channelNumber * bytesPerSample); //in ms
    trace("Obtained fragment size: %d. Obtained sound fragment duration: %.3f.", requestedFragmentSize, fragmentDuration);

    session_params_t sessionParams = {
        .ssrc = ssrc,
        .pt = payload,
        .packetBytes = requestedFragmentSize, //hard coded for now, requestedFragmentSize,
        .bytesPerSample = bytesPerSample,
    };
    

    /*
    *   Circular buffer
    */
    //TODO: fill from bufferingTime
    int bufferingBytes = bufferingTime * rate * channelNumber * bytesPerSample / 1000;
    int numberOfBlocks = 2 * bufferingBytes / requestedFragmentSize;
    trace("Num. blocks in cbuf: %d\n", numberOfBlocks);
    void* circularBuffer = cbuf_create_buffer(numberOfBlocks, requestedFragmentSize);
    
    /*
    *   Multicast socket configuration
    */

    //TODO: support for local ip, non-multicast for testing

    //MCast packet test

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
        execSender(sockId, &sendAddr, sessionParams);
    } else {
        trace("Receiver!");
        execReceiver(sockId, &sendAddr, sessionParams);
    }
     

    /*
    *   Cleanup
    */
    cbuf_destroy_buffer(circularBuffer);
    return 0;
}

//Does no endianness conversion
static void printRTPHeader(rtp_hdr_t* rtp) {
    printf("RTP Header: \n");
    printf("\tVersion: %d\n", rtp->version);
    printf("\tPadding: %d\n", rtp->p);
    printf("\tExtension: %d\n", rtp->x);
    printf("\tCSRC Count: %d\n", rtp->cc);
    printf("\tMarker: %d\n", rtp->m);
    const char* ptStr = rtp->pt == PCMU ? "PCMU" : (rtp->pt == L16_1 ? "L16_1" : "Unknown");
    printf("\tPayload Type: %d (%s)\n", rtp->pt, ptStr);
    printf("\tSequence Num: %d\n", rtp->seq);
    printf("\tTimestamp: %d\n", rtp->ts);
    printf("\tSSRC: %.X\n", rtp->ssrc);
}

static void ntohRTP(rtp_hdr_t* header) {
    header->ssrc = ntohl(header->ssrc);
    header->seq = ntohs(header->seq);
    header->ts = ntohl(header->ts);
}

static void htonRTP(rtp_hdr_t* header) {
    header->ssrc = htonl(header->ssrc);
    header->seq = htons(header->seq);
    header->ts = htonl(header->ts);
}

static void execSender(int sockId, struct sockaddr_in* sendAddr, session_params_t session)
{  
    char sendBuffer[MAX_PACKET_SIZE];
    char recvBuffer[MAX_PACKET_SIZE];

    ZERO_ARRAY(sendBuffer);
    const size_t expectedPacketSize = session.packetBytes + sizeof(rtp_hdr_t);

    rtp_hdr_t* rtpHeader = (rtp_hdr_t*) sendBuffer;
    *rtpHeader = (rtp_hdr_t) {
        .version = RTP_VERSION,
        .pt = session.pt,
        .ssrc = session.ssrc,
        .seq = 0, //TODO: make it random
        .ts = 0, //TODO: make it random for the initial value
    };

    trace("RTP Header to send:\n");
    printRTPHeader(rtpHeader);

    htonRTP(rtpHeader);

    //Test Audio samples
    ASSERT(session.pt == L16_1);
    const size_t numSamples = session.packetBytes / session.bytesPerSample;
    i16* audioBuf = (i16*)(sendBuffer + sizeof(rtp_hdr_t));
    usize msgSize = numSamples * sizeof(i16) + sizeof(rtp_hdr_t);
    ASSERT(msgSize <= expectedPacketSize);

    for (size_t i = 0; i < numSamples; i++)
    {
        //To test endianness
        audioBuf[i] = htons(i + 0xf00); //Convert host (LE) -> AFMT_S16_BE 
    }    

    isize result;
    /* Using sendto to send information. Since I've bind the socket, the local (source) port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
    if ( (result = sendto(sockId, sendBuffer, msgSize, /* flags */ 0, (struct sockaddr *)sendAddr, sizeof(struct sockaddr_in)) )<0) {
        panic("sendto error");
    } else {
        trace("Sender: Using sendto to send data to multicast destination"); 
    }

    struct sockaddr_in remoteSAddr = {0}; 
    socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
    if ((result = recvfrom(sockId, recvBuffer, expectedPacketSize, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
        panic("recvfrom error\n");
    } else {
        char ipBuf[64];
        const char* ip = inet_ntop(AF_INET, &remoteSAddr.sin_addr, ipBuf, sizeof(ipBuf));
        trace("Received message from %s. Size: %d bytes\n", ip, result);

        ASSERT(result == (isize)expectedPacketSize);
        rtp_hdr_t* header = (rtp_hdr_t*)recvBuffer;
        ntohRTP(header);
        printRTPHeader(header);
    }
}

static void execReceiver(int sockId, struct sockaddr_in* sendAddr, session_params_t session)
{
    /* Using sendto to send information. Since I've bound the socket, the local (source)
        port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
    char recvBuffer[MAX_PACKET_SIZE];
    isize result;
    ASSERT(session.pt == L16_1);

    const size_t expectedPacketSize = session.packetBytes + sizeof(rtp_hdr_t);

    struct sockaddr_in remoteSAddr = {0}; 
    socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
    
    if ((result = recvfrom(sockId, recvBuffer, expectedPacketSize, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
        panic("recvfrom error\n");
    }

    char ipBuf[64];
    const char* ip = inet_ntop(AF_INET, &remoteSAddr.sin_addr, ipBuf, sizeof(ipBuf));
    printf("RTP message from %s. Size: %ld bytes, expected= %ld\n", ip, result, expectedPacketSize);

    //TODO: make this more robust
    ASSERT(result == (isize)expectedPacketSize);
    rtp_hdr_t* header = (rtp_hdr_t*)recvBuffer;
    ntohRTP(header);
    printRTPHeader(header);

    i32 remainingBytes = result - sizeof(rtp_hdr_t);
    ASSERT(remainingBytes == (i32)session.packetBytes);
    i16* samples = (i16*)(recvBuffer + sizeof(rtp_hdr_t)); 
    //Check samples
    size_t nSamples = session.packetBytes / session.bytesPerSample;
    bool validSamples = true;
    for (size_t i = 0; i < nSamples; i++)
    {
        if (ntohs(samples[i]) != i + 0xf00) {
            validSamples = false;
            break;
        }
    }
    if (validSamples) trace("Received samples are valid!");
    else trace("Received samples are invalid!!!");

    header->ssrc = 1;
    header->seq += 1;
    htonRTP(header);

    if ( (result = sendto(sockId, recvBuffer, expectedPacketSize, 0, (struct sockaddr*)sendAddr, sizeof(struct sockaddr_in)) )<0) {
        panic("sendto error\n");
    } else {
        printf("Receiver: Sending back the same RTP packet\n"); 
    }
}