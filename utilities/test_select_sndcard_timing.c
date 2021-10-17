/* Test several executions (for different sample format and rates) of 
- select + read of the sound card,
- select + write (For the first write operations, the corresponding
select does not block. When the buffer fills, then select is blocked.)
- select and read/write (as for a duplex connection)

to test if OS blocks in select or in read/write system calls.

Compile:
    gcc -o test_select_sndcard_timing test_select_sndcard_timing.c ../lib/configureSndcard.c

Execute:
    strace -x -T ./test_select_sndcard_timing
    strace -x -T -e select ./test_select_sndcard_timing
    strace -x -T -e read ./test_select_sndcard_timing
    strace -x -T -e write ./test_select_sndcard_timing

In the ouput, the last column indicates time in system call, should be LOW values 
(less than tens of ms) for read/write and high values for select (close to the duration
of the packet). An exception maybe the first time read is issued.

Alternative, execute as
    strace -X -tt -o traceName ./test_select_sndcard_timing
Then, read trace as
    cat traceName | ./diffTime | less
And observe values for different tests...
*/

/* According to POSIX.1-2001, POSIX.1-2008 */
#include <sys/select.h>
#include <stdbool.h>

#include <stdlib.h>
#include <stdio.h>

/* According to earlier standards */
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/soundcard.h>


#include "../lib/configureSndcard.h"

//#define BYTES_AUDIO_BLOCK 512
//#define BYTES_AUDIO_BLOCK 1024



void test_alternate_read_write(int sndCardDesc, int fragmentSize) {
    int i, res;
    fd_set readingSet, writingSet;
    char audio[fragmentSize];
    
    int count;

    printf("Reading and writing from soundcard\n");
    for (i=0; i<60; i++) {

        FD_ZERO(&readingSet);
        FD_SET(sndCardDesc, &readingSet);

        FD_ZERO(&writingSet);
        FD_SET(sndCardDesc, &writingSet);

        res = select(FD_SETSIZE, &readingSet, &writingSet, NULL, NULL);
        if (res < 1) {
            printf("Unexpected select return value, exiting");
            exit(-1);
        }

        if(FD_ISSET(sndCardDesc, &readingSet) == 1)
        {
            count = read(sndCardDesc, audio, fragmentSize);
            if (count != fragmentSize)
            {
                printf("Wanting to read %d bytes, obtained %d bytes\n", fragmentSize, count);
            }
        }
        if(FD_ISSET(sndCardDesc, &writingSet) == 1)
        {
            count = write(sndCardDesc, audio, fragmentSize);
            if (count != fragmentSize)
            {
                printf("Wanting to write %d bytes, obtained %d bytes\n", fragmentSize, count);
            }
        }        
    }
}

void test_sequential_read_write(int sndCardDesc, int fragmentSize) {
    int i, res;
    fd_set readingSet, writingSet;
    char audio[fragmentSize];
    
    int count;

    printf("Reading from soundcard\n");
    for (i=0; i<20; i++) {

        FD_ZERO(&readingSet);
        FD_SET(sndCardDesc, &readingSet);

        res = select(FD_SETSIZE, &readingSet, NULL, NULL, NULL);
        if (res != 1) {
            printf("Unexpected select return value, exiting");
            exit(-1);
        }

        /* should always be this case */
        if(FD_ISSET(sndCardDesc, &readingSet) == 1)
        {
            count = read(sndCardDesc, audio, fragmentSize);
            if (count != fragmentSize)
            {
                printf("Wanting to read %d bytes, obtained %d bytes\n", fragmentSize, count);
            }
        }
        else {
            printf("Unexpected select case, exiting");
            exit(-1);
        }
    }


    printf("Writing to soundcard\n");
    for (i=0; i<40; i++) {

        FD_ZERO(&writingSet);
        FD_SET(sndCardDesc, &writingSet);

        res = select(FD_SETSIZE, NULL, &writingSet, NULL, NULL);

        /* should always be this case */
        if(FD_ISSET(sndCardDesc, &writingSet) == 1)
        {
            count = write(sndCardDesc, audio, fragmentSize);
            if (count != fragmentSize)
            {
                printf("Wanting to write %d bytes, obtained %d bytes\n", fragmentSize, count);
            }
        }
        else {
            printf("Unexpected select case, exiting");
            exit(-1);
        }

    }
}

