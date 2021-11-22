#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/soundcard.h>


#include "common.h"
#include "audiocArgs.h"
#include "audioc_rtp.h"
#include "../lib/circularBuffer.h"
#include "../lib/configureSndcard.h"
#include "../lib/rtp.h"

typedef enum payload payload_t;
typedef struct {
    u32 ssrc;
    u8 pt;
    u32 fragmentBytes;
    u32 bytesPerSample;
} session_params_t;

static void signalHandler(int sigNum)
{
	(void) sigNum;
    //TODO: handle signal:
    //Print time
    //Close everything ......
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
        .fragmentBytes = requestedFragmentSize, 
        .bytesPerSample = bytesPerSample,
    };
    

    /*
    *   Circular buffer
    */
    //TODO: fill from bufferingTime
    bufferingTime += 200; // for 
    int bufferingBytes = bufferingTime * rate * channelNumber * bytesPerSample / 1000;
    int bufferingBlocks = bufferingBytes / requestedFragmentSize;
    trace("Num. blocks in cbuf: %d\n", bufferingBlocks);
    void* circularBuffer = cbuf_create_buffer(bufferingBlocks, requestedFragmentSize);
    
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

    usize expectedPacketSize = sessionParams.fragmentBytes + sizeof(rtp_hdr_t);
    usize samplesPerPacket = sessionParams.fragmentBytes / sessionParams.bytesPerSample;
    rtp_packet_t* packet = (rtp_packet_t*) malloc(expectedPacketSize);
    struct timeval timeout;
    fd_set readSet, writeSet;

    //RTP State variables
    bool startedReceiving = false;
    u16 inputSequenceNum = 0;
    u32 inputTimeStamp = 0;

    u16 outputSequenceNum = 0; //TODO: make it random
    u32 outputTimeStamp = 0; //TODO: make it random

    //TODO: Measure time
    isize cbufAccumulated = 0; //in blocks
    while (cbufAccumulated < bufferingBlocks)
    {
        memset(packet, 0, expectedPacketSize);

        timeout.tv_sec = 10;
        timeout.tv_usec = 0;

        FD_ZERO(&readSet);
        FD_SET(sndCardFD, &readSet); //Microphone read
        FD_SET(sockId, &readSet); //Network read
        
        FD_ZERO(&writeSet);
        // FD_SET(sockId, &writeSet); //Network write
        // Network writes don't block for a long time

        int res = 0;
        if ((res = select(FD_SETSIZE, &readSet, &writeSet, NULL, &timeout)) <0) 
        {
            int error = errno;
            switch (error)
            {
            case EINTR:
                fprintf(stderr, "select has been interrupted.\n");
                break;
            default:
                panic("select() error!");
                break;
            }
        } else if (res > 0) {
            //Reads
            if (FD_ISSET(sndCardFD, &readSet)) 
            {
                //We can read from the sound card
                u8* fragmentBuffer = (u8*)packet->payload;
                usize fragmentSize = sessionParams.fragmentBytes;
                

                isize readBytes;
                if ((readBytes = read(sndCardFD, fragmentBuffer, fragmentSize)) < 0)
                {
                    panic("Error reading %lu bytes (%lu samples) from sound card file", fragmentSize, samplesPerPacket);
                } else if (readBytes != fragmentSize)
                {
                    panic("Incomplete read of %lu bytes (actually read %lu bytes) from sound card file", fragmentSize, readBytes);
                }

                // Fill up RTP Header
                packet->header = (rtp_hdr_t) {
                    .version = RTP_VERSION,
                    .pt = sessionParams.pt,
                    .ssrc = sessionParams.ssrc,
                    .seq = outputSequenceNum, 
                    .ts = outputTimeStamp, //TODO: make it random for the initial value
                };

                trace("RTP Header to send:");
                printRTPHeader(&packet->header);

                htonRTP(&packet->header);

                isize result;
                /* Using sendto to send information. Since I've bind the socket, the local (source) port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
                if ( (result = sendto(sockId, packet, expectedPacketSize, /* flags */ 0, (struct sockaddr *)&sendAddr, sizeof(struct sockaddr_in)) )<0) {
                    panic("sendto error");
                } else {
                    trace("Packet with ts=%lu sent to network", outputTimeStamp); 
                }

                //Once we send it, seq and ts is incremented for the next packet
                outputSequenceNum += 1;
                outputTimeStamp += samplesPerPacket;
            }
            if (FD_ISSET(sockId, &readSet)) {
                isize result;
                struct sockaddr_in remoteSAddr = {0}; 
                socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
                
                if ((result = recvfrom(sockId, packet, expectedPacketSize, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
                    panic("recvfrom error");
                } else if (result != expectedPacketSize) {
                    //TODO: Fix if this happens
                    panic("Expected to receive full sized packet!");
                } else {
                    char ipBuf[64];
                    const char* ip = inet_ntop(AF_INET, &remoteSAddr.sin_addr, ipBuf, sizeof(ipBuf));
                    //trace("Received message from %s. Size: %d bytes\n", ip, result);

                    ASSERT(result == (isize)expectedPacketSize);
                    rtp_hdr_t* header = &packet->header;
                    ntohRTP(header);

                    if (header->version != RTP_VERSION) {
                        trace("Received invalid RTP Header, dropping");
                        continue;
                    }

                    if (header->pt != sessionParams.pt) {
                        fprintf(stderr, "Payload type mismatch between nodes (%s, expected %s). Closing. \n", payloadToStr(header->pt), payloadToStr(sessionParams.pt));
                        exit(1);
                    }

                    if (!startedReceiving) { 
                        startedReceiving = true;
                        inputSequenceNum = header->seq;
                        inputTimeStamp = header->ts;
                    } else {
                        i32 seqDifference = seqNumDifference(inputSequenceNum, header->seq);
                        i64 tsDifference = timestampDifference(inputTimeStamp, header->ts);

                        if (tsDifference % samplesPerPacket != 0) {
                            fprintf(stderr, "Mismatched packet sizes, exiting program.");
                            exit(1);
                        }

                        if(tsDifference < 0 || tsDifference > (i64)samplesPerPacket) {
                            //Received previous samples, ignore
                            //Either last packet was smaller than samplesPerPacket or
                            //this is a retransmission? ignore
                        }

                        if (seqDifference == 1) {
                            //Packet received as expected
                            if (tsDifference == (i64)samplesPerPacket) {
                                //Packet received as expected
                            } else if(tsDifference > (i64)samplesPerPacket) {
                                //Samples have been skipped (not sent, no loss occured), we need to introduce a silence
                                i64 numBlocks = tsDifference / samplesPerPacket;
                                ASSERT(numBlocks > 1);
                                i64 numSilenceBlocks = numBlocks - 1;
                                for (usize i = 0; i < numSilenceBlocks; i++)
                                {
                                    void* bufferBlock = cbuf_pointer_to_write(circularBuffer);
                                    if (bufferBlock)
                                        memcpy(bufferBlock, packet->payload, sessionParams.fragmentBytes);
                                    else {
                                        fprintf(stderr, "Circular buffer is full, dropping silences.\n");
                                        break;
                                    }
                                }
                            }

                            void* bufferBlock = cbuf_pointer_to_write(circularBuffer);
                            if (bufferBlock)
                                memcpy(bufferBlock, packet->payload, sessionParams.fragmentBytes);
                            else {
                                fprintf(stderr, "Circular buffer is full, dropping packet.\n");
                            }
                        } else if(seqDifference > 1) {
                            //1 or more packets have been lost
                            i64 lostPackets = seqDifference - 1;
                            //Compute silence time from tsDifference
                        } else if(seqDifference < 0) {
                            //Received a previous packet
                        } else {
                            //Retransmission, ignore
                        }

                        inputSequenceNum = header->seq;
                        inputTimeStamp = header->ts;
                    }

                    //printRTPHeader(header);
                }
            }

            //Write
            // if (FD_ISSET(sndCardFD, &writeSet)) {
                
            // }
            // if (FD_ISSET(sockId, &writeSet)) {
                
            // }

        } else {
            printf("Timeout has expired.\n");
        }
    }
    
     

    /*
    *   Cleanup
    */
    cbuf_destroy_buffer(circularBuffer);
    return 0;
}


