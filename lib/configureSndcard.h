/*******************************************************/
/* configureSndcard.h */
/*******************************************************/

#ifndef SOUND_H
#define SOUND_H

#include <stdbool.h>



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


/* This function changes the configuration for the descSnd provided to the parameters 
 * stated in dataSndQuality. 
 * If an error is found in the configuration of the soundcard, the process is stopped 
 * and an error message reported.
 * It always opens the sound card, and return the corresponding soundcard descriptor */
void configSndcard (int *descSnd, /* Does not use the initial value of the variable.
                                    Returns number of soundcard descriptor */
        int *format,              /* Receives a soundcard format (see 'soundcard.h').
                                    Returns format actually configured at the soundcard 
                                    (may be different than requested). */
        int *channelNumber,     /* Receives the requested number of channels. 
                                    Returns number of channels (1 or 2) actually 
                                    configured at the soundcard (may be different than requested) */
        int *rate,              /* Receives the requested sampling rate. 
                                    Returns sampling rate (Hz) actually configured at 
                                    the soundcard (may be different than requested) */
	    int *fragmentSize,      /* Receives the requested fragment size in bytes.
                                   Returns the actual fragment size (always a power of 2 
                                   value equal or lower to the requested value). */
        bool duplexMode         /* Receives a request to configure DUPLEX mode (true) or not. 
                                   Returns same value.*/
	);

/* Configures volume for descSnd. 
 * If 'stereo' is set to 1, it configures both channels, otherwise configures just one. 
 * Receives a number in the [0..100] range.
 * The function returns the volume actually configured in the device after performing 
 * the operation (could be different than requested).
 * If an error is found in the configuration of the soundcard, the process is stopped 
 * and an error message reported. */
int configVol (int stereo, int descSnd, int vol);

/* prints fragment size configured for sndcard */
void printFragmentSize (int descriptorSnd);

#endif /* SOUND_H */


