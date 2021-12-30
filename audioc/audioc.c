//
//	AudioC - Marcos Oviedo Rodriguez - 100384018
//	Para compilar el codigo ejecute:
//  > gcc -Wall -Wextra -std=gnu99 -g lib/*.c audioc/*.c -o audioc -lm
//

#include <stdio.h>
#include <signal.h>

#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>

#include "common.h"
#include "audiocArgs.h"
#include "audioc_rtp.h"
#include "../lib/circularBuffer.h"
#include "../lib/configureSndcard.h"
#include "../lib/rtp.h"

#include <stdlib.h>

typedef enum payload payload_t;
typedef struct {
    u32 ssrc;
    u8 pt;
    u32 fragmentBytes;
    u32 bytesPerSample;
    i32 sampleRate;
} session_params_t;


typedef struct {
    i32 packetsPlayed;
    i32 silencesPlayed;
    i32 timeouts; //Technically count as silences
    i32 lostPackets;
    i32 packetsRecorded;
    struct timeval playbackStart;
} statistics_t;

static session_params_t sessionParams = {};
static statistics_t stats = {};
static rtp_packet_t* packet;
static void* circularBuffer;

static void signalHandler(int sigNum)
{
	(void) sigNum;

    struct timeval stopTime;
    if(gettimeofday(&stopTime, NULL) != 0) {
        panic("Could not get current time from gettimeofday()!");
    }

    printf("Interrupted audioc\n");

    printf("Played packets: %d\n", stats.packetsPlayed);
    printf("Silent packets: %d\n", stats.lostPackets + stats.silencesPlayed + stats.timeouts);
    printf("\tDue to detected silence (~): %d\n", stats.silencesPlayed);
    printf("\tDue to packet loss (x): %d\n", stats.lostPackets);
    printf("\tDue to timeouts (t): %d\n", stats.timeouts);

    if (stats.packetsPlayed > 0) {
        //in us
        i64 start = stats.playbackStart.tv_usec + stats.playbackStart.tv_sec * 1000000;
        i64 end = stopTime.tv_usec + stopTime.tv_sec * 1000000;

        i64 diff = end - start;
        double theoreticalPlayback = stats.packetsPlayed * sessionParams.fragmentBytes / 
            (double)(sessionParams.bytesPerSample * sessionParams.sampleRate);
        
        printf("Total playback time (theoretical): %f seconds.\n", theoreticalPlayback);
        printf("Total playback time (wall clock): %f seconds.\n", (double)diff / 1e6);
    } else {
        printf("No audio was played.\n");
    }
    
    printf("Recorded (and sent) packets: %d\n", stats.packetsRecorded);

    //Free buffers
    free(packet);
    cbuf_destroy_buffer(circularBuffer);

    exit(0);
}

static void readAudioFragment(int sndCardFD, rtp_packet_t* packet, session_params_t sessionParams)
{
    u8* fragmentBuffer = (u8*)packet->payload;
    usize fragmentSize = sessionParams.fragmentBytes;

    isize readBytes;
    usize packetSize = sessionParams.fragmentBytes + sizeof(rtp_hdr_t);
    if ((readBytes = read(sndCardFD, fragmentBuffer, fragmentSize)) < 0)
    {
        panic("Error reading %lu bytes (%lu samples) from sound card file", fragmentSize, packetSize);
    } else if ((usize)readBytes != fragmentSize)
    {
        panic("Incomplete read of %lu bytes (actually read %lu bytes) from sound card file", fragmentSize, readBytes);
    }
}

