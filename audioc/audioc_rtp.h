#pragma once

#include "common.h"
#include "../lib/rtp.h"

#pragma pack(push, 1)
typedef struct {
    rtp_hdr_t header;
    u8 payload[]; //open ended array
} rtp_packet_t;
#pragma pack(pop)

void printRTPHeader(rtp_hdr_t* rtp);
void ntohRTP(rtp_hdr_t* header);
void htonRTP(rtp_hdr_t* header);