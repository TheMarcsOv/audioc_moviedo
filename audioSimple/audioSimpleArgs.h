/*******************************************************/
/* audioSimpleArgs.h */
/*******************************************************/

/* Expected format for audioSimple execution
audioSimple record|play [-b(8|16)] [stereo] [-vVOLUME] [-rRATE] [-sBLOCK_SIZE] fileName
*/

#ifndef AUDIO_H
#define AUDIO_H


/* POSIBLE OPERATIONS */
enum audioSimpleOperations {
    RECORD, 
    PLAY
};

/* -------------------------------------------------------------------------------- */
/* FUNCTIONS */

/* Parses arguments from command line 
 * Returns  EXIT_FAILURE if it finds an error when parsing the args. In this
 * case the returned values are meaningless. It prints a message indicating
 * the problem observed.
 * Returns EXIT_SUCCESS if it could parse the arguments correctly */
int args_capture(  int argc,  char * argv[],
        int *audioSimpleOperation,  /* Does not use the initial value of the variable.
                                        Returns operation id, see 'enum audioSimpleOperations operations' */
        int *bitNumber,         /* Does not use the initial value of the variable.
                                        Returns number of bits, 8 or 16*/
        int *channelNumber,         /* Does not use the initial value of the variable. 
                                        Returns number of channels, 1 or 2 */
        int *rate,                  /* Does not use the initial value of the variable.
                                        Returns sampling rate */
        int *vol,                   /* Does not use the initial value of the variable.
                                        Returns volumen in [0..100] range */
        int *blockSize,             /* Does not use the initial value of the variable.
                                        Returns size in bytes used for read/write to soundcard and file */
        char **fileNameStr	        /* Does not use the initial value of the variable.
                                        Returns a pointer to a  string with enough space (created 
                                       with malloc). 
                                       Code using this function must free the pointer (free(fileNameStr)) */
        );

/* Prints values in a friendly manner */
void args_print (int audioSimpleOperation, int sndCardFormat, int channelNumber, int rate, int vol, int blockSize, char *fileName);


#endif /* AUDIO_H */

