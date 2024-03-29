#pragma once

#include "common.h"
#include "../lib/rtp.h"
#include "audiocArgs.h"

#pragma pack(push, 1)
typedef struct {
    rtp_hdr_t header;
    u8 payload[]; //open ended array
} rtp_packet_t;
#pragma pack(pop)

void printRTPHeader(rtp_hdr_t* rtp);
void ntohRTP(rtp_hdr_t* header);
void htonRTP(rtp_hdr_t* header);

extern u8 silenceMU8[256];
extern u8 silenceL16BE[512];

//Safely calculate the difference between sequence numbers taking wrapping into account
inline static i32 seqNumDifference(u16 from, u16 to)
{
    return (i32)(i16)(to - from);
}

//Safely calculate the difference between timestamps taking wrapping into account
inline static i64 timestampDifference(u32 from, u32 to)
{
    return (i64)(i32)(to - from);
}

inline static const char* payloadToStr(enum payload pt) {
    switch (pt)
    {
    case PCMU:
        return "PCMU";
    case L16_1:
        return "L16_1";
    default:
        return "Unknown";
    }
}