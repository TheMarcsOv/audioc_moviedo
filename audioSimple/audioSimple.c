/*
audioSimple record|play [-b(8|16)] [stereo] [-vVOLUME] [-rRATE][-sBLOCK_SIZE]  fileName

Examples on how the program can be started:
./audioSimple record -b16 audioFile
./audioSimple record audioFile
./audioSimple play -b8 stereo -v90 -r8000 -s1024 audioFile

To compile, execute
gcc -Wall -Wextra -o audioSimple audioSimple.c audioSimpleArgs.c configureSndCard.c

Operations:
- play:     reads from a file and plays the content

-b 8 or 16 bits per sample
VOL volume [0..100]
RATE sampling rate in Hz

default values:  8 bits, vol 90, sampling rate 8000, 1 channel, 4096 bytes per block 
*/

#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <sys/soundcard.h>

#define ENDIAN_SWAP16(X) ((((X) >> 8) & 0xff) | (((X) & 0xff) << 8))

#include "audioSimpleArgs.h"
#include "../lib/configureSndcard.h"

void record (int descSnd, const char *fileName, int fragmentSize);
void play (int descSnd, const char *fileName, int fragmentSize);


const int BITS_PER_BYTE = 8;

/* only declare here variables which are used inside the signal handler */
char *buf = NULL;
char *fileName = NULL;     /* Memory is allocated by audioSimpleArgs, remember to free it */

/* activated by Ctrl-C */
void signalHandler (int sigNum __attribute__ ((unused)))  /* __attribute__ ((unused))   
-> this indicates gcc not to show an 'unused parameter' warning about sigNum: 
is not used, but the function must be declared with this parameter */
{
    printf ("\naudioSimple was requested to finish\n");
    if (buf) free(buf);
    if (fileName) free(fileName);
    exit (0);
}


int main(int argc, char *argv[])
{
    struct sigaction sigInfo; /* signal conf */

    int bitsPerSample;
    int sndCardFormat;
    int channelNumber;
    int rate;
    int vol;
    int audioSimpleOperation;       /* record, play */
    int descriptorSnd;
    int requestedFragmentSize;

    /* we configure the signal */
    sigInfo.sa_handler = signalHandler;
    sigInfo.sa_flags = 0; 
    sigemptyset(&sigInfo.sa_mask); /* clear sa_mask values */
    if ((sigaction (SIGINT, &sigInfo, NULL)) < 0) {
        printf("Error installing signal, error: %s", strerror(errno)); 
        exit(1);
    }
    /* obtain values from the command line - or default values otherwise */
    if (EXIT_FAILURE == args_capture (argc, argv, &audioSimpleOperation, &bitsPerSample, 
            &channelNumber, &rate, &vol, &requestedFragmentSize, &fileName))
    { 
        exit(1);  /* there was an error parsing the arguments, the error type 
                   is printed by the args_capture function */
    };

    /* Map bits to format, we use PCM MU_LAW and S16_BE */
    if (bitsPerSample == 8) {
        sndCardFormat = AFMT_MU_LAW;
    }
    else {
        sndCardFormat = AFMT_S16_BE;
    }

    printf("Requested fragmentsize %d\n", requestedFragmentSize);

    /* create snd descriptor and configure soundcard to given format, rate, number of channels. 
     * Also configures fragment size */
    /* Do not configure Duplex mode: either records or plays, not do both (remember Duplex mode may
    restrict the available configurations to use, for example, reducing the number of bits that can 
    be used for recording/playing) */
    int duplexMode = false; 
    configSndcard (&descriptorSnd, &sndCardFormat, &channelNumber, &rate, &requestedFragmentSize, duplexMode); 
    vol = configVol (channelNumber, descriptorSnd, vol);

    /* obtained values -may differ slightly - eg. frequency - from requested values */
    args_print(audioSimpleOperation, sndCardFormat, channelNumber, rate, vol, requestedFragmentSize, fileName);
    printFragmentSize (descriptorSnd);
    printf ("Duration of each packet exchanged with the soundcard :%f\n", (float) requestedFragmentSize / (float) (channelNumber * sndCardFormat / BITS_PER_BYTE) / rate); /* note that in this case sndCardFormat is ALSO the number of bits of the format, this may not be the case */

    if (audioSimpleOperation == RECORD)
        record (descriptorSnd, fileName, requestedFragmentSize); /* this function - and the following functions - are coded in configureSndcard */
    else if (audioSimpleOperation == PLAY)
        play (descriptorSnd, fileName, requestedFragmentSize);

    /* Never reaches here, but the compiler stops generating warnings */
    return 0;
};