static void execSender(int sockId, struct sockaddr_in* sendAddr, session_params_t session)
{  
    char sendBuffer[MAX_PACKET_SIZE];
    char recvBuffer[MAX_PACKET_SIZE];

    ZERO_ARRAY(sendBuffer);
    const size_t expectedPacketSize = session.fragmentBytes + sizeof(rtp_hdr_t);

    rtp_packet_t* packet = (rtp_packet_t*) sendBuffer;
    packet->header = (rtp_hdr_t) {
        .version = RTP_VERSION,
        .pt = session.pt,
        .ssrc = session.ssrc,
        .seq = 0, //TODO: make it random
        .ts = 0, //TODO: make it random for the initial value
    };

    trace("RTP Header to send:\n");
    printRTPHeader(&packet->header);

    htonRTP(&packet->header);

    //Test Audio samples
    ASSERT(session.pt == L16_1);
    const size_t numSamples = session.fragmentBytes / session.bytesPerSample;
    i16* audioBuf = (i16*)packet->payload;
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

    const size_t expectedPacketSize = session.fragmentBytes + sizeof(rtp_hdr_t);

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
    rtp_packet_t* packet = (rtp_packet_t*)recvBuffer;
    ntohRTP(&packet->header);
    printRTPHeader(&packet->header);

    i32 remainingBytes = result - sizeof(rtp_hdr_t);
    ASSERT(remainingBytes == (i32)session.fragmentBytes);
    i16* samples = (i16*)packet->payload; 
    //Check samples
    size_t nSamples = session.fragmentBytes / session.bytesPerSample;
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

    packet->header.ssrc = session.ssrc;
    packet->header.seq += 1;
    
    trace("RTP Header to send back:");
    printRTPHeader(&packet->header);

    htonRTP(&packet->header);

    if ( (result = sendto(sockId, recvBuffer, expectedPacketSize, 0, (struct sockaddr*)sendAddr, sizeof(struct sockaddr_in)) )<0) {
        panic("sendto error\n");
    } else {
        printf("Receiver: Sending back the same RTP packet\n"); 
    }
}