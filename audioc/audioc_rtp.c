#include "audioc_rtp.h"

#include <stdio.h>


//Does no endianness conversion
void printRTPHeader(rtp_hdr_t* rtp) {
    printf("RTP Header: \n");
    printf("\tVersion: %d\n", rtp->version);
    printf("\tPadding: %d\n", rtp->p);
    printf("\tExtension: %d\n", rtp->x);
    printf("\tCSRC Count: %d\n", rtp->cc);
    printf("\tMarker: %d\n", rtp->m);
    const char* ptStr = payloadToStr(rtp->pt);
    printf("\tPayload Type: %d (%s)\n", rtp->pt, ptStr);
    printf("\tSequence Num: %d\n", rtp->seq);
    printf("\tTimestamp: %d\n", rtp->ts);
    printf("\tSSRC: %.X\n", rtp->ssrc);
}

void ntohRTP(rtp_hdr_t* header) {
    header->ssrc = ntohl(header->ssrc);
    header->seq = ntohs(header->seq);
    header->ts = ntohl(header->ts);
}

void htonRTP(rtp_hdr_t* header) {
    header->ssrc = htonl(header->ssrc);
    header->seq = htons(header->seq);
    header->ts = htonl(header->ts);
}