/* creates a new file fileName. Creates 'fragmentSize' bytes from descSnd 
 * and stores them in the file opened.
 * If an error is found in the configuration of the soundcard, 
 * the process is stopped and an error message reported. */  
void record (int descSnd, const char * fileName, int fragmentSize)
{
    int file;
    int bytesRead;
    int bytesWrite;

    /* Creates buffer to store the audio data */

    buf = malloc (fragmentSize); 
    if (buf == NULL) { 
        printf("Could not reserve memory for audio data.\n"); 
        exit (1); /* very unusual case */ 
    }

    /* opens file for writing */
    if ((file = open  (fileName, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU)) < 0) {
        printf("Error creating file for writing, error: %s", strerror(errno));
        exit(1);
    }

    /* If it would be needed to store inside the file some data 
     * to represent the audio format, this should be the place */

    printf("Recording in blocks of %d bytes... :\n", fragmentSize);

    while (1) 
    { /* until Ctrl-C */
        if ((bytesRead = read (descSnd, buf, fragmentSize)) < 0)
        {
            printf("Error reading from soundcard, error: %s\n", strerror(errno));
            exit(1);
        } 
        if (bytesRead!= fragmentSize)
        {
            printf ("Recorded a different number of bytes than expected (recorded %d bytes, expected %d)\n", bytesRead, fragmentSize);
        }
        printf (".");fflush (stdout);

        if ((bytesWrite = write (file, buf, fragmentSize)) < 0)
        {
            printf("Error writing to file, error: %s\n", strerror(errno));
            exit(1);
        }
        if (bytesWrite!= fragmentSize) 
        {
            printf("Written to file a different number of bytes than expected, exiting\n"); 
            exit(1);
        }
    }
}

/* This function opens an existing file 'fileName'. It reads 'fragmentSize'
 * bytes and sends them to the soundcard, for playback
 * If an error is found in the configuration of the soundcard, the process 
 * is stopped and an error message reported. */
void play (int descSnd, const char * fileName, int fragmentSize)
{
    int file;
    int bytesRead;
    int bytesWrite;

    /* Creates buffer to store the audio data */
    buf = malloc (fragmentSize); 
    if (buf == NULL) { printf("Could not reserve memory for audio data.\n"); exit (1); /* very unusual case */ }

    /* opens file in read-only mode */
    if ((file = open (fileName, O_RDONLY)) < 0) {
        printf("File could not be opened, error %s\n", strerror(errno));
        exit(1);
    }

    /* If you need to read from the file and process the audio format, this could be the place */
    printf("Playing in blocks of %d bytes... :\n", fragmentSize);

    while (1)
    { 
        if ((bytesRead = read (file, buf, fragmentSize)) <0)
        {
            printf("Error reading from file, error: %s\n", strerror(errno));
            exit(1);
        } 
        if (bytesRead != fragmentSize)
            break; /* reached end of file */

        //TEST: Perform processing
        // int sampleCount = fragmentSize / sizeof(short);
        // signed short* samples = (signed short*)buf;
        // static float t = 0;
        // const int SR = 44100;
        // for (int i = 0; i < sampleCount; i++)
        // {
        //     signed short origVal = samples[i];
        //     signed short val = 10000.f * sinf(t);
        //     t += 2 * M_PI * 440.f / (float)SR;
        //     if (t >= (2 * M_PI)) t -= 2 * M_PI;
            
        //     //samples[i] = ENDIAN_SWAP16(val);
        //     samples[i] = ENDIAN_SWAP16(origVal);
        // }
        //

        if ((bytesWrite = write (descSnd, buf, fragmentSize)) < 0)
        {
            printf("Error writing to soundcard, error: %s\n", strerror(errno));
            exit(1);
        }
        if (bytesWrite!= fragmentSize)
        {
            printf ("Played a different number of bytes than expected (played %d bytes, expected %d; exiting)\n", bytesWrite, fragmentSize);
            exit(1);
        }
        printf (".");fflush (stdout);

    }
};