void print_packet_duration(int bits, int rate, int fragmentSize){
    printf("ESTIMATED packet duration: %f us\n", fragmentSize*8.0 /(bits) * 1000000.0 / rate );
}

int main() {

    int sndCardDesc, fragmentSize;

    int format, channel, rate, bits;
    channel = 1;

    printf("Execute as \nstrace -x -T -o traza test_./select_sndcard_timing\n");
    printf("1. Check that fragment size has the same value as requested\n");
    printf("2. Check that read/write times in the trace (cat traza... | ./diffTime | grep select | less) are the similar to those predicted for each test\n");

    printf("\n\nTest #1\n");
    fragmentSize = 1024; /* bytes */
    format = AFMT_MU_LAW;
    rate =  8000;
    bits = 8;
    
    printf("Some sequential reads, then some sequential writes\n");
    printf("Format: %d, channels: %d, rate: %d, requested fragment_size: %d\n", format, channel, rate, fragmentSize);
    configSndcard (&sndCardDesc, &format, &channel, &rate, &fragmentSize, true);
    printf("Final fragmentsize: ");
    printFragmentSize(sndCardDesc);
    print_packet_duration(bits, rate, fragmentSize);
    test_sequential_read_write(sndCardDesc, fragmentSize);
    close(sndCardDesc);

    printf("\n\nTest #2\n");
    format = AFMT_S16_BE;
    // channel = 1;
    rate =  8000;
    fragmentSize = 1024;
    bits = 16;

    printf("Some sequential reads, then some sequential writes\n");
    printf("Format: %d, channels: %d, rate: %d, requested fragment_size: %d\n", format, channel, rate, fragmentSize);
    configSndcard (&sndCardDesc, &format, &channel, &rate, &fragmentSize, true);
    printf("Final fragmentsize: ");
    printFragmentSize(sndCardDesc);
    print_packet_duration(bits, rate, fragmentSize);
    test_sequential_read_write(sndCardDesc, fragmentSize);
    close(sndCardDesc);


    printf("\n\nTest #3\n");

    format = AFMT_MU_LAW;
    // channel = 1;
    rate =  8000;
    fragmentSize = 1024;
    bits = 8;

    printf("Alternate read and write (i.e., duplex) operation\n");

    printf("Format: %d, channels: %d, rate: %d, requested fragment_size: %d\n", format, channel, rate, fragmentSize);
    configSndcard (&sndCardDesc, &format, &channel, &rate, &fragmentSize, true);
    printf("Final fragmentsize: ");
    printFragmentSize(sndCardDesc);
    print_packet_duration(bits, rate, fragmentSize);
    test_alternate_read_write(sndCardDesc, fragmentSize);
    close(sndCardDesc);

    printf("\n\nTest #4\n");
    format = AFMT_S16_BE;
    // channel = 1;
    rate =  8000;
    fragmentSize = 1024;
    bits = 16;

    printf("Alternate read and write (i.e., duplex) operation\n");

    printf("Format: %d, channels: %d, rate: %d, requested fragment_size: %d\n", format, channel, rate, fragmentSize);
    configSndcard (&sndCardDesc, &format, &channel, &rate, &fragmentSize, true);
    printf("Final fragmentsize: ");
    printFragmentSize(sndCardDesc);
    print_packet_duration(bits, rate, fragmentSize);
    test_alternate_read_write(sndCardDesc, fragmentSize);
    close(sndCardDesc);
}