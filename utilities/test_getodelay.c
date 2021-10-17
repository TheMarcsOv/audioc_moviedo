/*  File to test if GETODELAY works. 
    Writes 1024 bytes to the soundcard, checks getodelay immediately.
    It should return something similar (lower) to 1024 bytes, not 0

Compile: 
    gcc -o test_getodelay test_getodelay.c ../lib/configureSndcard.c

Execute:
    ./test_getodelay
*/

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/soundcard.h>


#include "../lib/configureSndcard.h"

#define AUDIO_SIZE 1024

int main()
{
    int descriptorSnd;
    int requestedFragmentSize = AUDIO_SIZE;
    int sndCardFormat = 8;
    int channelNumber = 1;
    int rate = 8000;
    int duplexMode = false; 

    /* Allocates memory and fills it with zeros. Don't care about the values, just fill
    with something in case I check with valgrind */
    void *audioData = calloc(AUDIO_SIZE, 1);

    configSndcard (&descriptorSnd, &sndCardFormat, &channelNumber, &rate, &requestedFragmentSize, duplexMode); 

    int bytesInSndcard;
    if (ioctl(descriptorSnd, SNDCTL_DSP_GETODELAY, &bytesInSndcard) <0 )
    {
        printf ("Error calling ioctl SNDCTL_DSP_GETODELAY, error %s\n", strerror(errno));
        exit(1);
    }
    printf("\nNumber of returned bytes when the soundcard is empty, should be 0: %d", bytesInSndcard);
    /* at VirtualBox 5.0.40, returns 0 bytes, ok */

    printf("\n\nNow writing to the soundcard, expecting more than 0 in getodelay\n");



    int bytesWrite;
    if ( (bytesWrite = write(descriptorSnd, audioData, AUDIO_SIZE)) < 0)
    {
        printf("Error writing to soundcard, error: %s\n", strerror(errno));
        exit(1); 
    }
    else if (bytesWrite != AUDIO_SIZE)
    {
        printf ("Played a different number of bytes than expected (played %d bytes, expected %d)\n", bytesWrite, AUDIO_SIZE);
        exit(1);
    }

    if (ioctl(descriptorSnd, SNDCTL_DSP_GETODELAY, &bytesInSndcard) <0 )
    {
        printf ("Error calling ioctl SNDCTL_DSP_GETODELAY, error %s\n", strerror(errno));
        exit(1);
    }

    if (bytesInSndcard == 0)
    {
        printf ("WARNING: seems SNDCTL_DSP_GETODELAY it is not working, returns 0 bytes in queue.\n");
    }
    else {
        printf("Bytes in sndcard: %d (should be close to %d). \n", bytesInSndcard, requestedFragmentSize);
        /* at VirtualBox 5.0.40, returns 1024 bytes, ok. 
VirtualBox 5.2.6 returns 453, 703, 783.
VirtualBox 6.0.8 seems to work better, returning 1024!	*/
    }
    
	/* Another measurement */
	int i;
	int rounds = 1;
	for (i=0; i<rounds; i++) {
		if ( (bytesWrite = write(descriptorSnd, audioData, AUDIO_SIZE)) < 0)
		{
			printf("Error writing to soundcard, error: %s\n", strerror(errno));
			exit(1); 
		}
	}
	if (ioctl(descriptorSnd, SNDCTL_DSP_GETODELAY, &bytesInSndcard) <0 )
    {
        printf ("Error calling ioctl SNDCTL_DSP_GETODELAY, error %s\n", strerror(errno));
        exit(1);
    }
	printf("Writing more bytes");
	/* +1 in rounds is for the first packet included before. */
	printf("Bytes in sndcard: %d (should be close to %d). \n", bytesInSndcard, requestedFragmentSize*(rounds+1));
	/* VirtualBox 5.2.6 returns 2047 for rounds 8, 4, all the time - seems that this is a maximum.	*/
    return 0;
}
