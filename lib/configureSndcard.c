/*******************************************************/
/* configureSndcard.c */
/*******************************************************/

#include "configureSndcard.h"

#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/soundcard.h>

#include <string.h> /* strerror */


unsigned int ceil_log2(register unsigned int x); /* function to efficiently compute the log2 ceil of an integer, i.e. the integer part of the log2 of another integer. Examples:
                                                     floor_log2(31) -> 5
                                                     floor_log2(32) -> 5
                                                     floor_log2(33) -> 6 */
/* the ceil_log2 function is defined at the end of the file */

/* SOUNDCARD CONFIGURATION */
/* We separate (format, chan number, frequency) configuration from volume configuration */

/*=====================================================================*/
void configSndcard (int *descSnd, int *sndCardFormat, int *channelNumber, int *rate, int *requestedFragmentSize, bool duplexMode)
{
    unsigned int frgmArg, requestedFrgmArg; /* local variables for fragment configuration */
    int ioctl_arg;


    /* opens the audio device if it was not opened before */
    if ( ((*descSnd) = open("/dev/dsp", O_RDWR)) < 0) {
        printf ("Error opening  /dev/dsp - check (*) if file /dev/dsp exists or (*) if other program is using /dev/dsp, error %s", strerror (errno));
        exit(1);
    }

    /* CONFIGURING FRAGMENT SIZE */
    /* It sets the fragmen size only if it is opened for the first time. Remember that this operation MUST be done before additional configuration of the sound card */

    /* Argument to construct: 0xMMMMSSSS  , being MMMM fragments (MMMM=number of fragments) of size 2^SSSS (SSSS= log2 of the size of the fragments.
       We set the size of the fragments, and we do not request anything about the number of fragments */
    requestedFrgmArg = ceil_log2 (*requestedFragmentSize); /* ceil_log2 is defined at the end of this file. Sets SSSS part of fragment argument */

    if (requestedFrgmArg > 16) { /* too large value, exceeds 65536 fragment size */
        printf("Requested fragment size is too large, exceeds 65536\n");
        exit (1);
    }
    requestedFrgmArg = requestedFrgmArg | 0x7fff0000 ; /* this value is used to request the soundcard not to limit the number of fragments available. Sets MMMM part of fragment argument */



    frgmArg = requestedFrgmArg;
    if ((ioctl ( (*descSnd), SNDCTL_DSP_SETFRAGMENT, &frgmArg)) < 0) {
        printf ("Failure when setting the fragment size, error: %s\n", strerror (errno));
        exit(1);
    }

    if (frgmArg != requestedFrgmArg) {
        printf ("Fragment size could not be set to the requested value: requested argument was %d, resulting argument is %d\n", requestedFrgmArg, frgmArg);
        exit (1);
    }


    /* returns configured fragment value, in bytes, so it performs 2^frgmArg by shifting one bit the appropriate number of times */
    frgmArg = requestedFrgmArg & 0x000FFFF;
    *requestedFragmentSize = 1 << frgmArg;  /* returns actual configured value of the fragment size, in bytes*/


    /* WARNING: this must be done in THIS ORDER:  format, chan number, frequency. There are some sampling frequencies that are not supported for some bit configurations... */

    /* The soundcard format configured by SNDCTL_DSP_SETFMT indicates if there is any compression in the audio played/recorded, and the number of bits used. There are many formats defined in /usr/include/linux/soundcard.h. To illustrate this, we next copy from soundcard.h some of them:

#define SNDCTL_DSP_SETFMT               _SIOWR('P',5, int) -- Selects ONE fmt
#       define AFMT_QUERY               0x00000000      -- Return current fmt
#       define AFMT_MU_LAW              0x00000001
#       define AFMT_A_LAW               0x00000002
#       define AFMT_IMA_ADPCM           0x00000004
#       define AFMT_U8                  0x00000008
#       define AFMT_S16_LE              0x00000010      -- Little endian signed 16
#       define AFMT_S16_BE              0x00000020      -- Big endian signed 16
#       define AFMT_S8                  0x00000040
#       define AFMT_U16_LE              0x00000080      -- Little endian U16
#       define AFMT_U16_BE              0x00000100      -- Big endian U16
#       define AFMT_MPEG                0x00000200      -- MPEG (2) audio
#       define AFMT_AC3         	    0x00000400       Dolby Digital AC3
 */



    /* Feb 2020, Changed order: first set SPEED, then FMT. 
    Otherwise, fragment size is not properly set.
    */
    ioctl_arg = *rate;
    if ((ioctl((*descSnd), SNDCTL_DSP_SPEED, &ioctl_arg)) < 0) {
        printf ("Error in the ioctl which sets the sampling frequency, error %s", strerror (errno));
        exit(1);
    }
    if (ioctl_arg != *rate) {
        printf ("You requested %d Hz sampling frequency, the system call returned %d. If the difference is not much, this is not an error", *rate, ioctl_arg); }
    *rate = ioctl_arg;


    /* Added nov19 to fix a fragment size configuration problem in Linux 4.19 and 5.2 systems, Leganes.
    Problem: Fragment size is actually set when the format is configured.
        Fragment size was not properly set for U8.
    Fix: fragment is properly set if S16_BE format is requested, so first set S16_BE, then U8
    */

    // printf("Fragment size after speed, before setting FMT to 16 bits: ");
    // printFragmentSize(*descSnd);

    ioctl_arg = AFMT_S16_BE;
    if ((ioctl((*descSnd), SNDCTL_DSP_SETFMT, &ioctl_arg)) < 0) {
        printf("Error in the ioctl which sets the format/bit number, error %s", strerror (errno));
        exit(1);
    }
    if (ioctl_arg != AFMT_S16_BE)
    printf("It was not possible to set the requested format/bit number (you requested %d, the ioctl system call returned %d).\n", *sndCardFormat, ioctl_arg);
    
    // printf("Fragment size before FMT(xxx): ");
    // printFragmentSize(*descSnd);

    if (*sndCardFormat != AFMT_S16_BE) {
        printf("Configuring format %d\n", *sndCardFormat); 
    ioctl_arg = *sndCardFormat;
    if ((ioctl((*descSnd), SNDCTL_DSP_SETFMT, &ioctl_arg)) < 0) {
        printf("Error in the ioctl which sets the format/bit number, error %s", strerror (errno));
        exit(1);
    }
    if (ioctl_arg != *sndCardFormat)
        printf("It was not possible to set the requested format/bit number (you requested %d, the ioctl system call returned %d).\n", *sndCardFormat, ioctl_arg);
    }



    ioctl_arg = *channelNumber;
    if ((ioctl((*descSnd), SNDCTL_DSP_CHANNELS, &ioctl_arg)) <0) {
        printf("Error in the ioctl which sets the number of channels, error: %s", strerror(errno));
        exit(1);
    }
    if (ioctl_arg != *channelNumber)
        printf("It was not possible to set the requested number of channels (you requested %d, the ioctl system call returned %d).\n", *channelNumber, ioctl_arg);




    if (duplexMode) {
        if ((ioctl ((*descSnd), SNDCTL_DSP_SETDUPLEX, 0)) < 0) {
            printf ("Error in the ioctl which configures the soundcard in DUPLEX mode, error %s", strerror (errno));
            exit(1);
        }
    }
}


