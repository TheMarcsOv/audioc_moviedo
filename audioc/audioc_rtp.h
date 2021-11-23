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

//Safely calculate the difference between sequence numbers without overflow
inline static i32 seqNumDifference(u16 from, u16 to)
{
    if (to >= from) return (i32)to - (i32)from; //easy case
    else return (i32)0xffff - (i32)to + (i32)from + (i32)1;
}

//Safely calculate the difference between timestamps without overflow
inline static i64 timestampDifference(u32 from, u32 to)
{
    if (to >= from) return (i64)to - (i64)from ; //easy case
    else return (i64)0xffffffff - (i64)to + (i64)from + (i64)1;
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