static void sendAudioPacket(rtp_packet_t* packet, int sockId, struct sockaddr_in* sendAddr, session_params_t sessionParams, u16 seq, u32 ts)
{
    packet->header = (rtp_hdr_t) {
        .version = RTP_VERSION,
        .pt = sessionParams.pt,
        .ssrc = sessionParams.ssrc,
        .seq = seq, 
        .ts = ts, 
    };

    htonRTP(&packet->header);

    isize result;
    usize packetSize = sessionParams.fragmentBytes + sizeof(rtp_hdr_t);
    /* Using sendto to send information. Since I've bind the socket, the local (source) port of the packet is fixed. In the rem structure I set the remote (destination) address and port */ 
    if ( (result = sendto(sockId, packet, packetSize, /* flags */ 0, (struct sockaddr *)sendAddr, sizeof(struct sockaddr_in)) )<0) {
        panic("sendto error");
    } else {
        verboseInfo(".");
    }
}

static bool validateRTPHeader(rtp_hdr_t* header, session_params_t sessionParams)
{
    if (header->version != RTP_VERSION) {
        return false;
    }

    if (header->pt != sessionParams.pt) {
        fprintf(stderr, "Payload type mismatch between nodes (%s, expected %s). Closing. \n", payloadToStr(header->pt), payloadToStr(sessionParams.pt));
        return false;
    }
    return true;
}

static bool pushSilence(void* cbuf, usize fragmentSize, isize *outCbufCount, payload_t payload)
{
    u8* bufferBlock = (u8*)cbuf_pointer_to_write(cbuf);
    if (bufferBlock) {
        usize blockSize;
        u8* silenceArray;
        if (payload == PCMU) {
            blockSize = ARRAY_COUNT(silenceMU8);
            silenceArray = silenceMU8;
        } else {
            blockSize = ARRAY_COUNT(silenceL16BE);
            silenceArray = silenceL16BE;
        }
        usize fullCopies = fragmentSize / blockSize;
        usize partialCopy = fragmentSize % blockSize;
        for (usize i = 0; i < fullCopies; i++)
        {
            memcpy(bufferBlock, silenceArray, blockSize);
            bufferBlock += blockSize;
        }
        memcpy(bufferBlock, silenceArray, partialCopy);
        //memset(bufferBlock, 0, fragmentSize);
        *outCbufCount = (*outCbufCount) + 1;
        return true;
    }
    return false;
}