/*=====================================================================*/
/*  For stereo, it configures both channels with the same volume. */
/* returns the actual volume set in the soundcard after performing the operation */
int configVol (int channels, int descSnd, int vol)
{
    int volFinal, volLeft, volRight;
    volFinal = volLeft = volRight = vol;
    if (channels == 2) {
        volFinal = (volRight << 8) + volLeft;
    }

    /* configures the same volume for playing and recording */
    if ((ioctl (descSnd, MIXER_WRITE (SOUND_MIXER_MIC), &volFinal)) < 0) {
        printf("Error when seting volume for MIC, error %s\n", strerror (errno));
        exit(1);
    }
    if ((ioctl (descSnd, MIXER_WRITE (SOUND_MIXER_PCM), &volFinal)) < 0) {
        printf("Error when seting volume for playing, error %s\n", strerror (errno));
        exit(1);
    }

    /* we assume both channels have been equally configured; we take one to return the resulting volume */
    volLeft = volFinal & 0xff;
    return volLeft;
}



/* prints the fragment size actually configured */
void printFragmentSize (int descriptorSnd)
{
    unsigned int frgmArg; /* local variables for fragment configuration */

    /* get fragment size */
    if ((ioctl (descriptorSnd, SNDCTL_DSP_GETBLKSIZE, &frgmArg)) < 0) {
        printf("Error getting fragment size, error %s", strerror (errno));
        exit(1);
    }
    printf ("Fragment size is %d\n", frgmArg);
}


/* the next functions to obtain the logarithm of an integer in an efficient way are copied from http://aggregate.org/MAGIC/
   The reader is referred to this link if he requires more detail on how they work.
   */

unsigned int ones32(register unsigned int x)
{
    /* 32-bit recursive reduction using SWAR...
       but first step is mapping 2-bit values
       into sum of 2 1-bit values in sneaky way
       */
    x -= ((x >> 1) & 0x55555555);
    x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
    x = (((x >> 4) + x) & 0x0f0f0f0f);
    x += (x >> 8);
    x += (x >> 16);
    return(x & 0x0000003f);
}

unsigned int floor_log2(register unsigned int x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
#ifdef	LOG0UNDEFINED
    return(ones32(x) - 1);
#else
    return(ones32(x >> 1));
#endif
}

#define WORDBITS 32
unsigned int ceil_log2(register unsigned int x)
{
	register int y = (x & (x - 1));

	y |= -y;
	y >>= (WORDBITS - 1);
        x |= (x >> 1);
        x |= (x >> 2);
        x |= (x >> 4);
        x |= (x >> 8);
        x |= (x >> 16);
#ifdef	LOG0UNDEFINED
        return(ones32(x) - 1 - y);
#else
	return(ones32(x >> 1) - y);
#endif
}

uint32_t previous_power_2 (uint32_t x)
{
    x = x | (x >> 1);
    x = x | (x >> 2);
    x = x | (x >> 4);
    x = x | (x >> 8);
    x = x | (x >> 16);
    return x - (x >> 1);
}
