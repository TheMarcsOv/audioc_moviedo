#include <stdio.h>
#include "common.h" 

int main(int argc, char const *argv[])
{
    u16 a = 65504;
    u16 b = 65520;
    u16 c = 0;

    ASSERT((b - a) == (u16)16);
    ASSERT( (i32)(a - b) == ((i32)-16) );

    u16 d = c - b;
    printf("d=%u\n", d);
    ASSERT((u16)(c - b) == (u16)16);

    for (u16 i = 0; i < 16; i++)
    {
        d = c - b;
        //printf("d=%u\n", d);
        ASSERT((u16)d == (u16)16);

        c++; b++;
    }
    
    i32 revD = (i32)(b-c);
    ASSERT(revD == (i32)-16);

    {
        u16 current = 15;
        u16 recieved = 0;
        i32 tsDiff = (i32)(i16)(recieved - current);
        printf("%d\n", tsDiff);
        ASSERT( tsDiff == -15 );
    }

    printf("SUCCESS\n");

    return 0;
}