int main(int argc, char** argv)
{
    srand(time(NULL));

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

    int rate = 8000;
    int channelNumber = 1; //We only support mono
    
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

    sessionParams = (session_params_t) {
        .ssrc = ssrc,
        .pt = payload,
        .fragmentBytes = requestedFragmentSize, 
        .bytesPerSample = bytesPerSample,
        .sampleRate = rate,
    };    

    /*
    *   Circular buffer
    */

    int bufferingBytes = bufferingTime * rate * channelNumber * bytesPerSample / 1000;
    //The behavior of audiocTest is to round down the buffering blocks count, so we do it here too
    int bufferingBlocks = (int)floor((float)bufferingBytes / (float)requestedFragmentSize);
    
    trace("Bytes for buffering: %d", bufferingBytes);

    // +200ms for safety
    int bufferByteCapacity = bufferingBytes + (200 * rate * channelNumber * bytesPerSample / 1000);
    int bufferBlockCapacity = bufferByteCapacity / requestedFragmentSize;

    trace("Num. blocks in cbuf: %d, buffer block threshold: %d\n", bufferBlockCapacity, bufferingBlocks);
    
    circularBuffer = cbuf_create_buffer(bufferBlockCapacity, requestedFragmentSize);

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

    usize expectedPacketSize = sessionParams.fragmentBytes + sizeof(rtp_hdr_t);
    const usize samplesPerPacket = sessionParams.fragmentBytes / sessionParams.bytesPerSample;
    trace("Samples per packet: %d\n", samplesPerPacket);
    packet = (rtp_packet_t*) malloc(expectedPacketSize);
    struct timeval timeout;
    fd_set readSet, writeSet;

    //RTP State variables
    bool startedReceiving = false;
    u16 inputSequenceNum = 0;
    u32 inputTimeStamp = 0;

    u16 outputSequenceNum = 0; //TODO: make it random
    u32 outputTimeStamp = 0; //TODO: make it random

    //1st phase

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
        
        // Network writes don't block for a long time
        FD_ZERO(&writeSet);
        
        //No timeout in 1st phase
        int res = 0;
        if ((res = select(FD_SETSIZE, &readSet, &writeSet, NULL, NULL)) <0) 
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
                
                readAudioFragment(sndCardFD, packet, sessionParams);
                sendAudioPacket(packet, sockId, &sendAddr, sessionParams, outputSequenceNum, outputTimeStamp);               

                //Once we send it, seq and ts is incremented for the next packet
                outputSequenceNum += 1;
                outputTimeStamp += samplesPerPacket;

                stats.packetsRecorded++;
            }
            if (FD_ISSET(sockId, &readSet)) {
                isize result;
                struct sockaddr_in remoteSAddr = {0}; 
                socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
                
                if ((result = recvfrom(sockId, packet, expectedPacketSize, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
                    panic("recvfrom error");
                } else if ((usize)result != expectedPacketSize) {
                    //TODO: Fix if this happens
                    panic("Expected to receive full sized packet!");
                }

                ASSERT(result == (isize)expectedPacketSize);
                rtp_hdr_t* header = &packet->header;
                ntohRTP(header);

                if (!validateRTPHeader(&packet->header, sessionParams)) {
                    fprintf(stderr, "Invalid received RTP packet. Exiting.");
                    exit(1);
                }

                if (!startedReceiving) { 
                    startedReceiving = true;
                    inputSequenceNum = header->seq;
                    inputTimeStamp = header->ts;

                    char ipBuf[64];
                    const char* ip = inet_ntop(AF_INET, &remoteSAddr.sin_addr, ipBuf, sizeof(ipBuf));
                    trace("Started receiving from %s.\n", ip, result);
                } else {
                    i32 seqDifference = seqNumDifference(inputSequenceNum, header->seq);
                    i64 tsDifference = timestampDifference(inputTimeStamp, header->ts);

                    if (tsDifference % samplesPerPacket != 0) {
                        fprintf(stderr, "Mismatched packet sizes, exiting program.");
                        exit(1);
                    }

                    if (seqDifference == 1) {
                        //Packet received as expected
                        if(tsDifference > (i64)samplesPerPacket) {
                            trace("Silence in buffering phase!");
                        }
                    } else {
                        trace("Warning: Unexpected sequence number received in buffering phase!");
                    }
                }

                void* bufferBlock = cbuf_pointer_to_write(circularBuffer);
                if (bufferBlock) {
                    memcpy(bufferBlock, packet->payload, sessionParams.fragmentBytes);
                    cbufAccumulated++;
                } else {
                    fprintf(stderr, "Circular buffer is full, dropping packet.\n");
                }
                

                verboseInfo("+");

                inputSequenceNum = header->seq;
                inputTimeStamp = header->ts;
            }

        } else {
            printf("Warning: Timeout has expired in buffering phase.\n");
        }
    }
    
    //trace("Finished buffering phase.");

    // 2nd loop
    while (1) {
        memset(packet, 0, expectedPacketSize);

        i32 bytesInCard = 0;
        if (ioctl(sndCardFD, SNDCTL_DSP_GETODELAY, &bytesInCard) < 0)
        {
            panic("Error calling ioctl SNDCTL_DSP_GETODELAY");
        }

        //Estimate time until sound card depletion - 10ms (for safety)
        //  T = remaining in sound card (ms) + remaining in buffer (ms) - 10 ms
        float timeInBuffer = cbufAccumulated * samplesPerPacket * 1000.f / (float)rate; //ms
        float timeInCard = (bytesInCard / bytesPerSample) * 1000.f / (float)rate; //ms
        float remainingTime = MAX(timeInBuffer + timeInCard - 10.f, 0.0f); //ms
        i64 remUSecs = (i64) (remainingTime * 1000.f); //us

        timeout.tv_sec = 0;
        timeout.tv_usec = remUSecs;

        FD_ZERO(&readSet);
        FD_SET(sndCardFD, &readSet); //Microphone read
        FD_SET(sockId, &readSet); //Network read
        
        FD_ZERO(&writeSet);
        if (cbufAccumulated > 0) {
            //Only add the sound card to the writing set if the buffer is not empty
            FD_SET(sndCardFD, &writeSet); //Microphone write
        }
        
        //printf("Timer(%ld us = %f buffer ms + %f card ms), Acc. Buffer: %ld blocks.\n", remUSecs, timeInBuffer, timeInCard, cbufAccumulated);
        
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

            //Write operations
            if (FD_ISSET(sndCardFD, &writeSet)) {

                if (cbuf_has_block(circularBuffer)) {
                    //We can play audio
                    void* block = cbuf_pointer_to_read(circularBuffer);
                    ASSERT(block);
                    isize n = write(sndCardFD, block, sessionParams.fragmentBytes);

                    if (n < 0) {
                        printError("Error playing %d byte block at sound card.", sessionParams.fragmentBytes);
                    } else if (n != sessionParams.fragmentBytes) {
                        printError("Played a different number of bytes than expected (played %d bytes, expected %d)", 
                            n, sessionParams.fragmentBytes);
                    }

                    if (stats.packetsPlayed == 0) {
                        if(gettimeofday(&stats.playbackStart, NULL) != 0) {
                            panic("Could not get current time from gettimeofday()!");
                        }
                    }

                    stats.packetsPlayed++;
                    verboseInfo("-");
                    cbufAccumulated--;
                } else {
                    //Empty buffer
                    trace("Circular buffer is overrun!");
                }
            }

            //trace("Buffer blocks: %d", cbufAccumulated);
            //Read operations
            if (FD_ISSET(sndCardFD, &readSet)) {
                //We can read from the sound card
                
                readAudioFragment(sndCardFD, packet, sessionParams);
                //Simulate packet loss       
                sendAudioPacket(packet, sockId, &sendAddr, sessionParams, outputSequenceNum, outputTimeStamp);  

                //Once we send it, seq and ts is incremented for the next packet
                outputSequenceNum += 1;
                outputTimeStamp += samplesPerPacket;

                stats.packetsRecorded++;
            }
            if (FD_ISSET(sockId, &readSet)) {
                
                isize result;
                struct sockaddr_in remoteSAddr = {0}; 
                socklen_t sockAddrInLength = sizeof (struct sockaddr_in); /* remember always to set the size of the rem variable in from_len */	
                
                if ((result = recvfrom(sockId, packet, expectedPacketSize, 0, (struct sockaddr *) &remoteSAddr, &sockAddrInLength)) < 0) {
                    panic("recvfrom error");
                } else if ((usize)result != expectedPacketSize) {
                    panic("Expected to receive full sized packet!");
                }

                rtp_hdr_t* header = &packet->header;
                ntohRTP(header);

                if (!validateRTPHeader(&packet->header, sessionParams)) {
                    fprintf(stderr, "Invalid received RTP packet. Exiting.");
                    exit(1);
                }
                   
                i32 seqDifference = seqNumDifference(inputSequenceNum, header->seq);
                i64 tsDifference = timestampDifference(inputTimeStamp, header->ts);

                if (tsDifference % samplesPerPacket != 0) {
                    fprintf(stderr, "Mismatched packet sizes, exiting program.");
                    exit(1);
                }

                bool discard = false;

                if(seqDifference < 1 || tsDifference < (i64)samplesPerPacket) {
                    //Received previous samples, ignore
                    //Either last packet was smaller than samplesPerPacket or
                    //this is a retransmission? ignore
                    trace("Re-TX: current(seq=%d, ts=%d), recv(seq=%d, ts=%d)\n", 
                        inputSequenceNum, inputTimeStamp, header->seq, header->ts);
                    discard = true;
                }else if (seqDifference == 1) {
                    //Packet received as expected
                    if (tsDifference == (i64)samplesPerPacket) {
                        //Packet received as expected
                    } else if(tsDifference > (i64)samplesPerPacket) {
                        //Samples have been skipped (not sent, no loss occured), we need to introduce a silence
                        i64 numBlocks = tsDifference / samplesPerPacket;
                        ASSERT(numBlocks > 1);
                        i64 freeBlocks = bufferBlockCapacity - cbufAccumulated;
                        i64 numSilenceBlocks = MIN(numBlocks - 1, freeBlocks);
                        //printf("(Silence packet) Pushing %ld silences.\n", numSilenceBlocks);
                        for (i64 i = 0; i < numSilenceBlocks; i++)
                        {
                            if (!pushSilence(circularBuffer, sessionParams.fragmentBytes, &cbufAccumulated, sessionParams.pt)) {
                                trace("Circular buffer is full, dropping silences.\n");
                            } 
                            stats.silencesPlayed++;
                            verboseInfo("~");
                        }
                    }                    
                } else if(seqDifference > 1) {
                    //1 or more packets have been lost (K-1)
                    i64 lostPackets = seqDifference - 1;
                    
                    // tsDiff = (K+J)*F
                    //Either lost or silence blocks + the received one (K+J)
                    i64 pendingBlocks = tsDifference / samplesPerPacket;
                    ASSERT(pendingBlocks > lostPackets);
                    //leave one for the data buffer 
                    i64 freeBlocks = bufferBlockCapacity - 1 - cbufAccumulated;
                    i64 silenceBlocks = MAX(0, MIN(pendingBlocks - 1, freeBlocks));
                    ASSERT((pendingBlocks - 1 >= lostPackets) && "Wrong pendingBlocks");
                    
                    //Silence blocks implied in this packet (J)
                    //i64 actualSilenceBlocks = silenceBlocks - lostPackets;
                    //printf("(Lost packet) Seq. dif =%d, lost %ld packets. Pushing %ld silences in total.\n", seqDifference, lostPackets, silenceBlocks);

                    for (i64 i = 0; i < silenceBlocks; i++)
                    {                        
                        //We assume the lost blocks come first
                        if (i < lostPackets) {
                            verboseInfo("x");
                            stats.lostPackets++;
                        } else {
                            verboseInfo("~");
                            stats.silencesPlayed++;
                        }

                        if (!pushSilence(circularBuffer, sessionParams.fragmentBytes, &cbufAccumulated, sessionParams.pt)) {
                            fprintf(stderr, "Circular buffer is full, dropping silence.\n");
                        }
                    }

                } else if(seqDifference <= 0) {
                    //Retransmission, ignore
                    //trace("Warning: Retransmission packet received!");
                    discard = true;
                }

                if (!discard) {
                    //Add the samples we just received
                    void* bufferBlock = cbuf_pointer_to_write(circularBuffer);
                    if (bufferBlock) {
                        //TODO: it may be possible to eliminate this copy
                        //by peeking the header and then doing recvfrom() directly on the buffer
                        memcpy(bufferBlock, packet->payload, sessionParams.fragmentBytes);
                        cbufAccumulated++;
                    } else {
                        fprintf(stderr, "Circular buffer is full, dropping packet.\n");
                    }

                    verboseInfo("+");
                
                    inputSequenceNum = header->seq;
                    inputTimeStamp = header->ts;
                }
            }
        } else {
            bool success = pushSilence(circularBuffer, sessionParams.fragmentBytes, &cbufAccumulated, sessionParams.pt);
            //If the buffer is somehow full something has gone wrong
            if (!success){
                fprintf(stderr, "Circular buffer is full, dropping silence.\n");
            }
            //Increment input counters as if it arrived correctly 
            stats.timeouts++;
            //silences do not increment the sequence number
            inputTimeStamp += samplesPerPacket;
            verboseInfo("t");
        }
    }

    /*
    *   Cleanup
    */
    free(packet);
    cbuf_destroy_buffer(circularBuffer);
    return 0;
}
